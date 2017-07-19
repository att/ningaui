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


#include	<stdarg.h>     /* required for variable arg processing  -- MUST be included before stdio! to prevent ast issues */
#include	<stdio.h>
#include	<string.h>


#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_lib.h>

static	Sfio_t	*fd = NULL;		/* default to standard error */
static	char	*udp_addr = NULL;
static	int	udp_fd = 0;

void ng_bleat_setudp( int fd, char *to )
{
	udp_fd = fd;
	udp_addr = to;
}

/* allow user to override stderr */
void ng_bleat_setf( Sfio_t *f )
{
	fd = f;
}

void ng_bleat( int vlevel, char *fmt, ...)
{
	extern int verbose;
	va_list	argp;			/* pointer at variable arguments */
	char	obuf[NG_BUFFER];	/* final msg buf - allow ng_buffer to caller, 1k for header*/
	char	time[150];		/* time according to ningaui */
	ng_timetype gmt;		/* now in gmt */
	char	*uidx; 			/* index into output buffer for user message */
	int	space; 			/* amount of space in obuf for user message */
	int	hlen;  			/* size of header in output buffer */

	if( vlevel > (verbose & 0x0f ) )		/* verbose now must be in the range of 0-15 */
		return;

	if( fd == NULL )	
		fd = sfstderr;			/* default to stderr */

 	gmt = ng_now( );                     /* get current gmt */
 	ng_stamptime( gmt, 0, time );        /* format time into ningaui standard */
 
	sfsprintf( obuf, 1023, "%I*u[%s] ", sizeof( gmt ), gmt, time );

	hlen = strlen( obuf );          
	space = NG_BUFFER - hlen;             /* space for user message */
	uidx = obuf + hlen;                   /* point past header stuff */

	va_start( argp, fmt );                       /* point to first variable arg */
	vsnprintf( uidx, space - 1, fmt, argp );
	va_end( argp );                                 /* cleanup of variable arg stuff */

	if( udp_addr )
		ng_writeudp( udp_fd, udp_addr, obuf, strlen( obuf ) );
	else
		sfprintf(  fd, "%s\n", obuf );

	if( vlevel < 0 )
		ng_log( vlevel * -1, obuf );
}



/* catigory oriented bleating */
void ng_cbleat( int cat, int vlevel, char *fmt, ...)
{
	static	int vglobal = -1;	/* the global setting the last time we were invoked */
	static 	int cat_mask;		/* catigory mask taken from global verbose  (verbose >> 8) */
	static	int c_level;		/* catigory level */

	extern int verbose;
	va_list	argp;			/* pointer at variable arguments */
	char	obuf[NG_BUFFER];	/* final msg buf - allow ng_buffer to caller, 1k for header*/
	char	time[150];		/* time according to ningaui */
	ng_timetype gmt;		/* now in gmt */
	char	*uidx; 			/* index into output buffer for user message */
	int	space; 			/* amount of space in obuf for user message */
	int	hlen;  			/* size of header in output buffer */


	if( vglobal != verbose )	/* change in verbose or first call; avoid doing calc each time */
	{
		c_level = (verbose >> 4) & 0x0f;	/* shift off regular verbose level and snarf cat level */
		cat_mask = verbose >> 8;		/* shift by a nibble to get catigory */

		vglobal = verbose;
	}

	if( cat  && !(cat & cat_mask ) )		/* not this catigory */
		return;

	if( c_level < vlevel )			/* global level lower than level on bleat */
		return;

	if( fd == NULL )	
		fd = sfstderr;			/* default to stderr */

 	gmt = ng_now( );                     /* get current gmt */
 	ng_stamptime( gmt, 0, time );        /* format time into ningaui standard */
 
	sfsprintf( obuf, 1023, "%I*u[%s][%02d] ", sizeof( gmt ), gmt, time, cat*10+vlevel );

	hlen = strlen( obuf );          
	space = NG_BUFFER - hlen;             /* space for user message */
	uidx = obuf + hlen;                   /* point past header stuff */

	va_start( argp, fmt );                       /* point to first variable arg */
	vsnprintf( uidx, space - 1, fmt, argp );
	va_end( argp );                                 /* cleanup of variable arg stuff */

	if( udp_addr )
		ng_writeudp( udp_fd, udp_addr, obuf, strlen( obuf ) );
	else
		sfprintf(  fd, "%s\n", obuf );

	if( vlevel < 0 )
		ng_log( vlevel * -1, obuf );
}

#ifdef SELF_TEST
int verbose = 0;
main( int argc, char **argv )
{

	int i = 0;
	int cat;

	ARGBEGIN
	{
		case 'v': OPT_INC( verbose ); break;
usage:		
		default:
			fprintf( stderr, " enter with -v0xMCV value (Mask, Clevel, Vlevel)\n" );
			exit( 1 );
	}
	ARGEND

	/* test all combinations of bleat and level 0-9 */
	for( i = 0; i < 16; i++ )
		ng_bleat( i, "verbose level %d test. level=%d",  i, verbose & 0x0f );

	/* test all catigory and verbose levels 0-9 */
	for( cat = 0; cat <= 8; cat++ )
		for( i = 0; i < 16; i++ )
			ng_cbleat( cat, i, "cbleat test level %d,%d test. cat=%02x level=%d", cat, i, verbose >> 8, (verbose >>4) & 0x0f );

	/* other random examples */
	ng_cbleat( CBLEAT_NET, 1, "cbleat NET,1 message" );
	ng_cbleat( CBLEAT_LIB, 1, "cbleat LIB,1 message" );
	ng_cbleat( CBLEAT_NET + CBLEAT_LIB, 1, "cbleat NET+LIB,1 message" );
	
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_bleat:Babble on to file depending on verbose level)

&space
&synop	ng_bleat( int level, char *fmt, ...)
&break	ng_cbleat( int catigory, int level, char *fmt, ...)
&break	ng_bleat_setudp( int udp_fd, char *to ) .br
&break	ng_bleat_setf( Sfio_t *f ) .br

&space
&desc	&This is a  &lit(printf) like function that accepts a level as the first 
	parameter and will formate and write a string write to the an output file only if the 
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
&subcat ng_cbleat( int catigory, int level, char *str, ...)
	Like &lit(ng_bleat) this function writes the formatted message out, 
	however it uses both catigory and level values to determine when to 
	supress messages.  
	This function treats the global verbose value as:
&space
&litblkb
   MMMMMMMM CCCCVVVV
&litblke
&space
	Where:
&begterms
&term	VVVV : is the regular bleat level value (0-15).
&term	CCCC : is the catigory bleat level value.
&term	MMMMMMMM : Is the catigory mask.
&endterms
&space
	
	The message passed to ng_cbleat() is written to standard out if
	the level is greater than or equal to the catigory bleat level portion 
	of the global verbose value, and if the catigory mask flag passed in 
	is set in the mask portion of the global variable. 
	
&space
	Thus, if the global verbose value is 0x212, calls to &lit(ng_cbleat)
	produce messages only if &lit(catigory ^& 0x02) is true AND
	level is greater or equal to 1. 
	A catigory of 0 is treated as always true, and the message will 
	be written using just the level setting (ng_bleat functionality). 

&space
	The double verbose level is used so that application (or regular bleat
	messages) can be disabled, while allowing a multi layered amount of 
	catigory messages to be output. 
	

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

&examp
	The following are some examples of calling ng_bleat and ng_cbleat:
&space
&litblkb
	ng_bleat( 1, "message when level >= 1 );
	ng_bleat( 0, "message always written" );

	ng_cbleat( CBLEAT_NET, 0, "always when net mask is on" );
	ng_cbleat( CBLEAT_LIB, 1, "lib mask and verbose level >= 1" );
	ng_cbleat( CBLEAT_NET + CBLEAT_LIB, 1, "lib or netmask and level >= 1" );
&litblke
&space
	Programmes using the standard ningaui argument processing, should
	set the verbose level with &lit(OPT_INC(verbose);) which can handle
	the command line argument &lit(-v0x213) allowing the effective setting 
	of catigory 2 level 1 for ng_cbleat() calls, and level 3 for ng_bleat() calls. 
	This is the same as -v531, but using a decimal
	value in relation to catigory level setting is not as obvious.  
	An 'old style' setting of -v2 is backwards compatable as it sets the 
	bleat level to 2, and the cbleat mask to 0. 
&space
	It is certainly possible for any application to set the catigory mask
	based on command line input and thus might accept multiple command line
	options that set catigory masks independantly of the standard -v option.
&space

&space
&see	&seelink(lib:ng_netif)
&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	25 Jun 2001 (sd) : Birth.
&term 	13 Sep 2004 (sd) : To ensure stdarg.h is included before stdio.h.
&term	31 Jul 2005 (sd) : changes to eliminate gcc warnings.
&term	01 Feb 2008 (sd) : Added cbleat. 
&term	09 Jun 2009 (sd) : Added finishing touches to cbleat. 
&endterms

&scd_end
------------------------------------------------------------------ */
