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
*  Mnemonic: FMinit
*  Abstract: This routine opens the initial input file and the output file
*            and does other necessary house keeping chores.
*  Parms:    argc - Number of arguments passed to fm from command line
*            argv - Argument vector list passed from command line
*  Returns:  Valid if all is well, ERROR if system could not be initialized.
*  Date:     17 November 1988
*  Author:   E. Scott Daniels
*
*  Modified:  29 Jun 1994 - To convert to rfm.
*              2 Dec 1994 - To correct a problem in RFM init strings.
*              5 Dec 1994 - To properly terminate font strings.
*                           To look and use environment variable to define
*                            the font table. (RFM_FTABLE)
*              6 Dec 1996 - To convert to hfm
*              4 Apr 1997 - To put in support for AFI's new tokenizer!
*             15 Apr 1997 - To get tokens as commands from command line.
*             31 Mar 2000 - To init text colour
*		08 Nov 2006 - Better arg processing; no parms allows stdin/out by default
*			rather than forcing a specification.
*		22 Oct 2007 - AFI settoken nolonger needs backquote as a seperator; fmgetparm
*			and fmgettok do the right thing now. 
******************************************************************************
*/
int FMinit( int argc, char **argv )
{
	int i;               /* loop index */
	char *ptr;           /* pointer to argument */
	char *ffilen;        /* pointer to font file name in environment */
	int file;            /* file number of font file */
	char buf[1024];      /* work buffer */
	char *ifname = "stdin";
	char *ofname = "stdout";

	version = strdup( "hfm V2.3/0b218" );

	if( argc >= 2 )
	{
		if( strcmp( argv[1], "-?" ) == 0 )
		{
			FMmsg( ERROR, "Usage: tfm [input-file> [output-file [inital-command]]]" );
			return ERROR;
		}

		if( *argv[1] != '-' )		/* allow - to be default to stdin */
			ifname = argv[1];
	}

	if( argc > 2 )
	{
		if( *argv[2] != '-' )		/* allow - to be default to stdout */
			ofname = argv[2];
	}

	if( strcmp( ifname, ofname ) == 0 )
	{
		FMmsg( ERROR, "input name cannot be same as output; this is just wrong");
		return( ERROR );
	}

	if( FMopen( ifname ) < VALID )       /* open the initial input file */
		return ERROR;                     /* getout if could not do it */

	symtab = sym_alloc( 4999 );		/* symbol table for variables */
	AFIsettoken( fptr->file, symtab, " \t", '&', '^', ":" );
	AFIsetflag( fptr->file, AFI_F_EOBSIG, AFI_SET );

	ofile = AFIopen( ofname, "w" );      /* open output file */
	if( ofile < VALID )
	{
		FMmsg( E_CANTOPEN, ofname );
		FMclose( );
		return( ERROR );
	}

	for( i = argc - 1; i > 2; i-- )         /* run remaining args in reverse */
		AFIpushtoken( fptr->file, argv[i] );   /* to push on input stack */

	snprintf( buf, sizeof( buf ), "+HFM text formatter (%s) started", version );
	FMmsg( -1, buf );

	/* write html startup stuff to the file */
	snprintf( buf, sizeof( buf ), "<HTML> <!-- %s generated this HTML document -->", version );
	AFIwrite( ofile, buf );
	AFIwrite( ofile, "<HEAD> <font size=2 >" );


	difont = NULL;                             /* initially no list item font */
	ffont = NULL;                              /* no figure font defined */
	curfont = strdup( "f2" );
	runfont = strdup( "f4");                

	if( (path =  getenv( "HFM_PATH" ))  == NULL )
		path = getenv( "XFM_PATH" );

	obuf = (char *) malloc( sizeof( char ) * 2048 );
	inbuf = (char *) malloc( sizeof( char ) * 2048 );

	sprintf( inbuf, ".dv hfm 1 " );   /* simulate a user command - define hfm */
	iptr = 0;
	optr = 0;                 /* start at beginning of the output buffer */
	max_obuf = 256;

	if( !obuf )
	{
		printf( "malloc of obuf failed\n" );
		return ERROR;
	}
	*obuf = (char) 0;

	sprintf( inbuf, ".dv hfm 1 : " );      /* simulate command to make */
	AFIpushtoken( fptr->file, inbuf );     /* what processer we are var */

	cur_col = firstcol = (struct col_blk *) malloc( sizeof( struct col_blk ) );
	if( cur_col == NULL )
		return( ERROR );

	cur_col->lmar = DEF_LMAR;
	cur_col->width = 8.5 * 72;     /* set single column width */
	cur_col->next = NULL;          /* by default we are in single column mode */

	flags = PARA_NUM;              /* turn on paragraph numbering */

	memset( tocname, 0, sizeof( tocname ) );
	snprintf( tocname, sizeof(tocname) - 5, "%s", ifname );
	if( (ptr = strrchr( tocname, '.' )) != NULL )
		*ptr = 0;
	strcat( tocname, ".toc" );		/* same filename with .toc extension */
	
	memset( pnum, 0, sizeof( pnum ) );

	for( i = 0; i < MAX_HLEVELS; i++ )   /* allocate and init header blks */
	{
		headers[i] = (struct header_blk *) malloc( sizeof( struct header_blk ) );
		if( headers[i] == NULL )
			return( ERROR );

		headers[i]->font = (char *) malloc( (strlen( DEF_HEADFONT )) + 1 );
		strcpy( headers[i]->font, DEF_HEADFONT );  /* move in default string */
	
		headers[i]->flags = HTOC;              /* initially only TOC flag set */
		headers[i]->indent = DEF_HEADINDENT;   /* set default indention */
		headers[i]->level = i+1;               /* set the level */
		headers[i]->skip = 2;                  /* skip down 2 lines */
		headers[i]->hmoffset = 0;              /* initially no header margin off */
	}

	headers[0]->size = DEF_H1SIZE;   /* set default header text sizes */
	headers[1]->size = DEF_H2SIZE;
	headers[2]->size = DEF_H3SIZE;
	headers[3]->size = DEF_H4SIZE;
	headers[0]->flags |= HEJECTC+HTOUPPER;   /* header level 1 defaults */

	flags &= ~PAGE_NUM;        /* turn off page numbering as default */

	textcolour = 0;
	textspace = 1;             /* hfm space calculations are different */
	boty -= 65;                /* so adjust standard defaults */


	return( VALID );
}            /* FMinit */
