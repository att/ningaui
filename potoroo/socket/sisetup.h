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
*  Mnemonic: nisetup.h
*  Abstract: This file contains the necessary include statements for each
*            individually compiled module.
*  Date:     17 January 1995
*  Author:   E. Scott Daniels
*  Mods:	7 Oct 2001 : port into Ningaui
*****************************************************************************
*/
#include <sys/types.h>          /* various system files - types */
#include <sys/socket.h>         /* socket defs */
#include <stdio.h>              /* standard io */
#include <errno.h>              /* error number constants */
#include <fcntl.h>              /* file control */
#include <netinet/in.h>         /* inter networking stuff */
#include <signal.h>             /* info for signal stuff */
#include <string.h>
#include <sys/time.h>		/* required for linux */
#include <sys/wait.h>

#include <ningaui.h>
#include <ng_lib.h>

/* pure bsd supports SIGCHLD not SIGCLD as in bastard flavours */
#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#define SI_RTN 1               /* includes internal prototypes from ng_socket.h */

#include "./siconst.h"          /* our constants */
#include "./sistruct.h"         /* our structure definitions */
#include "ng_socket.h"           /* our public defs */
