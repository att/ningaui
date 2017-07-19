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
  Mnemonic:	coll_netif.c
  Abstract:	These routines are the collate network interface routines.
		This implements a callback paradigm ontop of the ng_si 
		(socket) library functions. 
		The port listener establishes all of the callback routines 
		and then invokes the polling process to  listens
		for new TCP sessions and drives the appropriate 
		routines when TCP/UDP data is received and TCP sessions 
		are established/disconnected.
  Date: 	22 April 2003
  Author: 	E. Scott Daniels
&term	09 Jun 2009 (sd) : Remved reference to deprecated ng_ext.h header file.
 ------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>

#include	<ng_socket.h>		/* socket callback driver lib */

					/* symbol table name spaces */
#define NO_HANDLE	-1		/* ignore handle data */
#define	BY_HANDLE	1		/* sessions hashed using the network handle */
#define BY_NAME		2		/* sessions hashed using name */

#define SESS_UNK	0		/* unknown session */
#define SESS_SEN	1		/* seneshal session */

#define SESSION_OK	1		/* session wiht seneschal is ok */
#define SESSION_BAD	0		/* when we have lost the seneschal session */

#define SENESCHAL_RETRY	20		/* delay time before next attempt to connect to seneschal */

typedef struct	Session_t {		/* we should have but one session (seneschal), but maybe someday we become a daemon... */
	int	handle;			/* the network session handle */
	int	type;			/* seneschal session or some other kind */
	char	hname[25];		/* ascii version of handle for symtab lookup */
	char	*name;			/* session name if supplied by sender */
	char	partial[NG_BUFFER];	/* buffer if we receive partial record (no newline) */
} Session_t;

static const char *seneschal_port;	/* seneschals well known port */
static Symtab	*symbols = NULL;	/* hashes session name to a session block */
static char	*wbuf = NULL;		/* work buffer */
int	have_seneschal = SESSION_BAD;	/* maps the state of the seneschal session - if bad we try to connect */

/* ------------- local protocols -------------------- */
/* callbacks - driven when ng_siwait noitces an event */
static int cb_tcpdata( void *data, int handle, char *buf, int len );	/* buffer received on a tcp session (handle) */
static int cb_udpdata( void *data, char *buf, int len, char *from, int fd );    /* buffer received via UDP */
static int cb_newsess( void *data, int handle, char *from );		/* new connection */
static int cb_disc( void *data, int handle );				/* session blew away */
static int cb_signal( void *data, int flags );				/* signal received */

/* general manipulation functions */
static void eat_msg( Session_t *s, char *msg );			/* parses complete messages */
static Session_t *get_session( int handle, char *name );	/* finds a session block based on sid */
static void deref_session( int handle, char *name );		/* dereferences and cleans up a fallen session */
static void add_session( Session_t *s, int handle, char *name );	/* obvious */
static void add2partial( char *partial, int plen, char *msg, int mlen );	/* adds a buffer to the sessions partial msg buffer */

static void seneschal_conn( );					/* attempts connection to seneschal */



/* initialise and register callbacks.  called directly when a thread is NOT being used to 
   deal with network traffic 
*/
void *port_init( char *port )
{
	extern 	int ng_sierrno;
	extern	int errno;

	errno = 0;
	/*if( ng_siinit( SI_OPT_FG | SI_OPT_ALRM, atoi( port ), atoi( port ) ) != 0 )*/	/* initialise the network interface */
	if( ng_siinit( SI_OPT_FG | SI_OPT_ALRM, atoi( port ), 0 ) != 0 )	/* initialise the network interface */
	{
		ng_bleat( 0, "port_init: unable to initialise the network interface sierr=%d err=%d(%s)", ng_sierrno, errno, strerror( errno ) );
		exit( 1 );
	}

	wbuf = (char *) ng_malloc( NG_BUFFER, "port_listener: work buffer" );	/* general working buffer */

	symbols = syminit( 127 );	/* should be a nice size for our small private idaho */

						/* register the routines driven by ng_siwait() whene events happen */
	ng_sicbreg( SI_CB_CONN, &cb_newsess, NULL );   		/* called when connection is received */
	ng_sicbreg( SI_CB_DISC, &cb_disc, NULL );    		/* disconnection */
	ng_sicbreg( SI_CB_CDATA, &cb_tcpdata, (void *) NULL );	/* cooked (tcp) data recieved */
	ng_sicbreg( SI_CB_RDATA, &cb_udpdata, (void *) NULL );	/* raw (udp) data received */
	ng_sicbreg( SI_CB_SIGNAL, &cb_signal, (void *) NULL );	/* signal received */

	return NULL;
}
/* ----------------------------------------------------------------------------
	initialises and then passes control to the socket interface.
	used when running threads as this will block until all sessions are 
 	closed, or a cb routine indicates a shutdown.
	invokes ng_siwait() to listen to network and drive our callbacks
   ---------------------------------------------------------------------------- 
*/
void *port_listener( char *port )
{
	extern 	int ng_sierrno;
	extern	int errno;

	port_init( port );			/* establish listener */

	ng_bleat( 3, "port_listener: setting alarm for seneschal connection attempt in 2s" );
	alarm( 2 );				/* alarm should pop a seneschal connection attempt after established */

	errno = 0;
	ng_siwait( );				/* loops waiting for input and drives our callback; returns when net shuts */

	ng_bleat( 2, "port_listener: ng_siwait finished: sierr=%d err=%d(%s)", ng_sierrno, errno, strerror( errno ) );

	return NULL;
}

/* ------- callback routines driven at various times by ng_siwait( ) ---------------- */

/* deal with a signal */
static int cb_signal( void *data, int flags )
{

	ng_bleat( 5, "cb_signal: signal: %x seneschal-state=%d", flags, have_seneschal );

	if( flags & SI_SF_ALRM  )
	{
		if( ! have_seneschal )				/* alarm - need to try seneschal again? */
		{
			seneschal_conn( );			/* connect, it will reset alarm if connect fails */
			
			if( have_seneschal )
			{
				symtraverse( symtab, JOB_SPACE, submit_job, NULL );	/* submit pending jobs */
				if( timeout )
					expiry = ng_now( ) + (timeout * 10);	/* timeout is seconds, expiry is tenths */
			}
		}
		else
		{
			ng_bleat( 5, "cb_signal: invoking maint: seneschal-state=%d", flags, have_seneschal );
			if( job_maint( ) )			/* jobmaint returns true if its time to exit */
				return SI_RET_QUIT;		/* causes us to stop */
				
			have_seneschal = SESSION_OK;		

			alarm( 3 );		/* wake again in 3 seconds to do maint */
		}

	}

	return SI_RET_OK;		/* keeps us going */
}


/* called when we receve a connection request 
  	- create session block
	- register it by the handle - must wait for a message to register name

	At the moment these are not expected since we use a generic port and not a well known one
	because we establish a session with seneschal, and expect nobody to need to start a session to 
	us.
*/
static int cb_newsess( void *data, int handle, char *from )
{
	extern int 	errno;

	Session_t	*s;

	s = (Session_t *) ng_malloc( sizeof( Session_t ), "cb_newsess: session mgt block" );
	memset( s, 0, sizeof( Session_t ) );

	s->handle = handle;
	s->type = SESS_UNK;			/* anything inbound is a session */

	sfsprintf( s->hname, sizeof( s->hname ), "%d", handle );
	add_session( s, handle, NULL );				/* install pointer to session block in the symtab */

	ng_bleat( 2, "newsess: session connection from: %s (%d)", from, handle );

	return 0;
}

/* called when someone terminates a session 
	dereference the session block and free memory
*/
static int cb_disc( void *data, int handle )
{
	Session_t	*s;
	int 	type;
	int	map[2];			/* job conversion map */

	if( (s = get_session( handle, NULL )) )
	{
		type = s->type;
		ng_bleat( 2, "seesion disconnected: %d type=%s", handle, s->type == SESS_SEN ? "seneschal" : "other" );
		deref_session( handle, s->name );		/* trash both symtab references to this block */
		if( s->name )
			ng_free( s->name );
		ng_free( s );

		if( type == SESS_SEN )			/* lost the seneschal session -- cannot go on */
		{
			have_seneschal = 0;			/* will cause an attempt next time the alarm pops */
			ng_bleat( 0, "cb_disc: lost seneschal session" );
			map[0] = JS_RUNNING;		/* convert running jobs to pending -- resbumitted on reconnect */
			map[1] = JS_PENDING;
			symtraverse( symtab, JOB_SPACE, convert_job, map );
			running_jobs = 0;			/* we have to assume that none are running now */

#ifdef ABORT_ON_LOSS
			failed_jobs = 128;
			return SI_RET_QUIT;			/* cause a termination */
#endif
		}
			
	}
	else
		ng_bleat( 0, "cb_disc: could not find session for handle: %d", handle );

	return 0;
}


/* called when data received on TCP session
  we assume that complete messages are newline terminated and may be
  broken up across multiple TCP/IP datagrams. Partial messages 
  are queued on the session block until the whole message is received. 
  we call eat_msg() for each complete message that was in the buffer. 
  in a threaded app, we would likely hang the message onto the work 
  queue rather than calling eat_msg().
*/
static int cb_tcpdata( void *data, int handle, char *buf, int len )
{
	Session_t *s;
	char 	*msg;		/* at the start of the message in the buffer */
	char	*mend;		/* at the end of the current message in the buffer */
	char	*end;		/* end of the buffer */
	long	mcount = 0;	/* msg contained in the tcp buffer */

	end = buf + len;	/* mark end */
	mend = buf;		/* start searching with the beginning of the buffer */

	if( (s = get_session( handle, NULL )) )
	{
		while( mend < end )
		{
			mcount++;
			msg = mend;			/* start looking where we left off */
			if( s->partial[0] == 0 )						/* only if nothing queued */
				for( ; msg < end && (*msg == ' ' || *msg == '\t'); msg++ );	/* skip leading white space */

			for( mend = msg; mend < end && *mend != '\n'; mend++ );			/* skip to end of message */

			if( mend < end  && *mend == '\n' )					/* complete message */	
			{
				*mend = 0;			/* make string */
				mend++;				/* where to start to dig next one */

				if( s->partial[0] )		/* last time in for this session we had a partial buffer */
				{
					add2partial( s->partial, sizeof( s->partial ), msg, mend-msg );
					eat_msg( s, s->partial );			/* eat the reconstruction */
				}
				else						/* message is complete as is */
					eat_msg( s, msg );			/* deal with it */

				s->partial[0] = 0;		/* nothing partial any more */
			}
			else					/* partital message -- assuming we reached the buffer end */
			{
				add2partial( s->partial, sizeof( s->partial ), msg, mend-msg );	 /* tack it into the hold buf */
			}
		}

		ng_bleat( 4, "cb_tcpdata: tcp buffer contained %d messages %s", mcount, s->partial[0] ? "incomplete" : "complete" );
	}
	else
		ng_bleat( 0, "cb_tcpdata: message received for non-existant session: %d(sid)", handle );


	return 0;
}


/* 
   UDP buffers contain commands to act on.
	None expected at the moment

*/
static int cb_udpdata( void *data, char *buf, int len, char *from, int handle )
{
	int	rc = SI_RET_OK;			/* return value - set to SI_RET_QUIT when we want to shut off */
	char	*strtok_p;			/* cache for strtok_r */
	char	*tok;				/* for parsing stuff */
	char	*msg;				/* pointer to the message to use (may become a copy) */
	

	ng_bleat( 4, "udpdata: got %d bytes", len );		

	if( buf[len-1] < ' ' )		/* assume newline or somesuch terminator */
	{
		buf[len-1] = 0;			/* make a string and just point msg at it - faster */
		msg = buf;
	}
	else					/* copy - cannot guarentee that buf[len+1] is inside the buffer */
	{
		if( len >= NG_BUFFER -1 )
		{
			ng_bleat( 0, "udpdata: buffer overrun from: %s (%.100s)", from, buf );
			return SI_RET_OK;
		}

		memcpy( wbuf, buf, len );
		wbuf[len] = 0;

		msg = wbuf;
	}

	ng_bleat( 4, "udpdata: got (%s)", msg );
	tok = strtok_r( msg, " ", &strtok_p );		/* first non blank */

	switch( *tok )			/* ordered by frequency expected (mostly) */
	{
			default:	
				ng_bleat( 0, "udpdata: unrecognised message from: %s (%s)", from, tok );
				break;
	}

	return rc;
}

/* 
	build an id string (name=junk) to send to seneschal so we get 
	status messages from jobs we submit.
	we do NOT want this id to be the same as the last time as we might 
	get status messages that we are not aware of because we do not checkpoint
*/
char *build_id( )
{
	char	buf[NG_BUFFER];
	
	sfsprintf( buf, sizeof( buf ), "name=collate_%x_%d\n", ng_now( ), getpid( ) );

	return strdup( buf );
}

/* ----------------- general support routines -------------------------- */

/*	attempt session with seneshal. if it fails, set an alarm to drive a retry 
	nawab assumes that both it and seneschal are running on the same host.
	this does not have to be, but for now the leader runs both.
*/
static void seneschal_conn( )
{
	extern	int 	errno;
	static char *myid = NULL;	

	Session_t *s;			/* session block */
	char	seneschal_address[256];	/* address to connect to */
	int	fd;			/* session file descritor if successful */
	const char *jobhost = NULL;	/* host that seneschal is on */


	if( ! myid )
		myid = build_id( );		/* build an id string to send to seneschal */

	if( (seneschal_port = ng_env_c( "NG_SENESCHAL_PORT" )) == NULL )		/* get his well known port */
	{
		ng_bleat( 0, "seneschal_conn: abort: cannot find seneschal port (NG_SENESCHAL_PORT) in environment/cartulary" );
		exit( 1 );
	}

	if( (jobhost = ng_env_c( "srv_Jobber" )) == NULL )
	{
		ng_bleat( 0, "seneschal_conn: abort: cannot find seneschal host (srv_Jobber) in environment/cartulary" );
		exit( 1 );
	}

	sfsprintf( seneschal_address, sizeof( seneschal_address ), "%s:%s", jobhost, seneschal_port );
	if( (fd = ng_siconnect( seneschal_address )) >= 0 )
	{
		ng_bleat( 2, "seneschal_conn: session established with seneschal (%s) on fd %d", seneschal_address, fd );
		s = (Session_t *) ng_malloc( sizeof( Session_t ), "cb_newsess: session mgt block" );
		memset( s, 0, sizeof( Session_t ) );

		s->handle = fd;
		s->type = SESS_SEN;			/* mark as a seneschal session */

		sfsprintf( s->hname, sizeof( s->hname ), "%d", fd );
		add_session( s, fd, "seneschal" );				/* install pointer to session block in the symtab */
		have_seneschal = SESSION_OK;			/* need to send jobs to seneschal */
		ng_sisendt( fd, myid, strlen( myid ) );		/* register so we get notifications */

		alarm( 1 );					/* set a quick wake up call to process & resend */
	}
	else
	{
		ng_bleat( 5, "seneschal_conn: connetion failed: %s:  %s", seneschal_address, strerror( errno ) );
		alarm( SENESCHAL_RETRY );
	}
}

/* 	support for tcp callback - adds msg to the paritally filled buffer 
*/
static void add2partial( char *partial, int plen, char *msg, int mlen )
{
	int  nend;			/* new end of message */

	if( partial == NULL || msg == NULL )	
		return;
		
	nend = mlen + strlen( partial );
	if( (strlen( partial ) + mlen  + 1) < plen )		/* room? */
	{
		memcpy( partial + strlen( partial ), msg, mlen );		/* tack on the message then deal with it */
		partial[nend] = 0;
	}
	else
	{
		msg[mlen-1] = 0;
		ng_bleat( 0, "cb_tcpdata:buffer overrun filling partial: %.100s(had...) %s(got)", partial, msg );
		*partial = 0;				/* trash the bufer */
	}
}

/*
	driven when a complete message has been constructed from TCP datagrams.
	expected messages depend on the source. when the source is seneschal, 
	then we expect the message to be a status buffer and pass it to parse_status.
	These are the only messages expected at this time.
*/
static void eat_msg( Session_t *s, char *msg )
{
	ng_bleat( 4, "eat_message: (%s)", msg );
	switch( s->type )
	{
		case SESS_SEN:				/* seneschal status message */
			ng_bleat( 3, "eat_msg: status: (%s)", msg );
			if( *msg )
				parse_status( msg );
			break;

		default:	
			ng_bleat( 0, "eat_message: unknown session type: %d", s->type );
			break;
	}

	alarm( 3 );		/* prevent wedging if alarm popped while handling data */
}


/*	add a session block to the hash table; sessions are referenced by their 'handle'
	and their name if it is supplied by the other end on a name= message
 */
static void add_session( Session_t *s, int handle, char *name )
{
	if( s )
	{
		if( name )
			symlook( symbols, name, BY_NAME, s, SYM_NOFLAGS  );

		if( handle != NO_HANDLE )	
		{
			sfsprintf( s->hname, sizeof( s->hname ), "%d", handle );
			symlook( symbols, s->hname, BY_HANDLE, s, SYM_NOFLAGS );
		}
	}
}

/* dereference a session block from the hash table */
static void deref_session( int handle, char *name )
{
	char	buf[100];

	if( name )
		symdel( symbols, name, BY_NAME );

	if( handle != NO_HANDLE )
	{
		sfsprintf( buf, sizeof( buf ), "%d", handle );
		symdel( symbols, buf, BY_HANDLE );
	}
}

/* find the session block based on either name or handle */
static Session_t *get_session( int handle, char *name )
{
	Syment	*se;
	char	buf[100];

	if( name )
	{
		if( (se = symlook( symbols, name, BY_NAME, NULL, SYM_NOFLAGS )) )
			return (Session_t *) se->value;
	}
	else
	{
		sfsprintf( buf, sizeof( buf ), "%d", handle );
		if( (se = symlook( symbols, buf, BY_HANDLE, NULL, SYM_NOFLAGS )) )
			return (Session_t *) se->value;
	}
	
	return NULL;
}

/* find the seneschal session and send the message to it, returns 0 if no session */
int send2seneschal( char *buf )
{
	Session_t *s;			/* session block */
	int	rc = 0;			/* return code - 1 is good */
	int	status = 0;

	if( have_seneschal && buf )	
	{
		if( (s = get_session( NO_HANDLE, "seneschal" )) != NULL )
		{
			if( (status = ng_sisendt( s->handle, buf, strlen( buf ) )) >= 0 )
				rc = 1;
			else
				ng_bleat( 0, "send2senesschal: error sending buffer: ng_sisendt() returned %d", status );
		}
		else
		{
			ng_bleat( 0, "send2seneschal: cannot find seneschal session block" );
			have_seneschal = SESSION_BAD;
		}
	}

	return rc;
	
}
