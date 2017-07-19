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
*  Mnemonic: ng_sidokey
*  ABstract: This routine is called by siwait or sipoll to handle a key
*            that has been entered at the keyboard. We will do minor amounts
*            of "editing" control to be somewhat friendly. The key handeling
*            callback is driven when a complete message is received.
*            DOS Support only.
*  Parms:    gptr - Pointer to the master information block
*  Returns:  Nothing.
*  Date:     12 April 1995
*  Author:   E. Scott Daniels
*
******************************************************************************
*/
#include "sisetup.h"                /* various include files etc */

#define RETURN_KEY 0x0d             /* carriage return */
#define NEWLN_KEY  0x0a             /* line feed */
#define BACKSP_KEY 0x08             /* backspace key */

void ng_sidokey( )
{
 extern struct ginfo_blk *gptr;

 static char kbuf[256];                 /* keyboard type ahead buffer */
 static char *kidx = kbuf;              /* pointer at next spot for key */

 int ((*cbptr)());                      /* pointer to the callback function */
 int status;                            /* callback status processing */

 *kidx = getch( );                     /* get the key */

 switch( *kidx )                  /* see if we need to act on key value */
  {
   case RETURN_KEY:               /* buffer termination - make callback */
   case NEWLN_KEY:                /* who knows what the return generates */
     *kidx = EOS;                     /* terminate for callback */
     kidx = kbuf;                  /* reset for next pass */
     if( (cbptr = gptr->cbtab[SI_CB_KDATA].cbrtn) != NULL )
      {
       status = (*cbptr)( gptr->cbtab[SI_CB_KDATA].cbdata, kbuf );
       ng_sicbstat( status, SI_CB_KDATA );    /* handle status */
      }                                 /* end if call back was defined */
    break;

   case 0:             /* some alternate/function/arrow key pressed */
    getch( );          /* ignore it */
    break;

   case BACKSP_KEY:                 /* allow ctlh to edit buffer */
    putch( *kidx );                 /* echo the back sp */
    putch( ' ' );                   /* blank what was there */
    putch( *kidx );                 /* move back */
    kidx--;                         /* and back up the pointer */
    break;

   default:                        /* not specially processed */
    if( *kidx >= 0x20 )            /* if it is printable */
     {
      putch( *kidx );              /* echo the keystroke */
      kidx++;                      /* save it and move on to next */
     }
    break;
   }                                /* end switch */

}                                   /* ng_sidokey */
