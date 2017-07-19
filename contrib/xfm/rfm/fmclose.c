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
#include "rfm_const.h"


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
*************************************************************************
*
*   Mnemonic: FMclose
*   Abstract: This routine is responsible for closing the file that is
*             represented by the first file block on the list pointed to
*             by fptr.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     15 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 16 Mar 1990 - Not to use FI routine.. there must be a bug
*                           in them somewhere.
*             10 Dec 1992 - To use AFI fileio (used ansi std fread rtns)

*              3 Apr 1997 - Prep for phase out
************************************************************************
*/
void FMclose( )
{
 char buf[100];        /* buffer to report number records in file */

 if( fptr == NULL )
  return;

 AFIclose( fptr->file );                /* close the file */

#ifdef KEEP
 FMmsg( I_EOFREACHED, fptr->name );     /* indicate to user */
 sprintf( buf, "%ld", fptr->count );    /* convert # of records */
 FMmsg( I_LINES, buf );                 /* number processed message */
#endif


 free( fptr );           /* get lost */
 fptr = NULL;
}                /* FMclose */
