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
# Mnemonic:	green_light
# Abstract:	This process is started at the very end of initialisation (ng_node)
#		and is an indication, to all that care, that it is safe to run (all
#		initialisation is done).  At node stop time, this is the first process
#		killed.  The presence of this process only indicates that the node 
#		has been initialised, and is not pending shutdown.  It does NOT
#		indicate that any/all daemons are still healthy.
#		
# Date:		02 March 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include <unistd.h>
#include	<stdlib.h>
#include	<ningaui.h>


#include	"green_light.man"


int main( int argc, char **argv )
{
	ARGBEGIN
	{
usage:
		default:
			sfprintf( sfstderr,  "usage: %s\n", argv0 );
			exit( 1 );
	}
	ARGEND

	while(  1 )
		sleep( 5000 );
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_green_light:Indicator process that all is well)

&space
&synop	ng_green_light

&space
&desc	This process is started by ng_node as the last step during node initialisation.
	The process should run until ng_node is invoked to stop the system. 
	When ng_node is invoked to perform node shutdown, this is the first process that
	is killed.
	While the process is running it is safe to assume just two things:
&space
&smterms
&term	1 : Initialisation has completed and the cartulary has been rebuilt after startup so 
	any changes that were made while down are in place. 

&space
&term	2 :  The node has not started a shutdown process. 
&endterms
&space
	

&space
&space
&exit	This should never exit, so any exit code is a problem.

&space
&see	ng_init_state

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Mar 2005 (sd) : Sunrise.
&term	18 Dec 2006 (sd) : Fixed --man.
&endterms

&scd_end
------------------------------------------------------------------ */
