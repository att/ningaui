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
***************************************************************************
*
* Mnemonic: ng_siestablish
* Abstract: This routine will open and bind a file descriptor to a socket.
*           It will create a transport provider block to manage any comm
*           (UDP or TCP) that might occur through the fd.
* Parms:    gptr     - Pointer to the global information block
*           dname    - Pointer to a string indicating TCP or UDP.
*           port     - Specific port number 0 == any port
* Returns:  Pointer to a tp_blk structure, NULL if error.
* Date:     26 March 1995
* Author:   E. Scott Daniels
* Modified: 19 Apr 1995 - To keep returned address of the port.
*		08 Apr 2004 (sd) - added reuse socket option
*		29 Mar 2007 (sd) - corrected memory leak (dup alloc of addr)
*		02 Apr 2007 (sd) - Corrected double free of addr.
*
* NOTE:     It is assumed that the bind call will fail if it is unable to
*           allocate the requested, non-zero, port. This saves us the
*           trouble of checking the assigned port against the requested one.
***************************************************************************
*/
#include "sisetup.h"       /* include the necessary setup stuff */
#include <netinet/in.h>
#include <netinet/tcp.h>

struct tp_blk *ng_siestablish( char *dname, int port )
{
	extern struct ginfo_blk *gptr;
	extern int si_tcp_reuse_flag;
	extern int si_udp_reuse_flag;
	extern int si_no_delay_flag;
 
 	struct tp_blk *tptr;         	/* pointer at new tp block */
 	int 	status = OK;            /* processing status */
 	struct sockaddr_in *addr;    	/* IP address we are requesting */
 	int 	protocol;               /* protocol for socket call */
	int reuse_flag = 0;		/* we set locally based on the socket type and the type specific global flag */
 	char buf[50];                	/* buffer to build request address in */
	
 	tptr = (struct tp_blk *) ng_sinew( TP_BLK );     /* make new block */

 	if( tptr != NULL )
  	{
   		switch(  *dname )
    		{
			case 'u':
			case 'U':                     /* set type to udp */
 				tptr->type = SOCK_DGRAM;
 				protocol = IPPROTO_UDP;      			/* set protocol for socket call */
				reuse_flag = si_udp_reuse_flag ? 1 : 0;		/* enforce 1/0 */
 				break;
		
			case 't':
 			case 'T':
 			default:                     /* allothers set to TCP */
				reuse_flag = si_tcp_reuse_flag ? 1 : 0;
 				tptr->type = SOCK_STREAM;   			/* set to stream oriented stuff */
 				protocol = IPPROTO_TCP;     			/* set protocol for socket call */
    		}     
		
   		if( (tptr->fd = socket( AF_INET, tptr->type, protocol )) >= OK )
    		{                                          
			sprintf( buf, "00.00.00.00:%d", port );   				/* make addr using caller's addr:port */
			addr = ng_dot2addr( buf );		
		
        		if( tptr->type == SOCK_STREAM  && si_no_delay_flag )
                		setsockopt(tptr->fd, IPPROTO_TCP, TCP_NODELAY, &si_no_delay_flag, sizeof(si_no_delay_flag) );
	
#ifdef OS_FREEBSD
			if( tptr->type == SOCK_STREAM )
  				setsockopt(tptr->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_flag, sizeof( reuse_flag ) ) ;
			else
  				setsockopt(tptr->fd, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse_flag, sizeof( reuse_flag ) ) ;	/* set, allows multiple procs for mcast */
#else
  			setsockopt(tptr->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_flag, sizeof( reuse_flag ) ) ;
#endif
	
			status = bind( tptr->fd, (struct sockaddr *) addr, sizeof( struct sockaddr ));
		
			tptr->addr = addr;         
    		}
   		else                 
    			status = ! OK;       					/* socket call failed - force bad return later */
	
   		if( status != OK )    							/* socket or bind call failed - clean up stuff */
    		{
			ng_sitrash( TP_BLK, tptr );					/* this also frees addr as it is pointed to by the block at this point */
    		 
    	 		tptr = NULL;        /* set to return nothing */
    		}
  	}                  
	
 	return tptr;        
}                  
