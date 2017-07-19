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
*  Mnemonic: AFIsetflag
*  Abstract: This routine allows the user to set the flag values in the
*            file block.
*            here.
*  Parms:    file - File handle of the open file
*            flag - Bitmask flags AFI_F contained in AFI defs file.
*            opt  - If true (AFISET) then flags are ORd. If false (AFIRESET)
*                   then ~flag is ANDed with the current value.
*  Returns:  Nothing.
*  Date:     29 December 1993
*  Author:   E. Scott Daniels
*
*  Modified:  5 Jan 1994 - To set flag on all chained files too.
*******************************************************************************
*/
#include "afisetup.h"   /* get necessary headers */

void AFIsetflag( file, flag, opt )
 int file;
 int flag;
 int opt;
{
 struct file_block *fptr;     /* pointer at file representation block */

 if( file >= 0 && file <= MAX_FILES )
  for( fptr = afi_files[file]; fptr != NULL; fptr = fptr->chain )
   {
    if( opt )                 /* set the flags */
     fptr->flags |= flag;
    else
     fptr->flags &= ~flag;    /* reset the flag */
   }
}                             /* AFIsetsize */
