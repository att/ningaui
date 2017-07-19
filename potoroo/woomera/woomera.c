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

#include	<sys/types.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/wait.h>
#include	<stdio.h>
#include	<sfio.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<signal.h>

#include	"ningaui.h"
#include	"woomera.h"

#include	"woomera.man"

extern void dumpchannel(FILE *fp);

Node	*node_r;
Stats	wstat;
Symtab	*syms;

int LONG_TIMEOUT = (3*3600);			/* how long this last before pruning */
int SHORT_TIMEOUT = (1*3600);			/* how long this last before pruning */
int	log_state = LS_STDERR;			/* log to stderr */
int	ppid = 0;				/* if we are the child, our parent pid */

void	prstats(char *msg);

/* 
	parent process registers this signal handler.  when invoked, we assume 
	the child has told us to blow off and return (good) to our caller.
*/
void handle_sig_usr1( int s )
{
	if( !ppid )
	{
		sfprintf( sfstderr, "sigusr1 received; exiting normally" );
		exit( 0 );
	}
}

void handle_sig_alarm( int s )
{
	ng_bleat( 0, "abort: timeout while waiting on socket read" );
	exit( 1 );
}

void signal_setup( int s )
{
	struct sigaction sact;        /* signal action block */

  	memset( &sact, 0, sizeof( sact ) );
	switch( s )
	{
		case SIGALRM: sact.sa_handler = &handle_sig_alarm; break;

		case SIGUSR1: sact.sa_handler = &handle_sig_usr1; break;
	
		default:
			break;
	}

     	sigaction( s, &sact, NULL );    
}

/*
	fork a child; the parent returns immediately letting the invoking process continue.
	We close stdin/out/err and reopen them to /dev/null
*/
void detach( )
{
	int	cpid = 0;
	int	cstatus = 0;

	sfprintf( sfstderr, "forking child to wait for final jobs\n" );

	switch( (cpid = fork( )) )
	{
		case 0: 	break;        		/* child continues on its mary way */
		case -1: 	perror( "could not fork" ); return; 
		default:						/* parent just waits for ever */
				signal_setup( SIGUSR1 );		/* when its time to let go we get this from child */
#ifdef OLD_CODE
        			signal( SIGUSR1, handle_sig_usr1 );		/* we get a sigusr1 from a child when it is time for us to let go */
#endif
				waitpid( cpid, &cstatus, 0 );
				ng_bleat( 0, "woomera child stopped without a signal to back to woomera parent process -- exit 1 (cstatus=%02xx)\n", cstatus );
				exit( 1 );
				break;
	}

	ppid = getppid( );
	sfprintf( sfstderr, "child successfully detached; ppid=%d\n", ppid );
}


int
main(int argc, char **argv)
{
	char *version = "v2.2/08126";
	int serv_fd;
	char *port = ng_env("NG_WOOMERA_PORT");
	char stderrbuf[BUFSIZ];
	char *sfile = 0;
	int doreq = 1;
	long nextd = 0;
	long incd = 300;

	setvbuf(stderr, stderrbuf, _IOLBF, sizeof stderrbuf);
	ARGBEGIN{
	case 'p':	SARG(port); break;
	case 'q':	LONG_TIMEOUT /= 100; SHORT_TIMEOUT /= 100; break;
	case 'r':	doreq = 0; break;
	case 's':	SARG(sfile); break;
	case 'v':	OPT_INC(verbose); break;
	default:	goto usage;
	}ARGEND
	if(argc != 0){
usage:
		fprintf(stderr, "Usage: %s [-p port] [-s startup] [-r] [-q] [-v]\n", argv0);
		exit(1);
	}

	if( ! port )
	{
		ng_bleat( 0, "abort: could not find port (NG_WOOMERA_PORT) in cartulary, not on command line" );
		exit( 1 );
	}

	if( verbose )
	{
		fprintf( stderr, "starting %s\n", version );
		fprintf(stderr, "%s: using port %s\n", argv0, port);
	}

	detach( );				/* fork a child to do all of the real work; parent waits for signal */

	signal_setup( SIGALRM );

	if((serv_fd = ng_serv_announce(port)) < 0)
		exit(1);
	syms = syminit(0);

        signal( SIGPIPE, SIG_IGN );


	node_r = new_node(N_root);
	node_r->name = ng_strdup("root node");
	jpinit();
	prstats("initialising");

	startup(sfile, doreq);

	if(verbose > 1)
		fprintf(stderr, "entering main loop\n");
	for(;;){
		(void)load(1);			/* update if necessary */
		if(nextd < time(0)){
			nextd = time(0) + incd;
			fprintf(stderr, "periodic burble\n");
			dumpchannel(stderr);
			prstats("heartbeat");
		}
		/* read as much input as we can */
		if(scan_req(serv_fd)){		/* if we can't read any more requests */
			ng_log(LOG_CRIT, "server died: %s\n", ng_perrorstr());
			if(verbose)
				fprintf(stderr, "%s: server died\n", argv0);
			break;
		}
		/* catch as much as we can */
		jpoke();
		/* fire off as much as we can */
		if(execute(node_r))
			break;
		/* prune dead wood */
		prune();
		/* pause a second */
		/*sleep(1);		sleep now done as a part of probe in scan_req */
	}

	if(verbose)
		fprintf(stderr, "%s: preparing to exit\n", argv0);

	/* cleanup */
	ng_serv_unannounce(serv_fd);		/* release our listen port */
	prstats("about to exit");

	sfprintf( sfstderr, "%d jobs running\n", wstat.running);

	if( verbose )
		sfprintf( sfstderr, "signaling closing std* files, and parent to stop\n" );
	if( wstat.running )			/* if job running, then lets wait in a bg processes so that wstart can exit if needed */
	{
		dump_still_running( stderr );		/* list of jobs still running to stderr before we close it -- before close of stderr */

		sfclose( sfstdin );		/* while we wait, our parent dies allowing wstart to restart a fresh copy */
		sfclose( sfstdout );
		sfclose( sfstderr );
		ng_open012();				/* open stdin/out/err as dev/null */
	}

	if( ppid )
		kill( ppid, SIGUSR1 );		/* safe to have parent stop now */

	log_state = LS_MASTER;			/* cause final status messages to go to master log */
	while( jcatch(1) )		/* 1 == block while jobs are still execuing */
		sleep( 1 );

	save(node_r, "jqueue");			/* jobs queued at time of shutdown */

/*
stderr closed if child process
	if(verbose)
		symstat(syms, stderr);
	if(verbose)
		fprintf(stderr, "%s: exiting normally\n", argv0);
*/
	prstats("terminating");

	exit(0);
}


void
prstats(char *msg)
{
	char buf[NG_BUFFER];
	time_t t = 23;

	sfsprintf(buf, sizeof( buf ), "%s: load %d; jobs: %d running, %d ready, %d not ready, %d done; lims: pload=%d pmax=%d",
		msg, load(0), wstat.running, wstat.inqueue, wstat.notready, wstat.completed, pload, pmax);

	ng_log(LOG_INFO, "%s\n", buf);

	t = time( NULL );

	fprintf(stderr, "%s%s\n", ctime(&t), buf);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_woomera -- batch job execution manager
&name   &utitle

&synop  ng_woomera [-v] [-p &ital(port)]

&desc	&ital(Gk_woomera) accepts various requests (described below) via
socket calls to a well-known port, most commonly via &ital(ng_wreq).
&ital(Gk_woomera) manages jobs.
Jobs have names (assigned by the user or by &ital(ng_woomera)
itself), a priority, scheduling information, and a string &ital(str)
executed as &lit(ksh -c 'str').
Scheduling occurs through the following mechanisms (in descreasing order of importance):
&beglist
&item	a partial ordering is set by the &ital(after) clause.
&item	jobs with a (numerically) higher priority have precedence
&item	a partial ordering is set by the ordering of the jobs in the input file.
&item	restrictions imposed by process or resource limits.
&endlist
&space
The &ital(after)
clause specifies that something can happen after some point in time.
The point in time is specified by either a job id, which means when that job terminates,
or a resource id, which means when all jobs specifying that resource id have
run and terminated.
&space
The directives fall into three classes:
specifiying and scheduling jobs,
imposing various resource constraints,
and managing &ital(ng_woomera) itself. The directives shall be given one per line;
newlines preceded by a backslash are ignored.
&space
&beglist
&item	&lit(job) [&ital(jid)] &lit(^:) [&ital(rid) ...] [&lit(pri) &ital(p)] [&ital(after)] [&lit(cmd) &ital(command-string)]
&break
This defines a job with an optional name, optional resources consumed by the job,
an optional priority, optional &ital(after)
clauses, and an optional command string which starts as the first non-whitespace character after
&lit(cmd) and continues to the end-of-line.
&space
&item	pause [&ital(after)] [&ital(rid) ...]
&break
At the given time (now by default), suspend the sheduling of jobs
involving the given resource ids (by default, &lit(All)).
&space
&item	resume [&ital(after)] [&ital(rid) ...]
&break
At the given time (now by default), resume the sheduling of jobs
involving the given resource ids (by default, &lit(All)).
&space
&item	run [&ital(after)] &ital(jid) ...
&break
At the given time (now by default), execute the given jobs regardless of any constraints.
&space
&item	purge &ital(id) ...
&space
Remove the specified jobs.
Any such jobs currently executing are allowed to terminate.
Jobs are specified by either job id or resource id.
&endlist
&space
All constraints apply to the scheduling of jobs; any changes made do not affect jobs currently executing.
The constraints are:
&beglist
&item	limit rid n [&ital(after)]
After the given time (now by default), limit the number of executing jobs
specifying rid to be at most n.
&space
&item	&lit(pmin) &ital(n) [&ital(after)]
&break
After the given time (now by default), cause the number of executing jobs to be at least n.
If n is negative, remove the minimum constraint.
&space
&item	&lit(pmax) &ital(n) [&ital(after)]
&break
After the given time (now by default), cause the number of executing jobs to be at most n.
If n is negative, remove the maximum constraint.
&space
&item	&lit(pload) &ital(n) [&ital(after)]
&break
After the given time (now by default), adjust the number of executing jobs
 such that the system load is at most n.
If n is negative, remove the load constraint.
&space
&item	&lit(after) &ital(id) ... &lit(,)
&item	&lit(after anystatus) &ital(id) ... &lit(,)
&break
This is not a directive but a clause that can be part of most directives.
The id's are either job ids or resource ids.
Normally, only successful job terminations count;
however, with the second form, specifying &lit(anystatus),
the termination status is ignored.
&endlist
&space
Note that the pmin, pmax and pload constraints are NOT mutually exclusive;
for example, all three could apply at the same time.
Specifying one of these constraints does not alter either of the other two.
The constraints can be turned off by giving a negative argument.
&space
Management:
&beglist
&item	&lit(listr) [&ital(after)]
&break
After the given time (now by default), list the jobs currently running.
&space
&item	&lit(lists) [&ital(after)]
&break
After the given time (now by default), list the jobs not yet scheduled to run.
&space
&item	&lit(list) [&ital(after)]
&break
After the given time (now by default), list all jobs (equivalent to listr followed by lists).
&space
&item	&lit(explain) [&ital(id) ...]
&break
Describe scheduling factors concerning the specified jobs
(id is either job id or a resource id).
&space
&item	&lit(verbose) &ital(lev) [&ital(after)]
&break
After the given time (now by default), set the verbose level to &ital(lev).
&space
&item	&lit(exit) [&ital(after)]
&break
After the given time (now by default), cause &ital(ng_woomera)
to exit after currently running jobs terminate.
No more jobs will be started.
&endlist
&space
Apart not using keywords (such as &lit(pmax)),
an id can be any C identifier with the following restrictions:
all job names must start with a lowercase letter, and all resource ids
must start with an upper case letter.

&opts   &ital(Gk_woomera) unserstands the following options:

&begterms
&term 	-p &ital(port) : accept requests on port &ital(port).

&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	The program will return with a code of zero if it successfully
parsed all EMI records properly. An exit code of 1 is 
generated if the program x, y, or z.

&errors	The &ital(ng_emi_prs) program will write the following messages to the 
Gecko master log. 
&space
&beglist
&item	dunno
&endlist

&mods
&owner(Andrew Hume)
&lgterms
&term	08 Apr 2003 (sd) : Changes sigignore call to signal call to support BSD.
&term	16 Sep 2004 (sd) : Needed include <time.h> for 64 bit sanity.
&term	08 Feb 2005 (sd) : Added ability to terminate immediately while still waiting for
	running jobs to finish.  After termination, status information for the jobs
	running at termination time are logged to the master log rather than to 
	standard error. (v2.1)
&term	09 Oct 2005 (sd) : The ndel() funciton now deletes the symtab entry if it existed.
		Should prevent coredumps at odd times. 	Added string to match with 
		a purged status when explain invoked.
&term	20 Feb 2006 (sd) : added a (longish) timeout which kills the process when a connection 
		hangs.  
&term	12 Aug 2006 (sd) : Explain job now emits pid as the last token (things like shirlock need
		this info and we do not want to depend on verbose on in the log).  Also addressed
		an issue on Darwin: parent and child both writing to stderr caused issues. 
&endterms
&scd_end
*/
