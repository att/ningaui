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
*  Mnemonic: FMflush
*  Abstract: This routine is responsible for sending out the current buffer
*            with the proper formatting and no justificaiton.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     12 November 1992
*  Author:   E. Scott Daniels
*
*  Modified: 10 Oct 2001 - Converted to use format blocks
*	     20 Nov 2002 - Added kludgey way to set colours, provides some support, but not 
*			as finite of control as in hfm.
*		10 Apr 2007 - Memory leak cleanup 
*****************************************************************************
*/
void FMflush( )
{
	int 	i;		/* loop index */
	int	j;
	int	size;		/* info about a format segment */
	char	*font;
	char	*colour;
	int	start;
	int	end;
	int	largest;	/* largest font size on the line */
	int	things = 0;	/* number of tripples in the stack */
	int	first = 1;	/* true if working with first block from the list */
	int	last_cury;	/* cury before inc -- incase we need to ceject */
 	char 	jbuf[1024];    /* initial work buffer */
 	char 	jjbuf[1024];    /* work buffer */

	FMsetcolour( textcolour );

	if( optr == 0 )
		return;

 	FMfmt_end( );		/* mark the last format block as ending here */

	largest = FMfmt_largest( );	/* figure out largest point size for y positioning */
 
	last_cury = cury;
   	cury += largest + textspace;    /* move to the next y position */
   	if( flags & DOUBLESPACE )        /* if in double space mode ...    */
    		cury += textsize + textspace;   /* then skip down one more */

  	if( cury > boty )               /* are we out of bounds? */
	{
		cury = last_cury;
 		FMceject( );       /* move to next column */
 	}

	sprintf( jbuf, "%d -%d moveto\n", lmar, cury );  /* create moveto */
	AFIwrite( ofile, jbuf );      /* write the move to command or x,y for cen */

	TRACE(2)  "flush: lmar=%d cury=%d boty=%d obuf=(%s)\n", lmar, cury, boty, obuf );

 	while( FMfmt_pop( &size, &font, &colour, &start, &end ) )  /* run each formatting block put on the list */
 	{
		for( j = 0, i = 0; i <= end - start; i++ )		 /* generate (string) (fontname) fontsize */
		{
			if( obuf[start+i] == '^' )
				i++;
			jbuf[j++] = obuf[start+i];
		}
		jbuf[j] = 0;

		TRACE( 2 ) "flush: jbuf=(%s) start=%d end=%d size=%d font=%s\n", jbuf, start, end, size, font );
		sprintf( jjbuf, "(%s) (%s) %d ", font, jbuf, textsize );   
   		AFIwrite( ofile, jjbuf );               				/* output the information */
		things++;

		first = 0;							/* no longer working with the head */
		free( font );
	}
 
	if( things <= 0 )
		return;

   	if( flags2 & F2_CENTER )      /* set up for center command */
		sprintf( jbuf, " %d %d cent\n", linelen, things );
	else
   	if( flags2 & F2_RIGHT )      /* set up for right alignment command */
		sprintf( jbuf, " %d %d right\n", linelen, things );
	else	
		sprintf( jbuf, " %d splat\n", things );

	AFIwrite( ofile, jbuf );

	*obuf = 0;
	optr = 0;		/* reset the output buffer */
	FMfmt_add( );		/* add the current font back to the list */
}
