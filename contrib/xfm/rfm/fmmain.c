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
#define OWNS_GLOBALS 

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
*   Mnemonic:  FMmain
*   Abstract:  This routine is the driver for the FM formatter
*   Parms:     argc - Number of parameters passed to the routine
*              argv - Argument vectors
*                  [1] - input file name
*                  [2] - Output file name
*   Returns:   Nothing.
*   Date:      15 November 1988
*   Author:    E. Scott Daniels
*   Modified:   5 May 1992 - To support conversion to postscript
*              29 Oct 1992 - To call command only if .xx is followed by a
*                            blank so that token starting with a . can be
*                            included in the text (as long as they are more
*                            than 2 characters long.)
*              27 Nov 1992 - To support as is postscript commands
*              10 Dec 1992 - To use AFI routines for ansi compatability
*              15 Dec 1992 - To output a newline to stdout at end of run
*              21 Dec 1992 - To process the .toc file generated
*              13 Apr 1992 - To no longer break when blank as first char
*		18 Nov 2001 - To rewrite to allow for immediate ex of .im 
*		13 Nov 2007 - Cleanup a bit.
**************************************************************************
*/
main( argc, argv )
 int argc;
 char *argv[];
{
	int len;          /* length of token */
	char *buf;        /* buffer pointer to token */

	if( FMinit( argc, argv ) < VALID )
		return 1;

	FMrun( );			/* run the open file */

	if( tocfile >= OK )		/* clean up table of contents file */
	{				
		len = sprintf( inbuf, ".et\n" );
		AFIwrite( tocfile, inbuf );
		AFIclose( tocfile );	/* close the toc file */
	}

	FMflush( );             /* flush the current line to page buffer */
	FMpflush( );            /* flush the page out of the printer with headers
	AFIclose( ofile );      /* close the output file */
	sprintf( obuf, "%ld\n", words );

	FMmsg( I_WORDCNT, obuf );     /* write number of words message */
	return 0;                     /* assume end of file reached */
}                              /* FMmain */
