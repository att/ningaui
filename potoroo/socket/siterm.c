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
**************************************************************************
*  Mnemonic: ng_siterm
*  Abstract: This routine will terminate a session based on the tp_blk
*            that is passed into the routine. The transport session block
*            is released and removed from the ginfo list. The session is
*            terminated by issuing a t_unbind call (if the unbind flag is
*            on in the tpptr block), and then issuing a t_close.
*
*  NOTE: While not very likely, this function should never be called by 
*		the user programme. 
*
*  Parms:    gptr - Pointer to the global information block
*            tpptr - Pointer to tp block that defines the open fd.
*  Returns:  Nothing.
*  Date:     18 January 1995
*  Author:   E. Scott Daniels
*
*  Mod:		01 Mar 2007 (sd) - bumpped up verbose level on bleet 
*
**************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

void ng_siterm( struct tp_blk *tpptr )
{
	extern struct ginfo_blk *gptr;
	struct ioq_blk *iptr;
	struct ioq_blk *inext;

	/* if( tpptr != NULL && tpptr->fd >= OK ) */
	if( tpptr != NULL )
	{
		if( tpptr->fd >= 0 )		/* if still open */
			close( tpptr->fd );    

		if( tpptr->prev != NULL )            		/* remove from the list */
			tpptr->prev->next = tpptr->next;    	/* point previous at the next */
		else
			gptr->tplist = tpptr->next;        	/* this was head, make next new head */
	
		if( tpptr->next != NULL )   
			tpptr->next->prev = tpptr->prev;	/* point next one back behind this one */

		tpptr->next = tpptr->prev = NULL;
		tpptr->fd = -1;

		ng_sitrash( TP_BLK, tpptr );			/* ditch the block now that it is dequeued */
		tpptr = NULL;
	}
}                         /* ng_siterm */
