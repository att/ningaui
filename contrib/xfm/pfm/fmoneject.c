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
*  Mnemonic: FMoneject
*  Abstract: This saves the commands entered and executes them when the next 
*	     page eject occurs. Useful for restoring two column mode after 
*	     entering single column mode for a table or something.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     17 Aug 2001
*  Author:   E. Scott Daniels
*
* .oe [n=name] [all] [del|col|page] <commands>
****************************************************************************
*/
struct eject_info {
	struct eject_info  *next;
	struct eject_info  *prev;
	char	*name;
	char	*cmd_str;
	int	flags;
};

#define EF_ALL	0x01		/* apply to all, not just next */
#define EF_COL	0x02		/* at next/all column ejects */
#define EF_PAGE 0x04		/* at next/all page ejects */
#define EF_DEL	0x08

static struct eject_info *ej_head = NULL;
static struct eject_info *ej_tail = NULL;

static struct eject_info *find_ej( char *name )
{
	struct eject_info *ep;

	for( ep = ej_head; ep; ep = ep->next )
		if( strcmp( name, ep->name ) == 0 )
			return ep;

	return NULL;
}

static del_ej( struct eject_info *ep, char *name )
{
	struct eject_info *dp;

	if( ! ep && (! name || ! *name) )
		return;				/* should write a message */

	if( ep )
		dp = ep;			/* user had one in mind; may be one time, and unnamed */
	else
		dp = find_ej( name );

	if( !dp )
		return;
	
	if( dp->prev )
		dp->prev->next = dp->next;
	if( dp->next )
		dp->next->prev = dp->prev;

	if( ej_head == dp )
		ej_head = dp->next;
	if( ej_tail == dp )
		ej_tail = dp->prev;

	TRACE( 2 ) "fmoneject/del_ej: deleted: name=%s cmd=%s\n", name ? name : "unnamed", dp->cmd_str );
	if( dp->name )
		free( dp->name );

	free( dp->cmd_str );
	free( dp );
}

/* do the ejects for the specified type */
void FMateject( int page )
{
	FILE	*f = NULL;
	struct eject_info *ep;
	struct eject_info *nxt;		/* next so we can delete as we go if needed */
	char	*fname = "./.pfm_eject";
	int	type = 0;
	char	*tok = NULL;
	char	wrk[1024];

	if( page )
		type = EF_PAGE;
	else
		type = EF_COL;

	nxt = NULL;
	for( ep = ej_head; ep; ep = nxt )
	{
		nxt = ep->next;
		if( ep->flags & type )	
		{
			if( ! f && (f = fopen( fname, "w" )) == NULL  )
			{
				fprintf( stderr, "abort: ateject cannot open tmp file %s\n", fname );
				exit( 1 );
			}
	
			fprintf( f, "%s\n", ep->cmd_str );
			TRACE( 1 ) "ateject: queued for execution: %s\n", ep->cmd_str );

			if( ! (ep->flags & EF_ALL) )		/* if all flag is not on, then trash it after first use */
				del_ej( ep, NULL );
		}
	}

	if( f )
	{
		fclose( f );
		sprintf( wrk, ".im %s", fname );
 		AFIpushtoken( fptr->file, wrk );  	/* push to imbed our file and then run it */

		FMpush_state();
		
   		flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   		flags2 &= ~F2_ASIS;                  /* turn off asis flag */

		if( FMgettok( &tok ) )
		{
			TRACE( 2 ) "ateject: running: %s\n", tok );
			FMcmd( tok );
		}

		FMpop_state();
		unlink( fname );
	}
}

static list_ej( )
{
	struct eject_info *ep;

	for( ep = ej_head; ep; ep = ep->next )
		fprintf( stderr, "oneject data: type=%s name=%s cmd=%s\n", ep->flags & EF_PAGE ? "page" : "col", ep->name ? ep->name : "unnamed", ep->cmd_str );

}

void FMoneject( )
{
	int 	flags = 0;
	int	recognised = 1;
	char	*buf;
 	char	wrk[2048];  		/* tmp buffer to build new name in */
	char	*name = NULL; 
	struct eject_info *ep = NULL;

	if( FMgetparm( &buf ) <= 0 )     /* get the first command or "all" */
 		return;                     /* no name entered then get out */

	wrk[0] = 0;
	while( recognised )
	{
		if( strcmp( "all", buf ) == 0 )
			flags |= EF_ALL;
		else
		if( strncmp( "col", buf, 3 ) == 0 )
			flags |= EF_COL;
		else
		if( strncmp( "page", buf, 4 ) == 0 )
			flags |= EF_PAGE;
		else
		if( strncmp( "del", buf, 3 ) == 0 )
			flags |= EF_DEL;
		else
		if( strncmp( "n=", buf, 2 ) == 0 )
			name = strdup( buf+2 );
		else
		if( strncmp( "list", buf, 4 ) == 0 )
		{
			list_ej( );
			return;
		}
		else
		{
			strcat( wrk, buf );		/* save first token of command and force exit */
			break;
		}
	
		if( FMgetparm( &buf ) <= 0 )     
			if( ! flags & EF_DEL )		/* if not deleting, then */
 				return;			/* there must be at least one command if not deleting */
	}

	while( FMgetparm( &buf ) > 0 )
	{
		if( *wrk )			/* if not the first thing added */
			strcat( wrk, " " );	/* add a space before next thing */
		strcat( wrk, buf );
	}

	if( flags & EF_DEL )
	{
		del_ej( NULL, name );
		return;
	}

	if( ! flags )				/* default to column, one time */
		flags = EF_COL;

	ep = (struct eject_info *) malloc( sizeof( struct eject_info ) );
	if( !ep )
	{
		fprintf( stderr, "error allocating ep in on eject\n" );
		exit( 1 );
	}

	memset( ep, 0, sizeof( struct eject_info ) );
	ep->flags = flags;
	ep->cmd_str = strdup( wrk );
	if( name )
		ep->name = name;

	TRACE( 1 ) "fmoneject: added: flags=x%02x name=%s cmd=%s\n", flags, name ? name : "unnamed", ep->cmd_str );

	if( ej_head )			/* list exists, add at the end of the list */
	{
		ej_tail->next = ep;
		ep->prev = ej_tail;
		ej_tail = ep;
	}
	else
		ej_head = ej_tail = ep;

#ifdef OLD_STUFF

	if( all )
	{
		if( col )
		{
			if( onallcoleject )
				free( onallcoleject );
			onallcoleject = strdup( wrk );
		}
		else
		{
			if( oneveryeject )
				free( oneveryeject );
			oneveryeject = strdup( wrk );
		}
	}
	else
	{
		if( col )
		{
			if( oncoleject )
				free( oncoleject );
			oncoleject = strdup( wrk );
		}
		else
		{
			if( onnxteject )
				free( onnxteject );
			onnxteject = strdup( wrk );
		}
	}
#endif
}                         /* FMoneject */
