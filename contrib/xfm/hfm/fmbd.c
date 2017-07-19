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
*** this routine needs to be rewritten to get parms in any order
*** and not to require n to be supplied if right/font supplied
***
*   Mnemonic:  FMbd
*   Abstract:  This routine parses the .bd command to start a definition list
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      1 January 1989 (Happy New Year!)
*   Author:    E. Scott Daniels
*
*   Modified:  5 May 1992 - To support postscript conversion
*             12 Jul 1993 - To support right justification of terms and
*                           setting of a different font for terms.
*             21 Feb 1994 - To correct two col problem by keeping the amount
*                           indented in the stack and not the left marg value.
*              6 Apr 1994 - To call getpts to get point value of term size
*             10 Feb 2002 - To add auto skip and format options 
*
*    .bd <termsize[p|i]> [right] [font name] [s=n]
*         right - indicates that terms are to be right justified in the field
*         font  - name of the font to show terms in
*	  s=n - skip a line before each di.
*   Copyright (c) 1989  E. Scott Daniels. All rights reserved.
***************************************************************************
*/
void FMbd( )
{
	char *buf;           /* pointer at the token */
	int len;             /* len of parameter entered */
	int skip = 0;    	/* set to true if user wants automatic .sp with each .di */
	int tsize = 0;          /* requested term size - default is 1 inch = 72pts */
	int j = 0;
	int	anumflag = 0;
	int	astart = 1;
 	char *color = " "; 

	FMflush( );     		/* sweep the floor before we start */

	if( dlstackp >= MAX_DEFLIST )   			/* too deep */
	{
		while( FMgetparm( &buf ) );
		fprintf( stderr, "fmbd: too deep - bailing out \n" );
		return;
	}

	
	dlstackp++;				/* increase the definition list stack pointer */
	memset( &dlstack[dlstackp], 0, sizeof( dlstack[0] ) );
	if( textcolour )
		dlstack[dlstackp].colour = strdup( textcolour );

	while( (len = FMgetparm( &buf )) > 0 )    /* is there a length? */
	{                                    /* process the parameter(s) entered */
		if( ! j++ )
		{
			tsize = FMgetpts( buf, len );              /* get the term size in points */
			dlstack[dlstackp].indent = tsize;
			lmar += tsize;                 /* set the new indented left margin */
			linelen -= tsize;              /* shrink ll as to not shift the line any */
		}
		else
		{
			if( strncmp( buf, "right", len ) == 0 )  /* right just? */
			{
				flags2 |= F2_DIRIGHT;        /* turn on the flag */
			}
			else
			if( strchr( buf, '=' ) )		/* something = something */
			{
				switch( *buf )
				{
					case 'a':	
						if( isdigit( *(buf+2) ) )
						{
							dlstack[dlstackp].anum = DI_ANUMI;	/* integer numbering */
							dlstack[dlstackp].aidx = atoi( buf+2 );	/* where to start */
						}
						else
						{
							dlstack[dlstackp].anum = DI_ANUMA;	/* integer numbering */
							dlstack[dlstackp].aidx = *(buf+2);	/* where to start */
						}
						break;

					case 'c':		/* colour of items */
							free( dlstack[dlstackp].colour );
							dlstack[dlstackp].colour = strdup( buf+2 );
							break;

					case 'F':	if( difont ) 
								free( difont );			
							difont = strdup( buf+2 );
							break;

					case 'f':					/* format string */	
							dlstack[dlstackp].fmt = strdup( buf+2 );
							break;
				
					case 's':	
						dlstack[dlstackp].skip = atoi( buf + 2 );
						break;

					default:	break;
				}
			}
			else
			{
				if( difont != NULL )    /* free the font buffer if there */
					free( difont );
				difont = strdup( buf );
			}
		}
	}               /* end else */

	if( ! tsize )			/* no parms supplied, or bad parm */
	{
		tsize = 72;
		dlstack[dlstackp].indent = tsize;
		lmar += tsize;                 /* set the new indented left margin */
		linelen -= tsize;              /* shrink ll as to not shift the line any */
	}

	if( strncmp( "color", curfont, 5 ) == 0 )
		color = curfont;
	sprintf( obuf, " <table width=90%% border=0 > <font size=2 %s>", color );
 	AFIwrite( ofile, obuf );
	*obuf = 0;
}                                /* FMbd */
