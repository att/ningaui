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
****************************************************************************
*
*  Mnemonic: ng_sisendu
*  Abstract: This routine will send a UDP datagram to the IP address
*            that is passed in with the buffer. The datagram is sent
*            on the first raw FD in the tp list. If sending the datagram
*            would cause the process to block, the datagram is queued
*            and will be sent as a part of the ng_siwait loop when the FD
*            unblocks. If the datagram is queued, a copy of the caller's
*            buffers is made such that the caller is free to reuse the
*            buffers upon return from this routine.
*  Parms:    
*            abuf - Pointer to the dotted decimal address to send to
*            ubuf - User buffer to send.
*            ulen - Length of the user buffer.
*  Returns:  SI_OK if sent, SI_QUEUED if queued for later, SI_ERROR if error.
*  Date:     27 March 1995
*  Author:   E. Scott Daniels
*
*  Mods:	8 Oct 2001 : converted to use ng_ address routines
#		29 Aug 2006 (sd) : Reformatted to make more readable
*****************************************************************************
*/
#include "sisetup.h"     /* get setup stuff */

int ng_sisendu( char *abuf, char *ubuf, int ulen )
{
	extern struct ginfo_blk *gptr;
	extern int ng_sierrno;

	int fd;                     /* file descriptor to send to */
	int status = SI_OK;         /* status of processing */
	struct sockaddr *addr;      /* address to send to */
	struct tp_blk *tpptr;       /* pointer at the tp_blk for the session */
	struct ioq_blk *qptr;       /* pointer at i/o queue block */
	fd_set writefds;            /* fdset for select call */
	struct timeval time;        /* time to fill in for select call */

	ng_sierrno = SI_ERR_NOMEM;     /* set status incase of failure */
	if( (addr = (struct sockaddr *) ng_dot2addr( abuf )) == NULL )
		return ERROR;

	ng_sierrno = SI_ERR_HANDLE;        /* next possible failure is bad cookie */

	if( gptr->magicnum == MAGICNUM && addr != NULL )  /* ensure cookie is good */
	{
		ng_sierrno = SI_ERR_SESSID;

		for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM;
			tpptr = tpptr->next );    /* find the first udp (raw) block */
		if( tpptr != NULL )            /* found block for udp */
		{
			fd = tpptr->fd;           /* make easy access to fd */

			FD_ZERO( &writefds );     /* clear */
			FD_SET( fd, &writefds );  /* set for select to look at our fd */
			time.tv_sec = 0;          /* set time to 0 to force poll - no wait */
			time.tv_usec = 0;
		
			if( select( fd + 1, NULL, &writefds, NULL, &time ) > 0 ) /* write ok ? */
			{
				ng_sierrno = SI_ERR_TP;
				status = sendto( fd, ubuf, ulen, 0, addr, sizeof( struct sockaddr ) );
				ng_free( addr );
				addr = NULL;
				if( status == ulen )
					status = SI_OK;
			}
			else               /* write would block - build queue block and queue */
			{
				ng_sierrno = SI_ERR_NOMEM;
			
				if( (qptr = ng_sinew( IOQ_BLK )) != NULL )   /* alloc a queue block */
				{
					if( tpptr->squeue == NULL )    /* if nothing on the queue */
						tpptr->squeue = qptr;         /* simple add to the tp blk q */
					else                           /* else - add at end of the q */
					{
						for( qptr->next = tpptr->squeue; qptr->next->next != NULL;
						qptr->next = qptr->next->next );  /* find last block */
						qptr->next->next = qptr;   /* point the current last blk at new */
						qptr->next = NULL;         /* new block is the last one now */
					}                           /* end add block at end of queue */
	
					qptr->dlen = ulen;                    /* copy user's information  */
					qptr->data = (char *) ng_malloc( ulen, "sisendu:qptr->data" );    /* get buffer */
					memcpy( qptr->data, (const char *) ubuf, ulen );
					qptr->addr = addr;                             /* save address too */
					status = SI_QUEUED;                /* indicate queued to caller */
				}
				else
					status = SI_ERROR;
			}                       /* end else need to queue block */
		}                             	/* end if tpptr was not found */
		else
			status = SI_ERROR;
	}                         		/* end if a valid ginfo pointer passed in */
	else
		status = SI_ERROR;

	return( status );
}                               /* ng_sisendu */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sisendu:Send data via UDP)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_sisendu( char *abuf, char *ubuf, int ulen )

&space
&desc	
	&lit(Ng_sendu) causes the data in &lit(ubuf) to be sent via UDP to the address 
	contained in &lit(abuf.)  The length of the user data is passed in &lit(ulen.)

&space
&return The value SI_ERROR is returned if an error is detected, othewise SI_OK is returned.

&space
&see
	&seelink(socket:ng_siclear)
	&seelink(socket:ng_siclose)
	&seelink(socket:ng_siwait)
	&seelink(socket:ng_sipoll)
	&seelink(socket:ng_sisendt)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Mar 1995 (sd) : Original code.
&term	08 Oct 2001 (sd) : converted to use ng_ address routines in ng_netif (lib).
&term	29 Aug 2006 (sd) : Reformatted to make more readable.
&endterms


&scd_end
------------------------------------------------------------------ */
