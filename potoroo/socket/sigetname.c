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
 ------------------------------------------------------------------------
 Mnemonic:	sigetname
 Abstract:	returns the name of the socket for a give sid
 Parms:		sid - the socket id as returned by open/listen/connect
 Date:		21 July 2003 
 Author: 	E. Scott Daniels
 ------------------------------------------------------------------------
*/
#include "sisetup.h"

char *ng_sigetname( int sid )
{ 
 	struct sockaddr oaddr;     /* pointer to address in TCP binary format */
	int	len;

	/* as long as the sid that the user is given is the fd, then this is easy */
	len = sizeof( oaddr );
	getsockname( sid, (struct sockaddr *) &oaddr, &len );
	return ng_addr2dot( (struct sockaddr_in *)  &oaddr );
}
