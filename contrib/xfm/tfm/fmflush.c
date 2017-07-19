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
****************************************************************************
* TFM
*
*   Mnemonic: FMflush
*   Abstract: This routine is responsible for flushing the output buffer
*             to the output file and resetting the output buffer pointer.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     15 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 29 Jun 1994 - To convert to rfm.
*              6 Dec 1994 - To reset font even if nothing to write.
*              7 Dec 1994 - To prevent par mark at page top if in li list
*             14 Dec 1994 - To prevent par mark at top if in di list
*              6 Dec 1996 - To convert for hfm
*              9 Nov 1998 - To remove </font> - Netscape seems unable to
	                    handle them properly
*             22 Jan 2000 - To strncmp for color rather than strcmp
*             21 Mar 2001 - Revisiting an old friend; tfm conversion
****************************************************************************
*/
void FMflush( )
{
 char *xp;
 char *dp;               /* pointers to walk through buffers */
 int len;                /* length of the output string */
 char fbuf[2048];        /* buffer to build flush strings in */
 char fmt[255];          /* dynamic sprintf format string */
 char *xterm = NULL;     /* expanded di_term (must make % into %%) */
 int i;             /* the Is have it? */
 int li = 0;        /* list item indention */
 int iv = 0;        /* indention value */

 iv = (lmar/7);      /* basic indent value converted to char */

 if( lilist && lilist->xpos == 0 )
  {
   iv -= 2;
   li = lilist->xpos;
   lilist->xpos = 2;
   sprintf( fmt, "%%%ds%c %%s", iv, lilist->ch );   /* add current term */
  }
 else
  if( flags2 & F2_DIBUFFER )        /* need to back off some if term in buff */
   {
    iv -= dlstack[dlstackp].indent/7; 
    xterm = (char *) malloc( (strlen( di_term ) * 2) + 1 );
    for( xp = xterm, dp = di_term; *dp; )
     {
      *xp = *dp++;
      if( *xp++ == '%' )
       *xp++ = '%';
     }
    *xp = 0;

    sprintf( fmt, "%%%ds%s%%s", iv, xterm );   /* add current term */
    free( xterm );
   }
  else
   sprintf( fmt, "%%%ds%%s", iv );

 flags2 &= ~F2_DIBUFFER;

 if( optr == 0 ) 		 /* nothing to do so return */
  return;

 /* copy only the cached portion of the output buffer */
 FMjustify( );                          /* justify if on */
 sprintf( fbuf, fmt, " ", obuf ); 
 AFIwrite( ofile, fbuf );   /* write the text out to the file */

 *obuf = EOS;             /* make it end of string */
 optr = 0;

}                               /* FMflush */
