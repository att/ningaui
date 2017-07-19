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
 -----------------------------------------------------------------------------
  Mnemonic:	ng_seneschal
  Abstract:	Manages all of the jobs that nawab says can be run.
  Date:		25 Jun 2001
  Author: 	E. Scott Daniels
 -----------------------------------------------------------------------------
*/

#define SKIP_AST_COMON 1

#include	<stdlib.h>
#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<pthread.h>
#include	<signal.h>
#include	<errno.h>
#include	<limits.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>
#include	"seneschal.h"
#include	"seneschal.man"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>

#ifndef OPEN_MAX
#define OPEN_MAX 20		/* bloody linux does not define OPEN_MAX */
#endif


/* ------------------------ a few globals ----------------------------- */

Symtab		*symtab = NULL;		/* the big hash */
Node_t		*nodes = NULL;		/* master node list */
Resource_t	*resources = NULL;	/* master resource list */
Jobtype_t	*jobtypes = NULL;	/* master types list */

Job_t	*pq = NULL;		/* job queues: pending, resort, missing, running, complete */
Job_t	*rsq = NULL;
Job_t	*mq = NULL;
Job_t	*rq = NULL;
Job_t	*cq = NULL;
Job_t	*pq_tail = NULL;	/* queue tails */
Job_t	*rsq_tail = NULL;
Job_t	*mq_tail = NULL;
Job_t	*rq_tail = NULL;	
Job_t	*cq_tail = NULL;

File_t	*ffq = NULL;		/* queue of input files for jobs */
File_t	*ffq_tail = NULL;

Gar_t	*gar_list = NULL;	/* the list of stuff used to gar input files */

Sfio_t	*bleatf = NULL;		/* file for bleats if running as daemon */

int		suspended = 0;		/* job starts suspended */
int		ok2run = 1;		/* turn off to stop threads */
char		*myport = NULL;		/* port number we are listening on */
char		thishost[100];		/* name of this host for return status from woomera */
int		priority_creep = 1;	/* allow job's priority to creep up the longer it sits in pending queue */
int		deadlock = 0;		/* number of jobs that have all input, but scattered such that job cannot be run */
long		total_resources = 0;	/* resource pool: running + unassigned bid amounts */
int		gflags = 0;		/* see GF_ constants */
int		min_lease_sec = 18000;	/* min lease time (tenths) */
int		max_lease_sec = 864000;	/* max lease time ~ 24 hr (tenths) */
int		max_work = 5000;	/* max number of work blocks we will process before doing other stuff (dynamic config: sreq maxwork n) */
int		max_resort = 100;	/* max things on the resort queue to process; dynamic config: sreq maxresort n */
char		*version = "version 5.10/01289";	

					/* these two values are set to give the best performance in a worst case situation */
int		max_rm_req = 3500;	/* max number of dump1 requests to send in one batch */
int		freq_rm_req = 650;	/* frequency (tsec) between repmgr dump1 sends */

/* used by dump routines to track queue sizes; updated by q/dq routines */
static long	count_rsq = 0;		/* resort */
static long	count_mq = 0;		/* missing */
static long	count_rq = 0;		/* running */
static long	count_pq = 0;		/* pending (ready) */
static long	count_cq = 0;		/* complete */
static long	count_ffq = 0;		/* files in ffq */

/* ----- static prototypes (external protos in seneschal.h) ------------------ */
									/* from s_dump.c - debugging/dumping routines */
static Job_t *wallace_jobq( int fd, Job_t *j, char *id, int finfo );	/* dump information on a specific job queue */
static void wallace_sum( int fd, ng_timetype start, ng_timetype end );	/* dump a summary line */
static void wallace( Work_t *w );					/* main dump entry point for user interface */
static void wallace_rsrc( int fd, Resource_t *single );			/* dump resource information */
static void wallace_type( int fd );					/* dump job type information (tracking info used to calculate costs) */
static void wallace_node( int fd );					/* dump node information */
static void wallace_gar( int fd );					/* dump gar information */
static void wallace_files( int fd, char *id, File_t *first, int justone  ); /* dump file block information */

							/* from s_job.c job block operators */
static Job_t *new_job( char *name );			/* give birth */
static Job_t *add_job( char *buf, int qflag );		/* create a job and potentially queue it */
static void trash_job( Job_t *j );			/* delete a job block (puts back on a reuse queue) */
static void mk_filelst( Job_t *j, Node_t *n );		/* make input file list (full paths) */
static void mk_ofilelst( Job_t *j, int attempt );	/* make the output file list for a job (full paths on the node that job is going to */
static void del_job( char *name );			/* delete */
static void deref_files( Job_t *j );			/* dereference files that are needed by the job */
static void deref_rsrcs( Job_t *j );			/* dereference resources consumed by the job */
static int check_resources( Job_t *j, int cost );	/* see if all resources referenced are available for the job */
static void adjust_resources( Job_t *j, int amt );	/* tweek each of the resources that the job references (up or down) */
static void del_resource( char *name );		/* delete a resource (prevent from dumping info) */
static void req_jobs( File_t *f );			/* requeue jobs referencing file f to resort queue */
static void record_cost( Job_t *j, int time );		/* add the actual 'cost' of a job to our stats for cost calculation */
static void release_job( Job_t *j );			/*  */
static void hold_job( Job_t *j, int attempt, char *node, char *sbuf );
static void *resort_jobs( );				/* run the resort queue dispositioning each job to pending/missing queues */
static void extend_lease( char *name, int value );	/* reset a job's lease */
static void assign_node( char *buf );			/* assign a node to a job if still pending or missing */
static void investigate( Job_t *j );			/* dispatch an investigation into the actual state of the job (uses s_sherlock) */

static void usage( );					/* utility functions */
static void siesta( int tenths );			/* short rests */
static void set_targets( );		
static void force_job( char *name, int push );
static void add_limits( char *buf );
static char *wexpand( char *buf );			/* expand bquotes in cmd woomera stuff cmd real-command */

static void add_file_ref( File_t *f, Job_t *j );	/* add a reference to job in file */

static Job_t *get_job( char *name );			/* fetch/map */
static Node_t *get_node( char *name, int opt );
static void release_node( char *tok );			
static char *get_node_list( char *attrs, char *ubuf, ng_timetype now );	/* get node list that satisfy all attrs into ubuf. if ubuf is null returns strdup'd buf */
static Node_t *get_any_node( Job_t *j, Node_t *np, char *jreq );
static File_t *get_file( char *name, char *md5, char *gar );
static Node_t *map_f2node( Job_t *j, int *reason );	/* find a node having all input files specified by the job */
static Node_t *map_j2node( Job_t *j );			/* find a node that the job can run on, modulo bids */
static Resource_t *get_resource( char *name, int option );
static char *get_path( File_t *f, Node_t *n, char *nname );
static Jobtype_t *get_type( char *name, char *nname, int create );
static long get_cost( Job_t *j );			/* get a submission cost (execution+wait) for a job */
static long get_one_cost( Job_t *j, long size, int cost_type );	/* get a single cost type (execution or wait) for a job */

							/* s_gar.c */
static void gar_files( Job_t *j );			/* generates gar info for a specific job */
static void gar_calc( );				/* runs jobs on missing queue and generates gar info if it can */
static void make_gar( char *buf );			/* parses a gar:name=string buffer sent from nawab (or other user) */
static void clear_gar( Gar_t *g );			/* clears the gar block passed in */
static int add_gar( char *name, Node_t *n, char *f );	/* adds a node/file combo to a gar block */
static void make_gar_job( Node_t *n, char *cmd, char *fstring );		/* creates a gar job to generate n files on a given node */
static void submit_gar( );				/* causes gar jobs to be crated and placed onto the pending queue */

							/* s_files.c */
static int fetch_finfo( );				/* parse a repmgr dump updating the file blocks */
static int req_finfo( );				/* send a dump request to replication manager */
static void dq_file( File_t *f );			/* put a file block back into the available pool */

static void schedule( Node_t *n );		 	/* main workers */
static void comp_job( char *buf, int status, char *misc );
static void lease_chk( ng_timetype now );
static int run_jobs( );

static int test_nattr( char *cur, char *desired );
static void set_nattr( char *nname, char *ats );


/* ------------ someday theses should be moved to a library  ---------------
   the initial design used pthreads and so it was advantagous to keep 
   these things compiled as one unit. As senechal has grown, weve broken
   funcitons into separate files, but still build them as a single unit.
*/

#include	"s_rmif.c"	/* interface to repmgr */
#include	"s_nattr.c"	/* node attribute routines */
#include	"s_mpn.c"	/* max per node functions */
#include	"s_jobs.c"	/* routines that work on job blocks */
#include	"s_dump.c"	/* routines that dump info back to user */
#include	"s_gar.c"	/* routiens that manage garring input files for jobs */
#include	"s_files.c"	/* file block management routines */
#include	"s_fsr.c"	/* filesystem resource list builder */
#include	"s_ckpt.c"	/* checkpoint */
#include	"s_debug.c"


/* ------------------------ general utility routines ------------------------- */
static void usage( )
{
	sfprintf( sfstderr, "seneschal version %s\n", version );
	sfprintf( sfstderr, "usage: %s [-f] [-l directory] [-p port] [-s seconds] [-v]\n", argv0 );
	exit( 1 );
}

static void siesta( int tenths )		/* short rests */
{
        struct timespec ts;

        ts.tv_sec = tenths/10;
        ts.tv_nsec = (tenths%10) * 100000000;	/* convert to nano seconds */

        nanosleep( &ts, NULL );
}

/* expand tokens that fall between cmd woomera  and the second cmd. we expect that 
   cmd woomera is removed from the buffer.  back quotes following cmd are left for 
   expansion as the command is executed.
*/
static char *wexpand( char *buf )		
{
	char	*eb = NULL;				/* expanded buffer */
	char 	*tok;

	if( (tok = strchr( buf, '`' )) && tok < strstr( buf, "cmd" ) )		/* backquote before cmd token */
	{
		if( (eb = ng_bquote( buf )) == NULL )	/* we tested, it should always return something, but be parinoid anyway */
			eb = ng_strdup( buf );
	}
	else
		eb = ng_strdup( buf );		/* nothing back quoted before cmd -- just dup and go */

	return eb;
}

/* daemons should be neither seen nor heard */
static void disappear( )
{
	static int hidden = 0;
	int i;
	char	*ng_root;
	char	wbuf[256];

	if( hidden++ )
		return;            /* bad things happen if we try this again */


	switch( fork( ) )
	{
		case 0: 	break;        		/* child continues on its mary way */
		case -1: 	perror( "could not fork" ); return; 
		default:	exit( 0 );   		 /* parent abandons the child */
	}


	/* it is normal for daemons to change to / but we DO NOT because we have already established	
	   our "home" as NG_SENESCHAL_LAIR 
	*/

	
	sfclose( sfstdin );
	sfclose( sfstdout );
	sfclose( sfstderr );
	for( i = 3; i < OPEN_MAX; i++ )	/* close all open files, including stdin stdout */
			close( i );              
	ng_open012();				/* open stdin/out/err as dev null */

       if( ! (ng_root = ng_env( "NG_ROOT" )) )
        {
                ng_bleat( 0, "cannot find NG_ROOT in cartulary/environment" );
                exit( 1 );
        }

        sfsprintf( wbuf, sizeof( wbuf ), "%s/site/log/seneschal", ng_root );

	if( (bleatf = ng_sfopen( NULL, wbuf, "Ka" )) != NULL )
	{
		sfset( bleatf, SF_LINE, 1 );		/* force flushes after each line */
		ng_bleat_setf( bleatf );	
		ng_bleat( 1, "hide; tucked away now" );
	}
	else
	{
		ng_log( LOG_ERR, "unable to open bleat file: %s: %s", "site/log/seneschal.log", strerror( errno ) );
		exit( 1 );
	}

        ng_free( ng_root );

}

/* parse a buffer and add limits as encountered */
/* syntax: LIMIT:name=value[:name=value...]   (separator is the first character past limit; 
	"limit" is removed before call)

	if option == FORCE, the the limit is reset regardless, otherwise its set only if its -1
*/
static void add_limits( char *buf )
{
	Resource_t	*r;	
	char	*tok;		/* pointer at token in buffer */
	char	*val;		/* value to set resource limit to */
	char 	*strtok_p;
	char	sep[2];		/* user defined separator string from buffer */


	*sep = *buf;		/* pick up separator */
	*(sep+1) = 0;

	tok = strtok_r( buf+1, sep, &strtok_p );	

	while( tok )
	{
		if( val = strchr( tok, '=' ) )		/* separate at =value */
		{
			*val = 0;
			if( r = get_resource( tok, CREATE ) )
			{
				r->limit = atoi( val+1 );
				if( strchr( val+1, 'h' ) )		/* name=2h */
					r->flags |= RF_HARD;
				else
					r->flags &= ~RF_HARD;		/* turn off hard limit */
			}
		}

		tok = strtok_r( NULL, sep, &strtok_p );
	}

	set_targets( );
}


/* removes resource restrictions to that job will run with next bids */
static void force_job( char *name, int push )
{
	Job_t	*j;


	if( ! (j = get_job( name )) )
	{
		ng_bleat( 1, "force: no such job: %s", name );
		return;
	}

	if( j->state == PENDING )	/* can only do pending jobs */
	{
		j->run_delay = 0;

		if( push )
		{
			dq_job( j );
			if( pq )
				j->priority = pq->priority + 10;	/* cause it to land on top */
			q_job( j, PENDING );				/* requeue in a priority state */
			ng_bleat( 1, "force: job pushed to head of queue: %s", j->name );
		}
		else
		{
			deref_rsrcs( j );		/* dereference all resources */
#ifdef DYNAMIC
			j->nrsrc = 0;
			ng_free( j->rsrc );
			j->rsrc = NULL;
#else
			ng_bleat( 1, "force: job resources eliminated: %s", j->name );
#endif
		}
	}
	else
		ng_bleat( 1, "force: job not pending, no action: %s", j->name );
}

/* return the number of outstanding bids across all nodes */
int count_bids( )
{
	Node_t *n;
	int	count = 0;

	for( n = nodes; n; n = n->next )
		count += n->bid;

	return count;
}

/* set bids for a node if their desired work level is greater than
   their actual work level
*/
static void set_bids( )
{
	Node_t	*n;

	for( n = nodes; n; n = n->next )
		if( !n->suspended )
		{
			if( n->desired > n->rcount )		
			{
				n->bid = n->desired - n->rcount;				/* try to keep the node busy */
			}

			total_resources += n->bid + n->rcount;			/* total number of resources */
		}
}

/* calculate the target percentages for each resource */
static void set_targets( )
{
	Resource_t	*r;
	long	sum = 0;			/* sum of all limits */

	for( r = resources; r; r = r->next )		/* first pass - calc sum of limits */
		if( (! r->flags & RF_HARD ) && r->limit > 0 && r->ref_count )	/* only count those soft resources that have jobs referencing them */
			sum +=	r->limit;

	for( r = resources; r; r = r->next )		/* second pass - calc target for each */
	{
		if( r->limit == 0 )
			r->target = 0;
		else
		{
			if( r->flags & RF_HARD )
				r->target = r->limit;				/* hard and fast limit */
			else
				if( sum )
				{
					if( r->ref_count )
						r->target = ((double) r->limit/sum);		/* percentage allowed for this resource */
					else
						r->target = 0;					/* no ref - no target */
				}
				else	
					r->target = .99;
		}

		if( r->target < 0 )
			r->target *= -1;
	}
}

/* called to set all soft resource targets to .98 -- allows us to make a second pass over pending queue
   to sechedule more jobs if there were still bids after a pass with targets set 
*/
static void eliminate_targets( )
{
	Resource_t	*r;

	for( r = resources; r; r = r->next )		/* second pass - calc target for each */
	{
		if( r->limit > 0 )			/* dont change if unlimited, or forced off */
		{
			if( r->flags & RF_HARD )
				r->target = r->limit;				/* hard and fast limit -- always */
			else
				r->target = .98;
		}
	}
}


/* ------------------- main work routines ----------------------- */
/*	run the work queue and deal with the work that was left by the port listener 
	messages that cause work are typically formatted as:
		cmd:string:value:misc-info 
	the netif stuff points string and misc in the work block to the strings, 
	and converts value to an integer in the work block.  The meaning of the 
	contents of each are 'command' dependant.

	As seneschal grew, the original message format described above became
	a hinderance and much of the work blocks are pointers to 'unparsed' 
	buffers.  The ng_sreq interface still puts colons between each token. For
	unparsed messages, the misc pointer points at the buffer. 

	the work->fd can be passed to ng_siprintf() to send messages to the session 
	partner that sent the request.  the work->to the name of the session partner
	which can be saved and used to send stats to later.
*/
static void parse_work( )
{
	Node_t	*n = NULL;
	Work_t	*w = NULL;
	Job_t	*j = NULL;			/* pointer to newly added job */
	Resource_t *r = NULL;
	int	push = 0;		/* flag causes force_job to push job to head of queue */
	int	v;
	int	vreset = verbose;	/* so we can reset if we change verbose to our liking */
	int	max_allowed = max_work;		/* max allowed to do this pass */
	char	*tok;
	char	*strtok_p;		/* pointer for strtok_r calls */

	while( (w = get_work( )) != NULL )
	{
		push = 0;		

		switch( w->type )		
		{
			case STATUS:
				comp_job( w->string, w->value, w->misc );
				break;

			case JOB:	/* w->string has the job submission record; w->to has the name of the session to send stats to*/
				if( strncmp( w->misc, "LIMIT", 5 ) == 0 )
					add_limits( (char *) (w->misc) + 5 );
				else
					if( (j = add_job( w->misc, QUEUE )) )
					{
						if( j->state == COMPLETE || j->state == FAILED )
							send_stats( w->to, j->sbuf ); 		/* resend the stats to this requestor */
						else
							sfsprintf( j->response, sizeof( j->response ), "%s",  w->to );		
					}
				break;

			case IJOB:					/* job record, but its not compete so we cannot queue it */
				if( (j = add_job( w->misc, NO_QUEUE )) )		/* not complete cannot allow it to queue */
				{
					if( j->state == COMPLETE || j->state == FAILED )
						send_stats( w->to, j->sbuf ); 		/* resend the stats to this requestor */
					else
						sfsprintf( j->response, sizeof( j->response ), "%s",  w->to );		
				}
				break;

			case CJOB:					/*  sender has sent all info for the named job; queue it */
				ng_bleat( 3, "parse_work: cjob (%s)", w->misc );
				if( (j = get_job( w->misc )) )
				{
					if( j->state == COMPLETE || j->state == FAILED )
							send_stats( w->to, j->sbuf ); 		/* resend the stats to this requestor */
					else
					{
						if( ! j->username )
							j->username = ng_strdup( "nobody" );
						sfsprintf( j->response, sizeof( j->response ), "%s",  w->to );		
						q_job( j, RESORT );			/* new jobs go to resort queue for disposition */
					}
				}
				else
					ng_bleat( 0, "parse_work: could not match name on cjob msg to job: (%s)", w->misc );
				break;

			case INPUTIG:			/* input, but file size is ignored by scheduling/queuing calc functions */
			case INPUT:					/* input file info to add to job (jobname:stuff) */
				ng_bleat( 3, "parse_work: ijob input file (%s) type=%d ignore=%d", w->misc, w->type, w->type == INPUTIG  );
				if( (tok = strchr( w->misc, ':')) )
				{
					*tok = 0;
					if( (j = get_job( w->misc )) )
					{
						if( j->state == NOT_QUEUED )		/* can add files only if its not queued */
							add_infiles( j, tok+1, w->type == INPUTIG );
					}
					else
						ng_bleat( 0, "parse_work: unable to match input msg with job: (%s)", w->misc );

				}
				break;

			case OUTPUT:					/* output file info to add to job */
				ng_bleat( 3, "parse_work: ijob output file (%s)", w->misc );
				if( (tok = strchr( w->misc, ':')) )
				{
					*tok = 0;
					if( (j = get_job( w->misc )) )
					{
						if( j->state == NOT_QUEUED )		/* can add files only if its not marked complete (queued) */
							add_outfiles( j, tok+1 );
						ng_bleat(5, "addo: noutf=%d", j->noutf);
					}
					else
						ng_bleat( 0, "parse_work: unable to match output msg with job: (%s)", w->misc );
				}
				break;

			case USER:					/* user info for the incomplete job named */
				ng_bleat( 3, "parse_work: user data (%s)", w->misc );
				if( (tok = strchr( w->misc, ':')) )			/* past job name */
				{
					*tok = 0;
					if( (j = get_job( w->misc )) )
					{
						if( j->username )
							ng_free( j->username );
						j->username = ng_strdup( tok+1 );
					}
					else
						ng_bleat( 0, "parse_work: unable to match user data msg with job: (%s)", w->misc );
				}
				break;

			case NODE:
				assign_node( w->misc );			/* assume jobname:nodename */
				break;

			case LOAD:						/* try to keep this load on the sending node */
				if( w->string && w->misc )
				{
					if( (n = get_node( w->string, CREATE )) )
					{
						ng_bleat( n->suspended ? 0 : 1, "parse_work: load request of %d from %s %s", 
							w->value, w->string, n->suspended ? "(takes effect when node suspension lifted)" : "" );
						n->desired = w->value;	
						if( n->bid > n->desired )
							n->bid = n->desired;		/* new limit brings us down */
						schedule_ckpt();			/* force a chkpt at next opportunity */
					}
				}
				else
					ng_bleat( 1, "parse_work: error processing load: %x(w->string) %x(w->misc) %x(r)", w->string, w->misc, r );
				break;

			case BID:
				if( w->string && (n = get_node( w->string, CREATE )) )
				{
					if( n->suspended )
						ng_bleat( 1, "parse_work: bid of %d from %s not accepted; node suspended", w->value, w->string );
					else
					{
						ng_bleat( w->value ? 1:3, "parse_work: bid of %d from %s", w->value, w->string );
						total_resources += w->value - n->bid;	/* more resources to allocate */
						n->bid = w->value;	
					}	
				}
				break;

			case LIMIT:
				if( w->string && w->misc && (r = get_resource( w->string, CREATE )) != NULL )
				{
					if( ! strchr( w->string, '=' ) )		/* ignore it if they did a sreq limit xxxx=10 */
					{
						if( strchr( w->misc, 'n' ) )		/* 10n max/node */
						{
							if( r->mpn <= 0 )			/* moving up from unlimited */
								mpn_init_counts( r );	/* must set counters to current number running on each node */
							r->mpn = w->value;			/* set the new value */
						}
						else
						{
							r->limit = w->value;
							if( strchr( w->misc, 'h' ) )
								r->flags |= RF_HARD;
							else
								r->flags &= ~RF_HARD;
		
							set_targets( );					/* adjust all targets */
						}
					}
					else
						ng_bleat( 1, "parse_work: bad limit: %d(value) %s(resource)", w->value, w->string );
				}
				else
					ng_bleat( 1, "parse_work: error processing limit: %x(w->string) %x(w->misc)", w->string, w->misc );
				break;

			case MPN:				/* max jobs of this resource type per node */
				if( w->string && w->misc && (r = get_resource( w->string, CREATE )) != NULL )
				{
					if( ! strchr( w->string, '=' ) )		/* ignore it if they did a sreq mpn xxxx=10 */
					{
						if( r->mpn <= 0 )			/* moving up from unlimited */
							mpn_init_counts( r );	/* must set counters to current number running on each node */
						r->mpn = w->value;			/* set the new value */
					}
					else
						ng_bleat( 1, "parse_work: bad limit: %d(value) %s(resource)", w->value, w->string );
				}
				else
					ng_bleat( 1, "parse_work: error processing mpn: %x(w->string) %x(w->misc)", w->string, w->misc );
				break;

			
			case DUMP:
				wallace( w );		/* vomit something back to the user */
				w = NULL;		/* wallace() will dispose of w when needed (or requeue it) */
				break;

			case EXTEND:							/* extend lease, string should have job name */
				if( w->string )
					extend_lease( w->string, w->value );
				break;
	
			case NATTR:				/* receive a node's attributes */
				if( w->string )
					set_nattr( w->string, w->misc );	
				break;

			case DELETE:
				del_job( w->misc );
				break;

			case EXPLAIN:
				explain_all( w );
				w = NULL;					/* explain_all takes care, dont trash later */
				break;

			case GAR:
				make_gar( w->misc );		/* gar is added like a job - unparsed - so misc has what we want */
				break;
	
			case PUSH:				/* push to head of queue */
				push = 1;			/* only difference between this and run, fall into run */

			case RUN:				/* remove resource limits to make runable */
				if( w->string )
					force_job( w->string, push );			/* removes resource restrictions */
				break;
				
			case PAUSE:
				suspended = ! suspended;
				ng_bleat( 2, "suspended value reset; new value is: %d", suspended );
				schedule_ckpt();			/* force a chkpt at next opportunity */
				break;

			case SUSPEND:		/* expect: SUSPEND:n  || SUSPEND:all:n || SUSPEND:<node>:n */
				if( w->string )
				{
					if( strcmp( w->string, "all" ) == 0 )
						suspended = w->value;
					else
					{
						if( n = get_node( w->string, NO_CREATE ) )
						{
							n->suspended = w->value;
							if( w->value == 0 )				/* user sent a resume */
								n->flags &= ~ NF_RELEASED;		/* turn off release */
						}
					}

					schedule_ckpt();			/* force a chkpt at next opportunity */
				}
				break;

			case LISTFILES:			/* send a list of files via tcp to the to address in work */
				if( w->fd >= 0 )			/* its possible the sesion disco'd while waitin in queue */
				{
					tcp_dump_files( w->fd );
				}
				break;

			case NODECOSTS:			/* send a list of job type costs for nodes back to tcp */
				if( w->fd >= 0 )			/* its possible the sesion disco'd while waitin in queue */
					wallace_type( w->fd );
				break;

			case RELEASE:		
				ng_bleat( 3, "parse_work: release (%s)", w->misc );
				if( (tok = strtok_r( w->misc, ":", &strtok_p )) )			/* type:name */
				{
					if( strncmp( tok, "res", 3 ) == 0 )	/* res[ource]:name */
					{
						if( tok = strtok_r( NULL, ":", &strtok_p ) )
						{
							del_resource( tok );
							ng_siprintf( w->fd, "resource now released: %s\n.\n", tok );
						}
					}
					else
					if( strncmp( tok, "job", 3 ) == 0 )
					{
						if( tok = strtok_r( NULL, ":", &strtok_p ) )
						{
							if( obstruct_job( tok, 0 ) )
								ng_siprintf( w->fd, "job now released: %s\n.\n", tok );
							else
								ng_siprintf( w->fd, "release attempt failed: %s\n.\n", tok );
						}
					}
					if( strncmp( tok, "node", 4 ) == 0 )
					{
						if( tok = strtok_r( NULL, ":", &strtok_p ) )
						{
							release_node( tok );
							ng_siprintf( w->fd, "node now released: %s\n.\n", tok );
						}
					}
					else
					{
						ng_bleat( 0, "parse_work: unknown release type: (%s)", w->misc );
						ng_siprintf( w->fd, "release bad syntax? release {resource|job|node} <name>\n.\n" );
					}
				}
				break;

			case OBSTRUCT:		/* block a job until it is released */
				if( w->string && obstruct_job( w->string, 1 ) )
					ng_siprintf( w->fd, "job now blocked: %s\n.\n", w->string );
				else
					ng_siprintf( w->fd, "block attempt failed: %s\n.\n", w->string );
				break;

			case RMBUF:			/* parse the buffer that was received from repmgr */
				parse_rm_buf( w->misc );
				break;

			default:
				ng_bleat( 0, "parse_work: unknown work block type on queue: %d", w->type );
				break;
		}

		if( w )
			trash_work( w );			/* and it only took seconds to blow him away */


		if( --max_allowed < 1 )
		{
			gflags |= GF_WORK;			/* quick check of tcp stuff while work blocks are pending */
			return;
		}
	}

	gflags &= ~GF_WORK;		/* indicate we did not stop short */
	return;
}


/* parse a buffer and add a job to the pending queue */
/* syntax: <[type_]name>:<max-attempts>:<cost>:<priority>:<resources>:{*|node}:{*|<input file/md5 pairs>}:{*|<outputfiles>:<command> */
/* we allow the first character to be a token separator or the first character in the job 
	name. We also ignore any lead whitespace, and any record that has no tokens
	or has a # character as the first character of the first token.
	if the first character of the first token is a legit jobname character, 
	then the first unrecognised character is used as the field separator. this
	makes the logic a bit more messy, but provides sed like flexibility
*/


/*char **ng_tokenise(); */
#define NEXT(t,p)	if( ((t)=strtok_r((p),fs,&strtok_p)) == NULL ) { \
			ng_bleat( 0, "bad job submission record: %s", buf ); if( j ) ng_free( j ); return NULL; }

static struct Job_t *add_job( char *buf, int qflag )
{
	Syment	*se = NULL;	/* for job lookup */
	int	tcount;		/* number of tokens in the record */
	char	**tlist;	/* list of tokens from buffer */
	char	*tok;		/* pointer at stuff in buffer */
	char	*etok;		/* pointer at end of token (+1) */
	Job_t	*j = NULL;	/* new job block */
	char	*strtok_p;	/* pointer for strtok */
	char	*sep;
	char	fs;		/* field separator */
	char	esc = '\\';	/* escape character */
	int	p;		/* tmp holder for priority */
	int	i;

	if( ! buf || ! *buf )
		return NULL;

	tok = buf;
	while( isspace( *tok ) )
		tok++;
	if( ! *tok || *tok == '#' )			
		return NULL;					/* blank or comment record; skip */

	ng_bleat( 6, "add_job: (%s)", buf );

	if( !( isalnum( *tok ) || *tok == '_' ) || *tok == '.' )	/* test 1st non blank; if not a legal C identifier character  and .*/
	{
		fs = *tok;				/* then assume it is the field separator */
		tok++;					/* setup for ng_tokenise call */
	}
	else						/* find field sep as the first unrecognised character */
	{
		etok = tok + 1;
		while( etok && (isalnum( *etok )  || *etok == '_' || *etok == '.' ) )
			etok++;
		fs = *etok;
	}

	if( fs != ':' && !(gflags & GF_SEP_ALARM) )
		ng_log( LOG_WARNING, "job file separator was not a colon. found '%c'", fs );


	tcount = 0;
	tlist = ng_tokenise( tok, fs, esc, NULL, &tcount );	/* make token list */

	if( tlist == NULL || tcount < REQ_JOB_TOKENS ) 
	{
		ng_bleat( 0, "add_job: bad submission record: missing tokens: (%s)", buf );
		ng_free( tlist );
		return NULL;
	}

	if( tlist[CMD_TOK] == NULL )
	{
		ng_bleat( 0, "add_job: bad submission record: no command found: (%s)", buf );
		ng_free( tlist );
		return NULL;
	}

	tok = tlist[NAME_TOK];
	if( se = symlook( symtab, tok, JOB_SPACE, 0, SYM_NOFLAGS ) )	/* already have this job */
	{
		j = se->value;
		switch( j->state )
		{
			case MISSING:
			case PENDING:	
					break;			/* we allow them to be replaced if not done or running */

			default:				/* all others cannot be replaced */
				j->mod = ng_now( );			/* only reset the modification time */
				ng_bleat( 4, "add_job: duplicate job ignored; not queued on pending/missing: %s %d(state)", tok, j->state );
				ng_free( tlist );
				if( j->state == COMPLETE || j->state == FAILED )
					return j;				/* let the caller resend stats */
				return NULL;				/* running or housekeeping jobs dont send stats */
		}

		ng_bleat( 5, "add_job: duplicate (q=pending/missing) job replaced: %s", tok );
		dq_job( j );		/* we will requeue it as its priority may change */
		wash_job( j );		/* trash some of the things we will shove in from the new job record */
	}
	else
	{
		j = new_job( tok );		/* create it */
		j->pri_bump = ng_now( ) + PRI_BUMP_BASE;
		ng_log( LOG_INFO, "%s %s %s(job)", AUDIT_ID, AUDIT_NEWJOB, tok );
	}

	j->mod = ng_now( );			/* track the update to the job should we need it */


	j->max_att = tlist[ATTEMPT_TOK] ? atoi( tlist[ATTEMPT_TOK] ) : 5;		/* default to 5 max attempts */
	j->size = tlist[SIZE_TOK] ? atol( tlist[SIZE_TOK] ) : 600;			/* default to 10 minutes cost */
	j->cost = get_cost( j );
	j->priority = tlist[PRIORITY_TOK] ? atoi( tlist[PRIORITY_TOK] ) : 20;
	j->max_pri = j->priority +15;			/* we also cap at 90 when we try to bump, so this might not always hold */
	if( (j->ninf_need = atoi(tlist[NINF_NEED_TOK])) > 0 ) /* number of input files needed to make job runable; 0 == all are needed (def) */
	{
		j->flags |= JF_VARFILES;			/* indicate that we can run with less than all files */
	}

	add_resources( j, tlist[RESOURCE_TOK] );
	add_outfiles( j, tlist[OUTFILE_TOK] );
	add_infiles( j, tlist[INFILE_TOK], 0 );

	if( tlist[NODE_TOK]  )
	{
		if( *(tlist[NODE_TOK]) == '!' )
		{
			j->flags |= JF_ANYNODE;						/* any node except that named here */
			j->notnode = get_node( tlist[NODE_TOK]+1, CREATE );
		}
		else	
		{
			if( *(tlist[NODE_TOK]) != '*' &&  *(tlist[NODE_TOK]) != '{' && *tlist[NODE_TOK] != 0 )
				j->node = get_node( tlist[NODE_TOK], CREATE );
			else
			{

				if( *(tlist[NODE_TOK]) == '{' )		/* attributes expected as { Leader !Satellite } */
				{
					j->nattr = ng_strdup( tlist[NODE_TOK]+1 );
					if( etok = strchr( j->nattr, '}' ) )
						*etok = 0;

					ng_bleat( 3, "job %s requires node with attributes: %s", j->name, j->nattr );
				}
				else
					j->nattr = NULL;		
				j->flags |= JF_ANYNODE;
			}
		}
	}
		
	/* we cannot actually use the cmd token as the command is literal and might have had field separators in it 
	   causing it to be fragmented. Thus we 'seek' in the buffer until we find it
	*/
	j->ocmd = j->cmd = NULL;
	tok = buf;
	if( *tok == fs )
		tok++;			/* dont count lead separator in this */
	for( i = 0; i < CMD_TOK && (tok = strchr( tok+1, fs )); i++ );
	if( tok )
		j->ocmd = ng_strdup( tok+1 );


	/* all done parsing */

	if( qflag )			/* allowed to queue it? */
	{
		q_job( j, RESORT );			/* new jobs go to resort queue for disposition */
		gflags |= GF_NEWJOBS;					/* makesure we pick up file info */
	}

	if( ! se )				/* was not in the symtab - add it */
		symlook( symtab, j->name, JOB_SPACE, j, SYM_NOFLAGS );	
	

	ng_free( tlist );
	return j;
}


/*	tries to start all of the pending jobs. a job can be started if its assigned node
	has open bids, or any of the nodes that its affected file is on has open bids.
 	if a pending jobs mod time is less than the last read time, then it is purged as 
 	it was not updated during the last read.
	returns the highest priority that was submitted.
*/
static int run_jobs( )
{
	Job_t	*j;		/* pointer at the job it is working with */
	Job_t	*next;		
	Job_t	*ip;		/* insertion point in the queue to keep sorted */
	Node_t	*n = NULL;
	int	pcount = 0;
	int	moved = 0;
	int	ncount = 0;
	int	highp = -1;		/* highest priority submitted */
	long	dropcount = 0;		/* purge count */
	ng_timetype	now;

	now = ng_now( );
	deadlock = 0;				/* reset deadlock counter before we look at the pending queue */
	
	ng_bleat( 4, "run_jobs: start queue traverse" );
	for( j = pq; j; j = next )
	{
		next = j->next;			/* save now in case we move the block */

		if( priority_creep && now > j->pri_bump && j->priority < j->max_pri && j->priority < MAX_PRI_CREEP )
		{
			j->priority++;
			j->pri_bump = now + PRI_BUMP_BASE;
		}


		pcount++;

		if( j->flags & (JF_GAR + JF_HOUSEKEEPER) )	/* setup here is same for both, schedule treats differently */
		{
			if( j->run_delay <= now )		/* key to delay reruns cuz they are likely to have failed b/c file not in repmgr */
			{
				j->cost = get_cost( j );
				n = j->node;
				if( n->bid > 0 )
					n->bid--;
				j->lease = now + 9000;  		/* lease time fixed here -- wait as long as s_mvfiles waits for repmgr dumps*/
				dq_job( j );				/* off pending queue */
				q_job( j, RUNNING );			/* consumes the resources while adding to queue */
	
				n->jobs[n->jidx++] = j;			/* add this job to the batch */
				j->started = now;
				if( n->jidx >= BATCH_SIZE )
					schedule( n );			/* send the batch to woomera */
				moved++;
				ng_bleat( 4, "run_jobs: housekeeping job %s will be scheduled on node: %s %d(bid) %ld(cost) %d(%x)", 
							j->name, n->name, n->bid, j->cost, n->jidx-1, n->jobs[n->jidx-1] );
			}
		}
		else
		{
			/* job not blocked, rundelay is not in future, resources OK and we can find a node with bids... */
			if( !(j->flags & JF_OBSTRUCT) && j->run_delay <= now && (n = map_j2node( j )) && n->bid > 0 )		
			{
				if( ! j->node )
					j->node = n;			/* ensure its there */
				j->cost = get_cost( j );		/* must set node before recalcing cost */
	
				if( j->priority > highp )
					highp = j->priority;

				n->bid--;				/* one less slot for this node */
				mpn_adjust_all( j, n, 1 );		/* one more running on the node for resources with an mpn value */

				j->started = ng_now( );

				/*	cost is size of job * tsec/size average for job type as calculated by
					get_cost()  [tsec == tenths]. cost includes an estimated woomera queue
					wait time for the job type and thus is the minimum amount of time that 
					we expect to elapse between scheduling and status.  the lease is 
					at least 2c and is increased for each attempt.
				*/
				j->lease = j->started + ((j->cost * 2) * (j->attempt + 2));	/* attempt may still be -1 at this point */
				if( j->attempt >= (j->max_att - 1) )
					j->lease +=  36000;	/* try to avoid a lease exp on the last go round */

				ng_bleat( 2, "run_jobs: %s(name) %ld(unadj-lease-tsec)", j->name, j->lease - j->started );
				if( j->lease < j->started + min_lease_sec )		/* 30 min minimum lease for now */
					j->lease = j->started + min_lease_sec;
				else
					if( j->lease > j->started + max_lease_sec )	/* too much time we think */
						j->lease = j->started + max_lease_sec;
		
				dq_job( j );			/* off pending queue */
				q_job( j, RUNNING );		/* consumes the resources while adding to queue */

				n->jobs[n->jidx++] = j;		/* add this job to the batch */
				if( n->jidx >= BATCH_SIZE )
					schedule( n );			/* send the batch to woomera */
				moved++;

				ng_bleat( 4, "run_jobs: %s will be scheduled on node: %s %d(bid) %ld(cost) %ld(lease) %d(%x)(jobblk)", 
						j->name, n->name, n->bid, j->cost, j->lease - j->started, n->jidx-1, n->jobs[n->jidx-1] );
			}
			else
			{
				ng_timetype d = 0;
				if( j->run_delay > 0 )
					d = j->run_delay - now;
	
				ng_bleat( 5, "run_jobs: not run: %s(job) %I*ds(run_delay) flags=0x%04x ", j->name, Ii( d ), j->flags );
			}
		}
	}
	ng_bleat( 4, "run_jobs: end queue traverse" );

	for( n = nodes; n; n = n->next )
		if( n->jidx )
		{
			schedule( n );		/* not a full batch, but need to send out as their leases are ticking  */
			ncount++;
		}

	if( dropcount )
		ng_bleat( 2, "run_jobs: %d jobs dropped -- ancient mod time", dropcount );

	if( moved )
	{
		ng_bleat( 3, "run_jobs: %d(pcount) %d(moved) %d(nodes)", pcount, moved, ncount );
		if( verbose > 3 )
			wallace_rsrc( -1, NULL );
		if( verbose > 2 )
			wallace_sum( -1, 0, 0 );
	}

	return highp;
}

/* called when notified of a job completion */
/* if status is success (0) then move to completed code 
   if the status is bad, and the report is for the most recent attempt, then we will reschedule
   the job if max attempts have not been exceeded. if max attempts are exceeded then the job 
   is failed and reported to nawab.  if the report is not for the last attempt, then we ignore
   it as we have already dealt with the job from an expiration perspective.
 */  
static void comp_job( char *buf, int status, char *misc )
{
	Node_t	*n;
	Job_t	*j;
	long	exec_time = 0;		/* tents of seconds that the job took after woomera started it */
	int 	attempt;		/* attempt according to the notify message (jobname)*/
	char	*tok;
	char	*nname;			/* node name returning status */
	char 	*strtok_p;		/* pointer cache for strtok */
	char	*jname;			/* job name without attempt number */
	char	sbuf[4096];		/* status buffer */
	char	*ch;
	ng_timetype now;

	if( !buf || ! *buf )
		return;

	now = ng_now( );				/* time for lease expiration */

							/* buf syntax: <jobname> <attempt> <node> */
	jname = strtok_r( buf, " ", &strtok_p );		/* at job name */
	tok = strtok_r( NULL, " ", &strtok_p );			/* point at attempt value */
	if (tok == NULL) {
		ng_bleat(0, "comp_job: abort: no 2nd token in buf for: %s(jname)", jname);
		return;
		exit(1);
	}
	attempt = atoi( tok );
	nname = strtok_r( NULL, " ", &strtok_p );		/* node name */
	if (nname == NULL) {
		ng_bleat(0, "comp_job: abort: no 3rd token for: %s(jname) %d(att)", jname, attempt );
		return;
		exit(1);
	}


	if( (j = get_job( jname )) )		/* job exists as far as we know */
	{
		exec_time = misc ? atol( misc ) : now - j->started;	/* n(real) m(sys) o(user)  -- we get real as actual exec time */

		ng_log( LOG_INFO, "%s %s %s(job) %s(user) %s(node) %d(status) %d(att) %I*d(size) %I*d(isize) %ld(elapsed) %ld(queued) %s", AUDIT_ID, AUDIT_STATUS, jname, j->username, nname, status, attempt, Ii(j->size), Ii(j->isize), (long) (now - j->started), (long) ((now - j->started) - exec_time), misc ? misc : "-1(real) -1(usr) -1(sys)" );

		ng_bleat( 2, "comp_job: got status for: %s(job) %s(user) %s(node) %d(status) %d(att) %ld(tremain) %s", jname, j->username, nname, status, attempt, (long) (j->lease - now), misc ? misc : "" );

		if( !(n = j->node) )				/* could be null if lease popped and its sitting on the pending q */
		{
			n = get_node( nname, NO_CREATE );	/* so, find it based on the name in status */
		}

		if( status )	/* failed; dont schedule retry unless its from the last one submitted AND is running */
		{

			if( j->state == RUNNING &&  attempt == j->attempt )	/* failure was for the last time we scheduled */
			{
				dq_job( j );				/* remove from the queue */

				if( j->prime )				/* for housekeepers */
					j->prime->hexit = status;	/* primary job tracks our status */

				if( attempt >= j->max_att -1 )		/* too many tries on this one (attempt 0 based) */
				{
					ng_bleat( 1, "comp_job: failing job: max attempts exceeded: %s %s(user) %d(attempt) %s(failed_on)", 
							j->name, j->username, attempt, nname );
					sfsprintf( sbuf, sizeof( sbuf ), "%s:%s:%d:\n", j->name, nname, status );

					if( j->prime )
					{
						release_job( j );		/* this cleans it all up now */
					}
					else
					{
						j->sbuf = ng_strdup(  sbuf );	/* maintain status info while on the complete queue */
						j->lease = now + DEL_LEASE;	/* removed from complete queue after a while */
						deref_files( j );		/* refrences to files/resources are pointless now */
						deref_rsrcs( j );		
						j->exit = status;
						q_job( j, FAILED );		/* onto complete queue as a failed job */
					
						send_stats( j->response, sbuf ); 
					}
					
				}
				else
				{
					if( j->flags & JF_ANYNODE )		/* need to turn off node */
						j->node = NULL;			/* so it is reselected next try */

					if( (j->run_delay =((j->attempt+1) * j->delay)) < 3000 )	/*  delay restart to let jello set */
						j->run_delay += now;
					else
						j->run_delay = now + 3000;		/* not more than 5 minute delay please */
					j->lease = -1;
					q_job( j, PENDING );		/* return it to the pending queue for retry */

					ng_bleat( 2, "comp_job: rescheduled: %s %s(user) %d(attempt) %s(failed_on) %d(after-tsec)", j->name, j->username, attempt, nname, (long) (j->run_delay - now) );
				}
			}					/* ignore failures if job not on running queue */
			else
				ng_bleat( 2, "failure status for wrong attempt/not running: %s(job) %d(s_att) %d(j_att) %d(state)", j->name, attempt, j->attempt, j->state );
		}
		else			/* accept good status from whom ever ran the thing */
		{
			if( j->state == PENDING  || j->state == RUNNING )	/* if its back on pending, then lease probably popped */
			{
				dq_job( j );			/* take off the queue */


				if( j->prime )				/* for housekeepers */
				{
					record_cost( j, exec_time );
					j->prime->hexit = status;	/* primary job tracks our status */
				}

				if( j->prime )			/* this is an expunge job  */
					release_job( j );	/* release status of primary job that was on hold for this one */
				else				
				{
					j->lease = now + DEL_LEASE;	/* ensure it falls off the complete queue eventually */
					record_cost( j, exec_time );		/* mark down the cost of this job to better learn */

					j->exit = status;
					deref_rsrcs( j );		/* no longer need to reference resources */

					sfsprintf( sbuf, sizeof( sbuf ), "%s:%s:%d:%s\n", j->name, nname, j->exit, misc == NULL ? "" : misc );

					ng_log( LOG_INFO, "%s %s %s(job) %s(user) %s(node) %d(status) %d(att)", AUDIT_ID, AUDIT_COMPLETE, jname, j->username,  nname, status, attempt );
					if( j->outf && j->outf[0] )			/* if named output files, we must hold for expounge */
						hold_job( j, attempt, nname, sbuf );
					else				/* we can clean it completely up and be done */
					{
						deref_files( j );
						q_job( j, COMPLETE );		/* to final resting place */

						j->sbuf = ng_strdup( sbuf );
						send_stats( j->response, sbuf ); 
						ng_bleat( 2, "comp_job: job done: %s %s(user) %d(attempt) %s(node) %d(status)", j->name, j->username, attempt, nname, status );
					}
				}
			}
		}

	}
	else
		ng_bleat( 1, "comp_job: got status for unknown job: %s(name) %s(node) (%d)(status) %d(att)", jname, nname, status, attempt );
}


/* look for fname, if there: move and then load jobs from the file */
/* returns true if jobs loaded  */
static int load_jobs( char *fname )
{
	Sfio_t	*f;
	char	*buf;
	char	tfname[255];		/* temp name to move to */
	int	rcount = 0;		/* record counter */

	gflags &= ~GF_SEP_ALARM;		/* turn off from last load if it was on */

	if( (buf = strrchr( fname, '/' )) )		/* find last / */
	{
		strncpy( tfname, fname, (buf-fname) );
		tfname[buf-fname] = 0;
		strcat( tfname, "/reading.jobs" );
	}
	else
	{
		tfname[0] = 0;
		strcat( tfname, "reading.jobs" );
	}

	if( rename( fname, tfname ) == 0 )			/* if fname even existed */
	{

		if( (f = ng_sfopen( NULL, tfname, "Kr" )) )
		{
			ng_bleat( 3, "load_jobs: loading." );

			while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
			{
				rcount++;
				ng_bleat( 5, "load_jobs: read (%s)", buf );
				if( strncmp( buf, "LIMIT", 5 ) == 0 )
					add_limits( buf+5 );
				else	
					add_job( buf, QUEUE );
			}

			ng_bleat( 3, "load_jobs: loaded %d jobs", rcount );
			sfclose( f );
			unlink( tfname );
		}
		else
			ng_bleat( 0, "load_jobs: cannot open job input file: %s: %s", tfname, strerror( errno ) );

		ng_bleat( 6, "load_jobs: read %d records from job file", rcount );
	}
	else
		if( errno == 2 )
			ng_bleat( 5, "load_jobs: no job file" );
		else
			ng_bleat( 0, "load_jobs: error moving job file %s --> %s: %s", fname, tfname, strerror( errno ) );

	return rcount;
}

/*
   for running jobs that are about to have a lease check, dispatch a sherlock process that 
   susses out the job state and causes somekind of report back to us.  (the lease will 
   remain the same and if sherlock does not send a status, or extend the lease, the job
   will fail with a lease check; assuming the remote node is down and sherlock cannot run
   or somesuch.)
*/
void investigate( Job_t *j )
{
	char	cmd[NG_BUFFER];
	char	token[NG_BUFFER];
	int	clen = 0;
	int 	i;

						/* -L turns on the actions taken by sherlock! */
	clen = sfsprintf( cmd, sizeof( cmd ),  "ng_wrapper --detach ng_rcmd %s ng_s_sherlock -L -a %d -b -j  %s -w %s ", j->node->name, j->attempt, j->name, j->wname );
	for( i = 0; i < j->oidx; i++ )
	{
		clen += sfsprintf( token, sizeof( token ), "-o %s ", j->olist[i] );		/* add output file */
		if( clen < sizeof( cmd ) )
			strcat( cmd, token );			/* add only if it will fit */
	}

	ng_bleat( 1, "investigation starts: %s", cmd );
	system( cmd );
}

/* scoot down the queues checking for expired leases. */
/* we assume that the queues are sorted in lease expiration order so we 
   can stop our search with the first that has not expired.
   infrequently, we also run the completed queue and scrap deadwood from 
   there.  As the lease times are fairly long on that queue, we dont need 
   to do this very often in order to "keep up."
*/
static void lease_chk( ng_timetype now )
{
	Node_t		*n;
	Job_t		*j;
	Job_t		*next;
	char		buf[1024];		/* buffer to build dummy status things in */
	int		state;			/* job state (trashed when we pull it from the queue */
	long		ccount=0;		/* count of complete jobs purged */
	int		max;			/* max we will do at a time */

	now = ng_now( );

	for( n = nodes; n; n = n->next )   /* this is the only queue not sorted by lease, so we have to check all. queue should be short */
	{
		if( n->lease && n->lease < now )	
		{
			if( n->desired < 0 )
				n->desired *= -1;	/* turn it round to face right side out and we begin to send jobs to it now */
			n->lease = 0;			/* no more lease to check */
		}
	}

	for( j = rq; j && j->lease < now; j = next )
	{
		next = j->next;				/* putting j back on queue will reset its next! */
		ng_bleat( 2, "lease_chk: lease on running job expired: %s %s(node) %d(att) %ld(age) %d(cost)", 
					j->name, j->node->name, j->attempt, (long) (now - j->started), j->cost );	/* age is in tsec now */

		sfsprintf( buf, sizeof( buf ), "%s %d %s", j->name, j->attempt, j->node->name );  /* simulated status msg */
		comp_job( buf, LEASE_ERROR, NULL);		/* fail the job */
	}

	for( j; j && j->lease - now < 3000; j = j->next )		/* if short lease, then dispatch sherlock to suss its state out */
	{
		if( ! (j->flags & JF_IPENDING) )			/* not already under investigation */
		{
			j->flags |= JF_IPENDING;
			investigate( j );				/* dig into the state of this job (sherlock) */
		}
	}


	max = 100;					/* job purge (lots of frees) can take significant time; limit these */
	for( j = cq; max > 0 && j && j->lease < now; j = next )	/* in expiry order, so ok to check every time rather than on timer */
	{
		next = j->next;		/* j->next will be void after it gets trashed */
		state = j->state;	/* must use this as j->state trashed when we pull it from the queue */
		dq_job( j );		/* pull from the queue */

		if( state == DETERGE )			/* must leave until scrubbing is done  -- these should be rare */
		{
			j->lease = now + DEL_LEASE;	/* extend lease, and requeue */
			q_job( j, DETERGE );		/* put back on the queue in proper order */
			ng_bleat( 1, "lease_chk: reset lease for job in deterge state: %s(name)", j->name );
		}
		else
		{
			ccount++;
			ng_bleat( 5, "lease_chk: lease on completed job has expired: %s %d(state)", j->name, state );
			symdel( symtab, j->name, JOB_SPACE );
			trash_job( j );
		}
	}

	if( ccount )
		ng_bleat( 3, "lease_chk: %d jobs purged from complete queue", ccount );
}


/* 
	schedule the commands with woomera. Commands are 'trailed' by commands to 
	capture the command exit code, and then to send it back to us via a udp 
	message.  
	The syntax of the message is driven by the port_listener funciton.
	The commands are roughly:
		rc <- command exit status
		echo status string with exit code to ng_sendudp for transmission
		pass $rc to seoj; seoj will exit with this return code  (so that woomera marks the job with the correct exit status)
*/
static void schedule( Node_t *n )
{
	static  long wj_id_seq = 0;		/* woomera job names are formed with a timestamp and sequence number; this is the seq */
	static  ng_timetype wj_id_ts = 0;	/* timestamp used -- the time of the first job we queue */
	

	Job_t	*j;
	Job_t	*pj;			/* primary job for which a housekeeper is scheduled */
	char	*wrk;			/* working command buffer */
	char	*wptr;			/* pointer at cmd woomera if in command buffer */
	int 	i;
	int	k;
	char	buf[NG_BUFFER];
	char	jbuf[100];		/* woomera job name temp buffer */
	char	*tok;			/* pointer into command buffer */
	Sfio_t	*f;

	if( ! wj_id_ts )
		wj_id_ts = ng_now( );		/* simple id number -- we will add 1 for each job */

	sfsprintf( buf, sizeof( buf ), "ng_wreq -t 8 -s %s", n->name );		/* 8 might be too long, but we give it a chance */
	if( ! (f = sfpopen( NULL, buf, "w" )) )
	{
		ng_bleat( 0, "schedule: cannot open pipe to wreq: %s", strerror( errno ) );
		return;
	}

	for( i= 0; i < n->jidx; i++ )
	{
	 	if( (j = n->jobs[i]) != NULL )
		{
			j->flags &= ~JF_IPENDING;		/* reset investigation pending flag in case this is a restart */
			j->attempt++;

			if( j->flags & JF_HOUSEKEEPER )		/* need to build housekeeping command */
			{
				pj = j->prime;
				sfprintf( f, "job : pri 100 cmd ng_s_mvfiles -j %s %d", j->name, pj->attempt );
				for( k = 0; k < pj->noutf; k++ )
					if( pj->olist[k] )
						sfprintf( f, " %s =%s", pj->olist[k], pj->rep[k] ? pj->rep[k] : "d" );		/* files on the cmd line */
				sfprintf( f, "; rc=$?; ng_s_eoj %s %d $rc $SECONDS \"`times`\"; exit $?\n", j->name, j->attempt );
			
				ng_bleat( 2, "schedule: %s(job) %s(node) %d(att)", j->name, j->node->name, j->attempt );  
				ng_log( LOG_INFO, "%s %s %s(job) %s(node) %d(att)", AUDIT_ID, AUDIT_SCHED, j->name, j->node->name, j->attempt );

			}
			else
			if( j->flags & JF_GAR )		/* need to build gar command */
			{
				sfprintf( f, "job : pri 100 cmd %s; rc=$?; ng_s_eoj %s %d $rc $SECONDS \"`times`\"; exit $?\n", j->cmd, j->name, j->attempt );
				ng_bleat( 2, "schedule: %s(job) %s(node) %d(att)", j->name, j->node->name, j->attempt );  

			}
			else			/* schedule a normal job */
			{
				ng_free( j->cmd );		/* must rebuild command incase job changes nodes */
			
				if( (wptr = strstr( j->ocmd, "cmd woomera")) != NULL )		/* if cmd woomera RESOURCES.... cmd real command */
				{
					j->cmd = wexpand( wptr + 12 );		/* expand backquotes between woomera and cmd */	
				}
				else
					j->cmd = ng_strdup( j->ocmd );		/* pick up %i/%o things if we are reattempting */

					/* replace %F targets in the command */
				if( (j->outf && j->outf[0]) || (j->flist && j->flist[0]) ){
					wrk = freplace(n->name, j->cmd, j->flist, j->ninf, j->olist, j->noutf);
					ng_free(j->cmd);
					j->cmd = wrk;
				}

				if( j->outf )		/* if output files */
				{
					wrk = vreplace( j->cmd, "%o", j->olist, j->noutf );	/* put out files into the cmd */
					ng_free( j->cmd );
					j->cmd = wrk;
				}
	
				if( j->flist ) 			/* if input files */
				{
					char *fsr_list = NULL;

					wrk = vreplace( j->cmd, "%i", j->flist, j->ninf );	/* put in files into the cmd */
					ng_free( j->cmd );
					j->cmd = wrk;

					fsr_list = s_fsr( j->flist, j->ninf );		/* build filesystem resource list */
					wrk = replace( j->cmd, "%fsr", fsr_list );	/* expand into filesystem resource list */
					ng_free( j->cmd );
					ng_free( fsr_list );
					j->cmd = wrk;
				}

				wrk = replace( j->cmd, "%j", j->name );			/* put jobname in the command if %j indicated */
				ng_free( j->cmd );
				j->cmd = wrk;
	
				if( verbose > 3 && strchr( j->cmd, '%' ) )
				{
					ng_bleat( 0, "found percent(s) in command after vreplace %s", j->cmd );
					debug( j, NULL, NULL, "", 0 );
				}

				/*sfprintf( f, "job: Seneschal %s; rc=$?; ng_s_eoj %s %d $rc $SECONDS \"`times`\"; exit $?\n", j->cmd, j->name, j->attempt );   */
				if( j->wname )
					ng_free( j->wname );
				sfsprintf( jbuf, sizeof( jbuf ), "sj%I*x_%I*d", Ii(wj_id_ts), Ii(wj_id_seq) );
				j->wname = ng_strdup( jbuf );
				wj_id_seq++;							/* dont inc in Ii() */

				sfprintf( f, "job %s : Seneschal %s; rc=$?; ng_s_eoj %s %d $rc $SECONDS \"`times`\"; exit $?\n", j->wname, j->cmd, j->name, j->attempt );   
			
				ng_bleat( 2, "schedule: %s(job) %s(node) %d(att) %I*d(size) %I*d(isize) %d(cost)  %s...", j->name, j->node->name, j->attempt, Ii(j->size), Ii(j->isize), j->cost, j->cmd );  
				ng_log( LOG_INFO, "%s %s %s(job) %s(node) %d(att) %I*d(size) %I*d(isize)", AUDIT_ID, AUDIT_SCHED, j->name, j->node->name, j->attempt, Ii(j->size), Ii(j->isize) );
				
			}
		}

	}


	if( sfclose( f ) > 0 )			/* an error from wreq - woomera may not be up on none n */
	{				
		for( i= 0; i < n->jidx; i++ )		 /* we dont know what jobs did/didnt make it, so we have to let leases pop incase woomera did get some */
	 		if( (j = n->jobs[i]) != NULL )
				j->max_att++;		/* but we will bump max attempts up so that it does not count if woomera did not get it */

		ng_bleat( 1, "schedule: *** woomera smells off on node %s; auto-suspended for 5 minutes", n->name );
		if( n->desired > 0 )
		{
			n->desired *= -1;		/* stop auto sending (if on) -n makes it obvious we shut it off and what it was */
			n->lease = ng_now( ) + 3000;	/* givem them 5 minutes and we will try again */
		}
		n->bid = 0;				/* take their bids away too */
	}

	for( i= 0; i < n->jidx; i++ )		/* in theory, we submitted the lot, so ensure the list is clear */
		n->jobs[i] = NULL;		

	n->jidx = 0;
}

/* ------------------ mapping/fetching routines ------------------------------- */
/* map a name to a job block */
static Job_t *get_job( char *name )
{
	Syment *se;
	Job_t *j = NULL;

	if( (se = symlook( symtab, name, JOB_SPACE, 0, SYM_NOFLAGS )) != NULL )
		j = se->value;

	return j;
}

/* for a given job ensure that resources are available, and if so, then find a node
   that has open bids/cycles, has all of the files (if input files are needed), and
   has all of the attributes that the job needs, or does not have attributes 
   specificly listed as !attribute. if a node pointer (n) is 
   returned, then the file list in j is set to point to all input paths as they are 
   known on n, and the olist is set to output files with attempt numbers added.
*/
static Node_t *map_j2node( Job_t *j )
{
	Resource_t *r;			/* pointer to resource to validate */
	Node_t	*n = NULL;		/* selected node */
	int i;

	if( check_resources( j, 1 ) )		/* resources for job are ok */ /* change 1 to job cost when allocating this way */
	{
		if( j->inf[0] )		/* job has one or more input files -- all must live on same node */
		{
						/* map_f2node ensures mpn for n is not exceeded */
			n = map_f2node( j, NULL );	/* ensure target has all files, or find node w/ all files if no target */
		}
		else				
		{
			if( j->node )					/* no files, but a node specified - use it */
			{
				if( j->node->suspended || ! mpn_check( j, j->node )  )	/* if suspended | node is at max/node for rsc */
					n = NULL;
				else
					n = j->node;			/* taxi into position and hold  (bids checked by caller) */
			}
			else
				n = get_any_node( j, j->notnode, j->nattr ); 	/* neither ifiles nor node supplied - chose any with bids & attrs*/
		}

		if( n )							/* if a suitable node was located */
		{
			mk_filelst( j, n );				/* create a list of pointers to file paths on node n */
			mk_ofilelst( j, j->attempt + 1 );		/* create outfile list with attempt number */
		}
	}

	return n;
}

/*	get any node that has a bid count return null if no nodes have open bids 
	for now we will return the node with the largest bid count 
	np is a pointer to a node that the job should NOT run on (!name)

	we check to ensure that the node is not at max for the jobs resource(s)
*/
static Node_t *get_any_node( Job_t *j, Node_t *np, char *req_nattr )
{
	Node_t	*n = NULL;
	Node_t	*l = NULL;		/* pointer at the largest one */
	int	maxb = 0;		/* largest bid count seen */
	char	*nname = NULL;

	for( n = nodes; n; n = n->next )
	{
		if( 	n != np && 
			! n->suspended && 
			n->bid > maxb  &&
			test_nattr( n->nattr, req_nattr ) &&		/* and the nodes attributes match what is needed */
			mpn_check( j, n ) ) 				/* and max/node limits not exceeded for jobs resource(s) */
		{
			l = n;
			maxb = l->bid;
		}
	}

	return l;
}

/* map a name to a node block */
static Node_t *get_node( char *name, int opt )
{
	Syment	*se;
	Node_t	*n = NULL;
	char	*pp = NULL;			/* generic string pointer */
	int	nstate = 1;			/* name state 1 == ok */

	if( ! name || *name == 0 )		/* null pointer or immediate end of string */
		return NULL;

	
	if( ! isalpha( *name ) )
	{
		ng_bleat( 0, "invalid node name; does not begin with alpha: 0x%02x", (unsigned char) *name );
		*name = '?';
		nstate = 0;
	}

	for( pp = name+1; *pp; pp++ )
	{
		if( ! (isalnum( *pp ) || *pp == '_' || *pp == '.') )
		{
			ng_bleat( 0, "invalid node name; invalid char: 0x%02x", (unsigned char) *pp );
			*pp = '?';
			nstate = 0;
		}
	}

	if( !nstate )
	{
		ng_bleat( 0, "offending (sanitized) node name: %s", name );
		return NULL;
	}

	if( (se = symlook( symtab, name, NODE_SPACE, 0, SYM_NOFLAGS )) != NULL )
	{
		n = se->value;
	}
	else
		if( opt == CREATE )
		{
			n = ng_malloc( sizeof( Node_t ), "create node" );
			memset( n, 0, sizeof( Node_t ) );

			n->bid = 0;
			n->name = ng_strdup( name );

			if( (pp = ng_env( "NG_SENE_DEF_LOAD" )) != NULL )		/* new nodes get default load */
			{
				n->desired = atoi( pp );
				ng_free( pp );
			}
			else
				n->desired = 2;				/* small bit, but starts them running */

			n->next = nodes;		/* add to master node list */
			n->prev = NULL;
			nodes = n;

			symlook( symtab, n->name, NODE_SPACE, n, SYM_NOFLAGS );	/* install in the hash table */
			ng_bleat( 1, "get_node: created %s", n->name );
		}

	return n;
}

/* release a node.  kind of a misnomer as we cannot easily delete a node because of file tenticals 
   that might point to it, and because if it is still alive it will continue to send attributes 
   which would cause us to add it back straight away.   This will be seldomly used, so it should not 
   cause a lot of confusion.
   When we release all we do is to hide it from the user (not shown in dumps)
   and we stop checkpointing it.  We also suspend it so no jobs run there.
*/
static void release_node( char *tok )
{
	Node_t	*n = NULL;

	if( n = get_node( tok, NO_CREATE ) )
	{
		n->flags |= NF_RELEASED;		/* hide from dump and do not ckpt anymore */
		n->suspended = 1;
		schedule_ckpt();			/* force a chkpt at next opportunity */
	}
}

/* allows a node that was released to be added back to the mix */
static void reinstate_node( char *tok )
{
	Node_t	*n = NULL;

	if( n = get_node( tok, CREATE ) )
	{
		n->flags &= ~NF_RELEASED;		/* hide from dump and do not ckpt anymore */
		n->suspended = 0;
		schedule_ckpt();			/* force a chkpt at next opportunity */
	}
}

/* called by symtraverse to purge the natter map sym table -- forces recalc */
static void wash_nattrmap( Syment *se, void *data )
{
	if( se->value )			/* nattr combination seems to be in use still; just wash */
	{
		ng_free( se->value );
		se->value = NULL;
	}
	else
		symdel( (Symtab *) data, se->name, NATTR_SPACE );	/* nattr combination not seen for a while; delete */
}

/*	fill user's buf with a list of nodes that have the attributes listed 
	filling the existing buffer should be faster than filling a 
	stack buffer and strduping as we expect this to be called 
	a significant number of times for a dump/explain
*/
static char *get_node_list( char *req_nattr, char *ubuf, ng_timetype now )
{
	static 	nxt_refresh = 0;	/* time when we force a refresh */
	static	Symtab	*nattrmap = NULL;	/* small attribute string to node list mapping sym table */

	Node_t	*n = NULL;
	Syment	*se;
	char	buf[NG_BUFFER];		/* work buffer if ubuf is null */
	char	*wbuf;
	char	*np;

	if( !now )			/* could save a bunch on explain/dump to call once and pass here */
		now = ng_now( );

	if( ! nattrmap )
	{
		nattrmap = syminit( 71 );		/* small table */
		nxt_refresh = now + 3000;		/* dont need to refresh new table */
	}

	if( now > nxt_refresh )
	{
		symtraverse( nattrmap, NATTR_SPACE, wash_nattrmap, (void *) nattrmap );
		nxt_refresh = now + 3000;
	}

	if( (se = symlook( nattrmap, req_nattr, NATTR_SPACE, 0, SYM_NOFLAGS )) != NULL  )	/* its there and has a value */
	{
		if( se->value )				/* it has a value -- we use it */
		{
			if( ubuf )
			{
				strcpy( ubuf, (char *) se->value );
				return ubuf;
			}
	
			return ng_strdup( (char *) se->value );
		}
	}

	if( ! ubuf )			/* no list in hash, must build it */
		wbuf = buf;
	else
		wbuf = ubuf;

	*wbuf = 0;
	for( n = nodes; n; n = n->next )
	{
		if( test_nattr( n->nattr, req_nattr ) )
		{
			for( np = n->name; np && *np; np++ )
				*wbuf++ = *np;
			*wbuf++ = ' ';
			*wbuf = 0;
		}
	}

/*ng_bleat( 0, "added nattr list to hash: (%s) -> (%s)",  req_nattr, ubuf ? ubuf : buf ); */
	symlook( nattrmap, req_nattr, NATTR_SPACE, ng_strdup( ubuf ? ubuf : buf ), SYM_COPYNM );
	if( !ubuf )
		return ng_strdup( buf );

	return ubuf;
}

static char *xxxget_node_list( char *req_nattr, char *ubuf )
{
	Node_t	*n = NULL;
	char	buf[NG_BUFFER];		/* work buffer if ubuf is null */
	char	*wbuf;
	char	*np;

	if( ! ubuf )
		wbuf = buf;
	else
		wbuf = ubuf;

	*wbuf = 0;
	for( n = nodes; n; n = n->next )
	{
		if( test_nattr( n->nattr, req_nattr ) )
		{
			for( np = n->name; np && *np; np++ )
				*wbuf++ = *np;
			*wbuf++ = ' ';
			*wbuf = 0;
		}
	}

	if( !ubuf )
		return ng_strdup( buf );

	return ubuf;
}

/* check resources used by job j. return 1 if all resources are available */
static int check_resources( Job_t *j, int cost )
{
	int	i;
	Resource_t *r;

	if( ! total_resources )		/* dont waste time if there are no resources in the pool */
		return 0;

	for( i = 0; i < j->nrsrc; i++ )		
	{
		if( (r = j->rsrc[i]) != NULL &&  r->limit >= 0 )
		{
			if( r->flags & RF_HARD )				/* hard resource */
			{
				if( r->active  >= r->target )		/* too many running already */
				{
					ng_bleat( 5, "check_resource: %s hard resource unavailable: %s(nm) %d(active) %d(target)", j->name, r->name, r->active, r->target );
					return 0;
				}
			}
			else
			{
				if( r->target == 0 || (double) (r->active+cost)/total_resources > r->target )
				{
					ng_bleat( 5, "check_resource: %s soft resource unavailable: %s(nm) %.2f(potential) %.2f(target)", 
							j->name, r->name, ((double) r->active)/(double)total_resources, r->target );
					return 0;				/* bail now - no resource */
				}
				else
					ng_bleat( 5, "check_resource: %s soft resource available: %s(nm) %.2f(potential) %.2f(target)", 
							j->name, r->name, ((double) r->active)/(double)total_resources, r->target );
			}
		}
	}
 
	ng_bleat( 6, "check_resource: %s all resources available", j->name );
	return 1;		/* all resources referenced by the job are available (or no resources needed) */
}

/* adjust active component of all resources used by a job by amount */
static void adjust_resources( Job_t *j, int amt )
{
	int	i;
	Resource_t *r;

	for( i = 0; i < j->nrsrc; i++ )			/* for each recource the job uses */	
		if( (r = j->rsrc[i]) != NULL  )
		{
			if( r->flags & RF_HARD )	/* hard limit, inc/dec is not amount but 1 */
				r->active += amt < 0 ? -1 : +1;
			else
				r->active += amt;
			if( r->active < 0 )
				r->active = 0;		/* prevent going negative */
		}

	if( !(j->prime) && amt < 0 && (total_resources += amt ) < 0 )	/* bumpped up when bids are rcvd, so only dec it here */
		total_resources = 0;			/* dont allow it to go south */

}

/* gets pointer to resource blk; creating if not there */
static Resource_t *get_resource( char *name, int option )
{
	Syment *se;
	Resource_t *r = NULL;

	if( !name || !*name )		/* likely a limit command with nothing */
		return NULL;

	if( (se = symlook( symtab, name, RSRC_SPACE, 0, SYM_NOFLAGS )) != NULL )
	{
		r = se->value;
	}
	else
	{
		if( option == NO_CREATE )
			return NULL;

		r = ng_malloc( sizeof( Resource_t ), "create resource" );
		memset( r, 0, sizeof( Resource_t ) );

		r->next = resources;
		resources = r;				/* add to master list */
		r->name = ng_strdup( name );
		r->limit = -1;					/* defaults to unlimited if not set */
		r->mpn = -1;					/* defaults to unlimited if not set */
		r->ref_count = 0;

		symlook( symtab, r->name, RSRC_SPACE, r, SYM_NOFLAGS );	/* install in the hash table */
	}

	return r;
}

/*	gets pointer to type blk; creating if not there 
	if name is not of the format type_name or type.name then NULL returned 
	type blocks are hashed by nodename-jobtype or just jobtype
	if nodename is null.

	if the cost name separation scheme changes, the s_optif LP generator
	need to change to match.
*/
static Jobtype_t *get_type( char *name, char *nname, int create )
{
	Syment *se;
	Jobtype_t *t = NULL;
	int i;
	char tname[1024];		/* type name */
	char	*sep = NULL;		/* type/name separator */

	if( ! (sep = strpbrk( name, "_." )) )  		/* find first seperator */
		return NULL;				/* no . or _ then dont track */

/*
was finding pupdate.cycle rather than pupdate in pupdate.cycle_ning_xx
	if( ! (sep = strchr( name, '_' )) && ! (sep = strchr( name, '.' )) )	
		return NULL;
*/

	memset( tname, 0, sizeof( tname ) );
	if( nname )
		sfsprintf( tname, sizeof( tname ), "%s-", nname );	/* put in node name if there */
	strncat( tname, name, sep - name );				/* add type name to buffer */

	if( (se = symlook( symtab, tname, TYPE_SPACE, 0, SYM_NOFLAGS )) != NULL )
	{
		t = se->value;
	}
	else
	{
		if( create == CREATE )
		{
			ng_bleat( 1, "get_type: created (%s) from %s(name) %s(node)", tname, name, nname );
			t = ng_malloc( sizeof( Jobtype_t ), "create type" );
			memset( t, 0, sizeof( Jobtype_t ) );
	
			t->next = jobtypes;
			jobtypes = t;				/* add to master list */
			t->name = ng_strdup( tname );
	
			for( i = 0; i < MAX_COST_DATA; i++ )
				t->cost[i] = -1;
	
			symlook( symtab, t->name, TYPE_SPACE, t, SYM_NOFLAGS );	/* install in the hash table */
		}
	}

	return t;
}

/* 	get the estimated cost of a job (seconds) based on recent jobs of the same type 
	cost also includes the average woomera queue wait time for the job type. costing 
	info is stored as a sec/size-unit value, and the return from this routine is the 
	minimum number of tenths of seconds that we expect to elapse between the time the 
	job is scheduled, and the status is received.
	if we have no information for the job type, then we guess. 

	we import size, rather than using the job size, because in cases of woomera queues
	the cost of the queue time is not based on the job size.
*/
static long get_one_cost( Job_t *j, long size, int cost_type )
{
	Jobtype_t	*t;		/* pointer to job type */
	int	i;
	double total = 0.0;
	char	*nname;
	char	buf[1024];		/* build the qtime cost type name in */

	nname = j->node ? j->node->name : NULL;		/* should always be set, but lets prevent dumb coredumps */

	if( cost_type == WAIT_COST )
		sfsprintf( buf, sizeof( buf ), "wqtime-%s", j->name );		/* wqtime-jobname must match format saved in record_cost() */
	else
		sfsprintf( buf, sizeof( buf ), "%s", j->name );

	if( (t = get_type( buf, nname, NO_CREATE )) || (t = get_type( buf, NULL, NO_CREATE)) )	/* get generic name if node-type not def */
	{
/*
		ng_bleat( 3, "get_type: %s(job) %s(type) %.2f(total) %.5f(mean) %ld(size)  %.3f(cost)", j->name, t->name, t->total, t->mean, size, t->mean * size );
*/
		ng_bleat( 3, "get_type: %s(job) %s(type) %.2f(total) %.5f(mean) %ld(size)  %.3f(cost)", j->name, t->name, t->total, t->mean, size, (long) size/t->mean );
		if( t->mean > 0 )
			return (long) size / t->mean;
			/*return (long) (t->mean * size); */		/* cost is the mean for the past n obs * the size */
	}

	if( cost_type == WAIT_COST )	
		return 5;				/* queue wait time should never be based on job size */
	else
		return  ((size * 2)/3) + 5;		/* first one, or unknown type, pull a bunny from our bonnet, or something */
}

static long get_cost( Job_t *j )
{
	return get_one_cost( j, j->size, EXEC_COST ) + get_one_cost( j, 1, WAIT_COST );
}


/* return the full filename for File_t on a particular node or NULL */
/* need supply either node name or pointer */
static char *get_path( File_t *f, Node_t *n, char *nname )
{
	int i;

	i = f->nnodes;


	if( n  || (n = get_node( nname, 0 )) )
		for( i = 0; i < f->nnodes && n != f->nodes[i]; i++ );

	return i >= f->nnodes ? NULL : f->paths[i];
}

/* find a file. create a new one if it does not exist AND md5 points to a string */
static File_t *get_file( char *name, char *md5, char *gar )
{
	Syment *se;
	File_t *f = NULL;

	if( (se = symlook( symtab, name, FILE_SPACE, 0, SYM_NOFLAGS )) != NULL )
	{
		f = se->value;
	}
	else
		if( md5 )		/* if user passed this in, then create it */
		{
			f = (File_t *) ng_malloc( sizeof( File_t ), "get_file: file block" );
			memset( f, 0, sizeof( File_t ) );

			f->nnodes = 0;
			f->nodes = NULL;

			f->md5 = ng_strdup( md5 );		

			if( gar )
				f->gar = ng_strdup( gar );		/* save it if we did not reuse buffer earlier */

			f->name = ng_strdup( name );
			f->size = FSIZE_INVALID;
			f->ref_count = 0;
			
			f->next = NULL;			/*  add to fetch queue */
			f->prev = ffq_tail;
			if( ffq_tail )
				ffq_tail->next = f;
			else
				ffq = f;
			ffq_tail = f;
			count_ffq++;				/* count number in the list for fast summaries */

			symlook( symtab, f->name, FILE_SPACE, f, SYM_NOFLAGS );		/* add to symbol table for easy lookup */
		}


	return f;
}

/* given a list of pointers to File_t, return the node that is common bettween all 
   or NULL if no node has all files.  If more than one node has all files, 
   return the one with the most outstanding bids.
   if target was indicated for the node, then we will return this node, but only 
   when it has all of the input files.
   pref is the node that the job has indicated it should run on.
	we will not select the node pointed to by noton.

	1/3/2006 - Added ability to start job provided any file is available.
	the node selected will be the node that has the most number of available 
	files, that can also start the job immediately.
*/
static Node_t *map_f2node( Job_t *j, int *reason )
{
	File_t	**list; 			/* formarly parms, now we just get j-> in */
	int	llen; 
	Node_t	*target; 
	Node_t	*noton; 
	char 	*req_nattr;

	File_t	*f;		/* pointer to the file info */
	Node_t	*n = NULL;	/* node with largest bid */
	Node_t	*nodes[MAX_NODES];	/* list of nodes that have one or more of our files */
	int	hits[MAX_NODES];	/* number of files that each node has */
	int	last = 0;	/* last in nodes */
	int	need = 0;	/* number of hits needed to have a full set on a node */
	int	max_bid = 0;	/* largest bid seen */
	int 	max_hits  = 0;	/* largest hits seen */
	int	i;
	int	k;
	int	x;
	int	stuck = 1;		/* reset when we see a nod with a complete union */
	int	reject_reason = 0;	/* use JNS_ (job cannot be scheduled) const to explain reason */

	list = j->inf;			/* these used to be parms, this makes it easier */
	llen = j->ninf;			/* length of the file list */
	target = j->node;		/* if job is being forced to a particular node */
	noton = j->notnode;		/* specific node selected not to run the job on */
	req_nattr = j->nattr;		/* attributes needed by node we select */
	
	memset( hits, 0, sizeof( hits ) );
	memset( nodes, 0, sizeof( nodes ) );

	if( gflags & GF_MUSTBSTABLE && !(j->flags & JF_VARFILES) )		/* if files must be stable to run a job */
	{
		for( i = 0; i < llen; i++ )
		{
			if(  list[i] && ! list[i]->stable )
			{
				ng_bleat( 5, "map_f2node: file not stable: %s",  list[i]->name );
				if(  reason )
					*reason = JNS_STABLE;
				return NULL;			/* one bad apple spoils the attempt */
			}
		}
	}

	for( i = 0; i < llen; i++ )		/* figure out how many needed files (hits) each node has */
	{
		if( (f = list[i]) )
		{
			need++;			/* base the number of files needed off of actual pointer count */

			for( k = 0; k < f->nidx; k++ )	/* for each node that this file sits on */
			{
				if( f->nodes[k] )		
				{
					for( x = 0; x < last && nodes[x] != f->nodes[k]; x++);
					if( x >= last )
					{
						nodes[x] = f->nodes[k];
						last++;
					}
					hits[x]++;
				}
			}
		}
	}

	if( j->flags & JF_VARFILES )		/* job can start as long as minimum number of  files are available */
	{
		if( need > j->ninf_need )	/* keep the user from saying 6 needed but only having five in the list */
			need = j->ninf_need;	/* we will run the job on the node that has the most number of hits and can accept the job*/
	}

	reject_reason = JNS_FILES | JNS_BIDS | JNS_ATTRS;		/* assume these are all true */
	for( i = 0; i < last; i++ )			
	{
		ng_bleat( 5, "map_f2node: node=%s%s need=%d hits[%d]=%d bid[i]=%d !on=%s target=%s jneed=(%s) natr=(%s)", nodes[i]->name, nodes[i]->suspended ? "(suspended)" : "", need, i, hits[i], nodes[i]->bid, noton ? noton->name : "(empty)", target ? target->name : "any-node", req_nattr, nodes[i]->nattr );

		if( hits[i] >= need  &&  hits[i] >= max_hits ) 	
		{
			reject_reason &= ~JNS_FILES;		/* one node has files, so turn off the flag */
			stuck = 0;				/* at least one node had all files - it may not have bids or attributes though */

			if( nodes[i] != noton && (!nodes[i]->suspended) &&  nodes[i]->bid  && (nodes[i]->bid > max_bid || hits[i] > max_hits) )
			{
					/* [i] is the targeted node for job, or [i] has right attributes */
				if( (target == NULL && test_nattr( nodes[i]->nattr, req_nattr )) || target == nodes[i] ) 
				{
					reject_reason &= ~JNS_ATTRS;		/* node ok for attr requs so off that flag */
					if( mpn_check( j, nodes[i] ) )		/* and ok to run based on max per node resource limits */
					{
						n = nodes[i];
						max_hits = hits[i];		/* we track based on node with most number of files */
						max_bid = nodes[i]->bid;
					}
				}
			}
		}
	}

	if( reason )
		*reason = reject_reason;

	ng_bleat( 5, "map_f2node: selected %s max_bid=%d max_hits=%d jns=0x%x", n ? n->name : "(none selected)", max_bid, max_hits, reject_reason );
	deadlock += stuck;
	return n;
}

/* ------------------------ purge delete ----------------------------- */
void del_resource( char *name )
{
	Syment *se;
	Resource_t *r = NULL;

	if( !name || !*name )		/* prevent barfing */
		return;

	if( (se = symlook( symtab, name, RSRC_SPACE, 0, SYM_NOFLAGS )) != NULL )
	{
		r = (Resource_t *) se->value;
		if( r->ref_count <= 0 )
			r->flags |= RF_IGNORE;			/* ignore when dumping resource info, but keep */
	}
}

/* ================================================================== */

main( int argc, char **argv )
{
	pthread_t plthread;			/* manages the port listener thread */

	char	*lair = NULL;			/* our home directory */
	ng_timetype	file_time = 0;		/* when we will next read repmgr dump */
	ng_timetype	sum_time = 0;		/* next time we write a summary message */
	int	threads = 1;			/* turn to 0 for no threads (debugging) */
	int	hide = 1;			/* disappear into daemon land if set; -f turns off */
	int	nfd = -1;			/* network fd for port listen calls */
	int	status = 0;
	int	hp;				/* highest priority for job scheduled by run_jobs() */
	ng_timetype startt;			/* used to capture time to do one loop */
	ng_timetype endt;

	bleatf = sfstderr;

	ng_srand();

	if( ng_sysname( thishost, sizeof( thishost )) )		/* sent on woomera jobs to get status back via tcp */
		exit( 1 );

	myport = ng_env( "NG_SENESCHAL_PORT" );			/* defaults come from the environment */
	lair = ng_env( "NG_SENESCHAL_LAIR" );

	ARGBEGIN
	{
		case 'f':	hide = 0;  break;		/* stay in the "foreground" */
		case 'l':	ng_free( lair ); SARG( lair ); break;
		case 'n':	threads = 0; break;		/* debugging as it renders seneschal useless */
		case 'p':	ng_free( myport ); SARG( myport ); break;
		case 's':	IARG( sum_time );break;
		case 'v':	verbose++; break;
		case '?':	usage( ); break;
		default:
usage:	
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	if( !myport )
	{
		ng_bleat( -1, "%s: cannot find port (NG_SENESCHAL_PORT) in cartulary/env", argv0 );
		exit( 1 );
	}

	if( !lair )
	{
		ng_bleat( -1, "%s: cannot find lair (NG_SENESCHAL_LAIR) in cartulary/env", argv0 );
		exit( 1 );
	}


	if( chdir( lair ) )
	{
		ng_bleat( -1, "%s: cannot switch to lair: %s: %s", argv0, lair, strerror( errno ) );
		exit( 1 );
	}

	ng_free( lair );
	lair = NULL;



	ng_bleat( 0, "verbose mode set to: %d", verbose );
	if( hide )
		disappear( );			/* do this before we open any files, or such */


	load_free_work( 200000 );		/* create a large number of work blocks and queue them on free queue */

	signal( SIGPIPE, SIG_IGN );
	symtab = syminit( 49999 );		/* LARGE; assuming we will be mapping 150000+ jobs and related files and nodes */
	mpn_init_tab( );			/* initialise the max per node mapping table */

	gflags = GF_DEFAULTS;			/* set default flags now */

	/* sfio is not pthread safe. thus we have disabled pthreads for now. port_init will
  	   initialise the network interface, register callbacks and allow this routine to call ng_sipoll()
	   to check for new data
	*/
#ifdef KEEP
	if( pthread_create( &plthread, NULL, port_listener, (void *) myport ) ) 	/* create network interface thread */
	{
		ng_log( LOG_ERR, "port listener initialisation failed" );
		ng_bleat( 0, " port listener init failed - bailing out now" );
		exit( 1 );
	}
	ng_sleep( 1 );			/* let thread establish */
#else
	port_init( myport );
#endif


	sum_time = ng_now( ) + SUM_DELAY;		/* next time to spit summary and gather file info */
	file_time = ng_now( ) + freq_rm_req;		/* delay assuming that we wont have jobs until nawab sees us up anyway */

	ng_log( LOG_INFO, "started; %s", version );
	ng_bleat( 0, "started, %s", version );

	read_ckpt( );					/* snarf the few bits we checkpoint in narbalek */

	while( ok2run )
	{
		startt = ng_now( );			/* time we started this pass -- use in place of ng_now() calls in the loop  */

		/* sipoll waits and drives our callback(s) when data received on tcp/udp ports. does multi sessions 
		   check for net data - we block for data (2sec) if no resort, pending dump, or work is queued. If 
		   any of those are not empty, we block .1 sec
		*/
		if( (status = ng_sipoll( (rsq || (gflags & GF_WORK) ) ? 10 : 200 )) > 0 )		
			while( (status = ng_sipoll( 10 )) > 0 );		/* get anything else immediatly (1/10th of a sec delay) available */
		if( status < 0 )					/* error says bail out */
			ok2run = 0;

		parse_work( );							/* do any work that was queued from network requests */
		total_resources = 0;
		set_bids( );							/* set any bids automatically if we can */

		if( gflags & GF_NEWJOBS || file_time < startt ) 		/* new load of file info when new jobs received, or they are stale */
		{
			set_targets( );						/* recalc targets as resources might have picked up new refs */
			gflags &= ~GF_NEWJOBS;
		}

		if( file_time < startt )		/* next request for repmgr info is past due */
		{
			if( work_qsize( NORMAL_Q ) < 100 )			/* dont send to rm if queue > 100 */
				rm_dump_req( );					/* send dump1 requests to repmgr */
			else
				ng_bleat( 3, "rmif file dump skipped; work queue too large" );
			file_time = startt + freq_rm_req;			/* reset stale file timer */
		}

		lease_chk( startt );			/* check for any expired leases */

		resort_jobs( );			/* disposition jobs on the resort queue */
		if( ! suspended )
		{
			submit_gar( );		/* put any gar jobs onto the pending queue */
			hp = run_jobs( );	/* schedule any jobs we can */
			calc_gar( hp );		/* calculate any gar jobs if there are available bids someplace */
			if( count_bids( ) )	/* bids still remaining -- turn off soft limits and schedule more jobs */
			{
				eliminate_targets( );		/* turns all soft limits to max */
				run_jobs( );			/* schedule what we can without soft limits */
				set_targets( );
			}
		}
		
		write_ckpt( startt );

		if( verbose && sum_time < startt )			/* sumarise to log  */
		{
			sum_time = startt + SUM_DELAY;
			wallace_sum( -1, 0, 0 );		
		}


#ifdef USE_PTHREADS
		if( ! any_work( ) )				/* if there is not anything on the work queue, then */
			ng_sleep( 2 );			/* chill a bit -- if pthreaded */
#endif
	}

	verbose = 2;				/* be chatty during termination */
	schedule_ckpt();			/* force a chkpt at next opportunity */
	write_ckpt( 1 );				/* one last write */

					/* these frees are wasted, but keep valgrind happy */
	ng_free( myport );
	ng_free( lair );			

	ng_log( LOG_INFO, "ended normally" );
	ng_bleat( 1, "exited normally" );
	return 0;
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_seneschal:Schedule and manage jobs on cluster nodes)

&space
&synop	ng_seneschal [-f] [-l directory] [-p port] [-s seconds]  [-v]

&space
&desc	&This accepts job submission records via a TCP/IP interface and 
	manages the execution of these jobs across the nodes in the cluster.
	Jobs are assigned to nodes based on the location of any affected file(s), 
	and work bids that have been received from each of the nodes. 
	When a job is assigned, it is scheduled for execution on the 
	node via &lit(ng_woomera) and is assigned an expiration or 
	lease time. As a part of the job that is executed, the script 
	&lit(ng_s_eoj) is executed to perform any end of job housekeeping 
	chores and to send a completion status message back to &this 
	(via a UDP/IP datagram).
	If a status datagram is not received before the job's lease
	time expires, the job is cancelled (internally within &this) and
	placed back into the pending queue to be rescheduled. 
	A job is rescheduled (after failure, or expiration)
	up to the number of times indicated in the job submission 
	input for the job. 
	The final status of the job, usually success, but failure if 
	the job was not executed successfully after a maximum number of
	attempts, is returned to the process submitting the job to 
	&this via the TCP/IP session that was established.

&space
&srev
&subcat	The TCP/IP Interface
	&This listens to a well known TCP/IP port (&lit(NG_SENESCHAL_PORT))
	for connection requests from external processes. Onces a session is 
	established, datagrams received on the session are expected to contain
	newline separated job submission records, limit description records, 
	or name records. 
	Job submission records define a single job that &this should schedule
	as soon as it is possible to do so.
	Limit records allow the sender to set &this resource limits at the 
	same time as it submits jobs. 
	The name record identifies the sender and is used to return the status 
	of jobs submitted by the sender.  Job submission records received on 
	the TCP/IP session prior to the receipt of a &ital(name) record will 
	be marked as &ital(unowned); &this makes no attempt to deliver job
	status information on unowned jobs. 

&space
&subcat	Name Records
	It is expected that a process will keep the TCP/IP session active 
	until at least all submitted jobs have been reported on. However, 
	it is expected that the TCP/IP session will sometimes be lost and 
	reestablished, thus job status information is reported using the 
	session name, rather than socket ID. This allows the process to 
	reconnect to &this and receive any status information for jobs 
	queued before the session was lost (provided the process sends the 
	same name as was used previously). 
	The name record, with the syntax of &lit(name=<value>) can be 
	sent at any point after the TCP/IP session has been estblished, 
	however, only jobs requests which are received &stress(after) the 
	session name will have their status information returned to the 
	sender. 
&erev
	
&subcat	Limit Records
	Limit records are used to set the current value for one or more 
	&this resources.  Limit records have the following syntax:
&space
&litblkb
	LIMIT:<name>=<value>[:<name>=<value>...]
&litblke
	&space
	Where:
&begterms
&term	LIMIT : Is a constant token that sets off a limit record.
&space
&term	^: (separator) : Is the user defined separator that is used on the remainder of 
	the record. It is the first charater beyond the &lit(LIMIT) token.
&term	name : Is the name of the resource whose limit is being set.
&space
&term	value : Is the value to which the limit will be set.
&endterms

&space
&subcat	Job Submission Records
	Job records have the following syntax:
&space
.ll +10
&litblkb
jobname:max-attempts:size:priority:resourcelist:{nodename|*}:{input-files|*}:{output-files|*}:command
&litblke
&space
.ll -10
	Where:
&begterms
&term	jobname : Is the name of the job that &this will use as a 
	&ital(basename) when submitting jobs to &ital(woomera). &This
	will append an attempt number to the job name. This name will 
	be used when reporting the final job status back to &ital(nawab).
&space
	Jobnames may contain any character that may legally appear in 
	a &lit(C) identifier. Furthermore, jobnames that contain an 
	underbar character (_) will be assumed to be constructed such 
	that &this may use the chararacters appearing before the underbar
	as a job type.  Job types will allow &this to track and better 
	schedule jobs based on a combination of their type and size.
&space
&term	^: (separator) : The field separator (represented in this example with a colon)
	is definable by the user, and is assumed to be the first unrecognised
	character following the jobname.  An unrecognised character is defined
	as being any character that cannot leagally appear in the job name
	If any charter other than a colon is used, &this will log an informational
	message in the log.
&space
&term	max-attempts : Is the maximum number of times that &this should
	try to run the job before reporting a hard failure.
&space
&term	size : Size is an arbitrary integer (32 bit) defined by the process creating the 
	input job list. &This may use this size in combination with the time 
	required to execute the job and the job's &ital(type) to make assumptions
	about the cost of executing jobs of &ital(type) allowing it to make better
	scheduling decisions.
&space
&term	priority : Is a numerical value between 0 and 100 which define the priority
	given to the job.  The larger the value, the higher priority given to 
	the job. 
&space
&term	resourcelist : Is a whitespace separated list of resource names that the 
	job consumes. &This uses these resources when determining whether or not 
	a job can be scheduled. They are &stress(not) &ital(woomera) resources and 
	are not passed to &ital(woomera) when the command is executed. Resource names
	do follow the same naming conventions imposed by &ital(woomera) and thus should
	begin with a capital letter. 	If a job does not consume any resources 
	then this field should contain an asterisk.
&space
&term	nodename : The name of the node that the job should be scheduled on
	if the &ital(filename) field contains an asterisk. If both the 
	&ital(filename) field and this field contain asterisks the job 
	is executed on any node in the cluster that has open bids. 
&space
&term	input-files : Is a whitespace separated list of file information about input files 
	that will be used by the job. 
	The file information consists of the MD5 checksum, the filename, and an optional 
	gar-name with each item separated from the next by commas.
	The gar-name, if supplied, is the name of a command string (supplied via a separate &lit(gar=)
	message) that &this can use to cause the file to be created if the file does not 
	exist.
	
	.sp
	The files contained in this list can be referenced in the 
	&ital(command) field using &lit(%i)&ital(n) targets (&ital(n) is an integer 
	number with 1 being the left most file defined in the list). When the job is 
	scheduled, the filenames contained in the input list are expanded to their full pathnames
	and then mapped into the &ital(command) string replacing the &lit(%i)&ital(n) place holders.
	If the job has no input files that &this must "expand," then this field
	should contain an asterisk (*). 
	If the &ital(nodename) field contains an asterisk the node(s) that contains
	all of the files listed will be targeted for the job.

&space
&term	output-files : Is a list of space separated tokens that define the 
	output files created by the job. The filenames in the list are 
	modified to include an attempt number and then substituted into the 
	command string using the &lit(%o)&ital(n) targets in &ital(command).
	Upon successful completion of the job, the filenames are changed 
	such that the attempt number is removed and the final result is 
	the list of files that was specified in this list. If an asterisk is placed 
	int the &ital(output-files) field, then &this does not attempt to 
	make any output file name substitution in the command, nor does &this 
	attempt any file name cleanup following the successful completion of 
	the job.
&space
&term	command : Is the command string that should be passed to &ital(woomera)
	for execution. This is &stress(all) information that would be placed 
	on a &ital(woomera) command &stress(following) the colon that separates
	the job name from the command string. Thus it should contain all 
	resources and dependancy clauses that are necessary.
	The command should contain references to all of the files listed in 
	both the input and output file lists on the job submission record. 
	References are made using &lit(%i)&ital(n) (input file) and &lit(%o)(&ital(n)
	targets. The &ital(n) in each target is the list element number of the 
	file (one based). As filenames are substituted into the command string
	the name is expanded to be the fully qualified filename for the file 
	as it exists on the node that will execute the job. 

&space
	As the command field is the last field, all characters from the start of the field
	until the next newline character will be considered a part of the command.
	This allows the command to contain the separator character that has been 
	used to separate the fields on the job specification record.
&endterms
&space
	The following is an example job record which defines job &lit(foo_jobx)
	to operate on file &lit(ds-gecko-p345-e35). The job "size" is 4360 and the 
	job and should be attempted a maximum of 4 times. The job may be executed on 
	any node that contains the input file. Two output files are produced (ds-gecko-p345-e35.goo, and 
	ds-gecko-p345-e35.gxx), and one imput file is supplied. The job consumes two
	&this resources, and also supplies &ital(woomera) resources in the command portion 
	of the record. 
&space
&litblkb
	foo_jobx:4:4360:R_fred R_wilma:*:abc123456xyz,ds-gecko-p345-e35:ds-gecko-p345-e35.goo ds-gecko-p345-e35.gxx:R_GOO_LIM cmd ng_part_goo -i %i1 -h %o2 -v >%o1
&litblke

&subcat Incomplete Job Specificaiton 
	Seneschal requires that the job specification be smaller than 8k as this is the 
	current maximum buffer size used by the process. In order to allow a job which 
	relies on more input files than would fit in an 8k job message, &this allows for 
	incomplete job messages to be submitted.  The incomplete message supplies all of 
	the same information as the regular job message, except &this expects the 
	session partner to send one or more input/output file specification messages
	that are added to the job as though they were received in the input or output
	fields of the job.  The incomplete job message has the same syntax as the 
	job message, except that it is preceded with &lit(ijob=). 

&space
	The input or output messages that follow the incomplete job message have the 
	syntax:
&space
&litblkb

   input=<jobname>:<file information>
   output=<jobname>:<file information>
&litblke
&space
	The file information that is supplied is in the same format as would have been supplied 
	for a file in a complete job record. Input/output records may contain information 
	for more than one file if necessary.
&space
	Incomplete jobs remain unqueued until a complete message is received for the 
	job. This message has the syntax &lit(cjob=<jobname>), and when received 
	causes &this to evaluate and queue the job onto the proper queue.  

&space
&srev
&subcat	Status Reporting
	Job completion status is returned to the process that submitted the job (the owner) 
	via the TCP/IP session that the owner created,
	provided that the TCP/IP session was named before the job
	was submitted, and the TCP/IP session is still established.
	If neither of these holds true, &this will discard the status.
	It is also the responsibility of the job owner (e.g. &ital(nawab)) to handle job failures.
	As it is likely for TCP/IP to hold and bundle records into the same 
	datagram, each status record will be separted from the previous by a 
	newline character with each having the following format:
&erev
&space
&litblkb
	<jobname>:<node>:<status>:<misc-data><newline>
&litblke
&space
	Where:
&begterms
&term	jobname : Is the name of the job as it was supplied via the 
	job submission file.
&space
&term	node : Is the name of the node on which the job was executed
	or last attempted.
&space
&term	status : Is the final status of the job. A zero indicates 
	that the job was successfully executed. A non-zero value 
	in this field indicates the status of the last attempt made to 
	run the job, and that the maximum number of attempts were 
	tried without a successful execution. 
&space
&term	misc-data : Is the first 1024 bytes of ASCII data that the job 
	placed into the file referenced by the environment variable 
	&lit(WOOMERA_JOB_TMP). If this file did not exist, or was
	empty, then an immediate newline will follow the colon separator.
&endterms

&space
&subcat	Job Priorities
	Jobs are placed into the pending execution queue in an order such that 
	the higher priority jobs are executed first. Jobs of the same 
	priority are further oredered in the queue using the estimated 
	cost of the job at the time the job is queued. If &ital(priority creep)
	is on (default) the priority of the job is slowly increased the 
	longer it sits in the queue.  The incremental amount is one unit every 
	thirty minutes a job resides in the pending queue. Priority creep should prevent
	low priority jobs from never being scheduled when the cluster is 
	busy.
&space
	The job's priority is allowed to increase to a maximum of the submitted priority
	value plus 15, or 90, which ever is smaller.  Thus a job submitted with a 
	priority less than 76 will reach it maximum priority after residing 
	on the pending queue for 7.5 hours. Jobs submitted with a priority 
	greater than 75, but less than 90 will reach their maximum priority
	in less time.  Jobs which are submitted with a priority of 90 or larger
	will not be subject to priority creep.
	
&space
&subcat	Job Costs and Leases
	The cost of executing any job is calculated by &this and used when 
	calculating the lease time that is associated when a job is scheduled
	on a node. Job cost is determined by observing the execution 
	time required by recent jobs of the same type as compared to the 
	size of each job. 
	The average time that a job spends queued for execution on each known 
	node is also tracked and used to calculate the lease for a job.
	For each successive attempts to run the job after a failure, 
	the lease time is increased by a factor of two.   Regardless of the 
	calculated value, &this will default to a minimum lease time of 30 minutes.

&space
	Eventually, nodes will
	bid for work not in terms of jobs, but in terms of time and thus the 
	cost of a job will also be used to select a node on which the job 
	can be executed. 
&space	
&subcat	Resources and Resource Management
	&This understands the concept of resources and resource limits. Each 
	job submitted to &this may optionally reference one or more resources
	that it &ital(consumes) while it is executing. These resources are used
	to control when the job can be scheduled. A job will 
	be scheduled, only when &stress(all) of the referenced resources 
	become available.  

&space
	Resources are managed by percentage rather than by unit. 
	A target percentage for each resource is computed by dividing each 
	reasource's limit by the sum of the limits for all known resources. 
	A job is candidate for scheduling, from a resource perspective, if 
	executing the job will not cause any of the referenced resources to 
	exceed their target percentage. 
	Resources are measured using the same units as the bids which are 
	submitted by cluster nodes, which is currently in terms of jobs 
	rather than capacity (seconds).  When capacity is used as resource
	units, the cost of a job is used to determine whether or not the 
	job will cause the resource's target to be exceeded.

.if hfm 
.** include only for the HTML version - not the tty version 
&space
	In order to determine whether or not a resource has exceeded its target, 
	&this uses the concept of a &ital(resource pool) as its measuring device.
	The &ital(resource pool) is the total number of resources that are currently 
	in use (running jobs, or total cost of running jobs) plus the current bid amount. 
	The resource pool is in constant flux depending on the number of running jobs, and the 
	bids that have been received from the cluster nodes. 
	A resource's current utilisation of the pool (percentage) is calculated by 
	dividing the active jobs (or costs) for that resource by the pool size.

&space	Resource limits may take one of three states: &ital(unbound, blocked,) 
	or &ital(bound). An &ital(unbound) resource has a limit value of 
	&lit(-1) and will not cause a job to block on the resource. Any resource
	that is referenced on a job will be defined as unblocked until &this 
	receives a limit value for the resource. &ital(Blocked) resources are 
	those with a limit value of zero (0). When the value of a resource is 
	set to zero, all jobs that reference that resource will be blocked 
	from being scheduled. By utilising a resource as either &ital(blocked)
	or &ital(unbound) the resource becomes a simple on off switch for the 
	referencing jobs. 

&space
	A &ital(bound) resource is a resource whose value is greater than zero (0).
	This value is &stress(not) a percentage, &stress(nor) is it the number 
	of jobs that are allowed to execute. The value is interpreted in conjunction 
	with all other resource limits that are known to &this in order to establish
	the target percentage for the resource. For example, consider three resources
	(R1, R2, and R3) with limits of 100, 50, and 25 respectively (175 resource units 
	total).  Under normal conditions, &this will try to schedule jobs such that 
	those consuming R1 will comprise roughly 57% of the total running (100/175), 
	R2 jobs to 28%, and R3 jobs to 14%.  

&space
	The percentage scheme works well, but under certain cirmstances the capacity 
	of bidding nodes will be under utilised.
	Under utilisation occurs when a resource is not referenced by any of the pending 
	jobs, or it is a blocking resource (limit value of 0).
	Under both circumstances the target percentage of capacity for the resource will still be 
	"reserved" by the simple algorithm described earlier. 
	To avoid this, resources that are blocked, or are unreferenced, do 
	&stress(not) have their limit value added in when computing the target percentages. 
	Thus if R1 (from the previous example) has no referencing jobs, or is blocked, 
	the targets computed for R2 and R3 would be 66% (50/75) and 33%(25/75) respectively. 
	This twist to the simple algorithm eliminates the need to adjust all limit 
	values when a resource becomes blocked, or all jobs for that resource have
	completed. &This automatically detects, and recomputes the target percentages
	whenever new jobs are added to its environment, or when a resource reference 
	count reaches zero.

&space
	&S also supports &ital(hard) resources. Running jobs with these resources are limited
	to the exact value specified for the resource rather than using the percentage 
	scheme as described earlier. This allows resources for physical aspects of 
	the cluster (transmission limits, tape drives, nodes) to be established. 

&space
.fi

&srev
&subcat	Command Interface
	Using &lit(ng_sreq) commands may be sent to &s.  Please refer to the 
	&lit(ng_sreq) man page for the proper command line syntax as it 	
	will likely differ from the syntax described below.  
&space
	All messages received via a TCP/IP session are first tested to determine
	if they match a recognised command syntax. If so, the command is executed, 
	or queued for execution, and possible results returned on the session.
	&S will always return an end of data indication when all data that resulted 
	from the command has been written to the TCP/IP session.  This end of data
	marker is currently a single dot character (.) followed by a newline character.
	&S allows the TCP/IP session to remain open in order for the sending programme
	to submit another command.

&space
	The following are the commands that are supported by this interface:
	
&space
&litblkb
     bid:<nodename>:<request amount>[:<resource req>]
     dump:value
     explain:[job] <jobname>|[resource] <resource>|[file] <filename>|running|missing|pending|complete
     extend:<jobname>:seconds
     hold:job-name
     jobstat:<jobname> <attempt> <nodename>:<status>:<command specific info>
     limit:resource-name:value[hsn]
     load:node-name:value                         
     maxlease:seconds
     mpn:resource-name:value
     nattr:node:string			
     node:jobname:nodename		
     pause:value
     push:<jobname>
     purge:<jobname>
     release:type:name                       
     resume:value                            
     rmif:<batch-size>:<seconds>
     run:<jobname>
     stable:1|0                              
     suspend:all|<node>
     verbose:value
     xit	(exit)
&litblke
&space
	For the non-obvious commands, a brief explination of the action taken is 
	provied.
&space
&begterms
&term	extend : Causes the lease for the job to be changed to &ital(seconds). If 
	seconds is 0, then this effectively causes the job to fail.  A reattempt 
	on the job will depend on the attempt that was being executed, and the 
	number of attempts allowed for the job.
&space
&term	hold	: Puts the named job on hold. It will not be started until a 
	release message is received for the job.
&space
&term	jobstat : These are messages returned from jobs that were submitted by 
	&s for execution. These messages provide a limited amount of information 
	to &s with regard to the success or failure of the job, and the node where 
	the job executed. 
&space
&term	maxlease : Sets the value that &this will use as a maximum lease time
	regardless of what is actually calculated. This value must be greater than 
	1800 seconds. 
&space
&term	limit : Sets a limit for the named resource. 	The values passed are of the form
	&lit(na) where &lit(n) is the value, and a is one of 'h,' 's,' or 'n' indicating 
	hard limit, softlimit, or max per node.
&space
&term	load : Causes &this to keep, when possible, &ital(value) jobs running on the named node.
&space
&term	node : Allows an external process to dynamically set the execution node for a job.
&space
&term	nattr : Defines the attributes that a node has.  Each node will send its attributes 
	to &this every few minutes.
&space
&term	push : Moves the job to the head of the pending queue.
&space
&term	release : Releases the named object. If type is &ital(job) then any hold placed 
	on the job is removed.  If the object type is resource, then &this deletes any
	knowledge of the named resource provided that the resource is not referenced 
	by any queued jobs. If type is &ital(node) then the node is logically removed from 
	&this.  When a node is released, it will no longer receive jobs, and it will be 
	excluded from checkpoint operations. A node can be 'unreleased' by using the 
	resume command. 
&space
	resume : Allows jobs to start being assigned to the node. If a node was released, 
	its release state will also be reversed. 
&space
&term	rmif : Provides the mechanism for the batch size and frequency  of communicaitons
	with replication manager. (see below.)
&space
&term	run : Deletes all resource restrictions that were placed on the job allowing 
	it to run when enough bids are available.
&space
&term	stable : If set to 1 causes &this to block jobs until all needed input files 
	are marked as stable by replication manager. Setting the value to 0 allows 
	&this to run jobs as soon as a single copy of the file is noticed.
&space
&term	xit : Causes &this to exit.
&endterms
&space

&subcat	Bidding
	&This distributes work based on bids that have been received from
	each node in the cluster, or based on a work load level that a node 
	has requested. 
	Bids are expected to contain the name of the node, the number of jobs that the 
	node would like to have assigned to it (&ital(njobs)), and optionaly
	the number of available resources (&ital(nresources)). &This attempts to 
	assign the number of jobs requested though it may be unable 
	to assign the desired number because of file locations
	and the number of jobs that have been supplied to &this.
	The following is the syntax of a bid datagram:
&space
&litblkb
	bid:<nodename>:<bid-amount>[:<nresources>]
&endlitb

&space
	As jobs are assigned to the node the bid count managed by &this
	is decremented. Once the bid count reaches 0 it is not increased
	until another bid request has been received. The node may 
	cancel any outstanding bids by submitting a bid of zero (0).
	Once the bid level has reached zero (0), &this will not schedule
	any additional jobs on the node until another bid request is 
	received.
&space
	&stress(NOTE^:) The resources portion of the bid datagram is 
	documented, but not implemented. If a datagram contains the 
	&ital(:nresources) information it will be ignored. When implemented
	this section will be expanded to include detailed information on 
	how the &ital(nresources) information is utilised by &this.

&space
&subcat Work Load Level
	A node may submit a work load level request to &this to indicate 
	the workload that the node would like &this to try to maintain.
	Once a load level request has been received, &this will automaticly
	set the bids for the node as jobs on the node complete. While it 
	may not always be possible, &this will attempt to ensure that the 
	jobs scheduled on the node are as close to the work load level 
	indicated. The work load level request has the following 
	syntax:
&space
&litblkb
	load:<nodename>:<loadvalue>
&endlitb

&space
	Currently &ital(loadvalue) is measured in terms of number of jobs, 	
	but it is likely to change to become the same units as the job cost
	is measured in.

&space  The node attribute command
	Nodes, via &lit(ng_rota) or similar process, should send a list of 
	their attributes to &s on a regular basis.  &This uses these attributes
	to possibly limit where jobs may execute; when jobs specify node 
	attributes that are required for the job. The syntax of the command
	message is:
&space
&litblkb
	nattr:<nodename>:<attribute> <attribute> ....
&litblke
&space
	The attribute list must be space separated, and when invoking &lit(ng_sreq)
	to send attributes to &this, the attributes should be quoted in order 
	to ensure that they are passed into &this as a single parameter.

&space
&subcat	The explain command
	Explain command is intended to be invoked by human users as the ouput 
	is  a bit easier to read than the output of &lit(dump).  It is also a bit 
	pared down as compared to &lit(dump), so it might be necessary to use &lit(dump) 
	to gather information from &s. The explain command expects a parameter 
	which indicates what is to be detailed. Currently, these may be explained:
&space
&begterms
&term	pend[ing] : explains all jobs on the pending queue.
&space
&term	miss[ing] : List all jobs currently on the missing queue.
&space
&term	comp[lete] : Causes all jobs on the complete queue to be exipained (a more brief
	listing than for the other two queues as less information is kept once a job
	has been placed on the complete queue.
&space
&term	job jobname : Explains a specific job.
&space
&term	resource rname : Causes information about the resource to be listed.
&space
&term	file filename : Generates information about the file.
&endterms
&space
	The keywords &lit(job, resource,) and &lit(file) need be supplied only if 
	the jobname could match one of the queue keywords, or if there is a 
	duplication between a jobname and a resource name. 

&space
&subcat	The Dump Command
	When a dump command is received, &s will wallace out a fair amount of
	detailed information depending on the dump type requested. The dump 
	command was origianlly intended to be used by other programmes 
	in their effort to report on the state of jobs managed by &s, and thus
	the dump types are specified as numbers rather than words. The following 
	lists these numbers and the output that is produced when received:
&space
&begterms
&term	1 : General summary (heartbeat).
&term	2 : General summary, nodes summary.
&term	3 : Same as 2 plus a dump of the pending queue (no file information).
&term	4 : General summary, nodes summary, pending and running queues (no file information).
&term	5 : Same as 4, file information for each pending job is also generated.
&term	6 : Same as 5 including the missing and completed job queue.
&term	7 : A dump of input file information is generated. 
&term	8 : Not implemented.
&term	9 : Dump a complete file listing of the files that are needed to run jobs. For each 
	file the job name, filename, and the nodes that the file is lcoated on are dumped. 
&term	10 : A dump of all resource information. 
&term	11 : A dump of all resource information with a count of the number of jobs consuming
		the resource that are currently running on each node. 
&term	20 : A dump of all job type data is produced.
&term	30 : A dump of all node information is produced. 
&endterms

&space
	Specifying values other than those listed above will result in an unpredictiable 
	series of output generated by &s.  When &s is managing a large number of jobs
	(more than 50,000) or a large number of input file references, it is likely 
	that a dump of the pending and/or missing queues can take several minutes. When 
	using &lit(ng_sreq) to send the dump or explain command to &s, it is recommended 
	that an appropriate timeout value (-t seconds) be supplied to prevent &lit(ng_sreq)
	from aborting the request.  &S has been coded such that dump and explain command 
	processing is interleaved with other processing and while the requestor of a 
	dump may wait several minutes, &s is servicing other requests in parallel and those
	users are not seeing any delay. 
	

&space	
&subcat Suspending things
	The &lit(suspend) message can be sent to cause &this to stop scheduling 
	jobs on a particular node, or on all nodes. The &lit(suspend) message 
	expects the name of a node to suspsend, or the word all to suspend all 
	process. Once a node is suspended, no new jobs (including housekeeping jobs) 	
	will be started on that node.  The &lit(resume) message can be used to 
	put a node back into service. 

&space
&subcat Setting resource limits
	Both percentage and hard resource limits are specified with the &lit(LIMIT)
	datagram. &S recognises the limit as a hard limit if the value has an
	&lit(h) appended to it (e.g. 2h). 
	
&space
&subcat	Push and Run messages
	The push command causes the named job to be pushed to the 
	head of the pending queue. The run datagram both pushes 
	the job to the head of the queue and drops all resource 
	requirements such that the job will be run on the next 
	node with available bids. Jobs that have missing input 
	files cannot be altered using either of these commands. 
&space
&subcat	The Replication Manager Interface
	&This sends 'dump1' requests to replication manager for each file that 
	is referenced by a job.  For each request, &this should receive the 
	node and full path of each instance of the file, the md5 checsum value
	and the filesize.  The 'dump1' requests are batched into groups of 
	3500 or less to allow for other work to be done.  The requests are 
	sent approximately every 15 seconds.   These values are optimal for 
	the worst case situation, and will probably do well for all environments.
	Should they need to be changed, they can be changed in real time with the 
	following ng_sreq command:
&space
&litblkb
   ng_sreq rmif <batch-size> <seconds>
&litblke

	Batch-size must be greater than 999 and seconds may not be set lower than
	15.  

&space
&subcat	Cartulary Variables
	Several cartulary variables supply default information to &this.
	The following list the variables that &this searches the cartulary
	for:
&space
&lgterms
&term	NG_SENESCHAL_PORT : Supplies the UDP port number that &this will 
	listen on for both bids and job completion messages.
&space
&term	NG_SENESCHAL_LAIR : Defines the directory pathname that &this uses
	as its working directory. It is the directory that it expects to find
	or write various communications files into.
&endterms

&space
&opts	The following options are recognised when entered on the command 
	line.

&space
&begterms
&term	-f : Remain in foreground. Prevents &this from becoming a daemon. All 
	verbose output is written to the standard error device. If this option is 
	not supplied, then &this will put itself into daemon mode which causes it 
	to sever ties from the invoking process/shell, close all files, and to 
	open a private log file in t&lit(/ningaui/site/log) for verbose output. 
&space
&term	-l directory : Supplies the name of the directory that &this will use as its 
	current working directory. &This expects to find several files in this 
	directory. Overides the value of &lit(NG_SENESCHAL_LAIR) cartulary variable.
&space
&term	-p port : Defines the UDP port that &this will listen on. When supplied on the 
	command line this option overrides the &lit(NG_SENESCHAL_PORT) cartulary variable.
&space
&term	-s seconds : Can be used to supply the number of seconds between the heartbeat
	message that &this writes to its log file.
&space
&term	-v : Causes &this to ramble on about things.  The more &lit(-v) options, the 
	more it goes on.
&endterms



&examp	The following are sample comands that can be sent via &lit(ng_sreq) to control
	the behavour of &this.

&space
	&lit(ng_sreq -e) .br
	Causes &this to cleanup and exit.
&space
	&lit(ng_sreq dump 3) .br
	Causes a dump of the pending and running queues to be displayed. 
&space
	&lit(ng_sreq limit Rscooter 30) .br
	Sets the value of the resource &ital(Rscooter) to 30.
&space
	&lit(ng_sreq limit Rscooter 2h) .br
	Sets the value of the resource &ital(Rscooter) to a &ital(hard) 2. Hard
	limits are not percentage based and &s will run a maximum of &ital(limit)
	jobs, regardless of the number of bids,  for jobs referencing hard resources.
&space
	&lit(ng_sreq verbose 3)
	.br Changes the verbose level in &this to 3. Verbose messages are written
	to the log file usually in &lit($NG_ROOT/site/log/seneschal.log).
&space
	&lit(ng_sreq explain pu_p0003_ed2) .br
	Causes &this to explain its view of the job &lit(pu_p0003_ed2).
	This may also be: .br
	&lit(ng_sreq explain job pu_p0003_ed2)
&space
	&lit(ng_sreq -j <job.file) .br
	Submits all of the job records contained in &ital(job.file) to &this. If
	the wait option (&lit(-w)) is supplied on the &lit(ng_sreq) comamnd, then 
	&lit(ng_sreq) will wait for a status response for each record that it submitted
	to &this. Status messages will be written to the standard output device. 
&space
	&lit(ng_sreq nattr bat10 'Leader Lgmem FreeBSD') .br

&space
&see	&ital(ng_woomera), &ital(ng_nawab), &ital(ng_sendbid), &ital(ng_s_eoj), &ital(ng_sreq), &ital(ng_s_mvfiles)
	&ital(ng_s_start) &ital(ng_s_sherlock) &ital(ng_goad)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	25 Jun 2001 (sd) : Thin end of the wedge.
&term	10 Oct 2001 (sd) : Modifications to add the TCP/IP support.
&term	31 Oct 2001 (sd) : To switch order of md5 and file. (boo!)
&term	31 Jan 2002 (sd) : To ensure map 2 any node checked bids > 0, and get_any_node took suspended flag into consideration.
&term	06 Feb 2002 (sd) : To block a job targeted for a specific node until input files are there too.
&term	08 Feb 2002 (sd) : To add support for garring input files.
&term	28 Mar 2002 (sd) : To correct a bug in attempt numbering on command expansion.
&term	16 Apr 2002 (sd) : Added support for hard limits.
&term	31 Jun 2002 (sd) : To allow for new replication manager symbolic names, and provide !node ability.
&term	03 Jun 2002 (sd) : Properly set bid for node when a lower load is received.
&term	04 Jun 2002 (sd) : To also respect notnode when selecting based on file list
&term	22 Jun 2002 (sd) : Added -j jobname to housekeeping jobs so they can extend their leases
&term	08 Jul 2002 (sd) : To correct a bug with resetting gar pointer when pulling a file off the alist
&term	09 Jul 2002 (sd) : Changed attempt separator to +, added restart delay for failed jobs.
&term	23 Feb 2003 (sd) : Mod to pass $SECONDS on s_eoj command.
&term	04 Feb 2003 (sd) : Added stuff to capture sys/usr from times shell builtin
&term	31 mar 2003 (sd) : Added support to get status via tcp rather than udp
&term	15 Jun 2003 (sd) : Up version to 3.0. Major tcp/ip enhancement + purge resource etc.
&term	24 Jul 2003 (sd) : Added stuff to audit message and timeout on woomera connect.
&term	19 Aug 2003 (sd) : Added max lease time for jobs.
&term	03 Oct 2003 (sd) : Ignores sigpipe signals. This was causing termination if user interrupted a dump/explain. (v3.2)
&term	05 Oct 2003 (sd) : To ensure that sfstd* files are closed at detach.
&term	11 Oct 2003 (sd) : Added stability support, and beefed up the man page.
&term	02 Dec 2003 (sd) : Added node attribute support (v3.3)
&term	12 Dec 2003 (sd) : Converted to ng_open012() to reopen standard in/out/err.
&term	12 Jan 2003 (sd) : Allowed resource limits to be set in the job statement (v3.4)
&term	17 Jan 2003 (sd) : Corrected woomera queue wait time tracking bug.  
&term	13 Feb 2004 (sd) : Added natter info to explain/dump, set default priority to 20 -- maxes out at the smaller of p+15 or 90.
&term	26 Feb 2004 (sd) : Now correctly sets anynode flag when a node assignment is received after a job is scheduled.
&term	20 Mar 2004 (sd) : Memory leak changes.
&term	11 May 2004 (sd) : Changed to resort only 100 jobs at a time so that we service other requests when large numbers of 
		jobs become elegible for movement to the pending queue.
&term	11 Jun 2004 (sd) : Added max/node limits for resources.  The ng_sreq limit command now supports the form Nn to set a 
	max per node limit of N. 
&term	14 Jun 2004 (sd) : Added a second scheduling pass which will happen if bids remain after the first pass. The soft 
		resource limits are disabled during the second pass, so that a resource that has only a few jobs, and a large target
		percentage, does not waste slots. 
&term	28 Jun 2004 (sd) : Added max work limit to parse no more than 50 work blocks before coming up for air (TCP check)
&term	09 Jul 2004 (sd) : Added time division to explain/dump to prevent large ones from blocking other work.
&term	10 Jul 2004 (sd) : Changed the work block to have a dynamic buffer rather than a static one -- keeps memory foot print
		small when a large number of requests are pending.
&term	14 Jul 2004 (sd) : Added ability to explain details about a single file.
&term	16 Jul 2004 (sd) : Corrected bug in finfo_fetch that could leave a job on the missing queue even if files were all 
		known to repmgr. 
&term	26 Jul 2004 (sd) : Release of job now clears restart_delay timer as well as the obstruct flag. 
&term	22 Aug 2004 (sd) : Explain runjobname does not dump running queue, but dumps info on the job.  
&term	14 Oct 2004 (sd) : Changed resource usage check to (active+cost > target) from (active+cost >= target) to prevent 
		leaving a single bid when only one resource is targeted to get work.
&term	25 Oct 2004 (sd) : Fixed max per node initialisation problem that required setting to be doubled after jobs submitted.
&term	17 Jan 2005 (sd) : Added qsize option.
&term	28 Oct 2005 (sd) : Mods to use $TMP when creating flists that are sent to target nodes with %F%i tags in command. 
&term	03 Nov 2005 (sd) : Fix issue with extend lease which was causing max per node to be ignored. (V4.2)
&term	04 Jan 2006 (sd) : Added support to start jobs that have a subset of the needed input files. (V4.3).
&term	01 Feb 2006 (sd) : Corrected problem in map_f2job() that was causing jobs to block under certian circumstanses. (v4.4)
&term	16 Mar 2006 (sd) : New nodes now get a desired work load of NG_SENE_DEF_LOAD if that pinkpages value is set.
			Added checkpointing of node information (v4.5).
&term	17 Mar 2006 (sd) : Added job name to each woomera job. The name is sj<timestamp>_<seq>. Also added interface to ng_s_sherlock
		which is invoked, just prior to lease expiration for a running job, to investigate the state of the job. Sherlock 
		will either report the status of the job if it can be found in woomera logs, fail the job, or extend the lease if 
		it believes the job is running. (v4.6)
&term	17 Apr 2006 (sd) : Force a node to be excluded if it has not yet sent attributes. 
&term	18 Apr 2006 (sd) : Reworked the command parsing. Added support for nawab to send hold job requests without 
		getting an ack message. (v4.7)
&term	17 May 2006 (sd) : Corrected issue when reading a checkpointed desired load that is less than 0. Now sets node lease
		so that the auto suspended state is reset. (v4.8)
&term	28 Aug 2006 (sd) : Pulled referenced to an internal malloc debugging routine. 
&term	01 Sep 2006 (sd) : If NG_SENE_DEF_LOAD is not defined we implement a hard coded default of 2 bids. 
&term	09 Sep 2006 (sd) : Added ability to directly send dump1 messages to repmgr rather than reading the dump file.
		Added the rmif command to allow the rmif parameters (max per request and frequency) to be supplied.
		(v5.0)
&term	10 Oct 2006 (sd) : Corrected memory leak in new s_rmif functions. 
&term	20 Oct 2006 (sd) : Corrected bug in siprintf() call that caused core dump when dump 5 received. 
&term	24 Oct 2006 (sd) : Corrected a bug in s_rmif.c that was not setting the session name correctly for repmgr sessions.
&term	22 Dec 2006 (sd) : Now checkpoints the repmgr dump1 interface frequency and amount (sene/cluster/rmif). (5.1)
&term	18 Jan 2007 (sd) : Added job status work queue so that status messages do not block behind gobs of dump1 
		repmgr responses, yet do not block the user requests put on the priorty queue.  Added support for user
		name associated with each job. (v5.2)
&term	22 Feb 2007 (sd) : set check for %s in command after expansion to happen only if verbose >= 3. (v5.2/02227)
&term	21 Mar 2007 (sd) : Fixed a few memory leaks (v5.2/03297)
&term	13 Apr 2007 (sd) : Added file trace ability. (v5.3)
&term	19 Apr 2007 (sd) : Added better diagnostics to the explain pending blocked job messages and work queue stats on the summary 
		dump output. Repmgr dump1 requests are sent only when the low priority work queue has little or nothing 
		queued. (v5.3/04197)
&term	25 Apr 2007 (sd) : Added interface to the repmgr d1 agent for better (faster) processing of dump1 information (v5.4)
&term	27 Apr 2007 (sd) : Played games to speed things up when slammed with lots of completions.
&term	01 May 2007 (sd) : Added logic to patiently wait for our port to free since some O/Ss seem not to honour the socket option
		of reuse.  (v5.4/04057)
&term	05 Oct 2007 (sd) : Changed maxwork limit test in s_netif.
&term	26 Oct 2007 (sd) : Changed maxwork limit to default to 5000.  (v5.5)
&term	10 Dec 2007 (sd) : Now saves queue processing limits in checkpoint.
		Accepts inputig= records from job owner. The file sizes for these files are not used in job size computations.
		Now lists the total ignored size (K) on job stats masterlog message.	
		(v5.6)
&term	27 May 2008 (sd) : Added checkpointing of each resource that is referenced by one or more jobs. 
&term	07 Oct 2008 (sd) : Added check to prevent bad node names from making it into the table. 
&term	20 Oct 2008 (sd) : Corrected problem with interface to repmgr; was not dropping references to node/path when a file's
		number of occurences dropped. 
&term	15 Jan 1009 (sd) : Added support for dump 11 command; dumps resource and the number of jobs with that resource running by node. 
&term	28 Jan 2009 (sd) : Extended support for work data to be larger than 2k. Some dump1 buffers were coming back bigger than this
		and causing odd problems with truncation.
&endterms

&scd_end
------------------------------------------------------------------ */
