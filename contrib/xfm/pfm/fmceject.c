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
*  Abstract: This rotine ejects the current column such that the next token
*            is placed at the top of the next column on the page. If the
*            current column is the last defined column then pflush is
*            called to flush the page and then the first column is selected.
*            FMflush MUST be called prior to this routine... This routine
*            cannot call FMflush as it is called by FMflush and a loop might
*            occur.
*  Parms:    None.
*  Date:     6 May 1992
*  Returns:  Nothing.
*  Author:   E. Scott Daniels
*
*  Modified: 15 Oct 1992 - To call box end and box start if in a box
*            19 Oct 1992 - To call end list routine to put out list item marks
*            21 Oct 1992 - To reset hlmar for new column
*            25 Mar 1993 - To properly handle list item xmar when in multi col
*             7 Apr 1994 - To check reset of real top y
*		26 Oct 2006 - Now uses FMatejet() to do stacked eject commands
			and to allow a set of commands on the .oe command.
****************************************************************************
*/
void FMceject( )
{
	int diff;     /* difference between lmar in col block and current lmar */
	int diffh;    /* difference betweeh header lmar in col block and current */
	int diffx;    /* diff between x value in list blk and curcol lmar */
	char *tok;

	if( flags2 & F2_BOX )    /* if a box is inprogress */
		FMbxend( );             /* then end the box right here */

	FMendlist( FALSE );      /* putout listitem mark characters if in list */

	diffh = hlmar - cur_col->lmar; /* figure difference between col left mar */
	diff = lmar - cur_col->lmar;   /* and what has been set using .in and .hm */
	if( lilist != NULL )           /* if list in progress */
	diffx = lilist->xpos - cur_col->lmar;  /* then calc difference */

	/* moved before peject incase it calls an oneject that puts in a drawing */
	cury = topy;                   			/* make sure were at the top */
	if( cur_col && cur_col->next != NULL )    /* if this is not the last on the page */
		cur_col = cur_col->next;       /* then select it */
	else
	{
		if( table_stack[0] )
			return;

		if( table_stack[0] == NULL )	/* we dont do page flush if in a table */
   			FMpflush( );           /* cause a page eject with headers etc */
	
		cur_col = firstcol;  /* select the first column */
  	}

	lmar = cur_col->lmar + diff;   /* set lmar based on difference calculated */
	hlmar = cur_col->lmar + diffh; /* earlier and next columns left edge */
	if( lilist != NULL )           /* if list then reset xpos for next col */
	lilist->xpos = cur_col->lmar + diffx;
	
	if( rtopy > 0  &&  rtopcount > 0 )     /* reset real topy if needed */
		if( (--rtopcount) == 0 )    /* need to reset */
		{
			topy = rtopy;
			rtopy = 0;
		}

	if( flags2 & F2_BOX )    /* if a box is inprogress */
		FMbxstart( FALSE, 0, 0, 0, 0 );     /* reset top of box to current y position */

	TRACE( 2 ) "ceject: new-lmar=%d new-hlmar=%d\n", lmar, hlmar );
	FMateject( 0 );			/* do any queued on eject commands */



#ifdef KEEP_OLD_WAY
/* if user wants to embed multiple commands on an oe statement, then they need to 
   put them in a file and oe should just have the .im command
*/
 if( oncoleject )
 {
	if( trace > 1 )
		fprintf( stderr, "ceject: oe (next col) = (%s)\n", oncoleject );

 	AFIpushtoken( fptr->file, oncoleject );  /* and push onto the input stack */
	free( oncoleject );
	oncoleject = NULL;

	FMpush_state( );
   	flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   	flags2 &= ~F2_ASIS;                  /* turn off asis flag */
	if( FMgettok( &tok ) )
		FMcmd( tok );
	FMpop_state( );
 }
 else
	if( onallcoleject )
	{
		if( trace > 1 )
			fprintf( stderr, "ceject: oe (all cols) = (%s)\n", oncoleject );
 		AFIpushtoken( fptr->file, onallcoleject );  
		FMpush_state( );
   		flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   		flags2 &= ~F2_ASIS;                  /* turn off asis flag */
		if( FMgettok( &tok ) )
			FMcmd( tok );
		FMpop_state( );
	}
#endif
}                  /* FMceject */
