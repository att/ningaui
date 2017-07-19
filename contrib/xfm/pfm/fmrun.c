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
* Mnemonic:	fmrun
* Abstract:	Drives input until the end of file is reached.
* 		Added to support imbed as an oneject command.
*		Allows an imbed command to be processed before
*		the rest of the tokens on the command line.
* Date: 	18 Nov 2001
* Author: 	E. Scott Daniels
*		10 Apr 2007 - Memory leak cleanup 
*
* -------------------------------------------------------------
*/

void FMrun( )
{
	char	*buf = NULL;
	int	len = 1;
	int	i = 0;
	char	b[2048];

	TRACE( 1 )  "run: starts\n" );
	while( len >= 0 ) 
	{
		if( flags & NOFORMAT )
			FMnofmt( );
		else
			if( flags2 & F2_ASIS )
				FMasis( );

		if( (len = FMgettok( &buf )) > 0 )
		{
			TRACE( 3 )  "run: (%s) lmar=%d\n", buf, lmar );

			if( len == 3 && *buf == CMDSYM && *(buf+3) == 0 )
			{
				switch( FMcmd( buf ) )			
				{
					case 0: 	FMaddtok( buf, len ); 			/* not a command -- add to output */
							break;

					case 1: 	break;					/* normal command handled -- continue */

					default: 	TRACE( 2 ) "run: pop run stack\n" );
							return;
							break;
				}
			}
			else
				FMaddtok( buf, len );
		}
	}
	TRACE( 1 )  "run: stops  len=%d\n", len );
}
