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
******************************************************************************
*
*  Mnemonic: ng_sigetlfd
*  Abstract: This returns the fd that was opened as the primary listening socket.
*		makes shutting off connections while draining easy.
*  Returns:  the file descriptor or -1
*  Date:     3 May 2006
*  Author:   E. Scott Daniels
*
*  Modified: 
******************************************************************************
*/
#include "sisetup.h"

int ng_sigetlfd( )
{
	extern struct ginfo_blk *gptr;

	if( gptr && gptr->magicnum == MAGICNUM )   /* good cookie at the gptr address? */
		return gptr->primary_listen_fd;
	
	return -2;		/* just incase they want to know there was an error */
}
