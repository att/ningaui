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
*  Mnemonic: ng_trash
*  Abstract: Delete all things referenced by a struct and then free the 
*		struct.
*
*  Returns:  Nothing
*  Date:     01 March 2007
*  Author:   E. Scott Daniels
*
******************************************************************************
*/

#include	"sisetup.h"

void ng_sitrash( int type, void *bp )
{
	struct tp_blk *tp = NULL;
	struct ioq_blk *iptr;
	struct ioq_blk *inext;

	switch( type )
	{
		case IOQ_BLK:
			iptr = (struct ioq_blk *) bp;
			ng_free( iptr->data );
			ng_free( iptr->addr );
			ng_free( iptr );
			break;

		case TP_BLK:						/* we assume its off the list */
			tp = (struct tp_blk *) bp;
			for( iptr = tp->squeue; iptr; iptr = inext )		/* trash any pending send buffers */
			{
				inext = iptr->next;
				ng_free( iptr->data );		/* we could recurse, but that seems silly */
				ng_free( iptr->addr );
				ng_free( iptr );
			}
	
			ng_free( tp->addr );             /* release the address bufers */
			ng_free( tp->paddr );
			ng_free( tp );                   /* and release the block */
			break;
	}
}

