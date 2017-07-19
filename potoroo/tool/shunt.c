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
 -------------------------------------------------------------------------
  Mnemonic:	shunt - file exchanger (ng_rcp replacement)
  Abstract: 	two modes:	sends a file to a remote shunt daemon
				runs as a daemon listening for connections and 
				then receiving a file.

		Basic daemon protocol (invoked with -l option):
			listen on well known port
			accept session from remote ng_shunt (treated as command session)
			fork child who:
				closes parent listening port
				opens a listening port (for data connection)
				sends the port number back to remote end
				accepts data connection on secondary port
				accepts filename, user name, filemode on command session
				opens file as .filename
				receives data and writes to file 
				receives end of file on command session
				closes file
				renames file to filename
				sets the mode
				sets the user
				sends final status to partner
				close command and data sockets

		Basic send protocol:
			open random tcp port
			connect to remote shunt (connection used for control)
			receive port number that daemon opened for data session
			connect to remote shunt using data port
			if data connect fails
				open listening port
				send connect2me request to daemon
				wait for connection
			send file open request
			send file on data session
			send end of file indication on command session
			receive close file/stats information
			terminate

		Messages exchaned between shunt processes are expected to be null 
		terminated, not newline terminated.  This allows a newer version
		of shunt, one that uses flow manager to parse messages on the 
		command session, to properly receive multi message datagrams from 
		an older shunt.

		The timestamp on the destination file will reflect the time 
		that the file was coppied, not the time on the src file.

		session stats (time/date size transmt time) are logged
		if error, dest file is removed. 
			
  Date:		22 July 2003
  Author: 	E. Scott Daniels
 -------------------------------------------------------------------------
*/

#define _USE_BSD	/* believe it or not, needed to compile on linux */
#include <sys/types.h>          
#include <sys/ioctl.h>
#include <sys/socket.h>         

#include <net/if.h>
#include <stdio.h>             
#include <errno.h>            
#include <fcntl.h>           
#include <netinet/in.h>         /* inter networking stuff */
#include <signal.h>         
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>	/* needed for wait */
#include <sys/wait.h>
#include <pwd.h>
#include	<unistd.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_ext.h>
#include	<ng_socket.h>

#include	"shunt.man"

/* ---------------------------------------- */
extern	int ng_sierrno;		/* si library failure reason codes */

#define	FL_HIDDEN	0x01	/* we have already hidden ourselves away */
#define FL_SENDER	0x02	/* we are running in send mode if that matters */
#define FL_OPENED	0x08	/* dest file was opened ok -- send/receive allowed */
#define FL_OK2RUN	0x10	/* ok to do our thing while this is set */
#define FL_EXDISC	0x20	/* disconnects are expected and are not errors */
#define FL_CHILD	0x40	/* processes is a child */
#define FL_CLOSED	0x80	/* remote file was closed successfully */
#define FL_ALWAYS_C2M	0x100	/* always send a connect2me -- mostly for testing */
#define FL_UNKNOWN_ABORT 0x200  /* unknown command causes us to abort (default) */

#define FL_DEFAULTS 	FL_UNKNOWN_ABORT

#define BLEAT_N_ERR	LOG_ERROR * -1	/* these cause bleat to write to stdout and to send log msg */
#define BLEAT_N_WARN	LOG_WARN * -1	
#define BLEAT_N_INFO	LOG_INFO * -1

char	*version="v2.2/03268"; 
char	*argv0 = NULL;
int	verbose = 0;
int 	logging = 1;			/* -q sets to 0 and keeps us from writing to master log (sender only) */

/* we parallelise by concurrent processes, so we can maintain these globally rather than in a struct */
int	listen_sid = -1;		/* sid of the listen socket -- child receiver must close */
int	cmd_sid = -1;			/* session id of the command session */
int	data_sid = -1;			/* session id of the data session */
int	in_fd = -1;			/* fdes for input (local if copy) */
int	out_fd = -1;			/* file des of the output file (local if receive) */
int	flags = FL_DEFAULTS;		/* FL_ constants */
int 	blocksize = NG_BUFFER;		/* num bytes we read and send on each msg  (-b causes modification) */
int	block_count = 10;		/* num blocks sent between checks for cmd messages */
ng_int64	expected_bytes = -1;	/* bytes expected -- stays -1 until the sender has sent the last byte; it will likely beat the last data */

char	*timeout_msg = NULL;	/* message as to why we timedout */
int	conn_to = 5;			/* connection time out  */
char	*srcfn = NULL;			/* where we get */
char	*destfn = NULL;			/* where we put (final name) */
char	*tmp_destfn = NULL;		/* name we give the file while doing the copy */
char	*desthost = NULL;		/* destination host */
char	*destmode = "0664";		/* mode and user to set on close */
char	*destusr = "ningaui";
pid_t	mypid = -1;

				/* statistics things */
ng_timetype	began = 0;		/* timers for start/stop receive */
ng_timetype	ended = 0;
ng_timetype	xbegan = 0;		/* xmit timers -- no setup */
ng_timetype	xended = 0;
ng_int64	bytes = 0;		/* bytes received/sent */


Sfio_t	*bleatf = NULL;			/* file for bleats if running as daemon */


/* --------------------- misc things -------------------------------- */
void usage( )
{
	char *cp;

	if( (cp = strrchr( argv0, '/' )) != NULL )
		cp++;
	else
		cp = argv0;

	sfprintf( sfstderr, "%s %s\n", cp, version );
	sfprintf( sfstderr, "usage: %s [--man] [-p port] [-U] [-v] [-f] -l\n", argv0 );
	sfprintf( sfstderr, "\t %s [-b blksize] [-m mode] [-p port] [-U] [-u userid] [-q] [-v] host sourcefile destfile\n", argv0 );
	exit( 1 );
}


/* signal handler - alarms and dead kids */
void sigh( int s )
{
	switch( s )
	{
		case SIGCHLD:   
			while( waitpid( 0, NULL, WNOHANG ) > 0 );	/* non blocking wait for all dead kids */
			signal( SIGCHLD, &sigh ); 			/* must reissue or on irix/sun they do not get reset */
			break;	

		case SIGALRM:
			ng_bleat( 0, "shunt[%d]: timeout: %s", mypid, timeout_msg );
			ng_log( LOG_ERR, "%s abort due to timeout: %s", flags & FL_SENDER ? "sender" : "receiver", timeout_msg ? timeout_msg : "unknown state" );
			exit( 2 );
			break;

		default: break;
	}

}

void set_timeout( int sec, char *msg, char *dstring )
{
	char buf[2048];

	sfsprintf( buf, sizeof( buf ), "%s: %s", msg, dstring == NULL ? "" : dstring );
	if( timeout_msg )
		ng_free( timeout_msg );
	timeout_msg = ng_strdup( buf );
	alarm( sec );
}

void clear_timeout( )
{
	alarm( 0 );
	timeout_msg = ng_strdup( "no alarm set" );		/* should not see -- panic if we do */
}

/*	daemons should neither be seen nor heard; tuck ourself away neith the covers   */
void hide( )
{
	int i;
	char	*ng_root;
	char	lname[255];	/* log name */

	if( flags & FL_HIDDEN )
		return;            /* bad things happen if we try this again */

	switch( fork( ) )
	{
		case 0: 	break;        /* child continues on its mary way */
		case -1: 	perror( "could not fork" ); return; break;
		default:	exit( 0 );    /* parent abandons the child */
	}

	mypid = getpid();
	sfclose( sfstdin );
	sfclose( sfstdout);
	sfclose( sfstderr );
	for( i = 3; i < 255; i++ )		/* realy should suss out max */
		close( i );              	/* dont leave caller's files open */

	if( (ng_root = ng_env( "NG_ROOT" )) )
	{
		sfsprintf( lname, sizeof( lname ), "%s/site/log/shunt", ng_root );
		free( ng_root );
	}
	else
	{
		if( logging )
			ng_log( LOG_ERR, "unable to find ng_root in env" );
		exit( 1 );
	}

	if( (bleatf = ng_sfopen( NULL, lname, "a" )) != NULL )		/* open bleat file and set to sfstderr */
	{
		sfset( bleatf, SF_LINE, 1 );		/* force flush after each line */
		sfsetfd( bleatf, 2 );
		ng_bleat_setf( bleatf );	
		ng_bleat( 1, "shunt[%d]: successfully detached", mypid );
	}
	else
	{
		if( logging )
			ng_log( LOG_ERR, "unable to open bleat file: %s: (%s)", lname, strerror( errno ) );
		exit( 1 );
	}

	flags |= FL_HIDDEN;
}

/* close the file and get ready to bug out */
void decamp( )
{
	char	mbuf[256];
	struct	passwd *pwd;

	sfsprintf( mbuf, sizeof( mbuf ), "end of file received: %I*d(expected) %I*d(received)", sizeof( expected_bytes), expected_bytes,  sizeof( bytes ), bytes );
	ng_bleat( 2, "shunt[%d]: %s", mypid, mbuf );
	if( close( out_fd ) != 0 )		/* close error */
	{
		ng_bleat( 0, "shunt[%d]: close error, file to be unlinked: %s: %s", mypid, tmp_destfn, strerror( errno ) );
		ng_siprintf( cmd_sid, "abort unable to close file: %s", tmp_destfn, strerror( errno ) );
		out_fd = -1;				/* incase they send more */
		unlink( tmp_destfn );			/* trash it now */
	}
	else
	{
		out_fd = -1;				/* incase they send more */
		flags |= FL_CLOSED;

		chmod( tmp_destfn, (mode_t) strtol( destmode, NULL, 8 ) );
		if( pwd = getpwnam( destusr ) )
		{
			ng_bleat( 1, "shunt[%d]: changing owner to %s uid= %d gid=%d: %s", mypid, destusr, pwd->pw_uid, pwd->pw_gid, tmp_destfn  );
			chown( tmp_destfn, pwd->pw_uid, pwd->pw_gid );		/* likely to fail in linux */
		}
		else
			ng_bleat( 0, "shunt[%d]: unable to get pw entry for %s; owner not set %s", mypid, destusr, tmp_destfn );
	
		if( rename( tmp_destfn, destfn ) )		/* true is bad */
		{
			ng_bleat( 0, "shunt[%d]: rename %s --> %s failed: %s", mypid, tmp_destfn, destfn, strerror( errno ) );
			unlink( tmp_destfn );
			ng_siprintf( cmd_sid, "abort error renaming file: %s --> %s: %s", tmp_destfn, destfn, strerror( errno ) );
			return;
		}

		ng_siprintf( cmd_sid, "closed 0" );	/* 0 is a good return code */
	}

	flags |= FL_EXDISC;			/* disconnects are now ok  */
}

/*  establish a listen port and send a connect2me request to the server.  we do this when our 
    attempt to setup a data session has failed -- assuming that the firewall did not allow
    an inbound connection to the random port that shunt setup for data on the other end. 
*/
void init_c2m( )
{
	int	lsid = -1;				/* listen sid for this */
	char	*buf;
	char	*tok;
	char	hname[1000];


	ng_bleat( 1, "shunt[%d]: sending a connect2me request", mypid );

	if( (lsid = ng_silisten( 0 )) >= 0 )			/* open any random tcp port to send to user for a data connection */
	{
		hname[0] = 0;
        	ng_sysname( hname, sizeof( hname ) );

		buf = ng_sigetname( lsid );				/* get the name of the socket (address.port) */
		tok = strrchr( buf, '.' );				/* chop lead which is likely just 0.0.0.0 */
		tok++;
		ng_bleat( 2, "shunt[%d]: connnect2me listen port established: %s:%s", mypid, hname, tok );
		ng_siprintf( cmd_sid, "connect2me %s:%s", hname, tok );
		free( buf );
	}
	else
	{
		ng_siprintf( cmd_sid, "shunt: unable to open a connect2me listener: %s",  strerror( errno ) );
		ng_bleat( 0, "shunt[%d]: unable to open a connect2me lister: %s", mypid, strerror( errno ) );
		ng_siclose( cmd_sid );				/* reject session */
	}
}

/* ------------------------ command session buffer processing -------------------------- */
/*	process info received on the command session 
	all command buffers are assumed to be terminated with a null byte
	expected commands and parms are:
		closed status			( file was closed on the remote end, their final status; 0 is good)
		dataport ipaddress:port		( socket remote end has opened to accept a data connection on)
		dataend	byte-count		( sender has an accurate byte-count, end of file reached when this many received)
		openfile filename username mode	( sender wants this file opened, username and mode should be set on close)
		openok				( receiver has successfully opened senders file)
		abort message			( other processes has encountered an unrecoverable error and is terminating)

	the command dialogue is so request/response oriented that we make the assumption 
	that there will not be multiple commands in a single buffer. 
*/
void do_cmd( char *buf, int len )
{
	extern	int errno;

	int	n;
	char	abuf[256];	
	char	*dbuf;			/* duplicated buffer */
	char	*tok;

	if( strncmp( buf, "openfile", 8 ) == 0 )		
	{
		ng_bleat( 1, "shunt[%d]: opening file: %s", mypid, buf+9 );
		dbuf = strdup( buf + 9 );
		destfn = strtok( dbuf, " " );
		destusr = strtok( NULL, " " );
		destmode = strtok( NULL, " " );

		n = strlen( destfn );
		tmp_destfn = ng_malloc( n+3, "tmp_destfn" );
		sfsprintf( tmp_destfn, n+2, "%s!", destfn );		/* seems a lot of work, but bsr insisted file! exist while writing */
		tmp_destfn[n+2] = 0;						/* ENSURE it has an end of string */
		
		unlink( destfn );					/* no harm to trash it now */
		unlink( tmp_destfn );					/* must blow it away to get funky mode */

		if( (out_fd = open( tmp_destfn, O_CREAT|O_WRONLY|O_TRUNC, 0200 )) >= 0 )		/* funky mode while we write to it */
		{
			ng_bleat( 1, "shunt[%d]: successful open of local file for writing: %s (%02x)", mypid, tmp_destfn, flags&FL_OK2RUN );
			ng_siprintf( cmd_sid, "openok" );
			flags |= FL_OPENED;
		}
		else
		{
			ng_bleat( 1, "shunt[%d]: unable to open local file for writing: %s: %s", mypid, tmp_destfn, strerror( errno ) );
			ng_siprintf( cmd_sid, "abort unable to open file: %s: %s", tmp_destfn, strerror( errno ) );
			flags &= ~FL_OK2RUN;
		}

		return;
	}
	else
	if( strncmp( buf, "openok", 6 ) == 0 )		/* other end has opened the output file */
	{
		ng_bleat( 2, "shunt[%d]: file open ack received from remote side", mypid );
		flags |= FL_OPENED;			/* there might be info to dig later */

		return;
	}
	else
	if( strncmp( buf, "dataport", 8 ) == 0 )		/* the address we are to connect to for sending our data */
	{
		ng_bleat( 2, "shunt[%d]: data channel ip address received: %s", mypid, buf+8 );
		if( flags & FL_ALWAYS_C2M )			/* always send a connect2me request -- never try to get to their dataport */
		{
			init_c2m();				/* set our listener, send the request and then we wait */
			return;
		}

		if( tok = strrchr( buf + 8, '.' ) )		/* rip off the port so we can use symbolic name supplied on cmd line */
		{
			sfsprintf( abuf, sizeof( abuf ), "%s:%s", desthost, tok+1 );
			tok = abuf;
		}
		else
		{
			sfsprintf( abuf, sizeof( abuf ), "%s:%s", desthost, buf+9 );	/* assume its just the port number */
			tok = abuf;
			
		}
		if( (data_sid = ng_siconnect( abuf ) ) >= 0 ) 			/* connect to it */
			ng_bleat( 2, "shunt[%d]: data channel established %s; sid=%d", mypid, tok, data_sid );
		else
		{
			ng_bleat( 0, "shunt[%d]: unable to establish data channel with %s: %s", mypid, tok, strerror( errno ) );
			init_c2m( );			/* setup a listen port and send a connect2me message */
			/*flags &= ~FL_OK2RUN; */
		}

		return;
	}
	if( strncmp( buf, "connect2me", 10 ) == 0 )	/* sender needs us to establish conn back for data -- firewall etc preventing them */
	{
		ng_bleat( 1, "shunt[%d]: request for us to establish data channel: %s", mypid, buf );
		tok = buf + 11;					/* other side must send ipaddr:port */
		
		if( data_sid >= 0 )
			ng_siclose( data_sid );			/* close the data listen port we opened in anticipation of their conn */

		if( (data_sid = ng_siconnect( tok ) ) >= 0 ) 			/* connect to the sender's data listen port */
		{
			ng_bleat( 2, "shunt[%d]: data channel established back to client %s; sid=%d", mypid, tok, data_sid );
		}
		else
		{
			ng_bleat( 0, "shunt[%d]: unable to establish data channel back to client: %s: %s", mypid,  tok, strerror( errno ) );
			flags &= ~FL_OK2RUN;
		}

		return;
	}
	else
	if( strncmp( buf, "closed", 6 ) == 0 )				/* receiver closed file -- their final status is here */
	{
		flags |= FL_EXDISC + FL_CLOSED;				/* disconnects are expected and not errors */
		if( (n = atoi( buf + 7 )) )				/* expect !0 status to be bad */
			flags &= ~FL_OK2RUN;				/* will cause us to abort things */
		ng_bleat( 2, "shunt[%d]: final remote status: %d (%s)", mypid, n, n ? "bad" : "good" );

		return;
	}
	else
	if( strncmp( buf, "dataend", 7 ) == 0 )		/* sender has sent last message, it still may be queued behind this */
	{

		sfsscanf( buf+8, "%I*d", sizeof( expected_bytes ), &expected_bytes );
		if( bytes >= expected_bytes )		/* already received the declared amount; get out now */
		{
			ng_bleat( 2, "shunt[%d]: received end of data, bytes expected=%I*d  bytes received=%I*d", mypid, Ii(expected_bytes), Ii(bytes) );
			decamp( );
		}
		else
			ng_bleat( 2, "shunt[%d]: end is near: %s expected=%I*d have=%I*d", mypid, buf, Ii(expected_bytes), Ii(bytes) );

		return;
	}
	else
	if( strncmp( buf, "abort", 5 ) == 0 )
	{
		ng_bleat( 0, "shunt[%d]: abort received: %s", mypid, buf+6 );
		flags &= ~FL_OK2RUN;					/* will cause us to abort things */

		return;
		
	}
	if( strncmp( buf, "verbose", 7 ) == 0 )		/* allows a child session to be set into a higher verbose mode to match sender */
	{
		int v;

		v = atoi( buf+8 );
		if( v >= verbose && v < 10 )
		{
			verbose = v;
			ng_bleat( 0, "shunt[%d]: verbose mode changed to %d", mypid, verbose );
		}
		else
			ng_bleat( 0, "shunt[%d]: verbose mode change ignored, request (%d) lower than current (%d)", mypid, v, verbose );
	}
	else
	{
		ng_bleat( 0, "shunt[%d]: unknown command received%s: %s", mypid, flags & FL_UNKNOWN_ABORT ? " FATAL" : "",  buf );
		if( flags & FL_UNKNOWN_ABORT )
			flags &= ~FL_OK2RUN;		/* will cause things to drop like a hot potato -- assume attack */
		
	}

}

/* ------------------------------ sending specific things --------------------------- */

/* send a block of stuff  from file (fd) to socket (sid) */
int send_block( int fd, int sid )
{
	char *buf[NG_BUFFER];
	int n = block_count;			/* number to send before returning */
	int got;				/* actual bytes read from disk */

	while( n-- && (got = read( fd, buf, blocksize )) > 0 )
	{
		bytes += got;
		ng_sisendtb( sid, buf, got );		/* send -- allowed to block */
	}

	if( got < 0 )				/* error */
	{
		if( logging )
			ng_log( LOG_ERR, "read error on input file: %s", strerror( errno ) );
		ng_bleat( 0, "send_block: read error on input file: %s", strerror( errno ) );
		flags &= ~FL_OK2RUN;			/* causes main to send an abort */
	}

 	return got > 0 ? 1 : 0;					/* value of last read -- 0 means we are done */
}

/* 	expect argv to be host filename destfilename */
/* 	returns 0 == good */
int send_file( int argc, char **argv, int port, char *comment )
{
	char	address[256];			/* address of remote host */
	char	mbuf[NG_BUFFER];		/* buffer to build msg in */
	int	ifd;				/* the file that we are reading */
	int	ec = 0;				/* exit code */


	desthost= argv[0];
	srcfn = argv[1];
	destfn = argv[2];

	xended = xbegan = began = ng_now( );	/* xbegan/end are reset later; keeps stats nice if we dont connect */

	if( (ifd = open( srcfn, O_RDONLY )) < 0 )
	{
		ng_bleat( 0, "shunt[%d]: local file open failed: %s: %s", mypid, srcfn, strerror( errno ) );
		exit( 2 );
	}

	sfsprintf( address, sizeof( address ), "%s:%d", desthost, port );	/* address for command interface */
	set_timeout( conn_to, "while connecting", "address" );
	if( (cmd_sid = ng_siconnect( address ) ) >= 0 )
	{
		clear_timeout( );
		flags |= FL_OK2RUN;				/* any error/signal will drop this and we will cleanup */

		ng_siprintf( cmd_sid, "verbose %d", verbose );	/* set the receiver verbose level to match ours if ours is larger */

		set_timeout( 90, "waiting for data channel to establish", NULL );
		while( data_sid < 0 )		/* until we have a data connection, or timeout pops and we abort */
		{
			ng_sipoll( 90000 );	/* wait for a dataport message and us to connect to it, or  for their conn if we send conn2me */
		}
		clear_timeout( );


		/* data channel address was received during poll, and a connection was established  send the open command */
		ng_bleat( 2, "shunt[%d]: sending file open request: openfile %s %s %s", mypid, destfn, destusr, destmode );
		ng_siprintf( cmd_sid, "openfile %s %s %s", destfn, destusr, destmode );		/* mode and user may come in later */

		set_timeout( 300, "waiting for successful remote file open", destfn );
		ng_sipoll( 30000 );			/* wait 300 seconds for an open ok message to come back */
		if( ! (flags & FL_OPENED) )
		{
			ng_bleat( 0, "shunt[%d]: unable to open remote file: %s", mypid, destfn );
			exit( 2 );
		}

		ng_bleat( 2, "shunt[%d]: file transmission begins", mypid );
		xbegan = ng_now( );					/* start of transmission - measure setup time */
		set_timeout( 300, "while sending block", NULL );
		while( (flags & FL_OK2RUN) && send_block( ifd, data_sid ) )		/* send a block of data */
		{
			set_timeout( 300, "while sending block", NULL );		/* reset with each block snt */
			ng_sipoll( 0 );				/* poll for command session traffic after every block -- no wait */
		}
		clear_timeout( );
		ng_sipoll( 1 );				/* one last check for errors on data receipt */
		ng_bleat( 2, "shunt[%d]: file transmission ended ok2run=%s", mypid, (flags & FL_OK2RUN) ? "yes" : "no" );

		xended = ng_now( );			

		close( ifd );
		if( flags & FL_OK2RUN )			/* sent all data without errors */
		{
			ng_bleat( 2, "shunt[%d]: sending dataend, waiting for final status", mypid );
			sfsprintf( mbuf, sizeof( mbuf ), "dataend %I*d", sizeof( bytes ), bytes );
			ng_siprintf( cmd_sid, "%s", mbuf  );		/* let them know to clean up */

			ng_sipoll( 30000 );				/* wait some for the final status */ 

			ended = ng_now( );			
		}
		else
		{
			ng_bleat( 2, "shunt[%d]: things not ok, sending abort", mypid );
			ng_siprintf( cmd_sid, "abort" );		/* ensure that they trash the file */
		}

		ng_bleat( 2, "shunt[%d]: dropping sessions", mypid );
		ng_siclose( data_sid );
		ng_siclose( cmd_sid );
	}
	else
		ng_bleat( 1, "shunt[%d]: unable to connect to: %s(%d)", mypid, address, ng_sierrno );

	
			
	if( (flags & FL_OK2RUN) && (flags & FL_CLOSED) )		/* still ok, and we got a closed ok message */
		ec = 0;
	else
	{
		ng_bleat( 2, "shunt[%d]: flags at failure = 0x%x", mypid, flags );
		ec = 1;					/* failure */
	}

	sfsprintf( mbuf, sizeof( mbuf ), "stats: %s => %s:%s  %d(rc) %I*d(bytes) %ld(elapsed) %ld(setup) # %s", srcfn, desthost, destfn, ec, sizeof( bytes ), bytes, (long) (xended - xbegan), (long) (xbegan - began), comment );
	ng_bleat( 1, "shunt[%d]: %s", mypid,  mbuf );				/* both private log and master log please */
	if( logging )
		ng_log( LOG_INFO, "%s", mbuf );

	ng_bleat( 2, "shunt[%d]: send returning: %d [%s]", mypid,  ec, ec ? "FAIL" : "OK" );
	return ec;

}


/* ------------------------------- receive specific things -------------------------- */


/* called when an initial connection is received.  We fork and setup to receive the file */
void create_receiver( int sid )
{
	extern int errno;
	char	*buf;
	int	cpid=-99;
	int 	o_sid;			/* old listen sid */

	o_sid = listen_sid;		/* we overload listen_sid, but need to keep original value til end when it can be closed */

	switch( (cpid=fork( )) )
	{
		case 0: 	break;        		/* child continues on its mary way */

		case -1: 	ng_bleat( 0, "shunt[%d]: could not fork: %s", mypid, strerror ( errno ) ); 
				ng_siclose( sid );	/* reject the session */
				return; 
				break;

		default:	
				ng_siclose( sid );		/* session is for the child to play with  */
				ng_bleat( 2, "shunt[%d]: parent continues child=%d", mypid, cpid );
				return; 	   		/* just go back to what we were doing -- listening */
	}

	mypid = getpid();					/* set child pid now */
	flags |= FL_CHILD | FL_OK2RUN;
	ng_bleat( 2, "shunt[%d]: child initialising", mypid );
	cmd_sid = sid;							/* save it now, not in parent */

	if( (listen_sid = ng_silisten( 0 )) >= 0 )			/* open any random tcp port to send to user for a data connection */
	{
		buf = ng_sigetname( listen_sid );				/* get the name of the socket (address.port) */
		ng_bleat( 1, "shunt[%d]: data listener established: %s", mypid, buf );
		ng_siprintf( cmd_sid, "dataport %s", buf );
		free( buf );
	}
	else
	{
		ng_siprintf( cmd_sid, "shunt: child is unable to open a listener for data connection: %s",  strerror( errno ) );
		ng_bleat( 0, "shunt[%d]: child is unable to open a data listen port: %s", mypid, strerror( errno ) );
		ng_siclose( sid );				/* reject session */
	}

	ng_siclose( o_sid );	/* if we close it before opening new listen things hang; si bug? */
}

/* driven by cbtcp to deal with data received on the session */
void receive( char *buf, int len )
{
	/*extern int errno;*/
	int n; 
	char	ebuf[1024];

	if( out_fd < 0 )
		ng_bleat( 0, "shunt[%d]: data received %d bytes --- ignored -- file not open!", mypid, len );
	else
	{
		if( (n = write( out_fd, buf, len )) != len )
		{
			sfsprintf( ebuf, sizeof( ebuf ),  "%s",  strerror( errno ) );
			ng_bleat( 0, "shunt[%d]: error writing to file: %s: %d(n) != %d(len): %s", mypid, tmp_destfn, n, len, ebuf );
			close( out_fd );
			unlink( tmp_destfn );			/* trash it now */

			ng_siprintf( cmd_sid, "abort error writing to file: %s(%s): %d(n) != %d(len) %s", destfn, tmp_destfn, n, len, ebuf );
			out_fd = -1;				/* prevent issues with anything left buffered */
		}
		else
			bytes += len;
	}

	if( expected_bytes >= 0 && bytes >= expected_bytes )
		decamp( );
}


/* ------------------------------- callback things ---------------------------------- */
int cbtcp( void *data, int fd, char *buf, int len )			/* driven when tcp data arrives */
{
	void	*flow_mgt = NULL;		/* pointer to flow info to manage input messages */
	char	*rec; 



	if( fd == cmd_sid )
	{
		if( !flow_mgt )
			flow_mgt = ng_flow_open( 1024 );		/* establish a flow input buffer for the session */

		ng_flow_ref( flow_mgt, buf, len );              	/* regisger new buffer with the flow manager */
		while( rec = ng_flow_get( flow_mgt, 0 ) )		/* get all complete, null terminated records */
		{
			ng_bleat( 3, "shunt[%d]: cooked: fd=%d cmd=%s", mypid,  fd, rec );
			do_cmd( rec, strlen(rec) );
		}
	}
	else
	if( fd == data_sid )
		receive( buf, len );		/* receive will send errors on command session if needed */
	else
		ng_bleat( 0, "shunt[%d]: buffer from unrecognised sid received sid=%d (%d)", mypid, fd, mypid );		/* should not happen */
		
	if( ! (flags & FL_OK2RUN ) )
	{
		if( out_fd >= 0 )
			close( out_fd );
		unlink( tmp_destfn );			/* trash it now */
	}

	return (flags & FL_OK2RUN) ? SI_RET_OK : SI_RET_QUIT;		/* force ng_siwait() to return to us */
}


/* 	driven for initial connection: we fork and close the listening port, opening one for a data connection
	driven after data connection listen port is up, we simply record the data session and go on.
*/
int cbconn( char *data, int sid, char *rmt_addr )
{
	ng_bleat( 2, "shunt[%d]: connection request received %d(fd) from: %s ", mypid, sid, rmt_addr );
	if( cmd_sid < 0 )
		create_receiver( sid );		/* fork and establish the receiver in the child; parent just continues to listen */
	else
		data_sid = sid;


	return SI_RET_OK;
}

/* deal with the loss of a session  */
int cbdisc( char *data, int sid )		/* disconnect received */
{
	ng_bleat( 2, "shunt[%d]: disconnect received for sid=%d", mypid, sid );
	if( flags & FL_SENDER )
	{
		if( !( flags & FL_EXDISC ) )
		{
			ng_bleat( 0, "shunt[%d]: disconnect was not expected", mypid );
			exit( 3 );
		}
		
	}
	else
	{
		if( !( flags & FL_EXDISC ) )
		{
			ng_bleat( 0, "shunt[%d]: disconnect was not expected, unlink file: %s (%d)", mypid, tmp_destfn ? tmp_destfn : "no open file", mypid );
			if( tmp_destfn )
				unlink( tmp_destfn );
			ng_siclose( data_sid );
			ng_siclose( cmd_sid );
			exit( 4 );
		}

	}

	return SI_RET_QUIT;			
}

int main( int argc, char **argv )
{

	const	char *cport;		/* pointer to var string from cartulary */
	char	*comment = "";		/* user supplied id that goes at end of stats message */
	char	*cp;
	int	port = -1;		/* port to open */
	int	be_listener = 0; 		/* background  (listener) */
	int	detach = 1;			/* detach if listener */
	int	status = 0;

	if( (cport = ng_env_c( "NG_SHUNT_PORT" )) != NULL )
		port = atoi( cport );				/* pick up default port number from cartulary */
	
	ARGBEGIN
	{
		case 'a':	flags |= FL_ALWAYS_C2M; break;		/* always send connect2me */
		case 'b':	IARG( blocksize ); break;
		case 'c':	SARG( comment ); break;
		case 'f':	detach = 0; break;
		case 'l':	be_listener = 1; break;				/* started as listener - move to background */
		case 'm':	SARG( destmode ); break;
		case 'p':	IARG( port ); break;
		case 't':	IARG( conn_to ); break;			/* connection timeout */
		case 'u':	SARG( destusr ); break;			/* try to set user name to this on the other end */
		case 'U':	flags &= ~FL_UNKNOWN_ABORT;		/* turn off flag that causes abort when unk cmd received */
		case 'q':	logging = 0;	break;			/* no ng_log messages */
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
				break;
	}
	ARGEND

	mypid = getpid();

	if( port < 0 )
	{
		ng_bleat( 0, "could not determine port (missing -p and NG_SHUNT_PORT not in cartulary)" );
		exit( 1 );
	}


	if( blocksize > NG_BUFFER )		/* enforce sanity */
		blocksize = NG_BUFFER;
	else
		if( blocksize < 1024 )
			blocksize = 1024;

	ng_siprintz( 1 );		/* send end of string when using siprintf */

	if( be_listener )						
	{
		if( detach )
		{
			logging = 1;			/* always logging if hidden */
			hide( );						/* MUST hide before starting network if */
		}

		ng_bleat( 0, "started; %s (%d)", version, mypid );		/* after hide so they go to the log */

		if( ng_siinit( SI_OPT_ALRM | SI_OPT_FG, -1, -1 ) == SI_OK )		/* dont ask for a tcp port; use listen later */
		{
			listen_sid = ng_silisten( port );				/* open here so we can get sid to close in children */
			ng_bleat( 1, "listening on port: %d", port );

			ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    			/* register rtn driven on connection */
			ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    			/* register rtn driven on session loss cb */
			ng_sicbreg( SI_CB_CDATA, &cbtcp, (void *) NULL );		/* tcp (cooked data) */
			/*  ng_sicbreg( SI_CB_SIGNAL, &cbsig, (void *) NULL );*/		/* called for singals */

			signal( SIGCHLD, &sigh ); 

			status = ng_siwait( );			/* socket interface stuff gets to drive -- all work done as result of callback */

			ng_bleat( 2, "shunt[%d]: %s is exiting %d(status) %d(sierror)", mypid, flags & FL_CHILD ? "CHILD" : "PARENT", status, ng_sierrno );
			ng_sishutdown( );          		/* for right now just shutdown */
	
		}
		else
			ng_bleat( 0, "shunt[%d]: unable to initialise network for tcp on port: %d (%d)", mypid, port, ng_sierrno );
	}
	else
	{
		status = 1;			/* assume bad */
		if( argc < 3 )			/*  three parms required to send */
		{
			usage( );
			exit( 1 );
		}
	
		if( (cp = ng_env( "NG_SHUNT_C2M")) != NULL )			/* if set, force connect2me mode */
			if( strcmp( cp, "yes" ) == 0 || *cp == '1' )
				flags |= FL_ALWAYS_C2M;

		flags |= FL_SENDER;


		if( ng_siinit( SI_OPT_ALRM | SI_OPT_FG, -1, -1 ) == SI_OK )		/* no ports open for sender */
		{
			ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    			/* register rtn driven on connection */
			ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    			/* register rtn driven on session loss cb */
			ng_sicbreg( SI_CB_CDATA, &cbtcp, (void *) NULL );		/* tcp (cooked data) */
			/*ng_sicbreg( SI_CB_SIGNAL, &sigh, (void *) NULL ); */		/* called for singals */


			status = send_file( argc, argv, port, comment );
		}
		else
			ng_bleat( 0, "shunt: unable to initialise: err=%d", ng_sierrno );
	}

	return  status;		/* shell expects 0 to be good */
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_shunt:File Exchanger)

&space
&synop	ng_shunt -l [-p port] [-U] [-v]	&break
	ng_shunt [-b blksize] [-c comment] [-m mode] [-p port] [-t conn-timeout] [-U] [-u user] [-q] [-v] host srcfile destfile

&space
&desc	&This causes &ital(srcfile) to be sent to &ital(host) and placed into &ital(destfile.)
	The destination filename must be fully qualified or odd results will occur. 
	The source file is transferred using the &this daemon running on the remote host, and 
	provides masterlog statistics about the transfer (filenames, and data transfer rates), 
	in addition to ensuring that remote files are removed in the event of an error.  
&space
	The modification time of the destination file will be set to the 
	time of the transfer, not the timestamp that is on the source file. If possible, file 
	mode and ownership will be set as requested (this may require that the &this daemon on 
	the remote host be running as a set-uid root programme).

&space
&subcat	Listener Mode
	When listening mode (-l) is specified, &this becomse a daemon that accepts sessions from 
	other processes to receive and save files. 
	Usually, the remote process is a command line invocation of &this, however processes 
	like &lit(ng_lambast) also use the shunt protocol, and thus can send files to a node 
	using &this to receive them.
 
&space
&opts	 The following commandline options are recognised. 

&begterms
&term	-a : Always use connect to client mode (similar to active FTP). 
	When this flag is set, &this does not attempt to connect to the daemon via the 
	randomly selected listen port, but sends a request for the daemon to connect back
	to it.  If this flag is not supplied, &this will attempt to connect to the data 
	port (like passive FTP), and if that fails &this will send a 'connect to me' request
	asking that the daemon establish the connection for data traffic.  Use of this flag
	should be needed only for testing, or in cases where it is known that a firewall 
	of somesort will block connection attempts to the remote host on random ports. The 
	latter will still work without this flag; the flag only serves to make the attempt
	more efficent. 
&space
&term	-b bytes : Supplies an alternate block size.  &This will use the block 
	size when reading from the disk, and will attempt to send this number 
	of bytes on each TCP/IP message. 
&space
&term	-c string : A comment string that is appended to the statistics message that
	is written to the master log. This allows transfer statstics to be classified
	by the sending application.
&space
&term	-f : Interactive (foreground) mode.  This applies only when the listen flag (-l) 
	is set, and prevents &this from detaching from the tty device. 
&space
&term	-l : Listen mode. When this option is specified &this becomes a daemon 
	and listens for requests from other nodes to receive files. 
&space
&term	-m mode : Provides the file mode to be set on the remote host. If not 
	supplied 0664 is used. While the file is being created it is given a 
	bizarre mode of 0200.
	This option is ignored when listener mode is indicated. 
&space
&term	-p n : Specifies the port number that &this should listen on, or send to.
&space
&term	-t sec : Indicates the maximum number of seconds that &this will wait while
	a connection to the remote node is pending.
	This option is ignored when listener mode is indicated. 
&space
	-q : Quiet, no logging mode.  Messages are not written to the master log
	when sending.  
	This option is ignored when listener mode is indicated. 
&space
&term	-u username : &This will attempt to change the file ownership such that 
	the supplied username owns the destination file. If not supplied &lit(ningaui)
	is assumed. (&this must be installed as a suid-root programme in most 
	instances for this to work)
	This option is ignored when listener mode is indicated. 
&space
&term	-U : Causes unknown commands received to be ignored, but not to cause the sessions
	to be disconnected and the transfer aborted.  By default, unknown commands received
	by either the sender or receiver are assumed to be an attack and cause the transfer
	to be aborted. 
&space
&term	-v : verbose mode. When sending, two &lit(-v) options produce a session 
	exchange dialogue between sender and remote listener. The verbose level indicated 
	on the command line is also sent to the remote &this process in order to cause 
	the remote process to match the verbosity level for the transfer.  Messages generated
	by the remote &this are written into the log on the remote node. 
&endterms

&parms	When in listen mode (-l), no command line positional parameters are expected.
	Any supplied will be ignored. The following positional parameters are required
	when started in sending mode:
&space
&begterms
&term	host : The name of the host to send the file to.
&term	src : The name of the file to send.
&term	dest : The name the file should be given on the remote host. 
&endterms
&space

&space
&exit	&This will exit with a non-zero return code if it is not able to copy the file 
	correctly. When running as the node listener, log messages are writen to 
	&lit($NG_ROOT/site/log/shunt).

&space
&see	ng_ccp, ng_lambast

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	23 Jul 2003 (sd) : Original code. 
&term	05 Oct 2003 (sd) : To ensure sf files are closed properly at detach.
&term	12 Dec 2003 (sd) : Added check for never getting close message -- timeout rather
	than disco.
&term	26 Dec 2003 (sd) : Added better check for read errors (v1.2).
&term	07 Sep 2004 (sd) : Added temp dest name of file! then renamed to file after closed.
&term	11 Oct 2004 (sd) : Added comment string capability.
&term	02 Nov 2004 (sd) : Changed sigh() to exit on alarm signal, after writing timeout msg rather
		than turning off ok2run flag. 
&term	15 Jun 2005 (sd) : Could not connect is now written only in verbose mode. 
&term	01 Aug 2005 (sd) : eliminated gcc warnings.
&term	25 Jan 2005 (sd) : Reset signal in signal handler to prevent issues on sun/irix
&term	08 May 2006 (sd) : Added more info on abort messages in receive(). 
&term	23 May 2006 (sd0 : Corrected order of parms passed on an error siprintf() call (caused coredump
		bug 774. (v1.4)
&tem	30 May 2006 (sd) : Now closes/unlinks output file before attempting to send status back to 
		source. We had issues with the status send causing a coredump and leaving the dest file 
		hanging. (v1.5)
&term	21 Nov 2006 (sd) : Converted to ng_sfopen().
&term	18 May 2007 (sd) : Added an 'active like' mode that causes the client shunt to send a connect2me
		request for a data connection if it is rejected when trying to connect to the remote 
		shunt's random listen port. (v2.0)
&term	27 Jun 2007 (sd) : Fixed bug that causes both sides to hang when the daemon is asked to connect back 
		to the sending process and there is a connection timesout.   Added pid to messages written to 
		private log. Sending side now sends a verbose message to the receiver (child) which will 
		use the sender's verbose level if it is greater.  (V2.1)
$term	05 Jul 2007 (sd) : Converted to using flow manager to deal with multiple commands in a single tcp message. 
		The messages are assumed to be null terminated rather than newline terminated so it is 
		backward compatable with older versions.
&term	26 Mar 2008 (sd) : Added more details to timeout messages and they are now logged in the masterlog.
&endterms

&scd_end
------------------------------------------------------------------ */
