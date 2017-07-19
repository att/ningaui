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
****************************************************************************
*
*  Mnemonic: ng_sicbreg
*  Abstract: This routine will register a callback in the table. Callbacks
*            are "unregistered" by passing a null function pointer.
*  Parms:    gptr - pointer to the general information block (SIHANDLE)
*            type - Type of callback to register (SI_CB_xxxxx)
*            fptr - Pointer to the function to register
*            dptr - Pointer that the user wants the callback function to get
*  Returns:  Nothing.
*  Date:     23 January 1995
*  Author:   E. Scott Daniels
*
****************************************************************************
*/
#include "sisetup.h"     /* get defs and stuff */

void ng_sicbreg( int type, int ((*fptr)()), void * dptr )
{
 extern struct ginfo_blk *gptr;

 if( gptr->magicnum == MAGICNUM )    /* valid block from user ? */
  {
   if( type >= 0 && type < MAX_CBS )   /* if cb type is in range */
    {
     gptr->cbtab[type].cbdata = dptr;   /* put in data */
     gptr->cbtab[type].cbrtn = fptr;    /* save function ptr */ 
    }
  }      /* endif valid block from user */
}        /* ng_sicbreg */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sicbreg:Register a user callback function)

&space
&synop	
	#include <ng_socket.h>
&space
	int ng_sicbreg( int type int ((*function)()), void *data );

&space
&desc	
	This function registers a user function that will be invoked 
	when events that match &ital(type) are noticed by &lit(ng_siwait.)
	The parameters of each callback function are described with the 
	callback types below:

&space
&lgterms
&term 	SI_CB_SIGNAL   : Drives the callback when a singal has been caught. Parameters passed to 
	the function are: void *data, int signumber. 
&space
&term 	SI_CB_UDATA    : Driven when UDP data is received. The parameters for the udp callback function are:
	void *data, char *buf, int buf_len, char *address, int sid.  
&space
&term 	SI_CB_CDATA    : Driven when TCP data is received.  The parameters passed to the tcp callback function are:
	void *data, int sid, char *buf, int buf_len.
&space
&term 	SI_CB_KDATA    : This callback is invoked when there is data on the standard input (keyboard). The parameters
	passed to the function are:
	void *data, char *buf.
	In this case, buf contains a null terminated ascii string as data from the standard input is assumed to be newline terminated. 
	(Stdin data is only read if the proper option (SI_OPT_TTY) is passed to the ng_siinit() function.)
&space
&term 	SI_CB_SECURITY : This callback function is invoked as a session connection request is being authorised. The function 
	is passed the following arguments:
	void *data, char *address. If this callback is not defined all sessions are allowed.
&space
&term 	SI_CB_CONN     : The connection callback is driven after session authorisation callback and is a notification to the user 
	programme that the session has been established. The parameters passed to this callback function are: 
	void *data, char *address.
&space
&term 	SI_CB_DISC    : When a session is lost this callback is driven. The parameters passed to this function are: 
	void *data, int sid.
&endterms

&space
&return The session id is returned on success, or -1 if there was an error.

&space
&see
	&seelink(socket:ng_siinit)
	&seelink(socket:ng_sicbreg)
	&seelink(socket:ng_sipoll)

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
