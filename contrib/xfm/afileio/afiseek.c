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
*  Mnemonic: AFIseek
*  Abstract: This routine will position the next read in the buffer such that
*            the data at the offset passed in is read. Prior to the seek flush
*            is called to ensure that any buffered data is written at the
*            proper position.
*  Parms:    file - The AFI file number (index into file list)
*            offset - Position to seek to (in bytes) relative to location
*            loc -  Location in the file (top bot or current) to base offset
*                   on.
*  Returns:  OK if successful, ERROR if not
*  Date:     28 January 1994
*  Author:   E. Scott Daniels
********************************************************************************
*/
#include "afisetup.h"  /* get necessary header files */

int AFIseek( file, offset, loc )
 int file ;
 long offset;
 int loc;
{
 struct file_block *fptr;     /* pointer at file representation block */

 fptr = afi_files[file];        /* get the pointer to the file block */
 if( fptr == NULL)
  return( AFI_ERROR );

 fflush( fptr->file );               /* flush before position cmd */

 fseek( fptr->file, offset, loc );

 fptr->flags &= ~F_EOF;               /* turn off the end of file */
 return( AFI_OK );                    /* send back number written */
}                                     /* AFIseek */
