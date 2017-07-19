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
* --------------------------------------------------------------------------
*  Mnemonic: fmtable 
*  Abstract: Start a table
*  Date:     26 Oct 2001 - converted from the hfm flavour 
*  Author:   E. Scott Daniels
*  Mods:
*		10 Apr 2007 - Memory leak cleanup.
*  		10 Oct 2007 : Changed to keep the original col anchor
*		29 Oct 2007 - Adding some trace points.
*		06 Nov 2007 - Corrected table width calculation.
* 
*  The n option allows user to turn off (no execute) the automatic
*  creation of the first cell. This allows them to call .tr and .cl 
*  straight off in order to supply special values for that cell and row
* 	.ta [b] [n] [a=align] [w=xx] [xi...]
*  	.et
* 	.cl [c=bgcolour] [t=fgcolour] [s=span-cols] [a=align-type] (ignored: r= v=  t=)
*	.tr [n] [c=bgcolour] [a=alignval] [v=valignvalue]
*	.th string
*  omitting w= allows browser to scale table. xx is a percentage.
*
* --------------------------------------------------------------------------
*/
void tab_vlines( struct table_mgt_blk *t, int setfree );

void FMtable( )
{
	struct table_mgt_blk *t;
	struct	col_blk 	*col;
	char colour[50];            /* spot to build colour string in */
	char align[50];
	char valign[50];
	char space[50]; 
	int do_cell = 1;         /* n option turns off */
	int len;
	char *ptr;
	int border = 0;          /* border size */
	char wstr[50];           /* spot to build width stirng in */
	int w = 100;

/*
	if( ! rtopy )
		rtopy = topy;
*/

	space[0] = 0;
	align[0] = 0;
	wstr[0] = 0;
	sprintf( valign, "valign=top" );   /* default to vert as brows def is cent*/

	FMflush( );                         /* terminate the line in progress */

	t = table_stack[ts_index++] = (struct table_mgt_blk *) malloc( sizeof( struct table_mgt_blk) );

	memset( t, 0, sizeof( struct table_mgt_blk ) );
	t->col_list = firstcol;		/* hold things that need to be restored when done */
	t->cur_col = cur_col; 
	t->topy = cury;
	t->old_topy = topy;
	t->old_linelen = linelen;
	t->lmar = lmar;
	t->hlmar = hlmar;
	t->padding = 5;
	t->maxy = cury;


	firstcol = NULL;
	cur_col = NULL;

	curcell = 1;        /* tableinfo[0] has cell count */
	colour[0] = (char) 0;

	while( (len = FMgetparm( &ptr )) != 0 )
		switch( *ptr )
		{
			case 'a': 
				sprintf( align, "align=%.13s", ptr + 2 );
				break;

			case 'b':   
				if( *(ptr+1) == '=' )
					border = atoi( ptr + 2 );
				else
					border++;
				break;

			case 'c':
/*
				t->bgcolour = strdup( ptr + 2 );
				sprintf( colour, "bgcolor=%.13s", t->bgcolour );
*/
				break;

			case 'n':
				do_cell = 0;    /* dont auto do a cell */
				break;

			case 'p':
				t->padding = atoi( ptr + 2 );
				break;
	
			case 's':
				sprintf( space, "cellspacing=%.13s", ptr +2 );
				break;

			case 't':
/*
				t->fgcolour = strdup( ptr + 2 );
				textcolour = t->fgcolour;
				flags2 |= F2_SETFONT;
*/
				break;

			case 'v': 
				sprintf( valign, "valign=%.13s", ptr + 2 );
				break;

			case 'w':          /* w=xxx percent of width to use for table */
				memcpy( wstr, ptr+2, len - 2 );
				wstr[len-2] = 0;
				w = atoi( wstr );
				sprintf( wstr, "width=%d%%", w );
				break;

			default:          /* assume column width definition */
				if( curcell < MAX_CELLS )
				{
					col = (struct col_blk *) malloc( sizeof( struct col_blk ) );
					memset( col, 0, sizeof( *col ) );
					col->next = NULL;
					col->width = FMgetpts( ptr, len );
					/*t->border_width += col->width + (2 * t->padding );*/
					t->border_width += col->width + ( t->padding );
					if( cur_col )
					{
						col->lmar = cur_col->lmar + cur_col->width + t->padding;
						cur_col->next = col;
						cur_col = col;
					}
					else
					{
						col->lmar = lmar + t->padding;
						cur_col = firstcol = col;
					}
					curcell++;                                 /* cell counter */
				}
				break;
		}

	firstcol->anchor =  t->col_list->anchor;		/* cary the anchor over for running head/feet */

	t->border = border;
	t->width = (linelen * w)/100;
	cur_col = firstcol;
	if( do_cell )
		linelen = cur_col->width - t->padding;
	else
		for( cur_col = firstcol; cur_col->next; cur_col = cur_col->next );

	lmar = firstcol->lmar + t->padding;

	if( ts_index > 1 )		/* if this is a table in a table */
	{
		t->topy -= t->padding;
		/*t->border_width += t->padding;*/
		topy = cury;			/* columns bounce back to here now */
	}
	else
	{
		topy = cury +t->padding;			/* columns bounce back to here now */
		if( border )			/* dont do outside for table in a table */
		{
			sprintf( obuf, "%d setlinewidth ", t->border );
			AFIwrite( ofile, obuf );
			sprintf( obuf, "%d -%d moveto %d %d rlineto stroke\n", t->lmar+t->padding, cury, t->border_width-t->padding, 0 );
			AFIwrite( ofile, obuf );
		}
	}

	cury += 3;			/* allow a bit of room for the line */

	if( trace > 1 )
	{
		struct	col_blk 	*cp;
		int	i = 0;
		fprintf( stderr, "table: lmar=%d borarder_width=%dp right_edge%dp\n", t->lmar, t->border_width, t->lmar+ t->border_width );
		for( cp = firstcol; cp; cp = cp->next )
			fprintf( stderr, "table: cell=%d lmar=%dp width=%dp\n", i++, cp->lmar, cp->width );

	}
}

/*
* --------------------------------------------------------------------------
*  Mnemonic: fmth
*  Abstract: save a table header placed into the table now, and if we page eject
*  Date:     02 Nov 2006 - converted from hfm
*  Author:   E. Scott Daniels
* 
*  .th string
* --------------------------------------------------------------------------
*/
void FMth( )
{
	struct table_mgt_blk *t;
	char	*buf;
	int	len; 
	int	totlen = 0;
	char	data[4096];

	if( (t = table_stack[ts_index-1]) == NULL )
	{
		while( (len = FMgetparm( &buf )) != 0 );		/* just dump the record */
		return;
	}
			
	*data = 0;
	while( (len = FMgetparm( &buf )) != 0 )
	{
		if( len + totlen + 1 < sizeof( data ) )
		{
			strcat( data, buf );
			strcat( data, " " );
		}
		totlen += len + 1;
	}

	if( t->header )
		free( t->header );
	t->header = strdup( data );

	TRACE( 2 ) "tab_header: header in place: %s\n", t->header );
	AFIpushtoken( fptr->file, t->header );			/* fmtr calls get parm; prevent eating things */
	
}

/* go to the next cell */
/*
* --------------------------------------------------------------------------
*  Mnemonic: fmcell
*  Abstract: Start  the next cell
*  Date:     26 Oct 2001 - converted from hfm
*  Author:   E. Scott Daniels
* 
*  .cl [c=bgcolour] [t=fgcolour] [s=span-cols] [a=align-type]
* --------------------------------------------------------------------------
*/

void FMcell( int parms )
{
	struct table_mgt_blk *t;
	int	span = 1;	/* number of defined columns to span */
	char buf[256];
	char buf2[256];
	char colour[50];        /* buffer to build bgcolor= string in */
	char rspan[50];          /* buffer to build rowspan= string in */
	char align[50];         /* buffer for align= string */
	char valign[50];         /* buffer for align= string */
	int len;
	char *ptr;

	align[0] = 0;
	buf[0] = 0;
	buf2[0] = 0;
	rspan[0] = 0;
	valign[0] = 0;
	colour[0] = 0;
	sprintf( valign, "valign=top" );

	if( ! table_stack[0] )		/* ignore if not in table */
	{
		/*fprintf( stderr, "(%s:%d) cell command table not started\n", fptr->name, fptr->count ); */
		return;
	}

	FMflush( );                       /* flush what was in the buffer first */
 
	t = table_stack[ts_index-1];

	/* at this point we recognise but ignore some hfm things */
	while( parms && (len = FMgetparm( &ptr )) != 0 )
		switch( *ptr )
		{
			case 'a':
				sprintf( align, "align=%s", ptr + 2 );
				break;
			case 'c':   
				sprintf( colour, "bgcolor=%s", ptr + 2 );
				break;
			case 'r':
				sprintf( rspan, "rowspan=%s", ptr + 2 );
				break;

			case 's':
				span = atoi( ptr+2 );
				break;

			case 't':      /* really does not make sense to support this */
				break;
		
			case 'v':
				sprintf( valign, "valign=%s", ptr + 2 );
				break;
		
			default: 
				break;
		}


	if( flags2 & F2_BOX )    /* if a box is inprogress */
		FMbxend( );             /* then end the box right here */

	FMendlist( FALSE );      /* putout listitem mark characters if in list */

				
	TRACE(2) " table/cell: cury=%d textsize=%d textspace=%d font=%s boty=%d topy=%d maxy=%d\n", cury, textsize, textspace, curfont, boty, topy, table_stack[ts_index-1]->maxy );

	if( cury > table_stack[ts_index-1]->maxy )
		table_stack[ts_index-1]->maxy = cury;

	if( cur_col->next != NULL )    		/* if this is not the last in the table */
		cur_col = cur_col->next;	/* then select it */
	else
		cur_col = firstcol;

	cury = topy;
	lmar = cur_col->lmar + t->padding;   /* set lmar based on difference calculated */
	hlmar = cur_col->lmar + t->padding; /* earlier and next columns left edge */

	if( lilist != NULL )           /* if list then reset xpos for next col */
		lilist->xpos = cur_col->lmar + t->padding;

	if( span < 1 )
		span = 1;

	linelen = 0;
	while( span )
	{
		linelen += cur_col->width;
	
		if( --span && cur_col->next )		/* still more to span */
		{
			cur_col = cur_col->next;
			cur_col->flags |= CF_SKIP;
		}
	}

	linelen -= t->padding;
}


/*
---------------------------------------------------------------------------
 Mnemonic: fmtr
 Abstract: start a new row in the current table
 Date: 		26 Oct 2001 - converted from hfml stuff
 The n option prevents cell from being called.
 syntax:   .tr [n] [c=bgcolour] [a=alignval] [v=valignvalue]

 Mods:		10 Apr 2007 -- fixed write of border to be in conditional.
---------------------------------------------------------------------------
*/
void FMtr( int last )
 {
	struct table_mgt_blk *t = NULL;
   char *ptr = NULL;             /* pointer at parms */
   int len = 0;
   int do_cell = 1;       /* turned off by n option */
   char colour[50];
   char align[50];
   char valign[50];
   char obuf[256];
	int row_top = 0;		/* used to calc the depth of each row */

   colour[0] = 0;
   align[0] = 0;
   valign[0] = 0;

	if( ! ts_index )
	{
		/*fprintf( stderr, "(%s:%d) table row command: table not started\n", fptr->name, fptr->count ); */
		return;			/* no table if idx is 0 */
	}

	t = table_stack[ts_index-1];
	/*t->maxy = cury;*/					/* force a reset */
	row_top = cury;
	TRACE(2) " table/tr: cury=%d textsize=%d textspace=%d font=%s boty=%d topy=%d maxy=%d\n", cury, textsize, textspace, curfont, boty, topy, table_stack[ts_index-1]->maxy );

	while( cur_col != firstcol )
		FMcell( 0 );				/* flush and calc maxy for the row */
	
	t->tot_row_depth += t->maxy - row_top;		/* add in depth of this column */
	t->nrows++;
	t->ave_row_depth = t->tot_row_depth / t->nrows; 	/* average depth of a row to predict end of page */


   while( (len = FMgetparm( &ptr )) != 0 )
    {
     switch( *ptr ) 
      {
       case 'a':
         sprintf( align, "align=%s", ptr + 2 );
         break;

       case 'c':
         sprintf( colour, "bgcolor=%s", ptr + 2 );
         break;

       case 'n':
         do_cell = 0;
         break;

       case 'v':
         sprintf( valign, "valign=%s", ptr + 2 );
         break;


       default: 
         break;
      }
    }

	cury = t->maxy + t->padding;

	if( t->border )
	{
		sprintf( obuf, "%d setlinewidth ", t->border );
		AFIwrite( ofile, obuf );

		tab_vlines( t, 0 );
		if( ! last )
		{
			sprintf( obuf, "%d -%d moveto %d %d rlineto stroke\n", t->lmar+t->padding, cury, t->border_width-t->padding, 0 );
			AFIwrite( ofile, obuf );
		}
	}

	if( cury + t->ave_row_depth >= boty - 35 )
	{
		topy = t->old_topy;
		FMpflush( );
		if( t->border )
		{
			sprintf( obuf, "%d -%d moveto %d %d rlineto stroke\n", t->lmar+t->padding, cury, t->border_width-t->padding, 0 );
			AFIwrite( ofile, obuf );
		}
		cury = topy;
		t->maxy = cury;
		if( t->header )
		{
			AFIpushtoken( fptr->file, ".tr :" );			/* must drop a table row after the header command(s) */
			AFIpushtoken( fptr->file, t->header );			/* fmtr calls get parm; prevent eating things */
		}
	}

	t->topy = cury;
	cury += t->padding;
	topy = cury;			/* columns need to bounce back to here */
	cur_col = firstcol;
 }

void tab_vlines( struct table_mgt_blk *t, int setfree )
{
	struct col_blk *c;
	struct col_blk *next;

	for( c = ts_index > 1 ? firstcol->next : firstcol; c; c = next )
	{
		sprintf( obuf, "%d setlinewidth ", t->border );
		AFIwrite( ofile, obuf );

		next = c->next;
		if( (c->flags & CF_SKIP) == 0 )
		{
			sprintf( obuf, "%d -%d moveto %d -%d lineto stroke\n", c->lmar, t->topy, c->lmar, cury );
			AFIwrite( ofile, obuf );
		}

		c->flags &= ~CF_SKIP;

		if( setfree )
			free( c );
	}

	if( t->border && ts_index < 2 )		/* no edges if table in a table */
	{
		int x;

		sprintf( obuf, "%d setlinewidth ", t->border );
		AFIwrite( ofile, obuf );

		x = (t->lmar + t->border_width); 
		sprintf( obuf, "%d -%d moveto %d -%d lineto stroke\n", x, t->topy, x, cury );
		AFIwrite( ofile, obuf );
	}
}

FMendtable( )
{

	struct	table_mgt_blk *t;
	struct	col_blk *next;
	int	i;
	int 	x;
	char	obuf[1024];


	if( ! (t = table_stack[ts_index-1]) )
		return;

	AFIpushtoken( fptr->file, ":" );			/* fmtr calls get parm; prevent eating things */
	FMtr( ts_index == 1 ? 0 : 1 );				/* flush out last row */

	TRACE( 1 ) "end_table: rows=%d  total_depth=%d  ave_depth=%d\n", t->nrows, t->tot_row_depth, t->ave_row_depth );
	cury = t->maxy;
 	if( t->border )
		tab_vlines( t, 1 );
	else
		for( cur_col = firstcol; cur_col; cur_col = next )
		{
			next = cur_col->next;
			free( cur_col );
		}

	cur_col = t->cur_col;
	lmar = t->lmar;
	hlmar = t->hlmar;
	linelen = t->old_linelen;
	firstcol = t->col_list;
/*
	FMceject( );			*//* set up for normal columns */

	cury += 3;
#ifdef KEEP
/* setting the temp top is up to the user as the table might be in a left column and the next 
column needs to go to the top of the physical page.  If this is a page wide table, then 
the user must setup multiple columns after .et and set the temp top.  we cannot assume this 
to be the case
*/
	if( rtopy == 0 )			/* allow for multiple columns under the table */
		rtopy = t->old_topy;

	topy = cury;
#endif

	topy = t->old_topy;
	if( t->header )
		free( t->header );
	free( t );
	table_stack[ts_index-1] = NULL;
	if( ts_index > 0 )
		ts_index--;			/* keep it from going neg */
	if( ts_index )
		topy = table_stack[ts_index-1]->topy;
}
