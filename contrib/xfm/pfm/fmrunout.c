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
*****************************************************************************
*
*  Mnemonic: FMrunout
*  Abstract: This routine is responsible for writing the running matter
*            (header, footer and page number) for the current page to the
*            output file.
*  Parms:    page - The current page number
*            shift- TRUE if we should shift the text to the right margin.
*  Returns:  Nothing.
*  Date:     25 October 1992
*  Author:   E. Scott Daniels
*
*  Modified: 10 Dec 1992 - To use AFI routines for ansi compatability
*            21 Feb 1994 - To use rightxy instead of right
*		10 Oct 2007 (sd)  - Added support to anchor head/feet to left col's anchor and not lmar
****************************************************************************
*/
void FMrunout( int page, int shift )
{
 char *cmd = "show";      /* default to show command with header text info */
 int y;                   /* spot for writing the footer/page number info */
 int x;                   /* either far left or right depending on shift */
 int len;                 /* length of string for output to write buffer */
 char *moveto = "moveto"; /* moveto command necessary for show command */
 char buf[2048];            /* output buffer */


 if( rhead == NULL && rfoot == NULL && (flags & PAGE_NUM) == 0 )
  return;               /* nothing to do - get out */

 FMsetfont( runfont, runsize );   /* set up the font in the output */
 FMfmt_add( );

 if( shift == TRUE )    /* if we need to shift, use right instead of show */
  {
   cmd = "rightxy";
   moveto = " ";     /* moveto not necessary when using right */
   x = MAX_X;        /* x must be far right for the right command */
  }
 else
  x = firstcol->anchor;    /* x is the firstcolumn's left margin */


 if( rhead != NULL )   /* if there is a running header defined */
  {
   sprintf( buf, "%d -%d %s (%s) %s\n", x, topy - 6 - textsize, moveto, rhead, cmd );
   AFIwrite( ofile, buf );             /* send out the header */

   sprintf( buf, "%d -%d moveto %d -%d lineto\n", firstcol->lmar, topy - textsize -4, MAX_X, topy - textsize - 4 );
   AFIwrite( ofile, buf );       /* seperate header from text w/ line */
  }


#ifdef KEEP
 y = MAX_Y + 27;  
 y = (11 * 72) - ((textsize + textspace ) * 2);		/* bottom - two lines */
 y = (11 * 72) - 25;
#endif
 y = boty + 27;  					/* set up the spot for write */

 if( rfoot != NULL  ||  flags & PAGE_NUM )  /* draw line to seperate */
  {
	TRACE(1) "runout: anchor=%d y=%d \n", firstcol->anchor, y );
   sprintf( buf, " %d -%d moveto %d -%d lineto stroke\n", firstcol->anchor, y-10, MAX_X, y-10 );
   AFIwrite( ofile, buf );
  }

 if( rfoot != NULL )   /* if there is a running footer defined */
  {
   sprintf( buf, "%d -%d %s (%s) %s\n", x, y, moveto, rfoot, cmd );
   AFIwrite( ofile, buf );               /* send out the footer */
	y += 12;
   /*y += textsize + textspace;  */          /* bump up for page number */
  }

 if( flags & PAGE_NUM )    /* if we are currently numbering pages */
  {
   sprintf( buf, "%d -%d %s (Page %d) %s\n", x, y, moveto, page, cmd );
   AFIwrite( ofile, buf );             /* send out the page number */
  }    /* end if page numbering */
}     /* FMrunout */
