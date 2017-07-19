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
*****************************************************************************
*  Mnemonic: ng_sibldpoll
*  Abstract: This routine will fill in the read and write fdsets in the
*            general info struct based on the current transport provider
*            list. Those tb blocks that have something queued to send will
*            be added to the write fdset. The fdcount variable will be set to
*            the highest sid + 1 and it can be passed to the select system
*            call when it is made.
*  Parms:    gptr  - Pointer to the general info structure
*  Returns:  Nothing
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*  Mod:		29 Aug 2003 (sd) - We now delete blocks here as its the only 
*			safe time to do so.
*		26 Oct 2004 (sd) - Corrected the fd count setting.
*		02 Jul 2008 (sd) - Allow read on a draining session to prevent 
*			possible deadlocks.
*		17 Jul 2008 (sd) - Corrected bug introduced by accidental check-in.
*
***************************************************************************
*/
#include "sisetup.h"      /* get definitions etc */

void ng_sibldpoll( )
{
	extern struct ginfo_blk *gptr;

	struct tp_blk *tpptr;             /* pointer into tp list */
	struct tp_blk *nextblk;             /* @ next incase we delete a block */

	gptr->fdcount = -1;               /* reset largest sid found */

	FD_ZERO( &gptr->readfds );        /* reset the read and write sets */
	FD_ZERO( &gptr->writefds );
	FD_ZERO( &gptr->execpfds );

	for( tpptr = gptr->tplist; tpptr != NULL; tpptr = nextblk )
	{
		nextblk = tpptr->next;

		if( tpptr->flags & TPF_DELETE )		/* provider has been marked as deletable, safe to do it here */
		{
			ng_siterm( tpptr );			/* clean up the storage */
		}
		else
		if( tpptr->fd >= OK )                       /* if valid file descriptor */
		{
			if( tpptr->fd >= gptr->fdcount )
				gptr->fdcount = tpptr->fd + 1;     /* save largest fd (+1) for select */

			FD_SET( tpptr->fd, &gptr->execpfds );     /* set all fds for execpts */

#ifdef DEADLOCKS
if we dont read on a draining session the session can deadlock, so we must allow it to read and we 
will drop the buffers on the floor in poll or wait.
			if( !(tpptr->flags & TPF_DRAIN) )                  /* if not draining */
#endif
			FD_SET( tpptr->fd, &gptr->readfds );       /* set test for data flag */

			if( tpptr->squeue != NULL )                  /* stuff pending to send ? */
				FD_SET( tpptr->fd, &gptr->writefds );   /* set flag to see if writable */
		}
	}

	if( gptr->kbfile >= 0 )     /* if we have input open */
		FD_SET( 0, &gptr->readfds );    /* set to check stdin for read stuff */
}                                 /* ng_sibldpoll */
