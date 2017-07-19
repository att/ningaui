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
  ---------------------------------------------------------------------------------------------
  Mnemonic:	s_files.c
  Abstract: 	This file contains seneschal routines that manipulate the file blocks
  Date:		14 Feb 2002 (Happy vd!)
  Author: 	E. Scott Danisls

  Mod:		11 Oct 2003 - Added stability check.
		17 Dec 2004 (sd) - We set FSIZE_IGNORE on files that have more than 5 references
			assuming that they are not to be considered in cost calculation.
		14 Sep 2006 (sd) - support for the TCP/IP ineterface to repmgr deprecated 
			the fetch routine here. Commented it out of the compile, but kept 
			for historic reasons. 
		07 Dec 2007 (sd) - We now ignore a file's size only if it comes in from nawab
			as an inputig= file. 
  ---------------------------------------------------------------------------------------------
*/
void file_trace( File_t *f, char *action )
{
	char	buf[NG_BUFFER];
	int 	i;
	int	len = 0;

	if( f->flags & FF_TRACE )
	{
		sfsprintf( buf, sizeof( buf ), "ftrace: %s %s ref=%d size=%I*d nodes=", action, f->name, f->nref, Ii( f->size ) );
		len = strlen( buf );
		for( i = 0; len < NG_BUFFER-2 && i < f->nidx; i++ )
		{
			if( i )
				strcat( buf, "," );
			if( (len += strlen( f->nodes[i]->name )) < NG_BUFFER-2 )
				strcat( buf, f->nodes[i]->name );
		}

		ng_bleat( 0, "%s", buf );
	}
}

/* toggle trace flag in file -- return 1 if we found and toggled */
int file_trace_toggle( char *fname )
{
	File_t *f;

	if( (f = get_file( fname, NULL, NULL )) != NULL )				/* is this file known? */
       	{										/* so capture the location(s) of each instance */
		if( f->flags & FF_TRACE )
		{
			file_trace( f, "tracing-ends" );		/* do before we turn it off */
			f->flags &= ~FF_TRACE;
		}
		else
		{
			f->flags |= FF_TRACE;
			file_trace( f, "tracing-begins" );
		}

		return 1;
	}
	
	return 0;
}

/*
	takes a file block and places it back on the available file queue
	releasing buffers as is needed.
*/
static void dq_file( File_t *f )
{
	int	ni;

	if( f->prev )				/* remove from file list */
		f->prev->next = f->next;
	else
		ffq = f->next;
	if( f->next )
		f->next->prev = f->prev;
	else
		ffq_tail = f->prev;

	file_trace( f, "delete" );

	ng_bleat( 5, "dq_file: deleting: no references: %s", f->name );
	symdel( symtab, f->name, FILE_SPACE );
	ng_free( f->name );
	count_ffq--;

	if( f->nnodes )
	{
		for( ni = 0; ni < f->nnodes; ni++ )	/* must hit all not just to nidx */
		{
			ng_free( f->paths[ni] );
		}
	}

	ng_free( f->nodes );
	ng_free( f->paths );
	ng_free( f->reflist );
	if( f->md5 )				/* we dont keep this yet */
		ng_free( f->md5 );
	ng_free( f->gar );

	ng_free( f );				/* poof */
}



/* 	
	add a reference to job j in file f
	we back reference files to jobs so that when a file's information changes we can easily 
	find the jobs that are affected without having to search the entire set of queues
*/
static void add_file_ref( File_t *f, Job_t *j )
{
	int 	i;
	int	nget = 0;

	if( f )
	{
		file_trace( f, "add-job-ref" );

		for( i = 0; i < f->nref && f->reflist[i]; i++ );	/* find first empty */

		f->ref_count++;

		if( i >= f->nref )			/* no empties found */
		{
			nget = f->nref ? 100 : 10;			/* modest amount the first time; more after that */
			f->nref += nget;				/* new size */
			f->reflist = ng_realloc( f->reflist, sizeof( Job_t * ) * f->nref, "add_file_ref:reflist" ); 
			memset( &f->reflist[i], 0,  sizeof( Job_t *) * nget );
		}

		f->reflist[i] = j;
	}
}




#ifdef NOT_DEPRECATED
---------------------------------------------------------------------------------------------------
we have replaced using the physical dump file that repmgr cuts with s_rmif.c 
funcitons which format dump 1 requests and send them to repmgr and expect 
information via TCP/IP.  This routine kept as historic reference for the next
few units of time. 



/*	reads the repmgr filelist dump from infname. updates file blocks 
	that match.
	Input from rempgr assumed to be a record that has the syntax:
		file <filename> md5=<cksum> size=<size> noccur=<n> token=<n> matched=<n>

	and is followed by zero or more "location" records with one of several syntax patterns:
		<node> <path>/<file>				(we use)
		attempt <other junk tokens that we will ignore> (we ignore)

	because it is taking so long to parse the dump file, we will read only  a few 
	records at a time so we can do real work with out much delay. If this routine 
	returns 1, then it should be recalled again (nearly immediately) to get more.

	we save the state if being recalled so that we can reset the file block we 
	were working when we sent control back. 
*/
static int fetch_finfo( )
{
	static	char *infname = NULL;		/* file name to try to read */
	static 	Sfio_t	*in = NULL;		/* input file des for reading data from repmgr */
	static	long	dcount = 0;		/* number deleted */
	static  long	ccount = 0;		/* number of calls required to accomplish */
	static	char	*restore = NULL;	/* file name in progress with when preempted */

	/*Sfio_t	*p;	*/	/* pipe to repmgr command */ 

	File_t	*f;		/* block that was allocated for a file */
	Syment	*se;		/* block from sym table */
	char	*buf;		/* record from repmgr */
	char	*tok;
	char	*name;		/* pointer at basename */
	char	*nname;		/* node name */
	char	*path; 		/* pointer at the path */
	char	*strtok_p;	/* strtok pointer */	
	int	ni;		/* node index */
	int	k;
	char	fname[NG_BUFFER];	/* current file that we are looking at */
	ng_int64	size;	/* size of the file we are looking at */
	int	count = 50000;	/* max records per pass */

	if( !ffq )
		return 0;		/* we only map files that are on the list, empty list, then save time */

	if( !infname )						/* env call is too expensive to do as often as we are here */
	{
		if( (infname = ng_env( "RM_DUMP" )) == NULL )
			infname = REPMGR_DUMPF;				/* not defined we will make our own request */

		ng_bleat( 1, "fetch_finfo: expecting to find repmgr dump in: %s", infname );
	}

	f = NULL;		/* default  to this -- override with saved info later if a recall */

	if( ! in )		/* file not opened -- not a recall */
	{
		if( (in = ng_sfopen( NULL, infname, "Kr" )) == NULL )
		{
			ng_bleat( 1, "fetch_finfo: unable to open ng_remreq data file: %s: %s", infname, strerror( errno ) );
			return 1;
		}
	
		ng_bleat( 3, "fetch_finfo: opened rmgr dump file (%s); parsing", infname );
		dcount = 0;			/* rest counter with new open */
		ccount = 0;
	}
	else
	{
		if( restore )			/* in the middle of a file block when we preempted -- get it back if still there */
		{
			if( (se = symlook( symtab, restore, FILE_SPACE, 0, SYM_NOFLAGS )) != NULL )	/* if it was not deleted while away */
			{
				f = se->value;
				req_jobs( f );	/* requeue any referencing jobs -- they were likely requeued missing while we were gone */
			}
			free( restore );
			restore = NULL;
		}

		ng_bleat( 4, "fetch_finfo: continuing with repmgr dump file (%s)", infname );
	}
		
	ccount++;
	while( (buf = sfgetr( in, '\n', SF_STRING )) != NULL )
	{
		if( ! *buf )		/*  an immediate end of string; blank line; stop snarfing for this file */
			f = NULL;
		else 
		if( strncmp( buf, "unattach", 8 ) == 0 || strncmp( buf, "nodes:", 6 ) == 0 )
		{
			f = NULL;			/* next section - stop snarfing if we were */
		}
		else
		if( strncmp( buf, "file ", 5 ) == 0 )			/* new file set */
		{
			strtok_r( buf, " :\t", &strtok_p );		/* skip the file label */
			name = strtok_r( NULL, " :\t", &strtok_p );	/* tokenise the basename */
			strcpy( fname, name );


			ng_bleat( 7, "fetch_finfo: found file line for %s", fname);
			if( (se = symlook( symtab, fname, FILE_SPACE, 0, SYM_NOFLAGS )) != NULL )	/* see if this is referenced by a job */
			{
				f = se->value;
				ng_bleat( 5, "fetch_finfo: found symbol %s(fname) %d(ref) %d(nnodes) %d(nidx)", fname, f->ref_count, f->nnodes, f->nidx);
				if( f->ref_count > 0 )			/* job still referencing this file - need to update it */
				{
					f->stable = 0;
					while( (tok = strtok_r( NULL, "\t ", &strtok_p )) && strncmp( "size=", tok, 5 ) );

					if( tok )
					{
						ng_bleat( 4, "fetch_finfo: size tok=%s f->size=%I*d f->ref=%d", tok, sizeof( f->size ), f->size, f->ref_count );
						if( f->size == FSIZE_INVALID || f->nidx == 0 )	/* first time spotted, or never had nodes before */
							req_jobs( f );				/* requeue any referencing jobs that were missing */

#ifdef KEEP
now that we are setting this as a nawab constant we will not automatically set and will always keep track of the file size
						if( f->size == FSIZE_IGNORE || f->ref_count > 5 )	/* if a file is needed by +5 jobs; we dont consider its size in cost calc */
							f->size = FSIZE_IGNORE;
						else
#endif
						sfsscanf( tok+5, "%I*d", sizeof( f->size ), &f->size );		/* convert size to int */

						while( (tok = strtok_r( NULL, "\t ", &strtok_p )) && strncmp( "stable=", tok, 7 ) );
						f->stable = tok ? (*(tok+7) == '0' ? 0 : 1) : 0; 
					}
					else
					{
						ng_bleat( 0, "fetch_finfo: no size found on file record for: %s", f->name );
					}
	
					f->nidx = 0;					/* reset node index, then  */
					if( f->nnodes )					/* clear node pointers because we */
						memset( f->nodes, 0, sizeof( Node_t *) * f->nnodes );	/* assume all have changed loc */
				}
				else				/* file not referenced, just push it out of our queue */
				{
					dcount++;
					dq_file( f );			/* unchain the file and put back on avail queue */	
					f = NULL;			/* dont need it revisited for additional lines */
				}
			}
			else
				f = NULL;			/* not in symtab, assume not referenced by any job */
		}
		else 				/* location record */
		if( f )							/* file was at somepoint referenced by a job */
		{							/* buffer syntax: node <path>/<basenae> */
			nname = strtok_r( buf, " \t", &strtok_p );  
			path = strtok_r( NULL, " \t", &strtok_p );
			ng_bleat( 5, "fetch_finfo: looking at node line, %s(nname) %s(path) %d(nidx) %d(nnodes)", nname, path, f->nidx, f->nnodes);
			if( (name = strrchr( path, '/' )) )
				name++;					/* at basename */
	
			if( nname && strcmp( nname, "attempt" ) != 0 && path && name  )
			{
				if( (ni = f->nidx) >= f->nnodes )			/* get node index */
				{
					f->nnodes += 12;				/* more please sir */
					f->nodes = ng_realloc( f->nodes, f->nnodes * sizeof( Node_t *), "(r)fetch_finfo:f->nodes" );
					f->paths = ng_realloc( f->paths, f->nnodes * sizeof( char *), "(r)fetch_finof:f->paths" );
					for( k = ni; k < f->nnodes; k++ )
					{
						f->nodes[k] = NULL;
						f->paths[k] = NULL;
					}
				}


				ng_free( f->paths[ni] );			/* release if we had one before */
				f->paths[ni] = ng_strdup( path );				/* hold the full name */
				f->nodes[ni] =  get_node( nname, CREATE );		/* something to point at */
				ng_bleat( 5, "fetch_finfo: %s lives on: %s (%d)", f->paths[ni], f->nodes[ni]->name, ni );

				f->nidx++;
			}			/* end if job was referenced */
		}			/* end else snarf was on */
		/* else ng_bleat( 8, "f is NULL");*/

		if( --count <= 0 )
		{
			if( f )				/* save the name to find this block when recalled */
				restore = strdup( f->name );
			return 1;			/* recall immediately */
		}
	}

	restore = NULL;				/* should be be -- but ensure */
	ng_bleat( 3, "fetch_finfo: %ld blocks deleted, %ld calls", dcount, ccount );
	sfclose( in );
	in = NULL;
	gflags |= GF_ALLOW_GAR;		/* gar checking can be done with new info */

	return 0;
}
#endif
