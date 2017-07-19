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
* ---------------------------------------------------------------------
* Mnemonic:	fmfmt.c contains routines to add, purge, and otherwise 
*		manipulate the format_blk list.
* Date: 	9 October 2001
* Author:	E. Scott Daniels
*		10 Apr 2007 - Memory leak cleanup 
*		06 Nov 2007 - Pops leftover blocks when restoring state.
* ---------------------------------------------------------------------
*/

static struct format_blk *fmt_lst = NULL;	/* pointer to the current format list */
static struct format_blk *fmt_stack[25];
static int fmt_idx = 0;

int FMfmt_save( )
{
	fmt_stack[fmt_idx++] = fmt_lst;
	fmt_lst = NULL;
}

int FMfmt_restore( )
{
	int size;
	char *font;
	char *colour;
	int start;
	int end;

	while( FMfmt_pop( &size, &font, &colour, &start, &end ) > 0 );		/* pop current things */
	fmt_lst = fmt_stack[--fmt_idx];						/* point at old list */
}

int FMfmt_largest( )				/* find the largest font in the list */
{
	struct format_blk *f;
	int 	size = textsize;
	
	for( f = fmt_lst; f; f = f->next )
		if( f->size > size )
			size = f->size;

	return size;
}

/* mark the ending position of the top block */
void FMfmt_end( )
{
	if( fmt_lst )
		fmt_lst->eidx = optr - 2;
}

/* add a block to the head of the list. current font and text size are used 
   we add to the head because we need to spit out the the stuff in reverse order
   because ps is stacked 
*/
void FMfmt_add( )
{
	struct format_blk *new; 

	if( fmt_lst && fmt_lst->sidx == optr )		/* likely an .sf and .st command pair */
	{
		new = fmt_lst;			/* just reset the current one */
		if( new->font )
			free( new->font );
		new->font = strdup( curfont );
		new->size = textsize;
		if( textcolour )
			new->colour = strdup( textcolour );
	}
	else
	{
		if( (new = (struct format_blk *) malloc( sizeof( struct format_blk ) )) )
		{
			memset( new, 0, sizeof( struct format_blk ) );
			new->sidx = optr ? optr-1: 0;
			new->eidx = optr;
			if( curfont )
				new->font = strdup( curfont );
			new->size = textsize;
			if( textcolour )
				new->colour = strdup( textcolour );
	
			FMfmt_end( );
	
			new->next = fmt_lst;		/* push on */
			fmt_lst = new;
		}
		else
			fprintf( stderr, "*** no memory trying to get format block\n" );
	}
}

/* get info and pop the head block */
int FMfmt_pop( int *size, char **font, char **colour, int *start, int *end )
{
	struct format_blk *next;

	*colour = *font = NULL;

	if( fmt_lst )
	{
		*size = fmt_lst->size;
		*font = fmt_lst->font;			/* pass back strings we duped earlier */
		*colour =  fmt_lst->colour;
		*start = fmt_lst->sidx;
		*end = fmt_lst->eidx;

		next = fmt_lst->next;			/* remove from queue and purge */

		free( fmt_lst );

		fmt_lst = next;

		return 1;
	}
	else
	{
		*size = textsize;
		if( *font )
			free( *font );
		*font = strdup( curfont );
		*start = *end = -1;
		*colour = strdup( "000000" );
	}

	return 0;
}

