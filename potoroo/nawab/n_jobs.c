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
 --------------------------------------------------------------------------------------
  Mnemonic:	n_jobs.c
  Abstract: 	This contains all job management/status checking routines for nawab
  Date:		11 January 2002
  Author: 	E. Scott Daniels

  Mod:		14 Mar 2002 (sd) : to allow _ in programme names
		21 May 2002 (sd) : to dec completed counter on resubmit all
		22 Feb 2003 (sd) : consolidated descriptor info on call to make_s_job - now 
			passes the pointer to the desc rather than most of the desc values.
		04 Dec 2003 (sd) : Corrected bug in purge seneschal job - core dump if 
			cmd did not contains a :.
		04 Jan 2005 (sd) : To add required input support
		28 Mar 2006 (sd) : added ability to use vnames in all iterations.
		13 Apr 2006 (sd) : Changed a message issued when status received for a job
			that we did not think was still in seneschal.
		20 Apr 2006 (sd) : fixed problem with lookup of lowstring variable if %vname supplied. (HBD CR)
		05 Mar 2008 (sd) : Corrected cause of core dump in get_nodes.
 --------------------------------------------------------------------------------------
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"ng_tokenise.h"

#include	"nawab.h"		/* must be before ref to repmgr */
/*
#include	"../repmgr/repmgr.h"*/	/* REP_ constants */

extern int gflags;


/*	determine a list of nodes that are members of the cluster 
	we use the -n form of the members command as we want all nodes 
	even if they have not participated in an election in a bit.
	if user supplied <nodes("ng_species FreeBSD")>  or similar, 
	then we run the command that was in the quotes.
*/
static char *get_nodes( char *icmd )
{
	Sfio_t	*f;
	int	i;
	char	*buf;		/* pointer to buf in sfio land */
	char 	*rs = NULL;	/* return string - must dup before we close pipe */

	if( (f = sfpopen( NULL, icmd ? icmd : "ng_members -r -n", "r" )) != NULL )
	{

		buf = sfgetr( f, '\n', SF_STRING );
		if( buf == NULL )
			return NULL;

		rs = strdup( buf );

		sfclose( f );

		for( i = 0; rs[i]; i++ )
		{
			if( rs[i] == ' ' )
			{
				if( *(rs+i+1) )
					rs[i] = ',';
				else
					rs[i] = 0;		/* trunc the trailing blank */
			}
		}
	}
	
	return rs;
}

/* ---------------------- utils ------------------------------- */
/* reset the jobs with curstate to new state for troupe */
void switch_jobstate( Troupe *t, int curstate, int newstate )
{
	int i;

	for( i = 0; i < t->njobs; i++ )
		if( t->jobs[i].status == curstate )
			t->jobs[i].status = newstate;
}

/* 	make an NG_dbname_vname or NG_vname variable and look it up; return the integer value */
long env_val( char *vname, char *dbname, long defaultv )
{
	const char	*c;
	char	buf[2000];

	if( dbname )
		sfsprintf( buf, sizeof( buf ), "NG_%s_%s", dbname, vname );
	else	
		sfsprintf( buf, sizeof( buf ), "NG_%s", vname );

	if( (c = ng_env_c( buf )) != NULL )
		return atol( c );

	ng_bleat( 1, "env_val: unable to find variable in cartulary/environment: %s", buf );

	return defaultv;
}


/* 	split a job name (troupe.prog_num) into its components 
	prog may contain underscores e.g. aaa.PJ_123455_65432_0 aaa is 
	troupe 0 is num and 123455_65432 is the prog name
	p=prog name pointer
	t=troupe name pointer
	n= pointer to the string to parse.
	WARNING: destroys the name!
*/
void split_name( char *n, char **p, char **t, int *num )
{
	char	*tok;
	
	*num = -1;
	*t = n;			/* trope is ttttt_ */
	*p = "no_prog_name";

	if( (tok = strchr( n, '.' )) )		/* at first . -- separates troupe from prog allowing both to have _s */
	{
		*tok = 0;
		*p = tok + 1;

		if( (tok = strrchr( tok+1, '_' )) )		/* at last _ */
		{
			*tok = 0;
			*num = atoi( tok+1 );
		}
	}
}

/*
	send a list of files to seneschal
	returns 0 on failure
*/
static int sendfiles( Flist *fp, char *def )
{
	int i;
	int status = 1;

	if( !fp )
		return( send2seneschal( def ) );

	for( i = 0; i < fp->idx && status; i++ )
		if( fp->list[i] )
			status = send2seneschal( fp->list[i] );

	return status;
}

/*
	send misc info about a job to seneschal (send before cjob= message)
	jname is the 'basename' for the job, including the under bar (_) that
	precedes the job number; we add the job number given that we are already
	building a string.
*/
static int sendmisc( char *jname, int jnum, char *uname )
{
	int i;
	int status = 1;
	char	buf[1024];

	if( !jname || ! uname )
		return 1;

	sfsprintf( buf, sizeof( buf ), "user=%s%d:%s\n", jname, jnum,  uname );

	if( ! send2seneschal( buf ) )
		return 0;

	return 1;
}

/* ---------------------- job stuff ------------------------------- */
/* runs t's blocked list and frees each troupe that was waiting on t */
void release_troupes( Troupe *tp )
{
	int	i;

	for( i = 0; i < tp->bidx; i++ )
	{
		tp->blocked[i]->waiting_on--;
		ng_bleat( 1, "release_troupes: %s nonlonger waititing on %s; waits on %d other troupes", 
				tp->blocked[i]->desc->name, tp->desc->name, tp->blocked[i]->waiting_on );
	}

	tp->bidx = 0;
	
	return;
}

/*	blocks all troupes that have an after clause 
	returns 1 if we think all is ok
  -----------------------------------------------------------------------------------
*/
int set_blocks( Programme *p )
{
	extern	char *yyerrmsg;
	extern	int	yyerrors;

	Troupe	*tp;		/* at the current troupe */
	Troupe	*stopper;	/* pointer to troupe that blocks tp */
	char	**blist = NULL;	/* points at a list of troupe names that we block after */
	int	bcount;		/* number of troupes in the list */
	int	i;
	int 	j;
	int	rc = 1;		/* status to return */
	char	msg[1000];

	for( tp = p->troupes; tp; tp = tp->next )
	{
		if( tp->desc->after && *tp->desc->after )			/* if this troupe is blocked until one or more finish */
		{
			bcount = 0;
			blist = ng_tokenise( tp->desc->after, ',', '\\', blist, &bcount );	/* tokenise the list */
			ng_bleat( 1, "set_blocks: %s will be blocked on %d troupe(s): %s", tp->desc->name, bcount, tp->desc->after );

			for( i = 0; i < bcount; i++ )
			{
				if( (stopper = (Troupe *) lookup( blist[i], TROUPE_SPACE + p->progid )) )
				{
					if( stopper->state == TS_PENDING || stopper->state == TS_ACTIVE )
					{
						for( j = 0; j < stopper->bidx && stopper->blocked[j] != tp; j++ );

						if( j >= stopper->bidx )	/* not already blocking this one */
						{
							tp->waiting_on++;					/* num this troupe waits on */
							if( stopper->bidx >= stopper->nblocked  )
							{
								stopper->nblocked += 10;
								stopper->blocked = ng_realloc( stopper->blocked, stopper->nblocked * sizeof( Troupe *), "block" );
							}

							stopper->blocked[stopper->bidx++] = tp;			/* points at all that it blocks */
						}
					
					}
				}
				else
				{
					sfsprintf( msg, sizeof( msg ), "set_blocks: could not find troupe in after clause on: %s;  searching for: (%s)", tp->desc->name, blist[i] );
					sbleat( 0, msg );
					if( ! yyerrmsg )
						yyerrmsg = strdup( msg );
					rc = 0;
					yyerrors++;			/* though we are not, the outside world sees us as a part of the yyparser */
				}
			}	

			ng_bleat( 1, "set_blocks: %s now waiting on %d troupes", tp->desc->name, tp->waiting_on );
		}
	}

	ng_free( blist );

	return rc;
}

/*	parse a status buffer that was received from seneschal for a job
	expect: jobname:nodename:status:data
	job name is expected to be: <troupname>.<progname>_<jobnum>

	if mode is RECOVERY_MODE, then we do not log the message
  -----------------------------------------------------------------------------------
*/
void parse_status( char *buf, int mode )
{
	Programme *p;
	Troupe	*tp;			/* troupe that owns the job */
	Syment	*se;

	int	jobnum;			/* job number (index into job array) */
	int	jobstat = 0;		/* job completion status 0 is good */
	char	*pname;			/* programme name */
	char 	*tname;			/* troupe name */
	char	*jobname;		/* pointer to various parts of the buffer */
	char	*node;			/* node where job executed */
	char	*data = NULL;
	char	*tok;
	char	*strtokp;		/* pointers needed to use strtok_r */
	char	*info = NULL;

	if( strcmp( buf, "." ) == 0  )	/* end of data message from seneschal -- we just ignore */
		return;

	info = strdup( buf );		/* keep if a failure; we destroy copy in buf w/ strtok */

	if( mode != RECOVERY_MODE )
		ng_log( LOG_INFO, "nawab_status: %s", info );		/* log for check point recovery */

	jobname = strtok_r( buf, ":", &strtokp );
	node = strtok_r( NULL, ":", &strtokp );
	if( (tok = strtok_r( NULL, ":", &strtokp )) )
	{
		jobstat = atoi( tok );
		data = strtok_r( NULL, ":", &strtokp );
	}

	if( ! jobname || ! node ) 		/* data is allowed to be NULL */
	{
		ng_bleat( 0, "parse_status: bad status buffer: job(%s) node(%s) data(%s)", jobname, node, data );	
		ng_free( info );
		return;
	}

	split_name( jobname, &pname, &tname, &jobnum );
	if( ! pname || ! tname )
	{
		ng_bleat( 0, "parse_status: bad job name: pname(%s) tname(%s)", pname, tname );	
		ng_free( info );
		return;
	}

	if( (p = (Programme *) lookup( pname, PROG_SPACE )) != NULL )
	{
		if( (tp = (Troupe *) lookup( tname, TROUPE_SPACE + p->progid )) != NULL )
		{
			if( jobnum >= 0 || jobnum <tp->njobs )			/* valid job number */
			{
				if( tp->jobs[jobnum].status == RUNNING )
				{
					switch( jobstat )
					{
						case 0:
							tp->jobs[jobnum].status =  FIN_OK;
							tp->cjobs++;
							ng_bleat( 2, "parse_status: job completed ok %s.%s_%d troupe is %d%% finished", 
								tname, pname, jobnum, ((tp->cjobs+tp->ejobs)*100)/tp->njobs );
							break;

						default:
							tp->ejobs++;
							tp->jobs[jobnum].status =  FIN_BAD;
							tp->jobs[jobnum].info =  info;
							ng_bleat( 1, "parse_status: job failed %s.%s_%d troupe is %d%% finished status: (%s)", 
								tname, pname, jobnum, ((tp->cjobs+tp->ejobs)*100)/tp->njobs, info );
							info = NULL;				/* prevents free later */
							break;
					}
				}
				else
					if( mode != RECOVERY_MODE )		/* job not running expected in recovery mode, so be slient */
						ng_bleat( 1, "parse_status: job status received, job state was not running: %s(job) %d(state) %d(status) %s(troupe) %s(prog)", jobname, tp->jobs[jobnum].status, jobstat, tname, pname );
			}
			else
				ng_bleat( 0, "parse_status: job number out of range on status: %s(pname) %s(troupe) %d(num)", pname, tname, jobnum );
		}
		else
			ng_bleat( 0, "parse_status: cannot find troupe in symtab: %s(prog) %d %s(troupe)", pname, p->progid, tname );
	}
	else
		ng_bleat( 0, "parse_status: cannot find programme in symtab: %s(prog) %s(troupe)", pname, tname );

	ng_free( info );
}


/*	make a filename with an senechal range {start-stop}
	it->app == dbname 
	Right now we support %p and %Pnum.  
*/
char *mk_iter_file( Programme *p, Io *ip, Iter *it )
{
	long	nparts;			/* number of partitions in the referenced database */
	char	*refvar;		/* reference var from another step */
	char	*stuff = NULL;		/* pointer to string to return */
	char	wrk[NG_BUFFER];
	char	*tok;			/* pointer into string for reference */
	char	*pat;			/* variable pattern in reference file to match and replace */
	char	hold;			/* we play in refvar, hold what we flip */

	*wrk = 0;
	pat = ip->val->range->pname;			/* %p or %Pnum */

	if( (nparts = env_val( "PARTS", it->app, 0  )) < 2 )			
	{
		ng_bleat( 0, "mk_iter_file: bad nparts value found: %d %s(app) %s(prog)", nparts, it->app, p->name );
		return NULL;
	}

		
	if( (refvar = (char *) lookup( ip->val->sval, VAR_SPACE + p->progid )) )
	{
		if( (tok = strstr( refvar, pat )) != NULL )
		{
			hold = *tok;
			if( strcmp( pat, "Pnum" ) == 0 )		/* assume junk%Pnumjunk --> junk{range}junk */
			{
				if( tok > refvar )
				{
					*tok = 0;
					sfsprintf( wrk, sizeof( wrk ), "%s{0-%d}", refvar, nparts-1 );
					*tok = hold;
					tok += strlen( pat );
					strcat( wrk, tok );		/* add any junk that follows */
				}
			}
			else				/* assume %p and thus we generate:  ds-dbname-{range}junk */
			{
				tok += strlen( pat );
				sfsprintf( wrk, sizeof( wrk ), "ds-%s-{0-%d}%s", it->app, nparts-1, tok );
			}

			stuff = strdup( wrk );
		}
		else
			ng_bleat( 0, "mk_iter_file: did not find pattern (%s) in file variable %s", pat, refvar );
	}
	else
		ng_bleat( 0, "mk_iter_file: did not reference variable name (%s)", ip->val->sval );
	
	return stuff;
}

/*	
	Parses an Io list (*ip) and adds each file to the input/output flist passed in.
	The info added to the flist is in the form that can be sent directly to seneschal.
	The replication factor is expected to be a string of the form:
		<symbolic-name> | default | every 
	Symbolic name strings can be preceded with a replication count making the 
	string something like: 2,Leader meaning keep two coppies and one must be on th e
	leader, or 4,BOSS_ABS_311 meaning keep 4 copies with one being on the node that 
	has registered  itself with replication manager using the symbolic name BOSS...
	Replication manager assumes that symbolic names begin with a capital letter, Nawab
	will NOT validate this allowing name to be the actual node name (not recommended, but 
	possible). 


	creates/extends a file list pointed to by fp. The list will contain input or output 
	definitions that can be sent to seneschal and added to an incomplete job.
	type is "input" or "output" which corresponds to the id string needed by senschal 
	at front to id file type  (input=jobname:stuff). If the file's ignore size flag is 
	set, the type is augmented to have ig added (e.g. inputig). This will cause senschal
	to ignore the file's size when computing job size.
	returns the pointer to the file list created or pointer if one was passed in.
  --------------------------------------------------------------------------------------------
*/
Flist *add_files( Programme *p, char *jname, Flist *fp, char *type, Io *ip, char *md5, char *pname )
{
	extern	yyerrors; 

	Syment	*se;			/* for fname lookup */
	char	*rep;
	char	*ebuf;			/* buffer with variable names expanded */
	char	*ifname;		/* iterative file name */
	char	buf[NG_BUFFER];
	char	gbuf[128];		/* buf to build gar info in */
	char	itype[128];



	
	sfsprintf( itype, sizeof( itype ), "%sig", type );
	while( ip )
	{
		/* seneschal needs input[ig]/output=<jobname>:<data> */
		sfsprintf( buf, sizeof( buf ), "%s=%s:", ip->flags & FLIO_IGNORESZ ? itype : type, jname );	

		if( !fp )			/* need to create a file list */
		{
			fp = (Flist *) ng_malloc( sizeof( Flist ), "addfiles:flist block" );
			memset( fp, 0, sizeof( Flist ) );
		}

		if( fp->idx >= fp->nalloc )			/* need more */
		{
			fp->nalloc += 20;
			fp->list = ng_realloc( fp->list, sizeof( char * ) *  fp->nalloc, "addfiles:realloc fp->list" );
			memset( &fp->list[fp->idx], 0, sizeof( char *) * 20 );
		}

		if( md5 )
		{
			strcat( buf, md5 );		
			strcat( buf, "," );
		}

		if( ip->val && ip->val->type == T_vrange ){			/* create an iteration file */
			if( (ifname = mk_iter_file( p, ip, ip->val->range->iter )) != NULL )
			{
				strcat( buf, ifname );
				ng_free( ifname );
			}
			else
				ng_bleat( 0, "add_files: could not properly compose iterative file name for: %s", ip->name );
		} 
		else 
		if( ip->val && ip->val->type == T_rlist )
		{
			/* do nothing! */
		} 
		else 
		{
			if( (se = symlook( symtab, ip->name, p->progid + VAR_SPACE, 0, SYM_NOFLAGS )) )  /* register iteration var buffer */
			{
				ebuf =  ng_var_exp( symtab, p->progid + VAR_SPACE, se->value, '%', '^', ':', VEX_CONSTBUF ); 		/* expand variables */
				strcat( buf, ebuf );
			}
			else
				ng_bleat( 0, "addfiles: cannot find %%%s in symtab", ip->name );
		}

		if( ip->gar )				/* if there is a gar for this io blk, then we add the NAME for seneschal to look up */
		{
			sfsprintf( gbuf, sizeof( gbuf ), ",%s_%s", pname, ip->name );	/* gar name is a combination to prevent collisions */
			strcat( buf, gbuf );
		}

		if( ip->rep )					/* replication factor */
		{
			strcat( buf, "," );			/* add replication string after a comma */
			strcat( buf, ip->rep );
		}
	

		ng_bleat( 3, "add_files: [%d] %s", fp->idx, buf );
		strcat( buf, "\n" );				/* seneschal wants newline terminated messages */
		fp->list[fp->idx++] = strdup( buf );		/* save the string that goes to seneschal */
		ip = ip->next;
	}

	return fp;
}


/*	create a seneschal job from various things. Assumes that any iteration variable that might be 
	in the buffer has been created and set before this routine is called. variables in the command 
	buffer are expanded before the buffer is added to the seneschal command string. the expansion 
	is done using the IONUM_SPACE in order to convert input/output variables to the order numbers
	that seneschal expects. using the ionum_space and a second expansion allows the user to write 
	the variable name defined in the list of inputs and outputs rather than the %i_ or %o_ that 
	seneschal wants. 

	seneschal command syntax:
 	jobname:max-attempts:size:priority:resourcelist:{nodename|*}:{input-files|*}:{output-files|*}:command
	the command portion must include all woomera information that would follow the colon on a womera
	command, including the cmd token.

	we make incomplete jobs (ijob=) as the input/output file names are not in the job string. They 
	are sent as separate messages so we do not limit the number of files by the max buffer len.

	NOTE: we canNOT take the nodes from the descriptor as sometimes the caller has a different set for 
	us to use. 
  -------------------------------------------------------------------------------------------------------------
*/
/*void make_s_job( Programme *p, Job *jp, char *jobname, int jobnum, char *rsrcs, Value *priority, Value *attempts, char *node, Io *inputs, Io *outputs, char *cmd )
*/
void make_s_job( Programme *p, Job *jp, Desc *d, int jobnum, char *node )
{
	char tcmd[NG_BUFFER];		/* work buffer */
	char	*ecmd;			/* command expanded to put in file order numbers for seneschal */
	int	job_size = 300;		/* dont know where the real number will come from, this will do for now */
	int	pri = 40;
	int	att = 3;
	int	reqdinputs = 0;		/* min files needed to start job; 0 == all files must be on a node to start job */
	char	full_name[1024];	/* full job name:  stepname_progname_jobnum */

	char	*jobname = d->name; 	/* we will map things that d points to to these for easier reference */
	char	*rsrcs = d->rsrcs; 
	char	*ersrcs = NULL;		/* expanded resources */
	Value	*priority = d->priority; 
	Value	*attempts = d->attempts; 
	Io	*inputs = d->inputs; 
	Io	*outputs = d->outputs; 
	char	*cmd = d->cmd;
	

	if( priority && priority->sval )
	{
		if( priority->type == T_vpname )				/* string is a var name */
			pri = ilookup( priority->sval, p->progid, pri );	/* convert from var name */
		else
			pri = atoi( priority->sval );
	}

	if( attempts && attempts->sval )
	{
		if( attempts->type == T_vpname )				/* string is a var name */
			pri = ilookup( attempts->sval, p->progid, att );	/* convert from var name */
		else
			att = atoi( attempts->sval );
	}

	if( d->reqdinputs && d->reqdinputs->sval )
	{
		if( d->reqdinputs->type == T_vpname )				/* string is a var name */
			pri = ilookup( d->reqdinputs->sval, p->progid, reqdinputs );	/* convert from var name */
		else
			reqdinputs = atoi( d->reqdinputs->sval );
	}

	if( rsrcs )
		ersrcs = ng_var_exp( symtab, p->progid + VAR_SPACE, rsrcs, '%', '^', ':', VEX_NEWBUF );

	sfsprintf( full_name, sizeof( full_name ), "%s.%s_%d", jobname, p->name, jobnum );		/* needed by add_files calls too */
                                /*             0  1  2  3  4         5   6  */
	sfsprintf( tcmd, sizeof( tcmd ), "ijob=%s:%d:%d:%d:Rnawab %s:%s:%d:", full_name, att, job_size, pri, ersrcs ? ersrcs : "", node ? node : "", reqdinputs ); 

	strcat( tcmd, "::cmd " );			/* cmd string has no filenames, listed with separate messages */
	ecmd = ng_var_exp( symtab, p->progid + IONUM_SPACE, cmd, '%', '^', ':', VEX_CONSTBUF ); 	/* replace i/o names with %i_ & %o_ */
	strcat( tcmd, ecmd );
	ng_bleat(2, "make_s_job: tcmd=(%s)", tcmd );			/* bleat before we add newline -- keeps log cleaner */
	strcat( tcmd, "\n" );									/* seneschal commands are newline termiated */

	if(verbose >= 3)
		symtraverse( symtab, p->progid+ IONUM_SPACE, showvar, NULL );

	jp->ifiles = add_files( p, full_name, jp->ifiles, "input", inputs, "noMD5", p->name );	/* right now we dont have md5 info for files, but seneschal wants something */

	jp->ofiles = add_files( p, full_name, jp->ofiles, "output", outputs, NULL, p->name );	

	jp->cmd =  ng_var_exp( symtab, p->progid + VAR_SPACE, tcmd, '%', '^', ':', VEX_NEWBUF ); 		/* expand variables */
	if( !jp->cmd )
		ng_bleat( 0, "ERROR: var_exp null: (%s)", tcmd );
	else
		ng_bleat( 2, "jp->cmd=(%s)", jp->cmd );

	sfsprintf( tcmd, sizeof( tcmd ), "cjob=%s\n", full_name );	
	jp->qmsg = strdup( tcmd );								/* cause seneschal to treate job as complete */
}
	
/* ---------------------------------------------------------------------------------------------------------------------
	For all of the make_X_jobs() functions:
	ival is a pointer to a character buffer where we put the current iteration value (node name, filename, etc ). The
	buffer is referenced by the symbol table using the var name (e.g. %n) that was defined in the user programme on 
	the iteration statement.  Putting our value into the buffer, allows the call to make_s_jobs to expand vars in 
	the command such that the iteratiion var expands to the right thing.  Thus the caller must set up the 
	proper link in the symbol table, and pass us the address to fill in. 
  ----------------------------------------------------------------------------------------------------------------------
*/

/*	create a job array based on a list of nodes in the descriptor. node list is expected to be a comma separated list of 
	nodes on which the job must run. 
*/

void make_node_jobs( Programme *p, Troupe *t, char *ival )
{
	int	nnodes;		/* number of nodes in the list */
	int	i;
	char	**nodes;	/* pointer to node tokens */
	char	*enodes;	/* list with %var expansions applied */

	if( t && t->desc && t->desc->nodes )
	{
		enodes = ng_var_exp( symtab, p->progid + VAR_SPACE, t->desc->nodes, '%', '^', ':', VEX_NEWBUF );
		nodes = ng_tokenise( enodes, ',', '\\', NULL, &nnodes );	/* tokenise the list */
		t->njobs = nnodes;

		if( nnodes )
		{
			t->jobs = (Job *) ng_malloc( sizeof( Job ) * nnodes, "make_jobs: node_job" );
			memset( t->jobs, 0, sizeof( Job ) * nnodes );

			for( i = 0; i < nnodes; i++ )
			{
				if( ival )
					strcpy( ival, nodes[i] );
				make_s_job( p, &t->jobs[i], t->desc, i, nodes[i] );
				sfsprintf( t->jobs[i].node, sizeof( t->jobs[i].node ), "%s", nodes[i] );  /* must save w/job for resubmit */
			}
		}
		else
			ng_bleat( 1, "empty node list after tokenising for troupe: %s", t->desc->name );

		ng_free( nodes );
		ng_free( enodes );
	}
} 

/*	makes jobs expanding the iteration variable with the format string passed in and the partition number
	allows the caller to build the datastore name prefix into the format string and thus have %p
	expand to ds-dsname-pnum or to just supply a %04d so that just the partition number is used
	ivalue is the buffer that has been registered in the symtab using the iteration variable name
	fmt - sprintf format string with one %04d embedded 
	ivalue - pointer to the buffer that has been registered with the iteration variable name
	we will maintain the variable Pnum so that when partitions(dbn) is used such that '%p' generates
	the whole name, the programme step can also have access to the part number alone as that is likely
	to be necessary.
  -------------------------------------------------------------------------------------------------------------
*/
void make_part_jobs( char *fmt, Programme *p, Troupe *t, Iter *it, char *ivalue  )
{
	Desc	*d;			/* associated descriptor */
	Job	*jp;

	long	nparts;			/* number of partitions */
	int	i;
	char	*tok;			/* token to do something with at some time */
	char	pnum_buf[10];		/* for 'system' supplied variable */

	d = t->desc;		/* shortcut */

	if( (nparts = env_val( "PARTS", it->app, 0  )) < 1 )
	{
		ng_bleat( 0, "make_jobs: bad nparts value found: %d %s(app) %s(troupe) %s(prog)", nparts, it->app, d->name, p->name );
		return;
	}

	t->njobs = nparts;
	t->jobs = (Job *) ng_malloc( sizeof( Job ) * nparts, "make_part_jobs: job" );
	memset( t->jobs, 0, sizeof( Job ) * nparts );

	if( d->nodes && (tok = strchr( d->nodes, ',' )) != NULL )
	{
		*tok = 0;			/* allow only one node here */
		ng_bleat( 0, "make_jobs: WARNING: nodes was a list for a partition iteration, used first: %s", d->nodes );
	}

	symlook( symtab, "Pnum", p->progid + VAR_SPACE, pnum_buf, SYM_NOFLAGS );		/* make Pnum available */
	for( i = 0; i < nparts; i++ )	
	{
		sprintf( ivalue, fmt, i );  		/* set iteration value */
		sprintf( pnum_buf, "%04d", i );
		jp = &t->jobs[i];
		make_s_job( p, jp, d, i, d->nodes );
		jp->node[0] = 0;
	}
	symdel( symtab, "Pnum", p->progid + VAR_SPACE );
}

/*	makes jobs expanding the iteration variable with the format string passed in and the partition number
	allows the caller to build the datastore name prefix into the format string and thus have %p
	expand to ds-dsname-pnum or to just supply a %04d so that just the partition number is used
	ivalue is the buffer that has been registered in the symtab using the iteration variable name
	fmt - sprintf format string with one %04d embedded 
	ivalue - pointer to the buffer that has been registered with the iteration variable name
	we will maintain the variable Pnum so that when partitions(dbn) is used such that '%p' generates
	the whole name, the programme step can also have access to the part number alone as that is likely
	to be necessary.
  -------------------------------------------------------------------------------------------------------------
*/
void make_iter_jobs( Programme *p, Troupe *t, Range *r )
{
	extern 	int	yyerrors; 

	Desc	*d;			/* associated descriptor */
	Job	*jp;

	Syment	*se;
	int	i;
	char	*tok;			/* token to do something with at some time */
	char	varval[NG_BUFFER];	/* buffer to put pnum's value into for expansion calls in  make_s_job */
	char	*ebuf;			/* pointer to buffer with expanded variable value */
	int	n;
	int	lo = 0;			/* converted lo/hi values */
	int	hi = 1;

	d = t->desc;		/* shortcut */


	if( isdigit( *r->iter->histr ) )
		hi = atoi( r->iter->histr );
	else
	{
		if( (se = symlook( symtab, r->iter->histr, p->progid + VAR_SPACE, NULL, 0 )) )
			hi = atoi( se->value );
		else
		{
			yyerrors++;
			sbleat( 0, "ERROR: unable to expand hi value in range: %s",  r->iter->histr );
			return;
		}
	}

	if( isdigit( *r->iter->lostr ) )
		lo = atoi( r->iter->lostr );
	else
	{
		if( (se = symlook( symtab, r->iter->lostr, p->progid + VAR_SPACE, NULL, 0 )) )
			lo = atoi( se->value );
		else
		{
			yyerrors++;
			sbleat( 0, "ERROR: unable to expand lo value in range: %s",  r->iter->lostr );
			return;
		}
	}

	/*n = (r->iter->hi - r->iter->lo) + 1;*/
	t->njobs = n = (hi - lo) + 1;			 /* remember, hi is inclusive so 0..1 is two jobs! */

	t->jobs = (Job *) ng_malloc( sizeof( Job ) * n, "make_iter_jobs: job" );
	memset( t->jobs, 0, sizeof( Job ) * n );

	if( d->nodes && (tok = strchr( d->nodes, ',' )) != NULL )
	{
		*tok = 0;			/* allow only one node here */
		ng_bleat( 0, "make_jobs: WARNING: nodes was a list for a lo..hi iteration, used first: %s", d->nodes );
	}

	symlook( symtab, r->pname, p->progid + VAR_SPACE, varval, SYM_NOFLAGS );		/* make Pnum available */
	for( i = 0; i < n; i++ )	
	{
		sprintf( varval, r->iter->fmt ? r->iter->fmt : "%05d", i+lo );
		jp = &t->jobs[i];
		make_s_job( p, jp, d, i,  d->nodes );
		jp->node[0] = 0;
	}
	symdel( symtab, r->pname, p->progid + VAR_SPACE );
}

#include "n_fiterator.c"

/*	creates the job array, then makes the jobs that are eventually submitted to seneschal 
  -------------------------------------------------------------------------------------------------------------
*/
void make_jobs( Programme *p, Troupe *t )
{
	Desc	*d;			/* associated descriptor */
	Range	*r;
	Iter	*it;
	Job	*jp;

	long	nparts;			/* number of partitions */
	long 	nnodes;			/* number of nodes */
	int	i;
	char	*applname;		/* application that this job applies to */
	char	ivalue[2000];		/* iteration value string (nodename, partnum etc) */
	char	fmt[50];		/* spot to build format statement in */
	char	*tok;			/* token to do something with at some time */
	

	d = t->desc;

	if( d->woomera )	
		ammend_cmd( p, d );		/* add woomera stuff to cmd, and remove the woomera string/ptr */

	if( t->jobs )
	{
		ng_bleat( 0, "make_jobs: troupe already has jobs; none made: %s (%s)", d->name, p->name );
		return;
	}
		
	order_files( p->progid, d->inputs, 'i' );			/* set input and output order of files */
	order_files( p->progid, d->outputs, 'o' );

	ng_bleat( 3, "make_jobs: making jobs for: %s", d->name );
	if( d->range )		/* an iteration of sorts */
	{
		if( ((r=d->range)->iter) != NULL )
		{
			ng_bleat( 3, "make_jobs: %s(name) job has an iteration iv=%s", d->name, r->pname );
			symlook( symtab, r->pname, p->progid + VAR_SPACE, ivalue, SYM_NOFLAGS );  /* register iteration var buffer */

			switch( (it=r->iter)->type )
			{
				case T_inodes:					/* nodal iteration */
					if( ! d->nodes )			/* we must create a list of all nodes */
				 		d->nodes = get_nodes( r->iter->app );

					make_node_jobs( p, t, ivalue );	
					break;

				case T_ipartitions:				/* partition (datastore name) iteration */
					sfsprintf( fmt, sizeof( fmt ), "ds-%s-%%04d", it->app );
					make_part_jobs( fmt, p, t, it, ivalue  );
					break;

				case T_ipartnum:				/* partition number iteration */
					make_part_jobs( "%04d", p, t, it, ivalue  );
					break;

				case T_iint:					/* int iterator */
					make_iter_jobs(p, t, r);
					break;

				case T_ipipe:				/* both handled with file_jobs */
				case T_ifile:				/* a job for each line in the file */
					make_file_jobs( p, t, it, r->pname );
					break;

				default:
					ng_bleat( 0, "make_jobs: unrecognised iteration type for troupe %s (%s)", d->name, p->name );
					break;
			}

			symdel( symtab, r->pname, p->progid + VAR_SPACE );  			/* trash the iteration var */
		}
		else
			ng_bleat( 0, "make_jobs: no iteration block for troup: %s (%s)", d->name, p->name );
	}
	else		
	{
		if( t->desc->nodes && *t->desc->nodes != '{' )		/* job applied to a list of nodes supplied in the programme */
		{
			ng_bleat( 3, "make_jobs: making one for specific list of nodes: %s", t->desc->nodes );
			make_node_jobs( p, t, NULL );	
		}
		else
		{
			ng_bleat( 3, "make_jobs: making one job for any node" );
			t->njobs = 1;
			t->jobs = (Job *) ng_malloc( sizeof( Job ) * t->njobs, "make_jobs: single job" );
			memset( t->jobs, 0, sizeof( Job ) * t->njobs );
			make_s_job( p, &t->jobs[0], d, 0, d->nodes );
		}
	}


	if( t->jobs && verbose > 1 )
	{
		ng_bleat( 1, "make_jobs: %s(prog) %s(troupe) %d(jobs) dumping first %d jobs", p->name, t->desc->name, t->njobs,  
				(verbose > 3) ? t->njobs : (t->njobs > 20 ? 20 : t->njobs) );
		for( i = 0; i < ((verbose > 3) ? t->njobs : (t->njobs > 20 ? 20 : t->njobs)); i++ )
			dump_job( &t->jobs[i] );
	}
	else
		if( ! t->jobs )
			ng_bleat( 0, "make_jobs: no jobs created for troupe %s (%s)", t->desc->name, p->name );
}

/* 	send all gar definitions for the programme. gar names are given as progname_ioname
	returns 0 on failure
  -------------------------------------------------------------------------------------------------------------
*/
void submit_gar( Programme *p, Troupe *tp )
{
	Io	*iop;
	char	gar[NG_BUFFER];		/* where we build the initial gar stuff */
	char	*eptr;			/* pointer to expanded gar stuff */

	for( iop = tp->desc->inputs; iop; iop = iop->next )
	{
		if( iop->gar )
		{
			sfsprintf( gar, sizeof( gar ), "gar=%s_%s:%s\n", p->name, iop->name, iop->gar );
			eptr = ng_var_exp( symtab, p->progid + VAR_SPACE, gar, '%', '^', ':', VEX_CONSTBUF ); 	/* replace variables */
			ng_bleat( 2, "submit_gar: %s(troupe) sending: %s", tp->desc->name, eptr );
			send2seneschal( eptr );
		}			
	}
}


/* 	send the jobs that are not marked as complete to seneschal 
	returns 0 on failure
	Mod: 12 Jun 2004 (sd) : added max2submit so that we come up for air if a programme has 
		way too many jobs to submit in one breath.  Allows service of user commands and 
		status messages from seneschal to be processed.
*/
int submit_jobs( Programme *p, Troupe *t )
{
	int 	i;
	int	max2submit = 250;		/* max jobs to submit at any one time. we leave troupe in _SUBMIT state if we max out */
	int	firstj = -1;			/* first job number submitted */
	char 	jname_base[1024];		/* build the base job name (troupe.prog_); sendmisc adds jobnum when sending user name */

	if( have_seneschal == 0 )			/* cannot even attempt if not connected */
		return;

	sfsprintf( jname_base, sizeof( jname_base ), "%s.%s_", t->desc->name, p->name );

	for( i = 0; i < t->njobs; i++ )
	{
		if( t->jobs[i].status < RUNNING )	/* job not yet submitted to seneschal */
		{
			if( firstj < 0 )
				firstj = i;

			if( send2seneschal( t->jobs[i].cmd ) )
			{
				if( sendmisc( jname_base, i, p->subuser ) &&
					sendfiles( t->jobs[i].ifiles, "input=* \n" ) && 
					sendfiles( t->jobs[i].ofiles, "output=* \n" ) )
				{
					t->jobs[i].status = RUNNING;			/* show we sent it off */
					send2seneschal( t->jobs[i].qmsg );		/* send message to cause seneschal to queue job */
				}
				else
				{
					t->jobs[i].status = FIN_BAD;			/* prevent reschedule attempt */
					ng_bleat( 0, "submit_jobs: submission of job files failed for troupe: %s at job number %d", t->desc->name, i );
					return 0;
				}
			}
			else
			{
				ng_bleat( 0, "submit_jobs: submission of jobs failed for troupe: %s at job number %d", t->desc->name, i );
				return 0;
			}

			if( --max2submit < 1 )
			{
				gflags |= GF_WORK_SUSP;			/* set suspended work flag so that progmaint called again asap */
				ng_bleat( 1, "submit_jobs: jobs %d -> %d submitted to seneschal,  troupe partially active: %s", firstj, i, t->desc->name );
				return 1;
			}
		}
	}

	t->state = TS_ACTIVE;			/* if no errors during submission; we get here and mark the troupe as a whole as active */
	if( firstj >= 0 )      /* if we bounced out last time right at the last job, prevent goofy message this time round */
		ng_bleat( 1, "submit_jobs: jobs %d -> %d submitted to seneschal,  troupe now active: %s", firstj, i-1, t->desc->name );
	else
		ng_bleat( 1, "submit_jobs: troupe now active: %s", t->desc->name );	

	return 1;	
}

/* send a hold command to seneschal for this job */
void hold_seneschal_job( char *job )
{
	char	*tok;
	char	*name;				/* start of job name */
	char	buf[200];

	if( tok = strchr( job, ':' ) )		/* at end of the job name */
	{
		*tok = 0;					/* mark end of name for sprintf */
		if( strncmp( job, "ijob=", 5 ) == 0 )
			name = job + 5;				/* skip ijob= if it was there */
		else
			name = job;
	
		sfsprintf( buf, sizeof( buf ), "hold=%s\n", name );	/* hold= keeps seneschal from sending ack; hold: from sreq */
		*tok = ':';
	
		send2seneschal( buf );			/* seneschal accepts commands from udp */
	}
}

void purge_seneschal_job( char *job )
{
	char	*tok;
	char	*name;				/* start of job name */
	char	buf[200];

	if( tok = strchr( job, ':' ) )		/* at end of the job name */
	{
		*tok = 0;					/* mark end of name for sprintf */
		if( strncmp( job, "ijob=", 5 ) == 0 )
			name = job + 5;				/* skip ijob= if it was there */
		else
			name = job;
	
		sfsprintf( buf, sizeof( buf ), "purge=%s\n", name );		/* seneschal command buffers must be newline terminated */
		*tok = ':';
	
		send2seneschal( buf );			/* seneschal accepts commands from udp */
	}
}

/*	resends jobs to seneschal.  as it is likely we are doing this to retry a failed job, we will 
	purge the job first (if requested). this is done if we are rerunning a job, and is not needed 
	if we are restarting and submitting based on a ckpt file.

	if 'what' is >= 0 then its the job number; constants RE_START_ALL/RE_START_FAILED are neg values 
*/
int resubmit_jobs( Programme *p, Troupe *t, int what, int purge )
{
	int 	i;
	int	count = 0;
	char 	jname_base[1024];			/* build the base job name (troupe.prog_); sendmisc adds jobnum when sending user name */

	if( have_seneschal == 0 )			/* cannot even attempt if not connected */
	{
		ng_bleat( 0, "resubmit: seneschal connection not established; cannot send resubmit request" );
		return 0;
	}

	sfsprintf( jname_base, sizeof( jname_base ), "%s.%s_", t->desc->name, p->name );

	if( what >= 0 )				/* restart a single job given the job number */
	{
		if( what < t->njobs )
		{
			ng_bleat( 1, "resubmit: for individual job: %d", what );
			if( purge )
				purge_seneschal_job( t->jobs[what].cmd );		/* must purge it first */

			if( send2seneschal( t->jobs[what].cmd ) )
			{
				if(	sendmisc( jname_base, what, p->subuser ) &&  
					sendfiles( t->jobs[what].ifiles, "input=* \n" ) && 
					sendfiles( t->jobs[what].ofiles, "output=* \n" ) )
				{
					send2seneschal( t->jobs[what].qmsg );		/* send message to cause seneschal to queue job */
					count++;
					if( t->jobs[what].status >= FIN_OK )		/* must dec finished counts if its not running */
					{
						if( t->jobs[what].status == FIN_OK )		
						{
							if( t->cjobs > 0 )
								t->cjobs--;
						}	
						else
							if( t->ejobs > 0 )
								t->ejobs--;			/* keep the bean counters happy */
					}

					t->jobs[what].status = RUNNING;			/* show we sent it off */
				}
				else
					ng_bleat( 0, "resubmit: could not resubmit files for troupe: %s at job %d", t->desc->name, what );
			}
			else
			{
				ng_bleat( 0, "resubmit: submission of jobs failed for troupe: %s at job number %d", t->desc->name, what );
				return 0;
			}
		}
		else
			ng_bleat( 0, "resubmit: job number is out of range: %d(entered) %d(max)", what, t->njobs );

		t->state = TS_ACTIVE;			/* if all jobs submitted, then mark the troupe as active */
		ng_bleat( 1, "jobs (%d) re-submitted to seneschal, from troupe: %s", count, t->desc->name );
		return 1;
	}

	switch( what )				/* convert what to job status type */
	{
		case RE_SUBMIT_BAD: 
			what = FIN_BAD;
			ng_bleat( 1, "resubmit: for bad jobs: (status = %d)", what );
			break;

		case RE_SUBMIT_RUNNING:
			what = RUNNING;
			ng_bleat( 1, "resubmit: for running jobs: (status = %d)", what );
			break;

		case RE_SUBMIT_UNSUB:		/* jobs that have never been submitted to seneshal */
			what = UNSUBMITTED;
			ng_bleat( 1, "resubmit: for unsubmitted jobs: (status = %d)", what );
			break;

		default:
			what = RE_SUBMIT_ALL;
			ng_bleat( 1, "resubmit: for all jobs" );
			break;
			
	
	}

	if( purge )
	{
		for( i = 0; i < t->njobs; i++ )			/* purge them all at once so as not to risk odd message overlap issues */
		{
			if( t->jobs[i].status == what )
				purge_seneschal_job( t->jobs[i].cmd );		/* must purge it first */
		}

		sleep( 1 );						/* let seneschal chew and swallow purges */
	}

	for( i = 0; i < t->njobs; i++ )
	{
		if( what == RE_SUBMIT_ALL || t->jobs[i].status == what )
		{
			count++;
			ng_bleat( 4, "resubmit: sending resubmission for job: %d", i );
			if( send2seneschal( t->jobs[i].cmd ) )		
			{
				if(	sendmisc( jname_base, what, p->subuser ) &&  
					sendfiles( t->jobs[i].ifiles, "input=* \n" ) && 
					sendfiles( t->jobs[i].ofiles, "output=* \n" ) )
				{
					send2seneschal( t->jobs[i].qmsg );		/* send message to cause seneschal to queue job */
					if( t->jobs[i].status == FIN_OK )		/* if finished, must dec a counter */
					{
						if( t->cjobs > 0 )
							t->cjobs--;					/* keep the bean counters happy */
					}
					else
					{
						if( t->jobs[i].status > FIN_OK && t->ejobs > 0 )	/* if finished, must dec a counter */
							t->ejobs--;					/* keep the bean counters happy */
					}
	
					t->jobs[i].status = RUNNING;			/* show we sent it off */
				}
				else
					ng_bleat( 0, "resub of job(s) failed for troupe: %s (unable to submit files for job %d)", t->desc->name, i );
			}
			else
			{
				ng_bleat( 0, "submission of jobs failed for troupe: %s at job number %d", t->desc->name, i );
				return 0;
			}
		}
	}

	t->state = TS_ACTIVE;			/* if all jobs submitted, then mark the troupe as active */
	ng_bleat( 1, "jobs (%d) re-submitted to seneschal, from troupe: %s", count, t->desc->name );

	return 1;	
}


