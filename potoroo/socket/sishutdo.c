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
*  Mnemonic: ng_sishutdown
*  Abstract: This routine will ensure that all tp blocks have been closed
*            with the transport provider and removed from the list. The
*            shutdown flag is set in addition.
*  Parms:    gptr - pointer to the ginfo structure (SIHANDLE)
*  Retrns:   Nothing.
*  Date:     23 March 1995
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
#include "sisetup.h"                   /* get includes and defines */

void ng_sishutdown( )
{
 extern struct ginfo_blk *gptr;

 gptr->flags |=  GIF_SHUTDOWN;    /* signal shutdown */

 if( gptr != NULL && gptr->magicnum == MAGICNUM )
  while( gptr->tplist != NULL )
   {
    gptr->tplist->flags |= TPF_UNBIND;    /* force unbind on session */
    ng_siterm( gptr->tplist );         /* and drop the session */
   }                                      /* end while */
}            /* ng_sishutdown */
