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

#include	<sys/types.h>
#include	<sys/fcntl.h>
#include	<stdlib.h>
#include	<sfio.h>
#include	<sys/time.h>
#include	"ningaui.h"

double hrtime_scale = 1e-6;

ng_hrtime
ng_hrtimer(void)
{
	struct timeval t;
	ng_hrtime ret;

	if(gettimeofday(&t, 0) < 0){
		return 0;
	}
	ret = t.tv_sec;
	ret *= 1000000;
	ret += t.tv_usec;

	return ret;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_hrtimer:High Resolution Time)

&space
&synop
	#include <ningaui.h>
	#include <ng_lib.h>

	ng_hrtime ng_hrtimer( )
&space
&desc
	&This fetches the current time and returns the value as both 
	seconds and micro seconds as one integer value.

&space
&see	ng_time

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Feb 1997 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

