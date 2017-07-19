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
*              3 Apr 1994 - To use AFI's new tokenizer!
*		13 Jan 2002 - To reference vars via symbol table - removed varlist
*		08 Nov 2006 - To allow input to default from stdin rather than having to say
******************************************************************************
*/
int FMinit( int argc, char **argv )
{
	int i;               /* loop index */
	char *ptr;           /* pointer to argument */
	char *ffilen;        /* pointer to font file name in environment */
	int file;            /* file number of font file */
	char buf[1024];      /* input buffer for reads */
	char *ofname = "stdout";
	char *ifname = "stdin";


	if( argc >= 2 )
	{
		if( strcmp( argv[1], "-?" ) == 0 )
		{
			FMmsg( ERROR, "Usage: rfm [input-file [output-file]]" );
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

 symtab = sym_alloc( 4999 );
 /* set afi input environment for tokenizing input and end of buffer signal*/
 AFIsettoken( fptr->file, symtab, " `\t", '&', '^', ":" );
 AFIsetflag( fptr->file, AFI_F_EOBSIG, AFI_SET );


 ofile = AFIopen( ofname, "w" );      /* open output file */
 if( ofile < VALID )
  {
   FMmsg( E_CANTOPEN, ofname );
   FMclose( );
   return( ERROR );
  }

 for( i = argc - 1; i > 2; i-- )         /* run in reverse order to */
  AFIpushtoken( fptr->file, argv[i] );   /* push commands from cmd line */

 FMmsg( -1, "RFM text formatter V2.1-0b086 started.");		


 AFIwrite( ofile, "{\\rtf1\\ansi \\deff4\\deflang1033" );		 /* rtf specific initialissation */

 path = getenv( "RFM_PATH" );    /* get our path search string for im cmds */
 if( path == NULL )
	path = getenv( "XFM_PATH" );   /* if no rfm specific, try for generic */


 if( (ptr = getenv( "RFM_FONT_TABLE" )) == NULL )
  {
	AFIwrite( ofile, "{\\fonttbl{\\f1 Symbol;}{\\f2 Arial;}{\\f3 MS Sans Serif;}" );
	AFIwrite( ofile, "{\\f4 Times New Roman;}{\\f5 Arial;}{\\f6 Roman;}" );
	AFIwrite( ofile, "{\\f11 Courier New;}{\\f19 Bookman Old Style;}" );
	AFIwrite( ofile, "{\\f21 Wingdings;}{\\f24 Modren;}" );
	AFIwrite( ofile, "{\\f26 Arial Rounded MT Bold;}" );
	AFIwrite( ofile, "{\\f74 Comic Sans MS;}}" );
  }
 else
  {
   file = AFIopen( ptr, "r" );
   if( file >= OK )
    {
     while( AFIread( file, buf ) >= 0 )
      {
       AFIwrite( ofile, buf );      /* copy user's input directly to output */
      }
     AFIclose( file );
    }
   else
    {
     FMmsg( 0, "Cannot open font file." );
     return( ERROR );
    }
  }

 AFIwrite( ofile, "\\margb360\\margl360\\margt720\\footery350" );

/* now initialize from a C point of view */

 difont = NULL;                             /* initially no list item font */
 ffont = NULL;                              /* no figure font defined */

 curfont = strdup( "f2" );
 runfont = strdup( "f4");   

 iptr = 0;
 optr = 0;                 				/* insert point into output buffer */
 obuf = (char *) malloc( sizeof(char) * MAX_READ );
 inbuf = (char *) malloc( sizeof(char) * MAX_READ );
 if( ! obuf || ! inbuf )
  {
   printf( "malloc of inbuf/obuf failed\n" );
   return ERROR;
  }

 sprintf( inbuf, ".dv rfm 1 : " );   		/* define rfm */
 AFIpushtoken( fptr->file, inbuf );  		

 cur_col = firstcol = (struct col_blk *) malloc( sizeof( struct col_blk ) );
 if( cur_col == NULL )
  return( ERROR );

 cur_col->lmar = DEF_LMAR;
 cur_col->width = 550 - DEF_LMAR; /* set single column width */
 cur_col->next = NULL;          /* by default we are in single column mode */

 flags = PARA_NUM;              /* turn on paragraph numbering */
 flags2 = F2_SETFONT;           /* cause flush to set the font */


	memset( tocname, 0, sizeof( tocname ) );
	snprintf( tocname, sizeof( tocname ) - 5, "%s",  ifname );	/* leave room for extension and null */
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

 textspace = 1;             /* rfm space calculations are different */
 boty -= 65;                /* so adjust standard psfm defaults */

 FMpara( 0, FALSE );        /* setup for first paragraph */
 FMsetmar( );               /* properly setup the right "margin" */
 rflags = RF_SBREAK;        /* section break has been issued */
 return( VALID );
}
