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
**************************************************************************
*
*   Mnemonic: FMfigure
*   Abstract: This routine places a blank line and then the word "figure"
*             and figure number into the file. The line following this
*             line will contain the user input statement if entered on the
*             command line. This routine should later be modified to contain
*             the logic if a figure table will ever be generated. Figure
*             numbers are the section number followed by a sequential number.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     9 December 1988
*   Author:   E. Scott Daniels
*
*   Modified: 30 Oct 1992 - To reduce figure text size by two from current size
*             26 May 1993 - To reduce space between figure text lines.
*             22 Feb 1994 - To ensure text font is set properly
*	      13 Oct 2001 - To setup for fonts using the font blocks
*****************************************************************************
*/
void FMfigure( )
{
 int i;              /* loop index and parameter length */
 char *buf;          /* pointer at parameter list entered */
 int oldspace;       /* old text space value */
 char *oldfont;      /* hold old font to restore at end */
 int  oldsize;       /* hold old text size to restore at end */
 char	fbuf[100];

 oldspace = textspace;  /* save stuff we need to restore when leaving */
 oldfont = curfont;     /* save old font for later */
 oldsize = textsize;    /* save old size for end */

 FMflush( );            /* end current working line */
 FMfmt_add( );

 if( textsize >7 )
 {
	cury -= 2;
 	textsize -= 2;         /* set to size for the font string */
 }
 curfont = ffont == NULL ? curfont : ffont;   /* set figure font if there */

 if( flags & PARA_NUM )                      /* h-n style numbering if para num on */
	sprintf( fbuf, "Figure %d-%d: ", pnum[0], fig );   /* gen fig number */
 else			
	sprintf( fbuf, "Figure %d: ", fig );   /* gen fig number */

 fig++;                           /* increment the fig number */

 FMaddtok( fbuf, strlen( fbuf ) );	/* stick label into buffer */

 while( (i = FMgetparm( &buf )) > 0 )    /* until all parameters gone */
  FMaddtok( buf, i );                    /* add it to the output buffer */

 FMflush( );                             /* send user line on the way */

 textspace = oldspace;  /* restore original text space value */
 textsize  = oldsize;   /* restore the size that was set on entry */
 curfont = oldfont;     /* restore the font set on entry */
 FMfmt_add( );		/* initial font info for next buffer */
}
