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
*   Mnemonic:  FMtc
*   Abstract:  This routine parses the .tc command and opens the toc file
*              if it is not open already. As PSFM processes .h_ commands that
*              cause TOC entries to be generated have PSFM source commands
*              commands placed into the TOC file. When the last user source
*              file is completed the TOC file is read by PSFM and the actual
*              TOC is gerated. Thus PSFM generates PSFM code as is runs that
*              it will also interpret prior to its termination!
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      21 December 1988
*   Author:    E. Scott Daniels
*
*   Modified:  23 Apr 1991 - To open toc file based on buffer not const
*              15 Dec 1992 - To convert to postscript and AFI
*               7 Apr 1994 - To setup for TOC now that linesize/cd are points
*               6 Sep 1994 - Conversion for RFM
*	.tc [on|off] (default is on)
***************************************************************************
*/
void FMtc( )
{
	char *buf;           /* pointer at the token */
	int len;             /* len of parameter entered */
	int i;               /* loop index */
	char title[255];     /* buffer to use to space over on write */

	len = FMgetparm( &buf );    /* is there a parm? */

	if( len <= 0 )     			/* no parameter entered */
		flags = flags | TOC;        /* assume that we should turn it on */
	else
	{
		switch( buf[1] )       
		{
			case 'n':
			case 'N':                   /* user wants it on */
				flags |=  TOC;        /* so do it */
				break;

			default:
				flags &=  ~TOC;	       /* if we dont recognise the string; off it */
				return;
		}             
	}               

	if( flags & TOC  &&  tocfile < VALID )      /* need to open and setup file */
	{
		tocfile = AFIopen( tocname, "w" );        /* open file for writing */
		if( tocfile < 0 )
		{
			FMmsg( E_CANTOPEN, tocname );
			return;
		}

		/* setup initial psfm commands in toc file */
		AFIwrite( tocfile, ".pa\n" );
		AFIwrite( tocfile, ".cd 1 7.5i i=.75i : .st 15 .sf f4\\i\n" );
		AFIwrite( tocfile, ".rh : .rf : .ll 7i .pn off .tc off\n" );
		AFIwrite( tocfile, ".ce Table Of Contents\n" );
		AFIwrite( tocfile, ".sp 2\n" );
	}
}      
