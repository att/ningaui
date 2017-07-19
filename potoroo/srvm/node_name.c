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
  --------------------------------------------------------------------
  Mnemonic: 	node_name
  Abstract: 	populate a buffer with the name of our node
  Date:		19 February 2005 (HBD EKD 16!)
  Author: 	E. Scott Daniels
  Mods:
  --------------------------------------------------------------------
*/

#include <string.h>
#include <ningaui.h>
#include <ng_lib.h>

char *node_name( void )
{
	char buf[1024];
	char *p;

	ng_sysname( buf, 1000 );
	if( (p = strchr( buf, '.' )) )
		*p = 0;				/* we want nothing of the domain */

	return ng_strdup( buf );
}
