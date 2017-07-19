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
*  Mnemonic: ng_sisend
*  Abstract: This routine is called to send a buffer of data to a partner
*            if the buffer has been queued waiting for the session to
*            unblock. The top qio block is removed from the tp block's
*            queue and is sent. It is assumed that for UDP data the
*            unit data structure was created and contains the buffer and
*            address and is pointed to by the qio block. The block and
*            associated buffers are then freed.
*  Parms:    gptr - POinter to the global information block
*            tpptr- Pointer to the tp block
*  Returns:  Nothing.
*  Date:     27 March 1995
*  Author:   E. Scott Daniels
*
*  Mod:		01 Jul 2004 (sd) : added partial send check (tcp only).
*		03 May 2005 (sd) : We send one at a time -- wait will continue 
*			to call us as long as squeue is not null.
*		07 Sep 2006 (sd) : Fixed double free; ->data was coded instead
*			of ->addr.  (HBDHAZ)
******************************************************************************
*/
#include "sisetup.h"      /* get include files etc */


int ng_sisend( struct tp_blk *tpptr )
{
	extern struct ginfo_blk *gptr;
	extern int ng_sierrno;
	int sent;			/* number of bytes actually transmitted */

	struct ioq_blk *qptr;          /* pointer at qio block for free */

	if( tpptr->squeue == NULL )    		/* who knows why we were called */
		return SI_OK;                   /* nothing queued - just leave */

#ifdef KEEP
/* just send first buffer else we risk dead lock if partner is flooding us at the same time */
	while( tpptr->squeue )			/* while something to send, and we sent a full buffer last time */
	{
#endif
		if( tpptr->type == SOCK_DGRAM )                                /* udp send? */
			sendto( tpptr->fd, tpptr->squeue->data, tpptr->squeue->dlen, 0, tpptr->squeue->addr, sizeof( struct sockaddr ) );   /* send it */
		else                                                            /* else tcp*/
		{
		 	if( (sent = send( tpptr->fd, tpptr->squeue->sdata, tpptr->squeue->dlen, 0 )) < tpptr->squeue->dlen )
			{
				if( sent < 0 )				/* error on session */
				{
					tpptr->flags |= TPF_DELETE;	/* mark for deletion -- may not be safe to delete right now */
					/*ng_siterm( tpptr );*/     /* trash the tp block */
					return SI_ERROR;
				}

				tpptr->squeue->sdata += sent;		/* point to first byte to send next time; leave block queued */
				tpptr->squeue->dlen -= sent;		/* bytes remaining in buffer to send */
				return SI_OK;				/* leave the block at head for next call */
			}
		}

								/* all of the buffer was sent; ditch the block */
		if( tpptr->squeue->addr != NULL )
			ng_free( tpptr->squeue->addr );    	/* lose address if udp */
		ng_free( tpptr->squeue->data );           	/* trash buffer or the udp block */
		qptr = tpptr->squeue;                  		/* hold pointer for free */
		tpptr->squeue = tpptr->squeue->next;   		/* next in queue becommes head */
		if( tpptr->squeue == NULL )
			tpptr->stail = NULL;			/* that was all she wrote */
		ng_free( qptr );
#ifdef KEEP
	}
#endif

	if( (tpptr->flags & TPF_DRAIN) && tpptr->squeue == NULL )  /* done w/ drain? */
	{
		tpptr->flags |= TPF_DELETE;	/* mark for deletion -- may not be safe to delete right now */
		/*ng_siterm( tpptr );*/     /* trash the tp block */
	}

	return SI_OK;
}                      /* ng_sisend */
