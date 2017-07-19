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
*
*  Mnemonic: ng_siexamp
*  Abstract: This is an example program using the SI run-time library to
*            communicate with another process. The foreign process' IP
*            address and port number is assumed to be passed in from the
*            command line. Once the connection is made the user is prompted
*            for input and the line is echoed to the foreign process.
*            This program is implemented using ng_sircv and not the callback
*            driver functions of the SI run-time library.
*  Parms:    argc, argv
*              argv[1] = xx.xx.xx.xx.port  (IP address of foreign process)
*  Returns:  Nothing.
*  Date:     30 March 1995
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
#include <stdio.h>
#include <string.h>

#include "sidefs.h"                 /* get defines for SI calls */

#define EOS '\000'

main( int argc, char **argv )
{
 SIHANDLE handle;    /* SI handle for SI calls - returned by ng_siinit */
 int sid;            /* session id of foriegn process session */
 int done = 0;	   /* flag to indicate we are done with the session */
 int kstat;          /* keyboard status */
 int rlen;           /* length of received string */
 char buf[256];      /* buffer to receive information in */
 char kbuf[256];     /* keyboard buffer */
 int s;
 extern int errno;

 if( argc < 2 )
  printf( "Missing arguments - enter IP.port address of foreign process\n" );
 else
  {
   if( (handle = ng_siinit( SI_OPT_FG, -1, -1 )) != NULL_HANDLE )
    {
     if( (sid = ng_siconnect( handle, argv[1] )) >= SI_OK )
      {
       printf( "Connection to %s was successful. \n", argv[1] );
       while( ! done )       /* loop until we are finished */
        {
         kstat = 0;
         rlen = 0;
         while( !(kstat = kbhit( )) &&               /* wait for key or msg */
                 (rlen = ng_sircv( handle, sid, buf, 255, 100 )) == 0 );

         if( kstat )         /* keyboard data waiting */
          {
           gets( kbuf );                        /* read and send to partner */
           if( strcmp( "stop", kbuf ) == 0 )    /* user say were done? */
            {
             printf( "User requested termination of sample program.\n" );
             done = 1;
            }

           if( ng_sisendt( handle, sid, kbuf, strlen( kbuf )+1 ) == SI_ERROR )
            {
             printf( "Error sending. Session lost?\n" );
             done = 1;
            }
          }
         else                   /* not keyboard - check for received data */
          {
           if( rlen > 0 )
             printf( "From partner: %s\n", buf );  /* print buffer */
           else
            if( rlen < 0 )          /* assume error on the session */
             {
              done = 1;          /* force session to stop */
              printf( "Session error - Session lost?\n" );
             }
          }
        }            /* end while not done */

       ng_sishutdown( handle );        /* were done - clean things up */
      }
     else
      printf( "Unable to connect to: %s\n", argv[1] );
    }
   else
    printf( "Unable to initialize the SI package. ng_sierrno = %d\n", ng_sierrno );
  }

}         /* end main */
