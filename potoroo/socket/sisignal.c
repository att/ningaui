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
******************************************************************************
*  Mnemonic: ng_sisignal
*  Abstract: This routine is called to process signals. It will set a flag
*            in the global var sigflags indication action is necessary in
*            the application. In the case of a child's death signal, the
*            deaths variable is incremented so that the parent knows the
*            number of waits to issue to clear all zombies.
*  Parms:    sig - The signal we are being invoked to handle.
*  Returns:  Nothing.
*  Date:     19 January 1995
*  Author:   E. Scott Daniels
*
******************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

void ng_sisignal( int sig )
{
 extern int deaths;      /* deaths in the family - we must clean up */
 extern int sigflags;    /* a mask of flags that have happened since last call */

 switch( sig )
  {
   case SIGCHLD:      /* child has passed to the great beyond */
    deaths++;        /* let parent (us) know we must burry it (wait) */
    break;

   case SIGQUIT:                /* set the termination flag */
sfprintf( sfstderr, "ng_sisignal: SIGQUIT trapped\n" );
#ifdef KEEP
	abort( );
	exit( 1 );
#endif
	break;

   case SIGTERM:
    sigflags |= SI_SF_QUIT;     /* set the quit flag -- terminate nicely */
    break;

   case SIGUSR1:              /* parent has sent us a request for status */
    sigflags |= SI_SF_USR1;    /* so let the main routine (wait) know */
    break;

   case SIGUSR2:
    sigflags |= SI_SF_USR2;
    break;

   case SIGALRM:
    sigflags |= SI_SF_ALRM;

   default:          /* no action if we dont recognize the signal */
    break;
  }                   /* end switch */
}                 /* ng_sisignal */
