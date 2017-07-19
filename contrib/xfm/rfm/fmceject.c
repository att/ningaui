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
*  Mnemonic: FMceject
*  Abstract: This rotine ejects the current column such that the next tokin
*            is placed at the top of the next column on the page. If the
*            current column is the last defined column then pflush is
*            called to flush the page and then the first column is selected.
*            FMflush MUST be called prior to this routine... This routine
*            cannot call FMflush as it is called by FMflush and a loop might
*            occur.
*  Parms:    force - forces hard eject if true
*  Date:     6 May 1992
*  Returns:  Nothing.
*  Author:   E. Scott Daniels
*
*  Modified: 13 Jul 1994 - To convert t rfm
*             8 Dec 1994 - To write par mark at top only if not in def list.
*            27 Jan 2000 - To not put hard page if in single col mode to
*                          prevent small # words with large spaces on last ln
****************************************************************************
*/
void FMceject( int force )
{
 int diff;     /* difference between lmar in col block and current lmar */
 int diffh;    /* difference betweeh header lmar in col block and current */
 int diffx;    /* diff between x value in list blk and curcol lmar */

 if( flags2 & F2_BOX )    /* if a box is inprogress */
  FMbxend( );             /* then end the box right here */

 FMendlist( FALSE );         /* putout listitem mark characters if in list */

 diffh = hlmar - cur_col->lmar;  /* figure difference between col left mar */
 diff = lmar - cur_col->lmar;   /* and what has been set using .in and .hm */
 if( lilist != NULL )                               /* if list in progress */
  diffx = lilist->xpos - cur_col->lmar;            /* then calc difference */

 if( cur_col->next != NULL )       /* if this is not the last on the page */
  {
   cur_col = cur_col->next;                              /* then select it */
   if( ((rflags & RF_PAR) == 0) && (lilist == NULL) )
    AFIwrite( ofile, "\\par" );                         /* need a par mark */
   FMsetcol( col_gutter );                /* write out col def information */
  }
 else
  {
   if( force || firstcol->next ) /* only do a hard page if in multi col mode */
    FMpflush( );                 /* force a page eject with headers etc */
   cur_col = firstcol;           /* select the first column */
  }

 lmar = cur_col->lmar + diff;   /* set lmar based on difference calculated */
 hlmar = cur_col->lmar + diffh;      /* earlier and next columns left edge */
 if( lilist != NULL )              /* if list then reset xpos for next col */
  lilist->xpos = cur_col->lmar + diffx;

 cury = topy;                           /* make sure were at the top */
 if( rtopy > 0  &&  rtopcount > 0 )     /* reset real topy if needed */
   if( (--rtopcount) == 0 )             /* need to reset */
    {
     topy = rtopy;
     rtopy = 0;
    }

 if( flags2 & F2_BOX )    /* if a box is inprogress */
	FMbxstart( FALSE, 0, 0, 0, 0 );
}                         /* FMceject */
