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
*
*  Mnemonic: FMright
*  Abstract: This routine processes the .ri command. It accepts an x value
*            (optionally) defining the right most point and uses the remainder
*            of the characters on the input as the string to right justify.
*            If the x value is omitted, then the current column width is
*            added to the current column left margin value and that value
*            is used as the right most x coordinate of the text. If a number
*            is to be the first part of the string and no x value is to be
*            given then the number should be escaped with a carrot ^.
*  Parms:    None
*  Returns:  Nothing
*  Date:     23 March 1993
*  Author:   E. Scott Daniels
*
*            .ri [x value] text to right justify <eos>
*******************************************************************************
*/
void FMright( )
{
 char *buf;          /* pointer at input buffer */
 int i;              /* length of parameter from input */
 int x;              /* x value for PostScript moveto */
 char wbuf[80];      /* buffer to build moveto command in */

 flags2 |= F2_RIGHT;    /* make flush place out right command */

 if( ( i = FMgetparm( & buf )) <= 0 )   /* if nothing else on line */
  return;                               /* then get out now */

 if( optr == 0 )     /* if nothing in buffer, force flush to setup next y */
  {
   optr = 1;
   obuf[0] = ' ';   /* set up blank for flush */
   obuf[1] = EOS;   /* string must be terminated */
  }
 FMflush( );                 /* flush out the buffer and setup new y value */

 if( isdigit( buf[0] ) )     /* if user started with a digit then assume x */
  x = atoi( buf );           /* convert buffer to integer */
 else
  {                          /* assume first token of string to add */
   FMaddtok( buf, i );       /* add it to output buffer strip and escape */
   x = cur_col->lmar + cur_col->width;  /* calculate default x value */
  }

 while( (i = FMgetparm( &buf )) > 0 )   /* while not at end of input line */
  FMaddtok( buf, i );                   /* add remainder of string tokens */

 sprintf( wbuf, "%d -%d\n", x, cury );  /* setup x,y for right command */
 AFIwrite( ofile, wbuf );
 FMflush( );                           /* flush remainder of the buffer */

 flags2 &= ~F2_RIGHT;         /* turn off right justify flag */
}            /* FMright */
