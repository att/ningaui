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
*************************************************************************
*
*   Mnemonic: FMheader
*   Abstract: This routine is responsible for putting a paragraph header
*             into the output buffer based on the information in the
*             header options buffer that is passed in.
*   Parms:    hptr - Pointer to header information we will use
*   Returns:  Nothing.
*   Date:     1 December 1988
*   Author:   E. Scott Daniels
*
*   Modified: 12-03-88 to space down topmar lines if page eject flag set.
*             12-08-88 not to space down past topmar if at start of col.
*             3 Feb 1989 - Not to reset figure number if not paragraph
*                          numbering.
*             7 Jul 1989 - To support variable expansion in headers.
*             23 Apr 1991- To add header left margin support.
*              5 May 1992- To support postscript conversion
*              7 Nov 1992- To skip before and after from header block
*             23 Nov 1992- To call ccol to ensure 5 lines remain in column
*	      13 Oct 2001 - To use the formatting blocks properly.
*	      27 Nov 2001 - To calc ccol value based on skip values
**************************************************************************
*/
void FMheader( hptr )
struct header_blk *hptr;
{
	int len;            /* len of parm token */
	int j;              /* index variables */
	int i;
	char buf[2048];     /* buffer to build stuff in */
	char *ptr;          /* pointer to header string */
	int oldlmar;        /* temp storage for left mar value */
	int oldsize;        /* temp storage for text size value */
	char *oldfont;      /* pointer to old font sring */

	FMflush( );          /* flush the current line */

	cury += (textsize + textspace) * (hptr->skip/10);  /* skip before */

	if( (hptr->flags & HEJECTC) && cury != topy ) /* if coleject and not there */
		FMceject( );
	else
		if( (hptr->flags & HEJECTP) && (cur_col != firstcol || cury > topy ) )
			FMpflush( );       					/* these go to top of page */
		else
			FMccol( (hptr->skip/10) + (hptr->skip%10) + 2 );      /* prevent header alone */


	pnum[(hptr->level)-1] += 1;     		 /* increase the paragraph number */

					/* preserve everything and set up header font etc */
	TRACE( 3 ) "header: preserving: lmar=%d curfont=%s textsize=%d\n", lmar, curfont, textsize );
	oldlmar = lmar;                  /* save left margin */
	oldfont = curfont;               /* save current text font */
	oldsize = textsize;              /* save old text size */
	if( hptr->level < 4 );           /* unindent if level is less than 4 */
		lmar = cur_col->lmar + hptr->hmoffset;
	textsize = hptr->size;           /* set text size to header size for flush */
	curfont = hptr->font;            /* set font to header font for flush */
	FMfmt_add( );

	if( hptr->level == 1 && (flags & PARA_NUM) )
		fig = 1;                          		/* restart figure numbers */

	for( i = hptr->level; i < MAX_HLEVELS; i++ )
		pnum[i] = 0;                    	   /* zero out all lower p numbers */

	if( flags & PARA_NUM )         			 /* number the paragraph if necessary */
	{
		sprintf( buf, "%d.%d.%d.%d", pnum[0], pnum[1], pnum[2], pnum[3] );
		for( i = 0, j = 0; buf[i] && j < hptr->level;  i++ )
		{
			if( buf[i] == '.' )
				j++;
		}             
		buf[i] = 0;
		FMaddtok( buf, strlen( buf ) );
	}

	while( (len = FMgetparm( &ptr )) > 0 )    /* get next token in string */
	{
		if( hptr->flags & HTOUPPER )
			for( i = 0; i < len; i++ )
				ptr[i] = toupper( ptr[i] );
		FMaddtok( ptr, len );
	}

				/* the call to FMtoc must be made while the header is in obuf */
	if( hptr->flags & HTOC )       /* put this in table of contents? */
		FMtoc( hptr->level );         /* put entry in the toc file */

	if( hptr->level < 4 )
		FMflush( );                      /* writeout the header line */

	lmar = oldlmar;                  /* restore left margin value */
	textsize = oldsize;              /* restore text size */
	curfont = oldfont;               /* restore font too */
	TRACE( 3 ) "header: restored: lmar=%d curfont=%s textsize=%d\n", lmar, curfont, textsize );
	FMfmt_add( );			/* setup for next buffer */

	if( hptr->level < 4 )      /* indent if not header level 4 */
	{
		cury += (textsize + textspace) * (hptr->skip%10); 		/* skip after */
		if( cury % 2 )
			cury++;		/* ensure its even after a header */
		for( optr = 0, i = 0; i < hptr->indent; i++, optr++ )	/* indent */
			obuf[optr] = BLANK;
		obuf[optr] = EOS;                       /* terminate incase flush called */
		osize = FMtoksize( obuf, hptr->indent );  /* set size of blanks */
	}
	else       /* for level 4 place text next to the header */
	{
		obuf[optr] = BLANK;
		optr++;
		obuf[optr] = EOS;
		osize = FMtoksize( obuf, strlen( obuf ) );  /* set size of stuff there */
	}
}                    /* FMheader */
