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
*   Mnemonic:  FMcd
*   Abstract:  This routine parses the parameters on the .cd command and
*              builds the required column blocks.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      1 December 1988
*   Author:    E. Scott Daniels
*
*   .cd n a=v[ip] w=v[ip] i=v[ip] g=v[ip]
*          	n - The number of columns     (required)
*	   	a=  The anchor indention of first col for running footers etc. indent value used if not supplied.
*           	w=  The width of each column  (optional)
*           	i=  The indention of col 1 from edge of page (optional)
*           	g=  The gutter space between each column of each column. (optional)
*
*   Modified:  3 Feb 1989 - To issue warning if new col size is larger than
*                           the line length and left margin combination.
*             23 Oct 1992 - To convert to postscript
*              6 Apr 1994 - To accept indention and gutter sizes
*		10 Oct 2007 (sd) - Added a=v[ip] support to allow anchor setting for head/feet
***************************************************************************
*/
void FMcd( )
{
	char *buf;                 /* pointer at the token */
	int gutter = DEF_GUTTER;   /* gutter value between cols */
	int i;                     /* loop index */
	int j;
	int len;
	int diff;
	int diffh;			/* calcuation of new margins */
	int diffx;
	struct col_blk *ptr;  /* pointer at newly allocated blocks */

	len = FMgetparm( &buf );    /* get the number of columns requested */
	if( len <= 0 )              /* if nothing then get lost */
	return;

	diffh = hlmar - cur_col->lmar; 	/* figure difference between col left mar */
	diff = lmar - cur_col->lmar;   	/* and what has been set using .in and .hm */
	if( lilist != NULL )           	/* if list in progress */
	diffx = lilist->xpos - cur_col->lmar;  /* then calc difference */

	while( firstcol != NULL )   /* lets delete the blocks that are there */
	{
	 	ptr = firstcol;
	 	firstcol = firstcol->next;   /* step ahead */
	 	free( ptr );                 /* loose it */
	}

	i = atoi( buf );               /* convert parm to integer - number to create */
	if( i <= 0 || i > MAX_COLS )
		i = 1;                        /* default to one column */

	firstcol = NULL;       /* initially nothing */

	for( i; i > 0; i-- )   /* create new col management blocks */
	{
	 	ptr = (struct col_blk *) malloc( sizeof( struct col_blk ) );
	 	ptr->next = firstcol;    /* add to list */
	 	firstcol = ptr;
	} 

	firstcol->width = MAX_X;        /* default if not set by parameter */
	firstcol->lmar = DEF_LMAR;      /* default incase no overriding parm */
	firstcol->anchor = -1;		/* bogus -- we use lmar later if a= is not supplied */

	while( (len = FMgetparm( &buf )) > 0 )   /* while parameters to get */
	{
	 	switch( tolower( buf[0] ) )            /* check it out */
	  	{
			case 'a':			/* anchor point */
				firstcol->anchor = FMgetpts( &buf[2], len-2 );
				break;
	
	   		case 'y':			/* allow user to specify the starting y */
				cury = FMgetpts( &buf[2], len-2 );
				break;
	
	   		case 'i':                 /* indention value set for col 1 */
	    			firstcol->lmar = FMgetpts( &buf[2], len-2 ); /* convert to points */
	    			break;
	
	   		case 'g':                 /* gutter value supplied */
	    			gutter = FMgetpts( &buf[2], len-2 );   /* convert to points */
	    			break;
	
	   		case 'w':
	    			buf+=2;                  /* skip to the value and fall into default */
	    			len-=2;                  /* knock down to cover skipped w= */
	                             	/* **** NOTICE NO break here on purpose *** */
	   		default:                  /* assume a number is a width */
	    			if( isdigit( buf[0] ) || buf[0] == '.' )
	     			firstcol->width = FMgetpts( buf, len  );  /* convert to points */
	    			break;
	  	}
	} 

	if( firstcol->anchor < 0 )
		firstcol->anchor = firstcol->lmar;		/* default to left margin setting */


	for( ptr = firstcol; ptr->next != NULL; ptr = ptr->next )
	{
	 	ptr->next->width = ptr->width;       /* set width of next block */
	 	ptr->next->lmar = ptr->lmar + ptr->width + gutter;
	}

	if( linelen > firstcol->width ) /* change if too big incase they dont set */
		linelen = firstcol->width -5;    /* no msg, becuase they may set later */

	cur_col = firstcol;        /* set up current column pointer */

	TRACE(1) "col-def: anchor=%d lmar=%d width=%d g=%di\n", firstcol->anchor, firstcol->lmar, firstcol->width, gutter/72 );

	lmar = cur_col->lmar + diff;   	/* set lmar based on difference calculated */
	hlmar = cur_col->lmar + diffh; 	/* earlier and next columns left edge */

	if( lilist != NULL )           	/* if list then reset xpos for next col */
		lilist->xpos = cur_col->lmar + diffx;
}       
