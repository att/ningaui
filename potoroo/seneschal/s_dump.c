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
-------------------------------------------------------------------------------------
  Mnemonic:	s_dump.c
  Abstract:	explain and dump (I feel like Im going to wallace) routines. 

		This file contains routines to dump information about what is going on 
		inside of seneschal. These routines vomit information to a TCP sesison
		and thus expect that the caller will pass in the file descriptor from 
		the work block that triggered the call.  In order to explain everything
		when there are a lot of jobs in the environment, and not to block other
		user requests and session notifications, the explain/dump routines will
		generate a limited number of sends and then return to the caller.   The 
		return is marked such that the caller should know to call the same routine
		to further work on the task.  It is expected that the caller will 
		do other processing between calls -- even something that might disrupt 
		the queue being processed.  State between calls is saved with the exdata
		structure. 

		To prevent disaster if a job block is requeued while we have given 
		control back to the caller, the job dequeue routine must check all 
		pending explain/dump commands. If a job that is being dequeued, is listed
		as next, in any of the explain blocks, the next pointer is adjusted so 
		that when we get back to explaining/dumping, we will be pointed at a 
		block that is still on the queue we were processing (or is NULL if the 
		job moved was the last).

		dumping is more complicated that explaining because different levels
		cause different queues to be dumped.  Each level is assigned various
		stages of process with each stage cooresponding to a job queue. Queues
		are processed until exhausted, and when all queues have been finished
		the main wallace() routine is allowed to finish up and trash the work 
		block.

		If the user breaks the TCP session during an ex/dump, we will likely 
		notice it with a failed siprintf() call, or when we are re-invoked
		the fd in the work block will be invalid (as set by the discon callback
		when we were notified of the drop).  

  Author:	E. Scott Daniels
  Mod:		16 Mar 2006 (sd) : hides node from display if released (we must keep 
			a node structure forever because some files may still point to 
			the node).
		07 Oct 2006 (sd) : Changed bleat level for next dump1 request message.
		20 Oct 2006 (sd) : Corrected problem causing coredump when dump5 processed.
		15 Nov 2006 (sd) : Made some changes to comments to better explain things.
		18 Apr 2007 (sd) : Added work queue stats to the summary
		20 Nov 2007 (sd) : Corrected dump pending job to prevent core dump if no 
			files are needed by the job. 
		15 Jan 2009 (sd) : Added support for dump 11.
-------------------------------------------------------------------------------------
*/

#define SIZE_VAR(a)	sizeof(a),(a)	/* for sfprintf things like %I*d */

#define JOB_ONLY	0		/* causes wallace rtns to list just the job info -- no file info */
#define JOB_AND_FILE	1		/* wallace funcitons will list job and needed input files */
#define FILE_ONLY	2		/* list only fileinfo w/ a job name; must be larger than others because we use < FILE_ONLY */

#define PENDING_STAGE	4		/* wallace stages for task interleaving */
#define MISSING_STAGE	3
#define RUNNING_STAGE	2
#define COMP_STAGE	1

#include <time.h>

	/* stuff we keep in order to break our explaining into chunks so as not to block */
struct exdata {
	int	flags;		/* see EF_ constants */
	int	stage;		/* dump stage for the level requested */
	Job_t	*next;		/* pointer at next job to do when recalled  (see note above about preventing disaster when a job moves queues) */
};

/* dont feel so good, think im going to wallace */

#define MAX_AT_ONCE	150		/* max things to explain before taking a break */
#define NEXPEND		25		/* max concurrent explains/dumps */

					/* exdata flags */
#define EF_INUSE	0x01		/* the block in the ex-data list is in use */

static struct exdata ex_pend[NEXPEND];
static int ex_inprogress = 0;		/* number of explains in progress */

/* ------------------ dump/explain local utility things ------------------------------------- */

static int exd_get( )			/* get index to next free ex block  -1 on err*/
{
	int i;

	for( i = 0; i < NEXPEND && ex_pend[i].flags & EF_INUSE; i++ );
	if( i < NEXPEND )
	{
		memset( &ex_pend[i], 0, sizeof( ex_pend[i] ) );
		ng_bleat( 3, "exd_get: new block %d", i );
		ex_inprogress++;
		return i;
	}

	return -1;
}

/* update info in the ex-data block or find a new one  and initialise if e < 0 */
static int exd_update( int e, Job_t *j )
{
	if( e >= 0 || (e = exd_get( )) >= 0 )
	{
		ex_pend[e].next = j;		/* pointer at next one to do when we come back */
		ex_pend[e].flags |= EF_INUSE;	/* mark it inuse */
		return e;			/* return the block number for reference */
	}

	return -1;
}

/*	drop use of an ex block */
static void exd_drop( int e )
{
	if( e >= 0 )
	{	
		ex_pend[e].next = NULL;
		ex_pend[e].flags = 0;
		if( --ex_inprogress < 0 )
			ex_inprogress = 0;
	}
}


/* find the block that points to j and adjust our pointer -- j is moving */
void exd_adjust( Job_t *j )
{
	int	need;
	int 	i;

	need = ex_inprogress;
	for( i = 0; need && i < NEXPEND; i++ )		/* must test all as concurrent explains could stop at the same place */
	{
		if( ex_pend[i].next == j )		/* a match */
		{
			ng_bleat( 2, "pending explain pointer was adjusted for job: %s", j->name );
			ex_pend[i].next = j->next;	/* by default go to next, tho someday ex_pending might indicate direction */
			need--;				/* we can short circuit if we find all current blocks */
		}
	}
}

/* requeue the work block.  If needed setup and point it to an exd block */
static void requeue( Work_t *w, Job_t *j )
{
	int e;

	if( (e = exd_update( w->value, j )) >= 0 )
	{
		w->value = e;				/* incase it was not a requeued block */
 		queue_work( PRIORITY_Q, w );			/* put it back at the end of the priority work queue */	
	}
	else
	{
		ng_bleat( 2, "too many concurrent explain/dumps in progress; request refused" );
		ng_siprintf( w->fd, "too many concurrent explain/dump requests; request refused" );
		ng_siprintf( w->fd, ".\n" );
		trash_work( w );
	}

	return;					/* errors -- nothing left to do */
}


/*  ------------- wallace and gromit things ------------------------------------------------------ */
/* dump a job queue */
static Job_t *wallace_jobq( int fd, Job_t *j, char *id, int finfo )
{
	File_t	*f;
	char	*name;
	char	*owner;
	char	*notsym = "";
	char	buf[2048];
	int	i;
	int	k;
	int 	kk;
	int	ss = 0;			/* send status */
	int 	targets = 0;
	int	max2do = MAX_AT_ONCE;	/* max per burst; before causing work block to be requeued */
	ng_timetype	now;

	now = ng_now( );

	if( ng_siblocked( fd ) )		/* si routines queue if send would block and */
		return j;			/* we dont want to queue in user land -- wait until clear */

	while( max2do > 0 && j && ss >= 0 )
	{
		max2do--;

		owner = j->response[0] ? j->response : "<empty>";
		notsym = "";
		if( j->node )			/* if running or specific node requested */
			name = j->node->name;
		else
		{
			if( j->notnode )
			{
				notsym = "!";
				name = j->notnode->name;
			}
			else
				if( j->nattr )				/* node attributes required by job */
					name = j->nattr;
				else
					name = "<any>";
		}

		*buf = 0;
		for( k = 0; k < j->nrsrc; k++ )
		{
			if( j->rsrc[k] )
			{
				if( k )
					strcat( buf, "," );
				strcat( buf, j->rsrc[k]->name );
			}
		}

		if( finfo < FILE_ONLY ) 			/* large finfo just lists the files, anything less and we list job info too */
			ss = ng_siprintf( fd, "%s: %s(name) %I*d(size) %d(pri) %d(cost) %d(att) %d(state) %s%s(node) %ld(expr) %s(rsrc) %s(owner) %ld(delay) %x(fl) %s\n", 
				id, j->name, Ii(j->size), j->priority, j->cost, j->attempt, j->state, notsym, name, 
				(long) (j->lease ? (j->lease-now)/10 : -1), buf, owner, (long) j->run_delay, j->flags, j->ocmd ? j->ocmd : "{command not buffered for dump}" );

		*buf = 0;
		if( finfo && ss >= 0 )
		{
			for( k = 0; k < j->ninf && ss >= 0; k++ )
			{
				if( (f = j->inf[k]) )
				{
					if( finfo >= FILE_ONLY )			/* large value is just file, so we add job name */
						sfsprintf( buf, sizeof( buf ), "%s %s %I*d ", j->name, f->name, SIZE_VAR( f->size ) );
					else
						sfsprintf( buf, sizeof( buf ), "%s %I*d ", f->name, SIZE_VAR( f->size ) );
					
					for( kk = 0; kk < f->nnodes; kk++ )
					{
						if( f->nodes[kk] )
						{
							strcat( buf, f->nodes[kk]->name );
							strcat( buf, " " );
						}
					}	

					ss = ng_siprintf( fd, "%s input: %s\n", id, buf );
				}
			}

			if( j->nattr && finfo < FILE_ONLY )			/* listing job info and it has attributes listed */
			{
				*buf = 0;
				get_node_list( j->nattr, buf, now );
				ng_siprintf( fd, "%s nattrs: { %s } %s\n", id, j->nattr, *buf ? buf : "no nodes have this attribute set");
			}
			
		}

		j = j->next;
	}				/* while jobs and si status is ok */

	return ss >= 0 ? j : NULL;	/* if somekind of write error, abort the rest of the queue */
}

/* summary dump */
static void wallace_sum( int fd, ng_timetype start, ng_timetype end )
{
	char	obuf[NG_BUFFER];
	long	job_total = 0;		/* job counts */

	long	bid_total = 0;		/* bid counts */
	long	bid_max = 0;		/* max bids on a single node */

	Job_t	*j;
	Node_t *n;
	File_t *f;

	job_total = count_pq + count_rq + count_cq + count_rsq + count_mq;		/* total queue stats */

	for( n = nodes; n; n = n->next )
	{
		bid_total += n->bid;
		if( bid_max < n->bid )
			bid_max = n->bid;
	}

	sfsprintf( obuf, sizeof(obuf), "summary: %s %d(jobs) %d(pq) %d(mq) %d(rsq) %d(rq) %d(cq) %d(stuck) %d(bids) %d/%d/%d(work) %I*d(ffiles) %I*d(time) %x(flags)", 
		suspended ? "SUSPENDED" : "", job_total, count_pq, count_mq, count_rsq, count_rq, count_cq, 
		deadlock, bid_total, work_qsize( PRIORITY_Q ), work_qsize( STATUS_Q ), work_qsize( NORMAL_Q ), Ii(count_ffq), sizeof( end ), (end-start)/10, gflags );

	if( fd >= 0 )
		ng_siprintf( fd, "%s\n", obuf );
	else
		ng_bleat( 0, "%s", obuf );
}

/* dump resource list */
static void wallace_rsrc( int fd, Resource_t *single )
{
	Resource_t *r;
	int ss = 0;

	for( r = single ? single : resources; r && ss >= 0; r= r->next )
	{
		if( !(r->flags & RF_IGNORE) )
		{
			if( r->flags & RF_HARD )
				ss = ng_siprintf( fd, "resource: %10s(name) %3dh(value) %3d(mpn) %3.0fh(target) %02d/%02d%%(run) %d(tot)\n", 
					r->name, r->limit, r->mpn, r->target, r->active, (100 * r->active)/(total_resources? total_resources : 1), r->ref_count );
			else
				ss = ng_siprintf( fd, "resource: %10s(name) %3d(value) %3d(mpn) %3.0f%%(target) %02d/%02d%%(run) %d(tot)\n", 
					r->name, r->limit, r->mpn, r->target*100, r->active, (100 * r->active)/(total_resources? total_resources : 1), r->ref_count );
		}

		if( single )
			return;			/* just one */
	}
}

static void wallace_mpn_counts( int fd, Resource_t *single )
{
	Resource_t *r;
	Node_t	*n;	
	int 	ss = 0;
	int	*count;
	

	for( r = single ? single : resources; r && ss >= 0; r= r->next )
	{
		if( !(r->flags & RF_IGNORE) )
		{
			if( r->flags & RF_HARD )
				ss = ng_siprintf( fd, "resource: %10s(name) %3dh(value) %3d(mpn) %3.0fh(target) %02d/%02d%%(run) %d(tot)\n", 
					r->name, r->limit, r->mpn, r->target, r->active, (100 * r->active)/(total_resources? total_resources : 1), r->ref_count );
			else
				ss = ng_siprintf( fd, "resource: %10s(name) %3d(value) %3d(mpn) %3.0f%%(target) %02d/%02d%%(run) %d(tot)\n", 
					r->name, r->limit, r->mpn, r->target*100, r->active, (100 * r->active)/(total_resources? total_resources : 1), r->ref_count );

			for( n = nodes; n && ss >= 0; n = n->next )
			{
				if( ! (n->flags & NF_RELEASED) )		/* dont show it if it has been released */
				{
					if( (count = mpn_counter( r->name, n->name )) != 0 )
						ss = ng_siprintf( fd, "\tmpn running count: %s %d\n", n->name, *count );
				}
			}
		}

		if( single )
			return;			/* just one */
	}
}


/*	dump type list 
	if to supplied, then we send on tcp session rather than bleating
*/
static void wallace_type( int fd )
{
	Jobtype_t *t;
	double	cost;
	int 	i;
	int	ss = 0;				/* send status */
	char	buf[NG_BUFFER];

	for( t = jobtypes; t && ss >= 0; t = t->next )
	{
		cost = 0;
		for( i = 0; i < MAX_COST_DATA && t->cost[i] >= 0; i++ )
		{
			ng_bleat( 4, "type: %s(name) %d(obs) %.2f(value-tenths)", t->name, i, t->cost[i] );
			cost += t->cost[i];
		}

		if( fd >= 0 )
		{
			ss = ng_siprintf( fd, "job_cost: %s %.4f %d\n", t->name, (cost/i)/10, i );		/* cost info is kept in tenths of seconds */
		}
		else
			ng_bleat( 1, "type: %s(name) %d observations: %.4f(sec/size-unit)", t->name, i, (cost/i)/10 );
	}

	if( fd >= 0 )
		ng_siprintf( fd, ".\n" );			/* end of transmission */
}

static void wallace_node( int fd )
{
	Node_t	*n;	
	int ss = 0;			/* send status - stop short if send fails */
	
	for( n = nodes; n && ss >= 0; n = n->next )
	{
		if( ! (n->flags & NF_RELEASED) )		/* dont show it if it has been released */
			ss = ng_siprintf( fd, "node: %s(name) %d(bid) %d(run) %d(want) %s {%s}\n", 
					n->name, n->bid, n->rcount, n->desired, n->suspended ? "SUSPENDED" : "", n->nattr ? n->nattr : "no-attrs"  );
		else
			ss = ng_siprintf( fd, "node: %s(name) -(bid) -(run) -(want) RELEASED {%s}\n", n->name,  n->nattr ? n->nattr : "no-attrs"  );
	}
}

/*	dump the files via tcp to the named process 
	this interface primarly for andrew's daemon to know what files are needed and where they are at present
*/
static void tcp_dump_files(  int fd )
{
	char	msg[NG_BUFFER];
	File_t	*f;
	Job_t 	*j;
	Node_t	*n;
	int 	i;
	int	k;
	int	ss = 0;					/* send status */

	ng_bleat( 3, "dumping files via tcp\n" );

	for( f = ffq; f && ss >= 0; f = f->next )
	{
		sfsprintf( msg, sizeof( msg ), "needed_input: %s nodes={", f->name );
		for( i = 0; i < f->nidx; i++ )
			if( (n = f->nodes[i]) != NULL  )
			{
				if( i > 0 )
					strcat( msg, "," );
				strcat( msg, n->name );
			}

		strcat( msg, "} jobs={" );

		for( k=0, i = 0; k< 10 && i < f->nref; i++ )		/* keep it to a sane number; 10 is plenty */
		{
			if( (j = f->reflist[i]) != NULL  )
			{
				if( k++ )				/* cannot rely on i as first might not be at i=0 */
					strcat( msg, "," );
				strcat( msg, j->name );
			}
		}

		ss = ng_siprintf( fd, "%s}\n", msg );	/* terminate and send */		
	}

	if( ss >= 0 )
		ng_siprintf( fd, ".\n" );			/* end of transmission */
}

static void wallace_files( int fd, char *id, File_t *first, int justone  )
{
	Syment	*se;
	File_t *f;
	char	buf[4096];		/* spot to create node name list in */
	char	buf2[4096];		/* spot to create node name list in */
	int	i;			/* generalised loop index */
	int	n;			/* number of actual node pointers we see */
	int	ss = 0;			/* send status */
	/*int	count = 0;*/		/* number we put out - must pause every so often to not use udp messages */
	char	*gar;
	
	for( f = first; f && ss >= 0; f = f->next )
	{
		buf[0] = 0;
		n = 0;
		for( i = 0; i < f->nnodes; i++ )
		{
			if( f->nodes[i] )
			{
				if( n++ )				/* add comma if not the first one; cannot rely on i */
					strcat( buf, "," );		/* as i==0 could be null! */
				strcat( buf, f->nodes[i]->name );
			}
		}	
		if( ! buf[0] )
			strcat( buf, "<none>" );

		buf2[0] = 0;
		n = 0;
		for( i = 0; n < 10 && i < f->nref; i++ )
		{
			if( f->reflist[i] )
			{
				if( n++ )				/* add comma if not the first one; cannot rely on i */
					strcat( buf2, "," );		/* as i==0 could be null! */
				strcat( buf2, f->reflist[i]->name );
			}
		}	
		if( ! buf2[0] )
			strcat( buf2, "<none>" );

		ss = ng_siprintf( fd, "%s %cstable %s(name) %lld(size) %s(md5) %s%s(gar) %s(nodes) %d(rcount) %s(refs)\n", id, 
				f->stable ? ' ' : '!', f->name,  f->size, 
				"noMd5", f->gar ? f->gar : "<undef>", f->gar_lease ? "!REQ" : "", buf, f->ref_count, buf2 );

		if( justone )
			return;
	}
}

static void wallace_gar( int fd )
{
	Gar_t	*g;
	int	ss = 0;		/* send status */

	for( g = gar_list; g && ss >= 0; g = g->next )
	{
		ss = ng_siprintf( fd, "gar: %s = (%s)\n", g->name, g->cmd );		
	}
}

/* 
	called to vomit a portion of a job queue. start where we left off and if we 
	finish, bump to the next stage and return.  The next call will start at the 
	beginning of the next queue stage. 
	The init-stage is the first stage (queue) to dump, the last-stage is the
	last queue that we dump for the dump level indicated by the user. 
	option is JOB_ONLY, FILE or JOB_AND_FILE (depends on level)

	return is 0 if done, 1 if more to do (so caller can add things at end if needed,
	and cleanup -- signal end to user)
*/
static int do_stage( Work_t *w, int init_stage, int last_stage, int option )
{
	int	e;		/* index into pending explain/dump list */
	Job_t 	*j = NULL;
	char	*str;		/* eye catcher string */

	if( w->value < 0 )			/* first call for this user dump request */
	{
		if( (e = exd_get( )) < 0 )	/* too many pending */
		{
			ng_siprintf( w->fd, "too many concurrent dump/explain commands; command not accepted\n.\n" );
			return 0;
		}

		w->value = e;
		ex_pend[e].stage = init_stage;		/* set the initial stage */
	}
	else
	{
		e = w->value;
		j = ex_pend[e].next;			/* get the job where we left off */
	}

	if( ! j )					/* no pick up point left from last go round */
	{
		switch( ex_pend[e].stage )		/* start at beginning of queue, and issue a seperator if not doing file-only */
		{
			case PENDING_STAGE:	
					if( option < FILE_ONLY )
						ng_siprintf( w->fd, "===== pending job queue =====\n" );
					j = pq; 
					break;

			case MISSING_STAGE: 	
					if( option < FILE_ONLY )
						ng_siprintf( w->fd, "===== missing job queue =====\n" );
					j = mq; 
					break;

			case RUNNING_STAGE:	
					if( option < FILE_ONLY )
						ng_siprintf( w->fd, "===== running job queue =====\n" );
					j = rq; 
					break;

			case COMP_STAGE:	
					if( option < FILE_ONLY )
						ng_siprintf( w->fd, "===== complete job queue =====\n" );
					j = cq; 
					break;

			default:	j = NULL;			/* who knows -- dont do anything */
		}
	}

	switch ( ex_pend[e].stage )					/* set eye catcher string for the wallace_jobq routine */
	{
			case PENDING_STAGE:	str = "pending"; break;
			case MISSING_STAGE: 	str = "missing"; break;
			case RUNNING_STAGE:	str = "running"; break;
			case COMP_STAGE:	str = "complete"; break;
			default:	str = "unknown";			/* who knows -- dont do anything */
	}

	if( !j || (j = wallace_jobq( w->fd, j, str, option )) == NULL )		/* if !j (nothing on the queue) or jobq finished */
		ex_pend[e].stage--;						/* progress to next stage */

	if( ex_pend[e].stage < last_stage )			/* this was the last stage, then cleanup */
		return 0;

	requeue( w, j );		/* set up for a recall */
	return 1;			/* weve requeued so caller should not finish up */
}

/* general dump routine 
   due to the implementation of symtab (ng_lib) it only dumps to stderr and will never go back to 
   the user via UDP.

	we assume that w->misc points at the dump level string -- not yet converted to int

	with the attempt to interleave dump/explain processing with other real work, the support of
	1x style dumping is deprecated (it was not used anyway).

   dump levels:	1 = summary
		2 = summary, nodes, 
		3 = summary, nodes, queues
		4 = summary, nodes, pending/missing/running queues (including input file info for pending & missing queues)
		5 = summary, nodes, pending/missing/running queues (including input file info for all queues)
		6 = summary, nodes, all job queues (including pending/running input file info)
		7 = files only 
		8 - summary, missing queue
		9 = job management and performance oriented output
		10= resource info
		20= type info
		30= node info
		40= gar commands
*/
static void wallace( Work_t *w )			
{
	Node_t *n;
	int	l;
	int	ss = 0;		/* send status */
	int 	fd;
	int	level;
	char	*what;		

	if( (fd = w->fd) < 0 )
	{
		ng_bleat( 2, "wallace: called with a bad w->fd; assume session dropped exd=%d", w->value );
		exd_drop( w->value );		/* if we had paused, free the block */
		trash_work( w );
		return;
	}	

	if( (what = w->misc) == NULL )
	{
		ng_bleat( 2, "wallace: w->misc was null; halting wallace exd=%d ", w->value );
		exd_drop( w->value );		/* if we had paused, free the block */
		ng_siprintf( w->fd, ".\n" );
		trash_work( w );
		return;
	}

	level = atoi( what );		/* convert level */
	fd = w->fd;

	/*
		assume that vomiting job queues are the only thing that need to be interleaved with other work.
		Thus 4 stages of work in progress:
			stage 4 -- pending; stage 3 -- missing; stage 2 running; stage 1 -- complete.

		cannot depend on w after here -- do_stage will trash it if necessary 
	*/
	switch( level )
	{


		case 2:
			if( ng_siprintf( fd, "===== nodes ====\n" ) >= 0 )
				wallace_node( fd );
			wallace_sum( fd, 0, 0 );
			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			break;

		case 3:
			if( ! do_stage( w, PENDING_STAGE, PENDING_STAGE, JOB_ONLY ) )
				if( ng_siprintf( fd, "===== nodes ====\n" ) >= 0 )
				{
					wallace_node( fd );
					wallace_sum( fd, 0, 0 );
					ng_siprintf( fd, ".\n" );
					exd_drop( w->value );		/* if we had paused, free the block */
					trash_work( w );
				}
			break;

		case 4:
			if( ! do_stage( w, PENDING_STAGE, RUNNING_STAGE, JOB_ONLY ) )
			{
				if( ng_siprintf( fd, "===== nodes ====\n" ) >= 0 )
				{
					wallace_node( fd );
					wallace_sum( fd, 0, 0 );
					ng_siprintf( fd, ".\n" );
					exd_drop( w->value );		/* if we had paused, free the block */
					trash_work( w );
				}
			}
			break;

		case 5:
			if( ! do_stage( w, PENDING_STAGE, RUNNING_STAGE, JOB_AND_FILE ) )
			{
				if( ng_siprintf( fd, "===== nodes ====\n" ) >= 0 )
				{
					wallace_node( fd );
					wallace_sum( fd, 0, 0 );
					ng_siprintf( fd, ".\n" );
					exd_drop( w->value );		/* if we had paused, free the block */
					trash_work( w );
				}
			}
			break;

		case 6:
			if( ! do_stage( w, PENDING_STAGE, COMP_STAGE, JOB_AND_FILE ) )
			{
				if( ng_siprintf( fd, "===== nodes ====\n" ) >= 0 )
				{
					wallace_node( fd );
					wallace_sum( fd, 0, 0 );
					ng_siprintf( fd, ".\n" );
					exd_drop( w->value );		/* if we had paused, free the block */
					trash_work( w );
				}
			}
			break;


		case 7:		
			wallace_files( fd, "files:", ffq, 0 );	
			wallace_sum( fd, 0, 0 );
			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			break;

		case 8:		
			if( ! do_stage( w, MISSING_STAGE, MISSING_STAGE, JOB_AND_FILE ) )
			{
				wallace_sum( fd, 0, 0 );
				ng_siprintf( fd, ".\n" );
				exd_drop( w->value );		/* if we had paused, free the block */
				trash_work( w );
			}
			break;

		case 9:		
			if( ! do_stage( w, PENDING_STAGE, MISSING_STAGE, FILE_ONLY ) )
			{
				wallace_node( fd );
				wallace_sum( fd, 0, 0 );
				ng_siprintf( fd, ".\n" );
				exd_drop( w->value );		/* if we had paused, free the block */
				trash_work( w );
			}
			break;

		case 10:	wallace_rsrc( fd, NULL ); 		/* dump resources */
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;

		case 11:	wallace_mpn_counts( fd, NULL ); 		/* dump resources with mpn counter info */
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;

		case 20:	wallace_type( fd ); /* dump type information */
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;
		case 30:	wallace_node( fd ); /* node info */
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;
		case 40:	wallace_gar( fd ); /* gar commands */
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;

		case 99:

   				ng_siprintf( fd, "dump levels:	\n1 = summary\n" );
				ng_siprintf( fd, "2 = summary, nodes \n" );
				ng_siprintf( fd, "3 = summary, nodes, queues\n" );
				ng_siprintf( fd, "4 = summary, nodes, pending/missing/running queues (including input file info for pending & missing queues)\n" );
				ng_siprintf( fd, "5 = summary, nodes, pending/missing/running queues (including input file info for all queues)\n" );
				ng_siprintf( fd, "6 = summary, nodes, all job queues (including pending/running input file info)\n" );
				ng_siprintf( fd, "7 = files only \n" );
				ng_siprintf( fd, "8 - summary, missing queue\n" );
				ng_siprintf( fd, "9 = job management and performance oriented output\n" );
				ng_siprintf( fd, "10= resource info\n" );
				ng_siprintf( fd, "20= type info\n" );
				ng_siprintf( fd, "30= node info\n" );
				ng_siprintf( fd, "40= gar commands\n" );
				ng_siprintf( fd, ".\n" );			/* end of data marker */
				trash_work( w );
				break;

		default:
				wallace_sum( fd, 0, 0 );
				ng_siprintf( w->fd, ".\n" );
				trash_work( w );
				break;
	}
}


/* ----------------- explain functions -- human oriented output ------------------- */
/* explain a queue -- return true if nothing more to explain and end of transmission should be sent */
/*void explain_all( int fd, char *what )*/
void explain_all( Work_t *w )
{
	Job_t *j;
	ng_timetype now;
	char	*tok;
	char	*strtok_p;		/* pointer for strtok_r */
	int 	count = 0;			/* number that we have spit -- before pause */
	int 	fd;
	int	e;			/* index into exdata */
	char	*what;

	if( (fd = w->fd) < 0 )
	{
		ng_bleat( 2, "explain-all: called with a bad w->fd; assume session dropped exd=%d", w->value );
		exd_drop( w->value );		/* if we had paused, free the block */
		ng_siprintf( w->fd, ".\n" );
		trash_work( w );
		return;
	}	

	if( (what = w->misc) == NULL )
	{
		ng_bleat( 2, "explain-all: w->misc was null; halting explain exd=%d ", w->value );
		exd_drop( w->value );		/* if we had paused, free the block */
		ng_siprintf( w->fd, ".\n" );
		trash_work( w );
		return;
	}

	now = ng_now( );

	if( w->value < 0 )			/* not a suspended explain -- figure out what queue to dump, or specific job name */
	{
		if( (tok = strtok_r( what, " ,:", &strtok_p )) == NULL )
		{
			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			return;
		}
	
		if( strcmp( tok, "pend" ) == 0 || strcmp( tok, "pending" ) == 0 ||  strcmp( tok, "all" ) == 0 )
			j = pq;
		else
		if( strcmp( tok, "miss" ) == 0 || strcmp( tok, "missing" ) == 0 )
			j = mq;
		else
		if( strcmp( tok, "run" ) == 0 || strcmp( tok, "running" ) == 0 )
			j = rq;
		else
		if( strcmp( tok, "comp" ) == 0 || strcmp( tok, "complete" ) == 0 || strcmp( tok, "done" ) == 0 )
			j = cq;
		else
		{
			while( tok )		/* may be: job jobname  or file filename  or resource rname or just name */
			{
				if( strcmp( tok, "job" ) == 0 )
				{
					tok = strtok_r( NULL, " ,:", &strtok_p );
					if(  explain_job( fd, tok, NULL, now ) < 0  )
						ng_siprintf( fd, "no such job: %s\n", tok );
				}
				else
				if( strcmp( tok, "resource" ) == 0 )
				{
					tok = strtok_r( NULL, " ,:", &strtok_p );
					if( ! explain_resource( fd, tok )  )
						ng_siprintf( fd, "no such resource: %s\n", tok );
				}
				else
				if( strcmp( tok, "file" ) == 0 )
				{
					tok = strtok_r( NULL, " ,:", &strtok_p );
					if(  ! explain_file( fd, tok ) )
						ng_siprintf( fd, "no such file: %s", tok );
				}
				else				/* assume name, try in job, resource, file order */
				{
					if(  explain_job( fd, tok, NULL, now ) < 0 && explain_resource( fd, tok ) == 0 && explain_file( fd, tok ) == 0 )
						ng_siprintf( fd, "no such job, resource, or file: %s\n", tok );
				}

				tok = strtok_r( NULL, " ,:", &strtok_p );
			}

			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			return;
		}
	}
	else
	{
		if( w->value  >= 0 )
			j = ex_pend[w->value].next;			/* line up and start again */
		else
		{
			ng_siprintf( fd, "internal seneschal error matching preempted explain data on recall\n.\n" );
			exd_drop( w->value );
			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			return;
		}

		if( ng_siblocked( fd ) )				/* we dont want to queue too many in user space or we run out of core */
		{
			requeue( w, j );		/* set up for a recall */
			return;
		}
	}


	for( j; j && count < MAX_AT_ONCE; j = j->next )
	{
		count++;
		if( explain_job( fd, NULL, j, now ) < 0 )	/* send error */
		{
			exd_drop( w->value );			
			ng_siprintf( w->fd, ".\n" );
			trash_work( w );
			return;
		}
	}

	if( j )			/* we had to stop short */
	{
		requeue( w, j );		/* set up for a recall */
		return;
	}

	exd_drop( w->value );		/* if we were a requeue -- must clean it */
	ng_siprintf( w->fd, ".\n" );
	trash_work( w );
}


/* returns 0 on 'failure' */
int explain_resource( int fd, char *what )
{

	Resource_t *r;

	
	if( (r = get_resource( what, NO_CREATE )) != NULL )
		wallace_rsrc( fd, r );
	else
		return 0;

	return 1;
}


/* dump specific info about a file including where it is on each node (full path) */
int explain_file( int fd, char *what )
{
	File_t	*f;
	int	i;

	if( (f = get_file( what, NULL, NULL )) == NULL )		/* find but dont create */
		return 0;						

	ng_siprintf( fd, "explain file: %s %cstable %s(name) %lld(size) %s(md5) %d(rcount)\n", what, f->stable ? ' ' : '!', f->name,  f->size, f->md5, f->ref_count );
	for( i = 0; i < f->nnodes; i++ )
	{
		if( f->nodes[i] && f->paths[i] )
			ng_siprintf( fd, "explain path: %s %s\n", f->nodes[i]->name, f->paths[i] );
	}

	return 1;
}

/*	makes an attempt to explain the state of a job */
/*	either name or jobpointer can be passed in */
int explain_job( int fd, char *name, Job_t *j, ng_timetype now )
{
	File_t	*f;
	Resource_t *r;			/* pointer to resource to validate */
	Node_t	*n = NULL;		/* selected node */
	int i;
	int k;
	int ss;				/* si status */
	int	rstate;
	char	*nname;			/* convenience name pointers; node/woomera job */
	char	*wname;
	char	buf[NG_BUFFER];


	if( !j && !(j = get_job( name )) )
	{
		return -1;
	}

	switch( j->state )
	{
		case RUNNING:	
			ss = ng_siprintf( fd, "explain: job %s is running; %s(node) %s(wname) %s(user) %d(att) %ld(lease) %ld(elapsed)\n", j->name, j->node->name, j->wname, j->username, j->attempt, (long) ((j->lease-now)/10), (long) ((now - j->started)/10) );
			break;

		case RESORT:	
			ss = ng_siprintf( fd, "explain: job %s blocked: awaiting queue reassignment\n", j->name );		/* unlikely */
			break;

		case MISSING:
			*buf = 0;
			if( j->ninf_need > 0 )
				sfsprintf( buf, sizeof( buf ), "%d", j->ninf_need );
			ss = ng_siprintf( fd, "explain: job %s %s(user) %s(target) {%s}(attrs) %s(reqinf) %s blocked: file(s) missing or has bad size\n", j->name, j->username, j->node ? j->node->name : "any", j->nattr ? j->nattr : "", *buf ? buf : "all", j->flags & JF_OBSTRUCT ? "on hold + " : "" );

			for( i = 0; i < j->ninf; i++ )
			{
				if( j->inf[i] )
					wallace_files( fd, "   explain: needed file: ", j->inf[i], 1 );		/* display jobs files */
			}

			if( j->nattr )
			{
				get_node_list( j->nattr, buf, now );
				ss = ng_siprintf( fd, "   explain: nattrs: {%s} %s\n", j->nattr, *buf ? buf: "nattr list not satisfied by any node" );	
			}
			break;

		case PENDING:
			if( j->flags & JF_OBSTRUCT )
				ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: externally induced hold\n", j->name, j->username );
			else
			if( j->run_delay > now )
				ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: restart delay set; %d seconds remaining\n", j->name,  j->username, (long) ((j->run_delay - now)/10)  );
			else
			{
					/* this is NOT the same order that we use to test the job for submission so that we can show stuck jobs even when they would not be runnable because of resources */
#ifdef KEEP
				if( j->node )		/* user supplied node; we will use it regardless (no need to check files) */
				{
					ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: no bids and/or missing files on: %s\n", j->name, j->username, j->node->name );
				}
				else
#endif
				{
					*buf = 0;
					if( j->notnode )
						sfsprintf( buf, sizeof( buf ), " !on=%s", j->notnode->name );
					else
						sfsprintf( buf, sizeof( buf ), " on=(%s)", j->node ? j->node->name : (j->nattr ? j->nattr:"any-node") );


					if( j->ninf > 0  &&  j->inf[0] )		/* any files in the list? */
					{
						int reason = 0;
						char *rstr; 

						map_f2node( j, &reason );
										/* order we check is important for giving most useful info */
						if( reason & JNS_FILES )	
							rstr = "input files not on common node"; 
						else
						if( reason & JNS_ATTRS )	
							rstr = "all input files not on node with desired attributes";
						else
						if( reason & JNS_STABLE )	
							rstr = "intput files are unstable"; 
						else
						{
							if( total_resources == 0 || check_resources( j, 1 ) )	
								rstr = "waiting on node with bids";
							else
								rstr = "waiting on resource(s)";
						}
						ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: %s %s (0x%02x)\n", j->name, j->username, rstr, buf, reason );
					}
					else				
					{
						if( total_resources == 0 || check_resources( j, 1 ) )	/* if job runable from a resource perspective */
						{
							ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: waiting on node with bids %s\n", j->name, j->username, buf );
						}
						else
						{
							ss = ng_siprintf( fd, "explain: job %s %s(user) blocked: waiting on resources\n", j->name, j->username );
							ss = ng_siprintf( fd, "\tresources:" );
							for( k  = 0; k < j->nrsrc; k++ )
								if( j->rsrc[k] )
									ss = ng_siprintf( fd, " %s", j->rsrc[k]->name );
							ss = ng_siprintf( fd, "\n" );
						}
					}
				}
			}

			for( i = 0; i < j->ninf; i++ )
				if( j->inf[i] )
					wallace_files( fd, "explain: files: ", j->inf[i], 1 );		/* display jobs files */
			if( j->nattr )
			{
				get_node_list( j->nattr, buf, now );
				ss = ng_siprintf( fd, "   explain: nattrs: {%s} %s\n", j->nattr, *buf ? buf: "nattr list not satisfied by any node" );	
			}

			break;

		default:	
			nname = j->node ? (j->node->name ? j->node->name : "unknown") : "unknown";
			wname = j->wname ? j->wname : "unknown";
			ss = ng_siprintf( fd, "explain: job %s completed; %s(user) %s(node) %s(wname) %d(state) %d/%d(exit/hexit)\n", j->name, j->username, nname, wname, j->state, j->exit, j->hexit );
			break;
	}

	return ss;
}

