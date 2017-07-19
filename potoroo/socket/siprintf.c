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
*  Mnemonic: ng_siprintf
*  Abstract: This routine will send a datagram to the TCP session partner
*            that is connected via the FD number that is passed in.
*		The parameters following the fd are the same as for bleat or
*		any of the printf functions.
*  Parms:    
*            fd   - File descriptor (session number)
*            fmt  - format string
*            args
*  Returns:  SI_OK if sent, SI_QUEUED if queued for later, SI_ERROR if error.
*  Date:     16 June 2003
*  Author:   E. Scott Daniels
*
*  Mod:	     13 Sep 2004 (sd) : Include stdarg first.
*****************************************************************************
*/
#include	<stdarg.h>     /* required for variable arg processing -- MUST be included first to avoid ast stdio.h issues */
#include "sisetup.h"     /* get setup stuff */
#include	<stdio.h>

static int send_asciiz = 0;			/* set to 1 if we send end of string too */

void ng_siprintz( int n )			/* called to set/restore the flag */
{
	send_asciiz = n ? 1 : 0;		/* this is the number of extra bytes we add to send length */
}

int ng_siprintf( int fd, char *fmt, ...)
{
	extern int verbose;
	extern int ng_sierrno;

	va_list	argp;			/* pointer at variable arguments */
	char	obuf[NG_BUFFER];	/* final msg buf - allow ng_buffer to caller, 1k for header*/
	char	*uidx; 			/* index into output buffer for user message */
	char	*s;    			/* pointer into string buffer */
	int	space; 			/* amount of space in obuf for user message */
	int	hlen;  			/* size of header in output buffer */


	ng_sierrno = SI_ERR_HANDLE;
	if( fd < 0 )	
		return SI_ERROR;

	va_start( argp, fmt );                       /* point to first variable arg */
	vsnprintf( obuf, sizeof( obuf ) - 1, fmt, argp );
	va_end( argp );                                 /* cleanup of variable arg stuff */

	return ng_sisendt( fd, obuf, strlen( obuf ) + send_asciiz );
}
