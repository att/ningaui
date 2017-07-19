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
DEPRECATED 

This has been deprecated. It was originally written for gecko to provide some
detailed run stats to a process with little effort on each user programmme. 
Trying to port it to verious linux, max, and BSD environs has proved to be 
nearly impossible.  I don't believe that any ningaui programme is using this
so the fact that it does nothing is harmless, though we will continue to 
generate a warning message to stderr as an indication that there was an attempt
to use it. 


***************************************************************************
*
*  Mnemonic: ng_stats
*  Abstract: This routine will provide a single interface to generalized
*            statistics logging.
*
 	at the moment we cannot support on mac  or windows or opterons
	and on solaris the /procfs is off limits with large file support -- go figure?
*
*  Date:     4 October 1997
*  Author:   E. Scott Daniels
*
********************** SCD AT END *****************************************
*/

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

void ng_stats( int action, ...)
{
	fprintf( stderr, "warning: attempt to use ng_stats() (potoroo/lib) which has been deprecated; no action taken\n" );
}

#ifdef REVERT_TO_ORIGINAL_CODE
/* better to rewrite this completely than to revert; you have been warned */
/* there are ast vsnprintf() issues on opteron */

#include <unistd.h>
#include <stdarg.h>     /* required for variable arg processing --  include EARLY to prevent ast stdio related issues! */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
/*#include <sys/fault.h>*/
#include <sys/syscall.h>

#ifdef	OS_SOLARIS
#include	<procfs.h>
#else
#include	<sys/procfs.h>
#endif

#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include	<ast_common.h>
#include <sfio.h>
#include <ningaui.h>
#inluce <ng_lib.h>
#include <ng_ext.h>

#if	!defined(OS_SOLARIS) && !defined(OS_IRIX)
typedef struct
{
	unsigned long	pu_minf;
}prusage_t;
#endif

static ng_hrtime hrt_start;    /* start time stamp */
static ng_hrtime hrt_end = - 1;

#ifdef OS_SOLARIS
static void showtime( char *buf, timestruc_t *t, char *s )
 {
  char wbuf[100];           /* work buffer */

  sprintf( wbuf, "%s%ld.%.3d ", s, t->tv_sec, t->tv_nsec/1000000 );
  strcat( buf, wbuf );
 }
#endif

#ifdef OS_IRIX
static void showtime( char *buf, timespec_t *t, char *s )
 {
  char wbuf[100];           /* work buffer */

  sprintf( wbuf, "%s%ld.%.3d ", s, t->tv_sec, t->tv_nsec/1000000 );
  strcat( buf, wbuf );
 }
#endif

static void showval( char *buf, unsigned long v, char *s )
 {
  char wbuf[100];           /* work buffer */

  sprintf( wbuf, "%s%lu ",  s, v );
  strcat( buf, wbuf );
 }

static void bld_msg( char *buf, prusage_t *pptr, int error )
 {
  if( error )
   sprintf( buf, "[stats error(%d) - no data available]", error );
  else
   {
    sprintf( buf, "[r=%.3f ",  (double) ((double)(hrt_end-hrt_start)*hrtime_scale) );
#ifdef	OS_SOLARIS
    showtime( buf, &pptr->pr_stime, "s="  );
    showtime( buf, &pptr->pr_utime, "u="  );
    showtime( buf, &pptr->pr_ttime, "tr="  );
    showtime( buf, &pptr->pr_tftime, "tp="  );
    showtime( buf, &pptr->pr_dftime, "dp="  );
    showtime( buf, &pptr->pr_kftime, "kp="  );
    showtime( buf, &pptr->pr_ltime, "lo="  );
    showtime( buf, &pptr->pr_slptime, "sl="  );
    showtime( buf, &pptr->pr_wtime, "wa="  );
    showtime( buf, &pptr->pr_stoptime, "st="  );
    showval(  buf, pptr->pr_minf, "mi="  );
    showval(  buf, pptr->pr_majf, "ma="  );
    showval(  buf, pptr->pr_nswap, "sw="  );
    showval(  buf, pptr->pr_inblk, "in="  );
    showval(  buf, pptr->pr_oublk, "ou="  );
    showval(  buf, pptr->pr_msnd, "ms="  );
    showval(  buf, pptr->pr_mrcv, "mr="  );
    showval(  buf, pptr->pr_sigs, "si="  );
    showval(  buf, pptr->pr_vctx, "vc="  );
    showval(  buf, pptr->pr_ictx, "ic="  );
    showval(  buf, pptr->pr_sysc, "sy="  );
    showval(  buf, pptr->pr_ioch, "io="  );
    showval(  buf, pptr->pr_count, "th="  );
    showtime( buf, &pptr->pr_rtime, "xr=" );
#endif
#ifdef	OS_IRIX
    showtime( buf, &pptr->pu_stime, "s="  );
    showtime( buf, &pptr->pu_utime, "u="  );
    showval(  buf, pptr->pu_minf, "mi="  );
    showval(  buf, pptr->pu_majf, "ma="  );
    showval(  buf, pptr->pu_inblock, "in="  );
    showval(  buf, pptr->pu_oublock, "ou="  );
    showval(  buf, pptr->pu_sigs, "si="  );
    showval(  buf, pptr->pu_vctx, "vc="  );
    showval(  buf, pptr->pu_ictx, "ic="  );
    showval(  buf, pptr->pu_sysc, "sy="  );
    showval(  buf, pptr->pu_size, "sz="  );
    showval(  buf, pptr->pu_rss, "rss="  );
#endif
#ifdef	OS_LINUX
    showval(  buf, pptr->pu_minf, "mi="  );
#endif

    strcat( buf, "]" );
   }
 }

void ng_stats( int action, ...)
{
 static int flags = 0;
 static int perror = 0;           /* error doing proc - abort further attempts */
 static prusage_t pdata;      /* stuff from ioctl - static for consistancy with original version */
 
 va_list argp;                /* pointer to variable arguments */
 char *fmt;                   /* pointer to sprintf format string */
 char smsg[1024];             /* spot to build our message in */
 char umsg[2048];             /* spot to build user message in */
 char pname[100];             /* file name of /proc file */
#ifdef	OS_SOLARIS
 long ctl[2];                 /* for proc ctl */
#endif
 int pfd;                /* file des for /proc */

 switch( action )
  {
	case STATS_START:                 /* start tracking pioc info */
	flags = STATS_START;

		if( perror )
			return;
		hrt_start = ng_hrtimer( );       /* get timestamp of starting */

#ifdef OS_SOLARIS
       sprintf( pname, "/proc/%d/ctl", getpid( ) );
       if( (pfd = open( pname, O_WRONLY )) >= 0 )		/* open in write mode so we can start tracking */
	{
		ctl[0] = PCSET; ctl[1] = PR_MSACCT|PR_MSFORK;
		if( write(pfd, ctl, sizeof(ctl)) != sizeof(ctl) )     /* start pioc tracing */
          	{
           		perror = 1;
           		ng_log(LOG_ERR, "stats: get usage error at start: %s", strerror( errno ));
          	}
		close( pfd );
	}
#endif
#ifdef OS_IRIX
	sprintf( pname, "/proc/%d", getpid( ) );
	if( (pfd = open( pname, O_RDONLY )) >= 0 )
	{
		if( ioctl( pfd, PIOCUSAGE, &pdata ) < 0 )     /* start pioc tracing */
		{
           		perror = 1;
           		ng_log(LOG_ERR, "stats: get usage error at start: %s", strerror( errno ));
		}
		close( pfd );
	}
#endif

	perror = 1;			/* silently do not trace */
	return;
     	break;

	case STATS_END:                   /* record ending info */
		if( perror )
			return;
		if( ! flags )                   /* must have called start first */
		{
			perror = 4;
			return;
		}
		
		flags = STATS_END;
#ifdef	OS_SOLARIS
		sprintf( pname, "/proc/%d/usage", getpid( ) );
		if( (pfd = open( pname, O_RDONLY )) >= 0 )
		{
			if( read(pfd, &pdata, sizeof(pdata)) != sizeof(pdata) )     /* read usage */
			{
				perror = 1;
				ng_log(LOG_ERR, "stats: ctl read error at end: %s", strerror( errno ));
			}
			close(pfd);
		}
		else
		{
			perror = 2;
			ng_log(LOG_ERR, "stats: cant open: %s: %s", pname, strerror( errno ));
		}
#endif
#ifdef	OS_IRIX
		sprintf( pname, "/proc/%d", getpid( ) );
		if(( (pfd = open( pname, O_RDONLY )) < 0 ) || ( ioctl( pfd, PIOCUSAGE, &pdata ) < 0 )) /* get current */
		{
			perror = 1;
			ng_log(LOG_ERR, "stats: ioctl error at end: %s", strerror( errno ));
		}
		else
			close(pfd);
#endif

		hrt_end = ng_hrtimer( );       /* get ending timestamp */
		break;

	case STATS_WRITE:
	case STATS_STDERR:
		if( flags )                /* if on a system that gathers stats; and start was called */
		{
			if( flags == STATS_START )      /* start stats captured, but no ending */
				ng_stats( STATS_END );         /* recurse to get them */

			bld_msg( smsg, &pdata, perror );   /* build stats portion of message */
		}
		else
			bld_msg( smsg, &pdata, 99 );   		/* dummy stats message with expected format */

		va_start( argp, action );       /* @ first var ptr (format string ptr) */
		fmt = va_arg( argp, char *);    /* pickup format string */
		if( fmt )
			vsprintf( umsg, fmt, argp );    /* format user's message if there */
		else
			*umsg = 0;                      /* no user message - add end of string */

		ng_log( LOG_INFO + 1, "%s %s",umsg ? umsg : "", smsg );

		if( action == STATS_STDERR )     /* echo to standard error too? */
			sfprintf( sfstderr, "%s: %s %s\n", argv0, umsg ? umsg : "", smsg );

		flags = STATS_START;  
		va_end( argp );                  /* cleanup variable arg stuff */
		break;

		default:                          /* assumed to be STATS_WRITE */
			break;
	}
}                 /* ng_stats */
#endif

#ifdef SELF_TEST
int main( int argc, char **argv )
{
	ng_stats( 1 ); 
	ng_bleat( 0, "sleeping 5" );
	sleep( 5 );
	ng_bleat( 0, "printing stats to stderr" );
	ng_stats( 2, "%s", "hello world from the stat-o-rama!" );
}

#endif


/*
----------------- Self Contained Documentation ----------------------------
&scd_start
&title  ng_stats - Record process statistics
&name &utitle

&synop
        #include <ningaui.h>
&break  void ng_stats( int action )
&break  void ng_stats( int action, char *format_string, ^... );

&desc   &ital(ng_stats(^)) provides a common method for Gecko
        processes to log statistics information. 
	The function is used to start micro accounting, and to format 
	and write the data to the master log. 
        The calling routine can optionally add a user formatted message 
	to the message written to the log file.

&space
&subcat	Statistics Generated
	Statistics are generated in a log message in the form of a two 
	character identifier and its cooresponding value separated by 
	an equal(=) sign.
	The values for real, system and user time are specified by 
	a single chararacter identifier to be consistant with earlier
	versions of this funciton. 
	All values are enclosed in a single pair of square brackets ([])
	and are placed following any user message this is passed to the
	function. 
	All time oriented values are presented in seconds. 
	The following lists the data captured along with the identifier
	used in the message:
&space
&smterms
&term	r=  : Real time used by the process (elapsed time).
&term   s=  : Time spent in system mode.
&term   u=  : Time spent in user mode.
&term   tr= : Other system trap CPU time.
&term   tp= : Text page fault sleep time.
&term   db= : Data page fault sleep time.
&term   kp= : Kernel page fault sleep time.
&term   lo= : User lock wait sleep time.
&term   sl= : Other user sleep time (includes I/O wait).   
&term   wa= : Wait-cpu (latency) time.
&term   st= : Stopped time.
&term   mi= : Minor page faults.
&term   ma= : Major page faults.
&term   sw= : Swaps.
&term   in= : Input block count.
&term   ou= : Output block count.
&term   ms= : Messages sent.
&term   mr= : Messages received.
&term   si= : Signals received.
&term   vc= : Voluntary contect switches.
&term   ic= : Involuntary context switches.
&term   sy= : System Calls.
&term   io= : I/O character count (reads and writes).
&endterms

&parms  &ital(ng_stats(^)) requires one parameter, &ital(action,) on all calls,
        and two parameters when the action code is set to STATS_WRITE.
        The following list describes the parameters.

&space
&begterms
&term   action : An integer constant indicating the action that is to
        be preformed by the function.

&space
&term   format_string : A pointer to a character string containing the
        printf style format information necessary to format and include
        a user portion of the status message. This parameter is required
        only when the action code is STATS_WRITE, and should be NULL if
        no user output is to be formatted.

&endterms
&space

&subcat Action Codes
        Three action codes are supported by &ital(ng_stats(^)). The following
        list the action code constants that are supported.

&space
&begterms

&term   STATS_START : When the STATS_START action code is passed to
        &ital(ng_stats(^)) the current real, system, and user times are 
	recorded as the starting statistics for the processing window.

&space
&term   STATS_END : The STATS_END action code causes the current real, system,
        and user times to be recorded as the ending times of a statics window.

&space
&term   STATS_WRITE : The action code STATS_WRITE causes the &ital(ng_stats(^))
        funciton to write a statistics message to the Gecko master log file.
        The message will contain the name of the process (assumed to be in
        the global variable &ital(argv0), a user message, and the difference,
        in seconds, between the times captured during the STATS_START and
        STATS_END calls to &ital(ng_stats(^)). If a call to record ending
        times has not been made when the STATS_WRITE call is issued, the
        current times will be recorded and used as the ending times; no
        call to &ital(ng_stats(^)) with the STATS_END action code is actually
        necessary.

&space
&term	STATS_STDERR : This action code causes the same action as the 
	&lit(STATS_WRITE) action code, and will also echo the message 
	that is written to the Gecko master log to the stderr file.
&endterms

&space
&subcat The Format String
        When the action code is STATS_WRITE, the second parameter is assumed
        to contain a pointer to a &ital(printf(^)) style format string, or
        a NULL pointer.  If the pointer is not NULL, then it and the
        remaining parameters passed to &ital(ng_stats(^)) are used to format
        the user portion of the message written to the Gecko master log.


&errors &ital(ng_stats(^)) does not generate any errors directly, however the
        user should exercise caution when passing a format string and
        parameters to be placed into the user portion of the statistics
        message. If the number of parameters indicated in the format string
        are not present in the argument list, then unexpected results may
        occur.
&space
	&ital(Gk_stats( STATS_WRITE)) &stress(must) be called as early in 
	the processing as possible to provide accurate results.
	If the &lit(STATS_WRITE) action is not invoked prior to calling 
	the function with either the write or end actions, then no 
	statistics can be generated, and an error message is written 
	between the square brackets. 

&examp  The following code segment illustrates how &ital(ng_stats(^)) can
        be used to generate statistics messages in the master log file.

.nf
        int rin = 0;
        int rout = 0;
                 :
        ng_stats( STATS_START );      / * get starting time * /
                 :
                 :
        ng_stats( STATS_WRITE, "Phase 1: Recs=%d", rin );
                 :
                 :
        ng_stats( STATS_WRITE, "Phase 2: Recs=%d, rout );
                 :
                 :
.fo

&files  /ningaui/site/log/master

&see    proc(1), proc(4)

&mods
&owner(Scott Daniels)
&lgterms
&term	05 Nov 1997 (sd) : To convert for ng_log changes
&term   16 Jul 1998 (sd) : To use PIOCUSAGE from ioctl to gather a boatload
	of stats.
&term	05 Apr 2004 (sd) : Converted the write case to generate even if the 
	stats are not available. 
&term 	13 Sep 2004 (sd) : To ensure stdarg.h is included before stdio.h.
&term	07 Jun 2007 (sd) : Added CPU_686 to a defined test. 
&endterms
&scd_end
*/
