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
 ---------------------------------------------------------------------------------
  Mnemonic:	argus
  Abstract:	A small daemon that watches system level things like cpu usage.
  Date:		3 April 2002 (hbd2me)
  Author: 	E. Scott Daniels
  Mods:		Added stuff to make it work under freebsd too.
 ---------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<sys/types.h>          /* various system files - types */
#include	<sys/ioctl.h>
#include	<errno.h>              /* error number constants */
#include	<fcntl.h>              /* file control */
#include	<signal.h>             /* info for signal stuff */
#include	<string.h>
#include	<stdlib.h>
#include	<signal.h>
#ifndef OS_DARWIN
#include	<sys/resource.h>	/* needed for wait */
#endif
#include	<sys/wait.h>

#include 	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>

#include	"argus.man"

extern	int verbose;
extern	char *argv0;

Sfio_t	*lf = 0;			/* log file */

char *of_fmt = "%I*u  usr=%.1f%% sys=%.1f%% idle=%.1f%% pagein=%.0f pageout=%.0f load=%.1f %.1f %.1f\n"; 	/* default format */
char *short_format = "%I*u  %.1f %.1f %.1f %.0f %.0f %.1f %.1f %.1f\n"; 

int	log2master = 0;			/* write to master log rather than log file */

void usage( )
{
	sfprintf( sfstderr, "usage: %s [-d sec] [-f] [-l] [-s]\n", argv0 );
}


/* daemons should not be seen nor heard */
static void hide( )
{
	extern	int errno;
	static	int hidden = 0;

	char	*ng_root;
	char	wbuf[256];
	int i;
	

	if( hidden )
		return;            /* bad things happen if we try this again */

	switch( fork( ) )
	{
		case 0: 	break;        /* child continues on its mary way */
		case -1: 	perror( "could not fork" ); return; break;
		default:	exit( 0 );    /* parent abandons the child */
	}

	if( ! (ng_root = ng_env( "NG_ROOT" )) )
	{
		ng_bleat( 0, "cannot find NG_ROOT in cartulary/environment" );
		exit( 1 );
	}

	sfsprintf( wbuf, sizeof( wbuf ), "%s/adm", ng_root );
	chdir( wbuf );

	sfclose( sfstdin );		/* odd things happen if we forget and leave sf flavours open */
	sfclose( sfstderr );			
	sfclose( sfstdout );
	for( i = 3; i < 256; i++ )	/* close the rest */
		if( i != 2 )
			close( i );              /* dont leave caller's files open */

	hidden = 1;

	
	lf = ng_sfopen( NULL, "/dev/null", "Kw" );		/* K says kill us on first error -- an ng_sfopen extension */
	sfsetfd( lf, 2 );				/* ensure we have a stderr open */

	sfsprintf( wbuf, sizeof( wbuf ), "%s/site/log/argus", ng_root );
	if( (lf = ng_sfopen( NULL, wbuf, "a" )) == NULL )	/* when hidden we write to log if not logging to master */
	{
		ng_bleat( 0, "cannot open log file: %s: %s", wbuf, strerror( errno ) );
		exit( 1 );
	}
	
	sfset( lf, SF_LINE, 1 );		/* cause flush with each line */
	ng_free( ng_root );
}

int main( int argc, char **argv )
{
	extern int	errno;

	int	daemon = 1;			/* hides if true after initialisation */
	int	delay = 15;
	ng_timetype now;
	char	buf[NG_BUFFER];

	double	*cpu_use;			/* cpu percentages */
	double  *load_ave;			/* load average info */

	lf = sfstdout;

	ng_malloc( 5, "first alloc -- debug prime" );
	ARGBEGIN
	{
		case 'd':	IARG( delay ); break;
		case 'f':	daemon = 0; break;
		case 'l':	log2master = 1; break;
		case 's':	of_fmt = short_format; break;
		case 'v':	OPT_INC( verbose ); break;
		default:
usage:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	if( daemon )
		hide( );

	if( delay < 3 )
		delay = 3;		/* anything less might impact the system */

	while( 1 )
	{
		now = ng_now( );
		if( (load_ave = ng_loadavg( ))  && (cpu_use = ng_cp_time_pctg( )) )
		{
			if( log2master )
			{
				sfsprintf( buf, sizeof( buf ), of_fmt, sizeof( now ), now, cpu_use[0], cpu_use[1], cpu_use[2], 0.0, 0.0, load_ave[0], load_ave[1], load_ave[2] );
				ng_log( LOG_INFO, "%s", buf );
			}
			else
				sfprintf( lf, of_fmt, sizeof( now ), now, cpu_use[0], cpu_use[1], cpu_use[2], 0.0, 0.0, load_ave[0], load_ave[1], load_ave[2] );
			
		}

		sleep( delay );
	}

	return 0;			/* manditory for linux */
}




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_argus:Watch and log system statistics)

&space
&synop	ng_argus [-d sec] [-f] [-l] [-s]

&space
&desc	Looks at various system statistics and writes a summary of them every 
	&ital(sec) seonds. Stats are written to the &lit(argus) file in the 
	standard log directory unless the &lit(-l) option is given which 
	causes the stats to be written to the master log. The &lit(-s) option 
	causes a short form message to be written. Records recorded have the 
	format of:
&litblkb
	<timestamp> usr=n% sys=n% idle=n%

	or

	<timestamp> usr sys idle
&litblke
&space
	The latter format of output is generated when the short form option 
	is given. 

&space
	The values calculated are based on the statistics amassed by the kernel
	during the delay period. Thus if the delay is 5 minutes (300 sec), then 
	the statistics are based on the number of observations that the kernel
	recorded during that time. 
&space
	&This automatically places it self into daemon mode, but will remain 
	attached to the control terminal if the &lit(-f) option is supplied.

&space
&see	proc(5)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Apr 2002 (sd) : Start.
&term	04 Jul 2002 (sd) : ensure  return 0 at end of main;
&term	06 May 2003 (sd) : Added FreeBSD stuff.
&term	17 Jul 2003 (sd) : Fixed load averages on linux and bsd.
&term	02 Sep 2003 (sd) : With the enhancement of ng_sysctl functions, was able 
		to make this a fairly generic programme.  Linux specific functions 
		were removed.
&term	05 Oct 2003 (sd) : To ensure sf files closed properly at detach.
&term	07 Jun 2007 (sd) : Changed Darwin constant references to match panoptic.ksh.
&term	09 Jun 2009 (sd) : Remved reference to deprecated ng_ext.h header file.
&endterms

&scd_end
------------------------------------------------------------------ */
