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
*  Mnemonic: ng_siclear
*  Abstract: Clear anything that we have queued on the session -- assuming that 
*		the user will call close and does not want the session to be put 
*		into a drain state. 
*  Parms:    
*            fd   - FD of the session to flush
*  Returns:  SI_OK if all goes well, SI_ERROR with ng_sierrno set if there is
*            a problem.
*  Date:     2 May 2006
*  Author:   E. Scott Daniels
*
*  Modified: 
*           	11 Aug 2008 (sd) : Corected cleanup -- freeing sdata was a no-no.
#				Corrected a bug: stail was not being set to null.
******************************************************************************
*/
#include "sisetup.h"

int ng_siclear( int fd )
{
	extern int ng_sierrno;
	extern struct ginfo_blk *gptr;

	struct ioq_blk *ibp;		/* current one working with */
	struct tp_blk *tpptr;      /* pointer into tp list */
	int status = SI_ERROR;     /* status of processing */

	ng_sierrno = SI_ERR_HANDLE;
	if( gptr->magicnum == MAGICNUM )   /* good cookie at the gptr address? */
	{
		ng_sierrno = SI_ERR_SESSID;
	
		if( fd >= 0 )     /* if caller knew the fd number */
		{
			for( tpptr = gptr->tplist; tpptr != NULL && tpptr->fd != fd;
				tpptr = tpptr->next );   /* find the tppblock to close */
		}
		else  				/* user did not know the fd - find first UDP tp blk */
			for( tpptr = gptr->tplist; tpptr != NULL && tpptr->type != SOCK_DGRAM;
				tpptr = tpptr->next );

		if( tpptr != NULL )
		{
			while( ibp = tpptr->squeue )		/* while something  queued */
			{
				tpptr->squeue = ibp->next;

				ng_free( ibp->data );		/* do NOT free sdata as it may point inside data buffer! */
				if( ibp->addr );
					ng_free( ibp->addr );
				ng_free( ibp );
			}

			tpptr->stail = NULL;
			
			status = SI_OK;               
		}                              /* end if we found a tpptr */
	}                                	/* end if the handle was good */

	return( status );                 /* send the status back to the caller */
}                                  

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siclear:Clear write buffer)

&space
&synop	int ng_si_clear( int sid );

&space
&desc	Invoking this funciton causes any queued write buffers for the session (sid) to be removed.
	This function should be invoked before closing a session if the user programme does not want 
	the session to 'hang' until the remaining buffers are sent.  

&space
&parms	These parameters are passed to this function:
&begterms
&term	sid : The sesison id that was given to the user programme when the session was started.
&endterms


&space
&returns	Returns &lit(SI_OK) if the session was successfully cleared. A return that is not this 
	inidciates an error.

&space
&see
	&seelink(socket:ng_siclose)
	&seelink(socket:ng_siconnect)
	&seelink(socket:ng_sisend)
	&seelink(socket:ng_sisendt)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 May 2006 (sd) : Its beginning of time. 
&term   11 Aug 2008 (sd) : Corected cleanup -- freeing sdata was a no-no.  Corrected a bug: stail was not being set to null.
&endterms


&scd_end
------------------------------------------------------------------ */

