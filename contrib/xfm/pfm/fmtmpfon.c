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
******************************************************************************
*
*  Mnemonic: FMtmpfont
*  Abstract: This routine is responsible for establishing a temporary font 
		change
*  Parms:    fname - Pointer to the font name string
*            fsize - The size of the new font
*  Returns:  Nothing.
*  Date:     20 Nov 2001
*  Author:   E. Scott Daniels
*
*	.tf name size <texttokens>[~token]
*	allow for macros like: .dv ital .tf &italfont &textsize $1~
****************************************************************************
*/
#include <stdio.h>
#include <unistd.h>

void FMtmpfont( )
{
	char 	*buf;
	char	*ofont;		/* old font */
	char	*tok;
	int	osize;
	int	len;

	if( (len = FMgetparm( &buf )) )
	{
		ofont = curfont;
		curfont = malloc( sizeof( char ) * (len+1) );
		strncpy( curfont, buf, len );
		curfont[len] = 0;

		if( (len = FMgetparm( &buf )) )
		{
			osize = textsize;
			if( (textsize = atoi( buf )) == 0 )
				textsize = osize;
			FMfmt_add( );			/* add a format block to the list */
		}
		else
		{
			free( curfont );
			curfont = ofont;
		}
	}

	while( (len = FMgetparm( &buf )) )
	{
		if( (tok = strchr( buf, '~' )) != NULL )
		{
			tok = NULL;

			/*FMaddtok( buf, tok-buf );*/
			FMaddtok( buf, len );
			tok++;
			len = tok-buf;
			break;
		}
		else
			FMaddtok( buf, len );
	}

	free( curfont );
	curfont = ofont;
	FMfmt_add( );	

	if( tok && *tok )	
	{
		flags2 |= F2_SMASH;
		if( *tok == '.' )
			AFIpushtoken(fptr->file, tok );
		else
			FMaddtok( tok, len );
	}
}     /* FMtmpfont */
