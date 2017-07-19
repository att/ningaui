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
* -----------------------------------------------------
* Mnemonic:	ng_sleep
* Abstract: 	A thread safe sleep.
* Date:		10 Oct 2001
* Author: 	E. Scott Daniels
* -----------------------------------------------------
*/


#include	<stdio.h>
#include	<time.h>


#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_lib.h>

void ng_sleep( time_t secs )
{
	struct timespec ts;

	ts.tv_sec = secs;
	ts.tv_nsec = 0;
	
#ifdef OS_SOLARIS
	sleep( secs );
#else
	nanosleep( &ts, NULL );
#endif
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sleep:Thread safe sleep)

&space
&synop	ng_sleep( time_t seconds )

&space
&desc	Puts the thread to sleep for the indicated &ital(seconds.) The underlying
system interface is &lit(nanosleep.)

&space
&parms	The following describe the parameters expected:
&begterms
&term	seconds : The number of seconds to sleep. &This may return before the 
specified number of seconds has elapsed if the thread is interrupted. 
&endterms

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 Oct 2001 (sd) : Orignial.
&term	18 Apr 2003 (sd) : Added time.h header include (mac osx)
&term	13 Sep 2004 (sd) : Removed reference to stdarg.h.
&endterms

&scd_end
------------------------------------------------------------------ */
