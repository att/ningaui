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
***** NOTE: This routine is obsolite as the AFIsetsize routine allows
*           the size to be set.
****************************************************************
*
*    Mnemonic:   aFIread
*    Abstract:   This routine reads characters from the file assoicated
*                with the file number passed in as does the AFIread rtn.
*                This routine allows the user to specify the number of
*                bytes to be placed into the buffer. This read routine is
*                unlike AFIread and AFIreadn insomuch as it will not
*                terminate the read when a newline (0x0a) is encountered.
*                The buffer is NOT converted into a string by adding
*                an end of string (\000) character.
*                The file should be opened in binary mode.
*    Parameters: file - AFI file number to read from
*                rec  - callers buffer to fill
*                n    - Number of bytes to read.
*    Returns:    The number of characters placed in the string, ERROR if
*                end of file encountered (no valid info in the buffer).
*    Date:       12 January 1993
*    Author:     E. Scott Daniels
*
****************************************************************
*/
AFIreadb( file, rec, n )
 int file ;
 char *rec;
 int n;
{
 struct file_block *fptr;     /* pointer at file representation block */
 int lastlen;                 /* last length returned by fread */

 fptr = afi_files[file];        /* get the pointer to the file block */
 if( fptr == NULL)
  return( ERROR );

 lastlen = fread( &rec[i], 1, n, fptr->file );   /* read bytes */

 if( lastlen == 0 )   /* then were at the end of the file */
  return( ERROR );

 return( n );         /* return number of characters actually read */
}                                       /* AFIread */
