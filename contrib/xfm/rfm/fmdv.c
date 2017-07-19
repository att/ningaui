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
*****************************************************************************
*
*  Mnemonic: FMdv
*  Abstract: This routine adds a new variable to the variable list or
*            modifies an existing variable on the list. The escape character
*            is removed from the front of a parameter ONLY if the next
*            character is in the set: [ : `
*            This allows these characters to not have effect as parms to
*            the dv command, but to have effect when the variable is
*            expanded.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     7 July 1989
*  Author:   E. Scott Daniels
*
*  Modified: 16 Mar 1990 - To eliminate expansion of var on redefiniton.
*            10 Feb 1991 - To use MAX_VARxxx constatnts on name and expansion
*                          string copies.
*            28 Jul 1994 - Not to leave a trailing blank.
*            24 Apr 1997 - To allow escaped [ characters
*            21 Nov 1999 - To make string not use fixed buffer size
*		12 Jan 2002 - To convert to symbol table 
****************************************************************************
*/
void FMdv( )
{
	char *buf;               /* parameter pointer */
	char *value;	 	/* pointer to current value - if prev defined */
	char *ovalue;	 	/* original value */
	int len;                 /* length of the parameter */
 	char str[MAX_VAREXP+1];               /* pointer at the expansion string */
	char name[128];		/* variable name */
	int i;                   /* index vars */
	int j;
	char tbuf[MAX_VARNAME+1];  /* tmp buffer to build new name in */

	len = FMgetparm( &buf );     /* get the name */
	if( len <= 0 )
		return;                     /* no name entered then get out */

	strncpy( name, buf, 127 );
	name[128] = 0;				/* ensure its marked */

	ovalue = sym_get( symtab, name, 0 );		/* must free last incase referenced in the .dv string */

	varbuf = NULL;        /* not going to execute it so remove pointer */

	i = j = 0;       /* prep to load the new expansion information */

	while( (len = FMgetparm( &buf )) > 0 )   /* get all parms on cmd line */
	{
		if( trace > 1 )
			fprintf( stderr, "dv adding tok: (%s)\n", buf );

		for( i = 0; i < len && j < MAX_VAREXP; i++, j++ )
		{
			if( buf[i] == '^' )                      /* remove escape symbol */
			switch( buf[i+1] )                      /* only if next char is special*/
			{
				case ':':
				case '`':
				case '[':
				case '^':
					i++;                              /* skip to escaped chr to copy */
					break;

				default:                              /* not a special char - exit */
					break;
			}                                      /* end switch */

			str[j] = buf[i];                        /* copy in the next char */
		}

		str[j++] = BLANK;          /* seperate parms by a blank */
	}

	j = j > 0 ? j - 1 : j;    /* set to trash last blank if necessary */
	str[j] = EOS;             /* and do it! */

	if( trace )
		fprintf( stderr, "dv created var: %s = (%s)\n", name, str ); 

	value = strdup( str );
	sym_map( symtab, name, 0, value );		/* add to symbol table */
	if( ovalue )
		free( ovalue );
}                         /* FMdv */
