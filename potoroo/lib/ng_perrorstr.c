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

#include	<string.h>
#include	<stdio.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>


/* this is needed only if -no-cpp-precomp is removed from cflags */
#ifndef xOS_DARWIN
extern int sys_nerr;
#ifdef OLD_GECKO
#ifdef OS_IRIX
extern char *sys_errlist[];    /* declared in stdio.h  as const */
#else
extern const char *sys_errlist[];    /* declared in stdio.h  as const */
#endif
#endif
#endif

/* both linux and darwin suck....
	linux:  strerror_r does not fill in the buffer but return a pointer to the string even
		though the prototype says it returns int with 0 being success. Further, the 
		prototype for strerror is either wrong or nonexistant in linux.

	darwin: Has not evloved quite yet... strerror_r does not exist
*/

char *xstrerror( int errno );		/* no prototype seems to work in linux */

/* added const to prevent warning on the return statement */
const char *
ng_perrorstr(void)
{
	static char buf[128];

#ifdef GOT_IT_RIGHT
	*buf = 0;
	strerror_r( errno, buf, sizeof( buf ) );
#else
	char *c;

	c = strerror( errno );		/* no prototype seems to work in linux */
	sfsprintf( buf, sizeof( buf ), "%s", c );
	/* free( c );			possible memory leak, but darwin returns constant buffer */
#endif

	return buf;
	

#ifdef ORIG_GECKO
sys_errlist deprecated in linux 2.6 -- 
	if((unsigned)errno < sys_nerr)
		return(sys_errlist[errno]);
	sprintf(buf, "Unknown error %d", (unsigned)errno);
	return buf;
#endif 
}

void
perror(const char *s)
{
	if( argv0 )
		sfprintf(sfstderr, "%s: ", argv0);

	if(*s)
		sfprintf(sfstderr, "%s: ", s);

	sfprintf(sfstderr, "%s\n", ng_perrorstr());
}

#ifdef SELFTEST
#include <fcntl.h>
main( )
{
	int f;
	int e;


	open( "no_such_file", O_RDONLY, 0 );
	e = errno;
	perror( "no_such_file -- argv0 empty" );
	argv0 = "selftest!";
	perror( "no_such_file -- argv0 defined" );
	printf( "errno was: %d %s\n", e, strerror( e ) );
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_perrorstr:Ningaui implementation of perrorstr)

&space
&synop	char *ng_perrorstr( void )
	void perror( const char *str )

&space
&desc	&ital(Ng_perrorstr) and &ital(perror) functions provide a
	error message interface to the standard error device.
&space
	&ital(Ng_perrorstr) will look up the error message string 
	that corresponds to the system &lit(errno) integer value.
	A pointer to the string is returned to the caller.

&space
	&ital(perror) will at most three items to the standard error device.
	The first item written is the value in the global variable &lit(argv0) 
	(assumed to be 
	the programme name). 
	If &ital(str) is not &lit(NULL), then
	the string it references is written followed by a trailing colon (:).
	Lastly the string associated with the current value of the 
	system &lit(errno) variable is printed.

&space
&see	perror(3), strerror(3)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 28 Mar 2001 (sd) : Converted from Gecko. Added SCD.
&term	21 Apr 2004 (sd) : Converted to use strerror_r().
&term	29 Jul 2005 (sd) : Corrected include of strings.h to string.h.  Causing core dumps on opteron.
&endterms

&scd_end
-------------------------------------------------------------------*/
