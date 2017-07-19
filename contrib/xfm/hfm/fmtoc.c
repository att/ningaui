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
*              6 Sep 1994 - Conversion for RFM
*              6 Dec 1996 - Conversion to hfm
****************************************************************************
*/
void FMtoc( level )
 int level;
{
 char buf[MAX_READ];      /* buffer to build toc entry in */
 int i;                   /* pointer into buffer */
 int j;

 if( tocfile == ERROR )    /* if no toc file open then do nothing */
  return;

 if( (flags & TOC) == 0 )        /* if user has them turned off */
  return;                        /* then dont do anything */

 buf[0] = '`';                       /* quote lead blanks */
 for( i = 1; i < (level-1)*2; i++ )
  buf[i] = BLANK;                /* indent the header in toc */

 for( j = 0; j < optr && i < 70; j++, i++ )   /* copy into toc buffer */
  buf[i] = obuf[j];
 buf[i++] = '`';             /* terminate the quote */
 buf[i] = EOS;              /* terminate the string */

 if( level == 1 )               /* skip an extra space for first level */
  AFIwrite( tocfile, ".sp 1 \n" );     /* skip a line */

 AFIwrite( tocfile, buf );  /* and output it to the toc file */

}                   /* FMtoc */

