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
*  Mnemonic: ng_sipoll
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
*  Parms:    delay - 100ths of seconds to block (max).
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
*  Modified: 	11 Apr 1995 - To pass execption to select when no keyboard
*            	19 Apr 1995 - To call do key to do better keyboard editing
*            	18 Aug 1995 - To init kstat to 0 to prevent key hold if
*                          network data pending prior to entry.
*	     	16 Oct 2001 - To correct problem with tp block ref afer disc
*	     	09 Feb 2002 - To change ms delay to 100ths of seconds.
*	     	28 Oct 2004 (sd) - corrected an issue where if we shutdown in 
*			the processing loop we core dumped.
*	     	24 May 2005 (sd) - now tests to see that fd is >= 0 (core dumps
*			if the session was closed.)
*	     	16 June 2005 (sd)  - corrected the position of assigning 'nextone'
*			as it was not being assigned when the fd was < 0.
*	     	28 Sep 2006 (sd) : Cleanup of some comments (HBD DAWD)
*		05 Nov 2007 (sd) : initialise tpptr and nextone.
*		11 aug 2008 (sd) : added check for force2queue flag; we do not send 
*			queued data if this is set. 
**************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

int ng_sipoll( int msdelay )
{
	extern int errno;
	extern int ng_sierrno;
	extern int deaths;       /* number of children that died and are zombies */
	extern int sigflags;     /* flags set by the signal handler routine */
	extern struct ginfo_blk *gptr;

	int fd;                       /* file descriptor for use in this routine */
	int ((*cbptr)());             /* pointer to callback routine to call */
	int status = OK;              	/* return status */
	int addrlen;                  	/* length of address from recvfrom call */
	int i;                        	/* loop index */
	struct tp_blk *tpptr = NULL;         /* pointer at tp stuff */
	struct tp_blk *nextone = NULL;       /* pointer at next tp block to examine */
	int pstat;                    	/* poll status */
	struct timeval  delay;        	/* delay to use on select call */
	struct sockaddr uaddr;       	/* pointer to udp address */
	char 	ibuf[2048];		/* keyboard input buffer */
	char *addr_str;			/* converted address string */


	ng_sierrno = SI_ERR_SHUTD;

	if( gptr->flags & GIF_SHUTDOWN )     /* cannot do if we should shutdown */
		return( ERROR );                    /* so just get out */

	ng_sierrno = SI_ERR_HANDLE;

	if( gptr->magicnum != MAGICNUM )     /* if not a valid ginfo block */
		return( ERROR );

	delay.tv_sec = msdelay/100;		/* user submits 100ths, cvt to seconds and milliseconds */
	delay.tv_usec = (msdelay%100) * 10;       

	ng_sibldpoll( gptr );                 /* build the fdlist for poll */
	pstat = select( gptr->fdcount, &gptr->readfds, &gptr->writefds, &gptr->execpfds, &delay ); 

	if( (pstat < 0 && errno != EINTR) || (sigflags & SI_SF_QUIT) )
	{                             /* poll fail or termination signal rcvd */
		gptr->fdcount = 0;           /* prevent trying to look at a session */
		gptr->flags |= GIF_SHUTDOWN; /* cause cleanup and exit at end */
		deaths = 0;                  /* dont need to issue waits on dead child */
		sigflags = 0;                /* who cares about signals now too */
	}

	if( deaths )			/* keep the dead from being zombies -- send to heaven */
	{                   		
		while( waitpid( 0, NULL, WNOHANG ) > 0 );                    /* non-blocking, portable wait for all dead kids */
		deaths = 0;
	}                   

	if( sigflags && (cbptr = gptr->cbtab[SI_CB_SIGNAL].cbrtn) != NULL )	 /* if signal received and processing them */
	{
		while( sigflags != 0 )
		{
			i = sigflags;                  /* hold for call */
			sigflags = 0;                  /* in case we are interrupted while away */
			status = (*cbptr)( gptr->cbtab[SI_CB_SIGNAL].cbdata, i );
			ng_sicbstat( status, SI_CB_SIGNAL );    /* handle status */
		}                                           /* end while */
	}

	if( pstat > 0  &&  (! (gptr->flags & GIF_SHUTDOWN)) )
	{
#ifndef OS_IRIX
		if( FD_ISSET( 0, &gptr->readfds ) )       /* check for keybd input (hard test on fd == 0)*/
		{
			fgets( ibuf, 2000, stdin );   /* get the stuff from keyboard */
			if( (cbptr = gptr->cbtab[SI_CB_KDATA].cbrtn) != NULL )
			{
				status = (*cbptr)( gptr->cbtab[SI_CB_KDATA].cbdata, ibuf );
				ng_sicbstat( status, SI_CB_KDATA );    /* handle status */
			}
		}
#endif

		/*for( tpptr = gptr->tplist; tpptr != NULL && !(gptr->flags & GIF_SHUTDOWN); tpptr = nextone )*/
		for( tpptr = gptr->tplist; tpptr != NULL; tpptr = nextone )
		{
			nextone = tpptr->next;			/* in case we disc and trash the block later */

			if( tpptr->fd >= 0 ) 			/* we did not term the session */
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
				else
				{
					if( FD_ISSET( tpptr->fd, &gptr->readfds ) )  /* read event pending? */
					{
						fd = tpptr->fd;                     /* quick ref to the fd */
	
	 					if( tpptr->flags & TPF_LISTENFD )     /* listen port setup by init? */
	  					{                                    /* yes-assume new session req */
							errno=0;
	   						status = ng_sinewsession( tpptr );    /* make new session */
	  					}
	 					else                     			/* data received on a regular port */
						{
	  						if( tpptr->type == SOCK_DGRAM )		/* udp socket? */
	   						{
								addrlen = sizeof( uaddr );
	    							status = recvfrom( fd, gptr->rbuf, MAX_READ, 0, &uaddr, &addrlen );
	    							if( status > 0 && ! (tpptr->flags & TPF_DRAIN) )
	     							{                         /* if good status call cb routine */
	      								if( (cbptr = gptr->cbtab[SI_CB_RDATA].cbrtn) != NULL )
	       								{
										gptr->rbuf[status] = 0;	/* safe even if binary data is in buffer */
										addr_str = ng_addr2dot( (struct sockaddr_in *) &uaddr );
										status = (*cbptr)( gptr->cbtab[SI_CB_RDATA].cbdata,
				      								gptr->rbuf, status, addr_str, fd );
										ng_free( addr_str );
										ng_sicbstat( status, SI_CB_RDATA );    /* handle status */
	       								}
	     							} 
	   						}                                  
	  						else
	   						{                                			/* else receive on tcp session */
	    							status = recv( fd, gptr->rbuf, MAX_READ, 0 );	/* read data */
	    							if( status > OK  &&  ! (tpptr->flags & TPF_DRAIN) )
	     							{
	      								if( (cbptr = gptr->cbtab[SI_CB_CDATA].cbrtn) != NULL )
	       								{
										gptr->rbuf[status] = 0;			/* safe even if binary data is in buffer */
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
	   						}                       /* end tcp read */
						}				/* end else data received on regular port */
					}		/* end read pending */
				}                    /* end if event on this fd */
			}                      /* end for each fd in the list */
		}		/* end if fd was still good */
	}                        /* end if not in shutdown */

	if( gptr->flags & GIF_SHUTDOWN )      /* we need to stop for some reason */
	{
		if( gptr->tplist == NULL )        /* indicate all fds closed */
			ng_sierrno = SI_ERR_NOFDS;
		ng_sierrno = SI_ERR_SHUTD;             /* indicate error exit status */
		status = ERROR;                /* status should indicate to user to die */
		ng_sishutdown( gptr );            /* clean things up */
	}                          /* end if shutdown */
	else
		status = pstat;               /* user can continue to process */

	return( status );           /* send status back to caller */
}      


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sipoll:Socket Interface Callback Driver)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_sipoll( int delay );

&space
&desc	
	Once the socket interface has been established, and callback functions 
	registered, the &lit(ng_sipoll) function can be invoked to check for 
	events and drive callback functions based on the current events. Unlike
	&lit(ng_siwait) this function returns control to the user programme after one round 
	of checking. The delay value passed in is the maximium time that (ms) the function
	will wait for an event to occur. 


&space
&return 
	The status returned to the caller is SI_OK for normal processing, and SI_ERROR to 
	indicate that a callback flagged the socket interface environemnt to stop. 

&space
&see
	&seelink(socket:ng_siinit)
	&seelink(socket:ng_siwait)
	&seelink(socket:ng_cbreg)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	 28 Mar 1995 (sd) : Original code.
&term	11 Apr 1995 (sd) :  To pass execption to select when no keyboard
&term	19 Apr 1995 (sd) :  To call do key to do better keyboard editing
&term	18 Aug 1995 (sd) :  To init kstat to 0 to prevent key hold if
&term	  	network data pending prior to entry.
&term	16 Oct 2001 (sd) :  To correct problem with tp block ref afer disc
&term	09 Feb 2002 (sd) :  To change ms delay to 100ths of seconds.
&term	28 Oct 2004 (sd) (sd) :  corrected an issue where if we shutdown in 
&term		the processing loop we core dumped.
&term	24 May 2005 (sd) (sd) :  now tests to see that fd is >= 0 (core dumps
&term		if the session was closed.)
&term	16 June 2005 (sd)  (sd) :  corrected the position of assigning 'nextone'
&term		as it was not being assigned when the fd was < 0.
&endterms


&scd_end
--------------------------------------------------------------------- */
