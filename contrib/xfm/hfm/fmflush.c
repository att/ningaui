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


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
****************************************************************************
*
*   Mnemonic: FMflush
*   Abstract: This routine is responsible for flushing the output buffer
*             to the output file and resetting the output buffer pointer.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     15 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 29 Jun 1994 - To convert to rfm.
*              6 Dec 1994 - To reset font even if nothing to write.
*              7 Dec 1994 - To prevent par mark at page top if in li list
*             14 Dec 1994 - To prevent par mark at top if in di list
*              6 Dec 1996 - To convert for hfm
*              9 Nov 1998 - To remove </font> - Netscape seems unable to handle them properly
*             22 Jan 2000 - To strncmp for color rather than strcmp
*		27 Nov 2002 - To add no font support
****************************************************************************
*/
void FMflush( )
{
	int len;           /* length of the output string */
	char fbuf[512];    /* buffer to build flush strings in */
	char colour[50];   /* colour string if textcolour defined */

	colour[0] = 0;

	if( optr == 0 )  /* nothing to do so return */
		return;

	if( flags2 & F2_SETFONT && ! (flags3 & F3_NOFONT) )    	/* no font means we are in a css div */
	{                           
		if( textcolour )
			sprintf( colour, "color=%s ", textcolour );
		sprintf( fbuf, "<font size=%d %s>", textsize < 1 ? 2 : textsize, colour );
		AFIwrite( ofile, fbuf );
		flags2 &= ~F2_SETFONT;
	}


	AFIwrite( ofile, obuf );   /* write the text out to the file */

	*obuf = EOS;             /* make it end of string */
	optr = 0;

	if( flags2 & F2_CENTER )       /* need to write out end center tag */
		AFIwrite( ofile, "</center>" );

	optr = 0;
}                               /* FMflush */
