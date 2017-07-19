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
# ---------------------------------------------------------------------------------
# Mnemonic:	ng_rm_d1agent
# Abstract:	provide an application interface to repmgr dump1 facility
#		
# Date:		23 Apr 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sfio.h>
#include <ng_socket.h>
#include <signal.h>

#include <netinet/in.h>
#include <ningaui.h>

#include <rm_d1agent.man>



#define END_OF_DATA 	0xbe		/* repmgr defines this in remgr.h so it MUST match that! */

#define MAX_SESSIONS	256

#define DEF_FI_LEASE	3300		/* we expect users to renew watches every 5.5 min unless they set their own lease */

			/* symbol table name spaces: 0-max_sessions are used to map basenames watched by each session using the fd */
			/* all other symtab namespaces start with MAX_SESSIONS and work up */
#define SESSION		MAX_SESSIONS + 1		/* maps session blocks */

#define D1INFO		1		/* work types */
#define NOT_FOUND	2

typedef struct session {
	char	*name;
	int	flags;
	int	fd;			/* file descriptor */
	void	*flow;			/* flow management stuff to allow records to be split between tcp messages */
	long	fcount;
	long	lease_len;		/* number of tenths of seconds that a file watch lease will be set/extended by */
} Session_t;

typedef struct finfo {
	struct finfo *next;
	char	*request;		/* the dump1 string to send to repmgr; base points into this string */
	char	*base;
	char	*d1;			/* the junk we last got from repmgr about the basename */
	int	sid;			/* quick reference back to the session */
	ng_timetype expiry;
} Finfo_t;

typedef struct work_blk {
	struct work_blk *next;
	int	type;
	char 	*buf;
} Work_t;

extern int 	verbose;
extern int	ng_sierrno;

Session_t *sessions[MAX_SESSIONS];		/* indexed by the session fd */
Symtab	*symmap = NULL;		
char	*rmhost = NULL;				/* set if user supplies a hard host on the command line */
char	*rport = NULL;
int	alarm_time = 0;
int	ok2run = 1;

long	ecount;					/* number of watches expired during last pass */
long	changes[4];				/* changes noticed in last 5/10/15 min */
ng_timetype system_time = 0;			/* one global timestamp to prevent over requesting time */

Work_t	*wqh = NULL;				/* work queue head/tail */
Work_t 	*wqt = NULL;
long	wq_size = 0;				/* number things queued */
int	max_work = 25;				/* number of work blocks we will process in a single shot */
int	rm_lease = 6000;			/* amount of time (tenths) we wait for a repmgr response before declaring the sesion hung */

char	*version="v1.2/07098";

/* --------------------------------------------------------------------------------------------------- */

/* add a repmgr response to the queue to process later */
void queue_work( char *buf, int type )
{
	Work_t *wp;

	wp = (Work_t *) ng_malloc( sizeof( Work_t ), "work block" );
	memset( wp, 0, sizeof( *wp ) );

	wp->buf = strdup( buf );
	wp->type = type;

	if( wqt )			/* add to tail of queue */
	{
		wqt->next = wp;
		wqt = wp;
	}
	else
		wqt = wqh = wp;

	wq_size++;
	
}

/* find a file reference in the symbol table for the basename and session */
Finfo_t *get_finfo( Session_t *sp, char *base )
{
	Syment *se = NULL;

	if( (se = symlook( symmap, base, sp->fd, NULL, SYM_NOFLAGS )) != NULL )
		return (Finfo_t *) se->value;

	return NULL;
}

/* start watching a file for the process at the other end of the session */
void add_watch( Session_t *sp, char *buf, int force )
{
	char	*base;
	char	*cp;
	Finfo_t	*fp = NULL;

	cp = strchr( buf, ' ' );
	while( *cp == ' '  )
		cp++;

	if( *cp < ' ' )
		return;

	if( (fp = get_finfo( sp, cp )) == NULL )
	{
		fp = (Finfo_t *) ng_malloc( sizeof( *fp ), "file info block" );
		memset( fp, 0, sizeof( *fp ) );

		fp->request = ng_strdup( buf );				/* save whole request */
		fp->base = fp->request + (cp - buf );			/* point at basename in the request */
		fp->sid = sp->fd;					/* quick ref back to the session to report change */
		fp->expiry = system_time + sp->lease_len;			


		symlook( symmap, fp->base, sp->fd, fp, SYM_NOFLAGS );	/* map to the block */

		sp->fcount++;

		ng_bleat( 3, "watch added for: %s (%d): %s", sp->name ? sp->name : "unnamed", fp->sid, fp->base );
	}
	else
	{
		fp->expiry = system_time + sp->lease_len;				/* still asked for, update its lease */

		if( force && fp->d1 )			/* force it to change if it already existed */
		{
			ng_bleat( 3, "watch added with force for: %s", fp->base  );
			ng_free( fp->d1 );	
			fp->d1 = NULL;
		}
	}
}

/* stop watching the file */
void drop_watch( Finfo_t *fp )
{
	Finfo_t *pfp;			
	
	ng_bleat( 3, "watch dropped: %d %s", fp->sid, fp->base );
	sessions[fp->sid]->fcount--;
	symdel( symmap, fp->base, fp->sid );
	ng_free( fp->request );		/* remember base just points into this string so we do not free */
	ng_free( fp->d1 );
	ng_free( fp );
}

/* invoked by symtraverse for each finfo block in the session's space  
	drops all watches for a given session -- data is not used
*/
void drop_all_watches( Syment *se, void *data )
{
	Finfo_t *fp;
	int	sid;

	if( (fp = (Finfo_t *) se->value) != NULL )
	{
		drop_watch( fp );
	}
	else
		symdel( symmap, se->name, se->space );				/* this should never happen */
	
}

int count_finfo( )
{
	int i;
	long count = 0;

	for( i = 0; i < MAX_SESSIONS; i++ )
		if( sessions[i] )
			count += sessions[i]->fcount;

	return count;
}

/* --------------------------- session management ---------------------------------------------------- */
Session_t *get_session( char *name )
{
	Session_t *sp = NULL;
	Syment	*se = NULL;

	if( (se = symlook( symmap, name, SESSION, NULL, SYM_NOFLAGS )) != NULL )
		sp = (Session_t *) se->value;

	return sp;
}

/* called when we receive a disconnect to clean up the session */
void drop_session( int fd )
{
	Session_t *sp;

	if( (sp = sessions[fd]) == NULL )
		return;


	if( sp->fcount )
		symtraverse( symmap, fd, drop_all_watches, NULL );	/* drop all watches in the session's space */

	ng_bleat( 3, "session dropped: %s fd=%d fcount=%d", sp->name ? sp->name : "unnamed", fd, sp->fcount );
	if( sp->name )
	{
		symdel( symmap, sp->name, SESSION );
		ng_free( sp->name );
	}
	ng_flow_close( sp->flow );

	sessions[fd] = NULL;
	ng_free( sp );
}

/* connect to repmgr. if -s used on command line, we always connect to that host. if not
   then we connect to srv_Filer getting the value every time in case it moves
*/
int conn2rm( )
{
	static ng_timetype	expiry = 0;		/* prevent repmgr sessions from hanging (in linux we see this) */

        Session_t *sp;                   /* session block */

        int     fd;                     /* session file descritor if successful */
        char    *rm_host;             	/* host repmgr lives on */
	char	rm_address[1024];	/* address string host:port */
        
	if( count_finfo() == 0 )
		return -1;

	if( (sp = get_session( "repmgr" )) != NULL )
	{
		if( ng_now() > expiry )
		{
			ng_bleat( 1, "hung repmgr session being dropped" );
			expiry = 0;
			ng_siclear( sp->fd );			/* drain any queued data first or session will not close */
			ng_siclose( sp->fd );			/* close the socket first */
			drop_session( sp->fd );			/* then clean up our data */
			sp = NULL;
		}
		else
			return -1;				/* one already connected */
	}

	if( rmhost )
		rm_host = ng_strdup( rmhost );				/* user supplied a specific node; use it every time regardless */
	else
	{
        	if( (rm_host = ng_pp_get( "srv_Filer", NULL )) == NULL || !*rm_host )    /*get each time we connect; allows repmgr to move */
        	{
			if( rm_host )
				ng_free( rm_host );					/* if an empty string returned */
			ng_bleat( 0, "conn2rm: cannot determine srv_Filer host" );
			return -1;
        	}
	}
        
        sfsprintf( rm_address, sizeof( rm_address ), "%s:%s", rm_host, rport );
        if( (fd = ng_siconnect( rm_address )) >= 0 )
        {
		expiry = ng_now( ) + rm_lease; 		/* we will close the session if repmgr does not drop it in 5 min */
                ng_bleat( 3, "conn2rm: session established with repmgr (%s) on fd %d", rm_address, fd );
                sp = (Session_t *) ng_malloc( sizeof( Session_t ), "conn2rm: session mgt block" );
                memset( sp, 0, sizeof( Session_t ) );

		if( sessions[fd] )				/* just in case we still think one is there */
			drop_session( fd );

		sessions[fd] = sp;
		sp->name = ng_strdup( "repmgr" );
		sp->fd = fd;

		symlook( symmap, sp->name, SESSION, sp, SYM_NOFLAGS );		/* add to symbol table */
        }
        else
        {
                ng_bleat( 4, "conn2rm: connection failed: %s:  %s", rm_address, strerror( errno ) );
		return -1;
        }

	ng_free( rm_host );
	return fd;
}


/* process a message from client or repmgr */
void eat_msg( Session_t *sp, char *buf )
{
	char	*tok;
	char	*strtok_p;

	ng_bleat( 5, "eat_msg: %s", buf );
	switch( *buf )
	{
		case 'D':		/* Dump1 -- assume this is a force reset if we have info so they get info regardless of what we think */
			if( strncmp( buf, "Dump1 ", 6 ) == 0 )
			{
				*buf = 'd';
				add_watch( sp, buf, 1 );
			}
			break;

		case 'd':
			if( strncmp( buf, "dump1 ", 6 ) == 0 )
			{
				add_watch( sp, buf, 0 );		/* add watch without a reset */
			}
			break;

		case 'e':
			if( strncmp( buf, "exit 4360", 9 ) == 0 )
			{
				ok2run = 0;
				ng_bleat( 0, "exit command received; shutting down " );
			}
			else
			if( strncmp( buf, "expire ", 7 ) == 0 )
			{
				strtok_r( buf, " :=", &strtok_p );
				if( (tok = strtok_r( NULL, " :=", &strtok_p )) != NULL )		/* point at basename */
				{
					Finfo_t *fp;

					if( (fp = get_finfo( sp, tok )) != NULL )
						fp->expiry = 0;
				}
			}
			break;

		case 'f':					/* file info from repmgr */
			if( strncmp( buf, "file ", 5 ) == 0 )
			{
				queue_work( buf, D1INFO );			/* set it asside to process in chunks */
			}
			break;

		case 'l':					/* lease value */
			if( strncmp( buf, "lease ", 6 ) == 0 )
			{
				strtok_r( buf, " :=", &strtok_p );
				if( (tok = strtok_r( NULL, " :=", &strtok_p )) != NULL )		/* point at value */
				{
					if( *tok > '9' )
					{
						if( (sp = get_session( tok )) == NULL )
							break;
				
						tok = strtok_r( NULL, " :=", &strtok_p );
					}
	
					sp->lease_len = strtol( tok, 0, 10 );
					sp->lease_len *= 10;			/* convert to tenths of seconds */
					ng_bleat( 1, "session lease set: %s (%d) %I*d tsec", sp->name ? sp->name : "unnamed", sp->fd, Ii(sp->lease_len) );
				}
			}
			break;

		case 'n':					/* name <string>, not found: basename */
			if( strncmp( buf, "not found", 9 ) == 0 )
			{
				queue_work( buf, NOT_FOUND );
			}
			else
			if( strncmp( buf, "name ", 5 ) == 0 )
			{
				{
					strtok_r( buf, " :=", &strtok_p );
					if( (tok = strtok_r( NULL, " :=", &strtok_p )) != NULL )		/* point at name */
					{
						if( sp->name )
							ng_free( sp->name );
						sp->name = strdup( tok );
		
						symdel( symmap, sp->name, SESSION );
						symlook( symmap, sp->name, SESSION, sp, SYM_NOFLAGS );
						ng_bleat( 2, "session named: %d=%s", sp->fd, sp->name );
					}
				}
			}
			break;

		case 'r':
			if( strncmp( buf, "rmlease", 7 ) == 0 )
			{
				rm_lease = atoi( buf+8 ); 
				ng_siprintf( sp->fd, "ng_rm_d1agent: rmlease reset to %ds\n.\n", rm_lease/10 );	/* end of data back to sender on this one */
				ng_bleat( 0, "rmlease reset to: %ds", rm_lease/10 );
			}
			break;

		case 'v':
			if( strncmp( buf, "verbose ", 8 ) == 0 )
			{
				strtok_r( buf, " :=", &strtok_p );
				if( (tok = strtok_r( NULL, " :=", &strtok_p )) != NULL )		/* point at name */
				{	
					verbose = atoi( tok );
					ng_bleat( 0, "verbose changed to: %d", verbose );	
				}
				ng_siprintf( sp->fd, "%s verbose now: %d\n.\n", version, verbose );	/* end of data back to sender on this one */
			}
			else
			if( strncmp( buf, "verify", 6 ) == 0 ) 
			{
				ng_siprintf( sp->fd, "ng_rm_d1agent: %s\n.\n", version );	/* end of data back to sender on this one */
			}
			break;

		default:
			if( *buf && *buf != '\n' )			/* immediate newline likely repmgr's handshake */
				ng_bleat( 0, "unrecognised command received: %s", buf );
			break;
			
	}

}

/* --------------------  work ------------------------------------------------------------*/
/* called by symtraverse. se points to a finfo block and data to the repmgr file desc 
   sends the dump1 request for the file to repmgr
*/
void send2rm( Syment *se, void *data )
{
	int 	rfd;		/* fd for session with repmgr */
	Finfo_t *fp;

	if( (fp = (Finfo_t *) se->value) != NULL )
	{
		if( fp->expiry < system_time )		/* this has expired; just trash it */
		{
			ecount++;
			ng_bleat( 3, "watch expired for: %s %s", sessions[fp->sid]->name ? sessions[fp->sid]->name : "unnamed", fp->base );
			drop_watch( fp );
		}
		else
		{
			rfd = *((int *) data);

			ng_siprintf( rfd, "%s\n", fp->request );
		}
	}
}

/* send dump1 requests to repmgr */
void dump1_request( )
{
	int	rmfd = -1;
	int 	i;


	for( i = 0; i < MAX_SESSIONS; i++ )
		if( sessions[i] )
			break;				/* verify that we have something to watch before making a conn */
	if( i >= MAX_SESSIONS )
		return;

	if( (rmfd = conn2rm()) < 0 )
	{
		ng_bleat( 2, "skipped sending requests to repmgr, still waiting for response from last one" );
		return;					/* no session, or we are still waiting for response to last one */
	}

	ecount = 0;					/* counter of number expired this pass */
	for( i = 0; i < MAX_SESSIONS; i++ )
	{
		if( sessions[i] )
			symtraverse( symmap, sessions[i]->fd, send2rm, &rmfd );
	}
	ng_siprintf( rmfd, "%c", END_OF_DATA );		/* NOTE: repmgr does not want a newline after the end of data character */

	if( ecount )
		ng_bleat( 2, "%I*d watches expired during last pass", Ii(ecount) );
}

/* take a dump1 result for a file and update all finfo blocks. if we note a change, then send the buffer to the session 
file rm.b003.cpt: md5=14478cbcd8badab92c211f83f9d9aede size=551386 noccur=-2 token=0 matched=1 stable=1#ningd7 /ningaui/fs01/15/22/rm.b003.cpt#ningd5 /ningaui/fs00/01/00/rm.b003.cpt
*/
update_finfo( char *buf )
{
	Finfo_t *fp;
	char	*bp;	/* pointer to start of name in buf */
	char	*ep;	/* ponter to end */
	char	base[NG_BUFFER];
	int 	i;

	if( *buf == 'f' )			/* assume file basename: junk */
	{
		bp = buf + 5;		
		if( (ep = strchr( buf, ':' )) == NULL )
			return;

		*ep = 0;
		strcpy( base, bp );	/* save the basename for searches */
		*ep = ':';
	}
	else
	{
		ep = strrchr( buf, ' ' )+1;		/* at basename in not found: <name> message */
		strcpy( base, ep );
	}

	for( i = 0; i < MAX_SESSIONS; i++ )		/* check for change based on last sent to each session */ 
		if( sessions[i] )
			if( (fp = get_finfo( sessions[i], base )) != NULL )
			{
				if( fp->d1 == NULL || strcmp( fp->d1, buf ) )		/* change? */
				{
					if( fp->d1 )
						ng_free( fp->d1 );
					fp->d1 = ng_strdup( buf );
					ng_siprintf( fp->sid, "%s\n", fp->d1 );
					ng_bleat( 4, "watched file update: %s: %s", sessions[i]->name ? sessions[i]->name : "unnamed", fp->d1 );
					changes[0]++;
				}
			}
}


/* process a few things that are on the work queue -- not too many so that we do not block */
do_work( )
{
	Work_t 	*wp = NULL;
	int	limit = max_work;

	
	while( wqh && limit )
	{
		wq_size--;
		limit--;

		wp = wqh;
		if( (wqh = wp->next) == NULL )
			wqt = NULL;

		switch( wp->type )
		{
			case NOT_FOUND:
			case D1INFO:
				update_finfo( wp->buf );
				break;

			default:
				break;
		}

		ng_free( wp->buf );	
		ng_free( wp );
	}
}


/* --------------------- callback support ------------------------------------------------*/

/* new session from the outside world */
cbconn( char *data, int fd, char *buf )
{
	Session_t	*sp = NULL;

	if( fd < 0 || fd > MAX_SESSIONS )
		return SI_ERROR;

	ng_bleat( 4, "session accepted: fd=%d %s", fd, buf );

	sp = (Session_t *) ng_malloc( sizeof( Session_t ), "conn: session block" );
	memset( sp, 0, sizeof( *sp ) );

	if( sessions[fd] != NULL )
		drop_session( fd );

	sessions[fd] = sp;
	sp->fd = fd;
	sp->lease_len = DEF_FI_LEASE;		/* set default lease */

 	return( SI_RET_OK );
}

cbdisc( char *data, int f )
{
	drop_session( f );
}


/* we dont open a udp port, so this should never trip */
int cbudp( void *data, char *buf, int len, char *from, int fd )
{
	ng_bleat( 0, "cbraw: received %d bytes from: %s: ignored!", len, from );

	return SI_RET_OK;
}


/* driven for any tcp data */
cbtcp( void *data, int fd, char *buf, int len )
{
	void *flow = NULL;
	Session_t	*sp;
	char	*b;

	if( (sp = sessions[fd]) == NULL )
		return SI_RET_OK;

	if(  (flow = sp->flow) == NULL  )
		flow = sp->flow = ng_flow_open( NG_BUFFER ); 

	ng_flow_ref( flow, buf, len );
	while( (b = ng_flow_get( flow, '\n' )) != NULL )      	/* get each newline separated buffer */
		eat_msg( sp, b );

	return SI_RET_OK;
}


/* deal with a signal */
int cbsignal( void *data, int flags )
{
	if( flags & SI_SF_ALRM  )
	{
		if( alarm_time )
		{
			sfprintf( sfstdout, "cb_signal: alarm signal received; setting timer for %ds\n", alarm_time );
			alarm( alarm_time );		/* wake again in 3 seconds to do maint */
		}
		else
			sfprintf( sfstdout, "cb_signal: alarm received, not reset\n" );
	}
	else
	if( flags & SI_SF_USR1 )
		sfprintf( sfstdout, "cb_signal: user1 signal received\n" );
	else
	if( flags & SI_SF_USR2 )
		sfprintf( sfstdout, "cb_signal: user2 signal received\n" );
	else
		sfprintf( sfstdout, "cb_signal: unknown signal. flags=x%x\n", flags );
	
	return SI_RET_OK;		/* continue processing */
}


/* ------------------------------- utils ------------------------------------------------------------------------ */
void stats( )
{
	int	nsess = 0;
	long	fcount = 0;
	int	i;
	
	for( i = 0; i < MAX_SESSIONS; i ++ )
	{
		if( sessions[i] )
		{
			nsess++;
			fcount += sessions[i]->fcount;
		}
	}

	ng_bleat( 1, "summary: %I*d(conn) %I*d(files) %I*d(wq) %I*d/%I*d/%I*d/%I*d", Ii(nsess), Ii(fcount), Ii(wq_size), Ii(changes[0]), Ii(changes[1]), Ii(changes[2]), Ii(changes[3]) );
}

void usage( )
{
	sfprintf( stderr, "usage: %s [-p port] [-r rmport] [-s rm-host] [-v]\n", argv0 );
	exit( 1 );
}

/* -------------------------------------------------------------------------------------------------------------- */
int main( int argc, char **argv )
{
	char	*port = NULL;
	ng_timetype next_dump = 0;		/* time we next try to send dumps to repmgr */
	ng_timetype next_changes = 0;		/* time we next update the 5/10/15 change counters */
	ng_timetype now = 0;
	int	status = 0;
	int	init_tries = 5;

	signal( SIGPIPE, SIG_IGN );

	memset( sessions, 0, sizeof( sessions ) );
	symmap = syminit( 4999 );

	ARGBEGIN{
		case 'p':	SARG( port ); break;		/* place for logging selected messages */
		case 'r':	SARG( rport ); break;
		case 's':	SARG( rmhost ); break;			/* where repmgr is running */
		case 'v':	OPT_INC(verbose); break;
		default:	
usage:
				usage(); 
				exit( 2 );
				break;
	}ARGEND


	if( ! port  &&  (port = ng_env( "NG_D1AGENT_PORT" )) == NULL )
	{
		ng_bleat( 0, "no port; NG_D1AGENT_PORT not defined and -p not given" );
		exit( 1 );
	}

	if( ! port  &&  (port = ng_env( "NG_D1AGENT_PORT" )) == NULL )
	{
		ng_bleat( 0, "no port; NG_D1AGENT_PORT not defined and -p not given" );
		exit( 1 );
	}

	if( ! rport  &&  (rport = ng_env( "NG_REPMGRS_PORT" )) == NULL )
	{
		ng_bleat( 0, "no port; NG_REPMGRS_PORT not defined and -r not given" );
		exit( 1 );
	}

	while( init_tries-- > 0 && (status = ng_siinit( SI_OPT_ALRM | SI_OPT_FG, atoi(port), -1 )) != 0 )	
	{
		if( errno != EADDRINUSE )
		{
			ng_bleat( 0, "unable to initialise network interface: ng_sierrno = %d errno = %d\n", ng_sierrno, errno );
			exit( 1 );
		}

		ng_bleat( 0, "[%d] address in use: napping 15s before trying again", getpid() );
		sleep( 15 );
	}

	if( status != 0 )
	{
		ng_bleat( 0, "unable to initialise network interface: ng_sierrno = %d errno = %d\n", ng_sierrno, errno );
		exit( 1 );
	}

	ng_bleat( 1, "socket interface initialisation ok, listening on %s", port );

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );   		 /* register network callbacks */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    
	ng_sicbreg( SI_CB_CDATA, &cbtcp, (void *) NULL );
	ng_sicbreg( SI_CB_RDATA, &cbudp, (void *) NULL );
	ng_sicbreg( SI_CB_SIGNAL, &cbsignal, (void *) NULL );	


	ng_bleat( 1, "watching..." );
	while( ok2run )
	{
		status = ng_sipoll( wqh ? 10 : 200 );
		system_time = ng_now( );				/* all can use a common timestamp each loop */

		now = ng_now( );
		if( now > next_changes )
		{
			changes[3] = changes[0] + changes[2];		/* add current to 10 min to gen 15min */
			changes[2] = changes[0] + changes[1];		/* add current to 5 min to gen 10min */
			changes[1] = changes[0];
			changes[0] = 0;
			next_changes = now + 3000;
		}

		if( now > next_dump )
		{
			stats( );
			next_dump = now + 600;				/* try again in a minute */
			if( ! wqh )					/* only if work queue is empty */
				dump1_request( );			/* send the dump1 requests off */
		}

		do_work( );						/* process more that has been queued on the work queue */
	}

	ng_sishutdown( );          /* for right now just shutdown */
}              /* main */


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_d1agent:Dump1 agent)

&space
&synop	ng_rm_d1agent [-p port] [-r rmport] [-s rmhost] [-v]

&space
&desc	&This accepts connections on a well known socket (NG_D1AGENT_PORT) and serves
	as an interface between processes and the replication manager's dump1 facility.
	The client processes send &this one or more dump1 requests which are periodically
	forwarded to replication manager. When the data received from replication manager 
	for any requested file changes, the changed dump1 output is passed back to the client. 
&space
	The client is expected to keep the session with &this active; if the session is closed, or
	disconnects, &this will delete all of the watch requests that were established for the client.
	When a file watch is created (an initial dump1 received), it is given a five minute lease. 
	The client must renew the lease by sending another dump1 request for the file before the 
	lease expires in order to keep the watch active.   This allows programmes that currently 
	send dump1 commands directly to replication manager to avoid many changes assuming that they 
	send dump1 commands on a regular basis while they need to detect changes to a file's status, 
	and cease their requests when they are done. 
&space
	Client programmes do have the option of sending a &lit(lease) command to &this which will 
	cause all watches to be given a longer lease. The command format is 
&space
	lease n
&space
	where &lit(n) is the number of seconds that a watch is valid.  The new lease length applied to 
	all dump1 commands received after it has been set.  Depending on the nature of the client application, 
	if it is necessary to force the expiration of a lease, an &lit(expire) command may be sent to &this.
	The command has the syntax of
&space
	expire filename
&space
	and causes the lease on the current watch to be set to an expired state.  
&space
&subcat	Other Accepted Commands
	The following commands are also recognised and allow an external programme to control 
	&this in real time:
&space
&begterms
&term	rmlease n : Sets the repmgr session lease to &lit(n) tenths of seconds. 
&space
&term	verbose n : Sets the verbose level to &lit(n.)
&space
&term	verify : Causes a response to be written to the sender (the version number) to verify that it is
	alive and well.
&endterms
&space
	For each of these commands, an end of data marker (dot newline) is sent following the last newline 
	terminated data record.


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p n : The port number that &this should listen on.  If not supplied, the value associated with 
	the pinkpages variable &lit(NG_D1AGENT_PORT) is used. 
&space
&term	-r n : The port number that replication manager is listening on. This defaults to the 
	value given to the &lit(NG_REPMGRS_PORT) if this option is not supplied. 
&space
&term	-s host : The host that &this should expect to find replication manager running on. This 
	should only be used when debugging &this and the value of &lit(srv_Filer) should be allowed to 
	supply the host name.
&space
&term	-v : One or more of these cause &this to be chatty to its standard error device. 
&endterms


&space
&exit	An exit code that is not zero indicates an error. Diagnostics, if possible, will be written to 
	the standard error device. 

&space
&see	ng_repmgr, ng_repmgrbt, ng_rmreq, ng_rmbt_req, srv_d1agent

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	23 Apr 2007 (sd) : Something new!
&term	05 Oct 2007 (sd) : Fixed typo.
&term	02 Nov 2007 (sd) : Fixed problem with releasing a hung session.  Added the rmlease command allowing 
		the length of the lease given to a repmgr session to be controlled dynamically. Increased the 
		default repmgr session lease to 5 minutes. 
&term	20 Jun 2008 (sd) : Forces a clear of the repmgr session before it is closed so that the socket interface
		does not attempt to drain the session before letting it go. (was causing repmgr to block) v1.2
&endterms


&scd_end
------------------------------------------------------------------ */

