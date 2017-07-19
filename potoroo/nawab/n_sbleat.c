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

/* we implement a session bleat to send a message to a session 
   in order to allow us to give more errors to the user as they 
   submit programmes.  User programme submissions do come in via
   TCP/IP, and we expect that the net-if module will set the active
   session for bleating to -- will have to be reworked if we go 
   to threads.


   All messages are written if there is a known handle regardless of
   the value of urgency.  Urgency is passed to bleat to filter the 
   drivvel that users need to see but the log does not.
*/

#include	<stdarg.h>     /* required for variable arg processing */
#include	<stdio.h>
#include	<string.h>


#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_lib.h>

static	int	sb_handle = -1;	/* ng_socket interface session id */

/*	sb_handle is the active session receiving messages. we assume we are 
	single threaded through programme parsing
*/
void set_sbleat( int handle )
{
	sb_handle = handle;
}


/* looks a lot like bleat.... */
void sbleat( int urgency, char *fmt, ...)
{
	va_list	argp;			/* pointer at variable arguments */
	char	obuf[2048];		/* final msg buf  - we reserve the first few bytes for possible header things */
	char	time[150];		/* time according to ningaui */
	/*ng_timetype gmt;*/		/* now in gmt */
	char	*uidx; 			/* index into output buffer for user message */
	char	*s;    			/* pointer into string buffer */
	int	space; 			/* amount of space in obuf for user message */
	int	hlen;  			/* size of header in output buffer */


	snprintf( obuf, 200, "%s: ", argv0 );		/* use up to 200 bytes for the header */

	hlen = strlen( obuf );          
	space = 2048 - hlen;             	/* space for user message */
	uidx = obuf + hlen;                   /* point past header stuff */

	va_start( argp, fmt );                       /* point to first variable arg */
	vsnprintf( uidx, space - 1, fmt, argp );
	va_end( argp );                                 /* cleanup of variable arg stuff */

	ng_bleat( urgency, "%s", obuf );		/* anything here needs to be screamed to the world */
	if( sb_handle >= 0 )				/* and if n_netif has set a session partner to yammer to... */
		ng_siprintf( sb_handle, "%s\n", obuf );
}

#ifdef SELF_TEST
int verbose = 0;
main( int argc, char **argv )
{

	int i = 0;

	if( argc > 1 )
		verbose = atoi( argv[1] );

	ng_bleat( 0, "verbose level 0 test. verbose=%d i=%d", verbose, i );
	i++;
	ng_bleat( 1, "verbose level 1 test. verbose=%d i=%d", verbose, i );
	i++;
	ng_bleat( 2, "verbose level 2 test. verbose=%d i=%d", verbose, i );
	i++;
	ng_bleat( 3, "verbose level 3 test. verbose=%d i=%d", verbose, i );
	
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_bleat:Babble on to file depending on verbose level)

&space
&synop	ng_bleat( int level, char *fmt, ...)
	ng_bleat_setudp( int udp_fd, char *to ) .br
	ng_bleat_setf( Sfio_t *f ) .br
&space
&desc	&This is a  &lit(printf) like function that accepts a level as the first 
	parameter and will write to the an output file only if the 
	level passed in is greater or equal to the current setting 
	of the global variable &lit(verbose).
	By default, &this will write to the standard error device, 
	however if the &lit(ng_bleat_setf) or &lit(ng_bleat_setudp) functions
	have been invoked the destination of the message, if it should be 
	written, is controlled by the calling programme. 

	&This automatically writes a timestamp before the message
	and adds a newline following the message.

&space
&parms	The following parameters are recognised by &this:
&begterms
&term	level : An integer that is compared with the current setting 
	of the global variable &lit(verbose). If &lit(verbose) is 
	greater, or equal, to the value of &ital(level), then the 
	message is written. If &ital(level) is less than zero (0), 
	then a critical message is written to the log using &lit(ng_log).
&space
&term	fmt : A &lit(printf) style format statement. All remaining 
	parameters (if any are supplied) 
	are formatted using the contents of this string.
	
&endterms

&space
&subcat	ng_bleat_setf( Sfio_t *f) )
	This funciton allows the calling programme to define another 
	output file for messages to be written to.  The file pointer
	&ital(f) passed in should refernce an already open file
	that has been opened using one of the &lit(Sfio) routines. 
	It is the caller's responsibility to ensure that the file 
	is closed and/or flushed as is necessary.

&space
&subcat	ng_bleat_setudp( int udp_fd, char *to)
	This function allows the user to cause messages that will be 
	written by &lit(ng_bleat) to be written to the open UDP/IP 
	port destined for the IP address pointed to by the 
	&ital(to) parameter. The file descriptor is expected to be 
	an established (bound) file descriptor that can be used by 
	the &lit(ng_netif) library routines. 
&space
	Once the message destination has been altered using eithr 
	of the supplimental routines, it will remain set to the 
	new value until one of the routines is used to reset it.
	UDP messaging is turned &ital(off) by setting the destination 
	address (to) to &lit(NULL).  

&space
&see	ng_netif(lib)
&space
&mods
&owner(Scott Daniels)
&lgterms
&term 25 Jun 2001 : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
