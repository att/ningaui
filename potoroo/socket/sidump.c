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
*  Mnemonic: ng_sidump
*  Abstract: This routine will dump stuff via bleat -- mostly for application 
*		debugging.
*  Parms:    none.
*  Retrns:   Nothing.
*  Date:     23 March 1995
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
#include "sisetup.h"                   /* get includes and defines */

void ng_sidump( )
{
	extern struct ginfo_blk *gptr;
 	struct tp_blk *tpptr;      /* pointer into tp list */


	if( gptr != NULL && gptr->magicnum == MAGICNUM )
	{
		ng_bleat( 1, "sidump: geninfo: %04x(flags), %d(rbuflen)", gptr->flags, gptr->rbuflen );
		for( tpptr = gptr->tplist; tpptr; tpptr = tpptr->next )
			ng_bleat( 1, "sidump: tpblk: %d(fd) %04x(flags), %d(type)", tpptr->fd, tpptr->flags, tpptr->type );
	}

}            /* ng_dump */
