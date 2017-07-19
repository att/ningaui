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
*
*  Mnemonic: ng_sisetsig
*  Abstract: This routine will inform the operating system of the singals
*            that we need to catch. This routine is broken out to provide
*            the user program the ability to reset the ni signal handler
*            should a DB (or other unfriendly rtl) stomp on the signal
*            trapping that we established during initialization.
*  Parms:    sigs - Bit mask indicating signals to catch.
*  Returns:  Nothing.
*  Date:     9 February 1995
*  Author:   E. Scott Daniels
*
*  Modified:  6 Mar 1995 - To accept parm indicating signal(s) to be trapped
*****************************************************************************
*/
#include "sisetup.h"         /* get the necessary start up stuff */

void ng_sisetsig( int sigs )
{
 struct sigaction sact;                /* signal block to pass to os */


 memset( &sact, 0, sizeof( sact ) );
 sact.sa_handler = &ng_sisignal;         /* setup our signal trap */

 if( sigs & SI_SIG_TERM )
  sigaction( SIGTERM, &sact, NULL );    /* catch term signals */

 if( sigs & SI_SIG_HUP )
  sigaction( SIGHUP, &sact, NULL );     /* catch hup signals */

 if( sigs & SI_SIG_QUIT )
  sigaction( SIGQUIT, &sact, NULL );    /* catch quit signals */

 if( sigs & SI_SIG_USR1 )
  sigaction( SIGUSR1, &sact, NULL );    /* catch user1 signals */

 if( sigs & SI_SIG_USR2 )
  sigaction( SIGUSR2, &sact, NULL );    /* catch user2 signals */

 if( sigs & SI_SIG_ALRM )
  sigaction( SIGALRM, &sact, NULL );   /* catch alarm signals */
}                  /* SIsetsig */
