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
******************************************************************************
*
*    Mnemonic:   AFIread
*    Abstract:   This routine reads characters from the file assoicated
*                with the file number passed in and places them into the
*                buffer that is passed in. This routine assumes that
*                linefeed (newline) characters in the file and/or carriage
*                return linefeed character combinations in the file
*                indicate the end of a record if the file is not opened
*                with the binary flag in the options string or the binary
*                flag is not set with a call to AFIsetflag.
*                When in regular mode (not binary) the characters up to the
*                new line (or CR LF) are returned as a NULL terminated string
*                in the supplied buffer. If maxbuffer is reached prior to a
*                new line character then the entire buffer will contain
*                valid data and will be terminated by a NULL.
*                When in binary mode an entire buffer is returned to the
*                caller with a partial buffer returned only when the end of
*                file has been reached. NO substitutition for any characters
*                is done when in binary mode.
*    Parameters: file - AFI file number to read from
*                rec  - callers buffer to fill
*    Returns:    The number of characters placed in the string, ERROR if
*                end of file encountered (no valid info in the buffer).
*    Date:       25 June 1985 - Converted to AFI on 12/10/92
*    Author:     E. Scott Daniels
*
*    Modified:   29 Jan 1990 - To return partial line if eof detected.
*                29 Dec 1993 - To check for binary flag and handle right.
*                 2 Jan 1994 - To handle chained files correctly.
*                30 Jan 1994 - To support reading from stdin
*                11 Mar 1997 - To support pipes
*                 3 Apr 1997 - To count i/o operations
*		10 Apr 2007  - Fixed check for hard notification at eof of 
			file in the chain.
****************************************************************************
*/
#include "afisetup.h"   /* get necessary include files */

int AFIread( file, rec )
 int file ;
 char *rec;
{
 struct file_block *fptr;     /* pointer at file representation block */
 int i;                       /* for loop index */
 int lastlen;                 /* last length returned by fread */

 fptr = afi_files[file];        /* get the pointer to the file block */
 if( fptr == NULL || (fptr->flags & F_EOF) )  /* if no pointer or at end */
  return( AFI_ERROR );

 fptr->operations++;                     /* up the # of i/o done */

 if( fptr->flags & F_PIPE )              /* file is really a pipe */
  return( AFIreadp( file, rec ) );       /* let read pipe routine handle */

 if( fptr->flags & F_BINARY )       /* if in binary mode read to max buffer */
  {
   for( i = 0; i < fptr->max &&
             (lastlen = fread( &rec[i], 1, 1, fptr->file )) > 0; i++ );
  }
 else                               /* assume regular ascii read */
  {
   for( i = 0; i < fptr->max && (lastlen = fread( &rec[i], 1, 1, fptr->file )) > 0 && rec[i] != 0x0A; i++ );    /* read up to end, max or next lf */
  }


 if( i == 0 && lastlen == 0 )       /* then were at the end of the file */
  if( fptr->chain == NULL )         /* if no more */
   {
    fptr->flags |= F_EOF;
    return( AFI_ERROR );             /* then return error indication */
   }
  else
   {
	int state = fptr->flags & AFI_F_EOFSIG;	/* hard notification on close of file */

    	AFIclose1( file );   			/* close the one weve been reading */
	if( state )
		return -1;
	else
	{
    		if( fptr->tmb )			/* if tokenising, just return 0 as there may be unparsed tokens from the previous file  */
			return -1;
		else
    			return( AFIread( file, rec ) );  /* and try to read again */
	}
   }

 if( (fptr->flags & F_BINARY) == 0 )   /* if not binary */
  {
   if( rec[i] == 0x0a )                /* if line terminated by new line */
    {
     if( i > 0 && rec[i-1] == 0x0d )  /* 0x0d 0x0a combination in the record */
      i--;                            /* back up to write over CR as well */
     rec[i] = EOS;                    /* return to user with end of string */
    }
   else
    rec[i++] = EOS;       /* if not terminated with new line add eos at end */
  }

 return( i );                       /* return number of characters in string */
}                                       /* AFIread */
