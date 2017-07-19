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

#include        <sfio.h>
#include        <stdlib.h>
#include        "ningaui.h"

#include	"ng_stamptime.man"

int
main(int argc, char **argv)
{
   ng_timetype t = 0;
   char buf[512];
   char *ds;

	ARGBEGIN
	{
		default:
usage:
			sfprintf( stderr, "usage: %s ningaui-timestamp\n", argv0 );
			exit( 1 );
	}
	ARGEND

	t =	atoll( *argv );

   ds = (char *)ng_stamptime(t, 0, buf);

   sfprintf(sfstdout, "%s\n", ds);

   exit(0);
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_stamptime:Convert Ningaui timestampe to formatted string)

&space
&synop	ng_stamptime timetamp

&space
&desc	This is deprecated.  Instead of this, use: &lit(ng_dt -p timestamp) 

&space
&see	ng_dt

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 Dec 2006 (sd) : Added man page.
&endterms


&scd_end
--------------------------------------------------------------------------*/
