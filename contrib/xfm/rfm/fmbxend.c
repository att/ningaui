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
*  Mnemonic: FMbxend
*  Abstract: This routine is called to "end" a box at the current y position.
*            The routine does NOT reset the box in progress flag so that this
*            routine can be called to end a box at the end of a page so that
*            we do not have open boxes flowing from page to page. This routine
*            must NOT call FMflush as it will cause an unexitable recursion
*            to occur.
*            This routine assumes that the postscript "box" routine has been
*            defined and has the following format:
*                 x1 y1 x2 y2 box
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     15 October 1992
*  Author:   E. Scott Daniels
*
*  Modified:  1 Jul 1994 - To convert to rfm.
*****************************************************************************
*/
void FMbxend( )
{
 char buf[255];      /* buffer to build output string in */
 int len;            /* length of output to write */
 int i;              /* loop index */
 int x;              /* xposition for drawing line */
 int almar;          /* absolute left margin of box for vert lines */

 flags2 &= ~F2_BOX;               /* turn off box to force hard pard */
 if( (rflags & RF_PAR) == 0 )
  FMpara( 0, TRUE );                 /* end paragraph and end box */
 else
  FMpara( 0, FALSE );

 flags2 |= F2_BOX;            /* turn flag back on */

}       /* FMbxend */
