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
****************************************************************************
*
*  Mnemonic: ng_sisendt ng_sisendtb
*  Abstract: This routine will send a datagram to the TCP session partner
*            that is connected via the FD number that is passed in.
*            If the send would cause the process to block, the send is
*            queued on the tp_blk for the session and is sent later as
*            a function of the ng_siwait process.  If the buffer must be
*            queued, a copy of the buffer is created such that the
*            user program may free, or reuse, the buffer upon return.
#
#		ng_sisendtb sends on a tcp session allowing the send to block.
*  Parms:    
*            fd   - File descriptor (session number)
*            ubuf - User buffer to send.
*            ulen - Lenght of the user buffer.
*
*  BIG NOTE: user programmes SHOULD (maybe must) ignore, or some how process
*		SIGPIPE signals.  If the connection is lost and they try to 
*		send, the only indication that is given is the PIPE signal.
*		if they dont ignore it, the process dies. 
*
*
*  Returns:  SI_OK if sent, SI_QUEUED if queued for later, SI_ERROR if error.
*  Date:     27 March 1995
*  Author:   E. Scott Daniels
*  Mod:		02 Jul 2004 (sd) : Correctly handles a partial send,
*			and inidcates error if session disconnected
*		31 Mar 2008 (sd) : marks the tp block as 'closed' but does not 
*			delete it as it may not be safe here.
*		11 Aug 2008 (sd) : Checks for mustqueue flag and forces data to 
*			queue on our internal queue if the user has caused this flag
*			to be set.  Added siforcequeue() to allow user to set/reset
*			the flag. 
*
*****************************************************************************
*/
#include "sisetup.h"     /* get setup stuff */

void ng_siforcequeue( int fd, int val )
{
	extern struct ginfo_blk *gptr;
	struct tp_blk *tpptr;

	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != fd;
		tpptr = tpptr->next );                            /* find the block */

	if( tpptr )
	{
		if( val )
			tpptr->flags |= TPF_MUSTQUEUE;
		else
			tpptr->flags &= ~TPF_MUSTQUEUE;
	}
}

static void add_ioq( struct tp_blk *tpptr, char *ubuf, int ulen )
{
	extern int ng_sierrno;
	struct ioq_blk *qptr = NULL;

	if( ulen <= 0 )
		return;

	if( (qptr = ng_sinew( IOQ_BLK )) != NULL )   /* alloc a queue block */
	{
		if( tpptr->stail == NULL )    		/* if nothing on the queue */
		{
			tpptr->squeue = tpptr->stail = qptr;		/* simple add to the tp blk q */
		}
		else
		{
			tpptr->stail->next = qptr;
			tpptr->stail = qptr;
		}

		qptr->next = NULL;         			/* ensure it is null */
		qptr->dlen = ulen;           	
		qptr->sdata = qptr->data = (char *) ng_malloc( ulen, "sisendt -- queue" );	/* first send is from head of buffer so both same */
		memcpy( qptr->data, (const int *) ubuf, ulen );

		ng_sierrno = SI_ERR_QUEUED;                /* indicate queued to caller -- if they care to check  */
	}
}

/* send and allow to block */
int ng_sisendtb( int fd, char *ubuf, int ulen )
{
	extern int ng_sierrno; 
	int sent;
	int	len;

	len = ulen;

	/* if we change users "handle" away from being the fd, then this routine must change to run tp blocks */
	ng_sierrno = SI_ERR_TP;

	while( len && (sent = send( fd, ubuf, (unsigned int) len, 0 )) < len )		
	{
		if( sent < 0 )
			return SI_ERROR;

		len -= sent;
		ubuf += sent;
		sleep( 1 );
	}

	return ulen;
}

/* return true if session is not blocking */
int ng_siblocked( int fd )
{
	extern struct ginfo_blk *gptr;
	extern int ng_sierrno;

	struct tp_blk *tpptr;       /* pointer at the tp_blk for the session */

	if( gptr->magicnum == MAGICNUM )     /* ensure cookie is good */
	{                                   /* mmmm oatmeal, my favorite */
		for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != fd;
			tpptr = tpptr->next );                            /* find the block */

		if( tpptr )
			return tpptr->squeue != NULL;		/* true (blocking) if we have a queue */
	}

	return 0;
}


int ng_sisendt( int fd, char *ubuf, int ulen )
{
	extern struct ginfo_blk *gptr;
	extern int ng_sierrno;

	int sent;			/* bytes accepted by send */
	int sstatus = 0;		/* select status */
	fd_set writefds;            /* local write fdset to check blockage */
	fd_set execpfds;            /* exception fdset to check errors */
	struct tp_blk *tpptr;       /* pointer at the tp_blk for the session */
	struct ioq_blk *qptr;       /* pointer at i/o queue block */
	struct timeval time;        /* delay time parameter for select call */

	ng_sierrno = SI_ERR_HANDLE;

	if( gptr->magicnum == MAGICNUM )     /* ensure cookie is good */
	{                                   /* mmmm oatmeal, my favorite */
		ng_sierrno = SI_ERR_SESSID;

		for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != fd;
			tpptr = tpptr->next );                            /* find the block */

		if( tpptr != NULL )            /* found block for fd */
		{
			FD_ZERO( &writefds );       /* clear for select call */
			FD_SET( fd, &writefds );    /* set to see if this one was writable */
			FD_ZERO( &execpfds );       /* clear and set execptions fdset */
			/*FD_SET( fd, &execpfds ); */

			time.tv_sec = 0;            /* set both to 0 as we only want a poll */
			time.tv_usec = 0;

			if( !(tpptr->flags & TPF_MUSTQUEUE) && (sstatus = select( fd + 1, NULL, &writefds, NULL, &time )) > 0 )		/* poll to see if a write would block */
			{                                               		/* no block - send something */
				ng_sierrno = SI_ERR_TP;
				if( FD_ISSET( fd, &execpfds ) )   			/* error? */
				{
					tpptr->flags |= TPF_DELETE;	/* force cleanup when safe; may not be safe to delete here */
					/*ng_siterm( tpptr );	*/	/* clean up our portion of the session */
					return  SI_ERROR;		/* and signal an error to caller */
				}
				else                       /* no err, no blockage - send something */
				{
					if( tpptr->squeue == NULL )				/* nothing previously queued - just send */
					{
						if( (sent = send( tpptr->fd, ubuf, (unsigned int) ulen, 0 )) < ulen )  /* partial send? */
						{
							if( sent < 0 )
					  		{
								tpptr->flags |= TPF_DELETE;	/* force cleanup when safe; may not be safe to delete here */
								/* ng_siterm( tpptr );*/		/* clean up our portion of the session */
								return  SI_ERROR; 		/* and signal an error to caller */
							}
							add_ioq( tpptr, ubuf+sent, ulen-sent );
							ng_sierrno = SI_ERR_QUEUED;                /* indicate queued to caller -- if they care to check  */
						}

						return SI_OK;
					}
					else
					{
						add_ioq( tpptr, ubuf, ulen );		/* add to back of the queue */
						return ng_sisend( tpptr );		/* then send as much as we can */
					}
				}
			}
		
			if( sstatus < 0 )		/* error - assume session dropped */
			{
				ng_bleat( 2, "sisendt: select returned error when polling fd %d", tpptr->fd );
				ng_sierrno = SI_ERR_TP;
				return SI_ERROR;
			}
					/* write would block or we sent from the queue - queue this one for later */

			ng_sierrno = SI_ERR_QUEUED;
			add_ioq( tpptr, ubuf, ulen );			/* the whole thing is queued */
		}							/* end if tpptr was not found */
		else
			return SI_ERROR;
	}								/* end if a valid ginfo pointer passed in */

	return SI_OK;
}                           /* ng_sisendt */


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sisendt:Send data on a TCP session)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_sendt( int sid, char *buf, int blen )
&break  int ng_sendtb( int sid, char *buf, int blen)
&break	int ng_siblocked( int fd )

&space
&desc
	These functions allow the user to send data on a TCP session. The two send 
	functions will both send  the requested data on the session.  The difference
	between them is that the &lit(ng_sendtb) function will allow the process to 
	block if the request cannot be completed immediately.  The &lit(ng_sisent) 
	function will queue the data and immediately return to the caller should the 
	unerlying communication system indicate that the send would block. 

&space
	The &lit(ng_siblocked) function will return true if buffers have been 
	queued for the session and are still waiting to be sent.  

&space
	When a session with queued bufferes is closed (ng_siclose), the session is 
	put into a drain state and the socket interface environment will hold the session
	open until it is able to send all of the queued buffers.  While in drain 
	state the socket environment will not accept any sends, and any data received 
	from the remote end of the session will be discarded.  The user programme should 
	call &lit(ng_siclear) prior to invoking &lit(ng_siclose) if queued data should be 
	discarded. 

&space
&return The session id is returned on success, or -1 if there was an error.

&space
&see
	&seelink(socket:ng_siclear)
	&seelink(socket:ng_siclose)
	&seelink(socket:ng_siwait)
	&seelink(socket:ng_sipoll)
	&seelink(socket:ng_sisendu)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Mar 1995 : Original code.
&term	02 Jul 2004 (sd) : Correctly handles a partial send, and inidcates error if session disconnected.
&term	31 Mar 2008 (sd) : marks the tp block as 'closed' but does not delete it as it may not be safe to do so here.
&term	11 Aug 2008 (sd) : Checks for mustqueue flag and forces data to queue on our internal queue if the user has caused this flag
			to be set.  Added siforcequeue() to allow user to set/reset the flag. 
&endterms


&scd_end
------------------------------------------------------------------ */
