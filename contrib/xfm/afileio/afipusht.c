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
*****************************************************************************
*
*  Mnemonic: AFIpushtoken
*  Abstract: This routine will push a buffer onto the token management
*            input list. The caller can push an empty buffer onto the
*            stack if buf is NULL.
*  Parms:    file - The afi file handle of the open file
*            buf  - Pointer to the EOS terminated buffer to push
*  Returns:  AFI_OK if all is well, AFI_ERROR if bad file or other error.
*  Date:     28 March 1997
*  Author:   E. Scott Daniels
*
******************************************************************************
*/
#include "afisetup.h"
#include	"parse.h"

int AFIpushtoken( AFIHANDLE file, char *buf )
{
	int status = AFI_ERROR;       /* status of local processing */
	struct file_block *fptr;      /* pointer at the file structure */
	struct inputmgt_blk *imb;     /* pointer at the new input management block */

	if( (fptr = afi_files[file]) != NULL &&   /* valid file? */
		fptr->tmb != NULL              )  /* and setup for tokenized input */
	{
		fptr->flags &= ~F_EOF;              /* technically not eof yet */

		if( (imb = (struct inputmgt_blk *) AFInew( INPUTMGT_BLK )) != NULL )
		{
			if( buf != NULL )         /* user can cause an empty blk to be added */
			{                        /* mostly for the afi internal rtns */
				imb->buf = strdup( buf );
				imb->idx = imb->buf;
				imb->end = imb->buf + strlen( buf );   /* set up indexes into buffer */
				imb->next = fptr->tmb->ilist;          /* push it on the stack */
				fptr->tmb->ilist = imb;
			}

			status = AFI_OK;                  /* set good return code and scram */
		}                            /* end if imb allocated ok */
	}                              /* end if file ok and setup for tokens */

	return( status );
}                                        /* AFIpushtoken */
