#include <tcl.h>
#include <proj.h>
#include "laszip_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ProjState {
    Tcl_HashTable hash;		/* List projections by name */
    int uid;			/* Used to generate names */
} ProjState;

int
LasCmd(ClientData data, Tcl_Interp * interp, int objc,
	Tcl_Obj * CONST objv[])
{
    ProjState *statePtr = (ProjState *) data;
    int index;
    char buf[129];

    char *subCmds[] = { "fwd", "inv", NULL };
    enum Las { Fwd, Inv };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subCmds,
			    "option", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {

    case Fwd:
    case Inv:
	{
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
		return TCL_ERROR;
	    }
	    Tcl_HashEntry *entryPtr =
		Tcl_FindHashEntry(&statePtr->hash, Tcl_GetString(objv[2]));
	    if (entryPtr == NULL) {
		Tcl_AppendResult(interp, "Unknown proj: ",
				 Tcl_GetString(objv[2]), NULL);
		return TCL_ERROR;
	    }
	    PJ *P = Tcl_GetHashValue(entryPtr);
	    
	    char *file_name_in = strdup(Tcl_GetString(objv[3]));
	    char *file_name_out = strdup(Tcl_GetString(objv[4]));

            // create the reader
	    laszip_POINTER laszip_reader;
	    if (laszip_create(&laszip_reader)) {
		Tcl_AppendResult(interp, "DLL ERROR: creating laszip reader", NULL);
		return TCL_ERROR;
	    }
	    // open the reader
	    laszip_BOOL is_compressed = 0;
	    if (laszip_open_reader(laszip_reader, file_name_in, &is_compressed)) {
		Tcl_AppendResult(interp, "DLL ERROR: opening laszip reader for ",
			file_name_in, NULL);
		return TCL_ERROR;
	    }
	    // get a pointer to the header of the reader that was just populated
	    laszip_header_struct *header;
	    if (laszip_get_header_pointer(laszip_reader, &header)) {
		Tcl_AppendResult(interp,
			"DLL ERROR: getting header pointer from laszip reader", NULL);
		return TCL_ERROR;
	    }
	    // how many points does the file have
	    laszip_I64 npoints =
		(header->number_of_point_records ? header->number_of_point_records :
		 header->extended_number_of_point_records);

	    laszip_U8 format = header->point_data_format;
	    laszip_U16 length = header->point_data_record_length;
	    laszip_U8 versmajor = header->version_major;
	    laszip_U8 versminor = header->version_minor;

	    // get a pointer to the points that will be read
	    laszip_point_struct *laspoint;
	    if (laszip_get_point_pointer(laszip_reader, &laspoint)) {
		Tcl_AppendResult(interp,
			"DLL ERROR: getting point pointer from laszip reader", NULL);
		return TCL_ERROR;
	    }
	    // create the writer
	    laszip_POINTER laszip_writer;
	    if (laszip_create(&laszip_writer)) {
		Tcl_AppendResult(interp, "DLL ERROR: creating laszip writer", NULL);
		return TCL_ERROR;
	    }
	    // initialize the header for the writer using the header of the reader
	    if (laszip_set_header(laszip_writer, header)) {
		Tcl_AppendResult(interp, "DLL ERROR: setting header for laszip writer", NULL);
		return TCL_ERROR;
	    }
	    // open the writer
	    laszip_BOOL compress = (strstr(file_name_out, ".laz") != 0);
	    if (laszip_open_writer(laszip_writer, file_name_out, compress)) {
		Tcl_AppendResult(interp, "DLL ERROR: opening laszip writer for ",
			file_name_out, NULL);
		return TCL_ERROR;
	    }
	    // read the points
	    laszip_I64 p_count = 0;
	    laszip_F64 coordinates[3];
	    while (p_count < npoints) {

		// read a point
		if (laszip_read_point(laszip_reader)) {
                    sprintf(buf,"DLL ERROR: reading point %I64d", p_count);
		    Tcl_AppendResult(interp, buf, NULL);
		    return TCL_ERROR;
		}
		// copy the point
		if (laszip_set_point(laszip_writer, laspoint)) {
                    sprintf(buf,"DLL ERROR: setting point %I64d", p_count);
		    Tcl_AppendResult(interp, buf, NULL);
		    return TCL_ERROR;
		}
		// get coords and transform
		if (laszip_get_coordinates(laszip_writer, coordinates)) {
                    sprintf(buf,"DLL ERROR: getting coords of point %I64d", p_count);
		    Tcl_AppendResult(interp, buf, NULL);
		    return TCL_ERROR;
		}

	        PJ_COORD a = proj_coord(coordinates[0],
		                        coordinates[1],
		                        coordinates[2], 0.0);

                if (index == Fwd) {
		    if (proj_angular_input(P, PJ_FWD)) {
			a.lp.lam = proj_torad(a.lp.lam);
			a.lp.phi = proj_torad(a.lp.phi);
		    }

		    a = proj_trans(P, PJ_FWD, a);

		    if (proj_angular_output(P, PJ_FWD)) {
			a.lp.lam = proj_todeg(a.lp.lam);
			a.lp.phi = proj_todeg(a.lp.phi);
		    }
		} else {
		    if (proj_angular_input(P, PJ_INV)) {
			a.lp.lam = proj_torad(a.lp.lam);
			a.lp.phi = proj_torad(a.lp.phi);
		    }

		    a = proj_trans(P, PJ_INV, a);

		    if (proj_angular_output(P, PJ_INV)) {
			a.lp.lam = proj_todeg(a.lp.lam);
			a.lp.phi = proj_todeg(a.lp.phi);
		    }
		}

		coordinates[0] = a.v[0];
		coordinates[1] = a.v[1];
		coordinates[2] = a.v[2];

		// ADJUST POINT
		if (laszip_set_coordinates(laszip_writer, coordinates)) {
                    sprintf(buf, "DLL ERROR: setting coordinates for point %I64d",
			    p_count);
		    Tcl_AppendResult(interp, buf, NULL);
		    return TCL_ERROR;
		}
		// write the point
		if (laszip_write_point(laszip_writer)) {
                    sprintf(buf, "DLL ERROR: writing point %I64d", p_count);
		    Tcl_AppendResult(interp, buf, NULL);
		    return TCL_ERROR;
		}
		p_count++;
	    }

	    // close the writer
	    if (laszip_close_writer(laszip_writer)) {
		Tcl_AppendResult(interp, "DLL ERROR: closing laszip writer", NULL);
		return TCL_ERROR;
	    }
	    // destroy the writer
	    if (laszip_destroy(laszip_writer)) {
		Tcl_AppendResult(interp, "DLL ERROR: destroying laszip writer", NULL);
		return TCL_ERROR;
	    }
	    // close the reader
	    if (laszip_close_reader(laszip_reader)) {
		Tcl_AppendResult(interp, "DLL ERROR: closing laszip reader", NULL);
		return TCL_ERROR;
	    }
	    // destroy the reader
	    if (laszip_destroy(laszip_reader)) {
		Tcl_AppendResult(interp, "DLL ERROR: destroying laszip reader", NULL);
		return TCL_ERROR;
	    }

            Tcl_Obj *numPtr = Tcl_NewWideIntObj(npoints);
	    Tcl_SetObjResult(interp, numPtr);
	    return TCL_OK;
	}
    }
    return TCL_OK;
}

int 
Las_Init(Tcl_Interp * interp)
{
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgRequire(interp, "Tcl", "8.1", 0) == NULL) {
	return TCL_ERROR;
    }

    Tcl_CmdInfo projinfo;
    if (Tcl_GetCommandInfo(interp, "proj", &projinfo) == 0) {
	     Tcl_AppendResult(interp,"Command not found: ",
				    "proj", NULL);
	return TCL_ERROR;
    }
    ProjState *statePtr = (ProjState *) projinfo.objClientData;
    Tcl_CreateObjCommand(interp, "las", LasCmd, (ClientData) statePtr,
			 NULL);
    Tcl_PkgProvide(interp, "las", "0.1");

	    // load LASzip DLL
	    if (laszip_load_dll()) {
		Tcl_AppendResult(interp, "DLL ERROR: loading LASzip DLL", NULL);
		return TCL_ERROR;
	    }

    return TCL_OK;
}

//  vim: cin:sw=4:tw=75  

