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

#include <stdio.h>     
#include <stdlib.h>
#include <fcntl.h>    
#include <ctype.h>   
#include <string.h> 
#include <memory.h>
#include <time.h>

#include "../lib/symtab.h"		/* our utilities/tools */
#include "../afileio/afidefs.h"   


#include "fmconst.h"               /* constant definitons */


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
****************************************************************************
*
*  Mnemonic: FMelse
*  Abstract: This routine is called when the .ei command is encountered
*            during normal processing. If it is encountered then we were
*            processing the true portion of an if statement and we need
*            to skip the else section of text which is exactly what this
*            routine does. It will stop when the .fi command si found.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     20 July 1994
*  Author:   E. Scott Daniels
*
*  Modified: 27 Jul 1994 - To allow nested ifs in else clauses
*****************************************************************************
*/
void FMelse( )
{
 char *tok;              /* pointer to the token */
 int nested = 0;         /* number of nested levels we must resolve */

 while( FMgettok( &tok ) > OK )      /* while tokens still left in the file */
  {
   if( strncmp( ".if", tok, 3 ) == 0 )  /* nested if statement */
    nested++;                           /* increase number of fis to find */
   else
    if( strncmp( ".fi", tok, 3 ) == 0 )  /* found end if */
     {
      if( nested == 0 )                  /* if this is the last one to find */
       return;                            /* then get out */
      else
       nested--;                         /* else - just dec number looking fr*/
     }
  }            /* end while - if we drop out then unexpected end of file */

 FMmsg( E_UNEXEOF, ".ei not terminated" );
}    /* FMelse */

