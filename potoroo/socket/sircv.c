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
*****************************************************************************
*
*  Mnemonic: ng_sircv
*  Abstract: This routine allows the user program to receive data on a
*            session without using the callback structure of the library.
*            It is the caller's responsibility to provide a buffer large
*            enough to handle the received data.
*  Parms:    gptr - The SIHANDLE that the user received on init call
*            sid  - The session id that the user wants to check
*            buf  - Pointer to buffer to receive data in
*            abuf - Pointer to buffer to return address of UDP sender in (!null)
*            buflen-Length of the receive buffer
*            delay- Value to pass to poll (time out) -1 == block until data
*  Returns:  SI_ERROR - (ng_sierrno will contain reason) if failure, else the
*            number of bytes read. If the number read is 0 ng_sierrno will indicate
*            why: time out exceeded, signal received.
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*  Mods:     26 Mar 20001 - Changed to support UDP reads
*
******************************************************************************
*/
#include "sisetup.h"    /* get start up stuff */

int ng_sircv( int sid, char *buf, int buflen, char *abuf, int delay )
{
 extern struct ginfo_blk *gptr;
 extern int sigflags;           /* signal flags */
 extern int ng_sierrno;

 int status = SI_ERROR;         /* assume the worst to return to caller */
 struct tp_blk *tpptr;          /* pointer to transport provider info */
 fd_set readfds;                /* special set of read fds for this call */
 fd_set execpfds;               /* special set of read fds for this call */
 struct timeval *tptr = NULL;   /* time info for select call */
 struct timeval time;
 struct sockaddr *uaddr;       /* pointer to udp address */
 int addrlen;
 char *addr_str = NULL;		/* address string */

 ng_sierrno = SI_ERR_HANDLE;              /* set errno before we fail */
 if( gptr->magicnum != MAGICNUM )     /* if not a valid ginfo block */
  return( ERROR );

 ng_sierrno = SI_ERR_SESSID;             /* set errno before we fail */
 for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != sid;
      tpptr = tpptr->next );      /* find transport block */
 if( tpptr == NULL )
  return( ERROR );                      /* signal bad block */

 uaddr = (struct sockaddr *) ng_malloc( sizeof( struct sockaddr ), "sircv:uaddr" );
 addrlen = sizeof( *uaddr );

 ng_sierrno = SI_ERR_SHUTD;               /* set errno before we fail */
 if( ! (gptr->flags & GIF_SHUTDOWN)  &&  ! (sigflags) )
  {                        /* if not in shutdown and no signal flags  */
   FD_ZERO( &readfds );               /* clear select info */
   FD_SET( tpptr->fd, &readfds );     /* set to check read status */

   FD_ZERO( &execpfds );               /* clear select info */
   FD_SET( tpptr->fd, &execpfds );     /* set to check read status */

   if( delay >= 0 )                /* user asked for a fininte time limit */
    {
     tptr = &time;                 /* point at the local struct */
     tptr->tv_sec = 0;             /* setup time for select call */
     tptr->tv_usec = delay;
    }

   ng_sierrno = SI_ERR_TP;
   if( (select( tpptr->fd + 1, &readfds, NULL, &execpfds, tptr ) < 0 ) ||
       (sigflags & SI_SF_QUIT) )
    gptr->flags |= GIF_SHUTDOWN;     /* we must shut on error or signal */
   else
    {                                /* poll was successful - see if data ? */
     ng_sierrno = SI_ERR_TIMEOUT;
     if( FD_ISSET( tpptr->fd, &execpfds ) )   /* session error? */
      {
       ng_siterm( tpptr );                 /* clean up our end of things */
       ng_sierrno = SI_ERR_SESSID;               /* set errno before we fail */
      }
     else
      {
       if( (FD_ISSET( tpptr->fd, &readfds )) &&  ! (sigflags ) )
        {                                       /* process data if no signal */
         ng_sierrno = SI_ERR_TP;
         if( tpptr->type == SOCK_DGRAM )        /* raw data received */
          {
	   status = recvfrom( sid, buf, buflen, 0, uaddr, &addrlen );
           if( abuf )
	   {
            /* ng_siaddress( uaddr, abuf, AC_TODOT );*/
		if( (addr_str = ng_addr2dot( (struct sockaddr_in *) uaddr )) )
		{
			strcpy( abuf, addr_str );
			ng_free( addr_str );
		}

	   }
           if( status < 0 )                        /* session terminated? */
            ng_siterm( tpptr );                 /* so close our end */
          }
         else                                      /* cooked data received */
          {
           status = recv( sid, buf, buflen, 0 );   /* read data into user buf */
           if( status < 0 )                        /* session terminated? */
            ng_siterm( tpptr );                 /* so close our end */
          }
        }                                         /* end event was received */
       else                                       /* no event was received  */
        status = 0;                               /* status is just ok */
      }                       /* end else - not in shutdown mode after poll */
    }                     /* end else pole was successful */
  }                                 /* end if not already signal shutdown */

 if( gptr->flags & GIF_SHUTDOWN  &&  gptr->tplist != NULL )
  {             /* shutdown received but sessions not cleaned up */
   ng_sishutdown( gptr );
   status = SI_ERROR;                /* indicate failure on return */
  }                                  /* end if shut but not clean */
 else                                /* if not in shutdown see if */
  {
   if( sigflags )                   /* a signal was received? */
    {                              /* return = 0, ng_sierrno indicates why */
     status = 0;                   /* no data received */
     if( sigflags & SI_SF_USR1 )   /* set return code based on the flag */
      {
       ng_sierrno = SI_ERR_SIGUSR1;
       sigflags &= ~SI_SF_USR1;    /* turn off flag */
      }
     else
      if( sigflags & SI_SF_USR2 )   /* siguser 2 received */
       {
        ng_sierrno = SI_ERR_SIGUSR2;
        sigflags &= ~SI_SF_USR2;
       }
      else
       if( sigflags & SI_SF_ALRM )    /* alarm signal received */
        {
         ng_sierrno = SI_ERR_SIGALRM;
         sigflags &= ~SI_SF_ALRM;     /* put down the flag */
        }
    }                        /* end if sigflag was on */
  }                         /* end else not in shutdown mode */

 ng_free( uaddr );
 return( status );          /* send back the status */
}                           /* ng_sircv */
