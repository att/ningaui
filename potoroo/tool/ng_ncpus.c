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

/* -----------------------------------------------------------------
# Abstract: 	 determine the number of cpus on the local system
# Mod:		2 Apr 2003 (sd) : to support bsd and darwin environments
# Author:	E. Scott Daniels
 ---------------------------------------------------------------------
*/

#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
 
#include <sfio.h>
#include "ningaui.h"
 
#include "ng_ncpus.man"


#define FL_MEM 	0x01
#define FL_BYTE 0x02

int main( int argc, char **argv )
{

	ARGBEGIN 
	{
		case 'b':	
			sfprintf( sfstdout, "%d ", ng_byteorder( ) );
			break;

		case 'm':
			sfprintf( sfstdout, "%d ", ng_memsize( ) );
			break;

		default:
usage:
			sfprintf( sfstderr, "\nusage: %s [-b] [-m]\n", argv0 );
			exit( 1 );
			break;
	}
	ARGEND

	sfprintf( sfstdout, "%d\n", ng_ncpu( ) );
	exit( 0 );
}

#ifdef KEEP
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_ncpus:Determine the number of cpus)

&space
&synop	ng_ncpus

&space
&desc	 &This makes the appropriate inquiry to determine the number of CPUs
	on the node and writes that number to the standard output device. 
	Currently the ng_ncpu() library function supports Linux, FreeBSD,
	Solaris, Irix, and Mac OS X.

&space
&opts	Several options are supported to provide other system information with 
	a single process invocation:
&begterms
&term	-b : Byte order.  (not supported)
&space
&term	-m : Memory size.  The number of bytes of system memory. 0, if the 
	memory size cannot be determined. 
&endterms

&space
	Additional information is written to standard output in the order of the flags.
	The number of CPUs is always written last.

&space
&exit	This script always exits with a good return code, and retuns a count of 
	1 if the actual number cannot be determined.

&space
&see	sysctl(8), procfs(5) ng_ncpu(lib)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Apr 2003 (sd) : Revamped to support mac and FreeBSD.
&term	08 Sep 2003 (sd) : Converted from a shell script 
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
