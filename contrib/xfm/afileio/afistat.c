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
*  Mnemonic: AFIstat
*  Abstract: This routine will return a particular piece of information
*            about the file that is represented by the handle passed in.
*  Parms:    file - File number - index into the files array
*            opt  - Opion that describes the information that is to be
*                   returned.
*            data - Pointer to pointer if non integer data to be returned.
*  Returns:  An integer that is: TRUE if the file is chained, or the
*            number of files in the chain, or the pointer to the name
*            of the file, or the number of files opened (chained files
*            count as 1 open file, or the flags associated with the file.
*  Date:     9 February 1994
*  Author:   E. Scott Daniels
*
*  Modified:  3 Apr 1997 - To support operations counter
*****************************************************************************
*/
#include "afisetup.h"  /* get necessary header files */

long AFIstat( file, opt, data )
 int file;
 int opt;
 void **data;
{
 long i;                   /* loop index - return value */
 long j;                   /* loop index */

 struct file_block *fptr;   /* pointer to the file info block */

 if( opt != AFI_OPENED )   /* we need to select the block if not num opened */
  {
   fptr = afi_files[file];        /* get the pointer to the file block */
   if( fptr == NULL )         /* if no pointer then exit with error */
    return( AFI_ERROR );
  }

 switch( opt )            /* do what user has asked for */
  {
   case AFI_CHAINED:                  /* return true if file chained */
    i = fptr->chain != NULL ? TRUE : FALSE;
    break;

   case AFI_CHAINNUM:       /* return number on the chain */
    for( i = 0; fptr->chain != NULL; fptr = fptr->chain, i++ );
    break;

   case AFI_NAME:
    *data = (void *) fptr->name;    /* return pointer to name string */
    i = AFI_OK;                     /* set to ok just in case */
    break;

   case AFI_OPENED:                       /* return number of open files */
    for( i = 0, j = 0; j < MAX_FILES; j++ )
     if( afi_files[j] )
      i++;           /* count open file */
    break;

   case AFI_FLAGS:
    i = fptr->flags;
    break;
 
   case AFI_OPS:                /* return operations done on this file */
    i = fptr->operations;
    break;

   default:
    i = AFI_ERROR;              /* indicate error */
  }                             /* end switch */

 return( i );                   /* return what ever we calculated */
}             /* AFIstat */

