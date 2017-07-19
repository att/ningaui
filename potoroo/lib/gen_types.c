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
# Mnemonic:	gen_types.c
# Abstract:	Generate typedefs for ng_basic.h.
#		
# Date:		11 Oct 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include <ast_common.h>
#include <sfio.h>


void spit( char *s1, char *s2, char *s3 )
{
	if( ! *s3 )		/* did not resolve into a legit name */
	{
		sfprintf( sfstderr, "unable to compute typedef for %s\n", s1 );
		exit( 1 );
	}

	sfprintf( sfstdout, "typedef\t%s\t%s %s;\n", s1, s2, s3 );
}

#define INT	1
#define LONG	2
#define LONGLONG 3
#define SHORT	4


main( int argc, char **argv )
{
	int two = 0;
	int four = 0;
	int eight = 0;

	char *name[] = { "", "int", "long", "long long", "short" };

	if( argc > 1 )
	{
			sfprintf( sfstderr, "usage: %s\n", *argv );
			exit( 1 );
	}

	
	if( sizeof( short ) == 2 )
		two = SHORT;		/* can it be anything else? */

	if( sizeof( long ) == 4 )
		four = LONG;
	else
		if( sizeof( int ) == 4 )
			four = INT;

	if( sizeof( long ) == 8 )
		eight = LONG;
	else
		if( sizeof( long long ) == 8 )
			eight = LONGLONG;

	sfprintf( sfstdout, "/* WARNING: this file is generated during precompile -- do not edit! */\n\n" );
	sfprintf( sfstdout, "#ifndef ng_types_h\n" );
	sfprintf( sfstdout, "#define ng_types_h\t1\n\n" );

	spit( "unsigned", "char", "ng_byte" );

	spit( "\t", name[two], "ng_int16" );
	spit( "\t", name[four], "ng_int32" );
	spit( "\t", name[eight], "ng_int64" );

	spit( "unsigned", name[two], "ng_uint16");
	spit( "unsigned", name[four], "ng_uint32" );
	spit( "unsigned", name[eight], "ng_uint64" );
	sfprintf( sfstdout, "\n#endif\n" );

	exit( 0 );
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_gen_types:generate standard ng_intXX typedefs)

&space
&synop	ng_gen_types

&space
&desc	&This will generate the typedef statements for the various standard 
	ningaui types:
&beglist
&item	ng_int16
&item	ng_int32
&item	ng_int64
&item	ng_uint16
&item	ng_uint32
&item	ng_uint64
&item	ng_byte
&endlist

&space
	All output written to the standard output device. 

&space
&exit	Always zero.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	11 Oct 2004 (sd) : Fresh from the oven.
&endterms

&scd_end
------------------------------------------------------------------ */
