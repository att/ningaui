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

#include <sfio.h>
#include <unistd.h>
#include <stdlib.h>

#include "ningaui.h"

#include "rm_join.man"

int
main(int argc, char **argv)
{
	int c1, c2;

	ARGBEGIN{
	case 'v':	OPT_INC(verbose); break;
	default:	goto usage;
	}ARGEND
	if(argc != 0){
usage:
		sfprintf(sfstderr, "Usage: %s [-v]\n", argv0);
		exit(1);
	}

#define	IN()	(sfgetc(sfstdin))
#define	OUT(x)	sfputc(sfstdout, (x))

	c1 = IN();
	while(c1 != EOF){
		c2 = IN();
		if((c1 == '\n') && (c2 == '\t')){
			OUT('#');
			c1 = IN();
		} else {
			OUT(c1);
			c1 = c2;
		}
	}

	exit(0);
	return 0;
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_rm_join:Perform a join suitable for repmgr dump output)

&space
&synop	ng_rm_join [-v]


&space
&desc	&ital(Ng_rm_join) performs this pseudo ed command on its stdin
	&cw(s/\n\t/#/g) on the way through to its stdout.
	It is useful for getting all the info for a single file on one line.
	It also works well for &ital(ng_repmgr)'s &cw(explain) output.

&space
&opts	The following options are recognised by &ital(ng_log):
&begterms
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms
&space
&exit	When the binary completes, the exit code returned to the caller 
	is set to zero (0) if the number of parameters passed in were 
	accurate, and to one (1) if an insufficent number of parameters
	were passed. 

&space
&see	ng_repmgr

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	06 Feb 2002 (agh) : Original code 
&endterms
&scd_end
*/
