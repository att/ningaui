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
	VERSION 2 -- we implement the driver rather than depending 
	on some sort of alarm.
 ----------------------------------------------------------------------
  Mnemonic:	n_netif.c
  Abstract:	These routines are the nawab network interface routines.
		This implements a callback paradigm ontop of the ng_si 
		(socket) library functions. 
		The port listener establishes all of the callback routines 
		and then invokes the polling process to  listens
		for new TCP sessions and drives the appropriate 
		routines when TCP/UDP data is received and TCP sessions 
		are established/disconnected.
  Date: 	8 January 2002
  Author: 	E. Scott Daniels
  Mods:		04 Dec 2003 (sd) - Now uses srv_Jobber to find seneschal; this host 
			is used if jobber is not defined.
		26 Apr 2004 (sd) - Added error back to user if bad command.
		04 Jul 2005 (sd) - Corrected typo in message.
		11 Apr 2006 (sd) - OK message was being sent with trailing null; fixed.
		31 Oct 2006 (sd) - converted to ng_sfopen to detect write errors.
		18 Jan 2007 (sd) - now passes msg after =end to send the user id to prog parse
		01 Mar 2007 (sd) - Mods to close memory leaks in seneschal connect.
 ------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>

#include	<ng_socket.h>		/* socket callback driver lib */

					/* symbol table name spaces */
#define NO_HANDLE	-1		/* ignore handle data */
#define	BY_HANDLE	1		/* sessions hashed using the network handle */
#define BY_NAME		2		/* sessions hashed using name */

#define SESS_PGM	1		/* programme session */
#define SESS_SEN	2		/* seneshal session */

#define SENESCHAL_RETRY	20		/* delay time before next attempt to connect to seneschal */

extern	int 	gflags;

typedef struct	Session_t {
	int	handle;			/* the network session handle */
	Sfio_t	*of;			/* output file if a programme submission session */
	int	type;			/* seneschal session or programme feed session */
	char	fname[255];		/* name of the output file for the programme */
	char	hname[25];		/* ascii version of handle for symtab lookup */
	char	*name;			/* session name if supplied by sender */
	char	partial[NG_BUFFER];	/* buffer if we receive partial record (no newline) */
} Session_t;

static char 	*seneschal_port;	/* seneschals well known port */
static char	seneschal_address[256];		/* address to connect to */
static Symtab	*symbols = NULL;	/* hashes session name to a session block */
int	have_seneschal = SESSION_BAD;	/* maps the state of the seneschal session - if bad we try to connect */

/* ------------- local protocols -------------------- */
/* callbacks - driven when ng_siwait noitces an event */
static int cb_tcpdata( void *data, int handle, char *buf, int len );	/* buffer received on a tcp session (handle) */
static int cb_udpdata( void *data, char *buf, int len, char *from, int fd );    /* buffer received via UDP */
static int cb_newsess( void *data, int handle, char *from );		/* new connection */
static int cb_disc( void *data, int handle );				/* session blew away */
static int cb_signal( void *data, int flags );				/* signal received */

/* general manipulation functions */
static void eat_msg( Session_t *s, char *msg );
static Session_t *get_session( int handle, char *name );
static void deref_session( int handle, char *name );
static void add_session( Session_t *s, int handle, char *name );
static void add2partial( char *partial, int plen, char *msg, int mlen );

static void seneschal_conn( );



/* initialise and register callbacks.  called directly when a thread is NOT being used to 
   deal with network traffic 
*/
void *port_init( char *port )
{
	extern 	int ng_sierrno;

	if( ng_siinit( SI_OPT_FG | SI_OPT_ALRM, atoi( port ), atoi( port ) ) != 0 )	/* initialise the network interface */
	{
		ng_bleat( 0, "port_listener: unable to initialise the network interface sierr=%d err=%d(%s)", ng_sierrno, errno, strerror( errno ) );
		exit( 1 );
	}

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
	int 	ok2run = 1;
	int	status;

	port_init( port );			/* establish listener */

	errno = 0;

	ng_bleat( 2, "port_listener: initialised and running" );
	while( ok2run )
	{
		errno = 0;
		if( (status = ng_sipoll( (gflags & GF_WORK_SUSP) ? 10 : 200 )) < 0 )	/* no more than a 2 second wait; .1s if work pend */
			if( errno != EINTR && errno != EAGAIN )
				ok2run = 0;

		if( ok2run )
		{
			if( ! have_seneschal )				
				seneschal_conn( );		
			else
			{
				ng_bleat( 4, "port_listener: invoking maint: seneschal-state=%d",  have_seneschal );
				gflags &= ~GF_WORK_SUSP;		/* rest suspended work flag */
				prog_maint( NULL, NULL );		/* do any maintenance on all programmes listed in symtab */
				have_seneschal = SESSION_OK;		/* reset from SESSION_RESET state if needed */
			}
		}
	}
	

	ng_bleat( 1, "port_listener: ok2run is off: sierr=%d err=%d(%s)", ng_sierrno, errno, strerror( errno ) );

	return NULL;
}

/* ------- callback routines driven at various times by ng_siwait( ) ---------------- */

/* deal with a signal */
/* DEPRECATED -- we poll rather than depending on the alarm */
static int cb_signal( void *data, int flags )
{

	ng_bleat( 4, "cb_signal: signal: %x(flags) seneschal-state=%d", flags, have_seneschal );

	if( flags & SI_SF_ALRM  )
	{
		if( ! have_seneschal )				/* alarm - need to try seneschal again? */
			seneschal_conn( );			/* connect, reset alarm if connect fails */
		else
		{
			ng_bleat( 4, "cb_signal: invoking maint: seneschal-state=%d", flags, have_seneschal );
			gflags &= ~GF_WORK_SUSP;		/* rest suspended work flag */
			prog_maint( NULL, NULL );		/* do any maintenance on all programmes listed in symtab */
			have_seneschal = SESSION_OK;		/* reset from SESSION_RESET state if needed */
		}
	}
	
}


/* called when we receve a connection request 
  	- create session block
	- register it by the handle - must wait for a message to register name
*/
static int cb_newsess( void *data, int handle, char *from )
{
	Session_t	*s;
	char	*buf;

	s = (Session_t *) ng_malloc( sizeof( Session_t ), "cb_newsess: session mgt block" );
	memset( s, 0, sizeof( Session_t ) );

	s->handle = handle;
	s->type = SESS_PGM;			/* anything inbound is a session */

	sfsprintf( s->hname, sizeof( s->hname ), "%d", handle );
	add_session( s, handle, NULL );				/* install pointer to session block in the symtab */

	ng_bleat( 1, "newsess: programme submission  session received from: %s (%d)", from, handle );

	sfsprintf( s->fname, sizeof( s->fname ), "/tmp/nawab_%d.pgm", handle );
	if( (s->of = ng_sfopen( NULL, s->fname, "w" )) == NULL )
	{
		ng_bleat( 0, "cb_newsess: open failed for: %s: %s", s->fname, strerror( errno ) );
	}

	return 0;
}

/* called when someone terminates a session 
	dereference the session block and free memory
*/
static int cb_disc( void *data, int handle )
{
	Session_t	*s;

	if( (s = get_session( handle, NULL )) )
	{
		if( s->type == SESS_SEN )			/* lost the seneschal session */
			have_seneschal = 0;			/* will cause an attempt next time the alarm pops */
		else						/* ensure programme file is closed up nicely */
		{
			if( s->of )		/* valid open file */
			{
				sfclose( s->of );		/* we dont care if there were errors as we are trashing the file */
				unlink( s->fname );
			}
		}
			
		ng_bleat( 1, "session disconnected: %d type=%s", handle, s->type == SESS_SEN ? "Seneschal" : "programme" );
		deref_session( handle, s->name );		/* trash both symtab references to this block */
		if( s->name )
			ng_free( s->name );
		ng_free( s );
	}
	else
		ng_bleat( 0, "disc: could not find session for handle: %d", handle );

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

		ng_bleat( 3, "cb_tcpdata: tcp buffer contained %d messages %s", mcount, s->partial[0] ? "incomplete" : "complete" );
	}
	else
		ng_bleat( 0, "cb_tcpdata: message received for non-existant session: %d(sid)", handle );


	return 0;
}


/* 
   UDP buffers contain commands to act on.

*/
static int cb_udpdata( void *data, char *buf, int len, char *from, int handle )
{
	extern 	int	ng_sierrno;
	extern	char	*version;

	int	stat;
	int	rc = SI_RET_OK;			/* return value - set to SI_RET_QUIT when we want to shut off */
	int	oldv;				/* holds on to verbose while we change it to our liking for a bit */
	char	*strtok_p;			/* cache for strtok_r */
	char	*tok;				/* for parsing stuff */
	char	*msg;				/* pointer to the message to use (may become a copy) */
	char	wbuf[1024];			/* work buffer for messages */
	

	ng_bleat( 3, "udpdata: got %d bytes", len );		

	if( buf[len-1] < ' ' )		/* assume newline or somesuch terminator */
	{
		buf[len-1] = 0;			/* make a string and just point msg at it - faster */
		msg = buf;
	}
	else					/* copy - cannot guarentee that buf[len+1] is inside the buffer */
	{
		if( len >= NG_BUFFER )
		{
			ng_bleat( 0, "udpdata: buffer overrun from: %s (%.100s)", from, buf );
			return 1;
		}

		memcpy( wbuf, buf, len );
		wbuf[len] = 0;

		msg = wbuf;
	}

	ng_bleat( 3, "udpdata: got (%s)", msg );
	tok = strtok_r( msg, " ", &strtok_p );		/* first non blank */

	switch( *tok )			/* ordered by frequency expected (mostly) */
	{
			case '.':	break;					/* end of messages from seneschal */

			case 'd':						/* dump */
				ng_bleat_setudp( handle, from );
				uif_dump( strtok_r( NULL, "", &strtok_p ) );
				ng_sisendu( from, ".", 2 );
				ng_bleat_setudp( 0, NULL );
				break;

			case 'e':
				if( *(tok+2) == 'p' )				/* explain */
				{
					ng_bleat_setudp( handle, from );
					uif_explain( strtok_r( NULL, "", &strtok_p ) );
					siesta( 2 );				
					ng_sisendu( from, ".", 2 );
					ng_bleat_setudp( 0, NULL );
					break;
				}

				if( *(tok+2) == 'i' && *(tok+3) == 't'  )				
				{
					ng_bleat( 0, "received exit; shutting network down" );
					rc = SI_RET_QUIT;		/* cause network interface to shut off */
				}

				ng_sisendu( from, ".", 2 );			/* send end of data to requestor */
				break;

			case 'p':				/* purge */
				ng_bleat_setudp( handle, from );
				oldv = verbose;
				verbose = verbose ? verbose : 1;		/* ensure verbose so stuff sent to user from purge */

				uif_purge( strtok_r( NULL, "", &strtok_p ) );
				ng_sisendu( from, ".", 2 );
				ng_bleat_setudp( 0, NULL );
				verbose = oldv;				/* reset */
				break;

			case 'r':						/* resubmit */
				ng_bleat_setudp( handle, from );
				uif_resubmit( strtok_r( NULL, "", &strtok_p ) );
				ng_sisendu( from, ".", 2 );
				ng_bleat_setudp( 0, NULL );
				break;

			case 'v':
				tok = strtok_r( NULL, " ", &strtok_p );
				if( (verbose = atoi( tok )) < 0 )
					verbose = 0; 
				ng_bleat_setudp( handle, from );			/* send to user's udp from bleat */
				ng_bleat( 0, "version=%s; verbose level changed to: %d", version, verbose );	/* msg to user */
				ng_bleat_setudp( 0, NULL );
				ng_sisendu( from, ".", 2 );				/* reset bleats to log file */

				ng_bleat( 0, "version=%s; verbose level changed to: %d", version, verbose );	/* show in log too */
				break;
	

			default:	
				ng_bleat( 0, "udpdata: error: unrecognised message from: %s (%s)", from, tok );
				sfsprintf( wbuf, sizeof( wbuf ), "nawab: error: unrecognised command: %s\n", tok );
				ng_sisendu( from, wbuf, strlen( wbuf ) );
				ng_sisendu( from, ".", 2 );			/* this must be in a buffer by itself! */
				break;
	}

	return rc;
}

/* ----------------- general support routines -------------------------- */

/*	attempt session with seneshal. if it fails, set an alarm to drive a retry 
	nawab assumes that both it and seneschal are running on the same host.
	this does not have to be, but for now the leader runs both.
*/
static void seneschal_conn( )
{
	Session_t *s;			/* session block */

	int	fd;			/* session file descritor if successful */
	char	thishost[100];
	char	*sene_host;		/* host seneschal is living on */

	if( seneschal_port )
		ng_free( seneschal_port );

	if( (seneschal_port = ng_env( "NG_SENESCHAL_PORT" )) == NULL )		/* get his well known port */
	{
		ng_bleat( 0, "seneschal_conn: abort: cannot find seneschal port (NG_SENESCHAL_PORT) in environment/cartulary" );
		exit( 1 );
	}

	if( (sene_host = ng_env( "srv_Jobber" )) == NULL )		/* we get each time we need to connect allowing sene to drop and move */
	{
		ng_sysname( thishost, sizeof( thishost ) - 1 );
		sene_host = thishost;
	}

	sfsprintf( seneschal_address, sizeof( seneschal_address ), "%s:%s", sene_host, seneschal_port );
	if( (fd = ng_siconnect( seneschal_address )) >= 0 )
	{
		ng_bleat( 1, "seneschal_conn: session established with seneschal (%s) on fd %d", seneschal_address, fd );
		s = (Session_t *) ng_malloc( sizeof( Session_t ), "cb_newsess: session mgt block" );
		memset( s, 0, sizeof( Session_t ) );

		s->handle = fd;
		s->type = SESS_SEN;			/* mark as a seneschal session */

		sfsprintf( s->hname, sizeof( s->hname ), "%d", fd );
		add_session( s, fd, "seneschal" );				/* install pointer to session block in the symtab */
		have_seneschal = SESSION_RESET;			/* need to send jobs to seneschal */
		ng_sisendt( fd, "name=nawab\n", 12 );		/* register so we get notifications */
	}
	else
	{
		ng_bleat( 4, "seneschal_conn: connetion failed: %s:  %s", seneschal_address, strerror( errno ) );
	}

	ng_free( sene_host );
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
        otherwise we expect that its a programme statement coming from n_req or some	
	other source of nawab programmes. we queue the programme line until we see the 
	end and then parse it at that point. the end of the programme is marked wit 
	a record that begins with "=end" and it is not passed as a part of the 
	yyparsed grammar. 
*/
static void eat_msg( Session_t *s, char *msg )
{
	extern int yyerrors;
	extern	char *yyerrmsg;

	Programme 	*p;
	char	emsg[255];

	ng_bleat( 3, "eat_message: (%s)", msg );
	switch( s->type )
	{
		case SESS_SEN:				/* seneschal status message */
			ng_bleat( 2, "eat_msg: status: (%s)", msg );
			if( *msg )
				parse_status( msg, NORMAL_MODE );		/* deal with a job completion */
			break;

		case SESS_PGM:					/* programme input */
			if( strncmp( msg, "=end", 4 ) == 0  )	/* data after =end (user:node) is passed to parse prog */
			{
				if( s->of )			/* valid open file */
				{
					ng_bleat( 1, "eat_msg: end of programme rcvd on fd %d %s", s->handle, msg );
					set_sbleat( s->handle );		/* allow sbleat calls to send to this partner */

					if( sfclose( s->of ) == 0 )
						p = parse_pgm( s->fname, msg+4 );		/* parse it - sets yyerrors; creates programme */
					else
					{
						sbleat( 0, "ERROR: write errors detected while saving programme (%s); programme scratched: %s", s->fname, strerror( errno ) );
						p = NULL;
					}

					if( p == NULL || yyerrors )
					{
						sfsprintf( emsg, sizeof( emsg ), "%d error(s) parsing programme.\n", yyerrors );
						ng_sisendt( s->handle, emsg, strlen( emsg ) );
						if( yyerrmsg );
							ng_free( yyerrmsg );	
						yyerrmsg = NULL;
					}
					else
					{
						sfsprintf( emsg, sizeof( emsg ), "0 errors: programme parsed OK\n" );
						ng_sisendt( s->handle, emsg, strlen( emsg ) );
					}
					set_sbleat( -1 );		/* turn off */

				}
				else
				{
					sfsprintf( emsg, sizeof( emsg ), "1 error(s) recording/opening program\n" );
					ng_sisendt( s->handle, emsg, strlen( emsg ) +1 );
				}

				unlink( s->fname );
				s->of = NULL;
				ng_siprintf( s->handle, "~done\n" );		/* term msg expected by default by ng_sendtcp */
			}
			else			/* assume another record of the programme */
			{
				ng_bleat( 3, "eat_message: saving programme record: (%s)", msg );
				sfprintf( s->of, "%s\n", msg );
			}
			break;

		default:	
			ng_bleat( 0, "eat_message: unknown session type: %d", s->type );
			break;
	}
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
	Session_t *s;
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
	Session_t	*s = NULL;
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

/*	simple interface for the rest of nawab */
int udp2seneschal( char *buf )
{
	if( have_seneschal && buf && *buf )
		ng_sisendu( seneschal_address, buf, strlen( buf ) );
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
			{
				ng_bleat( 0, "send2senesschal: error sending buffer: ng_sisendt() returned %d", status );
				ng_siclose( s->handle );
				have_seneschal = SESSION_BAD;
			}
		}
		else
		{
			ng_bleat( 0, "send2seneschal: cannot find seneschal session block" );
			have_seneschal = SESSION_BAD;
		}
	}

	return rc;
	
}
