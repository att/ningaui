/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

/*
 ----------------------------------------------------------------------------------
  Mnemonic: 	rmif.c
  Abstract: 	repmgr interface funcitons
  Date:		07 September 2006 (HBDHAZ)
  Author: 	E. Scott Daniels
  Mods:		
		10 Oct 2006 (sd) : Fixed problem with index on lines 121/122
		20 Oct 2006 (sd) : Now weeds out odd things that are not 
			node/path pairs on dump1 response messages
		29 Mar 2007 (sd) : We now are much more patient if repmgr session 
			exists when it is time to send another batch of dump1 reqs.
		20 Oct 2008 (sd) : Corrected issue -- not releasing node/pwth info
			if the number of occurances dropped.
		28 Oct 2008 (sd) : Added catch to prevent coredump if repmgr sends us
			a bogus name. 
 ---------------------------------------------------------------------------------
*/


#define END_OF_DATA 	0xbe		/* repmgr defines this in remgr.h so it MUST match that! */

/* parse a file record from repmgr. expect:
	file <filename>: md5=<string> size=<value> stable=<value>...#node path/file[#node path/file...]
   x=y pairs after filename can be in any order.  We look for just md5 size and stable
   We populate matching file info datastructures with the information that we get. If this 
   is the first time we see a file, then we flag all jobs that reference the file for requeue to move 
   the job to the pending queue if this is the last missing file.
*/
void parse_rm_file( char *buf )
{
	File_t	*f = NULL;
	Syment 	*se = NULL;

	static	char **rec_toks = NULL;	/* record split by hash -- static to keep from thrashing malloc */
	char	*strtok_p = NULL;
	char	*tok;			/* pointer into each token */
	char	*p;			/* pointer into tok */
	
	char	*fname = NULL;		/* data from repmgr output */
	char	*md5 = NULL;
	ng_int64 size = 0;
	int	stable = 0;

	int	nrectoks;		/* number of tokens the record was split into  */
	int	i;
	
	rec_toks = ng_tokenise( buf, '#', '\\', rec_toks, &nrectoks );	 /* tokenise the buffer */
	if( nrectoks > 1 )						/* if there are no instances, dont bother */
	{
		strtok_r( rec_toks[0], " \t", &strtok_p );			/* set up token for parsing (skip file) */
		tok = strtok_r( NULL, " \t", &strtok_p );		/* at the filename */
		if( (p = strchr( tok, ':' )) )				/* has a trailing : */
			*p = 0;						/* lop it off */

		fname = tok;

		while( (tok = strtok_r( NULL, " ", &strtok_p )) != NULL )	/* for each of the others */
		{
			if( (p = strchr( tok, '=' )) )		/* everything after fname is var=val pairs; point at val */
				p++;
			else
			{
				ng_bleat( 0, "bad file record from repmgr: %s", buf );
				return;			
			}

			switch( *tok )
			{
				case 'm':			/* md5/matched value (ignrore matched value) */
					if( *(tok+1) == 'd' )
						md5 = p;
					break;

				case 's':			/* size/stable value */
					if( *(tok+1) == 't' )
						stable = atoi( p ); 		/* should be 1 for true */
					else
						size = atoll( p );		/* convert from ascii */
					break;

				default:			/*  we dont give a flip about the others */
					break;
			}
		}

		if( (se = symlook( symtab, fname, FILE_SPACE, 0, SYM_NOFLAGS )) != NULL ) 	/* some job needs this file */
       		{										/* so capture the location(s) of each instance */
			f = se->value;

			f->size = size;
			f->stable = stable;
			if( ! f->md5 )
				f->md5 = NULL;
#ifdef SOMEDAY_MAYBE
				f->md5 = ng_strdup( "noMD5" );	/* we dont track md5 at this point as it might not be avail at nawab time */
#endif

			if( f->nnodes == 0 )		/* first time we have info on this file */
				req_jobs( f );		/* put referencing jobs onto the requeue list -- remove from missing queue */
			
			for( i = 0; i < f->nidx; i++ )		/* clear out -- assume everything has changed */
			{
				if( f->paths[i] )
					ng_free( f->paths[i] );
				f->paths[i] = NULL;
				f->nodes[i] = NULL;		/* just clear pointer to the node block -- dont release it! */
			}
			f->nidx = 0;				/* start over */

			for( i = 1; i < nrectoks; i++ )		 /* do the rest of the hash separated tokens; node filename pairs */
			{
				tok = strtok_r( rec_toks[i], " \t",  &strtok_p );
				p = strtok_r( NULL, " \t",  &strtok_p );

				if( tok && p )
				{
					if( f->nidx >= f->nnodes )                       /* at max? */
                                	{
						int k;

                                       		f->nnodes += 25;                                /* more please sir */
                                       		f->nodes = ng_realloc( f->nodes, f->nnodes * sizeof( Node_t *), "parse_rm_file:f->nodes" );
                                       		f->paths = ng_realloc( f->paths, f->nnodes * sizeof( char *), "parse_rm_file:f->paths" );
                                       		for( k = f->nidx; k < f->nnodes; k++ )
                                       		{
                                               		f->nodes[k] = NULL;
                                               		f->paths[k] = NULL;
                                       		}
                               		}

					if( *p == '/' )			/* repmgr sends us things like attempt to destroy...; *p must be path */
					{
						if( (f->nodes[f->nidx] = get_node( tok, CREATE )) != NULL )	/* find pointer to node info, create if new */
						{
							f->paths[f->nidx] = ng_strdup( p );
							ng_bleat( 5, "parse_rm_file: %s lives on: %s", f->paths[f->nidx], f->nodes[f->nidx]->name );
							f->nidx++;
						}
						else
							ng_bleat( 0, "parse_rm_file: file info ignored, bad node name in repmgr buffer: %s", tok );
					}
				}
			}

			file_trace( f, "found-in-dump1" );
		}
		else
			ng_bleat( 3, "parse_rm_file: file not referenced by job: %s size=%I*d stable=%d", fname, Ii(size), stable );
	}
}

/* dispatch the buffer to the right function based on the first token 
	we expect the first token to be:
		file (file registration record)
		not (not found)
		handshake (probably just a newline)
*/
void parse_rm_buf( char *buf )
{
	ng_bleat( 4, "repmgr buffer: %s", buf );

	if( strncmp( buf, "file ", 5 ) == 0  )
	{
		parse_rm_file( buf );
	}
	else
		ng_bleat( 3, "ignored record from repmgr (%s)", buf );
}

/* 	open session to repmgr.  if successful, then dump in our list of files.
	repmgr is SINGLE THREADED on its socket and thus we must play the 
	game of closing the sesison after we notice an end of data.  To 
	prevent problems we abort the request attempt if there is already
	a session established (assuming weve sent our request(s) and are 
	still waiting for a reply).  If we try more than twice and find 
	an existing session (default time between tries is 90s) then we 
	assume we might have missed an end of data and we close the session.
	This should free repmgr and we should go again with the next attempt.
	The times_busy var is used to track this. 
*/
void rm_dump_req( )
{
	static char *skip2 = NULL;	/* allow us to request a smaller list and restart where we left off */
	static	int times_busy = 0;	/* see note above */

	File_t	*f = NULL;		/* pointer into list that we need  */
	Syment	*se; 
	char	buf[NG_BUFFER];
	int	max;			/* max number of dump1 requests to generate in one batch */
	int	sid;			/* si interface sid for session to remgr */
	int	got_agent = 0;		/* we are chatting with the d1 agent so we do things differently */
	ng_timetype start = 0;		/* time stats for initial testing and debugging */

	if( !ffq )			/* no needed files; ensure rm session is down and bail */
	{
		drop_rm( );		/* does not hurt if it is already down */
		if( skip2 ) 	
		{
			ng_free( skip2 );
			skip2 = NULL;
		}

		return;
	}
	
	if( (sid = conn2_d1agent( )) >= 0 )		/* try the agent first */
	{
		got_agent = 1;
	}
	else
	{
		if( (sid = conn2_rm( )) < 0  )		/* no agent; we will bang against repmgr directly */
		{
			if( ++times_busy > 100 )		/* assuming 60s intervals, we allow repmgr to take up to 3 minutes to respond */
			{
				ng_bleat( 1, ">100 successive attempts where repmgr was busy; closing the session" );
				drop_rm( );
			}
			else
				ng_bleat( 3, "connection to repmgr existed, delaying request" );
			return;
		}
	}

	times_busy = 0;

	if( skip2 )
	{
		if( (se = symlook( symtab, skip2, FILE_SPACE, 0, SYM_NOFLAGS )) != NULL ) 	/* if file is still in the queue */
			f = (File_t *) se->value;
		else
			ng_bleat( 1, "send_rm_request: skip2 file not found (%s), starting over", skip2 );  /* stats on how often this happens */
		ng_free( skip2 );
		skip2 = NULL;
	}

	if( ! f )
		f = ffq;			/* start at the beginning */

	start = ng_now();
	if( got_agent )
	{
		/*for( max = max_rm_req ; f && max > 0; f = f->next ) */		/* max_rm_req can be set via command (rmax) */
		for( f = ffq; f; f = f->next )			/* when chatting with the agent, send everything */
		{
			if( f->flags & FF_D1REQ )
				ng_siprintf( sid, "dump1 %s\n", f->name );
			else
			{
				ng_siprintf( sid, "Dump1 %s\n", f->name );		/* force a reset to ensure status back */
				f->flags |= FF_D1REQ;
			}
			max--;
		}
	}
	else
	{
		for( max = max_rm_req ; f && max > 0; f = f->next )		/* max_rm_req can be set via command (rmax) */
		{
			ng_siprintf( sid, "dump1 %s\n", f->name );
			max--;
		}
	
		ng_siprintf( sid, "%c", END_OF_DATA );		/* NOTE: repmgr does not want a newline after the end of data character */
	}

	start = ng_now() - start;
	if( f )
	{
		skip2 = strdup( f->name );		/* save our starting point for next time */
		ng_bleat( 2, "rm_dump_req: maxed on send; will skip to %s next round (took %I*dts) %s", skip2, Ii(start), got_agent ? "AGENT" : "" );
	}
	else
		ng_bleat( 2, "rm_dump_req: sent request for all/remaining files (took %I*dts) %s", Ii(start), got_agent ? "AGENT" : "" );
}

