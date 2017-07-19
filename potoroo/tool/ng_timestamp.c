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
#include	"ng_timestamp.man"

int
main(int argc, char **argv)
{
   char *sdate= NULL;
   ng_timetype t;

	ARGBEGIN
	{
		default:
usage:
			sfprintf( stderr, "usage: %s yyyymmddhhmmss\n", argv0 );
			exit( 1 );
	}
	ARGEND

	sdate =	argv[0];

   t = ng_timestamp(sdate);

   sfprintf(sfstdout, "%I*d\n", Ii(t) );


   exit(0);
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_timestamp:Convert time to a Ningaui timestamp)

&space
&synop	ng_timestmap yyyymmddhhmmss

&space
&desc	This is deprecated.  Instead of this, use: &lit(ng_dt -i yyyymmddhhmmss) 

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
