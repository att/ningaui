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
 ----------------------------------------------------------------------
  Mnemonic:	n_ckpt.c
  Abstract:	These routines support chckpointing nawab.
		checkpoint files are written as newline terminated records with 
		tilda (~) separated fields. the first field of each record is a 
		keyword describing the datastructure or component that can be 
		created with the record's information.  the order is important
		as several of the program parsing routines are used and expect that 
		related datastructures have already been created when they are 
		called. the order of records is:

		ckpt, 
		{
			programme, 
			{
				descriptor, 
				descriptor-command, 
				troupe, 
				{
					job, 
					flist,
					:
					flist,
					job-command
				}, 
				[input], 
				[output]
			}, 
			[variables], 
			[ionum-vars]
		}
		
		groups of records appearing inside of matching curley braces are expected
		to repeat, however the order of the records in the group must be as 
		indicated. records inside of square braces are not required to be present.
		command records are split from their associated descriptor/job record
		as they may contain tildas in the command and it is easier to parse
		the command when it is saved as a separate record. 

		currently output file information is saved in the check point file, 
		but not used when reading and rebuilding as the information is not 
		needed once the job commands are created.  

		records beginning with a hash (#) symbol are treated as comments.

		Error checking
		some error checking is done, generally to ensure that the previous 
		datastructures have been created when necessary, and to ensure that
		the number of job records found match the job count on the troupe
		record. If an error is detected during read a bad status is returned
		and an error message is generated. 

  Date: 	26 Feb 2002
  Author: 	E. Scott Daniels

  mods:		24 Feb 2003 (sd) - Changed field sep to ~ to prevent !!leader! issues. Erk.
		21 Sep 2004 (sd) - Verified timetype handled correctly for int64; comment mods
		04 Jan 2006 (sd) - Added support for required input
		12 Jun 2006 (sd) - Conversion to ng_tumbler from ng_ckpt.
		30 Oct 2006 (sd) - Converted to use ng_sfopen() and to check close return codes.
		18 Jan 2007 (sd) - Added support to save user and subnode info
		19 Jan 2007 (sd) - Added keep-tsec support
		01 Mar 2007 (sd) - Fixed memory leak in read -- tlist not freed
			Fixed memory leak in ckpt_write.
		19 Sep 2007 (sd) - Now capture md5 of checkpoint file for log msg.
		13 Nov 2007 (sd) - Fixed cause of core dump if md5 did not return a value.

  Notes:	Seneschal commands (maintained in the job blocks) are newline terminated.
		The newline in the ckpt file is stripped when read back in and 
		thus we must remember to add it when reconstructing the job.

 		Troupe state needs to be reset to pending so its jobs will 
		be resubmitted.
 ------------------------------------------------------------------------
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<sys/resource.h>	/* needed for wait */
#include	<sys/wait.h>	/* needed for wait */
#include	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"ng_tokenise.h"

#define	NO_SPACEMAN
#include	"nawab.h"		/* must be before ref to repmgr */

extern	int do_ckpt;			/* flag to turn off checkpointing */
extern	char *tumbler_vname;

/* ------------ static prototypes (globals in ningaui.h) -------------------- */
static void ckpt_io( Sfio_t *f, Io *ip, char *type );
static void ckpt_vars( Syment *se, void *data );
static void ckpt_iovars( Syment *se, void *data );
static void ckpt_programme( Syment *se, void *data );

#define	SAFE_PTR(a,b)	(a?a->b:"")		/* keep something like t->desc->value->str from failing if value is null */
#define SAFE_STR(a)	(a?a:"")		/* prevent "(null)" from being popped out by sfprintf */

/*	dump all io blocks in the chain */
static void ckpt_io( Sfio_t *f, Io *ip, char *type )
{
	for( ; ip; ip = ip->next )
		sfprintf( f, "%s~%s~%s~%s~%s~\n", type, ip->name, SAFE_PTR(ip->val,sval), SAFE_STR(ip->gar), SAFE_STR(ip->rep) );
}

/*	dump all variables associated with a programme */
static void ckpt_vars( Syment *se, void *data )
{
	Sfio_t	*f;

	f = (Sfio_t *) data;

	if( se->value != NULL )
		sfprintf( f, "variable~%s~%s\n", se->name, se->value );
}

/*	dump all io number variables associated with a programme */
static void ckpt_iovars( Syment *se, void *data )
{
	Sfio_t	*f;

	f = (Sfio_t *) data;

	if( se->value != NULL )
		sfprintf( f, "ionum~%s~%s\n", se->name, se->value );
}

/* we assume buffer has \n on it because they were seneschal messages */
static void ckpt_flist( Sfio_t *f, Flist *fp, char *type )
{
	
	int 	i;

	if( fp )
		for( i = 0; i < fp->nalloc; i++ )
			if( fp->list[i] )
				sfprintf( f, "flist~%s~%s", type, fp->list[i] );
}


/*	cause a programme to be dumped, cause all programmes to be dumped 
	data is a pointer to the open sf file for ckpt stuff
*/
static void ckpt_programme( Syment *se, void *data )
{

	static int	barf = 0;		/* keeps us from going deep */
	Sfio_t	*f;
	Programme	*p;
	Troupe	*t;
	Io	*ip;
	int	i;
	int	j;
	char	*tok;

	if( ! se )				/* no pointer, assume we called, pass ourselves to symtab to drive for each prog */
	{
		if( barf )
			return;			/* if we were invoked before the last one had a chance to be done */

		barf++;
		symtraverse( symtab, PROG_SPACE, ckpt_programme, data );
		barf = 0;
		return;
	}


	if( (p = (Programme *) se->value) == NULL )
		return;

	f = (Sfio_t *) data;
	sfprintf( f, "programme~%s~%d~%I*u~%I*u~%s~%s~%I*d\n", p->name, p->progid, sizeof( p->purge_time ), p->purge_time, sizeof( p->subtime), p->subtime, p->subuser, p->subnode, Ii( p->keep_tsec ) );

	for( t = p->troupes; t; t = t->next )				/* order important, desc must be found by read before troupe */
	{
		sfprintf( f, "desc~%d~%s~%s~%s~%s~%s~%s~%s~%s~\n", t->desc->progid, t->desc->name, SAFE_STR(t->desc->desc), 
				SAFE_STR(t->desc->after), 
				SAFE_STR(t->desc->nodes), SAFE_STR(t->desc->rsrcs), SAFE_PTR(t->desc->attempts,sval), 
				SAFE_PTR(t->desc->priority,sval), SAFE_PTR(t->desc->reqdinputs,sval) );

		sfprintf( f, "dcmd~%s\n", t->desc->cmd  );			/* desc cmd on separate line as it may have ! and makes tokenising harder if on desc line */

		sfprintf( f, "troupe~%d~%d~%d~%d~%d\n", t->waiting_on, t->njobs, t->cjobs, t->ejobs, t->state );

		for( i = 0; i < t->njobs; i++ )			/* each command goes on its own line for same reason as dcmd did */
		{
			for( tok = t->jobs[i].info; tok && *tok; tok++ )
				if( *tok == '~' )
					*tok = '|'; 
			sfprintf( f, "job~%d~%s~%s~%s", t->jobs[i].status, t->jobs[i].node, SAFE_STR(t->jobs[i].info), SAFE_STR(t->jobs[i].qmsg)  );	/* qmsg has embedded new line */
			
			ckpt_flist( f, t->jobs[i].ifiles, "in" ); 
			ckpt_flist( f, t->jobs[i].ofiles, "out" ); 
			sfprintf( f, "cmd~%s", t->jobs[i].cmd ? t->jobs[i].cmd : "\n" ); /* command has a newline on it cuz its a seneschal message */
		}
		

		/* input/output is worthless after we create jobs -- no longer will save it  */
#ifdef NEED_DEBUG
		ckpt_io( f, t->desc->inputs, "input" );
		ckpt_io( f, t->desc->outputs, "output" );
#endif
	}

	symtraverse( symtab, p->progid + VAR_SPACE, ckpt_vars, data );		/* funky recursion :) */
	symtraverse( symtab, p->progid + IONUM_SPACE, ckpt_iovars, data );
}


/* nothing more than a wrapper to waitpid, but if we ever need to do something like timeout this 
   will be the place
*/
void ckpt_wait( pid_t p )
{
	ng_bleat( 1, "ckpt_wait: blocked on child process: %d\n", p );
	waitpid( p, NULL, 0 );
	ng_bleat( 1, "ckpt_wait: child process has finished: %d\n", p );
}

/* take a checkpoint if one is not pending.  done via fork to ensure 
   nawab still is responsive to other activity -- writing the checkpoint when 
   there are lots of stuff can take a wee bit of time. we return the pid of the 
   one running so that at the end we can wait on it before exiting.
*/
pid_t ckpt_asynch( char *fname )
{
	static Tumbler	*tp = NULL;		/* tumbler information */
	static	pid_t	pending = 0;		/* one already pending */

	pid_t	status;	
	char	*name = NULL;

	
	if( pending )
	{
		if( (status = waitpid( 0, NULL, WNOHANG )) == pending )		/* child finished; ok to do another */
		{
			ng_bleat( 1, "ckpt_asynch: previous child finished: %d", pending );
			pending = 0;
		}
		else
		{
			ng_bleat( 2, "ckpt_asynch: previous child still running: %d", pending );
			return pending;				/* still pending; we can not do anything */
		}
	}

	if( tp == NULL )
		tp = ng_tumbler_init( tumbler_vname );

	name = ng_tumbler( tp, 1, 0, 0 );		/* get name in managed (repmgr) space; no peek -- BEFORE fork so sumblers update here */
	ng_bleat( 1, "ckpt_asynch: basename = (%s)  allocname=(%s)", fname, name );
	switch( (pending = fork( )) )
	{
		case 0: 							/* child process */
				ng_bleat( 1, "ckpt_asynch: child successfully started to take a ckpt in: %s", name );
				ckpt_write( name );				/* do the old synch stuff */	
				ng_bleat( 1, "ckpt_asynch: child successfully completed ckpt in: %s", name );
				ng_free( name );				/* moot */
				exit( 0 );					/* we always return with a good code */
				break;

		case -1: 	ng_bleat( 0, "ckpt_asynch: could not fork: %s", strerror( errno ) ); 
				break;

		default:	
				ng_bleat( 2, "ckpt_asynch: child forked successfully; parent continues: %d", pending );
				break;					/* parent process -- just return the childs ssn */
	}

	ng_free( name );
	
	name = ng_tumbler( tp, 0, 1, 1 );		/* peek at next file we will use */
	
	if( name )
	{
		char cmd[NG_BUFFER]; 

		ng_bleat( 1, "ckpt_asynch: asynchronously removing next ckpt file: %s", name );
		sfsprintf( cmd, sizeof( cmd ), "ng_wrapper --detach ng_rm_register -u -f %s %s", verbose ? "-v" : "", name );	
		system( cmd );
		ng_free( name );
	}
	else
		ng_bleat( 1, "ckpt_asynch: did not get a peek at next name; next ckpt file not unregistered" );

	return pending;
}

/*	create a chk point file */
void ckpt_write( char *fname )
{
	Sfio_t	*f;				/* file pointer to ckpt file */
	Sfio_t	*p;				/* file pointer to md5 computation process */
	ng_timetype	now;
	char	ptime[100];			/* pretty time */
	char	cmd[2048];
	char	*buf;
	int	rc;

	if( ! fname || ! do_ckpt )
		return;

	now = ng_now( );
	ng_stamptime( now, 1, ptime );

	ng_bleat( 1, "ckpt_write: writing checkpoint to: %s", fname );

	if( (f = ng_sfopen( NULL, fname, "w" )) != NULL )
	{
		sfprintf( f, "ckpt~%I*u~%s\n", sizeof( now ), now, ptime );
		ckpt_programme( NULL, f );			/* null se ptr causes it to traverse the programmes in the symtab */
		if( sfclose( f ) )
		{
			int en = errno;
			ng_bleat( 0, "nawab_ckpt: write/close error, file not registered: %s: %s", fname, strerror( en ) );
			ng_log( LOG_ERR, "nawab_ckpt: write error: %s: %s", fname, strerror( en ) );
			sfsprintf( cmd, sizeof( cmd ), "ng_rm_register -u -f -v %s", fname );	/* unregister if old one -- dont want to use */
			system( cmd );
		}
		else
		{
			sfsprintf( cmd, sizeof( cmd ), "ng_rm_register -f -v %s", fname );	
			if( (rc=system( cmd )) != 0 )					/* register the file with repmgr -- 0 is a good return code*/
			{
				ng_log( LOG_WARNING, "unable to register check point file: %s", fname );
				ng_bleat( 0, "ckpt_write error: unable to register check point file: %s rc=%d", fname, rc );
			}
			else
			{
				char *md5 = NULL;

				ng_bleat( 1, "computing md5 of checkpoint file: %s", fname );
				sfsprintf( cmd, sizeof( cmd ), "ng_md5 -t %s", fname );
				if( (p = sfpopen(  NULL, cmd, "r" )) != NULL )
				{
					if( (buf = sfgetr( p, '\n', SF_STRING )) != NULL )		/* dont know why, but this has failed before */
						md5 = ng_strdup( buf );
					else
					{
						ng_bleat( 0, "md5 on ckpt file failed to return a value: %s", fname );
						md5="=====invalid-md5-value========";			/* we must write something */
					}
			
					sfclose( p );
				}
				else
					md5="=====invalid-md5-value========";	/* we must write something */
	
				ng_log( LOG_INFO, "nawab_ckpt: checkpoint successfully written to: %s %s", fname, md5 );  
				ng_bleat( 1, "ckpt_write: checkpoint successfully written to: %s rc=%d md5=%s", fname, rc, md5 ); 
			}
		}
	}
	else
		ng_bleat( 0, "ckpt_write error: could not open ckpt file: %s: %s", fname, strerror( errno ) );

}

/*	create a chkpt file but as a 'dump' so its not registered or logged or anything.
	if prog name is passed in, then we dump just for that programme

	the `ng_nreq dump core` command drives this and this is VERY synchronous, so 
	we keep the dump core command somewhat secret and use it only in extreme 
	cases of debugging.
*/
void core_dump( char *fname, char *prog )
{
	Syment	*se = NULL;				/* pointer to programme's sym entry */
	Sfio_t	*f;
	ng_timetype	now;
	char ptime[100];			/* pretty time */
	char cmd[2048];

	now = ng_now( );
	ng_stamptime( now, 1, ptime );

	if( prog )
	{
		if( (se = symlook( symtab, prog, PROG_SPACE, (void *) 0, SYM_NOFLAGS )) == NULL )
		{
			ng_bleat( 0, "core_dump error:  cannot find programme: %s", prog );
			return;
		}
	}

	if( prog )
		ng_bleat( 1, "core_dump: for %s; writing to: %s", prog, fname );
	else
		ng_bleat( 1, "core_dump: writing to: %s", fname );

	if( (f = ng_sfopen( NULL, fname, "w" )) != NULL )
	{
		sfprintf( f, "coredump~%I*u~%s\n", sizeof( now ), now, ptime );
		ckpt_programme( se, f );			/* null se ptr causes it to traverse all programmes in the symtab */
		sfprintf( f, "== end ==\n" );
		sfclose( f );
	}
	else
		ng_bleat( 0, "core_dump error: could not open ckpt file: %s: %s", fname, strerror( errno ) );
}

/* crate a buffer that can be sent to seneschal (newine terminated) */
static char *ckpt_mksmsg( char *buf )
{
	char	*msg = NULL;

	if( buf  )
	{
		msg = (char *) ng_malloc( strlen( buf ) + 2, "ckpt_mksmsg" );		/* we add a new line */
		sfsprintf( msg, strlen( buf ) + 2, "%s\n", buf );
	}

	return msg;
}

/*	read and reconstruct. returns 0 if error */
int ckpt_read( char *fname )
{
	Sfio_t	*f;
	Programme	*p = NULL;		/* things we are creating */
	Troupe		*t = NULL;
	Desc		*d = NULL;
	Io		*ip = NULL;
	Job		*jp = NULL;
	Value		*vp;			/* tmp pointer to a new value */

	Syment	*se;			
	ng_timetype	now;
	ng_timetype	then;		/* when the chkpt file was created */
	char	*buf;			/* pointer to record read from file */
	char	*tok;
	char	**tlist = NULL;		/* points at a list of troupe names that we block after */
	int	tcount;			/* number of tokens that were found */
	int	rc = 1;			/* asssume we will be successful */
	long	progid; 		/* new id assigned to each programme */
	int 	jidx = -1;		/* job index into current troupe */
	int	jdone = 0;		/* jobs for a troupe that have a finished/error status */

	now = ng_now( );

	if( (f = ng_sfopen( NULL, fname, "r" )) != NULL )
	{
		while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			tlist = ng_tokenise( buf, '~', '\\', tlist, &tcount );			/* tokenise the buffer */
			tok = tlist[0];
			switch( *tok )		/* determine line type based on first token */
			{
				case 'j':						/* job:1:node:status: */
					if( t )
					{
						if( jidx >= t->njobs )	
						{
							ng_bleat( 0, "ckpt_read error: too many jobs found for troup %s: expected %d", t->desc->name, t->njobs );
							rc = 0;
						}
						else
						{
							++jidx;
							ng_bleat( 3, "ckpt_read: adding job %d; %s", jidx, buf );

							jp = &t->jobs[jidx];
							memset( jp, 0, sizeof( *jp ) );		/* fix 07/13/04 */
							strcpy( jp->node, tlist[2] );
							if( (jp->status = atoi( tlist[1] )) < FIN_OK )
								jp->status = 0;			/* was pending/running at ckpt time, set to pending */
							else
								jdone++;
			
							if( *(tlist[4]) )				
								jp->qmsg = ckpt_mksmsg( tlist[4] );	/* command complete message */

							if( *(tlist[3]) )				/* info data */
								jp->info = strdup( tlist[3] );		/* save it if there */
						}
					}
					else
					{
						ng_bleat( 0, "ckpt_read error: found job but no troupe defined: %s", buf );
						rc = 0;
					}
					break;

				case 'f':				/* flist:{in/out}:stuff */
					if( *(tlist[1]) == 'i' )
					{
						if( ! jp->ifiles )			/* need to create a filelist */
						{
							jp->ifiles = (Flist *) ng_malloc( sizeof( Flist ), "addfiles:flist block" );
							memset( jp->ifiles, 0, sizeof( Flist ) );
						}

						if( jp->ifiles->idx >= jp->ifiles->nalloc )			/* need more */
						{
							jp->ifiles->nalloc += 20;
							jp->ifiles->list = ng_realloc( jp->ifiles->list, sizeof( char * ) *  jp->ifiles->nalloc, "realloc ifiles" );
							memset( &jp->ifiles->list[jp->ifiles->idx], 0, sizeof( char *) * 20 );
						}

						ng_bleat( 3, "ckpt_read: adding input flist to job %d: %s", jidx, tlist[2] );
						jp->ifiles->list[jp->ifiles->idx++] = ckpt_mksmsg( tlist[2] );

					}
					else
					{
						if( ! jp->ofiles )			/* need to create a filelist */
						{
							jp->ofiles = (Flist *) ng_malloc( sizeof( Flist ), "addfiles:flist block" );
							memset( jp->ofiles, 0, sizeof( Flist ) );
						}

						if( jp->ofiles->idx >= jp->ofiles->nalloc )			/* need more */
						{
							jp->ofiles->nalloc += 20;
							jp->ofiles->list = ng_realloc( jp->ofiles->list, sizeof( char * ) *  jp->ofiles->nalloc, "realloc ofiles" );
							memset( &jp->ofiles->list[jp->ofiles->idx], 0, sizeof( char *) * 20 );
						}

						ng_bleat( 3, "ckpt_read: adding output flist  to job %d: %s", jidx, tlist[2] );
						jp->ofiles->list[jp->ofiles->idx++] = ckpt_mksmsg( tlist[2] );
					}
					break;

				case 'c':		
					if( *(tok+1) == 'm' )		/* cmd~setupa_Cycle1_0:3:300:95:Rnawab :ning02:::cmd >/tmp/daniels.cout */
					{
						if( jp )
						{
							ng_bleat( 3, "ckpt_read: adding cmd to job %d: %s", jidx, buf );

							if( tok = strchr( buf, '~' ) )			/* find our separator */
							{
								jp->cmd = ckpt_mksmsg( tok+1 );		/* cvt to something we can send to sene */
							}
							else
							{
								ng_bleat( 0, "ckpt_read error: bad command buffer: %s", buf );
								rc = 0;
							}
						}
						else
						{
							ng_bleat( 0, "ckpt_read error: command found before job: %s", buf );
							rc = 0;
						}
					}
					else			 /* ckpt~ng-timestammp~pretty time  */
					{
						sfsscanf( tlist[1], "%I*u", sizeof( then ), &then );
						ng_bleat( 0, "ckpt_read: ckpt file is %ld seconds old (%s)", (long) ((now-then)/10), buf );

					}
					break;

				case 't':						/* troupe~waitingon~jobs~cjobs~ejobs~state */
					if( t )						/* any cleanup from previous troupe */
					{
						if( jidx < t->njobs-1 )		/* did not find enough job definitions */
						{
							ng_bleat( 0, "ckpt_read error: missing jobs from %s(prog) %s(troupe); expected %d, found %d", 
								p->name, t->desc->name, t->njobs, jidx );
							rc = 0;
						}
						else
							ng_bleat( 1, "ckpt_read added troupe %s; %d(jobs) %d(pending)", t->desc->name, jidx+1, (jidx - jdone)+1 );
					}

					if( d )
					{
						if( (t = new_troupe( p, d )) == NULL )
						{
							ng_bleat( LOG_ERR * -1, "readckpt: panic: null troupe, possible corrupted ckpt file at: %s", buf );
							exit( 2 );
						}
						t->desc = d;
						t->njobs = atoi( tlist[2] );
						t->cjobs = atoi( tlist[3] );
						t->ejobs = atoi( tlist[4] );
						if( (t->state = atoi( tlist[5] )) == TS_ACTIVE )   /* change to pending to cause resubmit */
							t->state = TS_PENDING;


						t->nblocked = 0;			/* we dont recover these as we call set_blocks to recalc */
						t->bidx = 0;				
						t->waiting_on = 0;		

						t->jobs = (Job *) ng_malloc( sizeof( Job ) * t->njobs, "ckpt_read: node_job" );
						memset( t->jobs, 0, sizeof( Job ) * t->njobs );

						jidx = -1;
						jdone = 0;
						ng_bleat( 2, "ckpt_read: added troupe %s expecting %d jobs", t->desc->name, t->njobs );
					}
					else
					{
						t = NULL;
						ng_bleat( 0, "ckpt_read error: found troupe before desc: %s", buf );
						rc = 0;
					}
					break;


				case 'd':
					if( *(tok+1) == 'e' )	/* desc~progid~name~comment~after~nodes~rsrcs~attempts~priority~reqinputs */
					{
						d = ng_malloc( sizeof( *d ), "ckpt_read: desc");
						memset( d, 0, sizeof( *d ) );
						d->progid = progid;			/* new one was assigned when programme was parsed */
						/* d->progid = new_progid( );	*/	/* must assign a new internal id to prevent collisions */
						d->name = strdup( tlist[2] );
						d->desc = strdup( tlist[3] );
						d->after = strdup( tlist[4] );
						d->nodes = strdup( tlist[5] );
						d->rsrcs = strdup( tlist[6] );
						d->attempts = newvalue(T_vstring, strdup( tlist[7] ) );
						d->priority = newvalue(T_vstring, strdup( tlist[8] ) );
						if( tcount > 9 )
							d->reqdinputs = newvalue(T_vstring, strdup( tlist[9] ) );
						else
							d->reqdinputs = newvalue(T_vstring, strdup( "0" ) );

						ng_bleat( 2, "ckpt_read: added descriptor: %s", d->name );
					}
					else						/*  dcmd~command-string */
					{
						if( d )
						{
							ng_bleat( 3, "ckpt_read: adding dcmd : %s", buf );
							if( tok = strchr( buf, '~' ) )
								d->cmd = strdup( tok+1 );		/* we dont care if its buggered as we really will not use it anyway */
						}
						else
						{
							ng_bleat( 0, "ckpt_read error: found dcmd before desc: %s", buf );
							rc = 0;
						}

					}
					break;

				/* BIG NOTE:
					input/output/variable info is of no use and may be missing from the checkpoint file.  Once we 
					have created the jobs, and the flist info for each job, the variable stuff is worthless because
					we only have the last iterations values anyway.  
				*/
				case 'i':
					if( *(tok+1) == 'n' )				/* input~name~pattern~gar~replication~*/
					{
						if( d )
						{
							vp = newvalue(T_vstring, strdup( tlist[2] ) );			/* the file pattern */
							ip = newio( strdup( tlist[1] ), vp, *tlist[3] ? strdup( tlist[3] ) : NULL, 0 );
							ip->rep = strdup( tlist[4] );					/* stick in replication fact */
							ip->next = d->inputs;
							d->inputs = ip;				/* add to head of the list */

							ng_bleat( 3, "ckpt_read: added input: %s = %s := %s", tlist[1], tlist[2], tlist[3] );
						}
						else
						{
							ng_bleat( 0, "ckpt_read error: found input before desc: %s", buf );
							rc = 0;
						}
					}
					else						/*  ionum~dspart~%i1 */
					{
						ng_bleat( 3, "ckpt_read: added ionum var %s=%s", tlist[1], tlist[2] );
						symlook( symtab, tlist[1], IONUM_SPACE + progid, strdup( tlist[2] ), SYMTAB_FREE_VALUE | SYM_COPYNM );
					}
					break;

				case 'o':						/* output~newpart~ds-%DBN-p%p-%ED~~-2~ */
					ng_bleat( 2, "ckpt_read: skipping output" );  /* right now output stuff is not needed after job blocks built */
					break;

				case 'v':						/* variable:name~value */
					if( tok = strchr( buf, '~' ) )
					{
						if( tok = strchr( tok+1, '~' ) )	/* assume that value might have ~ in it */
						{		
							symlook( symtab, tlist[1], VAR_SPACE + progid, strdup( tok + 1 ), SYMTAB_FREE_VALUE | SYM_COPYNM );
							ng_bleat( 2, "ckpt_read: added var %s=%s", tlist[1], tok+1 );
						}
					}
					break;

				case 'p':			/*  programme~name~progid~purgetime~subtime~subuser~subnode~keep-tsec */
								/*	0	1	2	3	4	5	6	7*/
					if( p ) 		/* not first - cleanup previous if needed */
					{
						set_blocks( p );			/* set up any blocks based on after clauses */
					}

           				if( (se = symlook( symtab, tlist[1], PROG_SPACE, 0, SYM_NOFLAGS )) == NULL )   /* see if we can find it; make if cannot */
					{
						ng_bleat( 1, "ckpt_read: adding programme: %s user=%s", tlist[1], tlist[5] ? tlist[5] : "unknown" );

                        			p = (Programme *) ng_malloc( sizeof( Programme ), "ckpt_read: programme" );
                        			memset( p, 0, sizeof( *p ) );

											/* progid is used when we hit desc */
						progid = p->progid = new_progid( );	/* must assign a new internal id to prevent collisions */

#ifdef JUNK
                        			if( (progid = progid + PROGID_STEP) > PROGID_MAX )	/* we will assign new, incase they loaded a prog before ckpt read */
                                			progid = 0;
                        			p->progid = progid;
#endif
                        			p->name = strdup( tlist[1] );
                        			p->purge_time = strtoll( tlist[3], NULL, 0 );

						if( tcount > 4 && tlist[4] )			/* old ckpt might not have these */
							p->subtime = strtoll( tlist[4], NULL, 0 );	
						if( tcount > 5 && tlist[5] )
							p->subuser = ng_strdup( tlist[5] );
						if( tcount > 6 && tlist[6] )
							p->subnode = ng_strdup( tlist[6] );
						if( tcount > 7 && tlist[7] )
							p->keep_tsec = strtoll( tlist[6], NULL, 0 );

                        			symlook( symtab, p->name, PROG_SPACE, (void *) p, SYM_NOFLAGS );        /* register  it */

						t = NULL;
						d = NULL;		/* parinoia never hurts */
					}
					else
					{
						ng_bleat( 0, "ckpt_read error: programme already existed for: %s", buf );
						p = (Programme *) se->value;
					}
					break;

				case '#':	break;			/* comments make testing much easier! */

				default:
					if( tcount > 1 || *tlist[0] )		/* not a blank line */
					{
						ng_bleat( 0, "ckpt_read error: unknown buffer in ckpt file: (%s)", buf );
						rc = 0;
					}
					break;
			}
		}

		if( tlist )
		{
			ng_free( tlist );
			tlist = NULL;
		}
		if( sfclose( f ) )
		{
			ng_bleat( 0, "ckpt_read: error on read/close: %s: %s", fname, strerror( errno ) );
			exit( 1 );
		}
	}
	else
	{
		ng_bleat( 0, "ckpt_read error: could not open ckpt file: %s: %s", fname, strerror( errno ) );
		rc = 0;
	}

	if( t )
	{
		if( jidx < t->njobs-1 )		/* did not find enough job definitions */
		{
			ng_bleat( 0, "ckpt_read error: missing jobs from %s(prog) %s(troupe); expected %d, found %d", p->name, t->desc->name, t->njobs, jidx );
			rc = 0;
		}
		else
			ng_bleat( 1, "ckpt_read added troupe %s; %d(jobs) %d(pending)", t->desc->name, jidx, jidx - jdone );
	}

	if( p )
		set_blocks( p );

	return rc;
}

/*	reads status messages from a file and causes them to be parsed */
int ckpt_recover( char *fname )
{
	extern	errno;

	Sfio_t	*f;
	char 	*buf;
	long	count = 0;

	if( (f = ng_sfopen( NULL, fname, "r" )) != NULL )
	{
		while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			ng_bleat( 1, "ckpt_recover: status for job: %s", buf );
			count++;
			parse_status( buf, RECOVERY_MODE );
		}
	
		sfclose( f );

		ng_bleat( 1, "ckpt_recover: processed %d status messages", count );
	}
	else
	{
		ng_bleat( 0, "ckpt_recover: error opening recover file: %s: %s", fname, strerror( errno ) );
		return 0;
	}

	return 1;
}
