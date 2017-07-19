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
#include	<sfio.h>
#include	"ningaui.h"

ng_int64
btoi(ng_byte *b, int n)
{
	ng_int64 r;

	r = 0;
	while(--n >= 0) {
		r = (r<<8) | (*b++);
	}
	return r;
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_binary.c:Binary conversion routines)

&space
&synop	#include <ningaui.h>
&break	#include <ng_lib.h>
&break	ng_int64 btoi( ng_byte *b, int n )

&space
&desc
	&ital(Ng_byte) converts &lit(n) bytes of character data into an 
	integer. The integer value is returned. 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Feb 1997 (ah) : Created.
&term 	28 Mar 2001 (sd) : Converted from Gecko. Added SCD
&term	03 Oct 2007 (sd) : Added manual page. 
&endterms

&scd_end
#endif
-------------------------------------------------------------------*/
