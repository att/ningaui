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
*  Mnemonic: SIlisten
*  Abstract: Open a TCP port to which others may connect.
#		Allows the user to open multiple secondary listening ports
*  Parms:   port - Port number to open 
*  Returns: The file descriptor of the port, SI_ERROR if there is a problem
*  Date:    26 March 1995
*  Author:  E. Scott Daniels
*
*  Modified: 10 May 1995 - To change SOCK_RAW to SOCK_DGRAM
******************************************************************************
*/
#include "sisetup.h"

int ng_silisten( int port )
{
 extern struct ginfo_blk *gptr;
 extern int ng_sierrno;

 struct tp_blk *tpptr;      /* pointer into tp list */
 int status = SI_ERROR;     /* status of processing */

 ng_sierrno = SI_ERR_HANDLE;
 if( gptr->magicnum == MAGICNUM )   /* good cookie at the gptr address? */
  {
   ng_sierrno = SI_ERR_TP;

     tpptr = ng_siestablish(  TCP_DEVICE, port ); /* bind to tp */
  
     if( tpptr != NULL )                          /* got right port */
      {                                           /* enable connection reqs */
       if( (status = listen( tpptr->fd, 1 )) >= OK )
        {
         tpptr->next = gptr->tplist;  /* add to the list */
         if( tpptr->next != NULL )
          tpptr->next->prev = tpptr;
         gptr->tplist = tpptr;
         tpptr->flags |= TPF_LISTENFD;          /* this is where we listen */
		status = tpptr->fd;
        }
      }                                        /* end if tcp port ok */
     else
      status = ERROR;                          /* didnt get requested port */
    }                                          /* end if tcp port > 0 */

 return( status );    /* send the status back to the caller */
}                     /* SIclose */
