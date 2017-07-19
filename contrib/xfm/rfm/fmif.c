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
*  Mnemonic: FMif
*  Abstract: This routine is responsible for processing the conditional
*            source inclusion .if command. If the next token on the line
*            following the .if command is a defined variable then the
*            lines between the .if and .fi commands are processed normally.
*            If the variable is not defined, then the lines are skipped.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     28 July 1992
*  Author:   E. Scott Daniels
*
*  Modified: 20 Jul 1994 - To allow not processing
*            27 Jul 1994 - To allow nested ifs and elses
*            28 Jul 1994 - To allow ors and ands
*****************************************************************************
*/

#define VTY_CHAR  0
#define VTY_NUM   1 

#define COP_AND   0
#define COP_OR    1
#define COP_NONE  2

struct val_blk
{
	int type;
	long nval;          /* numeric value */
	char cval[1024];    /* character value */
} ;

/* get parms until we find end quote */
static void get_string( char *buf, char *tok )
{
	char	*p;
	int	len;
	int	tlen = 0;

	len = strlen( tok );
	*buf = 0;

	do
	{
		if( tlen + len > 1023 )
		{
			fprintf( stderr, "if: string too long: %s\n", buf );
			return;
		}

		strcat( buf, tok );
		strcat( buf, " " );

		if( index( tok, '"' ) )
		{
			*(buf+tlen + len - 1) = 0;	/* stomp on " at end */
			if( trace )
				fprintf( stderr, "if/get_str: built %s\n", buf );
			return;
		}

		tlen += len + 1;
	}
	while( (len = FMgetparm( &tok )) != 0 );    /* get parm - if none then return */
}

void FMif( )
{
	char *tok;                   /* pointer to the token */
	char *var;                   /* pointer to expanded var string */
	int vidx = 0;                /* value index */
	int len;                     /* length of token */
	int not = FALSE;             /* not indicated flag */
	int cop = TRUE;
	int status[100];
	int sidx = 1;                /* status index */
	int nested = 0;              /* number of nested .fis to look for */
	struct val_blk value[3];    /* value of variables */
	int i;
	int count = 0;

	if( (len = FMgetparm( &tok )) == 0 )    /* get parm - if none then return */
	{
		FMmsg( E_MISSINGPARM, "On .if statement" );
		return;
	}

	status[0] = FALSE;

	do                        /* test each name on the if statment */
	{
		TRACE( 1 ) "if: working on token=%s\n", tok );

		switch( tok[0] )
		{
			case 0:
				break;

			case '!':                /* not symbol? */
				not = TRUE;
				tok++;
				len--;
				break;

			case '|':
				if( sidx >= 2 )
				{
					status[sidx-2] = status[sidx-1] | status[sidx-2];
					sidx -= 2;
				}
				else
					status[sidx] = FALSE;
	
				sidx++;
				tok++;
				len--;
				break;

			case '&':                 /* & or &name */
				if( tok[1] != 0 )		/* late evaluating variable name -- we must expand for value */
				{
					char *vp = NULL;

					TRACE( 1 ) "fmif: expanding variable %s\n", tok );
					if( (vp = sym_get( symtab, tok+1, 0 )) != NULL )   
					{
						if( isdigit( *vp ) )      
						{
							TRACE( 2 ) "fmif: variable expanded to number: %s\n", vp );
							value[vidx].nval = atol( vp );
							status[sidx] = not ? ! value[vidx].nval : value[vidx].nval;
							value[vidx].type = VTY_NUM;    
							vidx++;
						}
						else
						{
							TRACE( 2 ) "fmif: variable expanded to string: %s\n", vp );
							strcpy( value[vidx].cval, vp ); 
							value[vidx].nval = not ? 0 : 1;
							value[vidx].type = VTY_CHAR;    
							vidx++;
						}
					}
					else
						value[vidx].nval = not ? 0 : 1;
					len = 0;			/* force get parm to fire */
				}
				else
				{
					TRACE( 1 ) "fmif: AND\n", tok );
					if( sidx >= 2 )
					{
						status[sidx-2] = status[sidx-1] & status[sidx-2];
						sidx -= 2;
					}
					else
						status[sidx] = FALSE;

					sidx++;
					tok++;
					len--;
				}
				break;

			case '<':
				if( sidx > 2 )
					sidx -= 2;
				else
					sidx = 1;
				if( vidx >= 2 )
				{
					if( value[0].type == VTY_NUM && value[1].type == VTY_NUM ) 
						status[sidx] = value[0].nval < value[1].nval;
					else
						if( strcmp( value[0].cval, value[1].cval ) < 0 )
							status[sidx] = TRUE;
						else
							status[sidx] = FALSE;

					if( not )
						status[sidx] = !status[sidx];
				}
				else
					status[sidx] = FALSE;

				sidx++;
				tok++;
				len--;
				vidx = 0;
				not = FALSE;
				break;

			case '>':
				if( sidx > 2 )
					sidx -= 2;
				else
					sidx = 1;
				if( vidx >= 2 )
				{
					if( value[0].type == VTY_NUM && value[1].type == VTY_NUM ) 
						status[sidx] = value[0].nval > value[1].nval;
					else
						if( strcmp( value[0].cval, value[1].cval ) > 0 )
							status[sidx] = TRUE;
						else
							status[sidx] = FALSE;
			
					if( not )
						status[sidx] = !status[sidx];
				}
				else
					status[sidx] = FALSE;

				sidx++;
				tok++;
				len--;
				vidx = 0;
				not = FALSE;
				break;
	
				case '=':
					if( sidx > 2 )
						sidx -= 2;
					else
						sidx = 1;
					if( vidx >= 2 )
					{
						if( value[0].type == VTY_NUM && value[1].type == VTY_NUM ) 
							status[sidx] = ! (value[0].nval - value[1].nval);
						else
						{
							status[sidx] = ! strcmp( value[0].cval, value[1].cval );
							TRACE( 2)  "(%s) (%s) == %d (not=%d)\n", value[0].cval, value[1].cval, status[sidx], not );
						}
	
						if( not )
							status[sidx] = !status[sidx];
					}
					else
						status[sidx] = FALSE;

					sidx++;
					tok++;
					len--;
					vidx = 0;
					not = FALSE;
					break;

				case '"': 
					tok++;
					len -= 2;
					get_string( value[vidx].cval, tok );
					value[vidx].nval = not ? 0 : 1;
					value[vidx].type = VTY_CHAR;    
					vidx++;
					not = FALSE;
					len = 0;
					break;

				default:                                 /* characters of somesort */
					value[vidx].type = VTY_NUM; 
					if( isdigit( tok[0] ) )      
					{
						tok[len] = 0;
						value[vidx].nval = atol( tok );
						status[sidx] = not ? ! value[vidx].nval : value[vidx].nval;
						vidx++;
					}
					else
					{
						if( sym_get( symtab, tok, 0 ) == NULL )   /* defined? */
							status[sidx] = not ? 1 : 0;                  /* save only status */
						else
							status[sidx] = not ? 0 : 1;
					}

					len = 0;                           /* done with the parameter */
					not = FALSE;
					sidx++;
					break;
			}

		if( status[sidx] != 0 )
			status[sidx] = TRUE;              /* set specifically to true */

		if( vidx > 2 )
			vidx = 0;



		if( len <= 0 )
			len = FMgetparm( &tok );       /* need another token to process */
	}
	while( len > OK );               /* while parameters - evaluate them */


	TRACE(2) "fmif: expression evaluated to: %s\n", status[sidx-1] ? "TRUE" : "FALSE" );

	if( ! status[sidx-1] )             /* statement evaluated to false */
	{                                              /* then skip until .fi */
		while( FMgettok( &tok ) > OK )   /* while tokens still left in the file */
		{
			if( strncmp( ".if", tok, 3 ) == 0 )  /* found nested if */
				nested++;                           /* increase nested count */
			else
				if( strncmp( ".fi", tok, 3 ) == 0 )  /* found an end if */
				{
					if( nested == 0 )                   /* and we need only it */
						return;                            /* then were done */
					else
						nested--;                          /* one less were looking for */
				}
				else
					if( (strncmp( ".ei", tok, 3 ) == 0) && (nested == 0) )  /* last else */
						return;
		}     
	}              
	else
		return;        

	FMmsg( E_UNEXEOF, ".if not terminated" );
}    

