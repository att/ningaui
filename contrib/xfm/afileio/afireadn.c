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
**** NOTE: This routine is obsolite as the AFIsetsize routine allows the
*          size to be set.
****************************************************************
*
*    Mnemonic:   aFIread
*    Abstract:   This routine reads characters from the file assoicated
*                with the file number passed in as does the AFIread rtn.
*                This routine allows the user to specify the maximum
*                number of bytes to be placed into the buffer if
*                a line feed character is not enountered.
*                This routine assumes that
*                linefeed (newline) characters in the file and/or carriage
*                return linefeed character combinations in the file
*                indicate the end of a record. In either case either the
*                maximum number of characters (as set in the file block)
*                or characters up to the end of the line are placed in the
*                buffer. The buffer is converted into a string by adding
*                and end of string (\000) character. The linefeed and/or
*                carriage return, linefeed characters are not returned as
*                a part of the string.
*                The file can be opened in eithr "text" or binary mode.
*    Parameters: file - AFI file number to read from
*                rec  - callers buffer to fill
*                max  - Maximum number to read
*    Returns:    The number of characters placed in the string, ERROR if
*                end of file encountered (no valid info in the buffer).
*    Date:       25 June 1985 - Converted to AFI on 12/10/92
*    Author:     E. Scott Daniels
*
*    Modified:   29 Jan 1990 - To return partial line if eof detected.
****************************************************************
*/
AFIreadn( file, rec, max )
 int file ;
 char *rec;
 int max;
{
 struct file_block *fptr;     /* pointer at file representation block */
 int i;                       /* for loop index */
 int lastlen;                 /* last length returned by fread */

 fptr = afi_files[file];        /* get the pointer to the file block */
 if( fptr == NULL)
  return( ERROR );

 for( i = 0; i < max &&
             (lastlen = fread( &rec[i], 1, 1, fptr->file )) > 0 &&
             rec[i] != 0x0A; i++ );    /* read up to end, max or next lf */


 if( i == 0 && lastlen == 0 )   /* then were at the end of the file */
  return( ERROR );

 if( rec[i] == 0x0a )   /* if line terminated by new line */
  {
   if( i > 0 && rec[i-1] == 0x0d )  /* 0x0d 0x0a combination in the record */
    i--;                   /* back up to write over it too */
   rec[i] = EOS;         /* return to user with end of string */
  }
 else
  rec[++i] = EOS;       /* if not terminated with new line add eos at end */

 return( i );    /* return number of characters in string */
}                                       /* AFIread */
