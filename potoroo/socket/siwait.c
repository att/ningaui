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
*  Mnemonic: ng_siwait
*  Abstract: This  routine will wait for an event to occur on the
*            connections in tplist. When an event is received on a fd
*            the status of the fd is checked and the event handled, driving
*            a callback routine if necessary. The system call poll is usd
*            to wait, and will be interrupted if a signal is caught,
*            therefore the routine will handle any work that is required
*            when a signal is received. The routine continues to loop
*            until the shutdown or cede flag is set, or until there are no open
*            file descriptors on which to wait.
*		The status returned from a callback routine directs wait to 
*		continue waiting, cede control back to its caller, or to 
*		full out close all sessions and quit. See the SI_RET_ constants
*		to determine what a callback routine should return.
*  Parms:    None.
*  Returns:  OK if the caller can continue, ERROR if all sessions have been
*            stopped, or the interface cannot proceed. When ERROR is
*            returned the caller should cleanup and exit immediatly (we
*            have probably received a sigter or sigquit.) If the return is
*	     0, then sierrno indicates whether things have all been shutdown
*	     or not.  if sierrno is also 0, then sessions still exist and 
* 	     if its not 0, then all sessions were stopped.
*  Date:     28 March 1995
*  Author:   E. Scott Daniels
*
*  Modified: 11 Apr 1995 - To pass execption to select when no keyboard
*            19 Apr 1995 - To call do key to do better keyboard editing
*            18 Aug 1995 - To init kstat to 0 to prevent key hold if
*                          network data pending prior to entry.
*	     16 Oct 2001 - To correct problem with tp block ref afer disc
*		11 Aug 2008 (sd) : To check for mustqueue flag and block sending
*			queued data if set (even if poll shows non-blocking session).
**************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

int ng_siwait( )
{
 extern int errno;
 extern int ng_sierrno;
 extern int deaths;       /* number of children that died and are zombies */
 extern int sigflags;     /* flags set by the signal handler routine */
 extern struct ginfo_blk *gptr;

 int fd;                       /* file descriptor for use in this routine */
 int ((*cbptr)());             /* pointer to callback routine to call */
 int status = OK;              /* return status */
 int addrlen;                  /* length of address from recvfrom call */
 int i;                        /* loop index */
 struct tp_blk *tpptr;         /* pointer at tp stuff */
 struct tp_blk *nextone;       /* pointer at next tp block to examine */
 int pstat;                    /* poll status */
 struct sockaddr *uaddr;       /* pointer to udp address */
 char *buf;
 char *ibuf;
 char *addr_str;		/* converted address string */

 buf = (char *) ng_malloc( 1024, "siwait: buf" );
 ibuf = (char *) ng_malloc( 2048, "siwait: ibuf" );

 ng_sierrno = SI_ERR_SHUTD;

 if( gptr->flags & GIF_SHUTDOWN )     /* cannot do if we should shutdown */
  return( ERROR );                    /* so just get out */

 ng_sierrno = SI_ERR_HANDLE;

 if( gptr->magicnum != MAGICNUM )     /* if not a valid ginfo block */
  return( ERROR );

 uaddr = (struct sockaddr *) ng_malloc( sizeof( struct sockaddr ), "siwait:uaddr" );

 do                                      /*  main wait/process loop */
  {
   ng_sibldpoll( gptr );                 /* build the fdlist for poll */
   errno = 0;
	if( gptr->fdcount < 0 )		/* no session to poll; assume tragic problem, or we are in shutdown mode */
		pstat = -1;
	else
   		pstat = select( gptr->fdcount, &gptr->readfds, &gptr->writefds, &gptr->execpfds, NULL ); 

   if( (pstat < 0 && errno != EINTR) || (sigflags & SI_SF_QUIT) )
    {                             /* poll fail or termination signal rcvd */
     gptr->fdcount = 0;           /* prevent trying to look at a session */
     gptr->flags |= GIF_SHUTDOWN; /* cause cleanup and exit at end */
     deaths = 0;                  /* dont need to issue waits on dead child */
     sigflags = 0;                /* who cares about signals now too */
    }

	/* it might be nice to wait only if we are forking for each new session */
    if( deaths )
    {
 	/*while( wait4( 0, NULL, WNOHANG, NULL ) > 0 ); */                    /* wait for all dead kids */
 	while( waitpid( 0, NULL, WNOHANG ) > 0 );                    /* non-blocking, portable wait for all dead kids */
    	deaths = 0;
    }

  if( sigflags &&  (cbptr = gptr->cbtab[SI_CB_SIGNAL].cbrtn) != NULL ) /* if signal received and processing them */
   {
     i = sigflags;                  /* hold for call */
     sigflags = 0;
     status = (*cbptr)( gptr->cbtab[SI_CB_SIGNAL].cbdata, i );
     ng_sicbstat( status, SI_CB_SIGNAL );    /* handle status */
   }

   if( pstat > 0  &&  (! (gptr->flags & GIF_SHUTDOWN)) )
    {
#ifndef OS_IRIX
     if( FD_ISSET( 0, &gptr->readfds ) )       /* check for keybd input */
      {
       fgets( ibuf, 2000, stdin );   /* get the stuff from keyboard */
       if( (cbptr = gptr->cbtab[SI_CB_KDATA].cbrtn) != NULL )
        {
         status = (*cbptr)( gptr->cbtab[SI_CB_KDATA].cbdata, ibuf );
         ng_sicbstat( status, SI_CB_KDATA );    /* handle status */
        }
      }
#endif

     for( tpptr = gptr->tplist; tpptr != NULL && !(gptr->flags & GIF_SHUTDOWN); tpptr = nextone )
      {
	nextone = tpptr->next;			/* incase we disc and trash the block later */
	if( tpptr->fd >= 0 )			/* still a live block */
	{
		if( !(tpptr->flags & TPF_MUSTQUEUE) && tpptr->squeue != NULL && (FD_ISSET( tpptr->fd, &gptr->writefds )) )
			ng_sisend( tpptr );              /* send if clear to send */

		if( FD_ISSET( tpptr->fd, &gptr->execpfds ) )
		{
			;  /* sunos seems to set the except flag for unknown reasons */
#ifdef NEVER
			if( (cbptr = gptr->cbtab[SI_CB_DISC].cbrtn) != NULL )
			{
				status = (*cbptr)( gptr->cbtab[SI_CB_DISC].cbdata, tpptr->fd );
				ng_sicbstat( status, SI_CB_DISC );    /* handle status */
			}
			ng_siterm( tpptr );
#endif
		}
		else						/* not an exception -- continue to process */
		{
			if( FD_ISSET( tpptr->fd, &gptr->readfds ) )  /* read event pending? */
			{
				fd = tpptr->fd;                     /* quick ref to the fd */

				if( tpptr->flags & TPF_LISTENFD )     /* listen port setup by init? */
				{                                    /* yes-assume new session req */
					errno=0;
					status = ng_sinewsession( tpptr );    /* make new session */
				}
	 			else                     /* data received on a regular port */
				{
					if( tpptr->type == SOCK_DGRAM )   /* udp socket? */
					{
						addrlen = sizeof( *uaddr );
						status = recvfrom( fd, gptr->rbuf, MAX_READ, 0, uaddr, &addrlen );
						if( status > 0 && ! (tpptr->flags & TPF_DRAIN) )
						{				                         /* if good status call cb routine */
							if( (cbptr = gptr->cbtab[SI_CB_RDATA].cbrtn) != NULL )
							{
								/*ng_siaddress( uaddr, buf, AC_TODOT );*/
								addr_str = ng_addr2dot( (struct sockaddr_in *) uaddr );
								gptr->rbuf[status] = 0;	/* quietly add end of string; safe even if usr expects binary data */
								status = (*cbptr)( gptr->cbtab[SI_CB_RDATA].cbdata, gptr->rbuf, status, addr_str, fd );
								ng_free( addr_str );
								ng_sicbstat( status, SI_CB_RDATA );    /* handle status */
							}
						} 
					}                                  
	  				else
	   				{                                /* else receive on tcp session */
	    					status = recv( fd, gptr->rbuf, MAX_READ, 0 );    /* read data */
	    					if( status > OK  &&  ! (tpptr->flags & TPF_DRAIN) )
	     					{
	      						if( (cbptr = gptr->cbtab[SI_CB_CDATA].cbrtn) != NULL )
	       						{
								gptr->rbuf[status] = 0;	/* we alloc'd bigger, add e of str; safe even if data is binary */
								status = (*cbptr)( gptr->cbtab[SI_CB_CDATA].cbdata, fd, gptr->rbuf, status );
								ng_sicbstat( status, SI_CB_CDATA );   /* handle cb status */
	       						}                            /* end if call back was defined */
	     					}                                     /* end if status was ok */
            					else
             					{     /* no bytes read - seems to indicate disc in sunos/linux */
              						if( (cbptr = gptr->cbtab[SI_CB_DISC].cbrtn) != NULL )
               						{
                						status = (*cbptr)( gptr->cbtab[SI_CB_DISC].cbdata, tpptr->fd );
                						ng_sicbstat( status, SI_CB_DISC );    /* handle status */
               						}

             		 				ng_siterm( tpptr );
             					}
	   				}                                                /* end tcp read */
				}					/* end data received on regular port */
			}                    				/* end read event pending */
      		}       						/* end not an exception */
	}								/* still have a live block */
      }								/* end for each tpptr */
    }                        /* end if not in shutdown */
  }                          /* end while */
 while( gptr->tplist != NULL && !(gptr->flags & (GIF_SHUTDOWN + GIF_CEDE)) );

 ng_free( buf );
 ng_free( ibuf );

 if( gptr->flags & GIF_CEDE )
 {
	ng_sierrno = 0;
	return 0;			/* all is ok, and things left initialised and running */
 }

 if( gptr->tplist == NULL )        /* indicate all fds closed */
  ng_sierrno = SI_ERR_NOFDS;

 if( gptr->flags & GIF_SHUTDOWN )      /* we need to stop for some reason */
  {
	if( ng_sierrno != SI_ERR_NOFDS )
   		ng_sierrno = SI_ERR_SHUTD;             /* indicate error exit status */
	status = ERROR;                /* status should indicate to user to die */
 	ng_sishutdown( gptr );            /* clean things up */
  }                          /* end if shutdown */
 else
  status = OK;               /* user can continue to process */

 ng_free( uaddr );              /* free allocated buffers */

 return( status );           /* send status back to caller */
}                            /* ng_siwait */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siwait:Socket Interface Callback Driver)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_siwait( );

&space
&desc	
	Once the socket interface has been established, and callback functions 
	registered, the &lit(ng_siwait) function can be invoked to wait for 
	data. The function will drive the appropriately defined callbacks for 
	session authorisation, session connection, new TCP data, new UDP data,
	signals and disconnection.  The function continues to operate until a 
	callback function returns a value of SI_QUIT.

&space
&return The session id is returned on success, or -1 if there was an error.

&space
&see
	&seelink(socket:ng_siinit)
	&seelink(socket:ng_sipoll)
	&seelink(socket:ng_cbreg)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Mar 1995 (sd) : Original implementation.
&term	01 Sep 2003 (sd) : Mod to allow multiple UDP ports to be open.
&term	
&endterms


&scd_end
------------------------------------------------------------------ */
