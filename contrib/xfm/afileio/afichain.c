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
****************************************************************************
*
*    Mnemonic: AFIchain
*    Abstract: This routine chains a newly opened file with an existing
*              file (that is also open). This is ideal for file merging.
*    Parms:    file - File number of open file to chain to.
*              fname - Name of the file to open.
*              opts - options for open. If first character is p assume next
*                     parameter is present and dont pass p to AFIopen.
*              path - Path string to pass to AFIopenp if p is in options.
*    Returns:  AFI_ERROR if unable to chain the file, AFI_OK if successful.
*    Date:     12 Feb 1989 - Converted to be part of the AFI routines 12/10/92
*    Author:   E. Scott Daniels
*
*    Modified: 4 Jan 1994 - To correct call to open
*              8 Jan 1995 - To call AFIopenp if p is in options string.
*             28 Mar 1997 - To support tokenized input streams
****************************************************************************
*/
#include "afisetup.h"        /* include all needed header files */

int AFIchain( int file, char *fname, char *opts, char *path )
{
 struct file_block *ptr;        /* pointer at the existing file */
 struct file_block *tptr;       /* pointer at chained file block */
 int tfile;                     /* temp file number returned by open */

 if( file < 0  ||  file > MAX_FILES )
  return( AFI_ERROR );

 if( (ptr = afi_files[file]) == NULL )
  return( AFI_ERROR );

 if( *opts == 'p' )                /* assume path parm passed in */
  {
   opts++;                 /* dont pass p option to the open routine */
   tfile = AFIopenp( fname, opts, path );  /* open using path string */
  }
 else
  tfile = AFIopen( fname, opts );      /* open the requested file */

 if( tfile < AFI_OK )
  return( AFI_ERROR );

 tptr = afi_files[tfile];          /* get pointer to file block for new file */
 tptr->chain = ptr;            /* put infront of existing file block */
 tptr->flags = ptr->flags;     /* make duplicate of previously opened file */
 tptr->flags &= ~F_STDIN;      /* cannot reopen the standard input file */
 tptr->max = ptr->max;
 strncpy( tptr->name, fname, MAX_NAME );  /* copy the file name in */
 tptr->name[MAX_NAME] = EOS;                 /* just in case long name */

 if( ptr->tmb != NULL )         /* file was setup for tokenized input */
  {                             /* so do the same and copy existing environ */
   AFIsettoken( tfile, ptr->symtab, ptr->tmb->tsep, ptr->tmb->varsym, ptr->tmb->escsym, 
                ptr->tmb->psep );
  }

 afi_files[file] = afi_files[tfile];    /* put new block at head of open list */
 afi_files[tfile] = NULL;           /* no longer need this slot in file list */

 return( AFI_OK );              /* ok - lets go */
}                               /* AFIchain */
