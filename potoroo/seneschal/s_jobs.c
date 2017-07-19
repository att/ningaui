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
 ------------------------------------------------------------------------------------------------------
    Mnemonic: 	s_jobs.c
    Abstract: 	This file contains routines that work on the job blocks
    		Included by seneschal.c
    Author: 	E. Scott Daniels
    Mods:	19 Mar 2002 (sd) - Corrected job cost calculation (size is input size)
		13 Jul 2002 (sd) - to properly circle on the type cost buffer when it hits max
		17 Jan 2004 (sd) - fixed the woomera queue time naming such that things are saved 
			correctly in the type list.
		25 Oct 2004 (sd) - Added call to initialise resource:node counts based on 
			currently running jobs when a resource limit of Xn is specified.
		10 Dec 2004 (sd) - mpn set checks specifically for -1 now and not < 0
		14 Dec 2004 (sd) - flipped cost units
		17 Dec 2004 (sd) - We now do not record job costs for really short duration jobs.
		03 Nov 2005 (sd) - extend lease was popping resources/mpn info by dqing job. We now 
			set a different state for the job which dq_job now removes the job from the 
			running queue and leaves resource information alone.  Requeing the job resets
			the state to RUNNING.
		04 Jan 2006 (sd) - support to allow job to run with less than all input files. 
		30 mar 2006 (sd) - changed comments only.
 ------------------------------------------------------------------------------------------------------
*/


/* create a new job management block */
static Job_t *new_job( char *name )
{
	Job_t *j;

	j = ng_malloc( sizeof( Job_t ), "new job" );
	memset( j, 0, sizeof( Job_t ) );

	j->name = ng_strdup( name );
	j->attempt = -1;		/* bumped at schedule time before used */
	j->state = NOT_QUEUED;
	j->delay = 900;			/* 1/10s of seconds to base restart delay on (mult by attempt number+1) */
	j->username = ng_strdup( "nobody" );

	ng_bleat(6, "new job(%s): noutf=%d", j->name, j->noutf);

	return j;
}

/* dereference resources pointed to by the job */
static void deref_rsrcs( Job_t *j )
{
	int i;

	for( i = 0; i < j->nrsrc; i++ )
		if( j->rsrc[i] )
		{
			if( --j->rsrc[i]->ref_count <= 0 )	/* deref resources retarget if it hits zero */
				set_targets( );
			j->rsrc[i] = NULL;		/* just in case */
		}
}

/* dereference files pointed to by job */
static void deref_files( Job_t *j )
{
	File_t	*f;			/* points into referenced file list to delete pointers back to us */
	int 	k;
	int	i;

	for( i = 0; i < j->ninf; i++ )		/* deref each file we are pointed at */
	{
		if( (f = j->inf[i]) )
		{
			for( k = 0; k < f->nref; k++ )
				if( f->reflist[k] == j )
					f->reflist[k] = NULL;

			j->inf[i] = NULL;		/* just in case */
			if( (--f->ref_count) < 1 )	/* no more references - take it off now */
				dq_file( f );
			
		}
	}
}

/*	cleans up a job block -- we do this only if we get a duplicate job and the job is on the 
	pending or missing queues.  This allows some things to be overlaid with new information.
*/
static void wash_job( Job_t *j )
{
	int i;

	j->node = NULL;			/* reset as it might be (re)assigned this time */

	deref_files( j );		/* dereference all files  (zeros j->inf pointers) */
	deref_rsrcs( j );		/* dereference all resources */

	memset( j->flist, 0, sizeof( j->flist[0] ) * j->ninf );	/* must zero if not freeing */
	j->iidx = 0;						/* index into flist is now reset too */

	ng_free( j->cmd );
	ng_free( j->ocmd );
	j->cmd = NULL;
	j->ocmd = NULL;

	for( i = 0; i < j->noutf; i++ )		/* we create these so we must free them */
	{
		ng_free( j->outf[i] );
		ng_free( j->olist[i] );
		ng_free( j->rep[i] );

		j->outf[i] = NULL;		
		j->olist[i] = NULL;
		j->rep[i] = NULL;		/* default number of reps */
	}

	j->oidx = 0;			/* outfile stuff based on this */
	ng_free( j->sbuf );		/* should not be there, but be parinoid */
	j->sbuf = NULL;
	ng_free( j->nattr );		/* node attributes list */
	j->nattr = NULL;

}

/* completely toss the job block out the window */
static void trash_job( Job_t *j )
{
	int i;

	/* do NOT free node - pointer at node block */

					/* dereference -- free arrays later */
	deref_files( j );		/* dereference files, zeros j->inf pointers */
	deref_rsrcs( j );		/* dereference all resources */


	ng_free( j->name );
	if( j->wname )
		ng_free( j->wname);		/* woomera job name */
	ng_free( j->nattr ); 
	ng_free( j->flist );		/* these point at things in file blocks, so we just free the array */
	ng_free( j->inf );		/* deref does not free */
	ng_free( j->rsrc );		/* deref just un points; we need to free array */
	ng_free( j->username );

	for( i = 0; i < j->noutf; i++ )
	{
		ng_free( j->outf[i] );
		ng_free( j->olist[i] );
	}


	ng_free( j->cmd );
	ng_free( j->ocmd );
	ng_free( j->outf );
	ng_free( j->olist );
	ng_free( j->rep );
	ng_free( j->sbuf );

	ng_free( j );
}

static void old_trash_job( Job_t *j )
{
	int i;

	/* do NOT free node - pointer at node block */

	deref_files( j );		/* dereference files, zeros j->inf pointers */
	deref_rsrcs( j );		/* dereference all resources */

	memset( j->flist, 0, sizeof( j->flist[0] ) * j->ninf );		/* must zero if not freeing */
	for( i = 0; i < j->noutf; i++ )
	{
		ng_free( j->outf[i] );
		ng_free( j->olist[i] );
	}

	ng_free( j->flist);		/* these two go away if using memory hunks */
	ng_free( j->inf );
	ng_free( j->rsrc );
	j->inf = NULL;			/* parinoid */
	j->flist = NULL;
	j->rsrc = NULL;

	ng_free( j->name );
	ng_free( j->wname );
	ng_free( j->cmd );
	ng_free( j->ocmd );
	ng_free( j->sbuf );
	ng_free( j->nattr );		/* required attributes string */

	ng_free( j );
}

/* 	run all jobs that reference file f and requeue them on the resort queue 
*/
static void req_jobs( File_t *f )
{
	Job_t	*j;
	int	i;

	for( i = 0; i < f->nref; i++ )
	{
		if( (j = f->reflist[i]) != NULL && j->state == MISSING )
		{
			dq_job( j );
			q_job( j, RESORT );
		}
	}
}

/* remove a job from the queue that it lives on */
/* returns name of queue removed from */
char *dq_job( Job_t *j )
{
	char *qname = "";

	if( !j  )
		return NULL;					/* paranoia keeps coredumps away */

	exd_adjust( j );		/* adjust any explain/dump pointers that may reference this job */

	switch( j->state )		/* take off the proper queue */
	{
		case NOT_QUEUED:
			break;

		case MISSING:	
			qname = "missing";
			count_mq--;

			if( j->prev )				/* remove */
				j->prev->next = j->next;
			else
				if( mq == j )		/* prevent queue corruption if not really on pending */
					mq = j->next;
			if( j->next )
				j->next->prev = j->prev;
			else
				if( mq_tail == j )		/* be as paranoid as with the head */
					mq_tail = j->prev;
			break;

		case RESORT:
			qname = "missing";
			count_rsq--;

			if( j->prev )
				j->prev->next = j->next;
			else
				if( rsq == j )		
					rsq = j->next;
			if( j->next )
				j->next->prev = j->prev;
			else
				if( rsq == j )				/* be as paranoid as with the head */
					rsq_tail = j->prev;
			break;

		case RUNNING:	
			count_rq--;
			qname = "running";
			if( j->node )
			{
				j->node->rcount--;			/* runing no more */
				if( ! j->prime )				/*  not a housekeeper */
					mpn_adjust_all( j, j->node, -1 );		/* one less running against nodes mpn count */
			}

			adjust_resources( j, -1 );		/* resource not being consumed by job any more */

			if( j->prev )				/* remove from running queue */
				j->prev->next = j->next;
			else
				if( rq == j )
					rq = j->next;
			if( j->next )
				j->next->prev = j->prev;
			else
				if( rq_tail == j )		/* be parinoid */
					rq_tail = j->prev;
			break;

		case RUNNING_ADJ:			/* job will be requeued on running, so keep resources etc */
			count_rq--;
			qname = "running";

			if( j->prev )				/* remove from running queue */
				j->prev->next = j->next;
			else
				if( rq == j )
					rq = j->next;
			if( j->next )
				j->next->prev = j->prev;
			else
				if( rq_tail == j )		/* be parinoid */
					rq_tail = j->prev;
			break;

		case PENDING:
			qname = "pending";
			count_pq--;

			if( j->prev )			/* remove from pending queue */
				j->prev->next = j->next;
			else
				if( pq == j )		/* prevent queue corruption if not really on pending */
					pq = j->next;
			if( j->next )
				j->next->prev = j->prev;
			else
				if( pq_tail == j )		/* be as paranoid as with the head */
					pq_tail = j->prev;
			break;
			
		default:					/* all other types assumed to be completed */
			qname = "completed";
			count_cq--;
			if( j->prev )			
				j->prev->next = j->next;
			else
				if( cq == j )
					cq = j->next;

			if( j->next )
				j->next->prev = j->prev;
			else
				if( cq_tail == j )
					cq_tail = j->prev;
				
			break;
		}

	j->state = NOT_QUEUED;
	j->prev = j->next = NULL;
	return qname;
}

/* 	add a job to a job queue 

	resort: add to head 
	missing: add to head
	pending: add in priority then cost order 
	running: add in expiration order 
	complete: add to tail (expiration order)
*/
void q_job( Job_t *j, int qid )
{
	Job_t *ip;		/* insertion pointer for running queue */
	char *qname = "complete";

	switch( qid )
	{
		case MISSING:
			qname = "missing";		/* for audit message */
			count_mq++;			/* insert in priority then size order (largest jobs first) */
			j->state = MISSING;	
			j->next = NULL;

			ip = NULL;
			j->prev = ip;
			j->next = mq;			/* plop on the head */
			mq = j;
			
			if( j->next )			/* if not last, chain back; else mark us as end */
				j->next->prev = j;
			else
				mq_tail = j;
			break;
		
		case RESORT:
			count_rsq++;
			qname = NULL;			/* prevent an audit message for this move */
			j->state = RESORT;
			j->next = rsq;
			if( j->next == NULL )
				rsq_tail = j;
			else
				j->next->prev = j;
			rsq = j;
			j->prev = NULL;
			break;

		case RUNNING:
			count_rq++;
			qname = "running";		/* for audit message */
			j->state = RUNNING;
			adjust_resources( j, +1 );		/* mark resource(s) as being consumed by "value" */
			j->node->rcount++;			/* number of jobs we consider to be running on the node */

			for( ip = rq_tail; ip && ip->lease > j->lease; ip = ip->prev );

			j->prev = ip; 
			if( ip )			/* insert after */
			{
				j->next = ip->next;
				j->prev->next = j;
			}
			else				/* insert at head of list */
			{
				j->next = rq;
				rq = j;
			}
			if( j->next )			/* if not last, chain back; else mark us as end */
				j->next->prev = j;
			else
				rq_tail = j;
			break;

		case RUNNING_ADJ:			/* a running job that is being adjusted on the queue -- no resource manipulation */
			count_rq++;
			qname = "running";		/* for audit message */
			j->state = RUNNING;

			for( ip = rq_tail; ip && ip->lease > j->lease; ip = ip->prev );

			j->prev = ip; 
			if( ip )			/* insert after */
			{
				j->next = ip->next;
				j->prev->next = j;
			}
			else				/* insert at head of list */
			{
				j->next = rq;
				rq = j;
			}
			if( j->next )			/* if not last, chain back; else mark us as end */
				j->next->prev = j;
			else
				rq_tail = j;
			break;

		case PENDING:
			count_pq++;
			qname = "pending";		/* for audit message */
			j->state = PENDING;	
			j->next = NULL;

		/* this is linear, and it sucks, but while I work all other issues it will do */
			for( ip = pq_tail; ip && ip->priority < j->priority; ip = ip->prev );		 /* find priorty match */
			if( gflags & GF_QSIZE )					/* queue based on size too */
				for( ; ip && ip->priority == j->priority && ip->size < j->size; ip = ip->prev ); /* we assume jobs with large size are most costly */
			j->prev = ip;

			if( ip )			/* not inserting at the head */
			{
				j->next = ip->next;
				j->prev->next = j;
			}
			else
			{
				j->next = pq;
				pq = j;
			}
			if( j->next )			/* if not last, chain back; else mark us as end */
				j->next->prev = j;
			else
				pq_tail = j;
			break;
	
		default:			/* several types go on the complete queue */
			count_cq++;
			j->state = qid;
			j->next = NULL;			/* add to complete queue */
			j->prev = cq_tail;
			if( cq_tail )
				cq_tail->next = j;
			else
				cq = j;
			cq_tail = j;
			break;
	}
}


/* add a list of output files to a job ) */
/* files are blank/tab separated list. The filename may be followed by a 
   comma and replication factor (string). rep factor is passed to s_mvfiles
   and is not parsed/understood by seneschal.
*/
static void add_outfiles( Job_t *j, char *list )
{
	char	*strtok_p;		/* pointer for strtok on main buffer */
	char	*tok = NULL;		/* token to deal with */
	char	*rep = NULL;		/* pointer to replication directive if there */
	int	i;
	
	tok = strtok_r( list, " \t", &strtok_p );
	if( !tok || *tok == '*' )
	{
		if( j->noutf )		/* clean up what might have been there */	
		{
			for( i = 0; i < j->noutf; i++ )
			{
				ng_free( j->outf[i] );
				ng_free( j->olist[i] );
				ng_free( j->rep[i] );
				j->olist[i] = NULL;
				j->rep[i] = NULL;		/* reset to default */
			}

			j->oidx = 0;

			deref_files( j );
		}

		return;			/* no output files to deal with */
	}

	while( tok )
	{
		if( j->oidx >= j->noutf )
		{
			j->noutf += 20;
			j->outf = (char **) ng_realloc( j->outf, sizeof( char *) * j->noutf, "(r)s_jobs:outfile buffer" );
			j->olist = (char **) ng_realloc( j->olist, sizeof( char *) * j->noutf, "(r)s_jobs:outlist buffer" );
			j->rep = (char **) ng_realloc( j->rep, sizeof( char *) * j->noutf, "(r)s_jobs:rep buffer" );
			for( i = j->oidx; i < j->noutf; i++ )
			{
				j->outf[i] = NULL;
				j->olist[i] = NULL;
				j->rep[i] = NULL;
			}
		}

		if( (rep = strchr( tok, ',' )) )			/* get replication parm if there */
		{
			*rep = 0;					/* terminate the file name for strdup */
			j->rep[j->oidx] = ng_strdup( rep+1 );			/* save the replication factor string */
		}
		else
			j->rep[j->oidx] = NULL;				/* nothing supplied, set to default */

		j->outf[j->oidx] = ng_strdup( tok );
		j->oidx++;

		tok = strtok_r( NULL, " ", &strtok_p );
	}
}

/*	add space to the input file array 
	caller may know an exact amount needed, if not (amt == 0) then we add a modest extension 
*/
static void incr_inf( Job_t *j, int idx, int amt )
{
	int i;

	if( amt )
		j->ninf += amt + 20;		/* caller may know that they need a huge number */
	else
		j->ninf += 20;			/* unknown number coming down, just alloc a few */	

	j->inf = (File_t **) ng_realloc( j->inf, sizeof( File_t *) * j->ninf, "(r)s_jobs:infile buffer" );
	j->flist = (char **) ng_realloc( j->flist, sizeof( char *) * j->ninf, "(r)s_jobs:flist buffer" );
	for( i = idx; i < j->ninf; i++ )
	{
		j->inf[i] = NULL;
		j->flist[i] = NULL;
	}
}

/*	add a list of input files to a job - list contains space separated md5,filename[,garname] tripples 
	if filename contains {n-m} then a series of files is created with numeric iteration n through m.
	(e.g. ds-dbname-{0-2317}-714.rpt causes 2318 file references to the job to be created.)
	keeps the flist array built to the proper size too 
	adds the job to the files reflist 
*/
static void add_infiles( Job_t *j, char *list, int ignore_fsize )
{
	char	*strtok_p;		/* pointer for strtok */
	char	*tok = NULL;		/* token to deal with */
	char	*md5;			/* pointer to md 5 */
	char	*file;			/* pointer to file name in pair */
	char	*gar;			/* pointer at the gar name if supplied */
	int	i;
	char	*p1;			/* series of pointers into filename to suss out {n-m} info */
	char	*p2;	
	char	*p3 = NULL;	
	int	start;			/* iteration range */
	int	stop;	
	char	nfname[2048];		/* new filename built from range */
	char	fmt[1024];		/* format string */

	
	tok = strtok_r( list, " \t", &strtok_p );
	if( !tok || *tok == '*' )
	{
		if( ! j->ninf )	
			incr_inf( j, 0, 0 );			/* add space in inf arrays */

		j->inf[0] = NULL;
		j->flist[0] = NULL;
		j->iidx = 0;			/* insertion point is here if they add jobs later */
		return;				/* no input files */
	}

	while( tok )
	{
		md5 = tok;
		if( (file = strchr( tok, ',' )) == NULL )
		{
			ng_bleat( 0, "bad md5,filename[,gar-name] tripple (no comma?) near %s", tok );
			return;
		}

		*file = 0;
		file++;
		if( gar = strchr( file, ',' ) )
		{
			*gar = 0;			/* terminate file name */
			gar++;				/* at the gar name */
		}
		
		if( 	(p1 = strchr( file, '{' )) != NULL &&		/* build an iterative list if {n-m} */
			(p2 = strchr( p1+1, '-' )) != NULL &&
			(p3 = strchr( p2+1, '}' )) != NULL )		/* found all three */
		{
			start = atoi( p1+1 );
			stop = atoi( p2+1 ) + 1;

			*p1 = 0;
			strcpy( fmt, file );			/* build format string for sprintf */
			strcat( fmt, "%04d" );
			strcat( fmt, p3+1 );
			
			if( j->iidx + (stop - start) > j->ninf )			/* will we need more room? */
				incr_inf( j, j->iidx, stop - start );

			ng_bleat( 2, "add_infiles: creating %d files from iteration: %s", stop-start, fmt );
			for( i = start; i < stop; i++ )
			{
				sfsprintf( nfname, sizeof( nfname ), fmt, i );
				j->inf[j->iidx] = get_file( nfname, "noMD5", gar );		/* create the file block if not there */
				if( ignore_fsize )
					j->inf[j->iidx]->flags |= FF_IGNORESZ;
				add_file_ref( j->inf[j->iidx], j );				/* point the file back at the job */
				j->iidx++;
			}	
		}
		else
		{
			if( j->iidx >= j->ninf )	
				incr_inf( j, j->iidx, 0 );			/* add space in inf arrays */

			j->inf[j->iidx] = get_file( file, md5, gar );		/* find or make a file block */
			if( ignore_fsize )
				j->inf[j->iidx]->flags |= FF_IGNORESZ;
			add_file_ref( j->inf[j->iidx], j );				/* add this job to the file's reflist, ups ref_counter */
			j->iidx++;
		}


		tok = strtok_r( NULL, " ", &strtok_p );
	}
}

/*	add a list of resources to the job j 
	list is separated by any combination of blanks, commas, or tabs.
*/
void add_resources( Job_t *j, char *list )
{
	char	*strtok_p;		/* pointer for strtok */
	char	*tok = NULL;		/* token to deal with */
	char	*set = NULL;		/* points to default setting (=2h) if supplied */
	int	i = 0;
	
	tok = strtok_r( list, " \t", &strtok_p );	/* skip leading whitespace */
	if( !tok || *tok == '*' )
	{
		if( ! j->rsrc )			/* nothing allocated */
		{
			j->nrsrc += 20;
			j->rsrc = ng_realloc(j->rsrc, sizeof(Resource_t *) * j->nrsrc, "s_jobs:resource list(1)");
		}

		j->rsrc[0] = NULL;		/* ensure that its nothing */
		return;
	}

	while( tok )
	{
		for( tok; *tok && !isalnum( *tok ); tok++ );			/* skip whitespace */
		if( *tok )
		{
			if( i >= j->nrsrc )
			{
				j->nrsrc += 20;
				if(j->rsrc)
					j->rsrc = ng_realloc(j->rsrc, sizeof(Resource_t *) * j->nrsrc, "(r)s_jobs:resource list");
				else
					j->rsrc = ng_malloc(sizeof(Resource_t *) * j->nrsrc, "s_jobs:resource list");
			}
	
			if( set = strchr( tok, '=' ) )
				*(set++) = 0;
	
			j->rsrc[i] = get_resource( tok, CREATE );		/* find or make resource */
			j->rsrc[i]->ref_count++;			/* up the references */
			j->rsrc[i]->flags &= ~RF_IGNORE;		/* ensure ignore flag is off */
	
			if( set && *set ) /*&& j->rsrc[i]->limit == -1 )*/			/* dont barf if yada=  is given rather than yada=2h */
			{
				if( strchr( set, 'n' ) )				/*  a max per node setting rather than a limit */
				{
					if( j->rsrc[i]->mpn == -1 )			/* set only if not previously set */
					{
						j->rsrc[i]->mpn = atoi( set );
						mpn_init_counts( j->rsrc[i] );		/* set resource:node counts based on what is running now */
					}
				}
				else
				{							/* could be hard or soft limit */
					if( j->rsrc[i]->limit == -1 )
					{
						j->rsrc[i]->limit = atoi( set );		/* set only if the value was unlimited */
						if( strchr( set, 'h' ) )
							j->rsrc[i]->flags |= RF_HARD;
							else
							j->rsrc[i]->flags &= ~RF_HARD;		/* turn off hard limit */
					}
				}
			}

			i++;
		}

		tok = strtok_r( NULL, " ,\t", &strtok_p );		/* space or comma separated list */
	}

	for( ; i < j->nrsrc; i++ )
		j->rsrc[i] = NULL;			/* zero unused */
}

/*
	obstruct a job -- set its no-run flag to prevent from running even if bids and
	resources are available. When it is released, state is set to 0, and we clear the 
	restart_delay too.
*/
static int obstruct_job( char *name, int state )
{
	Job_t	*j;
	char	*qname;				/* for verbose */

	if( j = get_job( name ) )
	{
		ng_bleat( 2, "obstruct_job: job now %s: %s", state ? "blocked" : "released", name);
		if( state )
			j->flags |= JF_OBSTRUCT;
		else
		{
			j->flags &= ~JF_OBSTRUCT;	/* off the flag */
			j->run_delay = 0;
		}
	}
	else
	{
		ng_bleat( 0, "obstruct_job: %s attempt failed: %s", state ? "block" : "release",  name );
		return 0;
	}

	return 1;
}

/* 
	pop a job from the queue and destroy it (remembering to remove it from the symbol table)
	if a job is a housekeeper, then we cause the lease to expire (if running) and set max attempts
	down so that it fails the primary job.

	mod 12 Jan 2004 -- allow multiple names from sreq command line
*/
static void del_job( char *list )
{
	Job_t	*j;
	char	*qname;				/* for verbose */
	char 	*name;				/* allow for name:name... in list */
	char	*strtok_p = NULL;

	name = strtok_r( list, " :,", &strtok_p );
	
	while( name )
	{
		if( j = get_job( name ) )
		{
			if( j->state != DETERGE )
			{
				qname = dq_job( j );		/* pull it off */
	
				if( j->prime )			/* housekeeper if job references a primary job */
				{
					ng_bleat( 1, "del_job: %s is a housekeeper, causing it to fail instead of straight purge", j->name );
					j->attempt = j->max_att + 1;
					j->lease = 0;
					q_job( j, RUNNING );		/* put back onto running queue - lease check will pop it off */
				}
				else
				{
					ng_bleat( 2, "del_job: %s deleted; was on %s queue", j->name, qname );
					symdel( symtab, j->name, JOB_SPACE );
					trash_job( j );				/* cleans up stuff inside */
				}
			}
			else
				ng_bleat( 1, "del_job: %s NOT purged, has housekeeping job running", name );
		}
		else
			ng_bleat( 1, "del_job: %s NOT purged, not found in table", name );

		name = strtok_r( NULL, " :,", &strtok_p );
	}
}

/* set the pathlist in j based on node */
static void mk_filelst( Job_t *j, Node_t *n )
{
	int	i;
	char	*fname;		/* full file name on node n */
	
	for( i = 0; i < j->ninf; i++ )
	{
		if( j->inf[i] )
			j->flist[i] = get_path( j->inf[i], n, NULL );
	}
}

/* makes the list of output file names with attempt number in them */
/* two types of file name are expected:
	goobername and goobername.ext
   convert to:
	goobername-n and goobername-n.ext
*/
static void mk_ofilelst( Job_t *j, int attempt )
{
	int	i;
	char	*fname;		/* expanded file name */
	char	*ip;		/* insertion point */
	char	att[20];	/* attempt number string to put into the file name */
	char	wrk[1024];

	for( i = 0; i < j->noutf; i++ )
		if( j->outf[i] )
		{
			if( (ip = strrchr( j->outf[i], '.' )) != NULL)			/* insert just before last dot */
			{
				sfsprintf( att, sizeof( att ), "+%d", attempt );
				memcpy( wrk, j->outf[i], ip - j->outf[i] );
				wrk[ip - j->outf[i]] = 0;
				strcat( wrk, att );
				strcat( wrk, ip );
			}
			else								/* or at end if no dot */
				sfsprintf( wrk, sizeof( wrk ), "%s+%d", j->outf[i], attempt );

			j->olist[i] = ng_strdup( wrk );
		}
		else
			j->olist[i] = NULL;
}

/*	change to acept a time value to base the cost on.  the queue time cost for the 
	node can be calculated as the difference between exec-time (passed in) and the 
	started-now vlaue.
*/
/*	record the cost of a jobtype to use in estimating cost/unit in the future 
	if time is < 0, then cost is based on the time that we sent the job off 
	to woomera.  Time is assumed to be tenths of seconds that the job took 
	to run. in addition to recording the cost, we calculate the mean cost with 
	the inserted value.

	We also keep the cost of waiting in a woomera queue on a node.  The 'jobname'
	that is used to track queue costs is wqtime-jobname.  It cannot be 
	wqtime_jobname or wqtime.jobname because the get_type function uses the job name 
	only to the first _ or . character.
*/
static void insert_type_value( Jobtype_t *t, double value )
{
	if( ! t )
		return;					/* prevent accidents */

	if( t->flags & TF_WRAPPED )
		t->total -= t->cost[t->nextc];		/* knock off the value about to be overlaid */
	t->total += value;				/* new sum of values in the cost list */
	t->cost[t->nextc] = value;			
	if( ++t->nextc >= MAX_COST_DATA )
	{
		t->nextc = 0;			/* circular buffer */
		t->flags |= TF_WRAPPED;			/* lets us know the right divisor */
	}

	t->mean = t->total/(t->flags & TF_WRAPPED ? MAX_COST_DATA : t->nextc);	/* calc the new mean value */
}
static void record_cost( Job_t *j, int time )
{
	Jobtype_t	*t;
	ng_timetype	now;
	double	cost;			/* cost to execute this job */
	char	*nname;			/* node name */
	int 	qtime = 0;		/* time spent in woomera queues */
	char	buf[1024];

	if( time < 20 )			/* ignore jobs that executed in less than 2 seconds */
		return;

	nname = j->node ? j->node->name : NULL;

	now = ng_now( );
	if( j->size < 1 )
		j->size = 1;

	if( time > 0 )
	{
		/*cost = ((double) (time))/(j->size);*/ 		/* cvt time into time/size-units */
		cost = ((double) j->size)/time; 		/* cvt time into size/time units */
		qtime = (now - j->started) - time;		/* time spent in woomera queue on node */
	}
	else
	{
		/*cost = ((double) (now - j->started))/(j->size);  */
		cost = ((double) j->size)/(now - j->started);
		qtime = now - j->started;				/* cannot calc actual, save something */
	}

	insert_type_value( get_type( j->name, nname, CREATE ), cost );	/* save cost for type on the node (if node known) */

	sfsprintf( buf, sizeof( buf ), "wqtime-%s", j->name );		/* save queue time for job type on the node (if available) */
	insert_type_value( get_type( buf, NULL, CREATE ), qtime+50 );

	if( nname )				/* if we used nname above, save a generic costs for the job type (not node based) */
	{
		insert_type_value( get_type( j->name, NULL, CREATE ), cost );
		sfsprintf( buf, sizeof( buf ), "wqtime-%s", j->name ); 			/* if wqtime-jobname changes, get_type must change */
		insert_type_value( get_type( buf, NULL, CREATE ), qtime+50 );
	}
}

/*	put a job on hold, create and queue an expounge job (EJ) to do cleanup 
	work (attempt file renaming mostly). For these houekeepers, 
   	the actual command string is built for the job when it is scheduled 
*/
static void hold_job( Job_t *j, int attempt, char *node, char *sbuf )
{
	Job_t	*ej;			/* pointer to the new job */
	char	wrk[NG_BUFFER];		/* work buffer */
	int	i;	

	j->sbuf = ng_strdup( sbuf );		/* copy the status info that will be sent to nawab */

	sfsprintf( wrk, sizeof( wrk ), "EJ_%s", j->name );
	ej = new_job( wrk );
	symlook( symtab, ej->name, JOB_SPACE, ej, SYM_NOFLAGS );	

	ej->prime = j;				/* point to the primary job */
	ej->size = 1;			/* size of job has no bearing on how long this takes; this keeps us from timing them out for smaller jobs */
	ej->max_att = 5;
	ej->priority = 500;			/* should not matter much, but just in case */
	ej->flags |= JF_HOUSEKEEPER;		/* these are scheduled immediatly  with a fixed lease time */
	ej->node = get_node( node, CREATE );	/* must run on node that the primary finished on */
	ej->username = strdup( j->username );

	ej->node->bid++;			/* we dont really use a bid, but need to give run-jobs something to do */
	
	if( attempt != j->attempt )		/* previous attempt completed, must rebuild file list */
	{
		for( i = 0; i < j->noutf; i++ )
			ng_free( j->olist[i] );		/* trash it as we need to buid new */

		mk_ofilelst( j, attempt );		/* create outfile list with attempt number */
	}

	j->attempt = attempt;			/* make them match again */
	j->node = ej->node;
	
	q_job( j, DETERGE );			/* put the primary job onto the complete  queue w/funky state */
	q_job( ej, PENDING );			/* put expounger on the pending queue */

	ng_bleat( 2, "hold_job: queued expounge job: %s %s(target)", ej->name, ej->node->name );
}

/* called when an expounge job (ej) has completed successfully.  send status back and 
   finish cleanup of things 
   we expect ej to have been pulled from the queue 
*/
static void release_job( Job_t *ej )
{
	Job_t	*j;			/* primary job */
	char	buf[1024];

	deref_rsrcs( ej );

	j = ej->prime;

	deref_files( j );			/* final cleanup of the original job */

	if( j->hexit )				/* housekeeping failed - fail the job */
	{
		j->state = FAILED;		/* job marked as failure */
		sfsprintf( buf, sizeof( buf ), "%s:%s:%d\n", j->name, j->node->name, j->hexit );
		send_stats( j->response, buf ); 
		if( j->sbuf )
			ng_free( j->sbuf );	/* replace with failure status from housekeeping */
		j->sbuf = ng_strdup( buf );
	}
	else				/* housekeeping was ok */
	{
		j->state = COMPLETE;		/* change from deterge to its final state */
		send_stats( j->response, j->sbuf ); 
	}

	ng_bleat( 3, "release_job: job released from hold: %s %d(attempt) %s(node) %d(status)", 
			j->name, j->attempt, j->node->name, j->exit );

	symdel( symtab, ej->name, JOB_SPACE );
	trash_job( ej );		/* not needed */
}

/* 	runs the resort queue to redisposition jobs that have landed there 
	jobs are moved to the pending queue if:
		they are housekeeper or gar jobs or
		they do not have any input files or
		all input files are available (there still may be deadlocks if files not all on same node)
	otherwise jobs are moved to the missing queue (need one or more files) 
*/
static void *resort_jobs( )
{
	Job_t	*j;			/* pointer at job we are dealing with */
	Job_t	*nj;			/* next to look at so we can delete from queue */
	Node_t	*n = NULL;		/* selected node */
	File_t	*f;			/* pointer at file to get size */
	ng_int64	size = 0;	/* filesize sum */
	ng_int64	ignored = 0;	/* sum of filesizes that were ignored */
	ng_int64	fsize;		/* size of the file we are looking at  */
	int	i;
	int	missing;			
	int	todo = max_resort;	/* max to do at a time so we come up for air and process requests */
	int	still_need;		/* number of files we still need to find if we are allowing a job to run with less than all inputs */
	int	minc; 			/* missing counter increment */

	for( j = rsq; j && todo; j = nj )
	{
		todo--;
		nj = j->next;			/* must grab next before we dq it */
		dq_job( j );

		if( j->ninf )				/* job has input files */
		{
			missing = 0;
			size = 0;
			ignored = 0;
			if( j->flags & JF_VARFILES )		/* job is allowed to run if some files are missing */
			{
				minc = 0;			/* we do not inc missing counter -- no short circuit */
				still_need = j->ninf_need;	/* number we must find to move the file to pending */
			}
			else
			{
				minc = 1;
				still_need = 0;
			}

			for( i = 0; !missing && i < j->ninf; i++ )
			{
				if( (f = j->inf[i]) )	
				{
#ifdef KEEP
we can use f->size now that we do not set size to -2 (IGNORE) 
					fsize = f->size == FSIZE_IGNORE ? 0 : f->size;		/* if ignorning size; set to zero */
					if( fsize < 0 || f->nidx == 0 )		/* we allow empty files (0 size), but insist on at least one node */	
#endif
					if( f->size < 0 || f->nidx == 0 )	/* we allow empty files (0 size), but insist on at least one node */	
					{
						missing += minc;		/* short circuit if must have all files */
						ng_bleat( 4, "resort_job: missing file: %d(hassize) %d(nodes) %s(name)", f->size ? 1 : 0, f->nidx, f->name );
					}
					else
					{
						if( !(f->flags & FF_IGNORESZ) )
							size += f->size;
						else	
							ignored += f->size;
						still_need--;
					}
				}
			}
			
			if( missing || still_need > 0 )	/* one or more files not available or did not find enough when allowing subset of input */
			{
				ng_bleat( 3, "resort_job: queued missing: %s (%d/%d)", j->name, missing, still_need );
				q_job( j, MISSING );
				j = NULL;				/* dont attempt to schedule it elsewhere */
			}
			else
			{
				j->size = (size/1024)+1;		/* track size in k  all jobs have a minimal size of 1 */
				j->isize = (ignored/1024);		/* track the amount ignored so we can spit it to the master log */
			}
		}
		else							/* no input files - job will queue, ensure job size is legit */
			if( j->size < 1 )
				j->size = 1;

		if( j )
		{
			ng_bleat( 3, "resort_job: queued pending: %s(name) %I*d(size) %I*d(fsizeK) %I*d(fignore)", j->name, Ii(j->size), Ii(size/1024), Ii(ignored/1024) );
			q_job( j, PENDING );			/* job not queued before now, then move to pending queue */
		}
	}

	if( todo < max_resort )
		ng_bleat( 3, "resort_job: requeued %d jobs", max_resort - todo );

	if( (rsq = j) == NULL )			/* exhausted the queue */
		rsq_tail = NULL;
}

/*	resets the lease on a job such that it will not expire before value
	seconds from now. if value < 5 min then the minimum of a 5 minute
	extension is used. if the request is to set it for 0 seconds, then we 
	will assume that the desire is to 'fail' the job and we set the lease
	down to a small number of seconds.
*/
static void extend_lease( char *name, int value )
{
	Job_t	*j;


	if( ! value )
		value = 5;			/* assume a request to extend to 0 is a fail the job request */
	else
		if( value < 300 )
			value = 300;			/* default to a 5 minute extension */

	if( ! (j= get_job( name )) )
	{
		ng_bleat( 1, "extend_lease: no such job: %s", name );
		return;
	}

	if( j->state == RUNNING )	/* can only do running jobs */
	{
		j->flags &= ~JF_IPENDING;			/* must turn off investigation pending so we start another when it gets down to the wire */
		j->state = RUNNING_ADJ;				/* dq will remove, but keep resource counts etc */
		dq_job( j );					/* we must re-queue as running queue is ordered by lease time */
		j->lease = ng_now( ) + (value * 10); 		/* convert to tenths */
		q_job( j, RUNNING_ADJ );			/* repliace on the running queue w/o adjusting resources or node count */
		ng_bleat( 1, "extend_lease: lease extended for %s expires in %d seconds", j->name, value );
	}
	else
		ng_bleat( 1, "extend_lease: job not running, lease not modified: %s", j->name );
}

/* assign a must run on node to a job; job must be pending or missing */
static void assign_node( char *buf )
{
	Job_t	*j;
	char 	*name;		/* job name */
	char	*nname;		/* new node */
	char 	*strtok_p;
	Node_t	*node;		

	if( !buf  )
		return;

	if( (name = strtok_r( buf, ":", &strtok_p )) )
	{
		if( ! (nname =  strtok_r( NULL, ":", &strtok_p )) )
		{
			ng_bleat( 1, "assign_node: bad input: missing node: %s", name );
			return;
		}
	}
	else
	{
		ng_bleat( 1, "assign_node: bad input: missing jobname:node" );
		return;
	}

	
	if( ! (j= get_job( name )) )
	{
		ng_bleat( 1, "assign_node: no such job: %s", name );
		return;
	}

	if( j->state == PENDING || j->state == MISSING )	/* can only do jobs that are waiting to run */
	{
		if( strcmp( nname, "Null" ) == 0 )
		{
			j->flags |= JF_ANYNODE;			/* any node will do */
			j->node = NULL;				/* allow it to go anywhere */
		}
		else
		{
			if( (node = get_node( nname, CREATE ))  )
			{
				j->flags &= ~JF_ANYNODE;			/* any node will not do at this point */
				j->node = node;
				ng_bleat( 3, "assign_node: %s now assigned to  %s", j->name, j->node->name );
			}
			else
				ng_bleat( 1, "assign_node: unable to find node: %s", nname );
		}
	}
	else
		ng_bleat( 2, "assign_node: job not on pending/missing queue, %s %d(state)", j->name, j->state );
}

/* assumes stuff is:  job-name name=value [name=val...]
*/
static void mod_job( int fd, char *stuff )
{
	char *tok;
	char *val;
	Job_t *j;
	char *strtok_p;

	if( (tok = strtok_r( stuff, " :", &strtok_p )) != NULL   &&  (j = get_job( tok )) != NULL)
	{
		while( tok = strtok_r( NULL, " :", &strtok_p ) )
		{
			if( val = strchr( tok, '=' ) )
			{
				val++;
				switch( *tok )
				{
					case 'a':			/* attemp max */
						if( *val == '+' )
							j->max_att += atoi( val );
						else
							j->max_att = atoi( val );
						break;

					case 'd':		/* after failure delay */
						j->delay = atoi( val );
						break;
	
					case 'p':		/* priority */
						if( *val == '+' )
							j->priority += atoi( val );
						else
							j->priority = atoi( val );
						break;
	
					default: 
						ng_siprintf( fd, "unrecognised name=value: %s", tok );
						break;
				}
			}
			else
				ng_siprintf( fd, "bad name=value pair: %s", tok );
		}
	}
	else
		ng_siprintf( fd, "mod_job: no such job: %s", stuff );
}

