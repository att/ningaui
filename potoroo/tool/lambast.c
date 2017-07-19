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
  Mnemonic:	lambast - send a file with a desired transmission rate or
			taking as close to a specific number of seconds as 
			possible
  Abstract:  	We open and connect to shunt on the dest host and write the 
		file using the indicated rate, or a rate calcuulated based 
		on the file size and the desired transmission time. 
		Stats are reported (if -s is on) to the standard output
		device and indicate the number of kbytes written, the 
		elapsed time (seconds), the transfer rate and the 
		percent error of that rate from the desired rate. 
		Min and max transfer rates are given based on 4 second 
		samples during the process. 
			
  Date:		24 January 2006
  Author: 	E. Scott Daniels
 -------------------------------------------------------------------------
*/

#define _USE_BSD	/* believe it or not, needed to compile on linux */
#include <sys/types.h>          /* various system files - types */
#include <sys/ioctl.h>
#include <sys/socket.h>         /* socket defs */
#include <sys/stat.h>
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
#include <pwd.h>
#include	<unistd.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>
#include	<ng_socket.h>

#include	"lambast.man" 

/* ---------------------------------------- */
extern	int ng_sierrno;		/* si library failure reason codes */

#define	FL_SENDER	0x01	/* we are running in send mode if that matters */
#define FL_OPENED	0x02	/* dest file was opened ok -- send/receive allowed */
#define FL_SEND_RANDOM	0x04	/* send random crap from a buffer -- no real input file */
#define FL_OK2RUN	0x10	/* ok to do our thing while this is set */
#define FL_EXDISC	0x20	/* disconnects are expected and are not errors */
#define FL_CHILD	0x40	/* processes is a child */
#define FL_CLOSED	0x80	/* remote file was closed successfully */

#define BLEAT_N_ERR	LOG_ERROR * -1	/* these cause bleat to write to stdout and to send log msg */
#define BLEAT_N_WARN	LOG_WARN * -1	
#define BLEAT_N_INFO	LOG_INFO * -1

char	*random_crap = NULL;

int	srate = 0;			/* desired send rate in Kbytes/second  -r to set from command line */
int	swindow = 0;			/* sending time window -t to set from command line (overrides -r) */

long 	max_rate = 0;			/* stuff to give stats from at end */
long	min_rate = 32767;

char	*argv0 = NULL;
int	verbose = 0;
int	gen_stats = 0;
ng_int64 fsize = 0;			/* size of file to compute rate when given a time */

char	*version="v1.0/01247"; 

int	cmd_sid = -1;			/* session id of the command session */
int	data_sid = -1;			/* session id of the data session */
int	in_fd = -1;			/* fdes for input (local if copy) */
int	out_fd = -1;			/* file des of the output file (local if receive) */
int	flags = 0;			/* FL_ constants */
int 	blocksize = NG_BUFFER;		/* num bytes we read and send on each msg  (-b causes modification) */
int	block_count = 10;		/* num blocks sent between checks for cmd messages */

char	*timeout_msg = "uninitalised";	/* message as to why we timedout */
int	conn_to = 5;			/* connection time out  */
char	*srcfn = NULL;			/* where we get */
char	*destfn = NULL;			/* where we put (final name) */
char	*tmp_destfn = NULL;		/* name we give the file while doing the copy */
char	*desthost = NULL;		/* destination host */
char	*destmode = "0664";		/* mode and user to set on close */
char	*destusr = "ningaui";

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
	sfprintf( sfstderr, "%s %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-s] [-T conn-sec] [-v] {-p port | -P} {-t sec | -r rate} host srcfile destfile\n", argv0 );
	sfprintf( sfstderr, "       %s [-s] [-T conn-sec] [-v] {-p port | -P} {-t sec | -r rate} -R size host destfile\n", argv0 );
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
			ng_bleat( 0, "lambast: timeout: %s", timeout_msg );
			exit( 2 );
			break;

		default: break;
	}

}

void set_timeout( int sec, char *msg )
{
	timeout_msg = msg;
	alarm( sec );
}

void clear_timeout( )
{
	alarm( 0 );
	timeout_msg = "no alarm set";		/* should not see -- panic if we do */
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

	the command dialogue is very request/response oriented that we make the assumption 
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
		ng_bleat( 1, "lambastd: opening file: %s", buf+9 );
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
			ng_bleat( 1, "lambastd: successful open of local file for writing: %s", tmp_destfn );
			ng_siprintf( cmd_sid, "openok" );
			flags |= FL_OPENED;
		}
		else
		{
			ng_bleat( 1, "lambastd: unable to open local file for writing: %s", tmp_destfn );
			ng_siprintf( cmd_sid, "abort unable to open file: %s: %s", tmp_destfn, strerror( errno ) );
			flags &= ~FL_OK2RUN;
		}

		return;
	}
	else
	if( strncmp( buf, "openok", 6 ) == 0 )		/* other end has opened the output file */
	{
		ng_bleat( 2, "lambast: file open ack received from remote side" );
		flags |= FL_OPENED;			/* there might be info to dig later */

		return;
	}
	else
	if( strncmp( buf, "dataport", 8 ) == 0 )		/* the address we are to connect to for sending our data */
	{
		ng_bleat( 2, "lambast: data channel ip address received: %s", buf+8 );
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
			ng_bleat( 2, "lambast: data channel established %s; sid: %d", tok, data_sid );
		else
		{
			ng_bleat( 2, "lambast: unable to establish data channel with %s", tok );
			flags &= ~FL_OK2RUN;
		}

		return;
	}
	else
	if( strncmp( buf, "closed", 6 ) == 0 )				/* receiver closed file -- their final status is here */
	{
		flags |= FL_EXDISC + FL_CLOSED;					/* disconnects are expected and not errors */
		if( (n = atoi( buf + 7 )) )				/* expect !0 status to be bad */
			flags &= ~FL_OK2RUN;				/* will cause us to abort things */
		ng_bleat( 2, "lambast: final remote status: %d (%s)", n, n ? "bad" : "good" );

		return;
	}
	else
	if( strncmp( buf, "dataend", 7 ) == 0 )
	{
		ng_bleat( 0, "internal error: dataend message receive; not expected as we are not receiver!" );
		exit( 1 );
		return;
	}
	else
	if( strncmp( buf, "abort", 5 ) == 0 )
	{
		ng_bleat( 0, "lambast: abort received: %s", buf+6 );
		flags &= ~FL_OK2RUN;					/* will cause us to abort things */

		return;
		
	}
	else
		ng_bleat( 0, "lambast: unknown command received: %s", buf );

}

/* ------------------------------ sending specific things --------------------------- */

/* send a block of stuff  from file (fd) to socket (sid) */
int send_block( int fd, int sid )
{
	static	at_eof = 0;
	char *buf[NG_BUFFER];
	int n = block_count;			/* number to send before returning */
	int got;				/* actual bytes read from disk */

	if( flags & FL_SEND_RANDOM )
	{
		if( ! at_eof )
			at_eof = fsize; 	/* number of bytes to send */

		while( n-- && at_eof > 0 )
		{
			got = at_eof > NG_BUFFER ? NG_BUFFER : at_eof;
			bytes += got;
			ng_sisendtb( sid, random_crap, got );
			at_eof -= NG_BUFFER;
		}

		return at_eof > 0;			/* non-zero means keep going */
	}

	while( n-- && (got = read( fd, buf, blocksize )) > 0 )
	{
		bytes += got;
		ng_sisendtb( sid, buf, got );		/* send -- allowed to block */
	}

	if( got < 0 )				/* error */
	{
		ng_bleat( 0, "send_block: read error on input file: %s", strerror( errno ) );
		flags &= ~FL_OK2RUN;			/* causes main to send an abort */
	}

 	return got > 0 ? 1 : 0;					/* value of last read -- 0 means we are done */
}

/* compute the overall tranfer rate, capture some info for end stats */
int cur_rate( )
{
	static ng_timetype chat = 0;			/* next time we chat if in verbose mode */
	static ng_timetype last_msg = 0;
	static ng_int64	last_bytes = 0;


	ng_timetype 	now = 0;
	ng_timetype	elap = 0;
	int		r;
	
	now = ng_now();

	if( last_msg == 0 )
	{
		last_msg = now;
		chat = now + 30;
	}
	

	elap =(now - xbegan)/10;			/* convert to seconds */

	r = (bytes/1024)/(elap ? elap : 1);		/* overall rate reported to caller */

	if( r > max_rate )
		max_rate = r;
	if( r < min_rate )
		min_rate = r;

	if( elap && chat < now && last_bytes != bytes )		/* change since last call */
	{
		double error;
		double pr = 0;			/* rate for this period of time */
		double pb = 0;			/* bytes written this period */
		ng_timetype pe;			/* period elapsed seconds */

		
		pe = (now - last_msg)/10;
		pb = (bytes - last_bytes)/1024;
		pr = pb/pe;

		last_msg = now;
		last_bytes = bytes;

		error = ((pr - srate)/srate) *100;		/* error from requested rate */
		ng_bleat( 5, "lambast: pr=%I*.2f pb=%I*.2f pe=%I*d", Ii(pr), Ii(pb), Ii(pe) );
		ng_bleat( 2, "lambast: current rate: %I*ds(elap) %I*.2fKb/s(rate) %I*.2f%%(pct-err) %I*d(tot)", Ii(pe), Ii(pr), Ii( error ), Ii(bytes) );

		chat = now + 30;		/* next one of these messages */
	}

	return r;
}

/* 	expect argv to be host filename destfilename 
 	returns 0 == good 
*/
int send_file( int argc, char **argv, int port )
{
	struct	stat	fstat;
	char	address[256];			/* address of remote host */
	char	mbuf[NG_BUFFER];		/* buffer to build msg in */
	int	ifd;				/* the file that we are reading */
	int	ec = 0;				/* exit code */
	int	i;
	ng_timetype now;


	desthost= argv[0];
	
	if( flags & FL_SEND_RANDOM )
	{
		srand( time( NULL ) );
		ng_bleat( 1, "lambast: sending random junk: %I*d bytes", Ii(fsize) );
		random_crap = (char *) ng_malloc( NG_BUFFER, "random crap" );
		for( i = 0; i < NG_BUFFER; i++ )
			*(random_crap+i) = rand( ) & 0x7f;

		srcfn = "/dev/null";
		destfn = argv[1];
	}
	else
	{
		srcfn = argv[1];
		destfn = argv[2];

		stat( srcfn, &fstat );
		fsize = fstat.st_size;
		ng_bleat( 2, "lambast: source file size: %I*d", Ii( fsize ) );
	}


	if( swindow )
	{
		srate = (fsize/1024)/swindow;		/* convert size to kbytes/sec */
		ng_bleat( 1, "user supplied window of %d seconds, converts file size (%I*d) to a rate of: %I*d KB/s", swindow, Ii(fsize), Ii(srate) );
	}
	else
		ng_bleat( 1, "lambast: user supplied rate %I*d KB/s will be used", Ii(srate) );



	if( (block_count = srate/819) < 1 )
		block_count = 1;
	ng_bleat( 2, "lambast: blockcount = %d", block_count );

	xended = xbegan = began = ng_now( );	/* xbegan/end are reset later; keeps stats nice if we dont connect */

	if( (ifd = open( srcfn, O_RDONLY )) < 0 )
	{
		ng_bleat( 0, "lambast: local file open failed: %s: %s", srcfn, strerror( errno ) );
		exit( 2 );
	}

	sfsprintf( address, sizeof( address ), "%s:%d", desthost, port );	/* address for command interface */
	set_timeout( conn_to, "while connecting" );
	if( (cmd_sid = ng_siconnect( address ) ) >= 0 )
	{
		clear_timeout( );
		flags |= FL_OK2RUN;				/* any error/signal will drop this and we will cleanup */

		ng_sipoll( 9000 );			/* wait for data (a socket port for data channel) -- up to 90 seconds */
		if( data_sid < 0  )			/* still dont have a data session */
		{
			ng_bleat( 0, "lambast: unable to establish a data channel" );
			exit( 2 );
		}	


		/* data channel address was received during poll, and a connection was established  send the open command */
		ng_siprintf( cmd_sid, "openfile %s %s %s", destfn, destusr, destmode );		/* mode and user may come in later */

		ng_sipoll( 9000 );			/* wait another 90 seconds for an open ok message to come back */
		if( ! (flags & FL_OPENED) )
		{
			ng_bleat( 0, "lambast: unable to open remote file: %s", destfn );
			exit( 2 );
		}

		ng_bleat( 2, "lambast: file transmission begins" );
		xbegan = ng_now( );					/* start of transmission - measure setup time */
		set_timeout( 300, "while sending block" );
		while( (flags & FL_OK2RUN) && send_block( ifd, data_sid ) )		/* send a block of data */
		{
			set_timeout( 300, "while sending block" );		/* reset with each block snt */
			ng_sipoll( 0 );				/* poll for command session traffic after every block -- no wait */
			while( cur_rate( ) > srate )
				ng_sipoll( 100 );		/* dirty way to get a microsecond sleep! */
		}
		clear_timeout( );
		ng_sipoll( 1 );				/* one last check for errors on data receipt */
		ng_bleat( 2, "lambast: file transmission ended ok2run=%s", (flags & FL_OK2RUN) ? "yes" : "no" );

		xended = ng_now( );			

		close( ifd );
		if( flags & FL_OK2RUN )			/* sent all data without errors */
		{
			ng_bleat( 2, "lambast: sending dataend, waiting for final status" );
			sfsprintf( mbuf, sizeof( mbuf ), "dataend %I*d", sizeof( bytes ), bytes );
			ng_siprintf( cmd_sid, "%s", mbuf  );		/* let them know to clean up */

			ng_sipoll( 30000 );				/* wait some for the final status */ 

			ended = ng_now( );			
		}
		else
		{
			ng_bleat( 2, "lambast: things not ok, sending abort" );
			ng_siprintf( cmd_sid, "abort" );		/* ensure that they trash the file */
		}

		ng_bleat( 2, "lambast: dropping sessions" );
		ng_siclose( data_sid );
		ng_siclose( cmd_sid );
	}
	else
		ng_bleat( 1, "lambast: unable to connect to: %s(%d)", address, ng_sierrno );

	
			
	if( (flags & FL_OK2RUN) && (flags & FL_CLOSED) )		/* still ok, and we got a closed ok message */
		ec = 0;
	else
	{
		ng_bleat( 2, "flags at failure = 0x%x", flags );
		ec = 1;					/* failure */
	}


	if( gen_stats )
	{
		double kb;
		double actual_rate;
		double pct_err;
		ng_timetype elapsed;

		if( (elapsed = (xended - xbegan)/10) < 1 )
			elapsed = 1;

		kb = ((double) bytes)/1024.0;
		actual_rate = kb/elapsed;				/* xbeg/end are transmission begin end, began is start of setup */
		pct_err = ((actual_rate - srate)/srate) *100;		/* error from requested rate */

		sfprintf( sfstdout, "lambast-stats: %d(rc) %s => %s:%s\n", ec, srcfn, desthost, destfn );
		if( ec == 0 )
		{
			sfprintf( sfstdout, "lambast-rate: %I*.2f(kbytes) %I*ds(elapsed) %I*.2f(KB/s) %I*.2f%%(pct-err) %I*dKB/s(max), %I*dKB/s(min)\n", Ii( kb ), Ii(elapsed), Ii(actual_rate), Ii(pct_err), Ii(max_rate), Ii(min_rate) );
		}
	}

	ng_bleat( 2, "lambast: send returning: %d (%s)", ec, ec ? "bad" : "good" );
	return ec;

}


/* ------------------------------- callback things ---------------------------------- */
int cbcook( void *data, int fd, char *buf, int len )
{
	if( fd == cmd_sid )
		do_cmd( buf, len );
	else
		ng_bleat( 0, "lambast: buffer from unrecognised sid received %d", getpid( ) );		/* should not happen */

	return SI_RET_OK;
}


/*
	should not happen as we do not open a listen port
*/
int cbconn( char *data, int sid, char *rmt_addr )
{
	ng_bleat( 2, "lambastd: connection request received %d(fd) from: %s  ERROR!", sid, rmt_addr );

	return SI_RET_OK;
}

/* deal with the loss of a session  */
int cbdisc( char *data, int sid )		/* disconnect received */
{
	ng_bleat( 2, "lambast: disconnect received for sid=%d", sid );
	if( flags & FL_SENDER )
	{
		if( !( flags & FL_EXDISC ) )
		{
			ng_bleat( 0, "lambast: disconnect was not expected" );
			exit( 3 );
		}
		
	}
	else
	{
		if( !( flags & FL_EXDISC ) )
		{
			ng_bleat( 0, "lambastd: disconnect was not expected, unlink file: %s (%d)", tmp_destfn, getpid() );
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
	int	port = -1;		/* port to open */
	int	be_listener = 0; 		/* background  (listener) */
	int	detach = 1;			/* detach if listener */
	int	status = 0;
	int	use_shunt_port = 0;
	int	req_arg_count = 3;

	
	ARGBEGIN
	{
		case 'p':	IARG( port ); break;			/* required to supply */
		case 'r':	IARG( srate ); break;			/* sending rate -- attempt to send at this rate (KB/sec) */
		case 't':	IARG( swindow ); break;			/* sending window -- attempt to send inside of window seconds */
		case 'T':	IARG( conn_to ); break;			/* connection timeout */
		case 's':	gen_stats = 1; break;
		case 'P':	use_shunt_port = 1; break;
		case 'R':	req_arg_count--; flags |= FL_SEND_RANDOM; IARG( fsize ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
				break;
	}
	ARGEND

	if( port < 0 )
	{
		if( use_shunt_port )
		{
			if( (cport = ng_env_c( "NG_SHUNT_PORT" )) != NULL )
				port = atoi( cport );				/* pick up default port number from cartulary */
			else
			{
				ng_bleat( 0, "could not find NG_SHUNT_PORT in env/cartulary" );
				exit( 2 );
			}
		}
		else
		{
			ng_bleat( 0, "must supply -p port or -P on command line!" );
			exit( 1 );
		}
	}

	ng_siprintz( 1 );		/* send end of string when using siprintf */

	status = 1;			/* assume bad */
	if( argc < req_arg_count )			/*  three parms required to send: host srcfile destfile */
	{
		usage( );
		exit( 1 );
	}
	
	flags |= FL_SENDER;


	if( ng_siinit( SI_OPT_ALRM | SI_OPT_FG, -1, -1 ) == SI_OK )		/* no ports open for sender */
	{
		ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    			/* register rtn driven on session loss cb */
		ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );		/* tcp (cooked data) */
		/*ng_sicbreg( SI_CB_SIGNAL, &sigh, (void *) NULL ); */		/* called for singals */


		status = send_file( argc, argv, port );
	}
	else
		ng_bleat( 0, "lambast: unable to initialise: err=%d", ng_sierrno );

	return  status;		/* shell expects 0 to be good */
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_lambast:Blast a file over TCP to shunt)

&space
&synop	
	ng_lambast [-s] [-T conn-sec] [-v] {-p port | -P} {-t sec | -r rate} host srcfile destfile
&space
	ng_lambast [-s] [-T conn-sec] [-v] {-p port | -P} {-t sec | -r rate} -R size host destfile

&space
&desc	
	&This is designed as a test utility only. 
&space
	&This sends a file, &ital(srcfilt) to the destination host placing it into &ital(destfile) 
	using the transfer rate (KB/s) supplied with the -r option, or a rate calculated based on the 
	transfer time (-t) value and the size of the input file.
	The -R option can be used to specify the number of random bytes to send rather than sending
	from an input file. 

&space
	&This assumes it is sending to &lit(ng_shunt) on the other end and thus uses the 
	shunt session setup and transfer protocol. To avoid accidently sending to a production 
	ng_shunt process, the port must specificly be given on the command line (-p), or &this 
	must be explicitly told to use the well known shunt port (-P) as defined in the 
	cartulary/environment.

&space

&space
&opts	 The following commandline flags are recognised; several of them are required!

&begterms
&term	-p n : Specifies the port number (n) that &this should communicate with the destination 
	&lit(ng_shunt) on. This flag, or -P must be supplied to select a port.
&space
&term	-P : Use the well known shunt port when sending. To avoid accidenlty sending to a production 
	shunt process, the use of the shunt well known port must specificly be requested. 
&space
&term	-r rate : Supplies the rate (KB/s) that &this should try to match when sending the data. 
	It may not be possible to send at the desired rate; &this will attempt to send the data
	as fast as it can if it is not possible to match the desired rate. Either this option, or the 
	&lit(-t) option, should be supplied or odd things might happen. 
&space
&term	-s : Stats mode on. This option causes a set of statistics to be prestnted on the standard output
	device at the end of the transmission.  The total number of kbytes, elapsed time, overall transmission 
	rate and the percent error of that rate from the requested rate are given.  A second line of stats 
	gives the exit code, source and destination file names. 
&space
&term	-T sec : Spcifies the maximum number of seconds that &this will wait for a connection attempt.
	If &ital(sec) seconds passes during a connection attempt, the connection is aborted and &this 
	exits badly.
&space
&term	-R size : Sends a random set of bytes such that &ital(size) bytes are transmitted in total.
	When this option  is used, the source file name is omitted from the command line. 

&space
&term	-t sec : This specifies the desired amount of time that &this should take to send the file. 
	Using the value supplied with this flag, &this will compute a transmission rate and attempt
	to send the file no faster than the computed rate.  It is possible, that the 
	file cannot be sent within the time specified; when this happens, &this will transmit the 
	data as fast as it can. Either this option, or the -r (rate) option should be supplied. 
&space
&term	-v : Turns on chatty mode. Use two -v parameters to show more detail about the session.
&endterms

&parms	Following the options on the command line, &this expects the following 
	positional parameters:
&space
&begterms
&term	host : The name of the host to send the file to.
&term	src : The name of the file to send. This parameter is omitted if the -R option is given.
&term	dest : The name the file should be given on the remote host. 
&endterms
&space

&space
&exit	&This will exit with a non-zero return code if it is not able to copy the file correctly.

&space
&see
	ng_shunt

&space

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Jan 2007 (sd) : Original code. 
&term	16 Feb 2007 (sd) : Fixed problem introduced when random mode added. 
&term	09 Jun 2009 (sd) : Remved reference to deprecated ng_ext.h header file.
&endterms

&scd_end
------------------------------------------------------------------ */
