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
  Mnemonic:	n_lib.c
  Abstract:	General lib routines that are used by nawab.
  Date: 	8 January 2002
  Author: 	Andrew Hume/E. Scott Daniels

  Mods:		23 Jul 2002 (sd) : Added file list support for jobs to manage huge
			numbers of files with. -- prevents buffer blowouts
		23 Mar 2004 : Free name in checkpoint, and to call asynch checkpoint.
		27 Mar 2005 (sd) : We no longer call sfclose() on stdin/out/err as we need
			sfio to maintain its structure for things to work properly in 
			yyparse(). 
		13 Apr 2006 (sd) : Purge troup now purges finished jobs to ensure 
			they are off the seneschal complete queue.
		19 Jan 2006 (sd) : Extended default purge time for errored programmes to 
			72 hours. Added support for user defined keep time.
		31 Jan 2006 (sd) : Added extra diagnostic if desc list is null when parse_desc() called.
		27 Mar 2007 (sd) : Found and corrected an issue with how subuser and subnode strings
			were allocated. 
		07 Apr 2008 (sd) : Removed duplicate (old) code that tried to unregister the next check
			point file. This is being done in n_ckpt.c and old code was getting it wrong and
			causing error messages in private log. 
		14 Oct 2009 (sd) : Corrected bleat call in purge_programme to be before the free of p.
 ------------------------------------------------------------------------
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"nawab.h"

#ifndef OPEN_MAX
#define OPEN_MAX 20		/* bloody linux */
#endif

static Desc *global_d = NULL;

extern	int	do_ckpt;		/* flag is set if we should do checkpointing */
extern	int	gflags;			/* global flags */
/* ---------------- ALLOCATION ---------------------------------------*/

Iter *
newiter(Type t)
{
	Iter *i;

	i = ng_malloc(sizeof(*i), "iter");
	i->type = t;
	i->app = 0;
	i->ref = 1;			/* inital reference count is one */
	i->fmt = NULL;
	return i;
}

Range *
newrange(char *name)
{
	Range *r;

	r = ng_malloc(sizeof(*r), "range");
	r->pname = name;
	r->iter = 0;
	return r;
}

Value *
newvalue(Type t, char *s)
{
	Value *v;

	v = ng_malloc(sizeof(*v), "value");
	v->type = t;
	v->sval = s;
	v->range = 0;
	return v;
}

Io *
newio(char *name, Value *v, char *gar, char flags )
{
	Io *i;

	i = ng_malloc(sizeof(*i), "io");
	i->name = name;
	i->val = v;
	i->gar = gar;
	i->filecvt = 0;
	i->flags = flags;

	i->rep = NULL;
	i->rlist = NULL;
	i->count = -1;
	i->next = NULL;
	return i;
}

/* old style -- called when cmd was encountered to build a desc with everything 
   The new method is to add info to the most recently added descriptor (mrad) 
   as we encounter it. This allows us to process statements in any order. 

   This is deprecated and nothing new should use it.
*/
Desc *
newdesc(Type type, char *name, char *comment, char *after, Range *range, char *rsrcs, Value *priority, Value *attempts, char *nodes, Io *inputs, Io *outputs, char *cmd, char *woomera, Value *reqdinputs )
{
	Desc *d;
	char	buf[NG_BUFFER];
	char	wbuf[2048];		/* woomera stuff */

	d = ng_malloc(sizeof(*d), "new-desc: desc");

	memset( d, sizeof( *d ), 0 );		/* for safety sake */
	d->type = type;
	d->name = name;
	d->val = 0;		/* if type is assignment */
	d->desc = comment;
	d->after = after;
	d->range = range;
	d->nodes = nodes;
	d->inputs = inputs;
	d->outputs = outputs;
	d->next = 0;
	d->rsrcs = rsrcs;
	d->attempts = attempts;
	d->priority = priority;
	d->reqdinputs = reqdinputs;

	/*d->cmd = cmd;		old */
	if( cmd )
	{		 /* add any womera job modifiers before cmd; seneschal wants: cmd [woomera <woomera-stuff> cmd]as tag */
			/* the cmd is added to the front of the string when sending to seneschal */
		if( woomera )
		{
			sfsprintf( wbuf, sizeof( wbuf ), "woomera %s cmd ", woomera );
		}
		/*sfsprintf( buf, sizeof( buf ), "woomera %s cmd %s", woomera ? woomera : "", cmd );  bad */
		sfsprintf( buf, sizeof( buf ), "%s%s", woomera ? wbuf : "", cmd ); 
		d->cmd = strdup( buf );
	}
	else
		d->cmd = 0;

	return d;
}

/* create a new and fairly empty, desc */
Desc *empty_desc( Type type, char *name )
{
	Desc *d;
	char	buf[NG_BUFFER];
	char	wbuf[2048];		/* woomera stuff */

	d = ng_malloc(sizeof(*d), "desc");
	memset( d, 0, sizeof( *d ) );

	d->type = type;
	d->name = name;

	return d;
}

/* 	build a new troupe and put on the end of the pending queue */
Troupe *new_troupe( Programme *p, Desc *d )
{
	Troupe 	*new = NULL;
	Troupe 	*tp;			/* index into the queue */
	Syment	*se;			/* pointer to symbol in table */

	new = (Troupe *) ng_malloc( sizeof( Troupe ), "new_troupe" );
	memset( new, 0, sizeof( Troupe) );

	new->desc = d;
	new->state |= TS_PENDING;

	for( tp = p->troupes; tp && tp->next; tp = tp->next ) ;		/* find last block on queue */

	if( (se = symlook( symtab, d->name, p->progid + TROUPE_SPACE, 0, SYM_NOFLAGS )) == NULL )
	{
		if( tp )
			tp->next = new;				/* add at end */
		else
			p->troupes = new;			/* first one on the pile */

		symlook( symtab, d->name, p->progid + TROUPE_SPACE, new, SYM_NOFLAGS );
	}
	else
	{
		/* ng_bleat( 0, "duplicate troupe, info not updated: %s(prog) %s(troupe)", p->name, d->name );*/
		sbleat( 0, "duplicate troupe, info not updated: %s(prog) %s(troupe)", p->name, d->name );
		ng_free( new );
		new = NULL;			/* discard for the moment */
	}

	return new;
}



/* ------------ MISC --------------------------------------------------------------------------- */
/*	explain stuff to the log file once every few minutes or so 
*/
static void natter( )
{
	static	ng_timetype next_rant = 0;	/* time do natter on next */
	ng_timetype	now;

	if( (now = ng_now( )) > next_rant )
	{
		next_rant = now + 3000;			/* once every 5 min should be plenty since users can dump in real time if needed */
		explain_all( NULL, NULL );				/* explain all progrmmes */
	}
}

/*	runs the troupes for the programme and releases jobs if a troupe is now unblocked 
	mod:	12 Jun 2004 (sd) : now supports TS_SUBMIT state to allow submission of large
		number of jobs to be broken up in order to come up for air now and again.
*/
void run_troupes( Programme *p )
{
	Troupe *t;
	Troupe *next;
	Troupe *prev;
	int	max2do = 250;		/* if programme has LOTS of troups, we must come up for air now and again */
	
	if( !p )
		return;


	for( t = p->troupes; t; t = t->next )
	{
		next = t->next;
		if( t->waiting_on < 1 && (t->state == TS_PENDING || t->state == TS_SUBMIT)  )	
		{
			if( t->state == TS_PENDING )			/* only once */
				submit_gar( p, t );				

			if( t->jobs == NULL )
				make_jobs( p, t );

			if( ! submit_jobs( p, t ) )			/* submit next n jobs */
			{
				ng_bleat( 0, "run_troupes: failed during submit of jobs to seneschal: %s (%s)", t->desc->name, p->name );
				max2do = 0;
			}

			max2do--;
		}

		if( max2do < 1  )		/* need to come up for air? */
		{
			ng_bleat( 1, "run_troupes: work suspended for air: %s.%s", p->name, t->desc->name );
			gflags |= GF_WORK_SUSP;			/* set suspended work flag for quick recall of prog_maint */
			return;					/* and short circuit the pass on the troupe */
		}
	}
}

/*	driven for each programme symbol table entry.  do what ever we need to do to it on a regular
	basis (like seeing what troupes can be submitted to seneschal etc.)

	if called with a null syment, then it will run the traverse command as an indirect recursion
*/
void prog_maint( Syment *se, void *data )
{
	static	int	barf = 0;		/* prevents runaway if symtab should return a null pointer */
	static	ng_timetype	burble = 0;	/* next time we should make some noise */
	static	ng_timetype	checkpt = 0;	/* next time we should take a checkpoint */
	static	pid_t	last_ckpt_pid = 0;	/* pid of last one started */

	Programme *p;
	Troupe	*t;

	int 	not_done = 0;
	int	errors = 0;			/* set if a troupe is complete but has errors; causes longer purge time when all are done */
	pid_t	this_ckpt = 0;
	ng_timetype 	now;
	char	*name = NULL;			/* ckpt filename of next ckpt - to erase it */
	char	cmd[1024];			/* buffer to build command in */

	now = ng_now( );

	if( ! se )			/* no pointer, assume we called (from netif), pass ourselves to symtab to drive for each prog */
	{
		if( barf )
			return;			/* if we were invoked before the last one had a chance to be done */

		/*if( (now > checkpt || gflags & GF_NEW_PROG) && do_ckpt )*/
		if( do_ckpt && now > checkpt  )
		{
			gflags &= ~GF_NEW_PROG;					/* turn off new prog since last ckpt flag */
			
			if( (this_ckpt = ckpt_asynch( "nawab")) != last_ckpt_pid )	/* new one started -- it will allocate the real name */
			{
				last_ckpt_pid = this_ckpt;
				checkpt = now + CHECKPT_DELAY;
			}
			else
				checkpt = now + 300;				/* short delay before retry */
		}

		barf++;
		symtraverse( symtab, PROG_SPACE, prog_maint, NULL );
		barf = 0;

		if( burble <= now )
			burble = now + (verbose > 2 ? 600 : 3000);			/* dont spit info stuff for a while */
		return;
	}




	/* se passed in, assume called from symtraverse: for each toupe in the list see if all jobs are complete yet - deal with it */

	if( (p = (Programme *) se->value ) == NULL )
	{
		ng_bleat( 1, "maint: se %s @ #%x had null value", se->name, se );
		return;
	}


	if( p->purge_time > 0 )				/* we know its done, just see if its too old to keep */
	{
		if( now > p->purge_time )		/* its been done and hanging for min keep around time */
			purge_programme( p );		/* ditch the puppy */
		return;
	}


	for( t = p->troupes; t; t = t->next )	/* butcher each troupe to see if it is done;  loop MUST examine all troupes before return  */
	{						
		if( have_seneschal == SESSION_RESET )	/* if we regained sene session, then troupe status must be changed to reset to resub jobs */
		{
			submit_gar( p, t );			/* must resubmit gar info as well */
			if( t->state == TS_ACTIVE || t->state == TS_SUBMIT )
			{
				t->state = TS_PENDING;					/* force us to resubmit troupes jobs */
				switch_jobstate( t, RUNNING, UNSUBMITTED );		/* reset all running jobs to unsubmitted */
				ng_bleat( 2, "maint: troupe now in reset state; reset %s:%s", p->name, t->desc->name );
			}
			else
				ng_bleat( 2, "maint: troupe state not reset; %s:%s %d(state)", p->name, t->desc->name, t->state );
		}

		switch( t->state )
		{
			case TS_ACTIVE:
				if( t->njobs == t->cjobs )
				{
					t->state = TS_DONE;			/* mark complete */
					release_troupes( t );			/* release anything that was blocked on this troupe */
					ng_bleat( 1, "maint: all jobs completed successfully for troupe: %s (%s)", t->desc->name, p->name );
				}
				else
				{
					if( burble <= now )
						ng_bleat( 3, "maint: active troupe: %s (%s) %d(ok) %d(err) %d(jobs)", t->desc->name, p->name, t->cjobs, t->ejobs, t->njobs );
	
					if( t->njobs == (t->cjobs + t->ejobs ) )		/* all done, but errors */
					{
						t->state = TS_ERROR;
						ng_bleat(1, "all jobs completed with %d errors for troupe: %s (%s)", t->ejobs, t->desc->name, p->name );
					}
					else
						not_done++;		/* must call run_troups later */
				}
				break;
	
			case TS_PENDING:
				not_done++;

			case TS_ERROR:		
				errors = 1;		/* its done, and if all are done, but there are errors, then we hold longer before purge */
				break;

			case TS_DONE:
				break;

			case TS_SUBMIT:			/* job submission was interrupted to breathe */
				not_done++;		/* must call run_troupes later */
				break;

			default:
				if( burble <= now )
					ng_bleat( 4, "maint: inactive troupe: %s (%s) %d(jobs) %d(after)", t->desc->name, p->name, t->njobs, t->waiting_on );

				not_done++;
				break;
		}
	}

	if( not_done  )			/* not all troups are finished, go do troup level maint for the programme */
	{
		run_troupes( p );			/* run list to see if any inactive troupes are now ready to run */
	}
	else						/* all jobs have completed (does not mean all were successful) */
	{
		ng_log( LOG_INFO, "%s %s %d(id) %s(prog)", AUDIT_ID, errors ? AUDIT_PROGERROR : AUDIT_PROGEND, p->progid, p->name );
		if( p->keep_tsec )
		{
			p->purge_time = now + p->keep_tsec;		/* user defined retention */
			ng_bleat( 1, "programme info retention is user defined: %I*dts", Ii(p->keep_tsec) );
		}
		else
		{
			if( errors )				/* if errors - keep for three days before purging */
				p->purge_time = now + (36000 * 72);
			else
				p->purge_time = now + 36000;		/* default to keep for an hour */
		}
	}

}


/* 	runs the input or output queue of a desc. for each file that has a value, the var name is registered 
	in the symtab and points to the io variable string. The position of the file in the list is 
	registered in the IONUM_SPACE area of the symbol table so that the name can be referenced in the 
	command and replaced with the %i__ or %o__ names that seneschal wants. The type parameter is used
	in the %i__ construct where we expect the type to be i or o, but it could be anything needed.
*/
int register_files( long progid, Io *head, char type )
{
	extern	char	*yyerrmsg;
	extern	int	yyerrors;

	Syment	*se;
	Io	*ip;
	Value	*val;
	int	qnum = 1;			/* the order of the file in the queue; one based per seneschal needs */
	int	status = 1;			/* gets set to zero to indicate an error and cause programme load failure */
	char	*vname;
	char	*tok;
	char	m[1024];			/* message and work buffer */

	for( ip = head; ip; ip = ip->next )
	{
		if( (val = ip->val) )				/* we only need to/can register things that have value */
		{
			ng_bleat(4, "register_file: considering %s:", ip->name);
			if( (se = symlook( symtab, ip->name, progid + VAR_SPACE, 0, SYM_NOFLAGS )) != NULL )
			{
				ng_bleat( 3, "register_file: warning: file was previously registered: %s == %s", ip->name, se->value );
			}
	
			if( ip->val->type == T_vpname )			/* something like <- x = %y, then we need to save %y, not y in symtab */
			{
				sfsprintf( m, sizeof( m ), "%%%s", ip->val->sval );
				se = symlook( symtab, ip->name, progid + VAR_SPACE, strdup( m ), SYMTAB_FREE_VALUE );
			}
			else
				se = symlook( symtab, ip->name, progid + VAR_SPACE, ip->val->sval, SYM_NOFLAGS );

			if( ip->val->range && ip->val->range->iter && *ip->val->range->iter->app == '%' )	/* has iter with var -- must expand */
			{
				tok = ng_var_exp( symtab, progid + VAR_SPACE, ip->val->range->iter->app, '%', '^', ':', VEX_NEWBUF );
				if( *tok != '%' )
				{
					ng_bleat( 3, "register_files: iteration variable expanded %s --> %s", ip->val->range->iter->app, tok );
					ng_bleat( 2, "register_file: %s = %s; iteration: %s(ref) %s(pname) %s(dbname)", ip->name, se->value, ip->val->sval, ip->val->range->pname, tok );
					ng_free( ip->val->range->iter->app );
					ip->val->range->iter->app = tok;
				}
				else
				{		
					ng_free( tok );
					sfsprintf( m, sizeof( m ), "register_files: cannot resolve %s in iteration", ip->val->range->iter->app );
					ng_bleat( 0, "%s", m );
					if( ! yyerrmsg )
					{
						yyerrmsg = strdup( m );
					}
					yyerrors++;
					status = 0;
				}
			}
			else
				ng_bleat( 2, "register_file: %s = %s", ip->name, se->value );
		}
	}

	return status;
}

static void
rl_expand(Rlist *rl, Io *ip, Io **head, Io **last, int *count, int space )
{
	char *buf;
	char nname[NG_BUFFER];
	Value *v;
	Io *new;
	int i;

	if(rl == 0){
		/* we have recursed to bottom, now just expand */

		buf = ng_var_exp( symtab, space, ip->val->sval, '%', '^', ':', VEX_CONSTBUF );
		sfsprintf( nname, sizeof( nname ), "%s_%d", ip->name, *count );
		(*count)++;
		ng_bleat( 4, "rl_expand: rlist: set %s to %s", nname, buf );
		v = newvalue( T_vstring, strdup( buf ) );
		new = newio( strdup( nname ), v, NULL, ip->flags );	

		if(*last)
			(*last)->next = new;
		else
			(*head) = new;
		(*last) = new;
		return;
	}
	for(i = rl->r->iter->lo; i <= rl->r->iter->hi; i++){
		sfsprintf(nname, sizeof nname, rl->r->iter->fmt ? rl->r->iter->fmt : "%05d", i);
		/*sfsprintf(nname, sizeof nname, "%05d", i);*/
		symlook( symtab, rl->r->pname, space, nname, SYM_NOFLAGS );
		rl_expand(rl->next, ip, head, last, count, space);
	}
	symdel( symtab, rl->r->pname, space );
}

/* 
	expand io blocks that were generated when programme lines such as 
		<- %foo = file( `sed '1d' </tmp/somefile`)
	or
		<- %foo = file( "/tmp/somefile")
	were encountered.

	assuming that the either of these programme statemensts 'generates' 
	three filenames (/tmp/file[abc]) we will do two things:

		a) for each filename generated from the list or command, we add
		an io block, simulating programme lines:
			<- %foo_0 = /tmp/filea
			<- %foo_1 = /tmp/fileb
			<- %foo_2 = /tmp/filec

		b) assign variables who have the 'file()' attribute such that
		the variable %foo will be given the value %F3,%foo_0. When the 
		command line is expanded to convert %foo into a %in (e.g. %i1
		as is desired by seneschal) it expands to %F3,%i1 telling
		seneschal to start at the 1st input file (%i1), and using three 
		input files (%F3), create a list of files in a temporary 
		file and substitute the %Fn,%in token with the name of the
		temporary file. 
		

	we run from the list pointed at by ip, and copy to a new list at head.
	any io blocks in the iplist that were not generated by file() statements, 
	are moved to the new list as is and are processed later. 
*/
Io *expand_io( Programme *p, Io *ip, long progid)
{
	extern	int	yyerrors;

	Sfio_t	*f;
	Io	*new;		/* new block */
	Io	*last;		/* points to the last normal block */
	Io	*head;		/* points to the head of the list if we changed it */
	Io	*next;
	Value	*v;
	Syment	*se;
	int	delete_ip;
	
	char	*buf;
	char 	*cmd;		/* command that will give us the list of files */
	char	nname[255];	/* interative name */

	head = NULL;
	last = NULL;
#define	ADD(ii)		{ if(last) last->next = ii; else head = ii; last = ii; }

	for(; ip; ip = next){
		next = ip->next;
		delete_ip = 0;
		if(ip->val && (ip->val->type == T_bquote))
		{
			/* pull this block out, expand with one per file returned by command */
			cmd = ng_var_exp( symtab, p->progid + VAR_SPACE, ip->val->sval, '%', '^', ':', VEX_CONSTBUF );
			ip->count = 0;
			ng_bleat( 2, "expand_io: running command to create input file list: %s", cmd );
			if( (f = sfpopen( NULL, cmd, "r" )) ){
				while( (buf = sfgetr( f, '\n', SF_STRING )) ){
					sfsprintf( nname, sizeof( nname ), "%s_%d", ip->name, ip->count );
					ip->count++;
					ng_bleat( 3, "expand_io: bquote: set %s = %s", nname, buf );
					v = newvalue( T_vstring, strdup( buf ) );
					new = newio( strdup( nname ), v, NULL, ip->flags );	
					ADD(new)
				}
				sfclose( f );

				if( !ip->count ){
					sbleat( 0, "expand_io: command did not return any files: %s", cmd );
					yyerrors++;
				} else
					delete_ip = 1;		/* as long as we expanded it, we ditch it */
			} else {
				ng_bleat( 0, "expand_io: could not popen: %s: %s", cmd, strerror( errno ) );
				yyerrors++;
			}
		} 
		else 
		if(ip->val && (ip->val->type == T_rlist))		 /* pull this block out, expand rlist */
		{	
			ip->count = 0;
			rl_expand(ip->rlist, ip, &head, &last, &ip->count, p->progid + VAR_SPACE);
			delete_ip = 1;
		} 
		else
			ADD(ip)			/* add to list to be processed later */

		if(ip->filecvt){
			sfsprintf(nname, sizeof nname, "%%F%d,%%%s_0", ip->count, ip->name);
			ng_bleat( 2, "expand_io: set var %s[%d] to %s", ip->name, progid + IONUM_SPACE, nname );
			if( (se = symlook( symtab, ip->name, progid + IONUM_SPACE, 0, SYM_NOFLAGS )) )
				ng_free( se->value );
			symlook( symtab, ng_strdup(ip->name), progid + IONUM_SPACE, strdup( nname ), SYMTAB_FREE_VALUE );
			/* memory leak of above ng_strdup; this value is never free'ed */
		}
	
		if(delete_ip)
			purge_1io( ip );		/* as long as we expanded it, we ditch it */
	}

	for(ip = head; ip; ip = ip->next)
		ng_bleat(3, "expand_io file on list: %s", ip->name);

	return head;				/* returns head of the list incase we replace the lead block */
}


/* 	runs the input or output queue of a desc. registers a %on or %in value in the ionum space for the 
	file name.  Allows references to a filename in the command to easily be translated to the %on/%in
	contstruct that seneschal wants. This must be done just before the jobs for the troupe are 
	created as there will be collisions where the current troupe references files named in other troupes.
*/
void order_files( long progid, Io *head, char type )
{
	Syment	*se;
	Io	*ip;
	int	qnum = 1;			/* the order of the file in the queue; one based per seneschal needs */
	char	*vname;
	char	wbuf[NG_BUFFER];

	for( ip = head; ip; ip = ip->next )
	{
		if( (se = symlook( symtab, ip->name, progid + IONUM_SPACE, 0, SYM_NOFLAGS )) )
			ng_free( se->value );
		sfsprintf( wbuf, sizeof( wbuf ), "%%%c%d", type, qnum );
		if(ip->count >= 0){
			ip->base = qnum+1;
			sfsprintf( wbuf, sizeof( wbuf ), "%%%c%d-%d", type, ip->base, ip->count );
		} else
			sfsprintf( wbuf, sizeof( wbuf ), "%%%c%d", type, qnum );
		symlook( symtab, ip->name, progid + IONUM_SPACE, strdup( wbuf ), SYMTAB_FREE_VALUE );
		ng_bleat( 2, "order_file: in ionum space:  %s = %s (type=%c qnum=%d count=%d)", ip->name, wbuf, type, qnum, ip->count );
		qnum++;
	}
}

char *
extend(char *base, char *add, char sep)
{
	char *ret;
	int a, b;

	if( base && *base )		/* base pointer passed in, and not an immed end of string */
	{
		b = strlen(base);			/* add the string with the given separator */
		a = strlen(add)+1;
		ret = ng_malloc(a+b+1, "extend");
		memcpy(ret, base, b);
		ret[b] = sep;
		memcpy(ret+b+1, add, a);
	}
	else
	{
		ret = strdup( add );		/* no base, just duplicate the addition */
	}

	ng_free(add);
	ng_free(base);

	return ret;
}

/* add dscriptor to the end of the list */
Desc *
appenddesc(Desc *orig, Desc *add)
{
	extern Desc *mrad;

	if(orig == 0)
		orig = add;
	else {
		Desc *d;

		for(d = orig; d->next; d = d->next)
			;
		d->next = add;
	}

	mrad = add;
	return orig;
}

/* adds the woomera string to the command buffer, then frees the string and nulls the pointer
   we only need to do it once, this is called the first time we build a seneschal job
*/
void ammend_cmd( Programme *p,  Desc *d )
{
	char 	wbuf[NG_BUFFER];
	char	*ebuf;			/* we allow the woomera string to be a %name -- thus we must expand it */

	if( d->woomera )		/* should be set, but be cautious */
	{
		ebuf = ng_var_exp( symtab, p->progid + VAR_SPACE, d->woomera, '%', '^', ':', VEX_NEWBUF );
										/* seneschal wants cmd woomera <stuff> cmd <seneschal stuff> */
		sfsprintf( wbuf, sizeof( wbuf ), "woomera %s cmd %s", ebuf, d->cmd );	/* we add the lead cmd as we build the sene buffer */
		ng_free( d->cmd );
		ng_free( d->woomera );
		ng_free( ebuf );
		d->woomera = NULL;
		d->cmd = ng_strdup( wbuf );
	}
}

/* 
	puts us under the covers  -- detach and become a daemon 
	with this we dont have to worry about whether or not the user
	started with & and/or no hup
*/
void hide( )
{
	static int hidden = 0;
	int i;
	char *ng_root;
	char wbuf[1024];

	if( hidden++ )
		return;            /* bad things happen if we try this again */

	switch( fork( ) )
	{
		case 0: 	break;        		/* child continues on its mary way */
		case -1: 	perror( "hide: could not fork" ); return; 
		default:	exit( 0 );   		 /* parent abandons the child */
	}


       if( ! (ng_root = ng_env( "NG_ROOT" )) )
       {
                ng_bleat( 0, "cannot find NG_ROOT in cartulary/environment" );
                exit( 1 );
       }

/*	sfclose( sfstdin );		do NOT close these in sfio.  we need to have them open for yyparse() to work right 
	sfclose( sfstdout );		we will close and reopen the real fds underneith the sf stuff
	sfclose( sfstderr );
*/

	for( i = 3; i < OPEN_MAX; i++ )	/* close all open files, including stdin stdout */
		close( i );              
 	ng_open012( );			/* ensure that stdin/out/err are open as things may try to write to them */


        sfsprintf( wbuf, sizeof( wbuf ), "%s/site/log/nawab", ng_root );		/* log file */
	if( (bleatf = ng_sfopen( NULL, wbuf, "a" )) != NULL )
	{
		sfset( bleatf, SF_LINE, 1 );		/* force flushes after each line */
		ng_bleat_setf( bleatf );	
	}
	else
	{
		ng_log( LOG_ERR, "unable to open bleat file: %s: %s", "site/log/seneschal.log", strerror( errno ) );
		exit( 1 );
	}

        ng_free( ng_root );
}

/* 
	find an unused prog id.
*/
int Xnew_progid( )
{
	extern long progid;

	if( (progid = progid + PROGID_STEP) > PROGID_MAX )
		progid = 10;

	return progid;
}

int new_progid( )
{
	Syment	*se;
	char	progs[25];		/* proid converted to string */
	extern long progid;
	int	started;

	started = progid;

	do
	{
		if( (progid = progid + PROGID_STEP) > PROGID_MAX )
			progid = 10;				/* 0-9 reserved */

		sfsprintf( progs, sizeof( progs ), "%d", progid );
		se = symlook( symtab, progs, ID_SPACE, 0, SYM_NOFLAGS );	/* if its NULL, progid is unused */
	}
	while( se && progid != started );		/* until we find an open progid (no symtab ent) */

	if( !se )		/* the progid is safe to use */
	{
		symlook( symtab, progs, ID_SPACE, (void *) 1, SYM_COPYNM );	/* symtab makes copy of name */
		ng_bleat( 1, "newprog; assigned id=%d (%s) started=%d", progid, progs, started );
		return progid;
	}

	ng_bleat( 0, "cannot find available programme id; aborting (with dump -- we hope)" );
	sbleat( 9, "internal nawab error; nawab is going to crash" );
	ng_log( LOG_CRIT, "cannot find available programme id; crashing with dump (we hope)" );
	abort( );				/* hopefully will gvie a core dump */
}

/* ------------------- DUMP/EXPLAIN -------------------------------------------------------------------- */
static char *typestr[] = {
#include	"n_enum.h"
};

#define	nicestr(s)	((s)? (s):"<null>")

const char *tstate_str( int state )
{
	switch( state )
	{
		case TS_PENDING:	return "pending"; break;
		case TS_DONE:		return "done"; break;
		case TS_ERROR:		return "error"; break;
		case TS_ACTIVE:		return "active"; break;
		case TS_SUBMIT:		return "submit"; break;
		default:		break;
	}

	return "unknown";
}

/*
	when in really verbose mode, these routines are invoked as a programme is loaded and
	parsed to spit details to our private log
*/
void
dumpdesc(Desc *d )
{
	Io *i;
	char buf1[4000], buf2[4000];

	if( ! d )
		return;

	ng_bleat( 0, "Desc %s @0x%x  comment '%s': type=%d(%s)", 
				d->name, d, nicestr(d->desc), d->type, typestr[d->type]);
	ng_bleat( 0, "\tafter %s; consumes %s; nodes=\"%s\"; reqd_input=%s; cmd='%s'", 
			d->after, nicestr(d->rsrcs), nicestr(d->nodes), dumpvalue( d->reqdinputs, buf1 ), nicestr(d->cmd));

	for(i = d->inputs; i; i = i->next)
		dumpio(i, "\tinput" );
	for(i = d->outputs; i; i = i->next)
		dumpio(i, "\toutput" );
	ng_bleat( 0, "\tval=%s; range=%s", dumpvalue(d->val, buf1), dumprange(d->range, buf2));
	ng_bleat( 0, "\tatt=%s; pri=%s", dumpvalue(d->attempts, buf1), dumpvalue(d->priority, buf2));
}

char *
dumpvalue(Value *v, char *buf)
{
	char buf1[4000];

	if(v)
		sprintf(buf, "(%s: sval='%s' range=%s)", typestr[v->type], nicestr(v->sval), dumprange(v->range, buf1));
	else
		sprintf(buf, "(<null>)");
	return buf;
}

char *
dumprange(Range *r, char *buf)
{
	if(r == 0)
		sprintf(buf, "[]");
	else if(r->iter)
		sprintf(buf, "[%s in (%s:%s)] ", r->pname, typestr[r->iter->type], nicestr(r->iter->app));
	else
		sprintf(buf, "[%s]", r->pname);
	return buf;
}

char *
dumprlist(Rlist *rl, char *buf)
{
	if(rl == 0)
		sprintf(buf, "{}");
	else {
		strcpy(buf, "{");
		for(; rl; rl = rl->next)
			dumprange(rl->r, strchr(buf, 0));
		
		strcat(buf, "}");
	}
	return buf;
}

void
dumpio(Io *io, char *pre )
{
	char buf[NG_BUFFER];
	char buf1[NG_BUFFER];

	ng_bleat( 0, "%s io %s: val=%s rep=(%s) gar=(%s) rlist=%s filecvt=%d",  nicestr(pre), io->name, dumpvalue(io->val, buf), io->rep, io->gar, dumprlist(io->rlist, buf1), io->filecvt );
}

void dump_troupe( Troupe *t )
{
	char	buf[4000];
	int	i;
	char	*state = "unknown";

	buf[0] = 0;
	for( i = 0; t->blocked && i < t->bidx; i++ )
	{
		if( t->blocked[i] )
		{
			if( i )
				strcat( buf, "," );	
			strcat( buf, t->blocked[i]->desc->name );
		}
	}

	if( ! *buf )
		sfsprintf( buf, sizeof( buf ), "none" );
		

	ng_bleat( 0, "troupe: %s(name) %s(state) %d(jobs) %d(done) %d(err) %d(%%) %d(after) blocks: %s", 
			t->desc->name, tstate_str(t->state), t->njobs, t->cjobs, t->ejobs, (t->cjobs*100)/(t->njobs ? t->njobs : 1), t->waiting_on, buf );
}

void dump_job( Job *jp )
{
	if( jp )
		ng_bleat( 0, "job: %d(status) %s(node) (%s)", jp->status, jp->node, jp->cmd );
}

void dump_programme( Programme *p )
{
	Troupe	*t;
	ng_timetype now;

	now = ng_now( );

	ng_bleat( 0, "programme: @#%x %s(name) %d(id) %ld(age) %d(sec2purge)", p, p->name, p->progid, (now-p->subtime)/10, p->purge_time ? (p->purge_time - now)/10:-1 );

	for( t = p->troupes; t; t = t->next )
		dump_troupe( t );
}

/* indirect recursion to dump all based on symtab entries */
void dump_programmes( Syment *se, void *data )
{
	if( ! se )
	{
		symtraverse( symtab, PROG_SPACE, dump_programmes, NULL );
		return;
	}

	dump_programme( (Programme *) se->value );
}


/* ---------- PARSE ----------------------------------------------------------------------------- */
/*	parse a programme that was received and saved in the file fname 
	calls yyparse to parse which generates a list of descriptors (desc->).
	yyerror is set to the number of errors. Returns a pointer 
	to the programme block.

	The programme has been read via the TCP interface and put into a tmp file.
	The file is opened here, and dup'd to stdin because yyparse insists on 
	reading from stdin.

	data is whatever is sent after the last line in the programme (=end). This line is 
	added by ng_nreq and we expect it to be username:nodename. 
*/
Programme *parse_pgm( char *fname, char *data )
{
	extern	long	auto_progname;		/* value to use when generating a programme name suffix */
	extern	long	progid;
	extern int	yydebug;
	extern int	lineno;
	extern	int	yyerrors;
	extern	char	*yyerrmsg;

	Programme	*p = NULL;
	Syment	*se;			/* to look up progname */
	int	status;
	int	f;			/* open file for yyparse */
	char	wbuf[1000];
	char	*uname = NULL;
	char	*node = NULL;
	char	*tok;
	char	*strtok_p;


	if( (tok = strtok_r( data, ":", &strtok_p )) == NULL )			/* expect [user[:node]] as data */
		uname = ng_strdup( "nobody" );					/* neither supplied */
	else
	{
		uname = ng_strdup( tok );					/* snarf uname and check for node */
		tok = strtok_r( NULL, ":", &strtok_p );
	}

	if( ! node )
		node = ng_strdup( "unknown" );
	else
		node = ng_strdup( tok );
		
	if( yyerrmsg )
		ng_free( yyerrmsg );

	yyerrmsg = NULL;
	yyerrors = 0;
	descs = NULL;
	prog_name = NULL;		/* name given to programme by submitter */
	lineno = 1;
	yydebug = verbose > 4;		/* only if REALLY chatty */

	ng_bleat( 1, "parsing program file: %s", fname );
	if( (f = open( fname, O_RDONLY )) )
	{
		dup2( f, 0 );			/* yyparse likes things on stdin */
		yyparse();
		close( f );
	}
	else
		ng_bleat( 0, "unable to open programme input file: %s: %s", fname, strerror( errno ) );

	if( yyerrors == 0 )			/* clean parse - go make the programme and troupes */
	{
		if( ! prog_name )			/* user did not supply a name */
		{
			sfsprintf( wbuf, sizeof( wbuf ),  "programme_%x",  auto_progname++ );
			sbleat(  1, "programme name not supplied, generated: %s", wbuf );
			prog_name = strdup( wbuf );
		}
		else
		{
			if( *(prog_name+strlen( prog_name ) - 1) == '_'  ) 	/* xxxx_   we will make unique */
			{
				sfsprintf( wbuf, sizeof( wbuf ),  "%s%x", prog_name, auto_progname++ );
				sbleat(  1, "programme name (%s) converted to: %s", prog_name, wbuf );
				ng_free( prog_name );
				prog_name = strdup( wbuf );
			}
		}

		if( (se = symlook( symtab, prog_name, PROG_SPACE, 0, SYM_NOFLAGS )) == NULL )	/* see if we can find it; make if cannot */
		{
			p = (Programme *) ng_malloc( sizeof( Programme ), "parse_pgm: programme" );
			memset( p, 0, sizeof( *p ) );

			p->progid = new_progid( );
			p->name = prog_name;
			p->purge_time = 0;
			p->subtime = ng_now( );
			p->subuser = uname;
			p->subnode = node;

			if( keep_time )
			{
				if( keep_time < 60 )		/* assume days not seconds supplied */
					p->keep_tsec = keep_time * 864000;
				else
					if( keep_time > 1800 )		/* not to let them go too small */
						p->keep_tsec = keep_time * 10;
			}

			keep_time = 0;

			symlook( symtab, p->name, PROG_SPACE, (void *) p, SYM_NOFLAGS );	/* register  it */
			ng_log( LOG_INFO, "%s %s %d(id) %s(prog) %s(user) %s(node)", AUDIT_ID, AUDIT_NEWPROG, p->progid, p->name, p->subuser, p->subnode );

			node = uname = NULL;			/* prevent from being released later */
		}
		else
		{
			p = (Programme *) se->value;			/* get a pointer to the exsting one */
			sbleat( 2, "WARNING: programme %s exists, extending with non-existant troupes from submission", prog_name );
		}

		ng_bleat( 1, "parsing programme: %d %s %s %s", p->progid, p->name, p->subuser, p->subnode );


		if( parse_desc( p ) )		/* build troupe blocks */
		{
			if( set_blocks( p ) ) 		/* set up things based on after clauses - done here to allow any order of descriptor entry */
			{	
				if( verbose > 1 )
					dump_programme( p );
			}
			else
			{
				ng_bleat( 0, "parse_pgm: errors setting blocks; purging programme: %s", p->name );
				purge_programme( p );
				p = NULL;
			}
		}
		else					/* parse descriptor error of some kind (it bleats the problem) */
		{
			ng_bleat( 0, "parse_pgm: errors parsing descriptor block list, programme purged: %s (%s)", p->name, yyerrmsg ? yyerrmsg : "no additional data" );
			sbleat( 9, "ERROR: parse error during second pass (descriptor expansion) in programme: %s (%s)", prog_name , yyerrmsg ? yyerrmsg : "no additional data");
			purge_programme( p );
			p = NULL;
		}

		descs = NULL;			/* prevent any accidents */
	}
	/* error message if yyparse failed is generated by caller */

	if( yyerrors && p )	
	{
		purge_programme( p );		/* it can happen if processing descriptors fails */
		p = NULL;
	}

	if( node )			/* if not attached to a prog block, then we need to trash */
		ng_free( node );
	if( uname ) 
		ng_free( uname );

	if( p )
		gflags |= GF_NEW_PROG;		/* force a checkpoint asap */

	return p;
}

/* 
	called indirectly via symtraverse 
*/
void showvar( Syment *se, void *data )
{
	ng_bleat( 2, "showvar: space=%d %s = (%s)", se->space, se->name, se->value );
}

/*	parse the descriptors and generate troupes etc 
	descriptors are pulled from the list as they are used so they 
	are not purged later.

	returns 0 if there are problems;
*/
int parse_desc( Programme *p )
{
	extern	long progid;
	extern	char	*yyerrmsg;
	extern	int	yyerrors;

	Syment	*se;		
	Desc *d;			/* pointer into the list */
	Desc *next;
	Desc *prev = NULL;		/* next/prev for removial from list */
	Troupe *t;
	Value	*val;

	char	*rdata;			/* data that the variable name reference */
	char	*tok;			/* general string pointer */
	char	*c;			/* pointer into tok or m */
	char	m[255];			/* spot for error messgae if we need  it */
	int	status = 1;		/* assume good return */

	if( descs == NULL )		/* this smells, something is not right inside of nawab, or user submitted an empty prog */
	{
		if( ! yyerrmsg )
			yyerrmsg = ng_strdup( "parse error, most likely caused by missing job group (troupe) name." );
		yyerrors++;
		ng_bleat( 0, "no descriptors were found (likely no troupe name given in programme) %s", p->name ? p->name : "unknown prog name" );
		return 0;
	}

	if( verbose > 1 )
		for(d = descs; d; d = d->next)
			dumpdesc( d );

	ng_bleat(4, "parse_desc: descs=#%x", descs);

	d = descs;
	while( d )
	{
		descs = next = d->next;
		ng_bleat(4, "parse_desc: loop: type=%d", d->type);

		switch( d->type )
		{
			case T_djob:
				d->next = NULL;
				if( d->cmd == NULL )
				{
					status = 0;			/* cause failure */
					sbleat( 0, "ERROR: cmd missing from job %s:%s", p->name, d->desc ? d->desc : "job had no id string" );
					yyerrors++;
				}

				if( (t = new_troupe( p, d )) )		/* puts on the troupe queue */
				{
					p->purge_time = 0;				/* new troupe; reset purge time if its there */

					ng_bleat( 3, "expanding inputs" );
					d->inputs = expand_io( p, d->inputs, p->progid );	/*  expand file( ... )  input/outputs */

					ng_bleat( 3, "expanding outputs" );
					d->outputs = expand_io( p, d->outputs, p->progid );

					if( ! register_files( p->progid, d->inputs, 'i' ) )	/* add file info to the sym tab */
						status = 0;
					ng_bleat(4, "parse_desc.3: status=%d", status);
					if( ! register_files( p->progid, d->outputs, 'o' ) )
						status = 0;
					ng_bleat(4, "parse_desc.4: status=%d", status);
					if( d->range && d->range->iter && d->range->iter->app && *d->range->iter->app == '%' )	/* variable entered as appl name */
					{
						tok = ng_var_exp( symtab, p->progid + VAR_SPACE, d->range->iter->app, '%', '^', ':', VEX_NEWBUF );
						if( *tok != '%' )
						{
							ng_free( d->range->iter->app );
							d->range->iter->app = tok;
						}
						else
						{		
							sfsprintf( m, sizeof( m ), "parse_desc: cannot resolve %s in iteration", d->range->iter->app );
							sbleat( 0, "%s", m );
							if( ! yyerrmsg )
								yyerrmsg = ng_strdup( m );
							yyerrors++;
							ng_free( tok );
							status = 0;			/* cause failure */
						}
					}
					ng_bleat(4, "parse_desc.5: status=%d", status);
					
					if( status )			/* all still going well */
						make_jobs( p, t );	/* make jobs for the troupe */
				}

				break;
			
			case T_dassign:
				if( (se = symlook( symtab, d->name, p->progid + VAR_SPACE, 0, SYM_NOFLAGS )) )
				{
					ng_bleat( 2, "parse_desc: variable replaced: %s", d->name );
					ng_free( se->value );			/* trash prevous value */
				}
				else
					ng_bleat( 2, "parse_desc: new var: %s (pid=%d)", d->name, p->progid );

				val = d->val;
				if( val->type != T_vrange )
				{
					rdata = ng_malloc( strlen( val->sval )+1, "parse_desc: rdata" );
					if( val->type == T_vpname )
						sfsprintf( rdata, strlen( val->sval ) + 1, "%%%s", val->sval );  /* put back the % */
					else
						sfsprintf( rdata, strlen( val->sval ) + 1, "%s", val->sval );	/* just a name no % */
					symlook( symtab, d->name, p->progid + VAR_SPACE, rdata, SYMTAB_FREE_VALUE | SYM_COPYNM );
					ng_bleat( 3, "parse_desc: variable (%s) was saved and references (%s)", d->name, rdata );
				}
				else
					ng_bleat( 0, "parse_desc: PANIC: dont know how to assign range for %s:", d->name );

				d->next = NULL;
				purge_desc( d );		/* no longer need this descriptor block */
				break;

			default:	ng_bleat( 0, "parse_desc: unknown descriptor type (%d) for %s", d->type, d->name );
					if( ! yyerrmsg )
						yyerrmsg = ng_strdup( "no descriptor blocks found, suspect yyparse failed silently" );
					yyerrors++;
					purge_desc( d );
					return 0;
					break;
		}

		d = next;
	}

	if( verbose > 2 )
		symtraverse( symtab, p->progid+ VAR_SPACE, showvar, NULL );
	ng_bleat(4, "parse_desc: status=%d", status);
	return status;
}

/* -------------- PURGE ------------------------------------------------------------------------- */
/* 
	called indirectly through a symtraverse call  
*/
void purge_syment( Syment *se, void *data )
{
	if( se->flags & SYMTAB_FREE_VALUE )
		ng_free( se->value );		/* we allocated a new copy so we must trash it */

	symdel( symtab, se->name, se->space );		/* purge this entry completely */
}

void purge_iter( Iter * i )
{
	if( i && --i->ref <= 0 )		/* only if this was the last reference to the iteration */
	{
		if( i->app )
			ng_free( i->app );

		if( i->fmt )
			ng_free( i->fmt );

		ng_free( i );
	}
}

void purge_range( Range *r )
{
	if( r )
	{
		if( r->pname )
			ng_free( r->pname );
		purge_iter( r->iter );
		ng_free( r );
	}
}

void purge_value( Value *v )
{
	if( v )
	{
		if( v->sval )
			ng_free( v->sval );
		purge_range( v->range );

		ng_free( v );
	}
}

/*	purge one io block */
void purge_1io( Io *i )
{
	if(i->rlist){
		Rlist *r, *rr;

		for(r = i->rlist; r; r = rr){
			rr = r->next;
			ng_free( r );
		}
	}

	purge_value( i->val );

	if( i->name )
		ng_free( i->name );
	if( i->rep )
		ng_free( i->rep );
	ng_free( i );
}

/* 	purges all in the chain */
void purge_io( Io *i )
{
	Io *next;

	while( i )	
	{
		next = i->next;
		purge_1io( i );
		i = next;	
	}
}

/* 	purge a descriptor */
void purge_desc( Desc *d )
{
	if( d )
	{
		purge_value( d->val );
		purge_io( d->inputs );
		purge_io( d->outputs );
		purge_range( d->range );

		ng_free( d->name );
		ng_free( d->desc );
		ng_free( d->after );
		ng_free( d->nodes );
		ng_free( d->woomera );
		ng_free( d->cmd );

		ng_free( d );
	}
}

/* 	trash d and anything pointed to down stream of d */
void purge_desc_list( Desc *d )
{
	Desc	*next;

	while( d )
	{
		next = d->next;
		purge_desc( d );
		d = next;
	}
}


/* pure list of input/output files from a job */
void purge_flist( Flist *fp )
{
	int i;

	if( fp )
	{
		for( i = 0; i < fp->idx; i++ )
			ng_free( fp->list[i] );

		ng_free( fp->list );
		ng_free( fp );
	}
}


/*	trash all things associated with the troupe 
	expired is set if the purge_time in the programme block is before now.  When a programme 
	has expired, then we do not bother sending purge commands to seneschal as we assume 
	any traces of the jobs have been cleaned up.  If the programme is not yet expired, 
	it is likely that the user is purging the programme and we need to clean up seneschal. 
*/
void purge_troupe( Troupe *t, int expired )
{
	int i;
	long pcount = 0;

	if( t )
	{
		if( ! expired )					/* if programme had not expired; user probably purged so we work harder */
			for( i = 0; i < t->njobs; i++ )		/* must put all of our jobs on hold first, or they may be scheduled */
			{
				if( t->jobs[i].status == RUNNING  )	
					hold_seneschal_job( t->jobs[i].cmd );		
			}

		for( i = 0; i < t->njobs; i++ )			/* wash up jobs; purge any jobs that seneschal has not reported on */
		{
			if( ! expired && t->jobs[i].status >= RUNNING  ) /* must purge all jobs -- they might be on the complete queue */
			{
				purge_seneschal_job( t->jobs[i].cmd );		
				pcount++;
			}

			purge_flist( t->jobs[i].ifiles );	
			purge_flist( t->jobs[i].ofiles );	
			ng_free( t->jobs[i].cmd );
			ng_free( t->jobs[i].qmsg );
			ng_free( t->jobs[i].info );
		}

		/*
			until we better work out the user interface, this needs to be off
		ng_bleat( 1, "purge_troupe: %ld running jobs purged for troupe: %s", pcount, t->desc->name );
		*/
		purge_desc( t->desc );

		ng_free( t->blocked );
		ng_free( t->jobs );
		ng_free( t );
	}
}

/* trash the whole progamme */
void purge_programme( Programme *p )
{
	Troupe 	*next;
	char	progs[25];
	int	expired;

	if( p )
	{
		expired = p->purge_time > 0 && ng_now() > p->purge_time;

		ng_bleat( 1, "purge_programme: %s(id) purged.", p->name, p->progid );
		symtraverse( symtab, p->progid + VAR_SPACE, purge_syment, NULL );	/* must purge symbol table BEFORE anything else */
		symtraverse( symtab, p->progid + TROUPE_SPACE, purge_syment, NULL );
		symtraverse( symtab, p->progid + IONUM_SPACE, purge_syment, NULL );
		symdel( symtab, p->name, PROG_SPACE );
		ng_bleat( 2, "purge_programme: %s(id) trace: symentries purged", p->name, p->progid );

		ng_free( p->subuser );
		ng_free( p->subnode );
		ng_free( p->name );

		for( ; p->troupes; p->troupes = next )				/* now purge the actual datastructures */
		{
			next = p->troupes->next;
			purge_troupe( p->troupes, expired );
		}
		ng_bleat( 2, "purge_programme: %s(id) trace: troupes purged", p->name, p->progid );

		sfsprintf( progs, sizeof( progs ), "%d", p->progid );
		symdel( symtab, progs, ID_SPACE );			/* trash the syment that blocks this id */

		ng_bleat( 2, "purge_programme: %s(id) trace: completely purged", p->name, p->progid );
		ng_free( p );
	}
}

/* I dont think this is referenced any more 
	and if it is, then 239+ XXX needs to be changed to the right scope
*/

char *XXXbquote(Value *v, char *cmd)
{
	char *ecmd;
	char *ret;
	int aret, nret, k;
	Sfio_t *fp;
	static char *errval = "bquote cmd failed";

	ecmd = ng_var_exp( symtab, 239 + IONUM_SPACE, cmd, '%', '^', ':', VEX_NEWBUF );
	if((fp = sfpopen(0, ecmd, "r")) == NULL){
		sfprintf(sfstderr, "%s: `%s` failed: %s\n", argv0, ecmd, ng_perrorstr());
		return errval;
	}
	aret = NG_BUFFER;
	ret = ng_malloc(aret, "bquote alloc");
	nret = 0;
	while((k = sfread(fp, ret+nret, aret-nret)) > 0){
		nret += k;
		if(nret >= aret){
			aret = 1.5*aret;
			ret = ng_realloc(ret, aret, "bquote realloc");
		}
	}
	sfclose(fp);
	ret[nret] = 0;
	return ret;
}


Rlist *
newrlist(Range *r)
{
	Rlist *rl;

	rl = ng_malloc(sizeof(*rl), "new rlist");
	rl->r = r;
	rl->next = 0;
	return rl;
}
/* ---------------------- UTILITY ----------------------------- */
void siesta( int tenths )                /* short rests */
{
        struct timespec ts;

        ts.tv_sec = tenths/10;
        ts.tv_nsec = (tenths%10) * 100000000;   /* convert to nano seconds */

        nanosleep( &ts, NULL );
}

/*	lookup something in the symtab */
void 	*lookup( char *name, int space )
{
	Syment	*se;

	if( (se = symlook( symtab, name, space, 0, SYM_NOFLAGS )) != NULL )
		return se->value;
	
	return NULL;
}

/* 	susses out the var from the symbol table and converts the value to integer.
	if missing returns the default value.  beccause we are looking up an integer
	we assume its in the variable space.
*/
int ilookup( char *name, long progid, int defval )
{
	char *v;		/* value from symbol table */

	if( v = (char *) lookup( name, progid + VAR_SPACE ) )
		return atoi( v );

	return defval;
}
