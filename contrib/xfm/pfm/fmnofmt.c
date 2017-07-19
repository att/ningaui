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
*   Mnemonic: FMnofmt
*   Abstract: This routine is called when the noformat flag is set. It
*             reads in lines from the input file and if there is not a
*             command in the first column then it places the line of
*             text as is out to the page buffer. If a command is on the
*             input line control is returned to the caller to process the
*             command.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     8 December 1988
*   Author:   E. Scott Daniels
*
*   Modified: 29 Oct 1992 - To convert for postscript generation
*              9 Mar 1993 - To escape ( ) and \ characters and to insert
*                           blank lines where found in the input text.
*              6 Apr 1994 - To remove dependancy on linelength.
*		10 Apr 2007 - Fixed buffer underflow
*******************************************************************************
*/
void FMnofmt( )
{
	int status;            /* status of the read */
	int i;                 /* loop index */

	status = FMread( inbuf );        /* get the next buffer */

	while( status >= 0  &&  inbuf[0] != CMDSYM && *inbuf != vardelim ) 
	{
		for( i = 0; i < MAX_READ-1 && inbuf[i] != EOS; i++, optr++ )
		{
			if( inbuf[i] == '(' || inbuf[i] == ')' || inbuf[i] == '\\' )
			{
				obuf[optr] = '\\';     /* escape the character first */
				optr++;
			}
			obuf[optr] = inbuf[i];         /* copy the buffer as is */
		}
	
		obuf[optr++] = ' ';    /* flush assumes addtok adds a trail blank and truncates */
		obuf[optr] = EOS;      /* terminate buffer for flush */
		
		FMflush( );            /* send the line on its way */

		status = FMread( inbuf );   /* get the next line and loop back */
	}           /* end while */

	inbuf[status > 0 ? status : 0] = 0;
	TRACE( 1 ) "nofmt -- pushing command/var: %s\n", inbuf );
	AFIpushtoken( fptr->file, inbuf );   				/* put command back into input stack */
	iptr = optr = 0;              					/* return pointing at beginning */
}      
