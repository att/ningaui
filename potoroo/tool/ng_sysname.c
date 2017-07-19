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
#include	<string.h>
#include	<sys/types.h>
#include	<sys/stat.h>

#include	<sfio.h>
#include	"ningaui.h"

#include	"ng_sysname.man"

int
main(int argc, char **argv)
{
	char	*p;
	int	full = 0;
	char	name[NG_BUFFER];

	ARGBEGIN{
		case 'f':	OPT_INC( full ); break;
		case 's':	full = 0; break;
		case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf( sfstderr, "Usage: %s [-s|f] [-v]\n", argv0);
			exit(1);
	}ARGEND

	if(argc != 0)
		goto usage;

	if(ng_sysname(name, sizeof name)){
		ng_log(LOG_ERR, "ng_sysname failed!!\n");
		printf("NoNoDe\n");
		exit(1);
	}

	if( (! full) && (p = strchr( name, '.' )) )
		*p = 0;

	sfprintf( sfstdout, "%s\n", name);

	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_sysname -- Print system name
&name   &utitle

&synop  ng_sysname [-f|s] [-v]

&desc	&ital(Ng_sysname) prints the current system name. Use it, and not almost
	portable alternatives like &ital(hostname) or &ital(uname).

&opts   &ital(Ng_sysname) understands the following options:

&begterms
&term	-f : print the full name (e.g. ning02.att.research.com)
&term	-s : print the short name (default).
&term 	-v : be chatty (verbose). More &lit(-v) options induce logorrhea.

&endterms


&exit	An exit code of 1 indicates some error.

&space
&owner(Scott Daniels)
&scd_end
*/
