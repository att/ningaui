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
*   Mnemonic: FMgettok
*   Abstract: This routine gets the next token from the input file buffer
*             and returns its length.
*   Parms:    buf - pointer to a buffer pointer that is to point at the token
*                   upon the return from this routine
*   Returns:  The length of the token or 0 if at end of file.
*   Date:     16 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 7-07-89 - To support variable expansion.
*             9-01-89 - To support user defined variable delimiter
*             4-13-93 - To allow a quoted string as one token
*             4-03-97 - To use AFI tokenizing support!
*		22 Oct 2007 - Fixes to backquoting token.
****************************************************************************
*/
int FMgettok( char **buf )
{
	int 	len = ERROR;     /* length of buffer read from input file */
	int 	i;               /* loop index */
	int 	j;
	char 	*cp;
	char	exbuf[2048];	/* extra work buf */


	*buf = inbuf;        /* new token is placed at start of the input buffer */
	**buf = 0;           /* ensure eos at start */

	while( (len = AFIgettoken( fptr->file, inbuf )) == 0 );  /* gettok or eof */

	if( *inbuf == '`' )
	{
     		AFIsetflag( fptr->file, AFI_F_WHITE, AFI_SET ); 

		i = 0;
		cp = inbuf + 1;			/* skip lead bquote */
		do
		{
			if( i + len >= sizeof( exbuf ) )
			{
				exbuf[50] = 0;
				fprintf( stderr, "buffer overrun detected in gettoken: %s...\n", exbuf );
				exit( 1 );
			}

			for( ; *cp && *cp != '`'; cp++ )
			{
				if( *cp == '^'  &&  *(cp+1) == '`' )
					cp++;					/* skip escape and put in bquote as is */
				exbuf[i++] = *cp;	
			}

			exbuf[i] = 0;

			if( *cp == '`' )
				break;

			cp  = inbuf;						/* next token will be here */
		}
		while( (len = AFIgettoken( fptr->file, inbuf)) >  0 );


		strcpy( inbuf, exbuf );
		*buf = inbuf;
		len = strlen( inbuf );
		TRACE(2) "getparm returning backquoted parameter: len=%d (%s)\n", len, inbuf );
	
       		AFIsetflag( fptr->file, AFI_F_WHITE, AFI_RESET ); 
	}


	TRACE(4) "fmgettok: returning(%s)\n", inbuf );
	 return( len );
}      
