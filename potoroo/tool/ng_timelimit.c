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

/* timelimit -- run a program but die with a SIGALRM if it takes longer
   than a given amount of wall clock time to run. */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <ningaui.h>
#include "ng_timelimit.man"

void usage( )
{
	fprintf(stderr, "Usage: timelimit seconds command args...\n");
	exit( 1 );
}

int
main (int argc, char **argv)
{
	int timelimit;

	ARGBEGIN
	{
		default:
usage:
			usage( );
			exit( 1 );
	}
	ARGEND

	if (argc < 3) {
		exit(1);
	}

	timelimit = atoi( *argv );

	if (timelimit <= 0) {
		fprintf(stderr, "Usage: timelimit seconds command args...\n");
		exit(2);
	}
	alarm((unsigned int)timelimit);

	execvp(argv[1], &argv[1]);
	perror("Cannot exec");
	exit(3);
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_timelimit:Execute a command and pop if it runs too long)

&space
&synop	ng_timelimit seconds command-string

&space
&desc	&This executes the command string supplied on the command line. If the command
	runs for longer than the number of seconds provided as the first parameter, 
	then the process is killed. 
&space
	The command is started using an exec() family function call and thus the
	time limit could be thwarted if the invoked command adjusts the alarm function.
	

&space
&parms	The number of seconds to allow the command to run and the command string are 
	required on the command line. 

&space
&exit	The exit code of the invoked command should be returned.

&space
&mods
&owner(Mark Plotnick)
&lgterms
&term	19 Dec 2006 (sd) : Added man page.
&endterms


&scd_end
------------------------------------------------------------------ */
