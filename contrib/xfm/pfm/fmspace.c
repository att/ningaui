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
*   Mnemonic:  FMspace
*   Abstract:  This routine processes the .SP command when entered.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      15 November 1988
*   Author:    E. Scott Daniels
*   Modified:   5 May 1992 - To support postscript conversion
*		12 Nov 2007 - Added half space.
*
***************************************************************************
*/
void FMspace( )
{
	char *buf;          /* pointer at the token */
	int lines;

	FMflush( );          /* flush out the current line */

	lines = FMgetparm( &buf );    	/* optional parm -- number of lines to skip */

	if( lines <= 0 )     		/* no parameter entered */
		lines = 1;          	/* defalut to one line */
	else
		lines = atoi( buf );   /* convert parameter entered */
	if( lines > 55 )
		lines = 55;           /* no more than 55 please */

	if( lines <= 0 )
                cury += (textsize+textspace)/2;   	/* half space */
	else
		cury += (textsize+textspace) * lines;   /* full space times number of lines */
}      
