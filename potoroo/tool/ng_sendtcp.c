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


/* -----------------------------------------------------------------------------------
	Mnemonic:	sendtcp -  version 2 that avoids using sfio to read from socket
	Abstract:	Establishes a session to node port and sends buffers read from 
			the standard input device.  It waits for responses back from 
			the session until the 'done string' is received. Responses 
			are written to standard out. 
	Date:		7 June 2001 
	Author: 	E. Scott Daniels
   -----------------------------------------------------------------------------------
*/
#include	<sys/stat.h>
#include	<sys/types.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h> 
#include	<unistd.h>
#include	<syslog.h>
#include	<stddef.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>

#include 	<ng_sendtcp.man>

char	*argv0 = NULL;
int	verbose = 0;
int	timeout = 30;		/* alarm if we dont get a response in n seconds */
int	ctimeout = 10;
int	retry_conn = 0;			/* retry failed connections until timeout period expires */

char	*timeout_msg = "timeout waiting for connection";

static void sigh( int x )
{
	ng_bleat( 1, timeout_msg );
	exit( 20 );
}

/* talk to the remote session by sending  a datagram */
void diga( int out, char *buf, int len )
{
	char c[NG_BUFFER];

	memcpy( c, buf, len );
	c[len] = 0;
	ng_bleat( 3, "sending: (%s)", c );
	write( out, buf, len );
}


/*	listen on the line and echo anything received
	at the moment this only works on ascii data and assumes that the end of the 
   	message is \n<endstring>\n
   	This should be fixed in the future, but will work for getting a file dump 
	from seneschal which is the goal at the moment.

	Apr 3 2006 - converted to use flowmgr rather than sfio for buffer management.
	With flowmgr the time to receive 177873 records (21783889 bytes) was just under
	2 secinds. When we used sfio to read the discrptor it took between 45 and 50s!
*/
int escucha( int fd, char *end_str, char sep_ch )
{
	char	buf[NG_BUFFER];
	char	*b;
	int	len;
	static	void *flow = NULL;
		
	ng_bleat( 2, "reading.... timeout=%d", timeout );
	alarm( timeout );						/* reset alarm before each read */

	if( ! flow )
		flow = ng_flow_open( NG_BUFFER ); 

	alarm( timeout );
	while( (len = recv( fd, buf, sizeof( buf )-1, 0 )) > 0 )
	{
		ng_flow_ref( flow, buf, len );
	        while( (b = ng_flow_get( flow, '\n' )) != NULL )      /* get each newline separated buffer */
		{
			if( strcmp( b, end_str ) == 0 )
			{
				ng_bleat( 1, "end string (%s) received", end_str );
				return 1;
			}

			sfprintf( sfstdout, "%s\n", b );
		}

		alarm( timeout );
	}

	ng_bleat( 2, "escucha: unexpected read failure: %s", strerror( errno ) );
	return -1;			/* session likely disco on us */
}

/* establish the session */
int phone( char *node, char *port, int *in, int *out )
{
	int status;

	if( !port )
	{
		ng_bleat( 0, "unable to get PORT from env" );
		return 0;
	}
	
	ng_bleat( 1, "attempting session to %s:%s", node, port  );

	while( (status = ng_rx( node, port, in, out )) && retry_conn )
		sleep( 1 );

	return ! status;
#ifdef OLD
	return ! ng_rx( node, port, in, out );			/* returns 0 for all ok */
#endif
}	

/* shut things down gracefully */
void hangup( int in, int out )
{
	if( in )
		close( in );

	if( out >= 0 )
		close( out );
}

void show_usage( )
{

	sfprintf( sfstderr, "%s version 2.3/05106\n", argv0 );
	sfprintf( sfstderr, "usage: %s [-c] [-e endstr] [-h] [-p] [-r] [-T seconds] [-t seconds] [-v] [-w] [-z] host:port <file \n", argv0 );
	exit( 1 );
}

/* parms: nodename port */
int main( int argc, char **argv )
{
	int	in;		/* file descriptors */
	int	out;
	char	*prompt = NULL;
	int 	hangup_rc = 0;		/* dont treat hangup as error */
	int	woomera = 0;		/* woomera special processing */
	int	wait_after_each = 1;
	int	quiet_end = 0;		/* by default in woomera mode we end with a bang */
	char	endch = '!';		/* end of transmission ch ! for woomera; -Z sets to null */
	char	*buf;
	char	*end_str = "~done";
	char	*port;		/* port to use */
	char	*host;		/* host to send to */
	char	ename[1024];
	char	sepch = '\n';		/* message seperation character */
/*
	Sfio_t	*sfin = NULL;		deprecated -- we use flowmgr to manage tcp buffers; sfio was slow
*/


	ARGBEGIN
	{
		case 'c':	retry_conn = 1; break;
		case 'd':	wait_after_each = 0; break;
		case 'e':	SARG( end_str ); break;
		case 'h':	hangup_rc=2; break;
		case 'p': 	SARG( prompt ); break;
		case 'q':	quiet_end = 1;	break;		/* dont end with a bang if in woomera mode */
		case 'r':	endch = 0xbe;; break;			/* send repmgr end of data as end   */
		case 'T':	IARG( ctimeout ); break;		/* connection timeout */
		case 't':	IARG( timeout ); break;
		case 'v':	OPT_INC(verbose); break;
		case 'w':	woomera = 1; wait_after_each = 0; break;
		case 'z':	sepch = 0; break;		/* message seperator is end of string */

				/* hacks for replication manager support */
		case 'Z':	endch = 0;; break;			/* send null as end of message  */
		case 'R':	wait_after_each = 0; woomera = 1; endch = 0xbe;; break;	/* send replication manager end of message */
		default:
usage:
			show_usage( );
			exit( 1 );
			break;
	}
	ARGEND

	sfset( sfstdout, SF_LINE, 1 );          /* force a flush with each write -- needed if we are piped someplace */

	signal( SIGALRM, sigh );		/* call our handler when alarm pops */

	host = argv[0];

	if( argc < 2 )
	{
		if( argc < 1 )
		{
			ng_bleat( 0, "missing parms." );	
			show_usage( );
			exit( 1 );
		}

		port = strchr( host, ':' );		/* allow user to enter host:port */

		if( ! port )
		{
			ng_bleat( 0, "bad host:port parameter: %s", host );	
			show_usage( );
			exit( 1 );
		}

		*port = 0;
		port++;
	}
	else
		port = argv[1];

	if( ! isdigit( *port ) )				/* assume cartulary name supplied */
	{
		if( ! (buf = ng_env( port )) )						/* assume something like: NG_CATMAN_PORT */
		{
			sfsprintf( ename, sizeof( ename ), "NG_%s_PORT", port );
			if( ! (buf = ng_env( ename ) ) ) 					/* allow user to say CATMAN */
			{
				ng_bleat( 0, "unable to find port (%s or NG_%s_PORT) in cartulary/environment", port, port );
				exit( 1 );
			}
		}

		port = buf;
	}

	alarm( ctimeout );
	if( phone( host, port, &in, &out ) )
	{
		alarm( 0 );						/* prevent timeout if we block before our read */
		timeout_msg = "timeout waiting for response";
		ng_bleat( 1, "connected to  %s:%s", host, port );

		if( prompt )
			sfprintf( sfstdout, "%s", prompt );

		while( out >= 0 &&  (buf = sfgetr( sfstdin, '\n', 0 )) != NULL )
		{
			alarm( timeout );							/* reset alarm before each write */
			diga( out, buf, sfvalue( sfstdin ) );

			if( wait_after_each && escucha( in, end_str, sepch ) <= 0 )		/* for all but woomera listen between sends */
			{
				ng_bleat( 1, "the bugger blew us off!" );
				hangup( in, out );
				exit( hangup_rc );	
			}

			alarm( 0 );						/* turn off until next write */

			if( prompt )
				sfprintf( sfstdout, "%s", prompt );
		}

		if( woomera )		/* in woomera mode, we dumpped everything in, but never listened (above) */
		{			/* now we write the end of stream chr woomera expects, then quietly read what comes back */
			char xb[10];

			alarm( timeout );			/* reset alarm before write */
			if( ! quiet_end )
			{
				sfsprintf( xb, sizeof( xb ), "%c\n\n", endch ); 	/* end of stream marker */
				diga( out, xb, 3 );
			}
			close( out );			
			out = -1;
			ng_bleat( 1, "output fdes now closed, listening\n" );
			alarm( timeout );			/* reset alarm before each write */
			escucha( in, end_str, sepch );
		}
		else
			if( ! wait_after_each )			/* wait now that all has been sent, if we did not wait after each send */
				escucha( in, end_str, sepch );
		
		hangup( in, out );
	}
	else
	{
		ng_bleat( 0, "unable to connect to: %s:%s", host, port );
		exit( 1 );
	}

	exit( 0 );
}



#ifdef KEEP
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sendtcp:Send datagrams to a programme listening via TCP/IP)

&space
&synop	ng_sendtcp [-c] [-e end-str] [-p] [-r] [-T sec] [-t sec] [-v] [-w] [-z] host:port

&space
&desc	&This attempts to establish a TCP/IP session with the application 
	listing via TCP/IP &ital(port) on &ital(host). If the session is
	established, &this reads and sends newline terminated buffers from 
	the standard input device to the application on the other end of 
	the socket. After each buffer is sent, &this waits for messages 
	to be returned and writes these messages to the standard output 
	device. When a message containing just &ital(end-str) is received, 
	&this will read the next buffer from the standard input and start
	the send read process again. This continues until the session is closed 
	or and end of file on standard input is reached. 
&space
	Messages from the remote session are expected to be newline terminated
	and may arrive packed multiple messages per a single TCP/IP 
	receive buffer.  The -z option allows the messages to be null (0)
	terminated rather than newline terminated. 
&space
	It is consdiered an error if the session drops before all data 
	has been read from the standard input device, or if there has been
	no data received for  longer than the timeout period. 

&space
	The -w option places &this into 'woomera mode' which causes all 
	information received on the standard input device to be sent to 
	the remote process before a read is attempted (as this is how
	woomera works). It is not considered an error for the socket to 
	be closed by the remote process when in woomera mode if all 
	data has been sent.

&space
&opts	The following options are recognised when placed on the 
	command line:
&space
&begterms
&term	-c : Continuous connection retry mode. &This will  attempt to 
	establish the connection to the remote application after each 
	failure, up until the connection timeout (-T) is reached. 
	If this option is not supplied, only one connection attempt is 
	made. 
&space
&term	-d : Dump all input before listening.  Similar to the woomera
	mode of exeution (&lit(-w)), the dump mode will read and send 
	all records from the standard input device before listening 
	for data from the other process. After the last record is read
	&this will then listen until the end marker is received. Once 
	the end marker is received, the session is shutdown.
&space
&term 	-e end-str : Defines the 'end of data' string that &this will 
	wait for. If omitted, then &this assumes &lit(~done) to be the
	end string. 
&space
&term	-h : Causes an unexpected disconnection of the sesion (hanup)
	to be treated as an error.
&space
&term	-p : Causes a prompt to be issued when &this is expecting 
	input from the standard input device. 
&space
&term	-r : Sends the repmgr end of data string as final character of 
	the message.  Mostly to assist with repmgr testing.
&space
&term	-T sec : Connection timeout (seconds).  The amount of time 
	that is assumed a connection can be established in.  If this
	number of seconds passes before a connection is created 
	&this exits with an error. 
&space
&term	-t sec : Time out (seconds). Supplies the number of seconds
	that &this should wait for a response from the remote process.
	If &ital(seconds) passes with no response, then &this will
	exit with a return code of 20.
&space
&term	-v : Verbose mode. 
&space
&term	-w : Woomera mode. 
	When in woomera mode, all data from the standard input device 
	is read and sent to the remote application  before any data is 
	expected back. In woomera mode, &this expects that the sending 
	process will close the socket when it has sent all that there 
	is to send. The end of input data flag is sent when the 
	end of the input file is reached.
&space
&term	-z : Change message seperator to an end of string (zero).
&endterms

&space
&parms	Two positional parameters are required to be entered on the 
	command line:
&space
&begterms
&term	host : The name or IP address of the host where the remote 
	application is executing.
&space
&term	port : The port number that the application listens on. This 
	can be a ningaui cartulary variable, the &lit(XXX) portion of 
	a cartulary variable with the format of
	&lit(NG_XXX_PORT), or it can be the actual port number. 
&endterms

&space
&exit	A non-zero return code indicates that there was an error
	in processing. Unless the &lit(-h) option is supplied, 
	&this does &stress(not) treat a session disconnection as 
	an error.  This allows &this to be used to communciate 
	with processes that write all they have to say and then 
	close the session without ever sending an end of data 
	message. 	An exit code of 20 indicates that a 
	timeout on the session occurred.


&space
&see	ng_sendudp

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 28 Nov 2001 (sd) : Thin end of the wedge.
&term	07 Dec 2001 (sd) : Updated doc, changed timeout exit code to 20.
&term	20 Feb 2003 (sd) : Now trashes nulls at teh end of a buffer to accurately
	detect the end of data string in some cases. 
&term	19 Jan 2004 (sd) : Converted to use sfread to avoid all buffer spanning issues. Version changed 
	to 2.0.
&term	17 Nov 2004 (sd) : Added connection timeout.
&term	23 Mar 2006 (sd) : Some minor changes with regard to where alarms are set. V2.1
&term	03 Apr 1006 (sd) : Converted away from using sfio to manage the input from the scokket.
		See internal comments for timings.  (HBD2ME!) V2.2.
&term	10 May 2006 (sd) : Added connection retry option and logic. V2.3
&term	26 Jun 2006 (sd) : Forces line dicipline on stdout. 
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
