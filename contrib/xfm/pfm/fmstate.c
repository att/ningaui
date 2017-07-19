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
* --------------------------------------------------------------------------------
* Mnemonic:	fmstate.c - push/pop state of things
* Abstract: 	At times during pfm processing its necessary to hold the current 
*		state of things. push saves the state and pop restores it to the 
*		last pushed value.
*
*		WARNING - The format stack is saved. If a set text/set font does
*		not happen right after the push then odd results will occur
*
* Date:		19 Nov 2001
* Author:	E. Scott Daniels
* Mods:		10 Apr 2007 - fixed leaks.
*		06 Nov 2007 - pushes a new format block when state is pushed
* --------------------------------------------------------------------------------
*/
static struct state {
	char	*obuf;
	char	*inbuf;
	char	*difont;
	int	optr;
	int 	lmar;
	int	llen;
	int	textsize;
	int	flags1;
	int	flags2;
	int	flags3;
	char	*textfont;
	char	*colour;
} state_stack[25];
static int state_idx = 0;

void FMpush_state( )
{
	TRACE( 2 ) "fmstate: pushed: lmar=%d\n", lmar );
	if( state_idx < 25 )
	{
		FMfmt_save( );
		FMfmt_add( );
		state_stack[state_idx].obuf = obuf;
		state_stack[state_idx].inbuf = inbuf;
		state_stack[state_idx].optr = optr;
		state_stack[state_idx].lmar = lmar;
		state_stack[state_idx].llen = linelen;
		state_stack[state_idx].textsize = textsize;
		state_stack[state_idx].textfont = curfont ? strdup( curfont ) : NULL;
		state_stack[state_idx].difont = difont ? strdup( difont ) : NULL;
		state_stack[state_idx].flags1 = flags;
		state_stack[state_idx].flags2 = flags2;
		state_stack[state_idx].flags3 = flags3;
		state_stack[state_idx].colour = textcolour;

		obuf = (char *) malloc( 2048 );
		inbuf = (char *) malloc( 2048 );

		memset( obuf, 0, 2048 );
		memset( inbuf, 0, 2048 );

		optr = 0;
		*obuf = 0;
	}

	state_idx++;
}

void FMpop_state( )
{
	if( state_idx > 0 )
	{
		if( --state_idx <= 24 )
		{
			TRACE( 2 ) "fmstate: popped: oldlmar=%d newlmar=%d\n", lmar, state_stack[state_idx].lmar );
			free( obuf );
			free( inbuf );
			free( curfont );
			free( difont );

			obuf = state_stack[state_idx].obuf;
			inbuf = state_stack[state_idx].inbuf;
			optr = state_stack[state_idx].optr;
			lmar = state_stack[state_idx].lmar;
			linelen = state_stack[state_idx].llen;
			textsize = state_stack[state_idx].textsize;
			curfont = state_stack[state_idx].textfont;
			difont = state_stack[state_idx].difont;
			textcolour = state_stack[state_idx].colour;
			FMfmt_restore( );
		}
	}
}
