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
*  Mnemonic: FMjustify
*  Abstract: This routine is responsible for sending out the current buffer
*            with the proper justification command and related parameters.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     12 November 1992
*  Author:   E. Scott Daniels
*
*  Modified: 04 Dec 1992 - To call flush if spaces are 0
*            10 Dec 1992 - To use AFI routines for ansi compatability
*             6 Apr 1994 - To take advantage of linelen being points now
*****************************************************************************
*/
void FMjustify( )
{
 int spaces;        /* number of blank spaces */
 int i;             /* loop index */
 int len;           /* length of the string for write */
 char jbuf[256];    /* buffer for write */

 for( i = 0; obuf[i] != EOS; i++ );   /* find end of the string */
 for( i--; obuf[i] == BLANK; i-- );   /* find last non blank */
 obuf[i+1] = EOS;                     /* ensure no blanks at end of str */

 for( spaces = 0, i = 0; obuf[i] != EOS; i++ )
  if( obuf[i] == BLANK )
   spaces++;                   /* bump up the count */

 if( spaces == 0 )
  {
   FMflush( );            /* dont call just with a 0 parameter */
   return;
  }

 if( optr == 0 )   /* need to do a move to? */
  {
   cury += textsize + textspace;    /* move to the next y position */
   if( flags & DOUBLESPACE )        /* if in double space mode ...    */
    cury += textsize + textspace;   /* then skip down one more */

   if( cury > boty )               /* are we out of bounds? */
    {
     FMceject( 0 );       /* move to next column */
    }

   sprintf( jbuf, "%d -%d moveto\n", lmar, cury );  /* create moveto */
   AFIwrite( ofile, jbuf );      /* write the move to command or x,y for cen */
  }                              /* end need to do a move to */

 if( flags2 & F2_SETFONT )   /* need to reset font to text font? */
  {
   FMsetfont( curfont, textsize );  /* set the font */
   flags2 &= ~F2_SETFONT;           /* turnoff setfont flag */
  }

 sprintf( jbuf , "%d (%s) %d just\n", spaces, obuf, lmar + linelen );
             /*   cur_col->lmar + cur_col->width ); */
 AFIwrite( ofile, jbuf );         /* write the buffer to output file */

 if( flags2 & F2_BOX && box.hconst == TRUE )  /* in box and horz lines */
  {
   sprintf( jbuf, "%d -%d moveto %d -%d lineto stroke\n",
                  box.lmar, cury + 2, box.rmar, cury + 2 );
   AFIwrite( ofile, jbuf );               /* output the information */
  }
}           /* FMjustify */
