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
***************************************************************************
*
*    Mnemonic: FMtoc
*    Abstract: This routine is responsible for putting the contents of the
*              output buffer into the table of contents file.
*              When this routine is called the header to be placed in the
*              output file must be in the obuf (output buffer).
*    Parms:    level - Paragraph level
*    Returns:  Nothging.
*    Date:     21 December 1988
*    Author:   E. Scott Daniels
*
*    Modified: 3 Feb 1989 - To not place page #s in toc if not numbering
*             15 Dec 1992 - To convert for postscript and AFI
*             11 Apr 1993 - To insert a newline on the last sprintf
****************************************************************************
*/
void FMtoc( int level )
{
 char buf[MAX_READ];      /* buffer to build toc entry in */
 int i;                   /* pointer into buffer */
 int j;
 

 if( tocfile == ERROR )    /* if no toc file open then do nothing */
  return;

 if( (flags & TOC) == 0 )        /* if user has them turned off */
  return;                        /* then dont do anything */

 if( level == 1 )               /* skip an extra space for first level */
 {
	AFIwrite( tocfile, ".sp 1 .sf Helvetica-Bold .st 12\n" );     /* change to stand out a bit */
 }
 else
  AFIwrite( tocfile, ".sf Helvetica .st 10\n" );  /* normal text */

 snprintf( buf, sizeof( buf ),  ".in +%.2fi\n", (double) (level-1) * .5 );
 AFIwrite( tocfile, buf );

 for( i=0, j = 0; j < optr && i < 70; j++, i++ )   /* copy into toc buffer */
  buf[i] = obuf[j];
 buf[i] = EOS;              /* terminate the string */
 AFIwrite( tocfile, buf );  /* and output it to the toc file */

 sprintf( buf, ".in -%.2fi\n", (double) (level-1) * .5 );
 AFIwrite( tocfile, buf );
 if( flags & PAGE_NUM )       /* if numbering the pages then place number */
  {
   sprintf( buf, ".cl : %s %d .tr\n", level == 1 ? ".sp 1" : "", page+1 );
   AFIwrite( tocfile, buf );            /* write the entry to the toc file */
  }                             /* end if page numbering */
 else
 {
   sprintf( buf, ".cl : .sp 1  .tr\n", page+1 );
   AFIwrite( tocfile, buf );            /* write the entry to the toc file */
 }

}                   /* FMtoc */

