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
  Mnemonic:	rcmd 
  Abstract: 	send a command to a remote parrot and wait for the 
		responses.  we can send via udp or tcp and will listen 
		for responses on both. Communication with parrot is via
		tcp/ip to prevent loss of data.
  Date:		31 Aug 2001
  Author: 	E. Scott Daniels
 -------------------------------------------------------------------------
*/

#include <sys/types.h>          /* various system files - types */
#include <sys/ioctl.h>
#include <sys/socket.h>         /* socket defs */
#include <net/if.h>
#include <pwd.h>
#include <stdio.h>              /* standard io */
#include <errno.h>              /* error number constants */
#include <fcntl.h>              /* file control */
#include <netinet/in.h>         /* inter networking stuff */
#include <signal.h>             /* info for signal stuff */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>

#include	"ng_rcmd.man"
#ifdef KEEP
#endif


#include "ng_socket.h"
#include <sfio.h>
#include <ng_socket.h>

extern int errno;
int print_ok = 1;
long msgcount = 0;

int timeout = 3600;	/* commands longer than this (by default) should be run via woomera */
int fd = -1;		/* session file descriptor -- we support only one */
int rc = 1;		/* return code, ideally we set it to the exit code from the remote command */
int verbose = 0;


/* ----------------------------------------------------------------------- */
void usage( )
{
	sfprintf( sfstderr, "%s version: 3.0/04228\n", argv0 );
	sfprintf( sfstderr, "usage: %s [-p port] [-t secs] host command tokens\n", argv0 );
	exit( 1 );
}

static void sigh( int i )
{
	if( fd < 0 )
	{
		ng_bleat( 0, "timeout while waiting for connection" );
		exit( 1 );
	}
	else
	{
		ng_bleat( 0, "timeout while waiting for data" );
       		ng_siclose( fd );
		exit( 1 );
	}
}

char *get_userinfo()
{
	struct passwd pw;
	struct passwd *result;
	unsigned char	buf[512];
	char	hostname[512];
	char	*cp;
	uid_t uid;

	uid = geteuid();
#ifdef OS_SOLARIS
	if( ! getpwuid_r( uid, &pw, (char *) buf, sizeof( buf ) ) )
#else
	if( getpwuid_r( uid, &pw, (char *) buf, sizeof( buf ), &result ) )
#endif
	{
		ng_bleat( 0, "unable to get password entry for: %d [FAIL]", uid );
		exit( 1 );
	}

	ng_sysname( hostname, sizeof( hostname ) );
	if( (cp = strchr( hostname, '.' )) != NULL )
		*cp = 0;
	
	sfsprintf( (char *) buf, sizeof( buf ), "%s:%d:%s", pw.pw_name, uid, hostname );
	return ng_strdup( (char *) buf );
}

/* deal with a message received; fd is needed if we need to respond to the message */
int show_msg( char *p, int fd )
{
	switch( *(p+1) )
	{
		case 'S':			/* stdout/stderr */
			if( strncmp( p, "~STDOUT", 7 ) == 0 ) 
				sfprintf( sfstdout, "%s\n", p+8 );
			else
				if( strncmp( p, "~STDERR", 7 ) == 0 ) 
					sfprintf( sfstderr, "%s\n", p+8 );
			break;

		case 'P':			/* pid -- means remote side has forked and is working */
			if( strncmp( p, "~PID ", 5) == 0 )
				ng_bleat( 1, "remote executon begins..." );
			break;

		case 'E':			/* exit code from remote process */
			if( strncmp( p, "~EC", 3 ) ==  0 )
			{
       				ng_siclose( fd );
				fd = -1;			/* not necessary, but just in case */
				rc = atoi( &p[4] );		/* set rc with that from the other end */
				return( SI_RET_QUIT );
			}
			break;

		case 'U':					/* request for user name/number */
			if( strncmp( p, "~UID", 4 ) == 0 )
			{
				unsigned char *ub;
				unsigned char *sb;
				char 	msg[NG_BUFFER];

				ub = get_userinfo(  );
				sb = ng_scramble( (unsigned char *) p+4, ub );			/* scramble user id/name */
				sfsprintf( msg, sizeof( msg ), "~IDUSR:" );	/* format scrambled buffer */
				memcpy( msg+7, sb, strlen(ub) + 2 );
				*(msg + strlen(ub) + 9) = '\n';			/* must have newline for parrot */
				ng_sisendt( fd, msg, strlen(ub) + 10 );		/* and send back */
				ng_free( ub );
				ng_free( sb );	
			}
			break;

		default:
			sfprintf( sfstderr, "rcmd: data smells: %s\n", p );
			exit( 1 );
			break;
	}

	return SI_RET_OK;
}
		
/* assume buffer contains null seperated messages */
int cbcook( void *data, int fd, char *buf, int len )
{
	static void *flow = NULL;		/* flow manager handle */
	char	*b;				/* next msg received */
	int	status = 0;			

	if( !flow )
		flow = ng_flow_open( NG_BUFFER );	/* setup to process inbound buffers */

	if( flow )
	{
		ng_flow_ref( flow, buf, len );
		while( (b = ng_flow_get( flow, '\000' )) != NULL )	/* get each null separated buffer */
			status = show_msg( b, fd );			/* something like ~STDERR:junk to help us direct the msg */
	}
	else
	{
		ng_bleat( 0, "cook: internal error - no flow" );
		exit( 1 );
	}

	if( timeout )
		alarm( timeout );

	return status;
}


/* should not happen as we dont open udp */
int cbraw( void *data, char *buf, int len, char *from, int fd )
{
	ng_bleat( 0, "CBRAW: received %d bytes from: %s.\n", len, from );
	ng_bleat( 0, "CBRAW: (%s)\n", buf );
	return SI_RET_OK;
}

int cbconn( char *data, int f, char *buf )
{
	ng_bleat( 2, "Connection request received fd=%d; refused!", f );
	return SI_RET_ERROR;
}

int cbdisc( char *data, int f )		/* disconnect received */
{
	fd = -1;
	ng_bleat( 2, "session was terminsted by the other end!" );

	return SI_RET_QUIT;
}

/* ----------------------------------------------------------------------- */
int main( int argc, char **argv )
{
	extern int errno;
	extern int ng_sierrno;

	int	status = 0;
	int	cto = 10;				/* connection time out */
	int	cmdln = 0;				/* length of command */
	char	*port = NULL;
	int	force = 0;
	int	detached = 0;			/* cmd parrot runs will detach */
	char	cmd[NG_BUFFER];			/* lets hope its not bigger than this */
	char	abuf[256];				/* address of remote parrot */
	char	*host;				/* host to send to */

	port = ng_env( "NG_PARROT_PORT" );	/* parrot port to communicate with */

	ARGBEGIN
	{
		case 'c':	IARG( cto ); break;		/* connection timeout */
		case 'd':	detached++; break;
		case 'f':	force = 1;  break;		/* undocumented in the man and usage  on purpose */
		case 'p':	SARG( port ); break;
		case 't':	IARG( timeout ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
				break;
	}
	ARGEND

	
	if( argc < 2 )		/* must have host, and then  at least one token command */
		usage( );

	if( timeout > 3600 )
		timeout = 3600;			/* sanity enforcement */

	host = *argv++;
	argc--;


	if( ng_siinit( SI_OPT_FG, 0, -1 ) != 0 )		/* dont need a udp port */
	{
		sfprintf( sfstderr, "unable to initialise network interface: ng_sierrno = %d errno = %d\n", ng_sierrno, errno );
		exit( 1 );
	}

	ng_bleat( 2, "network initialisation ok, registering callback routines" );

	cmdln = 6;
	if( detached )
		sfsprintf( cmd, sizeof( cmd ), "~CMD-D" );  /* command will detach; parrot will relase us pronto */
	else
		sfsprintf( cmd, sizeof( cmd ), "~CMD-OE" ); /* to parrot this means we want stderr/out */

	while( argc-- )			/* build the command to send */
	{
		if( (cmdln += strlen(cmd) + 2) >= NG_BUFFER )		/* include room for \n and null */
		{
			sfprintf( sfstderr, "ng_rcmd: command too long!" );
			exit( 2 );
		}

		strcat( cmd, " " );
		strcat( cmd, *argv++ );
	}

	strcat( cmd, "\n" );					/* add newline so parrot can process with ng_flow */

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    			/* register rtn driven on connection */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    			/* register rtn driven on session loss cb */
	ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );		/* tcp (cooked data) */
	ng_sicbreg( SI_CB_RDATA, &cbraw, (void *) NULL );		/* udp (raw) data */

	sfsprintf( abuf, sizeof( abuf ), "%s:%s", host, port );
	ng_bleat( 2, "connecting to: %s", abuf );

	signal( SIGALRM, sigh );
	if( cto )
		alarm( cto );				/* wait at most connection timeout sec for connection */

	if( (fd = ng_siconnect( abuf )) >= 0 )          /* attempt connection */
	{
		ng_bleat( 2, "sending: %s", cmd );
    		ng_sisendt( fd, cmd, strlen( cmd ) );	 /* send command */
		alarm( timeout );			/* turns off if not set, or resets to large value */
		status = ng_siwait( );			/* wait for data */
	}
	else
	{
		sfprintf( sfstderr, "could not connect to %s:%s\n", host, port );
		rc = 3;
	}


	ng_sishutdown( );          /* for right now just shutdown */
	ng_bleat( 2, "shutdown: %d(rc) %d(status) %d(sierror)\n", rc, status, ng_sierrno );

	exit( rc );
}              /* main */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rcmd:Ningaui remote command execution utility)

&space
&synop	ng_rcmd [-p port] [-t secs] host cmd tokens

&desc	Sends &ital(cmd tokens) to the &lit(ng_parrot) process running
	on &ital(host) for execution. The &lit(ng_parrot) process 
	buffers all output (both stderr and stdout) until the command 
	completes, and then the standard output and standard error 
	mesages are returned to &this. &lit(Ng_parrot) is expected to 
	return an initial &lit(PID) message within thirty (30) seconds
	of receiving the command from &this. If &this receives the 
	&lit(PID) message within the thirty second window, it will 
	wait indefinately for data from &lit(ng_parrot). &This assumes 
	that all data has been received when an exit code (&lit(~EC))
	message has been received. 
&space
&subcat Valid Commands
	To implement a small bit of security, 
	&this restricts the commands that it will send to a remote 
	host to only commands that start with the characters &lit(ng_).
	Furthermore, these commands must be in the &lit(PATH) of the 
	&ital(parrot) process running on the remote host. 
	Any command that does not start with &lit(ng_) will not be 
	executed. 
&space
&subcat	Communications
	&This now communicates with &ital(ng_parrot) via a TCP/IP session
	to ensure that communications are not lost. If an old version of 
	&ital(parrot) is running on the targeted host, the seesion will 
	not be accepted and the remote execution will be reported as having
	failed. 

&opts	The following options are recognised when placed on the command 
	line prior to command tokens.
&begterms
&term	-c sec : Defines the number of seconds a connection request is 
	allowed to block before &this gives up and assumes that the remote 
	processess is hung. If not supplied, a maximum of 10 seconds is 
	allowed to pass before giving up.
&space	
&term	-p port : Supplies the port number on the remote host that 
	messages are to be sent to. If not supplied, the port number
	for &lit(ng_parrot), as defined by the &lit(NG_PARROT_PORT)
	cartulary variable, will be used.
&space
&term	-t secs : Specifies the timeout (seconds) that &this will use 
	while waiting for a response from the remote host. 
	The timeout value prevents &this from blocking should the 
	remote process wedge. If this value is supplied, consideration 
	for the nature of remote command processing via &this must be 
	taken into account: the remote command is allowed to run to 
	completion before any data is returned to &this.  Thus the timeout
	should be long enough to accomodate this. If not supplied a
	timeout of 1 hour is used as a default.
	
&endterms

&parms	&This expects a minimum of two command line parameters to 
	be supplied. The first is the name of the host to which 
	the command should be sent. The second command line 
	pameter, through the last, is combined as a command and 
	sent to the remote host.

&examp	The following example illustrates how the &lit(ng_rm_where)
	command can be run on the leader host to find the location of 
	the file &ital(filename) on the current host:
&space
&beglitb
	ng_rcmd `ng_leader` ng_where `ng_sysname` $filename
&endlitb

&space
&mods
&owner(Scott Daniels)
&lgterms

&term 	31 Aug 2001 (sd) : original code. 
&term	04 Mar 2002 (sd) : To skip the space between STDOUT and first byte of data.
&term	21 May 2003 (sd) : Completed conversion to use TCP/IP.
&term	01 Jun 2003 (sd) : Fixed timeout. Added connection timeout.
&term	29 Mar 2004 (sd) : Added a 1 hour default timeout.  Commands that will run longer than
	this should be run via woomera anyway.
&term	07 Jul 2004 (sd) : Changed abort() call to exit() call.  No need to coredump.
&term	08 Nov 2004 (sd) : Converted to ng_flow functions to manage partial buffers from asynch
	tcp sessions.
&term	01 Aug 2005 (sd) : Changes to prevent gcc warnings.
&term	10 Jun 2007 (sd) : Now adds a newline to the command so that parrot can process with ng_flow functions.
&term	22 Apr 2008 (sd) : Added support to repond to parrot when asked for user info. (v3.0)
&endterms

&scd_end
------------------------------------------------------------------ */
