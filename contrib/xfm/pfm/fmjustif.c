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
*  Mnemonic: FMjustify
*  Abstract: This routine is responsible for sending out the current buffer
*            with the proper justification command and related parameters.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     12 November 1992
*  Author:   E. Scott Daniels
*
*  Modified: 04 Dec 1992 - To call flush if spaces are 0
*            10 Dec 1992 - To use AFI routines for ansi compatability
*             6 Apr 1994 - To take advantage of linelen being points now
*	     20 Nov 2002 - Added kludgey way to set colours, provides some support, but not 
*			as finite of control as in hfm.
*		10 Apr 2007 - Memory leak cleanup 
*****************************************************************************
*/
void FMjustify( )
{
 	char jbuf[1024];    /* initial work buffer */
 	char jjbuf[1024];    /* work buffer */
	int 	i;		/* loop index */
	int	j;
	int	size = 0;		/* info about a format segment */
	char	*font = NULL;
	char	*colour = NULL;
	int	start = 0;
	int	end = 0;
	int	largest = 0;		/* largest font size on the line */
	int	things = 0;		/* number of tripples in the just stack */
	int	blanks = 0;		/* number of blanks in the string */
	int	first = 1;		/* true if working with first block from the list */

	FMsetcolour( textcolour );

 	FMfmt_end( );		/* mark the last format block as ending here */

	largest = FMfmt_largest( );
 
   	cury += largest + textspace;    /* move to the next y position */
   	if( flags & DOUBLESPACE )        /* if in double space mode ...    */
    		cury += textsize + textspace;   /* then skip down one more */

  	if( cury > boty )               /* are we out of bounds? */
 		FMceject( );       /* move to next column */

	sprintf( jbuf, "%d -%d moveto\n", lmar, cury );  	/* create moveto */
	AFIwrite( ofile, jbuf );      				/* write the move to command or x,y for cen */

 	while( FMfmt_pop( &size, &font, &colour, &start, &end ) )	/* run each fmt block put on the list */
 	{		 /* to generate (string) (fontname) fontsize tripples while counting blanks */
		if( end - start >= 0 )		/* dont do a fmt that has no text with it */
		{
			for( j = 0, i = 0; i <= end - start; i++ )
			{
				if( obuf[start+i] == ' ' )
					blanks++;
				else
					if( obuf[start+i] == '^' )
						i++;
				jbuf[j++] = obuf[start+i];
			}

			while( jbuf[j-1] == ' ' )		/* trim trailing blanks */
			{
				j--;
				blanks--;
			}
			jbuf[j] = 0;

			TRACE( 2 ) "justif: lmar=%d cury=%d jbuf=(%s)\n", lmar, cury, jbuf );
			if( i )
			{
				sprintf( jjbuf, "(%s) (%s) %d ", font, jbuf, textsize );    /* no trail space  */

   				AFIwrite( ofile, jjbuf );               /* output the information */
				things++;
			}
			first = 0;			/* no longer working with the head */
		}

		if( font )
			free( font );
		if( colour )
			free( colour );
	}
	if( font )
		free( font );
	if( colour )
		free( colour );
 
	if( things )		/* actually wrote something, finish it off */
	{
		sprintf( jbuf, " %d %d %d just\n", things, linelen, blanks ? blanks : 1 );
		AFIwrite( ofile, jbuf );
	}

	optr = 0;		/* reset the output buffer */
	*obuf = 0;
	FMfmt_add( );		/* add the current font back to the list */
}           /* FMjustify */
