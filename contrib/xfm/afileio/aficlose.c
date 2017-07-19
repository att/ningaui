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
***************************************************************************
*
*    Mnemonic: AFIclose
*    Abstract: This routine is responsible for closing all files associated
*              with the file number passed in.
*    Parms:    file - Number of the file that is to be closed.
*    Returns:  Always returns ok.
*    Date:     17 November 1988 - Converted to AFI 12/10/92
*    Author:   E. Scott Daniels
*
***************************************************************************
*/
#include "afisetup.h"   /* get necessary header files */

int AFIclose( file )
 int file;
{
 struct file_block *ptr;      /* pointer at the block representing the file */

 while( afi_files[file] != NULL )       /* until no file block exists */
  AFIclose1( file );                 /* close next block on the chain */

 return( AFI_OK );
}                    /* AFIclose */
