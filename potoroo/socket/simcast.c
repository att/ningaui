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
****************************************************************************
*
*  Mnemonic:	ng_mcast
*  Abstract:	multi cast oriented routines
*			ng_simcast_join() -- join a multicast group 
*			ng_simcast_leave() -- depart from the group 
*			ng_simcast_ttl() - set time to live for multi cast packets
*  Parms:    
*  Returns:  
*  Date:	23 Aug 2004
*  Author:	E. Scott Daniels
*
*  Mods:	
*****************************************************************************
*/
#include	<sys/types.h>          /* various system files - types */
#include "sisetup.h"     /* get setup stuff */

/* bloody suns */
#ifdef OS_SOLARIS
typedef unsigned int u_int32_t;
#endif


/* rather than re-inventing the wheel, we rely on the ng_mcast functions implemented in the library */
int ng_simcast_join(  char *group, int iface, int ttl )
{
	extern struct ginfo_blk *gptr;
	struct	tp_blk *tpptr;       /* pointer at the tp_blk for the session */
	unsigned char opt = 0;
	u_int32_t uiface;

	if( iface < 0 )
		uiface = INADDR_ANY;
	else
		uiface = iface;

	
	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM; tpptr = tpptr->next );    /* find the first udp (raw) block */
	if( tpptr != NULL )            /* found block for udp */
	{
		if( ng_mcast_join( tpptr->fd, group, uiface, (unsigned char) ttl ) )
		{
			if( ttl >= 0 );
			{
				opt = ttl;
				setsockopt( tpptr->fd, IPPROTO_IP, IP_MULTICAST_TTL, &opt, sizeof( opt ) );
			}
			return SI_OK;
		}
	}

	return SI_ERROR;
}

int ng_simcast_leave( char *group, int iface )
{
	extern struct ginfo_blk *gptr;
	struct	tp_blk *tpptr;       /* pointer at the tp_blk for the session */

	if( iface < 0 )
		iface = INADDR_ANY;

	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM; tpptr = tpptr->next );    /* find the first udp (raw) block */
	if( tpptr != NULL )            /* found block for udp */
	{
		if( ng_mcast_leave( tpptr->fd, group, iface ) )
			return SI_OK;
	}

	return SI_ERROR;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_simcast:Join/Leave a multicast group)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_simcast_join( char *group, int iface, int ttl )
&break 	int ng_simcast_leave( char *group, int iface )

&space
&desc	These functions allow an application to join or leave a multicast group. 
	While a member of the group, broadcast messages are delivered to the user 
	programme through the UDP callback function. The user programme can use the 
	send UDP funciton of the library to send messages to the group.

&space
	&ital(Ng_simcast_join()) accpets the group (the multicast address) to be joined 
	using the interface &ital(iface.) The interface is likely just the constant &lit(INADDR_ANY,)
	but can be any value that is passed as an &lit(s_addr) member of various inet structs.

&space
	The &lit(ttl) setting for multicast has a double meaning. In addition to the usual 
	'max hops' meaning, its value is also used to limit the 'scope' of where the messages are 
	transmitted. The following table is the generally accepted mapping of ttl value to scope:

&begterms
&term 	TTL :      Scope
&term   0   :  Restricted to the same host. Won't be output by any interface.
&term 1    : Restricted to the same subnet. Won't be forwarded by a router.
&term <32    :      Restricted to the same site, organisation or department.
&term <64 : Restricted to the same region.
&term <128 : Restricted to the same continent.
&term <255 : Unrestricted in scope. Global.
&terms
&space
	The exact meaning of department, site, organisation, etc are not specific and may or may not 
	be understood/supported by any or all routers that the mcast packets are delivered to.

&space
&return The session id is returned on success, or -1 if there was an error.

&space
&see
	&seelink(socket:ng_siclear)
	&seelink(socket:ng_siclose)
	&seelink(socket:ng_siwait)
	&seelink(socket:ng_sipoll)
	&seelink(socket:ng_sisendu)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	29 Aug 2004 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

