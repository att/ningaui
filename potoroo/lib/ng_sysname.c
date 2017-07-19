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

#include	<sfio.h>
#include	<string.h>
#include	"ningaui.h"
#include	<sys/utsname.h>

int
ng_sysname(char *buf, int size)
{
	struct utsname u;

	if(uname(&u) < 0){
		perror("uname");
		return -1;
	}
	if(strlen(u.nodename)+1 > size){
		sfprintf(sfstderr, "%s: system name '%s' too big\n", argv0, u.nodename);
		return -1;
	}
	strcpy(buf, u.nodename);
	return 0;
}




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sysname:Fill a buffer with the current system name)

&space
&synop
	#include <ningaui.h>
&break;	#include <ng_lib.h>
&space
	int ng_sysname( char *buf, int size )

&space
&desc
	&This will fetch the currently defined name for the system, and place up to 
	&lit(size) characters of the name into &lit(buf). 

&space
&see

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Feb 1997 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

