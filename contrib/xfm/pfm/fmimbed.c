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
*  Mnemonic: FMimbed
*  Abstract: This routine is responsible for parsing the imbed string and
*            calling FMopen to open a file to process.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     15 November 1988
*  Author:   E. Scott Daniels
*  Modified: 	25 Aug 2000 - to add NF option
*		13 Nov 207 - Added run/stop command to stream to mark pop of 
*			the fmrun() command and return to this function. Allows
*			this rouitine to drive the imbed which is needed
*			for things like oneject that imbed files and push/pop the 
*			environment before/after the file. Basically negates the 
*			AFIchain() feature where the imbed file was pushed onto 
*			the stack of open files. c'est la vie! 
*
* .im [nf] filename
***************************************************************************
*/
void FMimbed( )
{
	char *fp = 0;
	char *buf;      /* pointer into the imput buffer of the fname token */
	int len;        /* length of the token */

	len = FMgetparm( &buf );
	if( strcmp( buf, "nf" ) == 0 )
	{
		len = FMgetparm( &buf );       /* point to the next token in the buffer */
		FMflush( );                    /* send last formatted line on its way */
		flags = flags | NOFORMAT;      /* turn no format flag on */
	}

	if( len <= 0 )
	{
		FMmsg( E_MISSINGNAME, ".IM" );
		return;
	}

	FMmsg( I_IMBED, buf );

	fp = strdup( buf );
	AFIpushtoken( fptr->file, ".sr" );  	/* push the special runstop command to mark end of imbed file */
	TRACE( 2 ) "imbed: starting with file %s\n", fp );
	FMopen( buf );             		/* open the imbed file */

	FMrun( );				/* run until we hit the end of the file */

	TRACE( 2 ) "imbed: finished with file %s lmar=%d\n", fp, lmar );
	free( fp );
}                                 /* FMimbed */
