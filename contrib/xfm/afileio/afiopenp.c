/*
* ---------------------------------------------------------------------------
* This source code is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* If this code is modified and/or redistributed, please retain this
* header, as well as any other 'flower-box' headers, that are
* contained in the code in order to give credit where credit is due.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Please use this URL to review the GNU General Public License:
* http://www.gnu.org/licenses/gpl.txt
* ---------------------------------------------------------------------------
*/
/*
*****************************************************************************
*
*  Mnemonic: AFIopenpath
*  Abstract: This routine will attempt to open the file name as it is passed
*            in after it has been appended to the directory names that are
*            contained in the path string. Directories are tried one at a
*            time until the end of the path string is reached, or the file
*            is opened. If the path string is NULL then only the name is
*            used on the open attempt.
*  Parms:    name  - Pointer to the file name.
*            mode  - Mode string to pass to AFIopen
*            path  - Pointer to a list of directory path names seperated by
*                    semicolons.
*  Returns:  Status as returned by AFIopen.
*  Date:     7 January 1995
*  Author:   E. Scott Daniels
*
*  Modified: 11 Mar 1997 - To make multi-thread strtok_r call and for pipes
*****************************************************************************
*/
#include "afisetup.h"    /* get necessary include files */

int AFIopenp( name, mode, path )
 char *name;
 char *mode;
 char *path;
{
 int file;          /* file number from AFIopen */
 char *tok;         /* pointer at token in path and file name */
 char *tpath;       /* temp path buffer */
 char *tname;       /* temp name buffer */
 char *lasts;       /* string place holder for strtok_r */

 if( path == NULL ||                   /* allow no path, or if its an */
     *name == '/'                  ||  /* absolute path name  or */
     strncmp( name, "./", 2 ) == 0 ||  /* specific path name  or */
     strcmp( name, "pipe" ) == 0   ||  /* pipe or */
     strcmp( name, "stdin" ) == 0  ||  /* standardin or standard out then */
     strcmp( name, "stdout" ) == 0     /* just ignore path and call open */
   )
  {
   return( AFIopen( name, mode ) ); 
  }

 tpath = (char *) malloc( (unsigned) strlen( path ) + 1 );
 tname = (char *) malloc( (unsigned) (strlen( name ) + strlen( path ) + 1) );

 strcpy( tpath, path );        /* save so we dont destroy caller's copy */
 tok = strtok( tpath, ":;" );  /* point at first token in path */

 do
  {
   sprintf( tname, "%s/%s", tok, name );  /* make full name */
   file = AFIopen( tname, mode );
   tok = strtok( NULL, ":;" );          /* point at next in path */
  }
 while( file < AFI_OK && tok != NULL );   /* until end of path or opened */

 if( tname )
  free( tname );           /* release temp storage */
 if( tpath )
  free( tpath );

 return( file );          /* send back the file handle */
}                         /* AFIopenpath */
