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


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
****************************************************************************
*
* Mnemonic: FMcss
* Abstract: Supports HTML cascading style sheets 
* Parms:    None.
* Returns:  Nothing.
* Date:     27 November 2002
* Author:   E. Scott Daniels
*
* Modified: 
* .cs start name {attrributes -- see below}  (starts a division)
* .cs table {atrributes} 		(builds the table style string)
* .cs end				(ends a divison) 
* Attributes: one or more may be defined.
	[bc=border-colour]
	[bs=border-style]
	[bw=border-width] (e.g. 1px)
	[bg=colour]
	[c=colour] 
	[fg=colour] 
	[f=fontfamily] 	(serif, sans-serif, cursive, fantasy, monospace)
	[fs=fontsize] 	(e.g. 10px)
	[l=left] 	(e.g. 10px)
	[t=top] 	(e.g. 10px)
	[w=width] 	(e.g. 10px)
	[s=fontstyle] 	(normal, italic, oblique)
	[h=height] 	(e.g. 10px)
	[p=positiontype] 	(relative, absolute)
	[i=textindent]		(e.g. 10px)
	[exact-name=value]	(there are just too many options to keep adding; so we do this now)
	[x=exact-name=value]	(allows anything not coded for here to be entered)

#	border-style is one of 
		none : No border, sets width to 0. This is the default value. 
		hidden : Same as 'none', except in terms of border conflict resolution for table elements. 
		dashed : Series of short dashes or line segments. 
		dotted : Series of dots. 
		double : Two straight lines that add up to the pixel amount defined as border-width. 
		groove : Carved effect. 
		inset : Makes the box appear embedded. 
		outset : Opposite of 'inset'. Makes the box appear 3D (embossed). 
		ridge : Opposite of 'groove'. The border appears 3D (coming out). 
		solid : Single, straight, solid line.
*
****************************************************************************
*/
#define CSS_END 0
#define CSS_TABLE 1
#define CSS_DIV 2
void FMcss( )
{
	static depth = 0;		/* if in a div then we set the no font flag */

	int type = CSS_END;
	int len;        /* length of parameter entered on .bx command */
	char *buf;      /* pointer at the input buffer information */
	char wbuf[256];
	char	*cp;


	FMflush( );
	*obuf = 0;

	len = FMgetparm( &buf );   /* get next parameter on command line */

	if( len > 0 )
	{
		if( toupper( buf[0] ) == 'S' )  /* start division */
		{
			depth++;
			type = CSS_DIV;
			FMgetparm( &buf );		/* get name */
			sprintf( obuf, "<div name=%s style=\"", buf );
		}
		else
		if( toupper( *buf ) == 'T' )	/* create a def for table */
		{
			type = CSS_TABLE;
			sprintf( obuf, "style=\"", buf );
		}


		while( FMgetparm( &buf ) )  
		{
			*wbuf = 0;
			if( *(buf+1) == '=' || *(buf+2) == '=' )	/* assume old style x= or xx= short versions */
			{
				switch( *buf )
				{
					case 'b':
					switch( *(buf+1) )
					{
						case 'c':		/* border colour */
							sprintf( wbuf, "border-color: %s; ", buf+3 );
							break;

						case 'g':
							sprintf( wbuf, "background-color: %s; ", buf+3 );
							break;
							
						case 's':	/* border style */
							sprintf( wbuf, "border-style: %s; ", buf+3 );
							break;

						case 'w':		/* border width */	
							sprintf( wbuf, "border-width: %spx; ", buf+3 );
							break;
					}
					break;
					
					case 'c':
						sprintf( wbuf, "color: %s; ", buf+2 );
						break;
	
					case 'f':  			/* fs=fonsize | f=fontfamily */
						if( *(buf+1) == 's' )	/* font size */
						{
							sprintf( wbuf, "font-size: %s; ", buf+3 );
						}		
						else
						{
							sprintf( wbuf, "font-family: %s; ", buf+2 );
						}
						break;
	
					case 'h':
						sprintf( wbuf, "height: %s; ", buf+2 );
						break;
	
					case 'i':
						sprintf( wbuf, "text-indent: %s; ", buf+2 );
						break;
	
					case 'l':
						sprintf( wbuf, "left: %s; ", buf+2 );
						break;
	
					case 'p':
						sprintf( wbuf, "position: %s; ", buf+2 );
						break;
	
					case 's':
						sprintf( wbuf, "font-style: %s; ", buf+2 );
						break;
	
	
					case 't':
						sprintf( wbuf, "top: %s; ", buf+2 );
						break;
	
					case 'w':		/* w=nnn[px] */
						sprintf( wbuf, "width: %s; ", buf+2 );
						break;

					case 'x':		/* x=exact-name=value */
						if( (cp = strchr( buf+2, '=' )) != NULL )
						{
							*(cp++) = 0;
							sprintf( wbuf, "%s: %s; ", buf+2, cp );
						}
						break;
	
					default:
						break;
				}
			}
			else				/* assume if not x= or xx= then its keyword=value */
			{
				if( (cp = strchr( buf, '=' )) != NULL )
				{
					*(cp++) = 0;
					sprintf( wbuf, "%s: %s; ", buf, cp );
				}
			}

			if( *wbuf != 0 )
			{
				TRACE(3) "css style added: %s\n", wbuf );
				strcat( obuf, wbuf );
			}
	
		}		/* end while */

			if( type == CSS_DIV )
				strcat( obuf, "\">\n" );
			else
				strcat( obuf, "\"\n" );
	}


	switch( type )
	{
		case CSS_END:
			sprintf( obuf, "</div>\n" );
			depth--;

		case CSS_DIV:
			TRACE(1) "css div: %s\n", obuf );
			AFIwrite( ofile, obuf );
			break;

		case CSS_TABLE:
			free( table_css );
			table_css = strdup( obuf );
			if( trace > 1 )
				fprintf( stderr, "fmcss: setting table_css = (%x) %s\n", table_css, table_css );
			break;
	}

	*obuf = 0;
	optr=0;

	if( depth > 0 )
		flags3 |= F3_NOFONT;		/* no font changes */
	else
		flags3 &= ~F3_NOFONT;

}                        /* fmcss */
