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
*******************************************************************************
*
*  Mnemonic: ng_sinew
*  Abstract: This routine is responsible for alocating a new block based on
*            the block type and initializing it.
*  Parms:    type - Block id to create
*  Returns:  Pointer to the new block or NULL if not successful
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*
*  Mods:	18 May 2007 (sd) : Ensured that all of ginfo was initialised.
*		11 Aug 2008 (sd) : we now memset structs to 0. 
******************************************************************************
*/
#include "sisetup.h"

void *ng_sinew( int type )
{
 void *retptr;                  /* generic pointer for return */
 struct tp_blk *tpptr;          /* pointer at a new tp block */
 struct ginfo_blk *gptr;        /* pointer at gen info blk */
 struct ioq_blk *qptr;          /* pointer to an I/O queue block */

 switch( type )
  {
   case IOQ_BLK:              /* make an I/O queue block */
    if( (qptr = (struct ioq_blk *) ng_malloc( sizeof( struct ioq_blk), "sinew: qptr" )) != NULL )
     {
	memset( qptr, 0, sizeof( struct ioq_blk ) );
      qptr->addr = NULL;
      qptr->next = NULL;
      qptr->data = NULL;
      qptr->sdata = NULL;
      qptr->dlen = 0;
     }
    retptr = (void *) qptr;    /* set pointer for return */
    break;

   case TP_BLK:
    if( (tpptr = (struct tp_blk *) ng_malloc( sizeof( struct tp_blk ), "sinew: tppblk" )) != NULL )
     {
	memset( tpptr, 0, sizeof( struct tp_blk ) );
      tpptr->next = NULL;
      tpptr->prev = NULL;
      tpptr->squeue = NULL;    /* nothing queued to send on session */
      tpptr->stail = NULL;    
      tpptr->addr = NULL;    
      tpptr->paddr = NULL;
      tpptr->fd = -1;
      tpptr->type = -1;
      tpptr->flags = TPF_UNBIND;   /* default to unbind on termination */
     }
    retptr = (void *) tpptr;   /* setup for later return */
    break;

   case GI_BLK:                /* create global info block */
    if( (gptr = (struct ginfo_blk *) ng_malloc( sizeof( struct ginfo_blk ) , "sinew: giblk" )) != NULL )
     {
	memset( gptr, 0, sizeof( struct ginfo_blk ) );

      gptr->magicnum = MAGICNUM;   /* inidicates valid block */
      gptr->childnum = 0;
      gptr->flags = 0;
      gptr->tplist = NULL;
      FD_ZERO( &gptr->readfds);      /* clear the fdsets */
      FD_ZERO( &gptr->writefds) ;
      FD_ZERO( &gptr->execpfds );
      gptr->kbfile = ERROR;          /* no keyboard file initially */
      gptr->rbuf = NULL;             /* no read buffer */
      gptr->cbtab = NULL;
      gptr->rbuflen = 0;
     }
    retptr = (void *) gptr;    /* set up for return at end */
    break;

   default:
    retptr = NULL;           /* bad type - just return null */
    break;
  }                          /* end switch */

 return( retptr );           /* send back the new pointer */
}                            /* ng_sinew */
