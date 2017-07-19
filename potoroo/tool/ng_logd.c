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
----------------------------------------------------------------------------
 Mnemonic:	ng_logd
 Abstract:	Daemon responsible for listing for log messages and writing 
		them to the approriate log. Once a message has been 
		written to the correct log an acknowledgement message is 
		returned to the caller.  See SCD at end for all of the 
		gories.

 Protocol:	This expects messages via udp broadcast to have a trailing 
		newline character so we dont need to do any extra manipulation
		or copying when writing the message to the log.
		Parrot expects messages to be newline separated as it will 
		broadcast each in seperate udp packets.

 Parms:		-p port : allows a specific port number to be supplied
		-n name : allows an alternate service name to use when 
			looking up the port number
		-man : dump the man page.
 Returns:
 Date:		2 April 2001
 Author: 	E. Scott Daniels
----------------------------------------------------------------------------
*/

#include	<string.h>
#include	<time.h>
#include 	<inttypes.h>
#include	<syslog.h>
#include	<unistd.h>
#include	<stdio.h>
#include 	<sfio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<ctype.h>

#include	<sys/socket.h>         /* network stuff */
#include	<net/if.h>
#include 	<netinet/in.h>
#include	<netdb.h>

#include	<ningaui.h>
#include	<ng_socket.h>

#include	"ng_logd.man"    /* our man page stuff and show_man routine */

extern char	*argv0;          /* goes away when we have a real library working */
extern int	verbose;
extern	int	ng_sierrno;	/* error reason from socket stuff */

/* --------------- prototypes -------------------- */
static void parse( int, char *, int, char * );

/* -------------- constants ---------------- */
				/* flag constants */
#define	FL_DAEMON	0x01    /* become a daemon */
#define FL_HIDDEN       0x02    /* already a daemon; dont try again */
#define FL_PAUSED	0x04	/* we have been shut up for a bit */
#define FL_VIATCP	0x08	/* write messages to a tcp partner for broadcast */

#define SQUIRREL_FILE	NG_ROOT "/tmp/ng_logd.cache"


/* ------------ globals -------------------- */
int	flags = FL_DAEMON;
char	*curlfn = NULL;		/* currently open log file name */
Sfio_t	*curlfd = NULL;       	/* current log "fd" */
char	*adjunctfn = NULL;	/* name of ajunct file */
Sfio_t	*adf = NULL;		/* adjunct file pointer */
char	*squirrelfn = NULL;	/* file where we will squirrel away messages when paused */
char	*verbosefn = NULL;	/* verbose file name */
Sfio_t	*verbosef = NULL;
Sfio_t	*dummy_stdin = NULL;	/* seems bad things happen if stdin/stdout are not open */
int	tcpfd = -1;		/* si handle for the tcp session if sending that way */
char 	*direct_address = NULL;	/* address from -T that overrides  ning<cluster> */

/* ------------------------------------------------------------- */
/* daemons should be neither seen nor heard */
static void hide( )
{
	int i;

	if( ! (flags & FL_DAEMON ) )	/* stay in the open if asked to */
		return;

	if( flags & FL_HIDDEN )
		return;            /* bad things happen if we try this again */

	switch( fork( ) )
	{
		case 0: 	break;        /* child continues on its merry way */
		case -1: 	perror( "could not fork" ); return; break;
		default:	exit( 0 );    /* parent abandons the child */
	}

	sfclose( sfstdin );
	sfclose( sfstderr );
	sfclose( sfstdout );
	for( i = 3; i < 200; i++ )	
		close( i );              /* dont leave caller's files open */

	flags |= FL_HIDDEN;

	if( verbosefn )			/* verbose file supplied, switch to that file */
	{
		if( (verbosef = ng_sfopen( NULL, verbosefn, "a" )) )
		{
			ng_bleat_setf( verbosef );		/* bleat now will write here */
			sfset( verbosef, SF_LINE, 1 );		/* force a flush with each write */

			ng_bleat( 0, "started -- hidden" );
		}
	}
	else
		verbose = 0; 					/* not if hidden and no specific file given */
}

/* write buffer to the cache file. if buf is null, then read the cache file and 
   write the messages to the log 
*/
static void cache( char *buf, int len )
{
	static Sfio_t	*f = NULL;		/* pointer to the open file */
	char	tname[255];			/* tmp file name */
	char	*ibuf;				/* input buffer from file */
	unsigned	count = 0;

	if( buf )				/* something to write */
	{
		buf[len-1] = 0;
		ng_bleat( 1, "cache: %d %s\n", len, buf );
		if( f || (f = ng_sfopen( NULL, squirrelfn, "a" )) )
			sfprintf( f, "%s\n", buf );
	}
	else					/* assume we will need to read and log the cached messages */
	{
		*tname = 0;

		if( f )				/* if not open, then no messages were cached */
		{
			ng_bleat( 2, "recovering cached messages from: %s", squirrelfn );
			if( sfclose( f ) )
			{
				sfsprintf( tname, sizeof( tname ), "%s.err", squirrelfn );	/* rename the file we ignored */
				rename( squirrelfn, tname );						

				ng_log( LOG_ERR, "error closing/writing cached message file; moved for analysis to: %s", tname ); 
				ng_bleat( 0, "error on close of cache file; ignoring contents; moved for analysis to: %s", tname );
			}
			else
			{
				sfsprintf( tname, sizeof( tname ), "%s.%I*u", squirrelfn, sizeof( ng_timetype), ng_now( ) );	/* tmp name for squirrel file */
				rename( squirrelfn, tname );						/* move to prevent data loss */

				if( (f = ng_sfopen( NULL, tname, "r" )) )
				{
					while( (ibuf = sfgetr( f, '\n', 0 )) != NULL  )
					{
						parse( 0, ibuf, (int) sfvalue( f ), NULL );		/* add to log without ack */
						count++;
					}
	
					if( ! sferror( f ) )
					{
						ng_bleat( 1, "cached messages recovered successfully: %d messages", count );
						sfclose( f );
						unlink( tname );			/* trash the temporary file */	
					}
					else
					{
						sfclose( f );				/* close but keep it */
						ng_bleat( 0, "errors during recovery of cached messages: %d messages left in %s", count, tname );
						ng_log( LOG_ERR, "error writing cached messages to log from: %s", tname );
					}
				}
				else
				{
						ng_bleat( 1, "could not open cached message file: %s", count, tname );
						ng_log( LOG_ERR, "could not open cached message file: %s", tname );
				}
			}

			f = NULL;		/* its static; clean it up */
		}
		else
			ng_bleat( 1, "squirrel file not opened, no recovery attempted" );
	}
}

/*	close and rename (roll over) adjunct log if open and new name supplied 
*/
void roll_adj( char *name )
{
	if( adf )		/* if adjunct open, we must close too */
	{
		if( sfclose( adf ) )
			ng_bleat( 0, "roll_adj: write or close error on adjunct file" );
		adf = NULL;
	}

	if( adjunctfn && name && *name )		/* good names */
	{
		if( rename( adjunctfn, name ) )	
			ng_bleat( 0, "roll_adj: move failed (%s -> %s): %s", adjunctfn, name, strerror( errno ) );
		else
			ng_bleat( 2, "roll_adj: success %s -> %s", adjunctfn, name  );
	}
	else
		ng_bleat( 1, "roll: adjunct file name, or new name invalid; not rolled" );
}

/* close and reset things */
void close_log( int all )
{
	if( curlfd )
		if( sfclose( curlfd ) )
			ng_bleat( 0, "close/write error to current log" );

	if( curlfn )
		free( curlfn );

	if( all && adf )		/* if adjunct open, we must close too */
	{
		if( sfclose( adf ) )
			ng_bleat( 0, "close/write error to current adjunct file" );
		adf = NULL;
	}

	curlfn = NULL;
	curlfd = NULL;
}

/* assuming alt log file messages will be few and far between, we will hold
   the last one we opened and cycle them as we get messages for different files.
*/
Sfio_t *open_log( char *fname )
{
	char	*lname;				/* real name of the log file after var expansion */

	ng_bleat( 4, "open_log %s  adj=%x", fname, adf );
	if( adjunctfn != NULL && adf == NULL )			/* one supplied, and not open at the moment */
	{
		if( (adf = ng_sfopen( NULL, adjunctfn, "a" )) != NULL )
		{
			ng_bleat( 2, "adjunct file opened: %s", adjunctfn );
			sfset( adf, SF_LINE, 1 );		/* force a flush with each write */
		}
		else
			ng_bleat( 0, "unable to open adjunct file: %s: %s", adjunctfn, strerror( errno ) );
	}

	if( curlfn )
	{
		if( strcmp( curlfn, fname ) == 0 )   		/* open one matches what was passed in */
			return curlfd;
		else
			close_log( 0 );				/* close just the current log; not ajdunct if its open */
	}
	

	if( isupper( *fname ) )				/* if a capitalised letter - assume its a var name */
	{
		if( ! (lname = ng_env( fname )) )                        /* get name from config  */
			if( ! (lname = ng_env( "MASTER_LOG" )) )           /* attempt to get master name */
				if( ! (lname = getenv( "NINGAUI_LOG" )) )  /* get name from environment */
					lname = LOG_NAME;                  /* use default if not there */ 
	}
	else                                                /* else assume user supplied filename to use */
		lname = fname;

	if( (curlfd = ng_sfopen( NULL, lname, "a" )) != NULL )   	/* ensure that the specified log is opened */
	{
		sfset( curlfd, SF_LINE, 1 );			/* force a flush with each write */
		curlfn = ng_strdup( fname ); 			/* save the VARIABLE name so we dont have to expand each time */
	}

	return curlfd;       /* will be null if error */
}

/* send the message received to a TCP buddy for rebroadcast on their wire */
static void rebroadcast( char *msg, int len )
{
	static	int fail_msg = 0;	/* we dont want to litter the log with failure messages */
	char 	buf[NG_BUFFER];
	char	*cname;		/* cluster name -- we assume the point of contact is ning<clustername>:parrot_port*/
	char	*pport;		/* parrot port number */

	if( tcpfd < 0 )		/* no session at the moment, lets try to get one */
	{
		pport = ng_env( "NG_PARROT_PORT" );
		cname = ng_env( "NG_CLUSTER" );
		if( !pport || !cname )
		{
			ng_bleat( 0, "cannot get parrot port or cluster name to open tcp session" );
			return;
		}

		if( direct_address )
			sfsprintf( buf, sizeof( buf ), "%s:%s", direct_address, pport );
		else
			sfsprintf( buf, sizeof( buf ), "%s:%s", cname, pport );
		ng_bleat( 1, "direct logging via tcp enabled to: %s", buf );
		free( pport );
		free( cname );
		if( (tcpfd = ng_siconnect( buf )) >= 0 )
		{
			fail_msg = 0;						/* if we disc and cannot reconn, 1 message */
			ng_bleat( 1, "connected to tcp partner: %s", buf );
			ng_sisendt( tcpfd, "~LOG\n", 5 );			/* put parrot in a log rebroadcast mode */
		}
		else
		{
			if( ! fail_msg )
				ng_bleat( 0, "unable to connect to tcp partner: %s", buf );
			fail_msg = 1;
			return;
		}
	}

	ng_sisendt( tcpfd, msg, len );		/* send of just as we got it -- newline terminated */
}

/* parse message and send ack */
static void parse( int nfd, char *obuf, int len, char *replyto )
{

	char	*fname;
	char	*seq = NULL; /* senders sequence information */
	char	*msg;        /* pointer past the broadcast header to msg */
	char	*buf;		/* copy of the buffer to trash */
	char	ack[100];    /* buffer to build ack message in */
	int	olen;		/* length of the original in case we must cache it */
	int	hlen;		/* length of the header */
	int	i;
	Sfio_t	*f;


	buf = obuf;	/* once upon a time the message was duped incase we needed to cache.  not needed, but refs to buf kept */

	if( *(buf+4) != ' '  &&  ! isdigit( *buf ) && *buf != '%' )			/* suspect header */
	{
		buf[len-1] = 0;
		ng_bleat( 0, "suspect header: replyto=%s buf=(%s)", replyto, buf );
		return;
	}

	if( (flags & FL_VIATCP) && replyto )	/* sending to a tcp rebroadcaster (we dont if it was cached -- null replyto) */
		rebroadcast( obuf, len );

	i = 0;
	for( msg = buf; *msg != ';' && *msg != 0; msg++ );		/* find end of header (first ;) */
	
	olen = len;				/* save incase we must cahce */
	hlen = msg - obuf;			/* save incase we must cache -- length of header */
	len -= hlen;				/* length of the remainer */

	seq = buf;

	if( *msg )
	{
		*msg = 0;            				/* make header its own string */
		for( msg++, len--; isspace( *msg ); msg++ )
			len--;					/* skip leading whitespace, and reduce len */
		
		if( seq && *seq == '%' )			/* command ok to trash header */
		{
			seq = strtok( buf, " \t" );		/* at first nonblank in header -- sequence number */
			fname = strtok( NULL, " \t" );		/* at the filename in header */

			if( strcmp( seq, "%stop" ) == 0 )
			{
				close_log( 1 );			/* close all files */
				ng_closeudp( nfd );
				ng_bleat( 1, "stop command received from: %s", replyto ? replyto : "unknown sender" );
				if( verbosef )
					sfclose( verbosef );
				exit( 0 );
			}
			else
			if( strncmp( seq, "%roll", 5 ) == 0 )			/* roll the adjunct file */
			{
				roll_adj( fname );				/* assume new name is there */
				ng_writeudp( nfd, replyto, "+ok\n", 4 );
			}
			else
			if( strcmp( seq, "%close" ) == 0 )
				close_log( 1 );				/* ensure adjunct closed too */
			else
			if( strcmp( seq, "%pause" ) == 0 )
			{
				close_log( 1 );				/* close all log files */
				flags |= FL_PAUSED;
				ng_bleat( 1, "mode is now paused" );
			}
			else
			if( strcmp( seq, "%resume" ) == 0 )
			{
				flags &= ~FL_PAUSED;
				cache( NULL, 0 );				/* dump cached messages to logs */
				ng_bleat( 1, "normal mode resumed" );
			}
			else
			if( strcmp( seq, "%ping" ) == 0 )
				ng_bleat( 1, "ping received from %s", replyto );
			else
			if( strcmp( seq, "%verbose" ) == 0 )
			{
				verbose = atoi( fname );
				ng_bleat( 0, "verbose changed to %d", verbose );
			}

			return;
		}
		

		if( flags & FL_PAUSED )	
		{
			*(obuf + hlen) = ';';		/* make the original message whole again */
			ng_bleat( 1, "caching %d bytes", olen );
			cache( obuf, olen );		/* squirrel it away for later */
			return;
		}

						/* not paused -- safe to trash the header */
		seq = strtok( buf, " \t" );		/* at first nonblank in header -- sequence number */
		fname = strtok( NULL, " \t" );		/* at the filename in header */
		if( ! fname || ! seq )		/* odd message truncation -- must ignore */
			return;			/* cannot do anything useful without these */

		if( (f = open_log( fname )) != NULL )
		{
			for( i = len-1; i > 0 && *(msg+i) < ' '; i-- );		/* old log function sent 00\n -- erk */
			i++;
			if( i < len )
			{
				*(msg+i) = '\n';
				len = i+1;
			}
			
			if( adf )				/* mirror the message to the adjunct file */
			{
				sfwrite( adf, msg, len );
				if( msg[len-1] != '\n' )		/* should not happen, but paranoia is good */
					sfwrite( adf, "\n", 1 );	/* lets enforce a newline at the end */
			}

			if( sfwrite( f, msg, len ) == len ) 		/* write to log file */
			{
				if( msg[len-1] != '\n' )		/* ditto */
					sfwrite( f, "\n", 1 );		/* lets enforce a newline at the end */
				sprintf( ack, "+%s", seq );
			}
			else
				sprintf( ack, "-%s", seq );

			if( replyto )		/* when dumping cache this is null, so no attempt to ack an old msg is made */
			{
				ng_bleat( 3, "reply to %s was: %s", replyto, ack );
				ng_writeudp( nfd, replyto, ack, strlen( ack ) + 1 );
			}
		} 
		else
			ng_bleat( 0, "cannot open log file: %s: %s", fname, strerror( errno ) );
	}
}

/* --------------------------- call back routines ------------------------------ */

        
int cbdisc( char *data, int fd )      
{
	if( fd == tcpfd )
	{
		ng_bleat( 0, "tcpip disconnect fid=%d", fd );
		tcpfd = -1;
	}

	return SI_RET_OK;
}

int cbcook( void *data, int fd, char *buf, int len )
{
	return SI_RET_OK;				/* we expect nothing from the tcp side of the house */
}

int cbraw( void *data, char *buf, int len, char *from, int fd )
{
	if( verbose > 2 )
	{
		char h;

		h = buf[len-1];
		buf[len-1] = 0;
		ng_bleat( 3, "raw: %d (%s) %02x", len, buf, (unsigned char) h );
		buf[len-1] = h;
	}

	parse( fd, buf, len, from );
	return SI_RET_OK;
}


/* ----------------------------------------------------------------------------- */

static void usage( )
{
	
	sfprintf( sfstderr, "usage: %s [-a adjunctfile] [-f] [-l verboselog] [-p port | -n service-name] [-p port] [-s cachefilename] [-t] [--man]\n", argv0 );
}

int main( int argc, char **argv )
{
	char	*version = "v2.4/02197";
	char	*port = NULL;
	char	*sname = NULL;			/* allow as service name from the cmd line */
	int	status;
	char	input[NG_BUFFER];   
	char	*ng_root = NULL;

	if( (ng_root = ng_env( "NG_ROOT" )) == NULL )
	{
		ng_bleat( 0, "ng_logd: abort: unable to find NG_ROOT in cartulary" );
		exit( 1 );
	}

	port = ng_env( "NG_LOGGER_PORT" );	/* default from cartulary */

							/* gather a few things from the cartulary */
	adjunctfn = ng_env( "LOGGER_ADJUNCT" ); 		/* get from cartulary/env if defined */
	if( (squirrelfn = ng_env( "LOG_SQUIRREL" )) == NULL )	/* cache things in a fairly wellknown location */
	{
		sfsprintf( input, sizeof( input ), "%s/site/log/logd.cache", ng_root );
		squirrelfn = strdup( input );
	}

	ARGBEGIN
	{
		case 'a': SARG( adjunctfn ); break;
		case 'f': flags &= ~FL_DAEMON; break;  		/* stay in foreground */
		case 'l': SARG( verbosefn ); break;		/* setup a verbose file if we become a daemon */
		case 'p': SARG( port ); break;         		/* converted to int in init */
		case 'n': SARG( sname ); break;
		case 's': SARG( squirrelfn ); break;		
		case 't': flags |= FL_VIATCP; break;		/* write log messages to tcp partner */
		case 'T': flags |= FL_VIATCP; SARG( direct_address ); break;
		case 'v': verbose++; break;

		case '?': 
			ng_bleat( 0, "%s %s", argv0, version );
		default:	
usage:			usage( ); exit( 1 ); 
	}
	ARGEND
	
	
	hide( );						/* tuck away unless -f was on; must hide before starting network */	
	ng_bleat( 1, "squirrel file: %s", squirrelfn );

	if( ng_siinit( SI_OPT_FG, -1, atoi( port ) ) == SI_OK )			/* open only udp port */
        {
                ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );                        /* register rtn driven on session loss cb */
                ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );              /* tcp (cooked data) */
                ng_sicbreg( SI_CB_RDATA, &cbraw, (void *) NULL );               /* udp (raw) data */

                status = ng_siwait( );

                ng_bleat( 2, "shutdown: %d(status) %d(sierror)\n", status, ng_sierrno  );
                ng_sishutdown( );          /* for right now just shutdown */
	}

	if( verbosef )
		sfclose( verbosef );

	return 0;
}

/* ---------- SCD: Self Contained Documentation ------------------- */
#ifdef NEVER_DEFINED
&scd_start
&doc_title(ng_logd:Log Daemon For Cluster Logging)

&space
&synop	ng_logd [-a filename] [-f] [-l filename] [-n service-name | -p port] [-s filename] [-v]
	

&space
&desc	&ital(Ng_logd) is a daemon that listens on a well known UDP port
	for Ninaui log messages. Messages that are received are written 
	to the appropriate log and then an acknowledgement is sent to 
	the sender.	The port that is listened to is determined 
	from the cartulary variable &lit(NG_LOGGER_PORT) or 
	command line parameters.
&space
&subcat Satellite Direct Logging
	If the &lit(-t) or &lit(-T) options are set on the command line, 
	&this will connect via TCP/IP to a repeater process (ng_parrot)
	and send any log messages received to the repeater.  The 
	messages are expected to be rebroadcast on the cluster network
	for recording in multiple master log files.  This mode allows 
	a satellite node (a node not capable of broadcasting messages
	on the cluster network) to supply real-time messages to the 
	cluster logs. 

&space
	In order for satellite direct logging to work, two cartulary 
	variables must be set. These are 
&space
&litblkb
  NG_LOG_BCAST=localhost
  LOG_ACK_NEEDED=1
&litblke
&space
	The broadcast variable causes the ng_log library function to send 
	log messages to the &lit(ng_logd) process running on the local 
	host and not to broadcast these messages.  The number of acks 
	that are needed is set to 1 to cause the &lit(ng_log) function 
	to send messages (in the past satellite nodes had this set to 0
	so as not to send log messages at all), but to exepct only 
	an ack from one &lit(ng_logd) process wich will be the local 
	process. 

&subcat	The Adjunct File
	It may at times be necessary to have &this keep an adjunct log file
	which contains &stress(all) messages that are received regardless
	of the log file that the message is actually written to. This 
	adjunct file is opened and written to only if the adjunct file 
	name is passed in from the command line, or the &lit(LOGGER_ADJUNCT)
	cartulary variable is defined. The adjunct file is also closed 
	when the &ilt(%close) or &lit(%pause) commands are received. 
&space
	The &lit(%roll) command will cause &this to rename the adjunct 
	file after closing it.  It also sends an acknowledgment message to 
	the sender (+ok) to indicate that the adjunct file has been 
	closed and renamed successfully.  The syntax of the roll command 
	is:
&space
&litblkb
   %roll new-filename;
&litblke
&space
	AFter the adjunct file is rolled over to the new name, the next 
	received log message will cause the file, with the name defined 
	at the time &this was started, to be opened and written to.

&space
&subcat	Pausing The Logger
	&This can be suspended from writing to any log files (including
	the adjunct file) in order to allow external processes to 
	maintain the log files. When a &lit(%pause) command is received 
	&this begins to write all received messages to a cache file
	(as defined by the &lit(NG_NACKLOG) cartulary variable). Messages
	are writtenn to this file in the format that they are received
	(see &ital(Message Format) below) so that they can be easily 
	parsed when normal logging is resumed. Should &this terminate
	(normally or abnormally) while paused, the messages in the 
	cache file are left intact and can be recovered.

&space
&subcat	Message Format
	&ital(Ng_logd) expects inbound messages to be in one of two 
	formats:
&beglitb
	%<command> <parms>;
	<sequence><whitespace><logfile>;[<whitespace>]<message-text>
&endlitb
&space
	The &lit(%<command>) messages allow for an interface to the daemon
	to force it to take action should this be necesary in the future. 
	The commands that are currently supported are:
&space
&begterms
&term	%close : Causes &ital(ng_logd) to close any currently open log 
	files. This can be used by a process who needs to move logfiles
	from "under" the daemon. If such is necessary, then the process
	should first move the log file, and then send the close command
	to &ital(ng_logd). Following the close command, the next message
	received for the moved log file will be written into the 
	new file.
&space
&term	%pause : &This will close all open files and start to cache messages 
	received following the receipt of this command.  Messages are cached
	in the &lit(NG_NACKLOG) log file. 
&space
&term	%resume : The receipt of this command causes the effects of the &lit(%pause)
	command to be inverted and messages will again be written to the 
	appropriate log file.  Any messages that were cached are written 
	to the appropriate log file in the order that they were received.
&space
&term	%roll : The roll command causes the adjunct file to be closed and 
	renamed assuming that the first parameter after the &lit(%roll) is 
	the new name of the file.  This allows the log fragment processor to 
	endure that &this is done writing to the current adjunct log file 
	before it begins to process it. &This will send a confirmation message
	to the sender of the &lit(%roll) command (+ok) to indicate that the 
	command was processed. 
&space
&term	%stop : Causes &ital(ng_logd) to shutdown.
&endterms

&space
	Any message that is received that does not begin with a percent 
	sign (%) is considered to be a log message with the second 
	recognised syntax illustrated earlier. The first token of a message
	buffer is expected to be a sequence number. This sequence number is
	used to reply to the sender with a status indicating the ability of
	&this to successfully write the message to the log file.
	&This adds a plus sign (+) to the sequence number and returns 
	it via UDP/IP message to the sender to indicate as successful dispensation 
	of the received messag. If &this is unable to successfully deal with 
	the message, a negative sign (-) is added to the front of the sequence
	number before the response message is returned.

&space
	The &ital(<logfile>) token is either a fiilename of 
	the log file that the message should be written to, or a cartulary 
	variable name that is used to map the log file path name. If a cartulary 
	varaible name is used, its name &stress(must) begin with a capitalised 
	letter.  The use of real filenames is &stress(not) recommended, but is 
	supported in order to be backward compatable with older programmes that 
	assumed that a filename was to be passed. 

&space	All tokens following the semicolon (;) are taken literly and written to 
	the log file.  Any embedded newline characters are replaced with blanks
	as the log file records are newline separated. 

&space
&subcat	Open Log Files
	The assumption that &ital(ng_logd) makes is that the vast majority 
	of the messages will be written to the same log file. Therefore,
	it will hold the mostrecently used log file open until it receives
	a message for a different log file.  When a message destined for 
	a log file that is not the currently open log file is received, 
	the current file is closed and the new file is opened. 
	Should it prove that multiple log files will be utilised more than 
	is expected, the daemon should be modified to maintain a series
	of open log files to prevent possible "thrashing" through opens
	and closes. 
	

&subcat Cartulary Variables
	Several cartulary variables affect the logging environment as a 
	whole.  Most of the variables listed below are of direct consequence
	to the &lit(ng_log) library function and not &this, but are listed
	here for completeness.
&space

&begterms        
&term   LOG_ABORT : Defines the abort message priority.
        &ital(Ng_log) and &ital(ng_alog) will abort when
        the number of required acknowledgements for a message of the indicated,
        or higher priority, is not received. If this variable is not
        defined the default is -1 which turns off the abort logic.
&space
&term   LOG_ACK_NEEDED : Specifies the number of positive acknowledgements
        that are waited for and implies that broadcast mode is enabled.
        If this cartulary file parameter is not
        specified, then 3 is assumed (also implying that broadcast mode
        is enabled). If this value is set to zero, or a
        negative value, then broadcast mode is disabled and the message is
        written to the indicated log file on the local host.
&space
&term   LOG_IF_NAME : The name of the interface that is used for
        sending broadcast messages. If this cartulary variable is not
        defined, then a value of &lit(eth0) is used as the default. 
	It is wise to allow the &lit(ng_init) script to set this variable 
	during initialisation based upon a network address rather than a 
	hard interface name as these values can change between system 
	starts.  If the cartulary variable &lit(NG_BROADCAST_NET) is 
	set (e.g. 135.207.3) then the value of &lit(LOG_IF_NAME) is set 
	to the interface currently associated with the network address. 
&space
&term   LOG_NACKLOG : Defines the file name of the file that should be
        used to log instances when not enough acks are received during
        the timeout period. If not defined then the file name   
        &lit(/ningaui/site/log/ng_log.nack) is used as a default.
&space          
&term   LOG_TIMEOUT : Defines the number of seconds that acknowlegements
        must be received in. If &lit(LOG_TIMEOUT) seconds passes
        before the required number of positive ackowledgements have
        been received, then an error state is entered.
        If this variable is not defined in the cartulary file then
        a default of 20 seconds is used. A value of 2 or 3 is recommended.

&space
&term   NG_LOG_BCAST : Supplies the IP address of the host that is running
        a Ningaui log daemon.  When the 'broadcast' address is supplied to
        &this via this cartulary variable, UDP log messages are sent to
        this address rather than to the standard UDP broadcast address for
        the network interface.
        If this cartulary variable is not defined, then messages are written
        using the broadcast address returned by the interface.  If this
        variable is set to a host name (including localhost) rather than
        a true broadcast address, then the &lit(LOG_ACK_NEEDED) variable
        should be set to 1 to prevent waiting on ack/nack messages that
        will never be received.
&endterms


&space
&opts	The following commandline options are recognised by &ital(ng_logd):
&space
&begterms
&term	-a filename : Supplies the name of an adjunct file that &stress(all) 
	messages are logged to.  &This will open and write all messages to 
	the file named if this name is supplied via a command line option.
	The adjunct file is closed when the &lit(%close), &lit(%roll) or &lit(%pause) 
	commands are received. 
&space
&term	-f : Prevents &ital(ng_logd) from becoming a daemon. The process 
	remains in the foreground.
&space
&term	-l filename : Allows &this to write verbose messages when running 
	in the background.  &ital(fIlename)  will be opened (in append mode)
	and all verbose messages that would be written with the current 
	verbose setting will be written to this file. By convention verbose
	messages are written to the standard error device, but also by 
	convention daemons close standard input, output and error when 
	they detach from their parent process. If this file name is not 
	supplied on the command line, the verbose level is set to 0
	when &this detaches, and thus verbose messages are lost. 
&space
&term	-n sname : Causes &ital(ng_logd) to use &ital(sname) as the service 
	name when looking its port number up. &ital(Sname) is assumed to be
	defined in &lit(/etc/services), or via yellow pages, with a prototype
	of UDP. If the &lit(-n) option is not supplied, the service name
	&ital(ng_logger) is used. This option overrides the value of the 
	&lit(NG_LOGGER_PORT) cartulary variable &stress(and) the &lit(-p)
	command line option if it was supplied.
&space
&term	-p port : Supplies the UDP port number that &ital(ng_logd) will listen 
	on. If this option is supplied the value of the &lit(NG_LOGGER_PORT)
	cartulary variable is ignored.
&space
&term	-s filename :	Supplies the name of the file that &this uses to 
	squirrel away messages when in a paused state. If this filename is 
	not supplied then the value of the &lit(LOG_NACKLOG) cartulary 
	variable is used. 
&space
&term	-t : Write 'raw' log messages to the process that is listening on 
	&lit(NG_PARROT_PORT) on the external interface node for the
	current cluster. The interface node name is assumed to be &lit(ning)&(<cluster>)
	(e.g. ningbing for the bing cluster, and ningm for the m cluster).
	The cartulary variable &lit(NG_LOG_BCAST) should be set to 
	&lit(localhost) and the variable &lit(LOG_ACK_NEEDED) should be set
	to 1 for satellite direct logging to work properly.  Messages 
	received by the remote &lit(ng_parrot) process will be rebroadcast
	on the logging UDP port and recorded in the various master log 
	files across the cluster. 
space
&term	-T host : Has the same effect as the &lit(-t) option except that it 
	uses &ital(host) as the name of the external interface node rather 
	than building one from the cluster name.  This is generally meant 
	for testing, or emergency situations and is not intended to be 
	coded in any startup scripts. 
&endterms

&space
&see	socket(2), ng_log, ng_log(lib), ng_parrot, ng_sendudp

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 2 Apr 2001 (sd) : Original code.
&term	31 May 2001 (sd) : To make it use the cartulary variable for its port.
&term	25 Sep 2001 (sd) : To add support for adjunct file, and pause/resume 
	commands.
&term	26 Dec 2001 (sd) : To treat log filename on message as a cartulary var 
	if capatialised.
&term	05 May 2003 (sd) : Added check in parse to avoid issues is message is 
	truncated from the start (odd lead truncation).
&term	07 Jul 2003 (sd) : Corrected problem introduced on 5/5. Erk.
&term	28 Aug 2003 (sd) : Added support for direct logging to a repeater (parrot)
	via a tcp/ip session.
&term	03 Oct 2003 (sd) : To ensure sf files are closed properly at detach.
&term	15 Dec 2003 (sd) : Corrected use of cluster name when processing -t.
&term	16 Aug 2004 (sd) : Added %roll command. (version bumped to 2.3)
&term	29 Jul 2005 (sd) : Rolling of log file happens now even if the adjunct was closed (during pause).
&term	30 Oct 2006 (sd) : Converted to using ng_sfopen and checking sfclose() return.
&term	05 Feb 2007 (sd) : Corrected handling of chached messages (was chopping after sequence number).
&term	20 Feb 2007 (sd) : Fixed message length calc that was causing cached messages (saved when paused)
		to carry past the newline. (v2.4)
&endterms

&scd_end
#endif
