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
  ----------------------------------------------------------------------
  Mnemonic:	ng_sysctl
  Abstract: 	Supports a limited interface to the system control
		routines to dig various things like number of cpus
		NOTE:
			these routines are very system dependant. The 
		module is designed to successfully compile on all systems
		and at worst defined stub routines that return nulls or
		constant values.  ifdefs are used to define each funcion
		based on the system type.  there are a couple of routines
		in this module that will compile for all systems.  These 
		functions will be publicly available, and systems that should
		return legit values are indicated:
			ng_ncpu	(number of cpus) [bsd,linux,mac,sun,sgi]
			ng_loadavg (return load averages) [linux]
			ng_cp_time (return timer ticks for all cpus) [bsd,linux,mac]
			ng_cp_time_delta	(returns timer tick delta) [bsd,linux,mac]
			ng_cp_time_pctg	  (returuns delta as a percentage) [bsd,linux,mac]
  Date:		18 April 2003
  Author: 	E. Scott Daniels
  Mod:		06 May 2003 (sd) : Added cp_time and load average functions
		02 Sep 2003 (sd) : cleaned up, added linux support 
  ----------------------------------------------------------------------
*/

#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#if defined( OS_SOLARIS ) || defined(OS_CYGWIN )
	/* not there for these puppies */
#else
#include	<sys/sysctl.h>
#endif
#include	<sys/resource.h>
#include	<sys/param.h>
#include	<string.h>
#include	<errno.h>
#include	<unistd.h>
#include	<sfio.h>
#include	<ningaui.h>

#ifdef OS_DARWIN
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <stdio.h>
#endif


#define CTIME_LEN	10	/* length of our buffer */

#define USR_TICKS	0	/* position of ticks in the buffer ng_cp_time returns */
#define NICE_TICKS	1
#define SYS_TICKS	2
#define INT_TICKS	3	/* interrupt */
#define IDLE_TICKS	4

/* easier way of passing sizeof(x), x to sf routines for %I */
#define IVAR(v)		sizeof((v)), &(v) 

/* return size of memory */
#if defined(OS_FREEBSD) 
#define MEM_DEFINED	1
long ng_memsize( )
{

	int mib[2];		/* request info */
	size_t len = 0;
	long long size = 0;
	int	stat = 0;

	mib[0] = CTL_HW;
	mib[1] = HW_PHYSMEM;
	len = sizeof( size );
	stat = sysctl( mib, 2, &size, &len, NULL, 0 );

	return size;
}
#endif

/* --------------- for linux we must open and read the /proc/meminfo file looking for the MemTotal line */
#ifdef OS_LINUX
#define MEM_DEFINED 1
long ng_memsize( )
{
	static long size = 0;
	Sfio_t *f = NULL;		/* file des for the stats file */
	char	*buf;				/* buffer read from file */
	
	if( size )
		return size;

	if( (f = ng_sfopen( NULL, "/proc/meminfo", "r" )) )
	{
        	while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
        	{
                	if( strncmp( buf, "MemTotal: ", 9 ) == 0 )  
                	{
				size = atol( buf + 10 );
				sfclose( f );
				return size * 1024;			/* convert from k to bytes */
			}
                }  

		sfclose( f );
	}

	return size;
}
#endif

/* ------------------ catch undefined memsize */
#ifndef MEM_DEFINED
long ng_memsize( )
{
	ng_bleat( 0, "ng_memsize not supported on this machine; returning 0" );
	return 0;
}
#endif

/* ------------------ catch undefined memsize */
#ifndef BYTEORDER_DEFINED
int ng_byteorder( )
{
	ng_bleat( 0, "ng_byteorder not supported on this machine; returning 0" );
	return 0;
}
#endif

/* return number of cpus */
#if defined(OS_FREEBSD) || defined(OS_DARWIN)
#define NCPU_DEFINED	1
int ng_ncpu( )
{
	int mib[2];		/* request info */
	size_t len = 0;
	int ncpu = -1;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof( ncpu );
	sysctl( mib, 2, &ncpu, &len, NULL, 0 );

	return ncpu;
}
#endif

#if !defined( OS_FREEBSD ) && !defined( OS_DARWIN )
#ifdef  _SC_NPROCESSORS_ONLN                            /* solaris  and linux */
#define NCPU_DEFINED	1
int ng_ncpu( )
{
        return sysconf(_SC_NPROCESSORS_ONLN); 
}
#else
#ifdef  _SC_NPROC_ONLN                                  /* sgi */
#define NCPU_DEFINED	1
int ng_ncpu( )
{
        return sysconf(_SC_NPROC_ONLN);
}
#endif
#endif
#endif

/* ----------------- catch for systems were we cannot determine cpu */
#ifndef NCPU_DEFINED
int ng_ncpu( )
{
	ng_bleat( 0, "ng_sysctl/ng_cpu: cannot determine number of cpus; estimating" );
	return 2;
}
#endif


#ifdef OS_DARWIN

/* cp_time returns current timer tics (an array of 5: usr, nice, sys, intrrupt, idle */

/* sysctl get by name not available for darwin */
#define CP_TIME_DEFINED 1
long *ng_cp_time( )
{
	static long ticks[10];

	natural_t ncpu;					/* num cpu */
	processor_info_array_t info;			/* spce for info from system */
	mach_msg_type_number_t ninfo;			/* size of the info returned */
	processor_cpu_load_info_data_t* cpuloadinfo;	/* load info */
	int i;
	int state;
	int sstate;					/* our state order */

	kern_return_t error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &ncpu, &info, &ninfo);

	if (error) 
	{
		mach_error("host_processor_info error:", error);
		return NULL;
	}

	cpuloadinfo = (processor_cpu_load_info_data_t*) info;

	memset( ticks, 0, sizeof( ticks ) );

	for( i=0; i < ncpu; i++)
	{
		for( state=0; state < CPU_STATE_MAX; state++ )  
		{
			switch( state )				/* xlate their order to our (sysctl) order */
			{
				case 0:	sstate = 0; break;	/* usr */
				case 1: sstate = 2; break;	/* sys */
				case 2: sstate = 4; break;	/* idle */
				case 3: sstate = 1; break;	/* nice */
				default: sstate = -1; break;
			}

			if( state >= 0 )
				ticks[sstate] += cpuloadinfo[i].cpu_ticks[state];
		}
	}

	vm_deallocate(mach_task_self(), (vm_address_t)info, ninfo);
	return ticks;
}
#endif

#if defined(OS_FREEBSD) && ! defined(OS_DARWIN)
#define CP_TIME_DEFINED 1
long *ng_cp_time( )
{
	extern int errno;
	static int *mib = NULL;
	static long ctime[CTIME_LEN];
	size_t len;

	if( ! mib )
	{
		len = sizeof( int ) * 10;
		mib = (int *) ng_malloc( sizeof( int ) * 10, "ng_cp_time: mib" );
		sysctlnametomib("kern.cp_time", mib, &len); 
	}

	len = sizeof( ctime );
	if( sysctl(mib, 2, ctime, &len, NULL, 0) != -1)
		return ctime;
	
	return NULL;
}
#endif

/* --------------- for linux we must open and read the /proc/stat file looking for the cpu line */
#ifdef OS_LINUX
#define CP_TIME_DEFINED 1
long *ng_cp_time( )
{
	static Sfio_t *f = NULL;		/* file des for the stats file */
	static long ticks[10];			/* buffer to put values into for return */
	char	*buf;				/* buffer read from file */
	
	memset( ticks, 0, sizeof( ticks ) );

	if( f || (f = ng_sfopen( NULL, "/proc/stat", "r" )) )
	{
		sfseek( f, 0, SEEK_SET );				/* rewind */
        	while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
        	{
                	if( strncmp( buf, "cpu ", 4 ) == 0 )  /* tick counts are: usr nice(usr) sys idle */
                	{
                        	sfsscanf( buf+4, "%I*d %I*d %I*d %I*d", IVAR(ticks[USR_TICKS]), IVAR(ticks[NICE_TICKS]), IVAR(ticks[SYS_TICKS]), IVAR(ticks[IDLE_TICKS]) );
                        	ticks[USR_TICKS] += ticks[NICE_TICKS];
        
				while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL );     /* must read to end */
				return ticks;
			}
                }  
	}

	return NULL;
}
#endif


/* ----------------------- catch for systems that cannot support timer ticks */
#ifndef CP_TIME_DEFINED
long *ng_cp_time( )
{
	return NULL;
}
#endif

/* ------------- these should work for all system given that at a minimum the stubs above are defined */
/* return the delta of cp time since the last call 
	normally the first call would result in garbage, so we return NULL the first time 
*/
long *ng_cp_time_delta( )
{
	static long ptime[CTIME_LEN];		/* previous values */
	static long delta[CTIME_LEN];		/* delta to return */
	static int okval = 0;
	int i;
	long *cv;				/* current values */

	if( (cv = ng_cp_time( )) )
	{
		for( i = 0; i < 5; i++ )
		{
			delta[i] = cv[i] - ptime[i];
			ptime[i] = cv[i];
		}
	}
	else
		return NULL;

	if( okval )
		return delta;
	
	okval = 1;
	return NULL;	
}

/* return an array of cpu usage percentages (usr, sys, idle) */
double *ng_cp_time_pctg()
{
	static double rslt[3];		/* results */
	long tot;			/* total ticks returned by delta */
	long *ticks;			/* number of ticks since last call */
	int i;

	if( (ticks = ng_cp_time_delta( )) != NULL )
	{
		ticks[2] += ticks[3];		/* interrupt counts as sys */
		ticks[0] += ticks[1];		/* nice counts as user */
		tot = 0;
		for( i = 0; i < 5; i++ )
			tot += ticks[i];
		rslt[0] = (double)(ticks[0] * 100)/(double)tot;		/* usr */
		rslt[1] = (double)(ticks[2] * 100)/(double)tot;		/* sys */
		rslt[2] = (double)(ticks[4] * 100)/(double)tot;		/* idle */
	}
	else
		rslt[0] = rslt[1] = rslt[2] = 0;
	
	return rslt;
}

/* return pointer to an array of double, 3 in length, containing the 
   standard 1,5,15 minute values
*/
#if defined(OS_FREEBSD) && ! defined(OS_DARWIN)
#define LOADAVG_DEFINED 1
double *ng_loadavg( )
{
	extern int errno;

	static int *mib = NULL;
	static double la[3];

	struct loadavg cv;		/* current values */
        size_t len;

        if( ! mib )
        {
                len = 10;
                mib = (int *) ng_malloc( sizeof( int ) * 10, "ng_cp_time: mib" );
                sysctlnametomib("vm.loadavg", mib, &len);
        }

	len = sizeof( cv );
	if( sysctl(mib, 2, &cv, &len, NULL, 0) != -1)
	{
		la[0] = (double)cv.ldavg[0]/(double)cv.fscale;
		la[1] = (double)cv.ldavg[1]/(double)cv.fscale;
		la[2] = (double)cv.ldavg[2]/(double)cv.fscale;
		return la;
	}
	else
		sfprintf( sfstderr, "error getting load average: %s\n", strerror( errno ) );

	return NULL;
}
#endif

/* -------------- linux we must open /proc/loadavg and read. cannot rewind it */
#ifdef OS_LINUX
#define LOADAVG_DEFINED 1
double *ng_loadavg( )
{
	static	double la[3];
	Sfio_t	*laf = NULL;
	char	*load = NULL;		/* buffer from load average file */

	la[0] = la[1] = la[2] = 0;
	if( (laf = ng_sfopen( NULL, "/proc/loadavg", "r" )) != NULL )
	{
		if( (load = sfgetr( laf, '\n', SF_STRING ))  )
			sfsscanf( load, "%I*f %I*f %I*f", IVAR(la[0]), IVAR(la[1]), IVAR(la[2]) );
		sfclose( laf );
	}

	return la;

}
#endif

#ifndef LOADAVG_DEFINED
double *ng_loadavg( )
{
	static double la[3];

	la[0] = 1;
	la[1] = 1;
	la[2] = 1;

	return la;
}	
#endif

#ifdef SELF_TEST

/* self test gets the number of cpus, and then for about 50 seconds reports 
   cpu and load average information 
*/
main( int argc, char **argv )
{
	int i;
	int j;
	long *l;
	double *p;
	double *la;

	printf( "%10s %d\n", "cpus=", ng_ncpu( ) );
	sfprintf( sfstdout, "%10s %I*d\n", "memsize=", sizeof( long ), ng_memsize( ) );
	
	ng_cp_time_delta( );		/* first call generates null */
	sleep( 1 );

	for( j = 0; j < 1000; j ++ )
	{
		if( (la = ng_loadavg( )) != NULL )
			printf( "loadave: %.2f %.2f %.2f\n", la[0], la[1], la[2] );

		if( (p = ng_cp_time_pctg( )) != NULL )
			printf( "cpu:     %5.1f%% %5.1f%% %5.1f%% (usr,sys,idle)\n", p[0], p[1], p[2] );

		sleep( 5 );
	}
}
	
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sysctl:Sysctl system interface for Ningaui)

&space
&synop
	#include <ng_ext.h> &break
	int ng_ncpu( ); 	&break
	long *ng_cp_time( ); 	&break
	long *ng_cp_time_delta( ); 	&break
	int *ng_cp_time_pctg(); 	&break
	double *ng_loadavg( ); 	&break

&space
&desc
	These functions provide various simplified interfaces to retrieve various 
	pieces of system related information. 
&space
&lgterms
&term	ng_ncpu : Returns the number of CPUs that are installed on the host.
&space
&term	ng_cp_time : Retuns an array of long integers that contain the current 
	timer ticks indicating time spent in user, nice, system, interrupt, and idle
	processing modes.  The values occurr in the order listed.
&space
&term	ng_cp_time_delta : Returns the difference of each of the timer ticks 
	returned by &lit(ng_cp_time) between the current time, and the time of 
	the last call to &lit(ng_cp_time_delta). The data is returned as an array 
	of long integers that occur in the array in the same order as values 
	returned by &lit(ng_cp_time).
	The first call to this function will return a NULL pointer as the initial
	values as there is nothing to use to calculate the delta.
&space
&term	ng_cp_time_pctg : Calculates the amount of time that has been spent in 
	user, nice, system, interrupt and idle modes as a percentage. The 
	percentage is calculated based on the use between the last call to this 
	function and the present time.  The array of integers returned are in 
	user, system, idle order. 

&space
&term	ng_cp_loadavg : This function retreives and returns the current load
	average values. The array  of three doubles is organised in the same order 
	as the information printed when the &lit(uptime) command is executed.
&space
&term
&endterm

&space
&see	sysctl(3), sysctl(1)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	18 Apr 2003 (sd) : Thin end of the wedge.
&term	06 May 2003 (sd) : Added timer and load ave stuff.
&term	02 Sep 2003 (sd) : Added support for ncpu on sun/irix.
&term	31 Jul 2005 (sd) : changes to prevent warnings from gcc.
&term	09 Apr 2007 (sd) : Changed to properly get ncpu function defined under darwin 9.x.
&endterms

&scd_end
------------------------------------------------------------------ */
