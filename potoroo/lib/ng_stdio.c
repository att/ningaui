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
 ---------------------------------------------------------------------------------
 Mnemonic:	ng_stdio
 Abstract:	standard io support
		contains functions to support mixing and matching of sfio and 
		standard system i/o.  This is NOT a replacement for either.
		
 Date:		12 Dec 2003
 Author:	E. Scott Daniels
 ---------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<errno.h>
#include	<string.h>
#include	<fcntl.h>
#include	<sfio.h>

#include 	<ningaui.h>

void ng_open012( )
{
	int fd;

	if( (fd = open( "/dev/null", O_RDWR | O_CREAT, 0777 )) < 0 ) 
	{
		sfprintf( sfstderr, "ng_open012: unable to open /dev/null: %s", strerror( errno ) );
		exit( 1 );
	}

	while( dup( fd ) < 3 );		/* dup guarenteed to return lowest not in use */

	if( fd > 2 )
		close( fd );		/* dont need to keep this one up */
}


#ifdef KEEP
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_stdio:Ningaui Standard I/O Support)

&space
&synop	
	#include <ningaui.h>
	ng_open012()

&space
&desc
&subcat ng_open012
	This function ensures that file descriptors 0, 1 and 2 are  open. If they 
	are not, then they are opened and pointed to &lit(/dev/null.)  Odd things 
	seem to happen, especially with the AST software, if these filedescriptors
	are not open.   This function is called automatically by the ARGBEGIN processing.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Dec 2003 (sd) : Thin end of the wedge.
&term	31 Jul 2005 (sd) : Changes to prevent gcc warnings.
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
