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
*  Mnemonic: ng_die.c
*  Abstract: This file contains the routines that are necessary to support
*            cleanup processing when routines must abort.
*  Date:     3 October 1997
*  Author:   E. Scott Daniels
*
****************************************************************************
*/
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include	<sfio.h>

#include <ningaui.h>

#define MAX_FUNERALS   16                /* max # of funerals we will attend */

static void ((*funerals[MAX_FUNERALS])());   /* funerals (routines) to attend*/
static void *flowers[MAX_FUNERALS];          /* flowers (data) to send */
static int fidx = 0;                         /* index into funerals */
static int dying = 0;                        /* flag that death is immanent */

/* called when user wants to abort - first drive all routines (funerals) 
   that were registered via the atdeath routine, write a critical error
   message, and then die 
*/
void ng_die( void )
{
 char cwd[512]; 

 if( ! dying++ )            
  {
   for( fidx--; fidx >= 0; fidx-- )
    (*funerals[fidx])( flowers[fidx] );    

   getcwd( cwd, 511 );

   ng_log( LOG_CRIT, " aborted. cwd=%s UT0001", cwd);

   signal( SIGABRT, SIG_DFL );    /* return to default to cause core dump */
   abort( );                      /* cause core dump */
  }
}

/* routine driven by system on abort signals - just call die */
void ng_abort( int i )
{
 ng_die( );
}

/* register the routine and data pointer to drive if ng_die is executed */
int ng_atdeath( void f(), void *data )
{
 if( ! dying && fidx < MAX_FUNERALS )
  {
   if( fidx == 0 )          /* cause SIGABRT to be caught 1st time */
    signal( SIGABRT, ng_abort );

   funerals[fidx] = f;      /* add the funeral to the list */
   flowers[fidx] = data;    /* flowers to take to the funeral */
   fidx++;                  
   return 1;                /* indicate funeral arrangements OK */
  }
 
 return 0;
}


/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_die:Abnormal termination support routines)

&space
&synop	
	#include <ningaui.h>
&break	#include <ng_lib.h>
&break	int ng_atdeath( void f( void ), void *data );
&break  void ng_die( void );

&space
&desc	The &ital(ng_atdeath(^)) and &ital(ng_die(^)) funcitons provide
	the ability for a program to register up to 16 functions that 
	should be invoked if a Gecko infastructure routine causes the 
	program to abort. 
&space
	&ital(ng_atdeath(^)) is used to register each function and 
	an optional pointer which will be passed to the function when 
	it is invoked by &ital(ng_atdeath(^).) If the funciton does
	not require any data then NULL should be passed as the second 
	parameter to the funciton. The funciton returns a 1 if the 
	routine and data pointer were successfully registered, and a
	0 if the maximum number of routines were already registered.

&space  Any routine that has detected an error from which there is no 
	possible recovery should invoke the &ital(ng_die(^)) function.
	&ital(ng_die(^)) will invoke each function that was registered
	using &ital(ng_atdeath(^)) and then it will abort the process using
	the &ital(abort(^)) system call. The atdeath functions are invoked
	in &bold(reverse) order to the way they were registered. 
&space
	When the first cleanup routine is registered the &ital(ng_atdeath(^)
	function begins to trap SIGABRT signals. This allows abort signals
	that are generated via the &ital(abort(^)) system call to be trapped
	and the &ital(ng_die(^)) function to be driven to cleanup.  

&space
&see	abort(3C), exit(2), atexit(3C)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Oct 1997 (Sd) : Original code. 
&term	05 Nov 1997 (Sd) : To embelish gk_log changes.
&term   28 Mar 2001 (sd) : To convert from Gecko.
&term	03 Oct 2007 (sd) : Updated manual page.
&endterms
&scd_end
*/
