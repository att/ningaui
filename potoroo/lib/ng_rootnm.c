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
# Mnemonic:	ng_rootnm
# Abstract:	Builds an NG_ROOT based name from teh string passed in
#		
# Date:		26 August 2003		
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include        <unistd.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <sys/types.h>
#include        <sys/stat.h>
#include        <string.h>
#include        <ctype.h>

#include        <sfio.h>
#include        <ningaui.h>
#include        <ng_ext.h>
#include        <ng_lib.h>



char *ng_rootnm( char *s )
{
	static char *ng_root = NULL;
	char buf[NG_BUFFER];

	if( ! ng_root )
	{
		if( ! (ng_root = ng_env( "NG_ROOT" ))  )
		{
			ng_bleat( 0, "ng_rootnm: cannot find NG_ROOT in environment/cartulary" );
			exit( 1 );
		}
	}

	if( *s == '/' )
		s++;
	sfsprintf( buf, sizeof( buf ), "%s/%s", ng_root, s );
	return strdup( buf );
}

#ifdef SELF_TEST
main( )
{
	char *p;

	printf( "(%s)\n", ng_rootnm( "/scooter/pies" ) );
	printf( "(%s)\n", ng_rootnm( "scooter/pies" ) );
}
#endif

#ifdef KEEP
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rootnm:Build a pathname based on the NG_ROOT value)

&space
&synop	char *ng_rootnm( char s )

&space
&desc	&This will create a pathname based on the current value of NG_ROOT
	using the string that is passed in. 

&space
&parms 	The caller must supply a NULL terminated ascii path to append to the 
	NG_ROOT value.  The string may contain a lead slash.

&space
&exit	Returns the pointer to the new pathname.

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Aug 2003 (sd) : Thin end of the wedge.
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
