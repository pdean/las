#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <tcl.h>
#include <proj.h>
#include <laszip/laszip_api.h>

typedef struct ProjState
{
  Tcl_HashTable hash;           /* List projections by name */
  int uid;                      /* Used to generate names */
} ProjState;

PJ_COORD
proj_trans_q (PJ * P, PJ_DIRECTION dir, PJ_COORD a)
{

//    printf("%.3f %.3f\n", a.v[0], a.v[1]);
  if (proj_angular_input (P, dir))
    {
      a.lp.lam = proj_torad (a.lp.lam);
      a.lp.phi = proj_torad (a.lp.phi);
    }

  a = proj_trans (P, dir, a);

  if (proj_angular_output (P, dir))
    {
      a.lp.lam = proj_todeg (a.lp.lam);
      a.lp.phi = proj_todeg (a.lp.phi);
    }

//    printf("%.3f %.3f\n\n", a.v[0], a.v[1]);

  return a;
}

static double taketime()
{
  return (double)(clock())/CLOCKS_PER_SEC;
}


int
LasCmd (ClientData data, Tcl_Interp * interp, int objc,
        Tcl_Obj * CONST objv[])
{
  ProjState *statePtr = (ProjState *) data;
  int index;
  char buf[256];

  char *subCmds[] = { "fwd", "inv", NULL };
  enum Las
  { Fwd, Inv };

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "option ?arg ...?");
      return TCL_ERROR;
    }
  if (Tcl_GetIndexFromObj (interp, objv[1], subCmds,
                           "option", 0, &index) != TCL_OK)
    {
      return TCL_ERROR;
    }

  switch (index)
    {

    case Fwd:
    case Inv:
      {
        if (objc != 5)
          {
            Tcl_WrongNumArgs (interp, 1, objv, "option ?arg ...?");
            return TCL_ERROR;
          }
        Tcl_HashEntry *entryPtr =
          Tcl_FindHashEntry (&statePtr->hash, Tcl_GetString (objv[2]));
        if (entryPtr == NULL)
          {
            Tcl_AppendResult (interp, "Unknown proj: ",
                              Tcl_GetString (objv[2]), NULL);
            return TCL_ERROR;
          }
        PJ *P = Tcl_GetHashValue (entryPtr);

        PJ_DIRECTION dir = index ? PJ_INV : PJ_FWD;

        char *file_name_in = strdup (Tcl_GetString (objv[3]));
        char *file_name_out = strdup (Tcl_GetString (objv[4]));
        
        double start_time = taketime();

        // create the reader
        laszip_POINTER laszip_reader;
        if (laszip_create (&laszip_reader))
          {
            Tcl_AppendResult (interp, "DLL ERROR: creating laszip reader",
                              NULL);
            return TCL_ERROR;
          }
        // open the reader
        laszip_BOOL is_compressed = 0;
        if (laszip_open_reader (laszip_reader, file_name_in, &is_compressed))
          {
            Tcl_AppendResult (interp, "DLL ERROR: opening laszip reader for ",
                              file_name_in, NULL);
            return TCL_ERROR;
          }
        // get a pointer to the header of the reader that was just populated
        laszip_header_struct *header_read, *header_write;
        if (laszip_get_header_pointer (laszip_reader, &header_read))
          {
            Tcl_AppendResult (interp,
                              "DLL ERROR: getting header pointer from laszip reader",
                              NULL);
            return TCL_ERROR;
          }
        // how many points does the file have
        laszip_I64 npoints =
          (header_read->number_of_point_records ? header_read->
           number_of_point_records : header_read->
           extended_number_of_point_records);

        // get a pointer to the points that will be read
        laszip_point_struct *laspoint;
        if (laszip_get_point_pointer (laszip_reader, &laspoint))
          {
            Tcl_AppendResult (interp,
                              "DLL ERROR: getting point pointer from laszip reader",
                              NULL);
            return TCL_ERROR;
          }
        // create the writer
        laszip_POINTER laszip_writer;
        if (laszip_create (&laszip_writer))
          {
            Tcl_AppendResult (interp, "DLL ERROR: creating laszip writer",
                              NULL);
            return TCL_ERROR;
          }

        // get a pointer to the header of the writer so we can populate it
        if (laszip_get_header_pointer (laszip_writer, &header_write))
          {
            Tcl_AppendResult (interp,
                              "DLL ERROR: getting header for laszip writer",
                              NULL);
            return TCL_ERROR;
          }

        // copy entries from the reader header to the writer header

        header_write->file_source_ID = header_read->file_source_ID;
        header_write->global_encoding = header_read->global_encoding;
        header_write->project_ID_GUID_data_1 = header_read->project_ID_GUID_data_1;
        header_write->project_ID_GUID_data_2 = header_read->project_ID_GUID_data_2;
        header_write->project_ID_GUID_data_3 = header_read->project_ID_GUID_data_3;
        memcpy(header_write->project_ID_GUID_data_4, header_read->project_ID_GUID_data_4, 8);
        header_write->version_major = header_read->version_major;
        header_write->version_minor = header_read->version_minor;
        memcpy(header_write->system_identifier, header_read->system_identifier, 32);
        memcpy(header_write->generating_software, header_read->generating_software, 32);
        header_write->file_creation_day = header_read->file_creation_day;
        header_write->file_creation_year = header_read->file_creation_year;
        header_write->header_size = header_read->header_size;
        header_write->offset_to_point_data = header_read->header_size; /* note !!! */
        header_write->number_of_variable_length_records = header_read->number_of_variable_length_records;
        header_write->point_data_format = header_read->point_data_format;
        header_write->point_data_record_length = header_read->point_data_record_length;
        header_write->number_of_point_records = header_read->number_of_point_records;
        for (int i = 0; i < 5; i++)
        {
          header_write->number_of_points_by_return[i] = header_read->number_of_points_by_return[i];
        }
        header_write->x_scale_factor = header_read->x_scale_factor;
        header_write->y_scale_factor = header_read->y_scale_factor;
        header_write->z_scale_factor = header_read->z_scale_factor;


        // transform centroid

        PJ_COORD a;

        double x = (header_read->max_x + header_read->min_x) / 2.0;
        double y = (header_read->max_y + header_read->min_y) / 2.0;
        double z = (header_read->max_z + header_read->min_z) / 2.0;
        a = proj_coord (x, y, z, 0.0);
        a = proj_trans_q (P, dir, a);

        x = 1000.0 * floor (a.v[0] / 1000.0);
        y = 1000.0 * floor (a.v[1] / 1000.0);
        z = 1000.0 * floor (a.v[2] / 1000.0);

        header_write->x_offset = x;
        header_write->y_offset = y;
        header_write->z_offset = z;

        header_write->max_x = 0.0;      // a-priori unknown bounding box
        header_write->min_x = 0.0;
        header_write->max_y = 0.0;
        header_write->min_y = 0.0;
        header_write->max_z = 0.0;
        header_write->min_z = 0.0;


        // we may modify output because we omit any user defined data that may be ** the header

        if (header_read->user_data_in_header_size)
        {
          header_write->header_size -= header_read->user_data_in_header_size;
          header_write->offset_to_point_data -= header_read->user_data_in_header_size;
          fprintf(stderr,"omitting %d bytes of user_data_in_header\n", header_read->user_data_after_header_size);
        }

        // add all the VLRs

        if (header_read->number_of_variable_length_records)
        {
          fprintf(stderr,"offset_to_point_data before adding %u VLRs is      : %d\n", header_read->number_of_variable_length_records, (laszip_I32)header_write->offset_to_point_data);
          for (int i = 0; i < header_read->number_of_variable_length_records; i++)
          {
            if (laszip_add_vlr(laszip_writer, header_read->vlrs[i].user_id, header_read->vlrs[i].record_id, header_read->vlrs[i].record_length_after_header, header_read->vlrs[i].description, header_read->vlrs[i].data))
            {
                sprintf(buf,"DLL ERROR: adding VLR %u of %u to the header of the laszip writer\n", i+i, header_read->number_of_variable_length_records);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
            }
            fprintf(stderr,"offset_to_point_data after adding VLR number %u is : %d\n", i+1, (laszip_I32)header_write->offset_to_point_data);
          }
        }

        // we may modify output because we omit any user defined data that may be *after* the header

        if (header_read->user_data_after_header_size)
        {
          fprintf(stderr,"omitting %d bytes of user_data_after_header\n", header_read->user_data_after_header_size);
        }



        // open the writer
        laszip_BOOL compress = (strstr (file_name_out, ".laz") != 0);
        if (laszip_open_writer (laszip_writer, file_name_out, compress))
          {
            Tcl_AppendResult (interp, "DLL ERROR: opening laszip writer for ",
                              file_name_out, NULL);
            return TCL_ERROR;
          }
        // read the points
        laszip_I64 p_count = 0;
        laszip_F64 coordinates[3];
        while (p_count < npoints)
          {

            // read a point
            if (laszip_read_point (laszip_reader))
              {
                sprintf (buf, "DLL ERROR: reading point %ld", p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }
            // copy the point
            if (laszip_set_point (laszip_writer, laspoint))
              {
                sprintf (buf, "DLL ERROR: setting point %ld", p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }
            // get coords and transform
            if (laszip_get_coordinates (laszip_reader, coordinates))
              {
                sprintf (buf, "DLL ERROR: getting coords of point %ld",
                         p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }

            a =
              proj_coord (coordinates[0], coordinates[1], coordinates[2],
                          0.0);

            a = proj_trans_q (P, dir, a);

            coordinates[0] = a.v[0];
            coordinates[1] = a.v[1];
            coordinates[2] = a.v[2];

            // ADJUST POINT
            if (laszip_set_coordinates (laszip_writer, coordinates))
              {
                sprintf (buf,
                         "DLL ERROR: setting coordinates for point %ld",
                         p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }
            // write the point
            if (laszip_write_point (laszip_writer))
              {
                sprintf (buf, "DLL ERROR: writing point %ld", p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }

            if (laszip_update_inventory (laszip_writer))
              {
                sprintf (buf, "DLL ERROR: updating inventory for point %ld",
                         p_count);
                Tcl_AppendResult (interp, buf, NULL);
                return TCL_ERROR;
              }
            p_count++;
          }

        // close the writer
        if (laszip_close_writer (laszip_writer))
          {
            Tcl_AppendResult (interp, "DLL ERROR: closing laszip writer",
                              NULL);
            return TCL_ERROR;
          }
        // destroy the writer
        if (laszip_destroy (laszip_writer))
          {
            Tcl_AppendResult (interp, "DLL ERROR: destroying laszip writer",
                              NULL);
            return TCL_ERROR;
          }
        // close the reader
        if (laszip_close_reader (laszip_reader))
          {
            Tcl_AppendResult (interp, "DLL ERROR: closing laszip reader",
                              NULL);
            return TCL_ERROR;
          }
        // destroy the reader
        if (laszip_destroy (laszip_reader))
          {
            Tcl_AppendResult (interp, "DLL ERROR: destroying laszip reader",
                              NULL);
            return TCL_ERROR;
          }
          
           fprintf(stderr,"total time: %g sec for reading %scompressed and writing %scompressed %ld points\n", 
                taketime()-start_time, (is_compressed ? "" : "un"), (compress ? "" : "un"), npoints);
          

        Tcl_Obj *numPtr = Tcl_NewWideIntObj (npoints);
        Tcl_SetObjResult (interp, numPtr);
        return TCL_OK;
      }
    }
  return TCL_OK;
}

int
Las_Init (Tcl_Interp * interp)
{
  if (Tcl_InitStubs (interp, "8.1", 0) == NULL)
    {
      return TCL_ERROR;
    }
  if (Tcl_PkgRequire (interp, "Tcl", "8.1", 0) == NULL)
    {
      return TCL_ERROR;
    }

  Tcl_CmdInfo projinfo;
  if (Tcl_GetCommandInfo (interp, "proj", &projinfo) == 0)
    {
      Tcl_AppendResult (interp, "Command not found: ", "proj", NULL);
      return TCL_ERROR;
    }
  ProjState *statePtr = (ProjState *) projinfo.objClientData;
  Tcl_CreateObjCommand (interp, "las", LasCmd, (ClientData) statePtr, NULL);
  Tcl_PkgProvide (interp, "las", "0.1");

  // load LASzip DLL
  if (laszip_load_dll ())
    {
      Tcl_AppendResult (interp, "DLL ERROR: loading LASzip DLL", NULL);
      return TCL_ERROR;
    }

  return TCL_OK;
}

//  vim: cin:sw=4:tw=75  

