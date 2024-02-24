gcc -shared -fpic -g -Wall -O2 -Ic:/tcl/include -DUSE_TCL_STUBS las.c laszip_api.c -o las.dll -Lc:/tcl/lib -ltclstub86 -Lc:/OSGEO4W64/apps/proj-dev/bin -lproj_6_1 
