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
	BIG NOTE:	There has been a grand attempt to make this routine 
		independant of AST if  SANS_AST is defined. This allows the 
		user to implement threads. To maintan this, do NOT use 
		ng_ functions. Also note that fprintf/sfprintf is controlled
		via a constant.
*/

#include	<sys/types.h>		/* needed for socket.h */
#include	<sys/uio.h>		/* needed for socket.h */
#include	<sys/socket.h>
#include	<string.h>
#include	<netinet/in.h>
#include	<netdb.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<fcntl.h>

#if defined( OS_LINUX ) || defined( OS_CYGWIN )
/* include nothing */
#else
#include	<sys/filio.h>
#endif

/* be careful of sysexits.h; it redefines EX_OK from unistd.h */

#ifdef SANS_AST
#else
#include	<stdio.h>
#endif

#include	<errno.h>

extern char *argv0;
extern int verbose;

/* we have one need to have no AST referenced routines because of threading issues */
#ifndef	FPRINTF
#ifdef SANS_AST
#	define	FPRINTF 	fprintf
#	define	STDERR		stderr
#else
#	include <sfio.h>
#	include	<ningaui.h>
#	define	FPRINTF 	sfprintf
#	define	STDERR		sfstderr
#endif
#endif

int
ng_serv_announce(char *service)
{
	int sock;
	struct sockaddr_in serv;
	struct servent *se;
	struct servent dummy;
	char *ss;
	int optval;

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		FPRINTF(STDERR, "%s: socket failed: %s\n", argv0, strerror( errno ));
		return -1;
	}
	if(fcntl(sock, F_SETFL, O_NDELAY) < 0){
		FPRINTF(STDERR, "%s: ioctl(FIONBIO) failed: %s\n", argv0, strerror( errno ));
		return -1;
	}
	dummy.s_port = htons( strtol(service, &ss, 10) );   /* ensure proper byte order */
	if(*ss == 0){	/* numeric */
		se = &dummy;
	} else {
		if((se = getservbyname(service, 0)) == 0){
			FPRINTF(STDERR, "%s: unknown service %s'\n", argv0, service);
			return -1;
		}
	}
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = INADDR_ANY;
	serv.sin_port = se->s_port;

	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof optval) < 0)
		FPRINTF(STDERR, "%s: setsockopt failed: %s", argv0, strerror( errno ));

	if(bind(sock, (struct sockaddr *)&serv, sizeof serv)){
		if(errno == EADDRINUSE){
			if(verbose > 1)
				FPRINTF(STDERR, "%s: bind failed, reusing\n", argv0);
			optval = 1;
			if(bind(sock, (struct sockaddr *)&serv, sizeof serv) == 0)
				goto success;
		}
		FPRINTF(STDERR, "%s: bind failed, port=%d: %s\n", argv0, ntohs( serv.sin_port ), strerror( errno ));
		close(sock);
		return -1;
	}
success:

	if(listen(sock, 5) < 0){
		FPRINTF(STDERR, "%s: listen failed: %s", argv0, strerror( errno ));
		close(sock);
		return -1;
	}
	return sock;
}


/* wait up to delay seconds for data on the listen port. if delay is <0 then we block 
   completely until there is something -- desired for threaded apps that have a listening 
   thread.
	returns the fd of the new session; or 0 (not a good thing I think) if it would block.
	return values < 0 indicate failure.
*/
int ng_serv_probe_wait(int sock, int delay)
{
	int length;
	struct sockaddr client;
	int msgsock;
	int junk = -1;

	if(verbose > 5)
		FPRINTF(STDERR, "ng_srv_probe waiting up to %d seconds\n", delay);
	length = sizeof client;

	if( delay && (junk = ng_testport( sock, delay )) <= 0 )		/* wait up to delay secs for something on the port */
		return junk;

	if((msgsock = accept(sock, &client, &length)) < 0){
		if(errno == EWOULDBLOCK)
			return 0;
		return -1;
	}
	if(verbose > 5)
		FPRINTF(STDERR, "ng_srv_probe: got one!\n");
#ifndef OS_LINUX
#define	NUP
#endif
#ifdef	NUP
	junk = 0;
	if(ioctl(msgsock, FIONBIO, &junk) < 0){
		FPRINTF(STDERR, "%s: ioctl(FIONBIO %d) failed: %s\n", argv0, junk, strerror( errno ));
		/* may as well let it go ... */
	}
#endif
	return msgsock;
}

int
ng_serv_probe( int sock )
{
	return ng_serv_probe_wait(  sock, 0 );		/* back compat with all that used expecting no delay at all */
}

void
ng_serv_unannounce(int sock)
{
	close(sock);
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start


&doc_title(ng_serv:Routines to support server socket communication)

&space
&synop	int ng_serv_announce( char *service ) 
&break	int ng_serv_probe_wait( int sock, int delay ) 
&break	int ng_serv_probe( int sock ) 
&break	void ng_serv_unannounce( int sock )

&space
&desc	The routines contained in this module provide the support 
	which allows an application to listen on a port for TCP/IP
	connection requests, accept those reqests, and to close
	the listening port.
&space
	WARNING:
&break
	The TCP/IP server that is implemented with these functions is 
	extremely synchronous.  The ningaui socket library functions
	can be used to implement a callback oriented socket based 
	server. 
	
&space
	&ital(Ng_serv_announce) Is invoked to establish a &ital(listening) 
	socket with TCP. &ital(Service) is expected to be a character 
	buffer containing the name of the servce or the port number. 
	If the service name is provided, the port number is determined
	by a call to the &lit(getservbyname()) system call. Upon a successful
	socket and listen setup, the file descriptor is returned to the 
	caller. A value less than zero is returned if there was an error.

&space
	&ital(Ng_serv_probe_wait) provides waits up to &ital(delay) seconds
	for a connection request to be received on &ital(socket).
	The return value indicates one of three possible states:
&space
&smterms
&term	0 : No connection request was received during the seconds 
	waited; no session exists.
&term	^>0 : A session was successfully accepted and the return value 
	is the file descriptor of the accepted session.
&term	^<0  : An error occurred and the socket environment is not 
	in a known state. 
&endterms

&space
	&ital(Ng_serv_probe) provides a &stress(non-blocking) method to 
	accept the next outstanding connection request. 
	A call to &ital(ng_serv_probe) is exactly the same as a call to 
	&tial(ng_serv_probe_wait) with a delay value of zero (0).
	The return values for &ital(ng_serv_probe) are the same as for
	&ital(ng_serv_probe_wait).

&space
	The &ital(ng_unanounce) function closes the listen socket 
	supplied as the only parameter. 

&space
&examp	The following is a code example of how these routines can be 
	used to accept a session request from a remote process
	using these functions:
&space
&litblkb
void big_ears( char *service )
{
	int	lport;		// listen port
	int	sock;		// connected socket
	int	delay = 2;	// wait up to 2 seconds each time for a connection
	
	if( (lport = ng_serv_announce( service )) >= 0 )
	{
		while( 1 )
		{
			if( (sock = ng_serv_probe_wait( lport, delay )) > 0 ) 
			{
				suck_in_info( sock );		// what ever reads and deals with data
				close( sock );
			}
			else
				if( sock < 0 )
					break;		// error of somesort

			do_other_work( );		// if work needs to be done asynch to connections
		}
	}

	close( lport );
&litblke

&space
&see	socket(2), tcp(4), getservbyname(3) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 28 Mar 2001 (sd) : Converted from Gecko. Added SCD.
&space
&term	20 Mar 2004 (sd) : mod to allow a non-ast implementation.  This does not allow 
	for a 'timeout' as it is implemented with ng_testport which is deep into 
	other ng_lib functions and thus very ast dependant. Reads will block forver
	which is not good at all if a process conects and does nothing but hold the 
	session! 
&space
&term	30 Apr 2004 (sd) : Corrected prob_wait to return the result of the ng_testport 
	call  if <= 0; 0 == no data, < 0 is error.  It was returning -1 if testport 
	returned no data.  This caused woomera grief. (HBD #18 KAD)
&term	18 Jun 2005 (sd) : Moved the REUSE setting to before the first bind attempt.
&term	31 Jul 2005 (sd) : Changed to eliminate gcc warnings.
&term	03 Oct 2007 (sd) : Updated man page.
&endterms

&scd_end
-------------------------------------------------------------------*/
