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
*  Mnemonic: ng_sicbstat
*  Abstract: This routine is responsible for the generic handling of
*            the return status from a call back routine.
*  Parms:    gptr - pointer to the ginfo block
*            status - The status that was returned by the call back
*            type   - Type of callback (incase unregister)
*  Returns:  Nothing.
*  Date:     23 January 1995
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
#include "sisetup.h"     /* get necessary defs etc */

void ng_sicbstat( int status, int type )
{
	extern struct ginfo_blk *gptr;

	switch( status )
	{
		case SI_RET_UNREG:                   /* unregister the callback */
			gptr->cbtab[type].cbrtn = NULL;     /* no pointer - cannot call */
			break;

		case SI_RET_CEDE:			/* cede control back to user - dont shutdown */
			gptr->flags |= GIF_CEDE;
			break;

		case SI_RET_QUIT:                 /* callback wants us to stop */
			gptr->flags |= GIF_SHUTDOWN;    /* so turn on shutdown flag */
			break;

		default:                 /* ignore the others */
			break;
	} 
}         /* ng_sicbstat */
