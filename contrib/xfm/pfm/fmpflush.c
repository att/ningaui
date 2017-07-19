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
*    Mnemonic: FMpflush
*    Abstract: This routine causes the running header and footer to be placed
*              into the file and then issues the newp command which will issue
*              a showpage and then a translate.
*    Parms:    None.
*    Returns:  Nothing
*    Date:     1 December 1988
*    Author:   E. Scott Daniels
*
*    Modified: 22 Apr 1991 - To put header and footer and page under 2nd
*                              column if in two column mode.
*               3 May 1991 - To support two sided page format of headers.
*               4 May 1991 - To shift pages by pageshift size
*                            Broke header/footer code to FMrunout
*              13 May 1992 - Conversion to PostScript output
*              25 Oct 1992 - To call fmrunout with page and shift only.
*              27 Nov 1992 - To also set hlmar on new column
*              10 Dec 1992 - To use AFI routines for ansi compatability
*               7 Apr 1994 - To reset topy to realy if realy not 0.
*	       17 Aug 2001 - To add support for on eject command buffer
*****************************************************************************
*/
void FMpflush( )
{
	int diff;                   /* difference between default lmar and cur lmar*/
	int diffh;                  /* difference between default lmar and header */
	int i;                      /* index into page number buffer */
	char tbuf[HEADFOOT_SIZE+1]; /* buf to gen page and head/foots in  */
	int len;                    /* number of characters in the string */
	int even = FALSE;           /* even/odd page number flag */
	int skip;                   /* # columns to skip before writing string */
	int shift = FALSE;          /* local flag so if stmts are executed only once */
	char *tok;

	page++;                             /* increase the page number */

	if( rtopy != 0 )          /* see if top margin reset temporarily */
	{
		topy = rtopy;               /* reset as it is good only to the end of pg*/
		rtopy = 0;                  /* indicate nothing set at this point */
	}

                              /* determine if we need to shift the page */
	if( flags2 & F2_2SIDE )      /* if in two sided mode */
	{
		if( (page % 2) != 0 )      /* and its an odd page (only shift odd pages) */
		shift = TRUE;
	}
	else                         /* page shift not 0 and one sided mode */
		shift = TRUE;               /* then EACH page is shifted */

	FMsetcolour( "#000000" );	/* headers maintain a constant colour */
	FMrunout( page, shift );     /* putout header/footer and page strings */
	FMsetcolour( 0 );		/* forces colour to be reset on next flush */

	AFIwrite( ofile, "newp\n" );    /* newpage ps definition "showpage" */

	diff = lmar - cur_col->lmar;  /* calculate diff in col default mar& cur mar */
	diffh = hlmar - cur_col->lmar;  /* calc diff in header left margin */
	cur_col = firstcol;           /* start point at the first column block */
	lmar = cur_col->lmar + diff;  /* set up lmar for first column */
	hlmar = cur_col->lmar +diffh; /* set up header left margin for first col */

	cury = topy;                  /* reset current y to top y position */

	TRACE( 2 ) "pflush: new-lmar=%d new-hlmar=%d\n", lmar, hlmar );
	FMateject( 1 );		/* do page eject stuff first */


#ifdef KEEP_OLD_WAY
	if( onnxteject )		/* run the next eject command and then discard it */
	{
		TRACE(2) "peject: next eject = (%s)\n", onnxteject );
	
 		AFIpushtoken( fptr->file, onnxteject );  /* and push onto the input stack */
		free( onnxteject );
		onnxteject = NULL;
		FMpush_state( );
   		flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   		flags2 &= ~F2_ASIS;                  /* turn off asis flag */
		if( FMgettok( &tok ) )
			FMcmd( tok );
		FMpop_state( );
	}
	 
	if( oneveryeject )
	{
		TRACE(2) "peject: every eject = (%s)\n", onnxteject );
 		AFIpushtoken( fptr->file, oneveryeject );  
		FMpush_state( );
   		flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   		flags2 &= ~F2_ASIS;                  /* turn off asis flag */
		if( FMgettok( &tok ) )
			FMcmd( tok );
		FMpop_state( );
	}
#endif
}                              /* FMpflush */
