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
*  Mnemonic: ng_sinewsession
*  Abstract: This routine can be called when a request for connection is
*            received. It will establish a new fd, create a new
*            transport provider block (added to the head of the list) and
*            will optionally fork a new child if the fork flag is set in the
*            general info block. The security callback and connection callback
*            routines are driven from this routine.
*  Parms:    gptr - Pointer to the general information block
*            tpptr- Pointer to the tp block that describes the fd that
*                   received the connection request.
*  Returns:  OK if all went well, ERROR if not.
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*
*  Mods:	8 Oct 2001 - conversion to use ng_ address routines
*		8 Oct 2008 - added support for nodelay on acceptance of new session.
******************************************************************************
*/
#include "sisetup.h"          /* get necessary defs etc */
#include <netinet/in.h>
#include <netinet/tcp.h>

int ng_sinewsession( struct tp_blk *tpptr )
{
 	extern struct ginfo_blk *gptr;
	extern int si_no_delay_flag;

 struct sockaddr *addr;             /* pointer to address of caller */
 struct tp_blk *newtp;              /* pointer at new tp block */
 int status = OK;                   /* processing status */
 int cpid;                          /* child process id after fork */
 int (*cbptr)();                    /* pointer to callback function */
 /*size_t addrlen; */                      /* length of address from accept */
 int addrlen;                       /* length of address from accept */
 /*char buf[50]; */                     /* buff to put dot decimal address in */
 char	*addr_str = NULL;			/* pointer to address */

 addr = (struct sockaddr *) ng_malloc( sizeof( struct sockaddr_in ), "sinewsess:addr" );
 addrlen = sizeof( struct sockaddr_in );

 status = accept( tpptr->fd, addr, &addrlen );   /* accept and assign new fd */
 addr_str = ng_addr2dot( (struct sockaddr_in *) addr );

 if( status >= OK )     /* session accepted and new fd assigned (in status) */
  {
   newtp = ng_sinew( TP_BLK );      /* get a new tp block for the session */
   if( newtp != NULL )
    {
     newtp->next = gptr->tplist;  /* add new block to the head of the list */
     if( newtp->next != NULL )
      newtp->next->prev = newtp;      /* back chain to us */
     gptr->tplist = newtp;
     newtp->paddr = (struct sockaddr_in *) addr; /* partner address*/
     newtp->fd = status;                         /* save the fd from accept */
       	if( si_no_delay_flag )
		setsockopt(newtp->fd, IPPROTO_TCP, TCP_NODELAY, &si_no_delay_flag, sizeof(si_no_delay_flag) );

     if( (cbptr = gptr->cbtab[SI_CB_SECURITY].cbrtn) != NULL )
      {                     /*  call the security callback module if there */
       /*ng_siaddress( addr, buf, AC_TODOT ); */
       status = (*cbptr)( gptr->cbtab[SI_CB_SECURITY].cbdata, addr_str );
       if( status == SI_RET_ERROR )           /* session to be rejected */
        {
         close( newtp->fd );     
         ng_siterm( newtp );               /* terminate new tp block */
         newtp = NULL;                        /* indicates failure later */
        }
       else
        ng_sicbstat( status, SI_CB_SECURITY );    /* handle other status */
      }                                    /* end if call back was defined */

     if( newtp != NULL )        /* accept and security callback successful */
      {
       newtp->flags |= TPF_SESSION;     /* indicate a session here */

       if( gptr->flags & GIF_FORK )     /* create a child to handle session? */
        {
         gptr->childnum++;                      /* increase to next value */
         if((cpid = fork( )) < 0 )               /* fork error */
          status = ERROR;                        /* indicate this */ 
         else
          if( cpid > 0 )                          /* this is the parent */
           {
            gptr->tplist->flags &= ~TPF_UNBIND;   /* turn off unbind flag */
            ng_siterm( newtp );                /* close child's fd */
           }
          else
           {                                      /* this is the child */
            while( gptr->tplist->next != NULL )   /* close parents fds */
             {
              if( gptr->kbfile >= OK )
               gptr->kbfile = -1;                 /* pseudo close the file */
              gptr->tplist->next->flags &= ~TPF_UNBIND;  /* just close */
              ng_siterm( gptr->tplist->next );        /* in term call */
             }
            gptr->flags |= GIF_AMCHILD;             /* set child indication */
           }                        /* end child setup */
        }                           /* end if we need to fork */

                    							/* if conn call back defined and child or not forking */
       if( (cbptr = gptr->cbtab[SI_CB_CONN].cbrtn) != NULL && ( (gptr->flags & GIF_AMCHILD) || ! (gptr->flags & GIF_FORK) ) )
       {
        /* ng_siaddress( addr, buf, AC_TODOT );           */   /* convert address */
         status=(*cbptr)( gptr->cbtab[SI_CB_CONN].cbdata, newtp->fd, addr_str );		/* drive callback */
         ng_sicbstat( status, SI_CB_CONN );               /* handle status */
       }                                
     }                                 
     else                                          /* accept failed */
      if( newtp != NULL )
       ng_siterm( newtp );      /* trash the provider block, & close */
    }                              /* end if established new fd ok */
   else
    status = ERROR;              /* indicate problems */
  }                              /* end if listen completed successfully */
 else                            /* listen failed */
  {
   ng_free( addr );
   status = ERROR;                /* indicate errors */
  }

 if( addr_str )
	ng_free( addr_str );

 return status;
}                                /* ng_sinewsession */
