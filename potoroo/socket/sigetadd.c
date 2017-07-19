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
*  Mnemonic: ng_sigetaddr
*  Abstract: This routine will get the address of the first listening
*            block on the tp list and return it in ASCII format to the
*            caller.
*  Parms:    gptr - Pointer to the global information block
*            buf  - Pointer to the buffer to hold the ascii string
*  Returns:  SI_OK if block found, SI_ERROR if no listen block exists
*  Date:     18 April 1995
*  Author:   E. Scott Daniels
*
*  Mods:	7 Oct 2001 : Converted to use ng_ address conversion rtns
******************************************************************************
*/
#include "sisetup.h"        /* get the standard include stuff */

int ng_sigetaddr( char *buf )
{
 extern struct ginfo_blk *gptr;
 char *b;

 struct tp_blk *tpptr;       /* Pointer into tp list */
 int status = SI_ERROR;       /* return status */

 for( tpptr = gptr->tplist; tpptr != NULL && !(tpptr->flags & TPF_LISTENFD);
      tpptr = tpptr->next );

 if( tpptr != NULL )
  {
   /*ng_siaddress( tpptr->addr, buf, AC_TODOT );  */ /* convert to dot fmt */
   if( (b = ng_addr2dot( tpptr->addr )) )
   {
	strcpy( buf, b );
   	status = SI_OK;                               /* ok status for return */
   }
   else
	status = ERROR;
  }

 return status;
}                /* ng_sigetaddr */
