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
/*
*******************************************************************************
*
*  Mnemonic: AFIwrite
*  Abstract: This routine will write out the buffer that has been passed in to
*            the file that is described by the AFI file number passed in.
*            If the buffer to be written ends with a linefeed (\n) or a crlf
*            pair then nothing is done to the buffer. If the buffer doesnt end
*            with a linefeed then a line feed is added and the fwrite routine
*            is allowed to covert that to a crlf if the file is opened in
*            "text" mode. For files opened in binary mode, the max buffer size
*            is written unless the length parameter is not 0, then it is used.
*  Parms:    file - The AFI file number (index into file list)
*            buf  - The string that is to be written to the file
*  Returns:  The value that is retunred by fwrite which is currently the
*            number of items written. If this is less than 1 then an error
*            has occurred.
*  Date:     10 December 1992
*  Author:   E. Scott Daniels
*
*  Modified:  2 Jan 1994 - To process a stdout file correctly
*            28 Jan 1994 - To flush after each write if NOBUF flag set
*            11 Mar 1997 - To handle writing to pipes.
*             3 Apr 1997 - To up the operations coutner.
*		10 Apr 2007 - Memory leak repair
********************************************************************************
*/
#include "afisetup.h"

int AFIwrite( int file, char *rec )
{
	struct file_block *fptr;     /* pointer at file representation block */
	int len;                     /* length of string to write */
	int status;                  /* status of the users write */

	fptr = afi_files[file];          /* get the pointer to the file block */
	if( fptr == NULL)
		return( AFI_ERROR );

	fptr->operations++;                /* up the operations counter */
	
	if( fptr->flags & F_BINARY )       /* ignore contents of buffer and write */
		len =  fptr->max;                 /* use parm unless it is 0; this is silly, but difficult to change now-- grumble */
	else
		len = strlen( rec );          /* get the record length to write */

	if( len <= 0 )
		return 0;

	if( fptr->flags & F_STDOUT )  /* writing to stdout??? */
	{
		printf( "%s", rec );     /* just print it */
		status = len;            /* so we can let 'em know it went well */
	}
	else
	{
		if( fptr->flags & F_PIPE )         /* handle a pipe a bit differently */
			status = write( fptr->pptr->pipefds[WRITE_PIPE], rec, len );
		else
		{
			status = fwrite( rec, len, 1, fptr->file ); /* write users record out */
			if( status == 1 )        /* if one block written */
				status = len;           /* then we can assume that length was written */
		}
	}

	if( rec[len-1] != 0x0a && (fptr->flags & F_AUTOCR) == 1 && (fptr->flags & F_BINARY ) == 0 )
	{                                 		/* generate line feed if necessary */
		if( fptr->flags & F_STDOUT )
			printf( "\n" );
		else
			fwrite( "\n", 1, 1, fptr->file );     /* write the newline for them */
	}

	if( fptr->flags & F_NOBUF )      /* no buffering on write */
		fflush( fptr->file );           /* then flush them */

	return( status );                /* send back number written */
}                                 /* AFIwrite */
