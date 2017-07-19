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
* Mnemonic:	ng_read
* Abstract:	provides a read with a timeout
*
*		NOTE: This function is included directly by no_ast.c in order to 
*		build a small library of functions that do not use ast routines.
*		Thus, dont add anything here that is ng_ or ast oriented.
*		
* Date:		29 September 2004
* Author:	E. Scott Daniels
* Mod:		31 Jul 1005 (sd) : changes to prevent gcc warnings.
* ---------------------------------------------------------------------------------
*/

#include <unistd.h>
#include <sys/types.h>


#ifdef SELF_TEST
#define SANS_AST 1
#include "ng_testport.c"
#endif 

#ifndef SANS_AST
#include <stdio.h>
#include <ningaui.h>
#include <ng_lib.h>
#endif

int ng_read( int fd, char *buf, size_t len, int timeout )
{
	if( timeout )
		if( ng_testport( fd, timeout ) <= 0 )		/* -n error; 0 no data */
			return -2;

	return read( fd, buf, len );
}

#ifdef SELF_TEST
main( )
{
	char buf[100];
	int s;

	fprintf( stdout, "reading from fd=2 timeout=5 -- we expect failure..." );
	fflush( stdout );
	s = ng_read( 2, buf, 10, 5 );
	printf( "status = %d (%s); this is %s\n", s, s > 0 ? "successful" : "failed", s > 0 ? "bad" : "GOOD" );
	printf( "please enter a small bit on stdin in the next 10 seconds\n" );
	s = ng_read( 0, buf, 90, 10 );
	printf( "data read from stdin; status == %d (%s)\n", s, s > 0 ? "successful" : "timedout" );
}
#endif

#ifdef NEVER_DEFINED
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_read:Raw read with timeout)

&space
&synop	ng_read( int fd, char *buf, size_t buflen, int timeout )
&space

&desc &This reads from the inidcated filedescriptor using the lowlevel
	read() system call.  A timeout on the read is implemented and 
	a return code of -2 is returned if the timer expired before there
	was any data to read on the file descriptor. If data becomes available 
	before the timeout period expires, the result of the read() is returned.

&space
&parms	The parameters expected are:
&begterms
&term	fd : The open file descriptor.
&term	buf : Poointer to the user buffer to fill with data.
&term	buflen : The size of buf, or the max number of bytes to read into buf.
&term 	timeout : The timeout period in seconds. If timeout is zero, then the read
	will block until data is available. 
&endterms


&space
&exit	The return value is the result of the read() system call, or a -2 if 
	the timeout period expired before data was available to read. 

&space
&see	ng_testport()

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	29 Sep 2004 (sd) : Fresh from the oven.
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
