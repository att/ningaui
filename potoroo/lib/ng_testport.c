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
----------------------------------------------------------------------------------
	Mnemonic: 	ng_testport
	Abstract: 	These rouitines provide an easy interface to test and 
			wait for data on a file descriptor.

			int ng_testport_m( int fd, int delay_sec, int delay_micro )  # test with microsec delay
			int ng_testportw( int fd, int delay )	test for write blocking
			int ng_testport( int fd, int delay )	test for read pending

	Return:		 1 = ok (will not block on write)/data pending for read
			 0 = timeout (no data received during wait) or write would block
			-1 = interrupted (EINTR)
			-2 = error (user programme should test errno)

	Date:		Unknown
	Author: 	E. Scott Daniels

	Mod:	25 Mar 2005 (sd) : Shaved delay from 1/10th second to 25000 
			microseconds. The delay is needed when testport[w] 
			is passed a delay of 0.
		30 Mar 2005 (sd) : Yet more fussing with the delay.  Seems on 
			bsd we need 100,000 to be effective.
		31 Jul 2005 (Sd) : Changes to prevent gcc warnings.
		11 Oct 2006 (sd) : made a few comment changes
----------------------------------------------------------------------------------
*/

#include	<sys/types.h>          /* various system files - types */
#include	<sys/ioctl.h>
#include	<sys/socket.h>         /* socket defs */
#include 	<netinet/in.h>
#include 	<arpa/inet.h>
#include	<string.h>
#include	<poll.h>

#include	<net/if.h>
#include	<errno.h>              /* error number constants */
#include	<fcntl.h>              /* file control */
#include	<string.h>
#include	<netinet/in.h>         /* inter networking stuff */
#include	<netdb.h>              /* for getxxxbyname calls */
#include	<string.h>

#ifdef	OS_SOLARIS
#include	<sys/sockio.h>		/* needed for sun */
#endif

#include	<sys/time.h>		/* needed for linux */
#include	<sys/select.h>		/* needed for linux */

#ifndef	SANS_AST
#include	<stdio.h>              /* when not using sfio, we need these */
#include	<sfio.h>  
#endif
/*	
	WARNING:
	this file is included by ng_netif to put it into the library, and directly by ng_serv when 
	building a non-sfio module (so we avoid linking in the library).
*/



/* test for write */
int ng_testportw( int fd, int delay )
{

	int status = -2;                /* assume the worst to return to caller */
	fd_set writefds;                /* fread fd list for select */
	fd_set execpfds;              /* exeption fd list for select */
	struct timeval *tptr = NULL;   /* time info for select call, null means no delay */
	struct timeval time;

	FD_ZERO( &writefds );               /* clear select info */
	FD_SET( fd, &writefds );     /* set to check read status */

	FD_ZERO( &execpfds );               /* clear select info */
	FD_SET( fd, &execpfds );     /* set to check read status */

	if( delay >= 0 )                /* user asked for a fininte time limit */
	{
		tptr = &time;                 /* point at the local struct */
		tptr->tv_sec = delay;             /* setup time for select call */
		tptr->tv_usec = delay ? 0 : 500;  /* micro seconds -- a small delay seems needed if delay is 0 or we dont seem to poll at all */
	}

	/* if( (status = select( fd + 1, NULL, &writefds, &execpfds, tptr ) >= 0 ) ) */
	if( (status = select( fd + 1, NULL, &writefds, NULL, tptr ) >= 0 ) ) 
	{		                                /* poll was successful - see if ok to write ? */
		if( FD_ISSET( fd, &writefds ) )
			status = 1;
		else                                    /* no event was received  */
			status = 0;                     /* status is would block */
	}                     
	else
		if( errno == EINTR || errno == EAGAIN )			/* interrupted */
			status = -1;			/* not an error */

	return  status;            /* error */
}                                 /* readudp */


/* 	if delay is zero, then mdelay seems to have no effect for values < 50,000.  No 
	effect means that we dont catch pending reads.
*/
int ng_testport_m( int fd, int delay, int mdelay )
{
	int status = -2;                /* assume the worst to return to caller */
	fd_set readfds;                	/* fread fd list for select */
	fd_set execpfds;              	/* exeption fd list for select */
	struct timeval *tptr = NULL;   	/* time info for select call, null  to select causes a full block */
	struct timeval time;

	FD_ZERO( &readfds );           	/* clear select info */
	FD_SET( fd, &readfds );     	/* set to check read status */

	FD_ZERO( &execpfds );           /* clear select info */
	FD_SET( fd, &execpfds );    	/* set to check read status */

	if( delay >= 0 )                /* user asked for a fininte time limit */
	{
		tptr = &time;                 	/* point at the local struct */
		tptr->tv_sec = delay;		/* setup time for select call */
		tptr->tv_usec = mdelay;  	/* microsecs -- anything less than 100000 seems to cause issues */
	}

	if( (status = select( fd + 1, &readfds, NULL, NULL, tptr ) >= 0 ) ) 
	{		                                /* poll was successful - see if data ? */
		if(  FD_ISSET( fd, &readfds ) )             /* buffer to be read */
			status = 1;
		else                                    /* no event was received  */
			status = 0;                     /* status is just ok */
	}                     
	else
		if( errno == EINTR || errno == EAGAIN )			/* interrupted or resource unavailable */
			status = -1;			/* not an error */

	return  status;            	/* some odd error */
}                                 /* readudp */

/* similar function to testport_m but uses poll() rather than select
	thought poll might give a better disc indication on a socket, but seems not 
	to be the case. 
*/
int ng_testport_p( int fd, int delay_milsec )
{
	struct pollfd pfd[1];
	int 	status;

	memset( &pfd, 0, sizeof( pfd ) );
	pfd[0].fd = fd;
	pfd[0].events = POLLIN + POLLERR+ POLLHUP + POLLNVAL;

	if( (status = poll( pfd, 1,  delay_milsec )) > 0 )	/* data ready, or detected error */
	{
		if( pfd[0].revents & (POLLERR+ POLLHUP + POLLNVAL) )
			return -2;
		return 1;
	}
	else
	{
		if( status == 0 )
			return 0;
		else
			if( errno == EINTR || errno == EAGAIN )
				return -1;
	}

	return -1;
}

int ng_testport( int fd, int delay )
{
	int status;
	int attempts = 10;

					/* a delay < 100000 on linux seems fine, but >=100000 is needed on bsd */
	while( (status = ng_testport_m( fd, delay, delay ? 0 : 100000 )) == -1  && --attempts > 0 );
	return status;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_testport:Test and/or wait for data on an open file descriptor)

&space
&synop
	#include <ningaui.h>
&break	#include <ng_lib.h>
&space
	int ng_testport( int fd, int delay)
	int ng_testport_m( int fd, int delay)
	int ng_testportw( int fd, int delay )

&space
&desc
	The &lit(ng_testport) function allows the user programme to block for 
	up to delay seconds if data is not ready for reading on the file descriptor.
	The function will return as soon as data is available, or the indicated
	number of seconds passes. 
&space
	&lit(Ng_testport_m)  is the same as &lit(ng_testport) except that the delay is 
	supplied in micro-seconds.  (values less than 100,000 on some flavours of UNIX 
	seem to be treated as 0). 

&space
	The &lit(ng_testportw) will block until a write on the file descriptor will 
	not block, or until the number of seconds indicated by &lit(delay) passes. 



&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Feb 1997 (sd) : Its beginning of time. 

&term 25 Mar 2005 (sd) : Shaved delay from 1/10th second to 25000 
			microseconds. The delay is needed when testport[w] 
			is passed a delay of 0.
&term		30 Mar 2005 (sd) : Yet more fussing with the delay.  Seems on 
			bsd we need 100,000 to be effective.
&term	31 Jul 2005 (sd) : Changes to prevent gcc warnings.
&term	11 Oct 2006 (sd) : made a few comment changes
&term	03 Oct 2007 (sd) : added man page.
&term	02 Jan 2009 (sd) : We now treat EAGAIN properly in all functions.
&endterms
&scd_end
*/
