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
*  Mnemonic: ng_siconnect
*  Abstract: Establishes a TCP connection to processes listening at addr
*
*  Returns:  The session number if all goes well, SI_ERROR if not.
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*
*  Mod:		01 Mar 2007 (sd) - Added call to sitrash to junk tp storage.
******************************************************************************
*/
#include "sisetup.h"

int ng_siconnect( char *addr )
{
	extern struct ginfo_blk *gptr;
	extern int ng_sierrno;

	struct tp_blk *tpptr;       /* pointer to new block */
	struct sockaddr *taddr;     /* pointer to address in TCP binary format */
	int fd = ERROR;             /* file descriptor to return to caller */

	ng_sierrno = SI_ERR_HANDLE;
	if( gptr->magicnum != MAGICNUM )
		return( ERROR );                      /* no cookie - no connection */

	ng_sierrno = SI_ERR_TPORT;
	tpptr = ng_siestablish( TCP_DEVICE, 0 );  /* open new fd on any port */
	if( tpptr != NULL )
	{
		taddr = (struct sockaddr *) ng_dot2addr( addr );		/* convert ascii address to struct */

		ng_sierrno = SI_ERR_TP;
		if( connect( tpptr->fd, taddr, sizeof( struct sockaddr ) ) < OK )
		{								/* error -- cleanup and bail */
			close( tpptr->fd );         /* clean up fd and tp_block */

			ng_free( taddr );
			ng_sitrash( TP_BLK, tpptr );	/* free tpptr and all internal pointers */
			tpptr = NULL;

			fd = ERROR;                 /* send bad session id num back */
		}
		else                           /* connect ok */
		{
			ng_sierrno = 0;

			tpptr->flags |= TPF_SESSION;    /* indicate we have a session here */
			tpptr->paddr = (struct sockaddr_in *) taddr;    /* at partner addr */
			tpptr->next = gptr->tplist;     /* add block to the list */

			if( tpptr->next != NULL )
				tpptr->next->prev = tpptr;      /* if there - point back at new */

			gptr->tplist = tpptr;           /* point at new head */
			fd = tpptr->fd;                 /* save for return value */
		}
	}                                  /* end  if we established a fd */
	
	return( fd );                    /* send back session number to user */
}                                 /* ng_siconnect */


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siconnect:Establish a TCP/IP connection)

&space
&synop	int ng_siconnect( char *address )

&space
&desc	This funciton attempts to create a connection to the address supplied in the &ital(address)
	buffer.  The address is expected in the form host:port.  Host can be either a textaual host
	name that is known to DNS or other name to IP address translation mechanism, or can be the 
	IPV4 dotted decimal string.  The port can beither the service name that can be looked up 
	via &lit(getsrvicebyname()), or can be the real port number. 
	The session id, necessary for all calls to socket interface functions, is returned to the 
	caller.  -1 is returned on error.


&space
&parms	The buffer containing the address is the only parameter expected.

&space
&return The session id is returned on success, or -1 if there was an error.

&space
&see
	&seelink(socket:ng_siclear)
	&seelink(socket:ng_siclose)
	&seelink(socket:ng_sisend)
	&seelink(socket:ng_sisendt)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Mar 1995 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

