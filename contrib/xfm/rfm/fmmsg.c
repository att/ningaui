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
*   Mnemonic: FMmsg
*   Abstract: This routine writes a message to the user console.
*             If the message number is negative then no message is
*             displayed. When the first character of a message is a +
*             or a * then the message is displayed regardless. If the
*             first character of the message is blank ( ) then the
*             message is displayed only if in verbose (noisy) mode.
*             The * character in the first column of a message causes
*             the current input file and line number to be appended to
*             the front of the message.
*   Parms:    msg - Message number of mesage to display.
*             buf - Non standard message to display with standard message
*   Returns:  Nothing.
*   Date:     16 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 	11 Apr 1994 - To allow for "silent" operation
*              	03 Apr 1997 - To get file info from AFI
*		15 Dec 2005 - To write messages to stderr rather than stdout
***************************************************************************
*/

  /* messages: begin with '*' if message is prefixed with file/line info */
  /*           begin with '+' if displayed even if in quiet mode         */
  /*           begin with ' ' if displayed only when in noisy mode       */

 char *messages[ ] = {
 " FM000: Copyright (c) 1992,1994,1997,2001,2003,2007 E. Scott Daniels. All Rights reserved",
 " FM001: End of file reached for file: ",
 "*FM002: Too many files open. Unable to include: ",
 "*FM003: Unable to open file. File name: ",
 "*FM004: Name on command line is missing. ",
 " FM005: Imbedding file: ",
 "*FM006: Unable to allocate required storage. ",
 " FM007: Lines included from the file = ",
 "+FM008: Required parameter(s) have been omitted from command line.",
 "*FM009: Unknown command encountered and was ignored.",
 "*FM010: Attempt to delete eject failed: not found.",
 "+FM011: Number of words placed into the document = ",
 "*FM012: Parameter takes margin out of range. ",
 "*FM013: Term length too large. Setting to 1/2 linelength.",
 "*FM014: Left margin set less than term size, term ignored.",
 "*FM015: Parameter takes line length past the current column width.",
 "+FM016: Unexpected end of input file.",
 "*FM017: Parameter missing or out of range",
 "*FM018: %%boundingbox statement missing or invalid in file: ",
 "*FM019: Definition list not started. .di command ignored.",
 "*FM020: Unrecognized parameter: ",
  "  "  };


void FMmsg( int msg, char *buf )
{
	char	wbuf[1024];
	char *ptr;              /*  pointer at the message */
	char *name = NULL;             /* pointer to file name */
	long line = 0;          /* line number in current file */

	if( msg >= VALID )      /* if user wants a standard message */
	{
		if( fptr != NULL && fptr->file >= 0 )
		{
			AFIstat( fptr->file, AFI_NAME, (void **) &name );  	/* get the file name */
			line = AFIstat( fptr->file, AFI_OPS, NULL ); 		/* get the line number */

			if( !name )
				name = "stdin";
		}
		else
			name = "unknown-file";

		ptr = messages[msg];
		if( ptr[0] != ' ' || (flags2 & F2_NOISY) )
		{
			ptr = &ptr[1];              /* skip the lead character */
			if( ptr[-1] == '*' && fptr != NULL )  /* print line number too */
				snprintf( wbuf, sizeof( wbuf ), "(%s:%ld) %s", name, line, ptr );
			else
				snprintf( wbuf, sizeof( wbuf ), "%s: %s", name,  ptr );
			if( buf != NULL )
			{
				/*strcat( wbuf, ": " );*/
				strncat( wbuf, buf, (sizeof( wbuf ) - strlen(wbuf)) -3 );
			}
			fprintf( stderr, "%s\n", wbuf );
		}
	}
	else
		if( buf != NULL )
			fprintf( stderr, "    %s\n", buf );       /* display only the user supplied buffer */
}                                     /* FMmsg */
