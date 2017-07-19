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
 ----------------------------------------------------------------------------------------
  Mnemonic:	parrot
  Abstract: 	Repeats what it hears to all that are listening... and then some!

		Initial design was to receive registrations from listeners and forward all 
		messages to each listener.  Listeners were required to register every so 
		often in order to remain current.  All of this traffic is UDP.

		Extensions...
		Accept and execute commands on behalf of a remote process. Initially
		these were commands from zookeeper user interface proceses (W1KW), but the ng_rcmd
		interface was created to allow an interface outside of the W1KW environment. 
		The rcmd interface is TCP/IP based sessions where the W1KW interface is UDP/IP.

		Parrot will accept a TCP/IP session and will then act as a ng_log broadcast 
		agent. All messages received on the session are broadcast on the local network.
		This allows satellite nodes to broadcast their masterlog messages even when they
		are remote from the main cluster. When this mode is entered, a child process is 
		forked to do the work.

		Once a TCP/IP session has been accepted, the session can be put into a multicast
		agent state.  When this state is entered, parrot will write messages received to 
		the multicast group, and will forward all broadcast and multicast messages that
		it hears back to the session partner. When this mode is entered, a child process
		is forked to do the work.

		Specific target UDP message forwarding.  In order to allow processes running
		on satellite nodes to respond to UDP requests from processes behind a firewall
		and not using well-known ports, parrot now accepts messages and will forward
		them to a named address:port combination.  Initally to support status queries
		sent by ng_sendudp to ng_narbalek, but there will likely be other needs for this.
		
  Date:		13 April 2001
  Author: 	E. Scott Daniels
  Revised:	26 Feb 2003 (sd) - to suport TCP sessions for rcmds as UDP aint cutting it.
		21 May 2003 (sd) - finshed tcp stuff, allowing tcp session to send
			multiple commands.  repeater still works only off of udp at the moment.

		25 Oct 2004 (sd) - added multicast agent support for remote 
			narbaleks.  A narbalek connects and sends us an ~MCAST port group
			message in causing us to fork a child to do the agent work.
			All messages received from the partner are sent to the mcast group
			as received. We require that all messages be newline terminated, 
			and assume that the processes participating in the group want the 
			same.
			Messages that are received via UDP (either as mcast or direct
			responses to mcast messages we forwarded) are returned to the 
			session partner.  The sender's IP address is prefixed onto the
			message such that it has the format of address:data\n.  The 
			partner should be able to treat the message as though it was 
			received via udp from the address. 
 ----------------------------------------------------------------------------------------
Revsion thoughts:
	the original kept a binary 'address' to eliminate the overhead of 
	converting the addres for each send.  the si lib does not provide
	for that -- it would be nice to extend it so we can again use the 
	binary addr in the ear.
*/

#define _USE_BSD
#include <sys/types.h>          /* various system files - types */
#include <sys/ioctl.h>
#include <sys/socket.h>         /* socket defs */
#include <net/if.h>
#include <stdio.h>              /* standard io */
#include <errno.h>              /* error number constants */
#include <fcntl.h>              /* file control */
#include <netinet/in.h>         /* inter networking stuff */
#include <signal.h>             /* info for signal stuff */
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>	/* needed for wait */
#include <sys/wait.h>


#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_ext.h>
#include	<ng_socket.h>

#include	"parrot.man"

/* prototypes ------------------ */
void spew( char *buf, int len );		/* send message to all registered listeners */

#define	FL_HIDDEN	0x01	/* we have already hidden ourselves away */
#define FL_HEARTBEAT	0x02	/* send heartbeat to all listeners */
#define FL_PARENT	0x04	/* process is parent */
#define FL_LOGGER	0x08	/* we were put into log relay state */
#define FL_MCASTER	0x10	/* we were put into multi-cast agent mode */
#define FL_GENERAL	0x20	/* general agent */
#define FL_AGENT	(FL_MCASTER + FL_GENERAL + FL_LOGGER) 	/* agent of somekind */

#define PF_CONN		0x01	/* partner has an active connection */
#define PF_SEND_NULL	0x02	/* send the end of string null */
#define PF_LEGIT	0x04	/* tcp sessions that are marked legit will have any messages they send spewed to all listeners */


struct	ears {				/* info we need on a listener */
	struct ears *next;	
	struct ears *prev;		/* chain */
	struct sockaddr_in *addr;	/* ng_netif "binary" address */
	char 	*id;			/* how we will know them when they reregister */
	ng_timetype expire;		/* incase they dont un-register we will knock them off */
	ng_timetype conn;		/* time of first register */
	unsigned long	xmits;		/* count of packets sent */
	int	prompt;			/* flag to prompt them to re-register */
};


/* partners establish tcp/ip sessions */
struct partner {
	struct	partner *next;
	struct  partner *prev;
	int	fd;			/* si handle to send */
	int	flags;			/* PF_ constatns */
	char	*addr;			/* ip address of parnter -- needed when acting as a mcaster agent */
};

char	*argv0 = NULL;
int	verbose = 0;

static int	send_beat = 0;			/* set to indicate that alarm popped and we need to send beat */
static int	cmd_id = 0;			/* keeps command stdout/err files unique */
static char	cmd_ch = '~';			/* all command buffers start with this bent character */
static char	esc_ch = '\\';			/* allows an escape to be sent as the lead character w/o triggering our command processor */
static int	nfd = -1;               	/* udp port file des */
static int	flags = FL_PARENT;
static int	send_err = 0;			/* set if pipe fail called to try to prevent sending to dead processes */
static int	ex_time = 300;			/* seconds afterwhich a listener is ejected */
static int	heartbeat = 60; 		/* heartbeat every n seconds (-h seconds) */
static int	mypid = 0;			/* for messages */
static int	ulistensid = -1;			/* so baby processes can close the parent's listen port */
static int	tlistensid = -1;			/* so baby processes can close the parent's listen port */

static char	*workd = NULL;			/* work directory */
static char	*mcast_ip = NULL;		/* full ip address (group:port) for sendu calls */
static char	*mcast_id = NULL;		/* id string that we ignore -- our broadcasts that come back to us */

static struct ears	*audience = NULL;	/* registered processes who want broadcast messages */
static struct partner	*strife = NULL;		/* list of trouble */

static Sfio_t	*bleatf = NULL;			/* file for bleats if running as daemon */
static char	*version = "v4.6/06047";


void usage( )
{
	sfprintf( sfstderr, "ng_parrot %s\n", version );
	sfprintf( sfstderr, "usage: %s [--man] [-e seconds] [-f] [-h] [-p port | -s service] [-v]\n", argv0 );
	exit( 1 );
}


/* signal handler - alarms and dead kids */
void sigh( int s )
{
	switch( s )
	{
		case SIGPIPE:
			send_err++;
			break;

		case SIGCHLD:   
		/*	while( wait4( 0, NULL, WNOHANG, NULL ) > 0 );	*/		/* wait for all dead kids -- breaks sgi */
			while( waitpid( 0, NULL, WNOHANG ) > 0 );		/* ports to sgi */
			signal( SIGCHLD, &sigh ); 			/* must reissue on irix */
			break;	

		case SIGALRM:
			if( heartbeat )
			{
				send_beat = 0;
				if( flags & FL_HEARTBEAT )		/* heartbeat too? */
					spew( "~HEARTBEAT", 11 );	/* and the beat goes on... */
				alarm( heartbeat ); 
			}
			break;

		default: 
			ng_bleat( 1, "unknown signal: %d", s );
			break;
	}

}

/* escape target characters in old with esym; returns pointer to a new buf, leaves old unharmed 
	if we find <escape><target> then we leave it alone 
*/
char *escape_buffer( char *old, char target, char esym )
{
	char new[NG_BUFFER];
	int oldi = 0;		/* index into old and new */
	int newi = 0;

	for( ; newi < NG_BUFFER-2 && *(old+oldi); oldi++ )
	{
		if( *(old+oldi) == esym )		/* if /"  then keep /" dont turn into //" */
			*(new+newi++) = *(old+oldi++);
		else	
			if( *(old+oldi) == target )
				*(new+newi++) = esym;
		*(new+newi++) = *(old+oldi);
	}

	*(new+newi) = 0;

	return ng_strdup( new );
}

/* daemons should neither be seen nor heard */
void hide( )
{
	int i;
	char	buf[256];
	char	*ng_root;

	if( flags & FL_HIDDEN )
		return;            /* bad things happen if we try this again */

	switch( fork( ) )
	{
		case 0: 	break;        /* child continues on its mary way */
		case -1: 	perror( "could not fork" ); return; break;
		default:	exit( 0 );    /* parent abandons the child */
	}

	mypid = getpid();

	for( i = 0; i < 40; i++ )
		if( i != nfd )
			close( i );              /* dont leave caller's files open */

	ng_log_reinit( );

	if( ! (ng_root = ng_env( "NG_ROOT" )) )
        {
                ng_bleat( 0, "cannot find NG_ROOT in cartulary/environment" );
                exit( 1 );
        }

	sfsprintf( buf, sizeof( buf ), "%s/site/log/parrot", ng_root );
	if( (bleatf = ng_sfopen( NULL, buf, "a" )) != NULL )
	{
		sfset( bleatf, SF_LINE, 1 );		/* force flushes after each line */
		ng_bleat_setf( bleatf );	
		ng_bleat( 1, "hide; tucked away now" );
	}
	else
	{
		ng_log( LOG_ERR, "unable to open bleat file: %s: %s", buf, strerror( errno ) );
		exit( 1 );
	}

	flags |= FL_HIDDEN;
	free( ng_root );
}

void drop_partner( struct partner *p )
{
	if( p )
	{
		ng_bleat( 2, "drop_partner (%d): dropping %d(fd) %02x(flags)", mypid, p->fd, flags );
		if( p->flags & PF_CONN )
			ng_siclose( p->fd );		/* disconnect first */

		if( p->next )
			p->next->prev = p->prev;
		if( ! p->prev )
			strife = p->next;
		else
			p->prev->next = p->next;

		if( p->addr )
			free( p->addr );
		free( p );
	}
}

struct ears *find( char *who )
{
	struct ears *ep;

	for( ep = audience; ep && strcmp( who, ep->id ) != 0; ep = ep->next );

	return ep;
}

/* send a message - if partner then send via tcp session, else send via udp 
	returns non zero if error perceived
*/
int send_msg( struct ears *ep, char *who, char *buf, struct partner *p )
{
	int sstat = 0;
	int send_null = 1;		/* send the end of string null too */


	if( p && !(p->flags & PF_SEND_NULL) )
		send_null = 0;

	if( p )
	{
		if( (p->flags & PF_CONN) == 0  )			/* no connection */
			return 1;

		send_err = 0;
		sstat = ng_sisendt( p->fd, buf, strlen( buf ) + send_null );
		if( send_err  || sstat < 0 )					/* pipe fail */
		{
			ng_bleat( 1, "send_msg(%d): session broken fd=%d", mypid, p->fd );
			p->flags &= ~PF_CONN;			/* turn off connection flag -- no more sends */
			return 1;
		}
	}
	else
	{
		if( who )
		{
			ng_sisendu(  ep ? ep->id : who, buf, strlen( buf )+1 );    /* send them the buffer */
		}
	}
	return 0;
}

void spitfile( char *fname, char *tag, struct ears *ep, char *who, struct partner *p, int send_nl )
{
	Sfio_t	*f;
	char 	*buf;		/* buf from the file */
	char	msg[4096];	/* constructed message to UDP to whomever */
	char	*te;		/* tag end in msg */
	int	nsent = 0;	/* number sent; remember this is udp, we must go slower */
	int	sstatus = 0;

	if( (f = ng_sfopen( NULL, fname, "r" )) != NULL )
	{
		sprintf( msg, "%s ", tag );	/* tag is the ~STDERR kind of thing prefixed to each */
		te = msg + strlen( msg );
		
		while( sstatus == 0 && (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			sfsprintf( te, 4000, "%s%c", buf, send_nl ? '\n' : 0x00 );
#ifdef OLD
			*te = 0;				/* makes faster, tag is constant */
			strncat( msg, buf, 4000 );
#endif
			sstatus = send_msg( ep, who, msg, p );

			if( !p && ++nsent > 100 )	/* pause only if spewing on udp */
			{
				sleep( 1 );
				nsent = 0;
			}
		}

		if( sfclose( f ) )
			ng_bleat( 1, "read error: %s: %s", fname, strerror( errno ) );
	}
	else
		ng_bleat( 0, "open failed: %s; %s", fname, strerror( errno ) );
}

/* make a child process if not a child already.  parent drops the session partner
   and child discards the other sessions

	returns 1 if the process is the child; < 1 to indicate the process is the parent
	or error.
*/
int mkbaby( struct partner *p, int need_udp )
{
	struct partner *q;		/* if we become the child, we drop all other parners */
	struct partner *nextq;		/* pointer at next partner to drop */
	char	buf[1024];

	if( ! (flags & FL_PARENT) )	/* if we are a child, dont need to fork if we got another task command */
		return 1;

	switch( fork( ) )
	{
		case 0: 	
			flags &= ~FL_PARENT;		/* we be child now */
			break;        			/* child continues on its mary way */

		case -1: 				/* something not right */
			ng_bleat( 0,  "mkbaby (%d): could not fork child;", mypid ); 

			if( p )
			{
				sfsprintf( buf, sizeof( buf ),  "~STDERR remote parrot could not fork process for command: %s", strerror( errno ) );	
				send_msg( NULL, NULL, buf, p );		
				sfsprintf( buf, sizeof( buf ), "~EC 1"  );	/* nothing else to come, send error end */
				send_msg( NULL, NULL, buf, p );			
				/* we do NOT drop the session */
			}
			return -1;

		default:				/* we are the parental unit */
			if( p )
			{
				drop_partner( p );	/* free the storage in this process, child manages everything */
			}
			return 0; 		      /* parent - continue to do parental things */
	}

	
	alarm( 0 );				/* we dont want alarms as a baby process */
	mypid = getpid( );
	for( q = strife; q; q = nextq )
	{
		nextq = q->next;
		if( q != p )
			drop_partner( q );
	}
	
	if( p && strife != p )			/* it should, but why risk it */
		strife = p;

	if( tlistensid >= 0 )			/* must be closed after all other networking adjustment has been accomplished */
	{
		int x;
		ng_bleat( 2, "mkbaby (%d): closing tcp listen port (sid=%d)", mypid, tlistensid );
		ng_siclose( tlistensid );	/* we dont want to accept new sessions from child */
		ng_siclose( -1 );		/* close parent's udp too */
		if( need_udp )
		{
		 	x = 	ng_siopen( 0 );		/* must have in child command proc incase we need to send output via udp */
			ng_bleat( 2, "mkbaby (%d): new udp fd opened: %d", mypid, x );
		}
	}

	return 1;
}

/*
 ------------------------------------------------------------------------------
 Run a user task.  We must be careful as we have closed standard out and 
 stderr.  We run as a forked process so that we can  continue to listen and hurl
 messages and are not affected by the length of the command.
 ------------------------------------------------------------------------------
*/
void task( char *who, char *buf, struct partner *p )
{
	struct	ears *ep = NULL;
	char	cmd[NG_BUFFER];
	int	i;
	int	err = 0;		/* send back stderr from command */
	int	out = 0; 		/* sent back stdout from command if true */
	char	stdoutf[1000];       	/* file name for std out */
	char	stderrf[1000];       	/* file name for stderr */
	char	*ebuf;			/* buffer after quotes have been escaped */
	int	myid;
	int	status;
	int	send_nl = 0;		/* send output back with newline terminated buffers rather than null terminated buffers */
	int	closeatend = 0;		/* session closed after end of first command if set */

	myid = cmd_id++;		/* for unique name in /tmp */

	ng_bleat( 2, "task (%d): started work on command from  %s (%s)", mypid, who ? who : "tcp session", buf );
	if( !p )
		ep = find( who );	/* must find ears if submitted by udp */

	if( (status = mkbaby( p, 1 )) < 1 )	/* make a baby, close parents tcp/udp listen ports and open udp for us */
		return;				/* parent or error; get out now (if it failed, mkbaby sends error back) */


	for( i = 0; *(buf+i) &&  *(buf+i) != '-' && !isspace( *(buf+i) ); i++ );

	sfsprintf( stdoutf, sizeof( stdoutf ), "/dev/null" );		/* default to nil */
	sfsprintf( stderrf, sizeof( stdoutf ), "/dev/null" );

 	if( *(buf+i) == '-' )
	{
		for( i++; *(buf+i) && !isspace( *(buf+i) ); i++ )
			switch( *(buf+i) )
			{
				case 'O': 
					ng_bleat( 1, "task (%d): generating output for command", mypid );
					out++; 		/* user wants output */
					sfsprintf( stdoutf, sizeof( stdoutf ), "%s/parrot_out.%d.%d", workd, myid, getpid( ) );
					break;

				case 'M': 
					out++; 		/* user wants output and error mixed */
					sfsprintf( stdoutf, sizeof( stdoutf ), "%s/parrot_out.%d.%d", workd, myid, getpid( ) );
					sfsprintf( stderrf, sizeof( stdoutf ), "&1", workd, myid, getpid( ) );
					break;

				case 'N':
					send_nl = 1;		/* send newline terminated records */
					if( p )
						p->flags &= ~PF_SEND_NULL;	/* turn off sending the end of string marker */
					break;

				case 'C':
					closeatend = 1;		/* close session at end */
					break;

				case 'E': 
					err++; 		/* user wants stderr too */
					sfsprintf( stderrf, sizeof( stderrf ), "%s/parrot_err.%d.%d", workd, myid, getpid( ) );
					break;

				case 'D': 			/* user wants nothing -- detached session */
					ng_bleat( 1, "task (%d): command to run detached; session to requestor will close", mypid );
					sfsprintf( cmd, sizeof( cmd ), "~EC 0"  );	/* nothing else to come, send clean end */
					send_msg( ep, who, cmd, p );		/* send back return code */
					drop_partner( p );			/* close session back to rcmd */
					p = NULL;
					who = NULL;
					ep = NULL;
					break;
			}
	}	

	if( !p && !ep && !who )			/* if we have nobody to send to, then dont even try */
	{
		out = 0;
		err = 0;
	}

	for( ; *(buf+i) && !isspace( *(buf+i) ); i++ );
	ng_bleat( 2, "task (%d): found cmd (%s) =%d (%s)", mypid, buf, i, buf+i );

	if( *(buf+i) && *(buf+i+1) )		/* something (like the command) remained in the buffer */
	{
		i++;

		if( p || ep || who )
		{
			sprintf( cmd, "~PID %d%c", mypid, send_nl ? '\n' : (char) 0 );	/* let them know that we are spinning on their stuff */
			send_msg( ep, who, cmd, p );			
		}

		
		ebuf = escape_buffer( buf+i, '"', '\\' );	/* must escape users " marks as we enclose the cmd in them */
		/*sfsprintf( cmd, sizeof( cmd ), "ksh \"(%s)\" >%s 2>%s", ebuf, stdoutf, stderrf ); this has issues with large command strings! */
		sfsprintf( cmd, sizeof( cmd ), "ksh -c \"%s\" >%s 2>%s", ebuf, stdoutf, stderrf );
		ng_free( ebuf );
		ng_bleat( 1, "task(%d): running command: %s", mypid, cmd );

		status = system( cmd );		/* do it! */
		ng_bleat( 1, "task(%d): command status: %d out=%d err=%d", mypid, status, out, err );
	
		if( p || ep || who )
		{
			if( out )
				spitfile( stdoutf, "~STDOUT", ep, who, p, send_nl );

			if( err )
				spitfile( stderrf, "~STDERR", ep, who, p, send_nl );
			ng_bleat( 2, "task(%d): stdout/err sent", mypid );

			sprintf( cmd, "~EC %d", status >> 8  );	/* we guarentee that the EC is the LAST thing coming! */
			send_msg( ep, who, cmd, p );		/* send back return code */
			ng_bleat( 2, "task(%d): exit code sent; unlinking: %s %s", mypid, stderrf, stdoutf );
		}
		
		if( out  &&  unlink( stderrf ) < 0 )
			ng_bleat( 0, "task (%d): unlink of stderr file (%s) failed: %d %s", mypid, stderrf, errno, strerror( errno ) );
		if( err  && unlink( stdoutf ) < 0 )
			ng_bleat( 0, "task (%d): unlink of stdout file (%s) failed: %d %s", mypid, stderrf, errno, strerror( errno ) );
	}

	if( !p )			/* command came in via udp, so we just exit here */
		exit( 0 );

	if( closeatend )
	{
		ng_siclose( p->fd );
		exit( 0 );
	}

	p->flags |= PF_SEND_NULL;		/* if it was turned off, make sure it is back on */
		

	/*
		Otherwise...
		we return to siwait (eventually) and exit when the remote site exits allowing us to run another
		command if they send it
	*/
}


void seat( char *who, int prompt )
{
	struct ears *ep;

	if( ! (ep = find( who )) )
	{
		ep = (struct ears *) ng_malloc( sizeof( struct ears ), "getting an ear" );
		ep->next = audience;
		if( ep->next )
			ep->next->prev = ep;
		ep->prev = NULL;
		ep->id = strdup( who );
		ep->addr = ng_dot2addr( who );
		ep->conn = ng_now( );
		ep->xmits = 0;
		audience = ep;

		ng_bleat( 1, "seat (%d): %s is now listening", mypid, audience->id );
	}
	else
		ng_bleat( 1, "seat (%d): %s was reseated", mypid, audience->id );
			

	ep->expire = ng_now( ) + (ex_time * 10);  /* how long to keep them - tenths of seconds */
	ep->prompt = prompt;                 /* need to reset every time because we turn it off when we prompt */

	ng_sisendu( ep->id, "~HI", 4 );
	/*ng_write_a_udp( nfd, ep->addr, "~HI", 4 ); */
}

/* throw them out based either on name or a direct pointer */
void eject( char *who, struct ears *ep, int notify )
{
	if( ep || (ep = find( who )) )
	{
		if( notify )
			ng_sisendu(  ep->id, "~DOWN", 6 );
			/*ng_write_a_udp( nfd, ep->addr, "~DOWN", 6 ); */

		if( ep->next )
			ep->next->prev = ep->prev;
		if( ep->prev )
			ep->prev->next = ep->next;
		else
			audience = ep->next;

		ng_bleat( 1, "eject (%d): %s has gone home", mypid, ep->id );

		free( ep->addr ); 
		free( ep->id );
		free( ep );
	}
	else
		if( verbose )
			sfprintf( sfstderr, "eject cannot find %s\n", who );
}

void vamoose( char *buf )
{
	struct ears	*ep;

	if( strncmp( buf+2, "4360", 4 ) == 0 )    /* dont let just any bloke shut us off! */
	{
		ng_bleat( 1, "vamoose (%d): show over; audience was %s", mypid, audience ? "full" : "empty" );


		while( audience )
			eject( NULL, audience, 1 );

		while( strife )
			drop_partner( strife );

		ng_closeudp( nfd );

		exit( 0 );
	}
}

/* 	Called when ~LOG received to set us up as a log message repeater. If we are not already a 
	child process we become one first as reboradcasting could over tax the parent 
	process -- we expect to get many log messages from a log sender We do not do any 
	validation on the message. Once established as a log msg repeater, all non-command 
	messages received via tcp are rebroadcast on the log 'frequency' using ng_log_bcast().
	The child process created will remain a log repeater until the session is lost.
*/
void relog( struct partner *p, char *msg )
{
	switch( mkbaby( p, 0 ) )		/* make baby and close parents tcp/udp listen ports */
	{
		case 0:		/* we are the parent -- nothing left to do */
			return;

		case 1:
			ng_bleat( 1, "relog (%d): child process created to rebroadcast log messages", mypid  );
			break;

		default:
			ng_bleat( 0, "relog (%d): unable to make baby process for log rebroadcast", mypid );
			drop_partner( p );
			return;
	}

	flags |= FL_LOGGER;		/* indicates that all non-command messages are broadcast as preformatted log messages */
}

/*	called to put ourself into mcast-agent mode where we will transmit anything received from 
 	partner to the group, and anything received on UDP will be returned to the partner in the 
	form of sender-address:data.   We assume all messages in each direction are newline terminated. 
*/
void start_mcast_agent( struct partner *p, char *msg )
{
	int	fd = -1;
	int	port;
	int	status;
	char	*mcast_group = NULL;

	switch( mkbaby( p, 0 ) )		/* make baby and close parents tcp/udp listen ports */
	{
		case 0:				/* we are the parent -- nothing left to do */
			return;

		case 1:
			ng_bleat( 1, "start_mcast (%d): child process created for multi-cast agent processing  pid=%d", getpid() );
			break;

		default:
			ng_bleat( 0, "start_mcast (%d): unable to make baby process for log mcasting", mypid );
			drop_partner( p );
			return;
	}

	flags |= FL_MCASTER;		/* flag for raw callback to act upon */
	port = atoi( msg );		/* assume msg is port<space>group-id */
	mcast_group = strchr( msg, ' ' ) + 1;
	
	if( ! mcast_group || !port )
	{
		ng_bleat( 0, "start_mcast: (%d): abort: bad message port=%d mcast_group=%s", mypid, port, mcast_group );
		exit( 1 );
	}

	ng_sireuse( 0, 1 );		/* turn on reuse flag on new UDP ports */
	if( (fd = ng_siopen( port )) < 0 )
	{
		ng_bleat( 0, "start_mcast: (%d): unable to open UDP port: %s", mypid, strerror( errno ) );
		exit( 1 );
	}
	ng_bleat( 2, "start_mcast: (%d): UDP port opened successfully: %d; joining: (%s)", mypid, port, mcast_group );

	mcast_ip = ng_malloc( 100, "mcast_agent: mcast_ip buffer" );
	sfsprintf( mcast_ip, 100, "%s:%d", mcast_group, port );

        status = ng_simcast_join( mcast_group, -1, 1 );                       /* join the  mcast group, on any interface */
	ng_bleat( 1, "start_mcast (%d): joined multicast group: %s status=%d errno=%d", mypid, mcast_group, status, errno );


	ng_sireuse( 0, 0 );				/* turn off reuse  */
	if( (fd = ng_siopen( 0 )) < 0 )			/* get a generic port for writing -- dont want messages coming from group port */
	{						/* ng_sisendu() defaults to sending using the last udp fd opened */
		ng_bleat( 0, "start_mcast: (%d): unable to open  generic UDP port: %s", mypid, strerror( errno ) );
		exit( 1 );
	}

}

/*	called to put ourself into general-agent mode like mcast-agent mode except that we do not 
	forward messages unless they arrive with the ~FORWARD command. we do return anything received.
	we assume that all messages are newline terminated.
*/
void start_general_agent( struct partner *p )
{
	int	fd = -1;		

	switch( mkbaby( p, 0 ) )		/* make baby and close parents tcp/udp listen ports */
	{
		case 0:				/* we are the parent -- nothing left to do */
			return;

		case 1:
			ng_bleat( 1, "start_general (%d): child process created for general agent processing", getpid() );
			break;

		default:
			ng_bleat( 0, "start_general (%d): unable to make baby process for udp messages", mypid );
			drop_partner( p );
			return;
	}

	flags |= FL_GENERAL;		/* flag for raw callback to act upon */
	
	if( (fd = ng_siopen( 0 )) < 0 )		/* get a generic port for writing -- dont want messages coming from group port */
	{					/* ng_sisendu() defaults to sending using the last udp fd opened, so we dont track */
		ng_bleat( 0, "start_general: (%d): unable to open  generic UDP port: %s", mypid, strerror( errno ) );
		exit( 1 );
	}

}

/* 	forward a message. expect the buffer to be:
	~F[ORWARD]<space>ip-address:port<space>message
*/
void forward_msg( char *buf )
{
	char *ap;		/* pointer to address */
	char *mp;		/* pointer to message */

	if( (ap = strchr( buf, ' ' )) )
	{
		ap++;
		if( (mp = strchr( ap, ' ' )) )
		{
			*mp = 0;
			mp++;
ng_bleat( 1, "forward: a=%s m=%s", ap, mp );
			ng_sisendu( ap, mp, strlen( mp ) );
		}
	}
}

/* dump stats of everybody to who */
void dump( char *who )
{
	struct ears *ep = NULL;
	char	buf[1024];
	ng_timetype now;

	now = ng_now( );

	ng_bleat( 2, "processing dump for: %s", who );
	for( ep = audience; ep; ep = ep->next )
	{
		sprintf( buf, "~DUMP:parrot: %s(id) %ld(conn-secs) %lu(packets)", ep->id, (now - ep->conn)/10, ep->xmits );
		ng_sisendu( who, buf, strlen( buf )+1 );    /* send them the buffer */
	}
	sprintf( buf, "~DUMP:parrot: [end of data]\n.\n" );
	ng_sisendu( who, buf, strlen( buf )+1 );    /* send them the buffer */
}

/* 
	commands are ~string; the ~ is detected and stripped in the callback routine. Commands 
	come in via udp or tcp and some are expected to be dealt with solely with udp (the 
	original update function)
		Cxxxx	    - Causes a command to be executed
		D[UMP]	    - dump some stats to requestor
		F[ORWARD]   - Forward a message to addr:port in the buffer.
		L[OG]       - Flag sender's non-command messages as log messages for rebroadcast
		M[CAST]	    - act as an agent for a multi cast group (mcast port group-id)
		OK	    - Certify the TCP partner
		P[REGISTER] - Register a udp listener and prompt them when time to renew
		Q4360	    - Causes us to exit
		R[EGISTER]  - Register a udp listener, no prompt at renewal time
		U[NREGISTER]- The udp listener is dropping
		V[ERBOSE] n - Change verbose level
*/

void do_cmd( char *from, char *buf, struct partner *p )
{
	char *tok;

	ng_bleat( 2, "docmd: working on: %s", buf );
	switch( *(buf+1) )		
	{
		case 'C':	task( from, buf, p ); break;		/* run the command */
		case 'D':	dump( from ); break;			/* spill our guts to requestor */
		case 'F':	forward_msg( buf ); break;		/* forward the message to the address inside */
		case 'G':	start_general_agent( p ); break;	/* become a general UDP agent for the remote process */
		case 'L':	relog( p, buf ); break;			/* rebroadcast the log msg received */
		case 'M':	start_mcast_agent( p, strchr( buf, ' ' )+1 ); break;		/* become a multi cast agent for someone */
		case 'O':	p->flags &= PF_LEGIT; break;		/* session that knows to send this is marked as legt for spewing */
		case 'P':	seat( from, 1 ); break; 		/* seat and prompt them when they expire */
		case 'Q':	vamoose( buf );	break;			/* cause us to stop if the right key is there */
		case 'R':	seat( from, 0 ); break;			/* give them a seat in the audience */
		case 'U':	eject( from, NULL, 0 ); break;		/* theyre going home now */

		case 'V':	
				if( (tok = strchr( buf, ' ' )) != NULL )
				{
					verbose = atoi( tok + 1 ); 
					ng_bleat( 0, "verbose changed: new value=%d", verbose ); 
				}
				else
					ng_bleat( 0, "verbose bad command, expected: ~Verbose n" ); 
				break;

		default:	
				if( strncmp( buf, "ng_", 3 ) == 0 )		/* assume an ng_* command to execute */
					task( from, buf, p );
				break;
	}
}

void spew( char *buf, int len )
{
	struct ears 	*ep;		/* pointer into the audience */
	struct ears 	*op;		/* listener to oust */
	ng_timetype	now;		/* current time to check for listener expiration */

	now = ng_now( );

	if( *buf == esc_ch )
		buf++;			/* allow \~ to start the buffer */

	ep = audience;

	while( ep )			/* caution: we use a while cuz ep=ep->next has to happen differently when ejecting */
	{
		ng_sisendu( ep->id, buf, len );
		ep->xmits++;

		if( ep->expire < now ) 			/* see if their lease on their seat has expired */
		{
			if( ep->prompt )
			{
				if( verbose )
					sfprintf( sfstderr, "%s extending expiration time until %u\n", ep->id, now+300 );
				ng_sisendu( ep->id, "~REREGISTER", 12 );
				ep->prompt = 0;
				ep->expire = now + 300;     /* give them 30s to reregister so they dont miss anything */
				ep = ep->next;
			}
			else
			{
				if( verbose )
					sfprintf( sfstderr, "%s ejected because their ticket expired:  %u\n", ep->id, now );
				op = ep;
				ep = ep->next;
				eject( NULL, op, 0 );	/* trash em */
			}
		}	
		else
			ep = ep->next;	/* must do this way because eject logic needs to point past b4 calling eject() */
	}
}


/* ------------------------------- callback things ---------------------------------- */

/* 	we expect things to be newline separated records 
	flow manager routines deals with records that are split across multiple
	tcp buffers provided the whole record is less than 8k.
*/
int cbcook( void *data, int fd, char *nbuf, int len )
{
	static int seq = 0;		/* our sequence number if logging */

	char	seqc[10];		/* buffer to convert sequence number into ascii */
	struct partner *p;
	char	*buf;			/* pointer to a record in the bufer */
	static void	*f = NULL;	/* pointer to flow control */
	char	cbuf[NG_BUFFER];
	int	status=SI_RET_OK;
int	imc;				/* internal message counter */

	/* -------------  this if should be phased out, but must wait until all new rcmd binaries (that put \n at end of cmd) are out there --------- */
	/* 	agents can send commands (handled later), but may send multiple commands in one buffer. 
		this old code does not handle multiple commands in a single buffer. however, old versions of rcmd
		do not terminate their buffers with a newline, which ng_flow requires to separate buffers.
	*/
	if( !(flags & FL_AGENT)  &&  *nbuf == cmd_ch  )			/* if not an agent, see if buffer has a command */
	{								/* we assume sender will send only one per tcp req which is why we
									   are phasing this out and handling it below */
		if( len >= NG_BUFFER )
			return SI_RET_OK;

		memcpy( cbuf, nbuf, len );
		cbuf[len] = 0;
		
		for( p = strife; p && p->fd != fd; p = p->next ); 		/* find partner */
		if( p )
		{
			do_cmd( NULL, cbuf, p );
			if( ! (flags & FL_PARENT) )			/* if we are a child, we need to quit if session is gone */
				status = (p->flags  & PF_CONN) ? SI_RET_OK : SI_RET_QUIT;
		}
		else
			ng_bleat( 0, "cbcook (%d): could not find partner block for fd: %d %02x(flags)", mypid, fd, flags );

		return status;
	}
	/* --------------- end phaseout ----------------------- */
	
	if( !f  && !(f = ng_flow_open( 0 )) )
	{
		ng_bleat( 0, "cbcook (%d): cannot open flow manager; buffer ignored", mypid );
		return SI_RET_OK;
	}

	ng_flow_ref( f, nbuf, len );		/* register the network buffer with flow manager */
	imc = 0;				/* count messages that were in this tcp buffer */
	while( (buf = ng_flow_get( f, '\n' )) != NULL )		/* for each newline terminated record */
	{							/* buffers come back null terminated */
		imc++;
		len = strlen( buf );
		ng_bleat( 2, "cbcook (%d): 0x%x(strife) %x(flags) got buffer: %d(len) (%s)", mypid, strife, flags, len, buf );

		if( flags & FL_LOGGER )			/* we were dropped to logging state; all non-cmds are logs */
		{
			if( *buf != '%' )		/* logd known to send %command to us -- we will not rebroadcast */
			{
				if( ++seq > 9999 )
					seq = 1;					/* ng_log cannot support a sequence number of 0 */
				sfsprintf( seqc, sizeof( seqc ), "%04d", seq );
				memcpy( buf, seqc, 4 );					/* must have a local seq, not remote's */
	
				for( p = strife; p && p->fd != fd; p = p->next ); 	/* should be first -- take no chances */
				len = strlen( buf );					/* log messages must have \n. we will add on top of */
				ng_bleat( 3, "log rebroadcast: imc=%d seq=%d (%s)", imc, seq, buf );	
	
				buf[len] = '\n';		/* end of string mark; log_bcast supports buf w/ len */
				ng_log_bcast( buf, len, seq ); 	/* in addition to buffers that are null terminated */
			}
			else
				ng_bleat( 1, "%%cmd from ng_logd ignored: %s", buf );
		}
		else
		if( flags & FL_MCASTER )		/* put into multicast mode; all received messages are multicast to group */
		{
			sfsprintf( cbuf, sizeof( cbuf ), "%s\n",  buf );  	/* put back the \n */
			ng_sisendu( mcast_ip, cbuf, strlen( cbuf ) ); 		/* will send on the random udp port so msgs come back to us */
#ifdef KEEP
/* if we dont want msg to come here, newer version of narbalek supports @addr:data format and will subs addr 
   for our from address when dealing with the data -- thus sending responses directly back to our partner.  This 
   could be bad as those responses are udp and might not get there
*/
			if( strife->addr )
			{
				sfsprintf( cbuf, sizeof( cbuf ), "@%s:%s\n", strife->addr, buf );  	/* add their addr to msg */
				ng_sisendu( mcast_ip, cbuf, strlen( cbuf ) ); 

			}
			else
				ng_bleat( 1, "buffer ignored -- partner ip addr for mcast msg not available: %s", buf );
#endif
		}
		else		/* this block is to replace phased out version above; rcmd binary needs to append \n to each buf */
		if( *buf == cmd_ch )			/* something like ~PREREG or ~FORWARD */
		{
			if( len >= NG_BUFFER - 2 )
				return SI_RET_OK;
	
			memcpy( cbuf, buf, len );
			
			cbuf[len] = '\n';			/* tokeniser chops the newline; must add back */
			cbuf[len+1] = 0;
			
			for( p = strife; p && p->fd != fd; p = p->next ); 		/* find partner */
			if( p )
			{
				do_cmd( NULL, cbuf, p );
				if( ! (flags & FL_PARENT) )			/* if we are a child, we need to quit if session is gone */
					status = (p->flags  & PF_CONN) ? SI_RET_OK : SI_RET_QUIT;
			}
			else
				ng_bleat( 0, "cbcook (%d): could not find partner block for fd: %d %02x(flags)", mypid, fd, flags );
	
			if( status == SI_RET_QUIT )
				return status;
		}
		else
		{
			for( p = strife; p && p->fd != fd; p = p->next );	/* find partner */
			if( p && p->flags & PF_LEGIT )				/* repeat to listeners only if session partner verified */
			{
				buf[len-1] = '\n';		/* we spit out newline terminated records */
				spew( buf, len );		/* just spit it in all directions */
			}
		}
	}

	return SI_RET_OK;
}


/* call back for UDP data */
int cbraw( void *data, char *buf, int len, char *from, int fd )
{
	char nb[NG_BUFFER];				/* new buffer to build string when multicast agenting */
	int flen;

	if( flags & FL_LOGGER )			/* should not happen; child closes udp but we take no chance */
		return SI_RET_OK;

	ng_bleat( 2, "cbraw (%d): received %d bytes from: %s (%s)", mypid, len, from, buf );

	if( flags & (FL_MCASTER+FL_GENERAL) )		/* if in some kind of agent mode, send the message back as sender-addr:msg */
	{
		buf[len-1] = 0;							/* all input is expected to be \n terminated -- make string */
		sfsprintf( nb, sizeof( nb ), "%s:%s\n", from, buf );		/* tcp messages are expected as addr:msg */
		ng_bleat( 3, "cbraw: forward msg to partner: (%s)", nb );
		if( ng_sisendt( strife->fd, nb, strlen(nb) ) == SI_ERROR ) 	/* pass along; mkbaby ensures partner == strife */
			return  SI_RET_QUIT;					/* we quit if we drop the session */

		return SI_RET_OK;
	}

	if( *buf == cmd_ch )			/* commands from zoo still arrive udp; rcmd sends via tcp now */
	{
		ng_bleat( 2, "cbraw (%d): its a command", mypid );
		buf[len] = 0;			/* it better be a string */
		do_cmd( from, buf, NULL );
	}
	else
		spew( buf, len );		/* just spit it in all directions */

	return SI_RET_OK;
}

/* signal handler - alarms and dead kids */
int cbsig( void *data, int s )
{
	switch( s )
	{
		case SIGCHLD:   break;	/* should be handled by socket interface */

		case SIGALRM:
			if( heartbeat )
			{
				send_beat = 0;
				if( flags & FL_HEARTBEAT )	/* heartbeat too? */
					spew( "~HEARTBEAT", 11 );	/* and the beat goes on... */
				alarm( heartbeat ); 
			}
			break;

		default: break;
	}

	return SI_RET_OK;
}

/*	called when a connection is established -- buf has address of partner */
int cbconn( char *data, int f, char *buf )
{
	struct partner *p;

	ng_bleat( 2, "cbconn (%d): Connection request received %d(fd) from %s", mypid, f, buf ? buf : "unknown-addr" );
	p = ng_malloc( sizeof( struct partner ), "partner" );

	memset( p, 0, sizeof( struct partner ) );

	p->flags = PF_CONN | PF_SEND_NULL;		/* connection and send null terminated messages */
	p->fd = f;
	p->next = strife;
	if( buf )
		p->addr = strdup( buf );

	if( strife )
		strife->prev = p;
	strife = p;			/* add new to the head of the list */

	return SI_RET_OK;
}

int cbdisc( char *data, int fd )		/* disconnect received */
{
	struct partner *p = NULL;

	ng_bleat( 2, "disc(%d): session was terminated by the other end!  %d(fd) %02x(flags)", mypid, fd, flags );

	/* find the partner and remove it from the list */
	for( p = strife; p && p->fd != fd; p = p->next );
	if( p )
	{
		p->flags &= ~PF_CONN;
		drop_partner( p );
	}
	else
		if( ! flags & FL_PARENT )
			ng_bleat( 0, "cbdisc (%d): could not find partner for: %d", mypid, fd );

	return flags & FL_PARENT ? SI_RET_OK : SI_RET_QUIT;		/* we quit if we were a child */
}

main( int argc, char **argv )
{
	extern	int errno;
	extern	int ng_sierrno;

	char	from[50]; 		/* udp address of sender */
	char	buf[4096];		/* receive buffer */
	const	char *cport;		/* pointer to var from cartulary */
	int	len;			/* length of message */
	int	port = -1;		/* port to open */
	int	bg = 1; 		/* background */
	int	status = 0;
	char 	*sname = NULL;

	mypid = getpid( );

	if( (cport = ng_env_c( "NG_PARROT_PORT" )) != NULL )
		port = atoi( cport );	/* pick up default port number from cartulary */
	
	if( (workd = ng_env( "TMP" )) == NULL )
	{
		workd = "/tmp";				/* better than nothing */
		ng_bleat( 1, "unable to find TMP in environment/cartulary, using /tmp" );
	}

	ARGBEGIN
	{
		case 'e':	IARG( ex_time ); break;
		case 'f':	bg = 0; break;
		case 'h':	IARG( heartbeat ); flags |= FL_HEARTBEAT; break;
		case 'p':	IARG( port ); break;
		case 's':	SARG( sname ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
				break;
	}
	ARGEND

	
	if( sname != NULL )
		port = ng_getport( sname, "udp" );
	if( port < 0 )
	{
		sfprintf( sfstderr, "could not determine port\n" );
		exit( 1 );
	}

	ng_bleat( 0, "started; %s", version ); 
	ng_bleat( 1, "listening on port: %d", port );

	if( bg )		/* MUST hide before starting network if */
		hide( );	


	ng_sireuse( 1, 0 );							/* allow tcp port to be reused -- quick start */
	if( ng_siinit( SI_OPT_ALRM | SI_OPT_FG, -1, -1 ) == SI_OK )             /* dont ask for a tcp port; get later */
	{
		tlistensid = ng_silisten( port );                               /* open here so we can get sid to close */
		ulistensid = ng_siopen( port );                               	/* udp port open */

		if( tlistensid < 0 || ulistensid < 0 )
		{
			ng_bleat( 0, "main: unable to get a listen port: %d(tcp) %d(udp)", tlistensid, ulistensid );
			exit( 1 );
		}

		ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    			/* register rtn driven on connection */
		ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    			/* register rtn driven on session loss cb */
		ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );		/* tcp (cooked data) */
		ng_sicbreg( SI_CB_RDATA, &cbraw, (void *) NULL );		/* udp (raw) data */
		ng_sicbreg( SI_CB_SIGNAL, &cbsig, (void *) NULL );		/* called for singals */

		signal( SIGCHLD, &sigh ); 
		signal( SIGPIPE, &sigh );		/* must ignore or we get trashed when a socket is closed before we are done writing */
		/*signal( SIGALRM, &sigh ); */

		if( heartbeat )
			alarm( heartbeat );


		status = ng_siwait( );

		ng_bleat( 2, "main (%d): shutdown %d(status) %d(sierror) %s", mypid, status, ng_sierrno, flags & FL_PARENT ? "PARENT" : "CHILD" );
		ng_sishutdown( );          /* for right now just shutdown */

	}
	else
		sfprintf( sfstderr, "unable to initialise network for tcp/udp on port: %d (%d)\n", port, ng_sierrno );
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_parrot:UDP Message Repeater)

&space
&synop	ng_parrot [--man] [-e seconds] [-f] [-p port] [-s servicename] [-v]

&space
&desc	&ital(Ng_parrot) is a simple repeater that opens a UDP 
	port and accepts messages on it. Each message that it receives, 
	that is not a command message, is written to all of the 
	processes that have registered.  Processes register for 
	messages by sending a &lit(~REGISTER) (see &ital(Command messages))
	message via UDP/IP to &ital(ng_parrot). The port that 
	&ital(ng_parrot) listens on is defined using the &lit(NG_PARROT_PORT)
	cartular variable, or by one of two command line options
	that provide a service name or port number.
&space
	Remote processes may also establish TCP/IP sessions with &this 
	in order to use &this to rebroadcast log messages onto the cluster
	local network, or to act as a multicast
	agent on behalf of the remote process. Both of these functions 
	support satellite nodes which are on different physical networks
	(broadcast logging not available), and may not have the ability
	for processes like ng_narbalek to join a multicast group.
	These functions are explained in the commands section. 
&space
&subcat	System Command Execution
	&This will execute system commands that are delivered 
	with the command message tag. These commands are executed 
	asynchronously from normal &this processing and thus do 
	not cause &this to block, or delay, its normal message 
	repeating function. Processes requesting system command
	execution, and requesting standard error or statndard output 
	data be returned, &stress(must) expect that this data 
	will be interleaved with the normal message traffic 
	that &this is repeating. Output from commands is buffered
	until the command completes and is &stress(not) returned
	in realtime.  

&space
&subcat	Command Messages
	Messages received are considered to be 
	command messages if the first octet translates into a tilda (~) 
	character. The remainder of the buffer is assumed to be 
	ASCII text which is then parsed by 
	&ital(ng_parrot). If the string is recognised as a command 
	it is acted upon. If the buffer is not recognised, then no
	action is taken and the message is discarded. 
	Command strings that are in uppercase characters are 
	considered to be &ital(Parrot) commands, and commands that 
	are lowercase are assumed to be Pink Pages commands. 
	The following 
	are recognised command strings that may follow a tilda:
&space
&begterms
.if [ 0 ]
.** shhhhh this is secret stuff.
&term	CMD[-[N][C][O][E]] : The remainder of the message is assumed to be a command to 
	execute and is passed to the operating system (using a call to &lit(system())).
	One &ital(whitespace) character is assumed to separate the 
	command token from the remainder of the message; this one 
	byte will &stress(not) be passed to the operating system.
	Command output that is written to the standard output devce
	is captured and returned if the &lit(O) "flag" is appended to 
	the end of the command token. If standard output is requested
	it will be tagged with &lit(~STDOUT) tags when sent.
	Similarly, the &lit(E) flag appended to the end of the &lit(CMD)
	token will cause the standard error generated by the command to 
	be returned and tagged with &lit(~STDERR).
	The exit code from the system call is converted to an ASCII
	string and returned to the process submitting the request with 
	a message tag of &lit(~EC). The generally expected order of 
	messages resulting from a command are the exit code, standard
	output (if any, and requested), followed by standard error 
	output (again if any and if requested). If either standard output
	or statandard error data is requested, but was not generated 
	by the exectued command, then &stress(nothing) is returned 
	for that data type. 
&space
	If 'N' is supplied, then records returned to the requestor are newline 
	terminated.  By default the records are null terminated.
&space
	A 'C' in the string indicates that the connection is to be 
	closed following the execution of the command.  Normally, &this 
	allows the connection to remain open which allows the process at 
	the other end to send another request.  This provides for the 
	case where it is not easy for the remote process to close the 
	session.
.fi
	
&space
&term	DUMP : Causes a stats message for each registered listener 
	to be transmitted to the process sending this command. The 
	stats includes IP address, number of minutes elapsed since
	their first registration command, and number of packets they 
	have been sent. All stats messages are prefixed with the 
	word &lit(~DUMP:parrot:). 
&space
&term	REGISTER : Adds the sender to the list of listeners to which 
	non-command messages will be forwarded. The listener's entry
	in the list will remain valid for a period of time after
	which &ital(ng_parrot) will expire it and remove it. This prevents
	the listener list from growing too long as a result of listeners 
	failing to remove themselves from the list when they exit. 
	This also prevents sending messages to processes that are not 
	really listeners causing them some confusion.
&space
&term	PREGISTER : Performs the same function as the &lit(REGISTER) 
	command, but when a user's entry is expired &ital(ng_parrot) 
	will send a prompt for the listener to re-register itself. 
	This eliminates the need for the listener to implement any 
	timing mechanism to determine when it needs to reregister 
	itself.  The prompt message that is sent is a simple buffer 
	containing the word &lit(~REREGISTER).  After a prompt 
	message has been issued the listener's information will be maintained
	for approximately 30 more seconds which should allow the listener
	enough time to re-register without missing any messages that 
	are distributed between the prompt and the receipt of the 
	new registration. 
&space
&term	UNREGISTER : Causes &ital(ng_parrot) to remove the listener from 
	its audience. It is considered good form for a listener to 
	un-register themself before terminating. 
&space
&term	LOG : Places &this into log rebroadcast mode.  A process is 
	forked and messages received from the session partner are 
	assumed to be &lit(ng_log) messages which are rebroadcast on the 
	local network. 
&space
	NOTE: .br
	If a node is being run as a satellite node, and is on the same physcial 
	network as the rest of the nodes, the NG_LOGGER_PORT &stress(must) be 
	different than the port used by the other nodes.  If it is the same, 
	the node that is emulating the satellite funciton will start 
	broadcasting recursive log messages. 
&space
&term	MCAST : Causes &this to fork a process which will act as a multicast group 
	agent for the session partner.  The port number and multicast 
	group id (e.g. 29055 235.0.0.7)  are expected to follow &lti(MCAST) as space 
	separated tokens in the command buffer.   Once in mulitcast agent
	mode, all messages from the partner are sent to the multicast group
	address and port. All UDP messages received are returned to the 
	partner.  Returned messages have the format of address:data in order
	for the parter to know the originator of the message.  Messages from 
	the session partne, and from UDP are expected to be newline terminated.
&space
&term	OK : Requests that &this verify that the partner is a legitimate 
	partner and that all messages received from the partner (when not in 
	any kind of agent mode) will be forwarded to all listeners.  Currenlty
	no verification data is sent with the command, but this may change
	as security is modified. 

&endterms

&space
&subcat	Commands To Listeners
	Any messages that are sent to listeners that &ital(ng_parrot)
	expects to be interpreted as commands, rather than information 
	that &ital(ng_parrot) is just repeating, will all begin with 
	the tilda character (&lit(~)). The following is a list of
	commands that a listener can expect to receive:
&space
&begterms
&term	DOWN : This message indicates that &ital(ng_parrot) is being 
	terminated. This allows the listener to enter into some sort of 
	re-registration logic, and/or to reflect on its user interface
	that it has lost communication with its message source.
&space
&term	EC : The token following the command token contains the ASCII
	number indicating the result of the last command executed 
	as the result of a command request. 
&space
&term	HEARTBEAT : Is just that, an indication that &this is still alive.
&space
&term	HI : This message confirms that a &lit(REQUEST) or &lit(PREQUEST)
	message has been received.
&space
&term	REREGISTER : This message indicates that the listener's place in 
	&ital(ng_parrot's) audience will expire in approximately thirty
	seconds. If necessary the listener should re-register itself 
	or at the end of the timeout period messages will not be delivered
	to the listener.  
&space
&term	STDERR : Indicates a message that was generated as a result of 
	a command request to the standard error of the command.
&space
&term	STDOUT : Identifies a message that was written to the standard 
	output by the command executed  as a result of a command request.
	
&endterms

&space
&opts	The following options are recognised by &ital(ng_parrot) when 
	entered on the commmand line:
&space
&begterms
&term	-e sec : The number of seconds that an audience listener will
	be kept is set to &ital(sec).  If this option is not supplied
	then a listener will be expired after 300 seconds (5 minutes).
&space
&term	-d : Indicates that the remote command will detach and run 
	without returning any information to &this.  The &lit(-d) option 
	is needed to allow the remote &lit(ng_parrot) process to properly 
	close the TCP/IP session to &this before the command is invoked.
	If a command detaches, and does not close open file descriptors, 
	a &ital(parrot) ported socket is held open and can prevent 
	&lit(ng_parrot) from restarting if needed.  This is manditory 
	when executing a remote command with &lit(ng_wrapper) and using 
	the &lit(--DETACH) flag to &lit(ng_wrapper).
&space
&term	-f : Causes &ital(ng_parrot) to remain in the foreground and 
	not to become a daemon. Useful if debugging with verbose mode.
&space
&term	-h sec : Causes a heartbeat message to be spewed to all listeners
	ever &ital(sec) seconds. This should be used only when it is 
	anticipated that messages for &this to repeat will be arriving
	less frequently than the listener processes will tollerate 
	going without a message. 
&space
&term	-p port : Allows the user to define a specific port that 
	&ital(ng_parrot) should open and listen on. 
	If this option is supplied then it overrides the port number
	defined by the &lit(NG_PARROT_PORT) cartulary paramater.
&space
&term	-s name : Supplies an alternate service name to use. The 
	system call &lit(getservbyname()) is invoked with this 
	name in order to determine the port number to open. 
	If this option is supplied then it overrides the port number
	defined by the &lit(NG_PARROT_PORT) cartulary variable and
	the port (-p) commandline value if it was supplied.
&space
&term	-v : Verbose mode is turned on. This is only appropriate when 
	not running as a daemon. 
&endterms

&space
&exit	&ital(Ng_parrot) will exit with a non-zero return code if it 
	is not able to initialise.  

&space
&see	ng_ppget(tool), ng_ppset(tool), 
	getservbyname(3), ng_netif

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	13 Apr 2001 (sd) : Original code. 
&term	12 May 2001 (sd) : Corrected a problem with the extension on a 
	listeners ticket such that it really is extended by 30 seconds.
&term	31 May 2001 (sd) : To make use of the cartulary port variable
&term	31 Jul 2002 (sd) : To add support for Pink Pages.
&term	21 May 2003 (sd) : Rewritten to support TCP/IP command requests
	and remove the implementation of pinkpages. 
&term	04 Jul 2003 (sd) : Corrected an issue in wait loop that was preventing it 
	from clearing all dead kids in the event of multiple children popping off
	at the same time.
&term	28 Aug 2003 (sd) : Modified to accept and rebroadcast log messages. Added
	tcp/ip buffer processing allowing multiple, newline separated, messages
	to be in one buffer.
&term	29 Jul 2004 (sd) : Ensure that alarm is not set in a child process. 
&term	30 Jul 2004 (sd) : Fixed parrot droppings (stderr/out files in $TMP) left 
	when sessions terminated before we were done writing to them.
&term	11 Aug 2004 (sd) : corrected possible buffer overrun problem. (v3.2)
	.** if stdout/stderr pathnames greater than 100 chrs combined 
&term	11 Sep 2004 (sd) : Added the ability to accept a detached flag on 
	the command -- prevents scripts (ng_wrapper/ng_endless) from holding 
	a parrot port and preventing restarts. (V3.3)
&term	25 Oct 2004 (sd) : Added multicast agent support (V4.0).
&term	19 Jan 2005 (sd) : Fixed verbose message that was putting out junk.
&term	23 Mar 2005 (sd) : Added cleanup if mkbaby() is not able to fork -- sends 
		back an error msg to stderr, and a bad return code.
&term	22 Apr 2005 (sd) : Changed verbosity on a couple of bleat messages.
&term	07 Aug 2005 (sd) : Corrected issue in send_msg that was not traping an error 
		when writing to a session that had terminated. 
&term	26 Sep 2005 (sd) : Ignores log rebroadcasts if they appear to be %command
		messages like %roll that ng_logd might be passing on to us.
&term	07 Oct 2005 (sd) : After chasing a problem with odd log messages, I added 
	a note to the man page; see LOG and what can happen if a node has the 
	satellite attribute but id on the same physical network (broadcasts work)
	with the rest of the cluster. 
&term	17 May 2006 (sd) : Fixed bug in sending command output back to a UDP 
		listener.
&term	01 Jun 2006 (sd) : Command now has double quotes (") escaped.  V4.2.
&term	15 Jun 2006 (sd) : Fixed coredump issue if verbose command did not have a space. (4.3)
&term	06 Aug 2006 (sd) : Now accepts N and C options for remote command execution.
&term	21 Nov 2006 (sd) : Converted sfopen() to ng_sfopen().
&term	07 Dec 2006 (sd) : Fixed problem with large command strings (tripping up seneschal sherlock invocations). (v4.5)
&term	02 Jun 2007 (sd) : Added forward command to support narbalek UDP responses through firewall. Added a verification
		for TCP sessions; this must be received from the session partner before its messages will be spewed to 
		all listeners. 
&term	10 Jun 2007 (sd) : Updated some comments to make code clearer.
&term	22 Jan 2008 (sd) : Fixed problem when CMD-M was sent; redirection of stderr was not right.
&endterms

&scd_end
------------------------------------------------------------------ */
