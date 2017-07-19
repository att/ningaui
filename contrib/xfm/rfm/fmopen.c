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

#include <stdio.h>     
#include <stdlib.h>
#include <fcntl.h>    
#include <ctype.h>   
#include <string.h> 
#include <memory.h>
#include <time.h>

#include "../lib/symtab.h"		/* our utilities/tools */
#include "../afileio/afidefs.h"   


#include "fmconst.h"               /* constant definitons */
#include "rfm_const.h"


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
****************************************************************************
*
*    Mnemonic: FMopen
*    Abstract: This routine processes an imbed statement and opens a new
*              file to process. It is also used by index generation and maybe
*		other functions. A file is opened and its info is stacked. 
*		the original purpose of this routine was to stack files and 
*		not to present and end of file until the last file on the stack
*		reached the end of file. The AFI routines now provide a generic
*		stack interface and now this function's responsibilities are 
*		significantly reduced.
*		file names for error messages. 
*    Parms:    name - Pointer to the name of the file to open
*    Returns:  ERROR if unable to open the file, otherwise VALID
*    Date:     15 November 1988
*    Author:   E. Scott Daniels
*
*    Modified: 16 Mar 1990 - Not to use FI routines.. there is a bug in
*                            them someplace.
*              10 Dec 1992 - To use AFI routines (which use ansi std fread)
*               3 Apr 1997 - To begin phase out of this routine.
*	       19 Oct 2001 - Cleanup - finish work started on in 97
*	        5 Feb 2001 - To save name 
*		21 Oct 2007 - Some general doc cleanup.
**************************************************************************
*/
int FMopen( char *name )
{
	int i;                   /* loop index */

	if( fptr == NULL )            /* if first file opened */
	{
		fptr = (struct fblk *) malloc( sizeof( struct fblk ) );
		if( fptr == NULL )
		{
			FMmsg( E_NOMEM, "FMopen" );
			return( ERROR );
		}

		memset( fptr, 0, sizeof( struct fblk ) );
		fptr->fnum = 0;
		if( (fptr->file = AFIopenp( name, "r", path )) == AFI_ERROR )
		{
			FMmsg( E_CANTOPEN, name );     
			free( fptr );	
			fptr = NULL;
			return( ERROR );
		}
		
		AFIsetsize( fptr->file, MAX_READ-1 );       /* input buffer size */
		strcpy( fptr->name, name );
	}
	else
	{
		if( AFIchain( fptr->file, name, "pr", path ) == AFI_ERROR )
		{
			FMmsg( E_CANTOPEN, name );     
			return( ERROR );
		}
		fptr->fnum = 0;
	}


	fptr->count = 0l;                  /* set record counter */

	return OK; 		/* beam us up scotty */
}			                  /* FMopen */
