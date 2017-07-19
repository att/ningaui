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

/* --------------------------------------------------------------------------
*
* Mnemonic:	fmsetcolour
* Abstract: 	Crude colour changing -- would be better, but not as fast to 
*		implement, if it was worked into the fmfmt stuff.
* Date:		20 Nov 2002
* Author:	E. Scott Daniels
* --------------------------------------------------------------------------
*/
FMsetcolour( char *t )
{
	static char *lastcolour = NULL;  

	double r;
	double g;
	double b;
	int c;

	char buf[100];

	if( ! t )
	{
		TRACE( 1 )  "setcolour: colour reset\n" );
		if( lastcolour )
			free( lastcolour );
		lastcolour = NULL;		/* allow user to reset to force a colour change next time */
		return;
	}

	if( lastcolour )
	{
		if( strcmp( t, lastcolour ) == 0 )
			return;
		else
			free( lastcolour );
	}

	lastcolour = strdup( t );


	TRACE( 2 )  "setcolour: colour=%s\n",  t ? t : "NULL" );
	if( *t == '#' )
		t++;

	sscanf( t, "%x", &c );

	b = c & 0xff;
	c  >>= 8;

	g = c & 0xff;
	c  >>= 8;

	r = c & 0xff;

	sprintf( buf,  "%0.2f %0.2f %0.2f setrgbcolor\n", r/255, g/255, b/255 );
	AFIwrite( ofile, buf );
}
