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
Abstract: 	
Date:		22 Feb 2005 (VD!); 
		24 May 2006 revampped to run as a distributed process tree application
Author: 	E. Scott Daniels

Mods:		30 Mar 2005 (sd) : fixed sdata and snames memory leaks.
-----------------------------------------------------------------------
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include	<sys/types.h>
#include	<sys/wait.h>

#include <sfio.h>
#include <ningaui.h>
#include <ng_tokenise.h>
#include <ng_lib.h>
#include <ng_socket.h>

#include "srvm3.h"

#include "srvm3.man"

#define ELECTION_TIME	600;	/* default of 1 minute (tenths) */

#define ST_GREEN	0	/* states of individual services */
#define ST_YELLOW	1
#define ST_RED		2

				/* field tokens from service data pp variable */
#define SD_LEASE_SEC	0	/* time after service started, before next election */
#define SD_NELECT	1	/* number of nodes to elect for the service */
#define SD_START_CMD	2		
#define SD_STOP_CMD	3
#define SD_SCORE_CMD	4
#define SD_CONT_CMD	5
#define SD_QUERY_CMD	6
#define SD_EXPECTD_TOKS	7

#define FL_FIXED_LIST	0x01		/* we are managing a fixed list of services provided on the command line */
#define FL_REMOTE	0x02		/* node is remote -- not allowed to become root */

#define NAME_SPACE	0

Symtab	*nametab = NULL;	/* quick hash into service list */
char	*mynode = NULL;
char	*scope = NULL;		/* scope of the services we manage (cluster or flock name) */
char	*port = NULL;		/* port to use */
char	*snames = NULL;				/* list of service names that we will drive */
int 	flags = 0;

char	*version = "srvm v3.6/0a209";	/* include srvm in the ver string so that it goes out on the dump */

Srv_info_t	*slist = NULL;		/* list of services we are driving elections for */


/* ---------------- prototypes ----------------------------------  */
/*int dpt_user_data( int sid, char *buf, int dpt_flags );*/
void start_election( Srv_info_t *sp );


/* --------------------- symtab management ----------------------- */
/* find a service by name */
static Srv_info_t *lookup( char *name )
{
	Syment *se = NULL;

	if( (se = symlook( nametab, name, NAME_SPACE, NULL, SYM_NOFLAGS )) )
		return (Srv_info_t *) se->value;

	return NULL;
}

/* add a service to the symtab */
static void insert( Srv_info_t *sp )
{
	Syment *se = NULL;

	if( ! sp )
		return;

	symlook( nametab, sp->name, NAME_SPACE, sp, SYM_NOFLAGS );
}

/* ------------------------------- utility ----------------------------- */
/* get the name of the node, sans domain crap */
static char *node_name( void )
{
	char buf[1024];
	char *p;

	ng_sysname( buf, 1000 );
	if( (p = strchr( buf, '.' )) )
		*p = 0;				/* we want nothing of the domain */

	return ng_strdup( buf );
}

/* send some stuff out to user on session sid */
void explain_sp( int sid, Srv_info_t *sp )
{
	Ballot_t *bp;
	ng_timetype now;
	char	ts[100];		/* buffer to format time in */
	char	state[256];		/* state string */

	if( sp )
	{
		if( ! sp->active )
		{
			ng_siprintf( sid, "service was dropped from service manager control\n" );
			return;
		}

	 	now = ng_now(); 
	
		sfsprintf( state, sizeof( state ), "%s %s", sp->state == ST_GREEN ? "UP" : (sp->state == ST_YELLOW ? "QUESTIONABLE" : "DOWN"), sp->qdata ? sp->qdata : "" );

		ng_siprintf( sid, " service: %s\n   start: %s\n    stop: %s\n   score: %s\n   query: %s\n   state: %s\n ele len: %ds\n  expiry: %I*ds\n", sp->name, sp->start_cmd, sp->stop_cmd, sp->score_cmd, sp->query_cmd, state, sp->election_len/10, sizeof( sp->lease ), sp->lease == 0 ? 0 : (sp->lease - now)/10 );
		ng_siprintf( sid, "last ele: %s\n ballots:\n", sp->election_start > 0 ? ng_stamptime( sp->election_start, 1, ts ) : "unknown" );
		if( sp->ballots )
		{
			char *comment = "";

			ng_siprintf( sid, "\t%12s %3s %-19s %4s\n", "NODE", "SCR", "LAST VOTE", "LAG" );
			for( bp = sp->ballots; bp; bp = bp->next )
			{
				if( bp->flags & BF_ELECTED )
					comment = "elected";
				else
				if( bp->flags & BF_IGNORE )
					comment = "ignored"; 
				else
				if( bp->ts < (sp->election_start-50) )
					comment = "old ballot";
				else
					comment = "";

				ng_siprintf( sid, "\t%12s %3d %12s %4ds %s\n", bp->nname, bp->score,  ng_stamptime( bp->ts, 1, ts ), bp->lag/10, comment ); 
			}
		}
		else
		{
				ng_siprintf( sid, "\tno election results are available as election for this service\n" );
				ng_siprintf( sid, "\thas not been held since srvmgr started on this node.\n" );
		}
	}
}
/*
	send info on everything
*/
void dump( int sid, int dpt_flags )
{
	Ballot_t *bp;
	Ballot_t *best;
	Srv_info_t *sp;
	ng_timetype now;

	now = ng_now( );

	ng_siprintf( sid, "%s %s %s\n", version, dpt_flags & DPT_FL_ROOT ? "ROOT" : "", mynode );
	for( sp = slist; sp; sp = sp->next )
	{
		explain_sp( sid, sp );
		if( sp->next )
			ng_siprintf( sid, "\n" );
	}
}

/* explain one service by name */
void explain( int sid, char *sname )
{
	Srv_info_t *sp;
	Ballot_t *bp;
	ng_timetype now;

	if( (sp = lookup( sname )) )
		explain_sp( sid, sp );
	else
		ng_siprintf( sid, "service is unknown: %s\n", sname );
}


/* force an election to be held on a service 
   nice for testing service manager, and when bouncing a service
*/
void force( int sid, char *sname )
{
	Srv_info_t *sp;
	Ballot_t *bp;
	ng_timetype now;

	if( (sp = lookup( sname )) )
	{
		now = ng_now( );
		sp->state = ST_RED;
		start_election( sp );
		sp->lease = now + sp->election_len; 
		ng_bleat( 0, "election forced: %s", sp->name );
		ng_siprintf( sid, "election forced: %s\n", sp->name );
	}
}

/* when a node starts, and thinks it might be the host last assigned the service, 
   it sends a probe command in to force us to probe the service.  This should 
   help us notice missing services faster when a node bounces 
*/
void force_probe( int sid, char *sname )
{
	Srv_info_t *sp;
	Ballot_t *bp;
	ng_timetype now;

	if( (sp = lookup( sname )) )
	{
		sp->lease = 0;
		ng_bleat( 0, "probe forced: %s", sp->name );
		ng_siprintf( sid, "probe forced: %s\n", sp->name );
	}
}

/* reset a service timer */
void reset_timer( int sid, char *sname )
{
	Srv_info_t *sp;
	ng_timetype now;

	if( (sp = lookup( sname )) )
	{
		now = ng_now( );
		sp->lease = now + sp->lease_time; 
		ng_bleat( 0, "lease reset: %s %ds", sp->name, sp->lease_time );
		ng_siprintf( sid, "lease reset: %s %ds\n", sp->name, sp->lease_time );
	}
}
	
static void send_eod( sid )
{
	if( sid >= 0 )
		ng_siprintf( sid, "#end#\n" );				/* end of data string back to user */
}

static void usage( )
{
	sfprintf( sfstderr, "%s %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-a agent-host:port] [-c cluster] [-d nnsd_port] [-p port] [-r] [-S service-list] [-t ttl-seconds] [-v]\n", argv0 );
}

/*
	run a command and return the first buffer of output
*/
char *run_cmd( char *cmd )
{
	char	*buf = NULL;		/* return from command */
	char	*p;
	int	status = 0;

	Sfio_t	*f;

	if( (f = sfpopen( NULL, cmd, "r" )) != NULL )
	{
		buf = sfgetr( f, '\n', SF_STRING ); 
		if( buf )
			buf = ng_strdup( buf );		/* must do this before close */
		status = sfclose( f );
		if( status > 100 )			/* assume something like 126/127 no such command or unable to execute */
		{
			ng_bleat( 0, "query command failed with a suspect status: %d", status );
/* what to do?   mark the sevice down? */
		}
	}
	else
	{
		ng_bleat( 0, "run_cmd: unable to execute command: %s: %s", cmd, strerror( errno ) );
		return NULL;
	}

	return buf;
}

/* start a command in a different process so that we can close TCP ports and the like */
void spawn_cmd( char *cmd )
{
	int 	cpid = 0;
	int	i;

	switch( (cpid = fork( )) )
	{
		case 0:				/* child */
			ng_bleat( 2, "subprocess started for cmd: %s", cmd );
			for( i = 3; i < 255; i++ )
				close( i );		/* force things like tcp listens down */
			run_cmd( cmd );
			ng_bleat( 3, "subprocess finishing for cmd: %s", cmd );
			exit( 0 ); 		/* die after the command */
			break;

		case -1: 
			ng_bleat( 0, "error forking for command: %s: %s", cmd, strerror( errno ) );
			break;

		default:
			return;
			break;
	}
}

/* ----------------------------- election support -------------------------------------------- */

/* find the ballot that matches the node name passed in b */
Ballot_t *find_ballot( Srv_info_t *sp, char *b )
{
	Ballot_t *bp;

	for( bp = sp->ballots; bp; bp = bp->next )
		if( strcmp( b, bp->nname ) == 0 )
			return bp;

	return bp;
}

Ballot_t *find_winner( Srv_info_t *sp )
{
	Ballot_t *bp;
	Ballot_t *best = NULL;

	if( ! sp->ballots )
		return NULL;

	/* find 1st !0 score; 0 guarentees its not a winner even if its the only vote -- reest elected flag as we go */
	/* if the timestamp is too far out of whack (before the election started) we consider it a zero vote */
	for( bp = sp->ballots; bp && (bp->score <= 0 || bp->ts < sp->election_start-50); bp = bp->next )
		bp->flags &= ~BF_ELECTED;			/* reset the elected flag */
	
	if( (best = bp) != NULL )		/* run the rest looking for the winner */
	{
		bp->flags &= ~BF_ELECTED;			
		for( bp = best->next; bp; bp = bp->next )
		{
			bp->flags &= ~BF_ELECTED;			
			if( bp->flags & BF_IGNORE )		/* we are ignoring votes from this one now */
			{
				if( bp->expiry <= sp->election_start )		/* timer pop -- they can participate  */
				{
					bp->flags &= ~BF_IGNORE; 
					ng_bleat( 1, "ignore ballot expires: %s from %s", sp->name, bp->nname );
				}
				else
					ng_bleat( 1, "ballot ignored: %s from %s score=%d", sp->name, bp->nname, bp->score );
			}			

			if( !(bp->flags & BF_IGNORE)  && bp->ts >= sp->election_start-50 && bp->score > best->score )
				best = bp;
		}
	}

	if( best )
		ng_bleat( 2, "%s: winner selected: %s %d", sp->name, best->nname, best->score );
	return best;
}

/* mark the last winner with an ignore flag -- they probably seem unable to keep the service running */
void ignore_winner( Srv_info_t *sp )
{
	Ballot_t *bp;

	if( ! sp->ballots )
		return;

	for( bp = sp->ballots; bp;  bp = bp->next )
	{
		if( bp->flags & BF_ELECTED )
		{
			ng_bleat( 1, "ignore ballots set: %s from %s", sp->name, bp->nname );
			bp->flags |= BF_IGNORE;
			bp->expiry = ng_now() + (sp->lease_time * 2);
		}
	}
}

/* 
   Generate a recap of the last election in terms of the find-winner logic.
   same logic as find winner, but details decision on each ballot for debugging or explaination when things seem odd 
   sid is the socket id that the info is sent to 
*/
void recap( int sid, char *buf )
{
	Ballot_t *bp;
	Ballot_t *best = NULL;
	Srv_info_t *sp;


	if( (sp = lookup( buf )) == NULL )
	{
		ng_siprintf( sid, "recap: unknown service: %s\n", buf );
		return;
	}

	if( ! sp->ballots )
	{
		ng_siprintf( sid, "recap: service running when srvmgr started; election has not been held: %s\n", sp->name );
		return;
	}

	ng_siprintf( sid, "recap last election for: %s\n", sp->name );

	/* find 1st !0 score; 0 guarentees its not a winner even if its the only vote -- reest elected flag as we go */
	/* if the timestamp is too far out of whack (before the election started) we consider it a zero vote */
	best = NULL;
	
	for( bp = sp->ballots; bp; bp = bp->next )
	{
		if( bp->flags & BF_IGNORE )
			ng_siprintf( sid, "ballot ignored: %s: score=%d\n", bp->nname, bp->score );
		else
		{
			if( bp->ts >= sp->election_start-50 && (best == NULL || bp->score > best->score) )
			{
				best = bp;
				ng_siprintf( sid, "new score to beat: %s: score=%d\n", bp->nname, bp->score );
			}
			else
				if( bp->ts < (sp->election_start-50) )			/* not considered -- old timestamp */
	 				ng_siprintf( sid, "   not considered: %s: old timestamp\n", bp->nname );
				else
					ng_siprintf( sid, "      not elected: %s score=%d\n", bp->nname, bp->score );
		}
	}

	if( best )
		ng_siprintf( sid, "  winner selected: %s score=%d\n", best->nname, best->score );
	else
		ng_siprintf( sid, "winner was not selected\n" );

	return;
}

/* ballot format is:  service:name:timestamp:score */
void save_ballot( char *buf )
{
	static char **tokens = NULL;
	int	ntok;
	Srv_info_t *sp;
	Ballot_t *bp;

	tokens = ng_tokenise( buf, ':', '^', tokens, &ntok );
	if( ntok != 4 )
	{
		ng_bleat( 0, "ballot was missing info: %s", buf );
		return;
	}


	if( (sp = lookup( tokens[0] )) == NULL )
		ng_bleat( 0, "ballot received for unknown service: %s in %s", tokens[1], buf );
	else
	{
		if( (bp = find_ballot( sp, tokens[1] )) == NULL )		/* no ballot yet received for the node */
		{
			ng_bleat( 2, "%s: ballot saved: %s (new)", sp->name,  buf );
			bp = (Ballot_t *) ng_malloc( sizeof( Ballot_t ), "save ballot: ballot" );
			bp->nname = strdup( tokens[1] );
			bp->next = sp->ballots;
			sp->ballots = bp;
		}	
		else
			ng_bleat( 2, "%s: ballot saved: %s", sp->name, buf );

		bp->ts = strtoull( tokens[2], NULL, 10 );	
		bp->flags &= ~BF_ELECTED;			/* reset the elected flag */
		bp->lag = bp->ts - sp->election_start;		/* time it took the node to vote */
		bp->score = atoi( tokens[3] );
	}

	return;
}

char *mk_ballot( Srv_info_t *sp, ng_timetype ts )
{
	char	wbuf[NG_BUFFER];

	sfsprintf( wbuf, sizeof( wbuf ), "ballot:%s:%s:%I*d:%d", sp->name, mynode, Ii(ts), sp->score );

	return strdup( wbuf );
}


/* send ballot, but we also need to save the data as though we received the ballot 
	invoking dpt_user_data pretends like we received the buffer, and it propigates
	it to all peers that do not match the sid (-1) we pass in.
*/
void send_ballot( char *b )
{
	dpt_user_data( -1, b, 0 );			
}

/* compute the score for the service on this node, and cast a ballot 
	assume buf has the vote message information -- service-name:timestamp
*/
void compute_score( char *buf )
{
	char	*p;
	Srv_info_t *sp;

	if( (p = strchr( buf, ':' )) != NULL )
	{
		*p = 0;
		p++;
	}

	if( (sp = lookup( buf )) == NULL )
	{
		ng_bleat( 1, "cannot find service to compute score: %s", buf );
		return;
	}

	if( p )
		sp->election_start =  strtoull( p, NULL, 10 );	

	if( (p = run_cmd( sp->score_cmd )) != NULL )
	{
		if( (sp->score = atoi( p )) > 100 )
			sp->score = 100;
		else
			if( sp->score < 0 )
				sp->score = 0;

		ng_free( p );

		p = mk_ballot( 	sp, ng_now() );
		if( p )
		{
			ng_bleat( 3, "%s: sending ballot: %s", sp->name, p );
			send_ballot( p );
			ng_free( p );
		}
	}
	return;
}

/* send out a message asking everybody to vote */
void start_election( Srv_info_t *sp )
{
	char buf[NG_BUFFER];
	ng_timetype now;
	
	if( sp->qdata )
	{
		ng_free( sp->qdata );
		sp->qdata = NULL;
	}

	now = ng_now( );
	if( (sp->election_start + 18000) > now )		/* second election in less than 30 minutes */
		ignore_winner( sp );				/* ignore the last winner -- seems there might be problems */

	sp->election_start = now;
	sfsprintf( buf, sizeof( buf ), "vote:%s:%I*d", sp->name, Ii( sp->election_start ) );
	ng_log( LOG_INFO, "%s: election started", sp->name );

	dpt_user_data( -1, buf, 0 );			/* pretend we got the buffer -- usr_data will forward it */
}

/* send a start service message to all in tree */
void start_service( Srv_info_t *sp, char *node )
{
	char buf[NG_BUFFER];
	
	sfsprintf( buf, sizeof( buf ), "switch:%s:%s", sp->name, node );
	dpt_user_data( -1, buf, 0 );			/* pretend we get the buffer; usr_data will pass it along to peers */
}

/*	switch the service.  expect buf to be service-name:nodename 
	if we are node, then start, else stop 
*/
void switch_service( char *buf )
{
	Srv_info_t *sp;
	char *p;

	if( (p = strchr( buf, ':' )) != NULL )
	{
		*p = 0;
		p++;					/* point at node name */
		if( (sp = lookup( buf )) != NULL )	/* find the service block that matches name */
		{
			if( strcmp( mynode, p ) == 0 )	/* we won! */
			{
				ng_bleat( 1, "%s: starting service (%s)", sp->name, sp->start_cmd );
				spawn_cmd( sp->start_cmd );	/* dont use run_cmd as it does not close fds */
			}
			else
			{
				ng_bleat( 1, "%s: not election winner, ensuring service is stopped (%s)", sp->name, sp->stop_cmd );
				spawn_cmd( sp->stop_cmd );
			}
		}
		else
			ng_bleat( 1, "could not find service to switch: %s", buf );
	}
	else
		ng_bleat( 0, "badly formed switch information; expected <sname>:<node> found: %s", buf );
}

/* unconditionally force the service down on this node -- sent by things like ng_node when stopping a node 
	expect that buf points to the service name to stop 
*/
void stop_service( char *buf )
{
	Srv_info_t *sp;
	char *p;

	if( (sp = lookup( buf )) != NULL )	/* find the service block that matches name */
	{
		ng_bleat( 1, "stop service on localhost received for %s, executing: %s", sp->name, sp->stop_cmd );
		spawn_cmd( sp->stop_cmd );
	}
	else
		ng_bleat( 1, "could not find service to stop: %s", buf );
}


/* -------------------------- service management ------------------------------------ */
/* 
	driven for each service when we are root -- decides what is going on and what to do 
*/
void manage( Srv_info_t *sp )
{
	Ballot_t *bp;		/* pointer to the winner's ballot */
	ng_timetype now;
	char	*buf;		/* stuff returned from command */
	char	*p;		/* pointer into buffer */
	char	*p2;

	now = ng_now( );

	switch( sp->state )
	{
		case ST_GREEN:				/* all ok, just check current state */
		case ST_YELLOW:				/* may not be doing well, check state */
			if( sp->lease < now )		/* time to check the state of the service */
			{
				ng_bleat( 3, "%s: checking state: %s", sp->name, sp->query_cmd );
				if( (buf = run_cmd( sp->query_cmd )) != NULL )
				{
					for( p = buf; *p && *p == ' '; p++ );		/* skip lead blanks */
					ng_bleat( 3, "%s: query returned state: %s", sp->name, p );
					if( (p2 = strchr( p, ':' )) != NULL )
					{
						*p2 = 0;
						p2++;				/* assume this points to node name or other useful info */
					}
		
					if( strcmp( p, "green" ) == 0 )
					{
						sp->state = ST_GREEN;
						sp->lease = now + sp->lease_time; 	/* check status again in a bit */
						if( sp->qdata )
							ng_free( sp->qdata );
						if( p2 )
							sp->qdata = ng_strdup( p2 );
						else
							sp->qdata = NULL;
					}
					else
					if( strcmp( p, "red" ) == 0 )
					{
						ng_bleat( 1, "%s: state: RED!; calling for election", sp->name );
						sp->lease = now + sp->election_len; 
						start_election( sp );
						sp->state = ST_RED;
					}
					/*if( strcmp( p, "yellow" ) == 0 )*/	
					else					/* treat unrecognised as yellow */
					{
						if( sp->state == ST_YELLOW )		/* already in yellow state -- call for an election */
						{
							ng_bleat( 1, "%s: state: second yellow state report; calling for election", sp->name );
							
							start_election( sp );
							sp->lease = now + sp->election_len; 
							sp->state = ST_RED;		/* service is assumed down */
						}
						else
						{
							ng_bleat( 1, "%s: state: first yellow state report; query again in %ds", sp->name, (sp->lease_time/20) + 5 );
							sp->state = ST_YELLOW;
							sp->lease = now + (sp->lease_time/2) + 50; 	/* check status again in a bit */
						}
					}
				}
				else
				{
					ng_bleat( 2, "%s: state: no result from queury; retry in %ds", sp->name, (sp->lease_time/20) + 5 );
					sp->state = sp->state == ST_YELLOW ? ST_RED : ST_YELLOW;
					sp->lease = now + (sp->lease_time/2) + 50; 	/* check status again in a bit */
				}
			}
			break;

		case ST_RED:			/* service was down -- we called for an election, see if its time to delcare a winner */
			if( sp->lease < now )	/* enough time given to receive votes */
			{
				ng_bleat( 2, "%s: election over, determining winner", sp->name );
				if( (bp = find_winner( sp )) != NULL )
				{
					bp->flags |= BF_ELECTED;
					ng_bleat( 1, "%s: election ended, award server to: %s", sp->name, bp->nname );
					ng_log( LOG_INFO, "%s: election ended, award server to: %s", sp->name, bp->nname );
					start_service( sp, bp->nname );
					sp->state = ST_GREEN;
					sp->lease = now + (sp->lease_time * 2);
				}
				else
				{
					sp->state = ST_YELLOW;				/* force another election after a brief pause */
					sp->lease = now + (sp->lease_time/2) + 50; 	
					ng_bleat( 0, "%s: could not determine a winner; setting lease to %ds", sp->name, (int) ((sp->lease_time/20) + 5) );
					ng_log( LOG_INFO, "%s: could not determine a winner; retry in %ds", sp->name, (int) ((sp->lease_time/20) + 5) );
				}
			}
			break;
	}					/* switch on state */
}


/*
	accept a string of service names from the list we create service 
	info blocks that are pushed on the list of services. We allow the 
	list of names to be of the form:  name[:scope]  where scope is 
	sets the default scope in the structre.
	This value can be overrridden
	by the score calculation process, and if omitted, defaults to the 
	scope that is supplied on the command line, or the cluster name if
	nothing was supplied on the command line.
*/
static void build_slist( char *names )
{
	Srv_info_t	*new = NULL;
	char	buf[2048];
	char	**stoks = NULL;		/* service name tokens from names */
	char	*service;
	char	*p = NULL;		/* pointer to the scope if there */
	int	nstoks = -1;		/* tok count */

	char	*sdata;			/* service data that was put into pinkpages */
	char	**itoks = NULL;		/* tokens from sdata */
	int	nitoks = -1;		/* tok count */
	int	space;			/* space where the var came from -- ignored */

	int	i;

	if( strchr( names, '!' ) )
		stoks = ng_tokenise( names, '!', '\\', stoks, &nstoks );
	else
		stoks = ng_tokenise( names, ' ', '\\', stoks, &nstoks );	/* not documented, but we allow spaces */
	for( i = 0; i < nstoks; i++ )
	{
		if( (service = stoks[i]) != NULL )			/* if !! or foo! gets into the list */
		{
			if( (p = strchr( service, ':' )) )
			{
				*p = 0;
				p++;				/* now points at the scope */
			}
	
			ng_bleat( 4, "building info for %s scope=%s", service, p ? p : "not supplied" );
	
			sfsprintf( buf, sizeof( buf ), "NG_SRVM_%s", service );	 	/* get specific server info from NG_SRVM_servicename */
			if( (sdata = ng_pp_get( buf, NULL )) != NULL || !*sdata )	/*  we dont care where the data was in pp space */
			{
				itoks = ng_tokenise( sdata, '!', '\\', itoks, &nitoks );	/* split data on bangs */
				if( nitoks >= SD_EXPECTD_TOKS )
				{
					if( (new = lookup( service )) == NULL )				/* not yet seen */
					{
						ng_bleat( 2, "creating new service block for: %s", service );
						new = ng_malloc( sizeof( *new ), "new service info block" );
	
						memset( new, 0, sizeof( *new ) );
	
						new->next  = slist;			/* just push on */
						slist = new;
	
						new->name = ng_strdup( service );		/* these things init only when we create */
	
						new->lease = 0;
						new->score = 0;
						insert( new );				/* give us reference in the hash table */
	
					}
	
					ng_free( new->start_cmd );			/* these things can change with each read so refresh */
					ng_free( new->stop_cmd );
					ng_free( new->score_cmd );
					ng_free( new->continue_cmd );			/* deprecated! */
					ng_free( new->query_cmd );
	
					new->continue_cmd = NULL;			/* this one is optional for back compat */
	
					ng_free( new->dscope );
					if( p )
						new->dscope = ng_strdup( p );
					else
						new->dscope = NULL;
	
					new->start_cmd = ng_strdup( itoks[SD_START_CMD] );
					new->stop_cmd = ng_strdup( itoks[SD_STOP_CMD] );
					new->score_cmd = ng_strdup( itoks[SD_SCORE_CMD] );
					new->continue_cmd = ng_strdup( itoks[SD_CONT_CMD] );
					new->query_cmd = ng_strdup( itoks[SD_QUERY_CMD] );
					new->active = 1;					/* mark as active so we check up on it */
	
					new->nelect = atoi( itoks[SD_NELECT] );			/* number of nodes to elect for the service */
	
										/* LEASE_SEC can be lll or lll+eee where eee is election duration */
					new->lease_time = atoi( itoks[SD_LEASE_SEC] ) * 10;	/* convert to tenths */
					if( (p = strchr( itoks[SD_LEASE_SEC], '+' )) )
						new->election_len = atoi( p+1 ) * 10;
					else
						new->election_len = ELECTION_TIME;
	
	
					ng_free( sdata );
				}
				else
					ng_bleat( 0, "data in %s has bad number of ! separated tokens. expected %d, found %d", buf, SD_EXPECTD_TOKS, nitoks );
			}
			else
			{
				ng_bleat( 0, "could not find service data in narbalek (%s) service not supported", buf );
			}
		}				/* end if service token was not null */
	}

	ng_free( stoks );
	ng_free( itoks );
}


/* ----------------------------- dptree call back rouitines ---------------------------------- */
/* process data received from a partner or from the outside world. If its a command we expect
	from the outside then we must send an end of data message back to 'release' them.
	for certain data buffers (vote, ballot, switch) we pass the buffer to other peers
	the buffer MUST be propigated before worked on so that if it is altered (tokenised)
	it will still be sent intact. 
*/
int dpt_user_data( int sid, char *buf, int dpt_flags )
{
	char *p;

	if( (p = strchr( buf, '\n' )) )
		*p = 0;				/* if we 'send ourself' a message it might have trailing newline */

	ng_bleat( 3, "dpt_user_data: sid=%d (%s)", sid, buf );
	switch( *buf )
	{
		case 'b':			/* a ballot was received; tuck it away (b[allot]:node:timestamp:service:score) */
			if( (p = strchr( buf, ':' )) )
			{
				ng_dpt_propigate( sid, buf );	/* propigate the buffer before we deal with it */
				save_ballot( p+1 );
			}
			break;

		case 'D':
			dump( sid, dpt_flags );
			send_eod( sid );
			break;

		case 'e':				/* explain e[xplain]:service-name */
			if( (p = strchr( buf, ':' )) )
				explain( sid, p+1 );
			send_eod( sid );
			break; 

		case 'f':				/* force election f[orce]:service-name */
			if( dpt_flags & DPT_FL_ROOT )			/* if we are the root node -- then we are the boss! */
			{
				if( (p = strchr( buf, ':' )) )
					force( sid, p+1 );
			}
			else
				ng_siprintf( sid, "force command must be sent to controlling node\n" );
			send_eod( sid );
			break; 

		case 'p':				/* insist on a probe right now */
			if( dpt_flags & DPT_FL_ROOT )			/* if we are the root node -- then we are the boss! */
			{
				if( (p = strchr( buf, ':' )) )
					force_probe( sid, p+1 );
			}
			else
				ng_siprintf( sid, "probe command must be sent to controlling node\n" );
			send_eod( sid );
			break;


		case 'r':					/* reset the timer for the named service r[eset]:service */
			if( strncmp( buf, "recap", 5 ) == 0 )
			{
				if( (p = strchr( buf, ':' )) )
					recap( sid, p+1 );
			}
			else
			{
				if( (p = strchr( buf, ':' )) )
					reset_timer( sid, p+1 );
			}
			send_eod( sid );
			break;

		case 's':					/* stop service (stop servicename) or switch service (s[witch]:sname:nodename) */
			if( strncmp( buf, "stop:", 5 ) == 0 )
			{
				stop_service( buf+5 );
				send_eod( sid );
			}
			else
				if( (p = strchr( buf, ':' )) )
				{
					ng_dpt_propigate( sid, buf );	/* pass it along first */
					switch_service( p+1 );		/* take action locally */
				}
			break;


		case 'v':			/* a request to vote for a service v[ote]:service-name */
			if( (p = strchr( buf, ':' )) )
			{
				ng_dpt_propigate( sid, buf );	/* pass it on first */
				compute_score( p+1 );		/* figure it out and send it off to all peers */
			}
			break;
			
		case 'V':				/* verbose v[erbose]:n */
			if( (p = strchr( buf, ':' )) )
				verbose = atoi( p+1 );
			ng_bleat( 0, "srvm3: %s verbose set to : %d", version, verbose );
			send_eod( sid );
			break;

		case 'x':			/* possible exit */
			send_eod( sid );
			if( strncmp( buf, "xit2361", 7 ) == 0 )
			{
				ng_bleat( 0, "exit command received" );	
				return 0;
			}
			break;

		default:
			ng_bleat( 0, "unknown command received: %s", buf );	
			break;
	}

	return 1;
}

/* 	driven at regular interivals -- about every 2 sec 
	dpt expects a 1 return code to indicate good -- keep going
	we use this to refresh services we are controlling and other maintenance not related to 
	messages received from peers. 

	we must also check flags to see if there was a root collision. if so, and we are still
	the root of the tree, we force an election of all services so as not to have multiple services running. 
*/
int dpt_user_maint( int dpt_flags )
{
	static ng_timetype next_build = 0;	/* next time we update the service list */
	static	int announced = 0;		/* keep announcements of rootness to a minimum */
	Srv_info_t	*sp = NULL;		/* pointer into the sevice list */
	ng_timetype	now = 0;

	while( waitpid( 0, NULL, WNOHANG ) > 0 );		/* clear any defunct processes out  (leftovers from start) */

	if( dpt_flags & DPT_FL_ADOPTED )
	{
		now = ng_now( );

								/* everybody, not just root,  keeps a list of services up to date */
		if( ! (flags & FL_FIXED_LIST) )			/* not a fixed list from -S */
		{
			if( flags & DPT_FL_COLLISION  ||  next_build < now )	/* dont pound narbalek too much, force update if collision */
			{
				next_build = 0;					/* force the rebuild if snames is populated */

				ng_free( snames );				
				snames = ng_pp_get( "NG_SRVM_LIST", NULL );	/* get the list of services we are now driving */
				if( ! snames || !*snames )
				{
					ng_free( snames );
					snames = NULL;
					ng_bleat( 2, "service list (NG_SRVM_LIST) not defined and -S not supplied" );
				}
				else
					ng_bleat( 3, "dynamic snames: %s", snames );
			}
		}

		if( snames && next_build < now )
		{
			for( sp = slist; sp; sp = sp->next )
				sp->active = 0;			/* mark down -- build will mark those still active */
			build_slist( snames );			/* add new services, update any existing service info, mark those in list active */
			next_build = now + 450;			/* build every 45s or so */
		}
	
		if( dpt_flags & DPT_FL_ROOT )			/* if we are the root node -- then we are the boss! */
		{
			if( flags & DPT_FL_COLLISION )		/* if there was a collision force an election on everything to clear any services assigned by rouge root */
			{
				ng_bleat( 0, "root collision detected; calling for election of all services" );
				for( sp = slist; sp; sp = sp->next )
				{
					sp->lease = now + sp->election_len; 
					start_election( sp );
					sp->state = ST_RED;		
				}
			}
			else							/* no collision, just manage each service and take care of business */
			{
				if( !announced )
				{
					ng_bleat( 0, "we are now the master (control) node" );
					announced++;
				}

				for( sp = slist; sp; sp = sp->next )
				{
					if( sp->active )			/* if service was still listed (marked active by build_slist) */
						manage( sp );			/* see what needs attending to with this service */
				}
			}
		}
		else
		{
			if( announced )
				ng_bleat( 0, "we are no longer the master (control) node" );
			announced = 0;
		}
	}

	return 1;
	
}

/* ------------------------------------------------------------------------------------------- */

int main( int argc, char **argv )
{
	
	char	buf[NG_BUFFER];

	char	*port = NULL;			/* vars to give to the dptree environment */
	char	*league = NULL;
	char	*cluster = "foo-cluster";
	char	*mcast_agent = NULL;
	char	*mcast_group = "235.0.0.7";
	char	*nnsd_port_str = NULL;

	int	mcast_ttl = 2;			/* time to live value for mcast mode */
	
	mynode = node_name( );
	scope = ng_env( "NG_CLUSTER" );			/* default scope to the cluster */
	
	ARGBEGIN 
	{
		case 'a':	SARG( mcast_agent ); break;
		case 'c':	SARG( cluster ); break;
		case 'd':	SARG( nnsd_port_str ); break;		/* we can use the narbalek name service daemon :) */
		case 'g':	SARG( mcast_group ); break;
		case 'p':	SARG( port ); break;			/* port we will listen on */
		case 'r':	flags |= FL_REMOTE;			/* node is remote -- not allowed to be root */
		case 'S':	flags |= FL_FIXED_LIST; SARG( snames ); break;	/* turn on the fixed list flag */
		case 't':	IARG( mcast_ttl ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
				break;
	}
	ARGEND

	ng_dpt_set_ttl( mcast_ttl );
	if( flags & FL_REMOTE )
		ng_dpt_set_noroot( 1 );				/* turn on -- do not allow us to be root */

	if( ! cluster )
		cluster = ng_env( "NG_CLUSTER" );
	if( ! port )
		port = ng_env( "NG_SRVM_PORT" );

	if( ! cluster || ! port )
	{
		ng_bleat( 0, "abort: cluster and/or port not in pinkpages and not supplied on command line" );
		exit (1 );
	}

	sfsprintf( buf, sizeof( buf ), "srvm_%s", cluster );
	league = ng_strdup( buf );

	nametab = syminit( 101 );
	if( ! nametab )
	{
		ng_bleat( 0, "could not create symbol table for names" );
		exit( 1 );
	}

	ng_bleat( 1, "starting: %s", version );
	ng_bleat( 2, "invoking dptree to drive" );
	ng_dptree( league, cluster, port, mcast_agent, mcast_group, nnsd_port_str );	/* it drives! */

	ng_bleat( 0, "srvm: ng_dptree has returned, we are done." );
	return 0;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srvmgr:Service Supervision Manager V3)

&space
&synop	srvmgr [-a agent] [-c cluster] [-d nnsd_port] [-g mcast-group] [-p port-port] [r] [-S service-list] [-v]

&space
&desc	&This executes on any node that is a candidate to host one or more 
	cluster services.  A cluster service, is any service that is required
	to support cluster functionality, and executes on a limited number 
	of cluster nodes.   Examples of cluster services are: replication 
	manager, seneschal, nawab, and the master log combination process. 
	While most services equate to a daemon, a daemon is not required for 
	a node to be selected to provide a cluster service. 
&space
	The service list is a list of services that &this will manage. Typically
	it is supplied via the pinkpages variable &lit(NG_SRVM_LIST) which 
	contains a list of space separated service names (e.g. Logger).  When 
	the list is supplied via the pinkpages variable, it is dynamic and 
	changes to the variable have an immediate effect on the running &this.
	If the list is supplied via the &lit(-S) command line option, then
	the list is static until &this is stopped and restarted.  

&space
&subcat Service Description Variables
	For each service listed in &lit(NG_SRVM_LIST), or that was provided on 
	the command line, &this expects to find a pinkpages variable with a 
	name that has the syntax: &lit(NG_SRVM_)&ital(service-name) (e.g.
	NG_SRVM_Filer).  The value assigned to a service variable is expected 
	to contain 7, bang (!) separated, pieces of information.  This data, 
	listed in the order that it is expected, is:
&space
&beglist
&item	Query frequency (seconds). Defines how often the service query process
	is driven to check on the service.  If the query indicates the service 
	is down, or &this drives the query process a number of times getting 
	a questionable response, &this will hold an election to determine a 
	new host for the service. In an election, each node running &this will
	be asked to drive the service's score calculation process to compute 
	a vote. By default nodes have 60 seconds to vote, however if the query
	time is appended with a +nnn value, the election is held 'open' for 
	nnn seconds.  
&space
&item	Number of nodes that should be elected to run the service. Typically 
	services need to exist on only one node, however this provides for the 
	case where the service is expected to reside on more than one node. 
&space
&item	Service start command.  The command that &this will execute on the node 
	that is awarded the service. 
&space
&item	Service stop command. The command that &this will execute on a node 
	that was running the service, but has lost the right to continue to 
	run the service. 
&space
&item	Sercvice score command. The command that &this will execute on a node
	to compute the node's score. 
&space
&item	Service continue command.  This is the command that is driven when an 
	election is won, and the service manager believes that the service is 
	already initialised and running on the node.  The continue service 
	funciton should ensure that the service is started, and should start 
	the service if it is not running. 
&space
&item	Service query command. This command is driven on a single node in 
	the cluster and is expected to determine the state of the service. The 
	service state is to be communicated back to &this via standard output.
	After invoking the service query command, &this reads the first 
	record from generated by the command and expects that the first token
	(space separated) will be one of three keywords: green, yellow, or
	red.  If 'green' is returned, &this considers the service active and 
	functioning. Yellow indicates that the service is questionable, and a 
	second query should be made after a brief pause to determine the actual 
	state.  &This considers a service as down if 'red' is returned, or if 
	two consecutive queries return 'yellow.'

&endlist

	The following is an example value for the Logger service defined as 
	NG_SRVM_Logger in pinkpages:
&space
&litblkb
   90!1!ng_srv_logger start!ng_srv_logger stop!ng_srv_logger score!ng_srv_logger continue|ng_srv_logger query
&litblke
&space
	In the case of the logger, the same command, ng_srv_logger, is used 
	for all functions with the positional parameter indicating the desired
	action by the programme.  It is not necessary to implement the service
	support interface as a single programme. 
	The service is queried every 90 seconds, and only 1 node is elected to 
	run the Logger service. 
&space
	The following example shows a longer time (5 minutes) between queries
	and an election period of one minute:
&space
&litblkb
   300+60!1!ng_srv_logger start!ng_srv_logger stop!ng_srv_logger score!ng_srv_logger continue|ng_srv_logger query
&litblke

&subcat Communications Tree
	&This is designed to execute on every host within a cluster. When the process 
	starts it discovers the other &this procsses that are running on other nodes, 
	and inserts itself into the communication tree that the other processes have 
	established
	The tree, maintained via TCP/IP sessions, is used to communicate between processes, 
	and to determine which &this process is the control process.  The control 
	process is the process at the root of the communication tree. 

&subcat The Control Process
	The instance of &this that is at the root of the communications tree is considered
	to be the service management control process.  The control process is responsible
	for driving the queries, calling for elections, and tabulating the ballots to 
	determine which node is awarded a service when one needs to be started. 
	
&space
&subcat Elections
	If it is determined that the service is not running, &this will initate an 
	election by sending a 'vote' request for the service  to each &this in the communication tree. 
	Each node that receives the request will drive the score calculator associated 
	with the service and will distribute a ballot via the communication tree. 
	The ballot, contains a numerical representation (0-99) of the node's ability to 
	host the service.  The higer the 'score' the more able the node is to host the service. 
	While the ballots are received by all nodes in the communication tree, the control
	process is responsible for determining the 'winner' and for causing the service to 
	start on that node. 
&space
	If an election for a service is called for within 30 minutes of the last election, &this 
	will assume that the last node awarded the service is having problems keeping the 
	service running. 
	When this happens, the node's ballots for the service will be ignored for a period of
	15 minutes.  If there is a question about which node was elected, and what scores
	were reported during the last election of a service, the &lit(ng_smreq) command can 
	be used to generate a recap or explaination of the service from the point of view
	of &this.


&space
&subcat Score Calculation
	Each service must provide a process that can be invoked by &this and 
	will generate a numeric score (0 - 99) which indicates the ability 
	of the node to host the service.  A score of zero (0), ensures that 
	the node will not be awarded the service.  For scores reported that
	are non-zero, the larger the score the more able the node is to host the 
	service. 
&space
	For each service, the computation of the score will be different. Typically
	the score will be based on factors that might include:
&space
&beglist
&item	Amount of available disk space
&item	Presence of other services running on the node
&item	The availability of a recent checkpoint file
&item	Node attributes
&item	Number and speed of CPUS
&endlist
&space

.if [ 0 ] 
CURRENTLY THIS IS NOT SUPPORTED
&subcat Multiple Service Instances
	It might be desireable to have a service running on more than one node in 
	the cluster.  The number of nodes to be awarded the service at the end of
	an election depends on the information contained in the service's pinkpages 
	variable (NG_SRVM_name).  Based on the number of desired hosts for the 
	service, &this will keep the top &ital(n) scores as it manages the election
	and will award the service to all winners at the same time. 
.fi

&space
&subcat Mulitple Control Processes
	Occasionally, due to several different possible causes, more than one node will 
	claim to be the root process. Gernally this is caused because a node did not 'hear' 	
	the root announcement as it attempted to join the tree and did not get any 
	reponses to its insertion request; this case is resolved quickly and usually without 
	any impact.  
&space
	The rare case of a physical network 'split' will cause the separation of 
	service manager process, into two or more groupes, and as a result multiple control processes emerge, one
	for each network fragment. 
	When this happens it is expected that each control process will elect services for its fragment, and  
	for as long as the nodes are segmented all will be well. As the 
	physical network reconnects into a single entity, 
	the control nodes detect each other (referred to as root collisions)  and  the processes 
	fall back into an organisation with a single control process. 
	With each collision, the process that remains the control process will ensure that 
	there are not multiple services executing by calling for an election of all managed 
	services, and restarting the services on the elected nodes, while stopping them on 
	any node that was not awarded the service. 
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a agent : Deinfes the host and port of the process that will act as the multicast
	agent.  This needs to be supplied only for nodes that are unable to multicast, 
	and when the nnsd service is not being used to manage communication between proceses.
&space
&term	-c cluster : Supplies the cluster name.
&space
&term	-d port : The port of the nnsd service that will manage the 'broadcast' list used
	to find and insert the process into the communications tree.
&space
&term	-p port : Supplies the port number that &this will use to communicate with other
	&this procsses. If not supplied, NG_SRVM_PORT is assumed to exist in pinkpages
	and will be used. 
&space
&term	-g address : The IP address of the multicast group that is to be used for 
	communication with other &this processes. 
&space
&term	-r : Remote node, process is not allowed to become the root of the tree.
&space
&term	-s scope : A scope to apply as a default to all services.  If not supplied 
	the cluster name is used. 
&space
&term	-S list : A list of service names to manage.  If multiple names are supplied
	they must be space separated.  For each name supplied, the pinkpages variable
	&lit(NG_SRVM_)&ital(name) is expected to exist and define the service management
	parameters. If this parameter is omitted, the list is taken from the 
	&lit(NG_SRVM_LIST) pinkpages variable.  Each name supplied may optionally be 
	assigned a scope by appending &lit(:)&ital(scopename) to the service name
	(e.g. DNSserver:lanseg_a). 
&space
&term	-t sec : The multicast time to live value in seconds. If not supplied 2s is used. 
&space
	
&term -v : Verbose mode.  Supplying more than two -v parameters generates lots of 
	goo to the standard error device. 
&endterms

&see
	&seelink(srvmgr:ng_smreq)
	&seelink(srmgr:ng_srvm_mon) 
	&seelink(pinkpages:ng_ppset) 
	&seelink(pinkpages:ng_ppget) 
	&seelink(pinkpages:ng_nar_set) 
	&seelink(pinkpages:ng_nar_get)


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Jan 2005 (sd) : Sunrise. (HBD-101-GMM)
&term 	22 Aug 2005 (sd) : Made some tweeks to prevent timing issues.  Election expiry 
	now increased by 8s  (was 4) when a node inserts itself as the better candidate. 
&term	20 Feb 2006 (sd) : Added continue service support.
&term	27 May 2006 (sd) : Converted from the round-robin protocol to a single control process
		protocol with communication via a binary tree of TCP/IP sessions. 
&term	13 Jun 2006 (sd) : Added timer rest command.
&term	27 Jun 2006 (sd) : Fixed issue if user entered service list that was not bang separated names.
&term	14 Jul 2006 (sd) : Found cause of listen port not being released; when we started a service we 
		did not close the fd and it was passed to the service and held as long as the service 
		was runing. We now fork and close all fds > 3 when we start or stop a service.  (v3.1)
&term	26 Jul 2006 (sd) : Unrecognised state name (not red/green) assumed to be yellow. 
&term	09 Sep 2006 (sd) : Fixed reset of election flag during winner determination. (Flag used only 
		for status dump.)
&term	27 Oct 2006 (sd) : Changed examination of ng_pp_get() functions to allow for the fact that it 
		now returns an empty string when the value is not found.
&term	27 Feb 2007 (sd) : Fixed memory leak in dpt_user_maint(). 
&term	13 Apr 2007 (sd) : Fixed report of qdata node when election is called for; scratches the qdata
		until next query.
&term	09 Aug 2007 (sd) : Corrected memory leak in compute_score(). 
&term	19 Jun 2008 (sd) : Added recap command to assist in understanding when it finds no winner. Changed
		reset lease message to include a newline. (v3.2)
&term	26 Sep 2008 (sd) : Added -r (remote) option to prevent process becoming the root on a remote (satellite) node (v3.3).
&term	15 Apr 2009 (sd) : Added support for a stop command to stop the process on the local node from the outside (v3.4).
&term	04 May 2009 (sd) : We now detect a root collision and the winning process will call for a new election 
		of every service.  This will eliminate any duplicate services that might have been the result
		of a tree split where the other root actually assigned services. (V3.5)
&term	20 Oct 2009 (sd) : Added more support to ignore a node's vote on a service if they were recently elected
		and the service seems not to be staying up. (V3.6)
&endterms
.sp

&scd_end
------------------------------------------------------------------ */
