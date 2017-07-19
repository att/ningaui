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
*  Mnemonic:	fmindex
*  Abstract:	Support for adding an index to the document
*  Date:	19 Oct 2007
*  Author: 	E. Scott Daniels
*
*	.ix snare word [word...]	# add words to list for index
*	.ix synonym existing new	# add new as a synonym for existng
*	.ix pause			# stop snaring words
*	.ix resume			# start snaring words (impled with first snare command)
*	.ix force token(s)		# force a reference on current page (mostly for multiword tokens)
*	.ix group name word [word...]	# grouping by name in the index listing each word
*	.ix groupb format tokens	# printf format string for start of group; %s is the group name
*	.ix groupe format tokens	# printf format string for end of group
*	.ix insert			# insert the index at this place in the document
*	.ix space format tokens		# define a space format string inserted at alpha breaks
*	.ix section format tokens	# same as space, but a better name
*	.ix term format tokens		# define the format statement for each term 
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

static char *term_fmt = NULL;		/* format for each term: ".di %s : " or somesuch */
static char *space_fmt = NULL;		/* format for 'space' between sections ".ed .sp 0 &bold(%c) .bd 1i"  or somesuch */
static char *groupb_fmt = NULL;		/* begin and end format strings for group terms */
static char *groupe_fmt = NULL;

/* insert the index right here in the doc! */
static void insert( )
{
	char	*tname = NULL;					/* temp file name template */
	int	tfd;						/* file des from mk tmp name */
	FILE	*tf;

	tname = strdup( "/tmp/xfm.index.XXXXXX" );		/* temp name template */
	TRACE(2) "index: creating tmp file for index commands: %s\n", tname );

	tfd =  mkstemp( tname );				/* make and open the file */
	if( (tf = fdopen( tfd, "w" )) == NULL )
	{
		close( tfd );
		return;
	}

	TRACE(1) "index: insert: term=(%s)  index space=(%s)\n", term_fmt, space_fmt );
	di_list( tf, term_fmt, space_fmt, groupb_fmt, groupe_fmt );			/* generate index commands to a temp file */
	fclose( tf );

	FMopen( tname );					/* push index stuff onto our read stack */
	unlink( tname );					/* delete on close */

	free( tname );
}


void fmindex( )
{ 
	char	*buf;
	int 	len;
	char	wbuf[2048];

	if( (len = FMgetparm( &buf) ) != 0  )
	{
		switch( *buf )
		{
			case 'f':					/* force an entry -- probably a multi token hit */
				if( strcmp( buf, "force" ) == 0 )
				{
					wbuf[0] = 0;
					while( (len = FMgetparm( &buf )) != 0 )
					{
						strcat( wbuf, " " );
						strcat( wbuf, buf );
					}

					TRACE( 1 ) "index: forcing: (%s)\n", wbuf+1 );
					di_add( wbuf+1, page + 1 );
				}
				break;

			case 'g':				/* groupe, groupb or groupe */
				if( strcmp( buf, "group" ) == 0 )
				{
					char *gname;

					if( (len = FMgetparm( &buf )) == 0 )
						return;
					gname = strdup( buf );
					TRACE(1) "index: group added: %s\n", gname );
					while( (len = FMgetparm( &buf )) != 0 )
					{
						di_group( gname, buf );
					}
					free( gname );
				}
				else
				if( strcmp( buf, "groupb" ) == 0 )		/* format to add 'space' between sections */
				{
					if( groupb_fmt )
						free( groupb_fmt );
					wbuf[0] = 0;
					while( (len = FMgetparm( &buf )) != 0 )
					{
						strcat( wbuf, " " );
						strcat( wbuf, buf );
					}
					strcat( wbuf, "\n" );
					groupb_fmt = strdup( wbuf );
					TRACE(1) "index: groupb format now: %s\n", space_fmt );
				}
				else
				if( strcmp( buf, "groupe" ) == 0 )		/* format to add 'space' between sections */
				{
					if( groupe_fmt )
						free( groupe_fmt );
					wbuf[0] = 0;
					while( (len = FMgetparm( &buf )) != 0 )
					{
						strcat( wbuf, " " );
						strcat( wbuf, buf );
					}
					strcat( wbuf, "\n" );
					groupe_fmt = strdup( wbuf );
					TRACE(1) "index: groupe format now: %s\n", space_fmt );
				}

				break;

			case 'i':
				if( strcmp( buf, "insert" ) == 0  )
				{
					TRACE(1) "index: being inserted\n" );
					insert( );
				}
				break;

			case 'p':
				if( strcmp( buf, "pause" ) == 0 )
				{
					TRACE(1) "index: snarfing is paused\n" );
					flags3 &= ~F3_IDX_SNARF;				
				}
				break;

			case 'r':
				if( strcmp( buf, "resume" ) == 0 )
				{
					TRACE(1) "index: snarfing has resumed\n" );
					flags3 |= F3_IDX_SNARF;				
				}
				break;

			case 's':
				if( strcmp( buf, "snare" ) == 0 )
				{
					flags3 |= F3_IDX_SNARF;			/* imply resume */
					while( (len = FMgetparm( &buf )) != 0 )
						di_register( buf, 1 );		/* register; case insensitive */	
				}
				else
				if( strcmp( buf, "synonym" ) == 0 )
				{
					char	*new;
					char 	*old;
					if( FMgetparm( &buf ) <= 0 )
						return;
					old = strdup( buf );
					if( FMgetparm( &buf ) <= 0 )
						return;
					new = strdup( buf );

					di_synonym( old, new );

					free( new );
					free( old );
				}
				else
				if( strcmp( buf, "space" ) == 0 || strcmp( buf, "section" ) == 0  ) /* format to add 'space' between sections */
				{
					if( space_fmt )
						free( space_fmt );
					wbuf[0] = 0;
					while( (len = FMgetparm( &buf )) != 0 )
					{
						strcat( wbuf, " " );
						strcat( wbuf, buf );
					}
					strcat( wbuf, "\n" );
					space_fmt = strdup( wbuf );
					TRACE(1) "index: space format now: %s\n", space_fmt );
				}
				break;

			case 't':
				if( strcmp( buf, "term" ) == 0 )		/* format string used to print each */
				{
					if( term_fmt )
						free( term_fmt );
					wbuf[0] = 0;
					while( (len = FMgetparm( &buf )) != 0 )
					{
						strcat( wbuf, " " );
						strcat( wbuf, buf );
					}
					term_fmt = strdup( wbuf );
					TRACE(1) "index: term format now: %s\n", term_fmt );
				}
				break;
		}
	}
}
