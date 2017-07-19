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
* Mnemonic: FMbox
* Abstract: This routine interprets the .bx command and calls either bxstart
*           or the bxend routine to start or end a box.
* Parms:    None.
* Returns:  Nothing.
* Date:     Original 5 May 1989 Rewritten for Postscript 15 October 1992
* Author:   E. Scott Daniels
*
* Modified: 11 Nov 1992 - To call flush before start and end
*             so we dont flush on an h on/off call.
* .bx {start | end} [H] <vert col1> <vert col2>... <vert col10>
*      H - draw a horizontal line after each line of text
* .bx H {on | off} start/stop drawing horizontal lines
****************************************************************************
*/
void FMbox( )
{
 int len;        /* length of parameter entered on .bx command */
 char *buf;      /* pointer at the input buffer information */
 int col;        /* column for vertical line placement */
 int i;          /* loop index */

 len = FMgetparm( &buf );   /* get next parameter on command line */

 if( len > 0 && toupper( buf[0] ) == 'S' )  /* start box */
  {
   if( (flags2 & F2_BOX) == 0 )   /* if not inprogress already */
    {
     FMflush( );             /* flush out last line in the box */
     FMbxstart( TRUE, 0, 0, 0, 0 );           /* initialize box block */
     flags2 |= F2_BOX;            /* indicate box in progress */
    }
  }
 else
  if( len == 0 || toupper( buf[0] ) == 'E' ) /* no parm or end asked for */
   {
    if( flags2 & F2_BOX )   /* if one is inprogress */
     {
      FMflush( );             /* flush out last line in the box */
      FMbxend( );             /* then terminate the box here */
      flags2 &= ~F2_BOX;      /* indicate box has stopped */
     }
   }
  else
   if( toupper( buf[0] ) == 'H' )    /* start or stop drawing horiz lnes */
    {
     len = FMgetparm( &buf );     /* get the next parameter */
     if( len > 0 && toupper( buf[1] ) == 'N' )   /* turn on horiz lines */
      box.hconst = TRUE;
     else                       /* default to turning them off */
      box.hconst = FALSE;
    }       /* end handle horizontal line stuff */
}                        /* FMbox */
