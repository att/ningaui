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
  Mnemonic:	ng_sendudp
  Abstract: 	Reads from standard input and sends the buffers to argv[1].
  Date:		13 April 2001
  Author: 	E. Scott Daniels
*/

#include <sys/types.h>          /* various system files - types */
#include <sys/ioctl.h>
#include <sys/socket.h>         /* socket defs */
#include	<stdlib.h>
#include	<unistd.h>
#include <net/if.h>
#include <stdio.h>              /* standard io */
#include <errno.h>              /* error number constants */
#include <fcntl.h>              /* file control */
#include <netinet/in.h>         /* inter networking stuff */
#include <signal.h>             /* info for signal stuff */
#include <string.h>

#include	<sfio.h>
#include	<ng_basic.h>
#include	<ng_lib.h>

#include	"ng_sendudp.man"

#define FL_MULTICAST	0x01

char	*argv0 = NULL;
int	verbose = 0;
int 	timeout = 30;
int	stop_after = 0;			/* if set to +n, then we will quit listening after n messages are received, or timeout reached */
char	*msep_str = "\n";		/* message seperator */

/* listen on the line and echo anything received */
int escucha( int in, char *end_str )
{
	extern	int errno;
	char	buf[4096];
	int 	n;
	int	count = 0;
	char	*tok;
		
	if( !end_str )
		return 0;

	ng_bleat( 2, "reading.... timeout=%d\n", timeout );
	while( (n = ng_readudp( in, buf, sizeof( buf ) - 1, NULL, timeout )) > 0 )
	{
		if( n >= 0 )
			buf[n] = 0;

		tok = strtok( buf, msep_str );				/* separate into messages */
		while( tok )
		{
			if( strcmp( tok, end_str ) == 0 )
			{
				ng_bleat( 1, "end string (%s) received", end_str );
				return 0;
			}

			sfprintf( sfstdout, "%s\n", tok );

			tok = strtok( NULL, msep_str );
		}

		if( stop_after && ++count >= stop_after )
		{
			ng_bleat( 2, "max messages received: %d\n", stop_after );
			return 0;					/* get out now */
		}

		ng_bleat( 2, "reading.... timeout=%d\n", timeout );
	}

	return 1;
}

void usage( )
{
	sfprintf( sfstderr, "version 1.1/03025\n" );
	sfprintf( sfstderr, "usage: %s [-man] [-d seconds] [-m] [-r endstring] [-s msg_seperator] [-t seconds] hostname:service\n", argv0 );
	exit( 1 );
}

int main( int argc, char *argv[] )
{
	extern int errno;

	char	*end_str = NULL;
	char	*to = NULL;		/* address to send messages to */
	int	fd;			/* port file descriptor */
	char	*buf;			/* buffer from read */
	char	*p;
	char	*mgroup = NULL;		/* multicast group */
	int 	delay = 0;		/* delay if user requests */
	int 	wrote = 0;		/* number wrote - to prevent delay after last message */
	int	rc = 0;			/* exit code */
	int 	flags = 0;
	int	port = 0;		/* normally we can use any udp port, if multicast we need specific one */

	ARGBEGIN
	{
		case 'd':	IARG( delay ); break;
		case 'm':	flags |= FL_MULTICAST; break;
		case 'p':	IARG( port ); break;
		case 'q':	IARG( stop_after ); break;
		case 'r':	SARG( end_str ); break;
		case 's':	SARG( msep_str ); break;
		case 't':	IARG( timeout ); break;
		case 'v':	verbose++; break;
usage:
		default: 	usage( ); exit( 1 ); break;
	}
	ARGEND

	if( argc )
	{
		to = argv[0];
	}
	else
	{
		usage( );
		exit( 1 );
	}

	if( flags & FL_MULTICAST )
	{
		mgroup = strdup( to );
		if( (p = strchr( mgroup, ':' )) != NULL )
		{
			*p = 0;
		}
	}

	if( (fd = ng_open_netudp_r( "0.0.0.0", port, flags & FL_MULTICAST ? 1: 0 )) >= 0 )		/* open and allow reuse */
	{
		if( flags & FL_MULTICAST )
		{
			if( ! ng_mcast_join( fd, mgroup, INADDR_ANY, (unsigned char) 8 ) )
			{
				ng_bleat( 0, "multicast group join failed! %s %s\n", mgroup, strerror(errno) );
				exit( 1 );
			}
			else
				ng_bleat( 1, "joined multicast group: %s ttl=8", mgroup );
		}

		while( (buf = sfgetr( sfstdin, '\n', SF_STRING )) )
		{
			if( wrote++ && delay ) 		/* dont sleep on first record, or after last */
				sleep( delay );		/* sleep before writing next one */

			if( *buf )
			{
				if( verbose )
					sfprintf( sfstderr, "to: %s (%s)\n", to, buf );

				ng_writeudp( fd, to, buf, strlen( buf )+1 ); 
			}
		}

		if( end_str )
			rc = escucha( fd, end_str );

		ng_closeudp( fd );
	}
	else
	{
		printf( "unable to open udp port.\n" );
		rc = 1;
	}

	return rc;
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sendudp:Send Messages Via UDP/IP)

&synop	ng_sendudp [-d delay] [-m]  [-p port] [-q msg-count] [-r endstr] [-s seperator] hostname:service

&space
&desc	&This reads records from the standard input device 
	and writes them to &ital(hostname:port) via UDP/IP.
	No effort to read any data from the network is made
	by &this.

&space
&opts	The following options are recognised by &this.
&space
&begterms
&term	-d seconds : Defines the number of seconds that &this 
	will pause between writing messages. If this value is 
	omitted, or a value of &lit(0) is entered, 
	then no delay is imposed. 
&space
&term	-m : Multicast mode. The address:port supplied on the 
	command line is taken as a multicast group and port and 
	&this sends the message to the group. If the end string 
	is supplied, reponses are waited on from the group until 
	the end string is received, or until the timer expires.
&space
&term	-p port : Supplies a specific port to open and send messages 
	from.  
&space
&term	-q msg-count : Quit after reciving &ital(msg-count) messages 
	if timeout seconds is not already reached. 
&space
&term	-r endstr : Read mode. After all input records have been 
	sent, &this will issue a read on the UDP port and echo all
	records received until *ital(endstr) is received as the first
	characters following a newline.

&space
&term	-s sep : Message seperator. &ital(Sep) is used as the message 
	seperator (newline is the default) to divide up the inbound 
	data. 
&space
&term	-t sec : Specify a max read timeout. If not supplied, then 
	30 seconds is used.
&endterms

&space
&parms	One command line parameter, the destination of messages, 
	is expected by &this. The destination must be specified 
	in terms of &ital(hostname) and &ital(servicename) of 
	the process that is to receive the messages. &ital(Hostname)
	may be the name of the host (as can be looked up by 
	&lit(gethostbyname()), or a dotted decimal IP address. 
	The &ital(servicename) may be either the name of the 
	process as can be looked up using &lit(getservbyname())
	or the port number that the process is listening on. 
	If a service name is given, rather than the port number, 
	then a communnication type of &lit(udp) is assumed 
	when making the &lit(getservbyname()) system call. The
	destination components must be supplied as one token 
	on the commandline separated with either a colon (:) or 
	semicolon (;).

&ital(this is ital).
&space
&exit	&This will exit with a zero (0) return code if it was 
	able to send all of the messages. A non-zero return 
	code indicates that some error was encountered during 
	initialisation or processing. 

&space
&see	ng_netif

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 13 Apr 2001 (sd) : Original code. 
&term	31 Jul 2002 (sd) : Added support for message seperator.
&term	03 Jan 2005 (sd) : The -m option no longer uses the port supplied on the 
	command line in the 'to' field as the port to open. 
&term	02 Mar 2005 (sd) : Flags was not being initialised properly; fixed. (version 1.1)
&term	09 Nov 2005 (sd) : Added -q option.
&endterms

&scd_end
------------------------------------------------------------------ */
