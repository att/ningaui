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
 ----------------------------------------------------------------------
 Mnemonc:	nar_cond.c
 Abstract: 	tests a conditional request returning true or false (0).
		The syntax expected in buffer is:
			{s|n}{==|!=|<[=]|>[=]}<value>

		s indicates a string compare, n a numeric (integer) compare.
		s/n is replaced with the current value for the variable 
		name passed in. If the variable is not defined, -1 is 
		used (numeric) and null string for string compares.
 Date:		28 Oct 2004
 Author: 	E. Scott Daniels
 ----------------------------------------------------------------------

*/

#ifdef SELF_TEST
#include "nar_const.h"
#endif

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_lib.h>
#include	<ng_ext.h>


/* see if the value for variable vname meets the condition */
int static test_value( char *vname, char* cond )
{
	char	*val = NULL;	
	long long	ival = 0;
	int	type;


	val = lookup_value( vname );

	switch( *cond )
	{
		case 's':	
			if( !val )
				val = "";
			switch( *cond )
			{
				case '=':
					cond++;
					if( *cond == '=' )
						cond++;
					return strcmp( cond, val ) == 0;

				case '!':
					cond++;
					if( *cond == '=' )
						cond++;
					return strcmp( cond, val ) != 0;

				case '<':						/* <[=] and >[=] are dicy at best -- should not depend on them */
					cond++;
					if( *cond == '=' )
						return strcmp( cond+1, val ) <= 0;
					return strcmp( cond, val ) < 0;

				case '>':
					cond++;
					if( *cond == '=' )
						return strcmp( cond+1, val ) >= 0;
					return strcmp( cond, val ) > 0;


				default:	return 0; 
			}
			break;

		default: 
			cond++;
			ival = val ? strtoll( val, 0, 0 ) : -1;	
			switch( *cond )
			{
				case '!':
					cond++;
					if( *cond == '=' )
						cond++;
					return ival != strtoll( cond, 0, 0 );

				case '<':
ng_bleat( 2, "cond: cval=%lld cond=%lld", ival, strtoll(cond+1, 0, 0) );
					cond++;
					if( *cond == '=' )
						return ival <= strtoll( cond+1, 0, 0 );
					return ival < strtoll( cond, 0, 0 );

				case '>':
					cond++;
					if( *cond == '=' )
						return ival >= strtoll( cond+1, 0, 0 );
					return ival != strtoll( cond, 0, 0 );

				case '=':
					cond++;
					if( *cond == '=' )
						cond++;
					return ival == strtoll( cond, 0, 0 );

				default:	return 0; 
				
			}
			break;
		
	}

	
}
