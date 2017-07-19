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

#include <string.h>
#include	<signal.h>
#include	<stdlib.h>
#include <sfio.h>


#ifdef OS_IRIX
#include        <sys/types.h>
#include        <sys/statfs.h>
#endif

#include "ningaui.h"
#include "ng_ext.h"

#if	defined (OS_LINUX) || defined (OS_SOLARIS) || defined( OS_CYGWIN )
#include "sys/statvfs.h"
#else
#include "sys/param.h"
#include "sys/mount.h"
#include "sys/stat.h"
#endif

#include	"sumfs.man"

static void proc1(char *path, double *t, double *a);

int
main(int argc, char **argv)
{
	int i;
	int dscnt = 0;
	double t, a;
	char fs[NG_BUFFER];

	ARGBEGIN{
	case 'd':	IARG(dscnt); break;
	case 'v':	OPT_INC(verbose); break;
	default:
	usage:		sfprintf(sfstderr, "Usage: %s [-v] [-d discount] [path ...]\n", argv0);
			exit(1);
	}ARGEND

	if((dscnt < 0) || (dscnt > 100)){
		sfprintf(sfstderr, "%s: discount must be in range 0..100\n", argv0);
		exit(1);
	}
	dscnt = 100-dscnt;
	t = a = 0;
	sfprintf(sfstdout, "fslist=");
	if(argc > 0){
		for(i = 0; i < argc; i++)
			proc1(argv[i], &t, &a);
	} else {
		ng_spaceman_fslist(fs, sizeof fs);
		for(i = 0; fs[i]; ){
			proc1(fs+i, &t, &a);
			while(fs[i++] != 0)
				;
		}
	}
	a *= dscnt/100.;
	sfprintf(sfstdout, "\ntotspace=%.0f freespace=%.0f\n", t, a);

	exit(0);
}

/* linux has a wimpy statfs and so it must be different */
#if	defined(OS_LINUX) || defined(OS_SOLARIS) || defined( OS_CYGWIN )
#define STAT statvfs
#else
#define STAT statfs
#endif

#ifdef OS_IRIX
#define GET_STATS(a,b)  STAT(a,b,sizeof(struct STAT),0)
#define BAVAIL  f_bfree
#else
#define GET_STATS(a,b)  STAT(a,b)
#define BAVAIL  f_bavail
#endif

static void
proc1(char *path, double *t, double *a)
{
	int k;
	struct STAT info;
	double tot, avail, scale;

	if( GET_STATS(path, &info) < 0 ){
		perror(path); 
		exit(1);
	}
	tot = info.f_blocks;
	avail = info.BAVAIL;
	scale = info.f_bsize;
#if defined(OS_LINUX) || defined (OS_IRIX) || defined(OS_SOLARIS)
	if(info.f_frsize)
		scale = info.f_frsize;
#endif
	scale /= 1024.*1024;
	tot *= scale;
	avail *= scale;
	k = 1;
	k = ng_spaceman_hldcount(path);
	sfprintf(sfstdout, "%s!%d!%.0f!", path, k, avail);
	*t += tot;
	*a += avail;
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_sumfs:Replication manager filesystem summary)

&space
&synop	sumfs [-v] [-d discount] [path...]


&space
&desc	&This generates, to the standard output device, 
	 a summary of replication manager filesystem space
	for the node on which the command is run.  The output is a record containing 
	triples of filesystem name, number of high level directories, and the amount of 
	free space in Kbytes. 
	The fields are separated with bang (!) characters.
	The record of tripples is followed by a record containing the total 
	space and free space in these filesystems.  See &ital(ng_rm_df) to get a 
	summary of replication manager space across an entire cluster.
&space
	Only the fileystems that are listed in &lit(NG_ROOT/site/arena) are listed.  If
	a filsystem is removed from the arena list, it will not be reported on.  
&space
&subcat The Discount
	By default &this reports on all space that is available.  It is often necessary 	
	to report the amount of free space as less than is actually free; reserving a 
	cushion of space depending on node type.  The &lit(ng_site_prep) script will 
	possibly set a pinkpages variable, &lit(SUM_FS_DISCOUNT,)  that is used when 
	reporting freespace to replication manager for a node.  The discount value is 
	passed to &this as a command line option (-d) and is assumed to be a 
	percentage of the free space that is NOT reported.  Thus a discount of 100, 
	will cause &this to report 0 Kbytes free, while a discount of 45 will cause the 
	freespace reported to be 55% of the actual value. 
&space
	Tools that use &this to calculate disk usage information for replication manager
	filesystems should NOT supply a discount value.

&space
&opts	The following options are recognised by &ital(ng_log):
&begterms
&term 	-d discount : Applies the discount amount which reduces the amount of total space
	reported. This allows a cushion of space to be maintained on the node. 

&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&space
&exit	When the binary completes, the exit code returned to the caller 
	is set to zero (0) if the number of parameters passed in were 
	accurate, and to one (1) if an insufficent number of parameters
	were passed. 

&space
&see	ng_rmreq(lib)  ng_repmgr   ng_rm_df

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	03 Aug 2001 (agh) : Original code 
&term	02 Apr 2003 (sd) : Mods to port to BSD and mac.
&term	21 Jul 2003 (sd) : added stuff to support irix port.
&term	27 Aug 2003 (sd) : added ifdefs to support sun port.
&term	16 Feb 2006 (sd) : added stuff for cygwin port. 
&term	16 May 2006 (sd) : Fixed man page (not printing with --man)
&endterms
&scd_end
*/
