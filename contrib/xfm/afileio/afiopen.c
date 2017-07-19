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
*************************************************************************
*
*   Mnemonic: AFIopen
*   Abstract: This routine opens a file to be processed by the file io
*             package using the parameters passed in.
*   Parms:    name - Name of the file to open
*             opts - Options to use for the open (read, write etc)
*   Returns:  The number of the file that was opened or AFI_ERROR.
*   Date:     17 November 1988 - Converted to AFI 12/10/92
*   Author:   E. Scott Daniels
*
*   Modified: 16 Mar 1990 - To initialize chain pointer.
*             29 Dec 1993 - To set flags to autocr as default.
*              3 Jan 1994 - To allow a stdout file to be opened.
*              4 Jan 1994 - To correct if statement (>=) and to change last
*                           for loop to strncpy
*              8 Jan 1995 - To allow stdin/stdout names to be recognized
*                           rather than a non standard option string.
*             11 Mar 1997 - To support opening a pipe.
*              3 Apr 1997 - To init operations counter
**************************************************************************
*/
#include "afisetup.h"              /* get necessary include stuff */

struct file_block **afi_files = NULL;

int AFIopen( char *name, char *opts )
{
	int file;              /* file i/o package file number */
	struct file_block *new; /*pointer at the new block */
	int status;            /* processing  status */

	if( ! afi_files )
	{
		afi_files = (struct file_block **) malloc( sizeof( *afi_files ) * MAX_FILES );
		if( ! afi_files )
			return AFI_ERROR;
	
		memset( afi_files, 0, sizeof( *afi_files ) * MAX_FILES );
	}

	for( file = 0; file < MAX_FILES &&  afi_files[file] != NULL; file++ );
	if( file >= MAX_FILES )
		return( AFI_ERROR );        /* no open spaces */

	new = (struct file_block *) malloc( sizeof( struct file_block ) );
	if( new == NULL )
		return( AFI_ERROR );              /* could not allocate the block */

	/* initialize the new file block */
	new->file = NULL;
	new->operations = 0;           /* no I/O yet */
	new->max = 81;                 /* maximum number to read per buffer */
	new->flags = F_AUTOCR;         /* set the default flags for the file */
	new->tmb = NULL;               /* not tokenizing at this point */


	strncpy( new->name, name, MAX_NAME );  /* save the file name */
	new->chain = NULL;             /* initially nothing pointed to here */

	if( strchr( opts, 'b' ) != NULL )    	/* binary specified? */
		new->flags = F_BINARY;          /* then set binary - no auto lf */

	if( strncmp( name, "stdin", 5 ) == 0 || strncmp( name, "/dev/fd/0", 8) == 0 )  /* "open" standard input */
	{
		new->flags |= F_STDIN;
		new->file = stdin; 
	}
	else
		if( strncmp( name, "stdout", 6 ) == 0 || strncmp( name, "/dev/fd/1", 8 ) == 0 )  /* "open standard output */
		{
			new->flags |= F_STDOUT;
			new->file = stdout; 
		}
#ifdef KEEP_IF_UNIX
		else
			if( strncmp( name, "pipe", 4 ) == 0 )     /* create a pipe */
			{
				new->pptr = (struct pipe_blk *) malloc( sizeof( struct pipe_blk ) );
				if( new->pptr != NULL )                /* allocation ok */
				{
					if( pipe( new->pptr->pipefds ) )     /* returns "TRUE" if error */
					{
						free( new->pptr );                   /* trash the storage */
						new->pptr = NULL;                    /* force error later */
					}
					else                               /* pipe successfully created */
					{
						if( strchr( opts, 'b' ) )        /* binary specified in the opts? */
						{
							new->flags |= F_BINARY;        /* set binary flag  - no chache */
							new->pptr->csize = 0;          /* indicate no cache allocated */
							new->pptr->cache = NULL;
						}
						else
						{
							new->pptr->cache = (void *) malloc( sizeof(unsigned char) * MAX_CACHE );
							if( new->pptr->cache == NULL )           /* allocation error */
							{
								free( new->pptr );
								new->pptr = NULL;                      /* force later error */
							}
							else
							{
								new->pptr->csize = MAX_CACHE;           /* indicate cache size */
								new->pptr->cptr = new->pptr->cache;     /* init pointers */
								new->pptr->cend = new->pptr->cache;     /* force real read */
							}
						}                                  /* end else not binary */
					}                       /* end pipe successfully created else */
				}                         /* end if new pipe block allocated ok */

				if( new->pptr == NULL )    /* error @ some point during pipe/allocation */
				{
					free( new );
					return( AFI_ERROR );     /* let caller know there were problems */
				}
	
				new->flags |= F_PIPE;            /* indicate pipe for read/write calls */
			}
#endif
			else                               /* assume regular file to be opened */
			{
				new->file = fopen( name, opts );
				if( new->file == NULL )
				{
					free( new );
					return( AFI_ERROR );
			}
		}    /* end else - must just have to open regular file */

	afi_files[file] = new;             /* point at the file block */

	return( file );
}      
