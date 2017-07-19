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

/* X
******************************************************************************
*
*  Mnemonic: ng_siopen
*  Abstract: This routine allows the user application to open a UDP port.
*            (it is similar to the ng_siconnect call for TCP). It will establish
*            a new file descriptor and bind it to UDP
*
*		WARNING:
*		The new block is added to the head of the list, thus udp writes will 
*		be written from the last port open.  This can be significant to 
*		some user code and must not be changed. 
*
*  Parms:   gptr - The pointer to the ginfo block (SIHANDLE to the user)
*           port - Port number to open - 0 any assigned port is ok
*  Returns: The file descriptor of the UDP port, SI_ERROR if there is a problem
*           a problem.
*  Date:    26 March 1995
*  Author:  E. Scott Daniels
*
*  Modified: 10 May 1995 - To change SOCK_RAW to SOCK_DGRAM
*		1 Sep 2003 (sd)  - Now allows multiple udp ports to be opened
******************************************************************************
*/
#include "sisetup.h"

int ng_siopen( int port )
{
 extern struct ginfo_blk *gptr;
 extern int ng_sierrno;

 struct tp_blk *tpptr;      /* pointer into tp list */
 int status = SI_ERROR;     /* status of processing */

 ng_sierrno = SI_ERR_HANDLE;
 if( gptr->magicnum == MAGICNUM )   /* good cookie at the gptr address? */
  {
   ng_sierrno = SI_ERR_TP;

     ng_sierrno = SI_ERR_TP;
     tpptr = ng_siestablish( UDP_DEVICE, port );   /* get a port for UDP */
     if( tpptr != NULL )                  /* we were successful */
      {
         tpptr->next = gptr->tplist;  /* add to the list -- code depends on new UDP blocks added to head*/
         if( tpptr->next != NULL )
          tpptr->next->prev = tpptr;
         gptr->tplist = tpptr;

         tpptr->flags |= TPF_UNBIND;    /* ensure port is unbound on error */
         status = tpptr->fd;            /* caller gets fd as good status */
      }                               /* end if we actually opened one */
  }                                   /* end if the handle was good */

 return( status );    /* send the status back to the caller */
}                     /* ng_siclose */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siopen:Open a UDP port)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_open( int port );

&space
&desc	Opens a UDP port on any interface. Typically an application needs only one UDP
	port which can be established with the &lit(ng_siinit) call, but in the cases
	where an additional UDP port, or delayed opening, is needed, this function can 
	be used.

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
&term	26 Mar 1995 (sd) : Original implementation.
&term	01 Sep 2003 (sd) : Mod to allow multiple UDP ports to be open.
&term	
&endterms


&scd_end
------------------------------------------------------------------ */

