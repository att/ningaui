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
*    Mnemonic: AFIclose1
*    Abstract: This routine is responsible for closing the first file
*              in the chain associated with the file number passed in.
*              If the "file" is a pipe both read and write ends of the
*              pipe are closed and the associated pipe block destroyed.
*    Parms:    file - Number of the file that is to be closed.
*    Returns:  VALID.
*    Date:     15 Feb 1989 - Converted to AFI 12/10/92
*    Author:   E. Scott Daniels
*
*    Modified: 11 Mar 1997 - To handle pipes
*              28 Mar 1997 - To handle tokenized input mgt blocks
*		10 Apr 2007 - Now free tsep.
*
***************************************************************************
*/
#include "afisetup.h"     /* get necessary headers */

int AFIclose1( int file )
{
	struct file_block *ptr;      /* pointer at the block representing the file */
	struct inputmgt_blk *imb;    /* pointer at the input mgt blk to free */

	ptr = afi_files[file];           /* get pointer to the file block */
	
	if( ptr == NULL )            /* bad file number passed in ? */
		return( AFI_ERROR );

	afi_files[file] = ptr->chain;     /* point at remaining if they are there */
	
	if( ptr->tmb != NULL )        /* token management block there? */
	{
		while( ptr->tmb->ilist != NULL )  /* while input mgt blocks on the list */
		{
			imb = ptr->tmb->ilist;          /* hold one to free */
			ptr->tmb->ilist = imb->next;
			free( imb->buf );
			free( imb );                     /* trash the storage */
		}

		if( ptr->tmb->psep )
			free( ptr->tmb->psep );          /* free the parameter seperator list */
		if( ptr->tmb->tsep )
			free( ptr->tmb->tsep );          /* free the parameter seperator list */

		free( ptr->tmb );                /* trash the token mgt blk */
	}                                  /* end if token management on the file */

	if( ptr->flags & F_PIPE && ptr->pptr != NULL )     /* valid  pipe ? */
	{
		close( ptr->pptr->pipefds[READ_PIPE] );      /* close both descriptors */
		close( ptr->pptr->pipefds[WRITE_PIPE] );

		if( ptr->pptr->cache != NULL );            /* some pipes dont cache */
			free( ptr->pptr->cache);                  /* trash the cache */
		free( ptr->pptr );                         /* trash the pipe block */
	}
	else
		if( ptr->file != NULL )                     /* valid file pointer */

	fclose( ptr->file );                       /* do the close */

	free( ptr );                                 /* free the buffer */

	return( AFI_OK );
}                                             /* AFIclose1*/
