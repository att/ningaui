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
****************************************************************************
*/
void FMflush( )
{
 int len;           /* length of the output string */
 char fbuf[512];    /* buffer to build flush strings in */
 char *cmd;         /* show, cen, right, true command */
 int freecmd = FALSE;  /* flag indicating that cmd was allocated */


 if( optr == 0 )  /* nothing to do so return */
  {                                        /* but..... */
   if( flags2 & F2_SETFONT )          /* reset font if flag is on before  */
    {                                 /* going back to caller */
     FMsetfont( curfont, textsize );  /* set the new font in file */
     flags2 &= ~F2_SETFONT;           /* turnoff setfont flag */
    }
   return;
  }

 cury += textsize + textspace;   /* keep track of where we are */

 if( cury >= boty )      /* if this would take us off the page... */
  {
   FMceject( 0 );          /* move on if past the end */
#ifdef KEEP
commented out because it puts a para before a line and we are not
page breaking.
   if( rhead != NULL  &&  lilist == NULL && dlstackp == -1 )
    {                           /* if a header - insert a blank line at top */
     AFIwrite( ofile, "\\par" );      /* if not in a list */
    }
#endif
  }

 if( (flags2 & (F2_CENTER | F2_RIGHT | F2_TRUECHAR)) == 0 )  /* no flags */
  cmd = "show";
 else
  if( flags2 & F2_CENTER )           /* centering text? */
   {
    if( rflags & RF_PAR )
     AFIwrite( ofile, "\\qc" );
    else
     AFIwrite( ofile, "\\par\\qc" );
   }
  else
   if( flags2 & F2_RIGHT )           /* place text to the right? */
    {
     if( rflags & RF_PAR )
      AFIwrite( ofile, "\\qr" );
     else
      AFIwrite( ofile, "\\par\\qr" );
    }

 if( flags2 & F2_SETFONT )   /* need to reset font to text font? */
  {
   FMsetfont( curfont, textsize );  /* set the new font in file */
   flags2 &= ~F2_SETFONT;           /* turnoff setfont flag */
  }


 /* copy only the cached portion of the output buffer */
 /*sprintf( fbuf, "%s", obuf );*/
 AFIwrite( ofile, obuf );   /* write the text out to the file */


/*printf( "FLUSH: @%d (%.50s)\n", cury, fbuf ); */

 if( flags2 & F2_CENTER || flags2 & F2_RIGHT )   /* need to stop center/ri */
  {
   AFIwrite( ofile, "\\par\\qj" );  /* term centered line */
   rflags |= RF_PAR;                /* set par flag as weve just done this */
  }
 else
  rflags &= ~RF_PAR;            /* turn off paragraph writen */

 if( freecmd == TRUE )          /* need to free cmd buffer? */
  free( cmd );


 rflags &= ~RF_SBREAK;           /* turn off the break issued flag */
 optr = 0;
}                               /* FMflush */
