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

#include	<sys/types.h>		/* needed for socket.h */
#include	<sys/uio.h>		/* needed for socket.h */
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<netdb.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<time.h>

#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>

#define		NRETRIES		5

/* we have one need to have no AST referenced routines because of threading issues */
#ifndef	FPRINTF
#ifdef SANS_AST
#include	<stdio.h>
#	define	FPRINTF 	fprintf
#	define	STDERR		stderr
#else
#	include <sfio.h>
#	include	<ningaui.h>
#	define	FPRINTF 	sfprintf
#	define	STDERR		sfstderr
#endif
#endif

extern int verbose;
extern char *argv0;
char *ohost = (char *) -1;		/* something is stompping on the stack in linux gethostbyname chain, use this to try to verify host on call to poot  when looking at dump */

static int
poot(char *host, char *service, int verbose)
{
	struct hostent *hp;
	struct sockaddr_in sin;
	struct servent *se;
	struct servent dummy;
	int port;
	int s, i;
	int en = 1;
	char *ss;

	ohost = host;

	if( (hp = gethostbyname(host)) == NULL )
	{
		en = errno; 					/* save to log if second one works */

		if( strcmp( host, "#ERROR#" ) == 0 )		/* if caller got snubbed by narbalek/pinkpages it would have sent this */
		{
			FPRINTF( STDERR, "%s: host lookup failed: %s: bad host name\n", argv0, host );
			return -1;
		}

		FPRINTF( STDERR, "%s: host lookup failed: %s %s: pausing 35s before retry\n", argv0, strerror( en ), host );

		sleep( 35 );
		if( (hp = gethostbyname(host)) == NULL )
		{
			en = errno;

			FPRINTF(STDERR, "%s: host lookup failed: %s: (%s)\n", argv0, strerror( en ), host);
#ifndef SANS_AST
			ng_log( LOG_WARNING, "ng_rx: both attempts to look up host (%s) failed: %s", host, strerror( en ) );
#endif
			return -1;
		}
#ifndef SANS_AST
		else
			ng_log( LOG_INFO, "gethostbyname required two calls host=%s first err=%s", host, strerror( en ) );
#endif
	}
	dummy.s_port = htons( strtol(service, &ss, 10) );   /* ensure proper order */
	if(*ss == 0){	/* numeric */
		se = &dummy;
	} else {
		if((se = getservbyname(service, 0)) == 0){
			FPRINTF(STDERR, "%s: unknown service '%s'\n", argv0, service);
			return -1;
		}
	}
	port = se->s_port;
	for(i = 0; hp->h_addr_list[i]; i++){
		memset((char *)&sin, 0, sizeof(sin));
		memcpy(&sin.sin_addr, hp->h_addr_list[i], hp->h_length);
		sin.sin_port = port;
		sin.sin_family = hp->h_addrtype;
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(s < 0){
			FPRINTF(STDERR, "%s: rx: socket(%s,%s) failed: %s\n", argv0, host, service, strerror( errno ));
			return -1;
		}
		if(connect(s, (struct sockaddr *)&sin, sizeof (sin)) >= 0)
			return s;
		close(s);
	}
	if(verbose)
		FPRINTF(STDERR, "%s: rx connect: host=%s->0x%lx port=%s->0x%lx; %s\n", argv0,
			host, (long) sin.sin_addr.s_addr, service, (long) sin.sin_port, strerror( errno ));
	return -1;

}
/* ofd is out to service, ifd is in from service */

int
ng_rx(char *mach, char *service, int *ifd, int *ofd)
{
	int fd;
	int n;
	int maxsleep = 20;
	int retries = NRETRIES;		/* be careful; max wait is maxsleep*2^retries */
	int do_init = 1;

again:
	if((fd = poot(mach, service, 1)) >= 0){
		*ifd = fd;
		if( ofd )				/* allow it to be null */
			*ofd = dup(fd);
#ifndef SANS_AST
		if(retries != NRETRIES)
			ng_log(LOG_INFO, "rx needed %d retries\n", NRETRIES-retries);
#endif
		return 0;
	}
	if((errno == ETIMEDOUT) && (retries > 0)){	/* timed out means its there but busy */
		if(do_init){
#ifdef SANS_AST
			srand48(time(0) ^ (71569 * getpid() ));		/* not *as* random as ng_srand(), but close */
#else
			ng_srand();
#endif
			do_init = 0;
		}
		retries--;
		n = drand48()*maxsleep + 0.5;
		if(verbose)
			FPRINTF(STDERR, "%s: service %s!%s timed out; retrying after %d secs; %d retries left\n", argv0, mach, service, n, retries);
#ifndef SANS_AST
		ng_log(LOG_INFO, "service %s!%s timed out; retrying after %d secs; %d retries left\n", mach, service, n, retries);
#endif
		maxsleep *= 2;
		sleep(n);
		goto again;
	}
	FPRINTF(STDERR, "%s: connection to service %s!%s failed: %s\n", argv0, mach, service, errno == 0 ? "unknown reason" : strerror( errno ));
#ifndef SANS_AST
	if(retries != NRETRIES)
		ng_log(LOG_INFO, "rx needed %d retries\n", NRETRIES-retries);
#endif
	return -1;
}


/* try once and return -- allows caller to implement any needed retries */
int
ng_rx1(char *mach, char *service, int *ifd, int *ofd)
{
	int fd;
	int n;
	int do_init = 1;


	if((fd = poot(mach, service, 1)) >= 0){
		*ifd = fd;
		if( ofd )				/* allow it to be null */
			*ofd = dup(fd);
		return 0;
	}

	FPRINTF(STDERR, "%s: connection to service %s!%s failed: %s\n", argv0, mach, service, strerror( errno ));
	return -1;
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rx:Establish TCP/IP socket connection with a server)

&space
&synop int ng_rx( char *machine, char *service, int *ifd, int *ofd )
	int ng_rx1( char *machine, char *service, int *ifd, int *ofd )

&space
&desc	&This will attempt to open a TCP/IP socket with the 
	server process defined by the &ital(service) name on the 
	host &ital(machine).  If the socket is established successfully, 
	&ital(ifd) is set to the file descriptor for input &stress(from) 
	the server and &ital(ofd) is the file descriptor that should be 
	used for output to the server. 
&space
	The function ng_rx1() will attempt the connection one time and if it
	fails will return immediately.  This allows the calling process
	the ability to determine the best retry method, if any retry is
	desired. 

&space
&parms	
&begterms
&term	machine : Is a character string containing the name of the host 
	to which the connection should be attempted.
&term	service : Is a character string containing the name of the 
	service or the port number that the server listents on. If a 
	name is provided, the port number is looked up using the 
	&lit(getservbyname()) system call.
&space
&term	ifd : A pointer to an integer in which the input filedescriptor for the 
	session is placed. 
&space
&term	ofd : A pointer to an integer into which the output filedescriptor for the 
	session is placed.  If this parameter is NULL, then the input filedescriptor
	can be used for both reading and writing on the session. 
&endterms

&space
&exit	On exit, &This returns a zero if it was 
	able to establish a connection. If a connection was NOT established, 
	a non-zero value is returned.

&space
&see	socket(2), tcp(4), getservbyname(3) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Mar 2001 (sd) : Converted from Gecko. Added SCD.
&term	20 Mar 2004 (sd) : To allow this function to be used without the AST
	libraries (needed to support a pthreaded app).
&term	06 Jun 2005 (sd) : Added the rx1 function.
&term	01 Aug 2005 (sd) : Changes to eliminate gcc warnings. 
&term	10 May 2006 (sd) : Added retry if gethostbyname() fails. 
&term	07 Oct 2006 (sd) : Fixed man page. 
&term	06 Jul 2007 (sd) : Added a small bit to help with debugging. 
&term	03 Oct 2007 (sd) : Corrected man page. 
&term	29 Oct 2007 (sd) : Added specific check for the error string that is returned by narbalek
		when it is unable to perform a lookup. Some callers were invoking without checking
		for the error string. 
&endterms

&scd_end
-------------------------------------------------------------------*/
