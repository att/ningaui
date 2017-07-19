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
*  Mnemonic: ng_sireinit
*  Abstract: This routine is called to re-initialise the Network Interface
*            routines after the user process has execed another SI based
*            process over the parent that accepted the session. We will 
*            rebuild the global data structure, and tp block for the fd 
*            passed in. This routine will also reset the signals. 
*            It is assumed that the process is executing in the background.
*            If the caller needs to have the SI routines trap the ALARM
*            signal it will need to call the ng_sisetsig routine directly with
*            the SI_SIG_ALRM flag as the parameter as there is no way for
*            this routine to receive a parameter to indicate that it is to 
*            be set. 
*  Parms:    gptr - Pointer to general info block - NULL on first call. 
*            fd - The file descriptro that is mapped to a session.
*                    requested. 
*  Returns:  0 if all reinitialised ok, 1 if not.
*  Date:     15 February 1995 
*  Author:   E. Scott Daniels
*
*  Modified: 21 Feb 1995 - To allow multiple calls if more than 1 fd open.
*             6 Mar 1995 - To pass setsig a signal mask parameter
**************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

int ng_sireinit( int fd )
{
 extern struct ginfo_blk *gptr;
 extern int ng_sierrno;

 int status = OK;                /* status of internal processing */
 struct tp_blk *tpptr;           /* pointer at tp stuff */
 int i;                          /* loop index */

 ng_sierrno = SI_ERR_NOMEM;     /* setup incase alloc fails */

 if( gptr == NULL )          /* first call to init */
  {
   gptr = ng_sinew( GI_BLK );                /* get a gi block */
   if( gptr != NULL )            /* was successful getting storage */
    {
     gptr->rbuf = (char *) ng_malloc( MAX_RBUF, "sireinit:rbuf" );   /* get rcv buffer*/
     gptr->rbuflen = MAX_RBUF;

     ng_sisetsig( SI_DEF_SIGS );   /* set up default sigs that we want to catch */

     gptr->cbtab = (struct callback_blk *) ng_malloc( (sizeof( struct callback_blk ) * MAX_CBS ), "sireinit: cbtab" );
     if( gptr->cbtab != NULL )
      {
       for( i = 0; i < MAX_CBS; i++ )     /* initialise call back table */
        {
         gptr->cbtab[i].cbdata = NULL;
         gptr->cbtab[i].cbrtn = NULL;
        }
      }          /* end if gptr->ctab ok */
     else        /* if call back table allocation failed - error off */
      {
       ng_free( gptr );
       gptr = NULL;        /* dont allow them to continue */
      }
    }                 /* end if gen infor block allocated successfully */
  }                   /* end if gptr was null when called */

 if( gptr != NULL  &&  (tpptr = (struct tp_blk *) ng_sinew( TP_BLK )) != NULL )
  {
   ng_sierrno = SI_ERR_TP;            /* if bad status then its a tp error */
   status = t_sync( fd );          /* sync to the new exec */
   if( status >= OK )              /* safe to continue */
    {
     tpptr->fd = fd;                 /* assign the session id */
     tpptr->type = SOCK_DGRAM;       /* assume TCP session */
     tpptr->next = gptr->tplist;     /* add to head of the list */
     if( tpptr->next != NULL )
      tpptr->next->prev = tpptr;     /* back chain if not only one */
     gptr->tplist = tpptr; 
    }
   else                     /* problem with the sync */
    gptr = NULL;
  }
 else 
  gptr = NULL;              /* allocation problem - ensure return is null */

 return gptr == NULL ? 0 : 1; 	/* if gptr is allocated, then all ok */
}     /* ng_sireinit */
