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
*  Mnemonic: ng_siclose
*  Abstract: This routine allows the user application to close a port
*            associated with a file descriptor. The port is unbound from
*            the transport providor even if it is marked as a listen
*            port. If the fd passed in is less than 0 this routine assumes
*            that the UDP port opened during init is to be closed (user never
*            receives a fd on this one).
*  Parms:    
*            fd   - FD to close.
*  Returns:  SI_OK if all goes well, SI_ERROR with ng_sierrno set if there is
*            a problem.
*  Date:     3 February 1995
*  Author:   E. Scott Daniels
*
*  Modified: 	19 Feb 1995 - To set TP blk to drain if output pending.
*            	10 May 1995 - To change SOCK_RAW to SOCK_DGRAM
*		29 Aug 2003 (sd) - To close and mark for delete; not to call term
*		03 May 2005 (sd) - added support for primary listen fd
*		31 Mar 2008 (sd) - corrected bug in setting flags;
******************************************************************************
*/
#include "sisetup.h"

int ng_siclose( int fd )
{
 extern int ng_sierrno;
 extern struct ginfo_blk *gptr;

 struct tp_blk *tpptr;      /* pointer into tp list */
 int status = SI_ERROR;     /* status of processing */

 ng_sierrno = SI_ERR_HANDLE;
 if( gptr->magicnum == MAGICNUM )   /* good cookie at the gptr address? */
  {
   ng_sierrno = SI_ERR_SESSID;

   if( fd >= 0 )     /* if caller knew the fd number */
   {
	if( fd == gptr->primary_listen_fd )
		gptr->primary_listen_fd = -1;

	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != fd; tpptr = tpptr->next );   /* find the tppblock to close */
   }
   else  			/* user did not know the fd - find first UDP tp blk */
	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM; tpptr = tpptr->next );

   if( tpptr != NULL )
    {
     ng_sierrno = SI_ERR_TP;

     if( tpptr->squeue == NULL )   /* if nothing is queued to send... */
      {
        tpptr->flags |= TPF_UNBIND;   	/* ensure port is unbound from tp */
	tpptr->flags |= TPF_DELETE;	/* delete the transport provider block when safe; may not be safe now */

	close( tpptr->fd );		/* close it, then mark for delete during next bld poll */
	tpptr->fd =  -1;		/* prevent accidental use */
	tpptr->type = -1;		/* no type */

       /* ng_siterm( tpptr );*/        /* do NOT run term here -- it may not be safe (we set drop flag earlier) */
      }
     else                       /* stuff queued to send - mark port to drain */
      tpptr->flags |= TPF_DRAIN;   /* and we will term the port when q empty */

     status = SI_OK;               /* give caller a good status */
    }                              /* end if we found a tpptr */
  }                                /* end if the handle was good */

 return( status );                 /* send the status back to the caller */
}                                  /* ng_siclose */

/* close the first tcp listen port on the list */
int ng_siclose_listen( )
{
 	extern struct ginfo_blk *gptr;
 	extern int ng_sierrno;
	struct tp_blk *tpptr;      /* pointer into tp list */

	for( tpptr = gptr->tplist; tpptr != NULL && !(tpptr->flags & TPF_LISTENFD); tpptr = tpptr->next );	/* find first tcp listen port */

	if( tpptr )					/* found one; close it */
		return ng_siclose( tpptr->fd );

 	ng_sierrno = SI_ERR_TPORT;
	return SI_ERROR;
}

/* close the first udp  port on the list */
int ng_siclose_udp( )
{
 	extern struct ginfo_blk *gptr;
 	extern int ng_sierrno;
	struct tp_blk *tpptr;      /* pointer into tp list */

	for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM; tpptr = tpptr->next );		/* find first udp port */

	if( tpptr )
		return ng_siclose( tpptr->fd );
	
 	ng_sierrno = SI_ERR_UPORT;
	return SI_ERROR;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siclose:Close a session)

&space
&synop	
	#include <ng_socket.h>
&break	int ng_siclose( int sid )
&break	int ng_siclose_listen( )
&break	int ng_siclose_udp()

&space
&desc	The ng_siclose() function causes the session associated with the session id passed in to be closed.
	If there are queued buffers to send on the session, the real session will be kept alive 
	until those buffers have been sent.  If the remote side of the socket is hung, closing the
	session without first clearing the buffers (see ng_siclear), could cause undesired results. 
&space
	As the only parameter, this function expects that it will be passed the session id that was 
	returned to the user programme when the session was established. 
&space
	The ng_siclose_listen() function finds the first open TCP listening port and closes it. 
	This is a convenience function to cose the listen port that might have been opened by ng_siinit()
	as the user programme typically does not receive the associated session id that would be 
	needed to pass to ng_siclose().
&space
	If a port was found, the return value is that of the close operation. If a port could not 
	be found to close, then SI_ERROR is returned with ng_sierrno set to SI_ERR_TPORT. 

&space
	The ng_siclose_udp() function closes the first UDP (datagram oriented) port on the session 
	list. Like the ng_siclose_listn() function this is primarily to allow the user programme to 
	close the UDP port opened by the ng_siinit() function as it's session id is not available 
	to the user programme.
&space
	If a port was found, the return value is that of the close operation. If a port could not 
	be found to close, then SI_ERROR is returned with ng_sierrno set to SI_ERR_UPORT.

&space
&return The return code from both functions is SI_OK if the session was closed. 

&space
&see
	&seelink(socket:ng_siclear)
	&seelink(socket:ng_siconnect)
	&seelink(socket:ng_sisend)
	&seelink(socket:ng_sisendt)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Feb 1995 (sd) : Its beginning of time. 

&term 19 Feb 1995 (sd) : To set TP blk to drain if output pending.
&term 10 May 1995 (sd) : To change SOCK_RAW to SOCK_DGRAM
&term 29 Aug 2003 (sd) : To close and mark for delete; not to call term
&term 03 May 2005 (sd) : added support for primary listen fd
&term	09 Dec 2008 (sd) : Added the close_listen and close_udp functions.
&endterms


&scd_end
------------------------------------------------------------------ */

