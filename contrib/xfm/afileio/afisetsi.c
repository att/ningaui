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
*  Mnemonic: AFIsetsize
*  Abstract: This routine will set the buffer size of the file passed in
*            such that future reads will be at maximum the number inidcated
*            here.
*  Parms:    file - File handle of the open file
*            num  - User number to be used as max buffer size (should always
*                   be one more than the user wants characters back to allow
*                   for a terminating EOS character.
*  Returns:  The new maximum. If the number is out of range then we will
*            ignore it and just return the current max buffer size.
*  Date:     29 December 1993
*  Author:   E. Scott Daniels
*
*  Modified:  5 Jan 1994 - To set size on all chained files.
*******************************************************************************
*/
#include "afisetup.h"   /* get necessary include files */

int AFIsetsize( file, num )
 int file;
 int num;
{
 struct file_block *fptr;     /* pointer at file representation block */
 int max = 0;

 if( file >= 0 && file < MAX_FILES )
  {
   for( fptr = afi_files[file]; fptr != NULL; fptr = fptr->chain )
    if( num > 0 )
     fptr->max = num;
   max = afi_files[file]->max;
  }

 return( max );         /* return the current setting */
}                       /* AFIsetsize */
