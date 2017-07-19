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

#include	<stdio.h>
#include	<stdlib.h>
#include	<time.h>
#include	<unistd.h>
#include	<sfio.h>

#include	"ningaui.h"

/*
	strategy: two ways to introduce randomness (== unpredictability
	to forecast next number produced).

	1) set seed in an arbitrary way
	2) start taking numbers from an arbitrary point along the sequence
		starting from 1)
*/

void
ng_srand(void)
{
	int pid;

	pid = getpid()%30000;
	/*
		srand48 only takes 32 bits. time(0) will give us 29 or so.
		next obvious thing is to smear the pid into this. we note
		71569 is the largest prime <= 2^31/30000 (30000 is largest pid).
	*/
	srand48(time(0) ^ (71569 * pid));
	/*
		get the congruential pipeline flowing, introducing
		more randomness based on the pid
	*/
	while(pid-- > 0)
		drand48();
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srand:Seed Random Number Generation)

&space
&synop	
	#include <ningaui.h>
&space
&break	ng_srand()

&space
&desc
	&This provides an alterate method of seeding the drand48() random number 
	generation.  It provides different seeding when multiple processes on the 
	same node invoke the seed function at the same time.  This function uses 
	both the time of day and the process ID to seed the random number generator, 
	and then makes an arbitrary number of calls to drand48() to further 
	introduce a difference between concurrently running processes. 

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Feb 1997 (ah) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

