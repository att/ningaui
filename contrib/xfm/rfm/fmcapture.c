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
***************************************************************************
*
*  Mnemonic: 	FMcapture
*  Abstract: 	capture all lines until .ca end command is noticed
*		lines are written to the file named on the command 
*  Parms:   
*  Returns:  	Nothing.
*  Date:     	06 November 2007
*  Author:   	E. Scott Daniels
*  Modified: 
*
*  Command syntax:  .ca {start filename|end}
*
*****************************************************************************
*/

void FMcapture( )
{
	FILE 	*f = NULL;
	char 	*fname;
	char 	*buf;
	char	*cp;
	int	i = 0;

   	if( FMgetparm( &buf) != 0  )
	{
		if( strcmp( buf, "end" ) == 0 )			/* should not happen */
			return;

		if( strcmp( buf, "start" ) == 0 )
			FMgetparm( &buf );

		fname = strdup( buf );	
		if( (f = fopen( fname, "w" )) == NULL )
			FMmsg( E_CANTOPEN, fname );			/* we skip what was to be captured, so continue */

		while( FMread( inbuf ) >= 0 )
		{
			if( inbuf[0] == CMDSYM )			/* must find .ca end in first spot */
			{
				if( inbuf[1] == 'c' && inbuf[2] == 'a' )  
				{
					for( cp = &inbuf[3]; *cp && (*cp == ' ' || *cp == '\t'); cp++ );
					if( *cp && strncmp( cp, "end", 3 ) == 0 )
					{
						iptr = (cp+3) - inbuf;		/* should not be anything, but leave pointed past end */
						if( f )
							fclose( f );
						TRACE( 1 ) "capture: %d lines captured in %s\n", i, fname );
						free( fname );
						return;                                     /* get out now */
					}
				}
    			} 

			if( f )
				fprintf( f, "%s\n", inbuf );

			i++;
		}
	}
}
