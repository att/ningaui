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
*  Mnemonic: siconst.h
*  Abstract: This file contains the constants that are necessary to support
*            The SI (Socket Interface) run-time routines
*	     Not considered for public consumption
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*
*
*  Mods:	11 Aug 2008 (sd) : Added MUSTQUEUE flag to allow user to force
*			data to queue even if session is not blocking.
****************************************************************************
*/

#define OK    0
#define ERROR (-1)

#define EOS   '\000'         /* end of string marker */

                              /* general info block flags */
#define GIF_SHUTDOWN   0x01   /* shutdown in progress */
#define GIF_FORK       0x02   /* fork a child for each session if set */
#define GIF_AMCHILD    0x04   /* am a child process */
#define GIF_NODELAY    0x08   /* set no delay flag on t_opens */
#define GIF_CEDE	0x10	/* sipoll should cede control back to user */

                              /* transmission provider block flags */
#define TPF_LISTENFD   0x01   /* set on tp blk that is fd for tcp listens */
#define TPF_SESSION    0x02   /* session established on this fd */
#define TPF_UNBIND     0x04   /* NIterm should unbind the fd */
#define TPF_DRAIN      0x08   /* session is being drained */
#define TPF_DELETE	0x10	/* block should be deleted when it is safe */
#define TPF_MUSTQUEUE	0x20	/* we must queue buffers for the session -- user can set/reset with ng_siforcequeue() */

#define MAX_CBS         8     /* number of supported callbacks in table */
#define MAX_FDS        16     /* max number open fds supported */
#define MAX_RBUF       NG_BUFFER   /* max size of receive buffer */
#define MAX_READ	NG_BUFFER-2	/* max to read */

#define TP_BLK    0           /* block types for rsnew */
#define GI_BLK    1           /* global information block */
#define IOQ_BLK   2           /* input output block */

#define MAGICNUM   219        /* magic number for validation of things */

#define AC_TODOT  0           /* convert address to dotted decimal string */
#define AC_TOADDR 1           /* address conversion from dotted dec */

#define NO_EVENT 0            /* look returns 0 if no event on a fd */
