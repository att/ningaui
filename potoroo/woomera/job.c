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
  Mods:	08 Apr 2003 (sd) : added freebsd style setpgrp call
	01 Aug 2005 (sd) : changed dup2() calls to have parms in right order. 
	08 Apr 2008 (sd) : added check for darwin 9.x as the pgrp syscall parms are different.
	10 Apr 2008 (sd) : added include of time.h for build on darwin 9.x
*/

#include	<sys/types.h>
#include	<sys/wait.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<signal.h>
#include	<time.h>

#include	"ningaui.h"
#include	"woomera.h"

/*
	job scheduling stuff.

	the heart of this is that execute() handles the ordering of jobs
	and adds them to a runqueue via jqueue(). once there, jcanrun()
	implements all scheduling restrictions.
*/

#define	LOAD_INTERVAL	10		/* seconds between looking up load */

#define	MAXPID		30000		/* from posix */

static Node	**jobs = 0;
static int	njobs;
static int	ajobs = 0;
static int	nrunning;
static int	pmin = 0;
	int	pmax = MAX_LIMIT;
	int	pload = MAX_LIMIT;

static int bmax, bload;
static void	jrun(void);
static int	jexec(Node *n);
static Node	*pid_node(int pid);
static void	set_pid_node(int pid, Node *n);

void
jadd(Node *n)
{
	Syment *s;
	Node *nn;

	if(verbose)
		fprintf(stderr, "adding job %s (pri=%d cmd=%s)\n", n->name, n->pri, (n->cmd?n->cmd:"<none>"));
	pop_rsrc(n);
	/*s = symlook(syms, n->name, S_JOB, n, SYM_NOFLAGS);*/
	s = symlook(syms, n->name, S_JOB, n, SYM_COPYNM);			/* save name */
	if(s->value == n){
		/* not already there, so add it to the root */
		add_prereq(node_r, n, 1);
		wstat.notready++;
	} else {
		nn = s->value;
		if((nn->status == J_not_ready) || (nn->status == J_running)){
			fprintf(stderr, "can't overload job %s: status=%d\n", n->name, nn->status);
			ng_log(LOG_ERR, "can't overload job %s: status=%d", n->name, nn->status);
			return;
		}
		/* copy in place */
		nset(nn, n);
	}
}

void
jforce(char *jname, int promote)
{
	Syment *s;
	Node *n;

	s = symlook(syms, jname, S_JOB, 0, SYM_NOFLAGS);
	if((s == 0) || (s->value == 0)){
		goto nojob;
	} else {
		n = s->value;
		switch(n->status)
		{
		case J_unknown:
	nojob:
			if(verbose)
				fprintf(stderr, "forcing job %s: no such job\n", jname);
			printf("forcing job %s: no such job\n", jname);
			break;
		case J_not_ready:
			if(verbose)
				fprintf(stderr, "forcing job %s\n", jname);
			n->ready = 1;
			n->force = 1;
			jqueue(n, promote);
			break;
		case J_running:
			if(verbose)
				fprintf(stderr, "forcing job %s: already running\n", jname);
			printf("forcing job %s: already running\n", jname);
			break;
		case J_finished_okay:
		case J_finished_bad:
			if(verbose)
				fprintf(stderr, "forcing job %s: already finished\n", jname);
			printf("forcing job %s: already finished\n", jname);
			break;
		}
	}
}

void
jpurge(char *jname)
{
	Syment *s;
	Node *n;

	s = symlook(syms, jname, S_JOB, 0, SYM_NOFLAGS);
	if((s == 0) || (s->value == 0)){
		goto nojob;
	} else {
		n = s->value;
		switch(n->status)
		{
		case J_unknown:
	nojob:
			if(verbose)
				fprintf(stderr, "purging job %s: no such job\n", jname);
			printf("purging job %s: no such job\n", jname);
			break;
		case J_not_ready:
		case J_finished_okay:
		case J_finished_bad:
			if(verbose)
				fprintf(stderr, "purging job %s\n", jname);
			n->ready = 1;
			n->status = J_purged;
			n->expiry = time(0)+SHORT_TIMEOUT;
			jdelete(n);		/* get it out of the run queue now though */
			/* prune will pick it up */
			break;
		case J_running:
			if(verbose)
				fprintf(stderr, "purging job %s: already running\n", jname);
			printf("purging job %s: already running\n", jname);
			break;
		}
	}
}

void
jqueue(Node *n, int head)
{
	int i;

	if(n->onqueue == 0){
		wstat.notready--;
		wstat.inqueue++;
		inc_rsrc(n, 0);
		n->onqueue = 1;
	}
	if(verbose > 1)
		fprintf(stderr, "queueing job %s [head=%d]\n", NAME(n), head);
	if(njobs >= ajobs){
		ajobs = (ajobs)? 2*ajobs:128;
		jobs = ng_realloc(jobs, ajobs*sizeof(*jobs), "job queue");
	}
	/* insert by priority */
	if(head)
		i = 0;
	else {
		for(i = 0; i < njobs; i++){
			if(n->pri > jobs[i]->pri)
				break;
		}
	}
	/* insert before i */
	if(i != njobs)
		memmove(&jobs[i+1], &jobs[i], (njobs-i)*sizeof(jobs[0]));
	jobs[i] = n;
	if(verbose > 2)
		fprintf(stderr, "inserting job %s at position %d/%d\n", n->name, i, njobs);
	njobs++;
	jrun();
}

void
jdelete(Node *n)
{
	int i, k;

	k = 0;
	for(i = 0; i < njobs; i++)
		if(jobs[i] != n)
			jobs[k++] = jobs[i];
	if(njobs != k){
		njobs = k;
		wstat.inqueue--;
		n->onqueue = 0;
	} else
		wstat.notready--;
}

void
jpoke(void)
{
	while(jcatch(0))
		;
	jrun();
}

static int
jcanrun(Node *n)
{
	int ret, i;
	Resource *r;

	if(verbose > 1)
		fprintf(stderr, "canrun(%s):", NAME(n));
	/* first do proc constraints */
	if(nrunning >= pmax){
		ret = 0;
		bmax++;
		if(verbose > 1)
			fprintf(stderr, " no: nrun(%d) >= pmax(%d)\n", nrunning, pmax);
	} else if(nrunning < pmin){
		ret = 1;
		if(verbose > 1)
			fprintf(stderr, " maybe: nrun(%d) < pmin(%d);", nrunning, pmin);
	} else if(load(1) >= pload){
		ret = 0;
		bload++;
		if(verbose > 1)
			fprintf(stderr, " no: load(%d) >= pload(%d)\n", load(1), pload);
	} else
		ret = 1;
	if(ret == 0)
		return ret;

	/* now do resource constraints */
	for(i = 0; i < n->nrsrcs; i++){
		r = n->rsrcs[i];
		if(r->paused){
			ret = 0;
			r->blocked++;
			if(verbose > 1)
				fprintf(stderr, " no: resource %s: paused\n", r->name);
			break;
		}
		if(verbose > 1)
			fprintf(stderr, " resource %s: run=%d limit=%d\n", r->name, r->running, r->limit);
		if(r->running >= r->limit){
			r->blocked++;
			ret = 0;
			if(verbose > 1)
				fprintf(stderr, " no: resource %s: run(%d) >= limit(%d)\n", r->name, r->running, r->limit);
			break;
		}
	}
	if(ret && (verbose > 1))
		fprintf(stderr, " okay\n");
	return ret;
}

static void
initr(Syment *sym, void *arg)
{
	Resource *r = (Resource *)sym->value;

	r->blocked = 0;
}

static void
jrun(void)
{
	int i, k;
	Node *n;

	symtraverse(syms, S_RESOURCE, initr, 0);
	for(i = 0; i < njobs; i++){
		if(jcanrun(n = jobs[i])){
			if(jexec(n))
				jobs[i] = 0;
		}
	}
	k = 0;
	for(i = 0; i < njobs; i++)
		if(jobs[i])
			jobs[k++] = jobs[i];
	njobs = k;
}

static int
jexec(Node *n)
{
	int pid, i, null;
	Node *nn = n;
	int outfd;

	outfd = fileno(n->out->fp);
	wstat.inqueue--;
	wstat.running++;
	n->onqueue = 0;
	if(verbose > 1)
		fprintf(stderr, "jexec '%s'\n", n->cmd);
	fflush(stderr);
	fflush(stdout);
	if((pid = fork()) < 0){
		perror("fork");
		return 0;
	}
	if(pid == 0){						/* child -- setup and exec user command */
		n->pid = getpid( );
#ifdef NOT_BROKEN_UNDER_DARWIN
		if(verbose)
			fprintf(stderr, "job %s(%s) thingy: pid=%d\n", n->name, n->cmd, n->pid );
#endif
		/* careful of output fd */
		if((outfd == fileno(stderr)) || (outfd == fileno(stdout)))
			outfd = -1;
		fclose(stderr);
		fclose(stdout);

/* bloody osx 9.x buggered the system call parms */
#if defined(OS_FREEBSD)  || defined(OS_DARWIN)
#if defined(OS_VER9)
                setpgrp();              /* prevent signals propogating back up */
#else           
                setpgrp( 0, getpid( ) );                /* prevent signals propogating back up */
#endif                  
#else                           
                setpgrp();              /* prevent signals propogating back up */
#endif                  
                


		/* cleanup file descriptors */
/*BOGUS!!!!! make 255 max file descriptor */
		for(i = 0; i < 255; i++)
			if(i != outfd)
				close(i);
		/*
			a bunch of idiot software has the nasty property that it fails
			in odd and obscure ways when stdin/stdout/stderr is closed.
			stdout is covered below; do stdin and stderr here
		*/
		if((null = open("/dev/null", O_RDWR, 0777)) < 0){
			ng_log(LOG_ERR, "cannot open /dev/null; continuing: %s", ng_perrorstr());
		}
		if(null != 0)
			dup2(null, 0);
		if(null != 2)
			dup2(null, 2);
		dup2( outfd >= 0? outfd : null, 1 );	/* stdout is node output */

		dec_channel(n->out);			/* it will be closed after the job finishes */
		/* we won't bother closing null; its a harmless thing to pass onto the child */
		/* now onto business */
		signal(SIGPIPE, SIG_DFL);		/* do not pass onto our children */
		execlp("ksh", "ksh", "-c", n->cmd, 0);
		ng_log(LOG_ERR, "cannot execute ksh: %s", ng_perrorstr());
		perror("ksh");
		/* must be _exit; woomera (the parent) calls exit */
		_exit(1);
	} else {				/* continuing on in the main woomera */
		set_pid_node(pid, n);
		n->pid = pid;			/* save for explains etc */
		n->status = J_running;
		nrunning++;
		dec_rsrc(n, 0);			/* dec runnable resource */
		inc_rsrc(n, 1);			/* inc running */
		if(verbose)
			fprintf(stderr, "job %s(%s) started: pid=%d\n", n->name, n->cmd, pid);
	}
	return 1;
}

int
jcatch(int wt)
{
	int status, pid;
	Node *n;

	/* the trick is we may get child processes of ones we forked */
	for(;;){
		if((pid = waitpid(-1, &status, wt? 0:WNOHANG)) <= 0)
			return 0;
		if(verbose > 1)
			fprintf(stderr, "waitup got pid=%d, status=0x%x\n", pid, status);
		if(n = pid_node(pid))
			break;
	}
	/* node n has exited */
	wstat.running--;
	wstat.completed++;
	if(verbose){
		int ks;
		char *estr = NULL;	/* ending string */
		char mbuf[1024];

		fprintf(stderr, "job %s (pid %d): ", n->name, pid);
		if(WIFEXITED(status)){
			estr = "exited normally status=";
			status =  WEXITSTATUS(status);
		} else if(WIFSIGNALED(status)){
			estr = "terminated by signal ";
			status =  WTERMSIG(status);
		} else {
			estr = "weird status =";
		}

		sfsprintf(mbuf, sizeof(mbuf), "job %s (pid %d): %s%d",  n->name, pid, estr, status);
		if(log_state != LS_STDERR)
			ng_log(LOG_INFO, "%s", mbuf);
		else
			sfprintf(sfstderr, "%s\n", mbuf);
	}
	set_pid_node(pid, 0);
	if(WIFEXITED(status) && (WEXITSTATUS(status) == 0))
		n->status = J_finished_okay;
	else
		n->status = J_finished_bad;
	nrunning--;
	dec_rsrc(n, 1);
	n->expiry = time(0)+LONG_TIMEOUT;
	/*if(verbose) */
		fprintf(stderr, "job %s: status=%d [%d] expiry=%d\n", n->name, n->status, n, n->expiry);
	return 1;
}

void
jpmin(int n)
{
	if((n < 0) || (n > MAX_LIMIT))
		n = MAX_LIMIT;
	if(verbose && (n != pmin))
		fprintf(stderr, "%d: pmin: %d -> %d\n", NOW(), pmin, n);
	pmin = n;
}

void
jpmax(int n)
{
	if((n < 0) || (n > MAX_LIMIT))
		n = MAX_LIMIT;
	if(verbose && (n != pmax))
		fprintf(stderr, "%d: pmax: %d -> %d\n", NOW(), pmax, n);
	pmax = n;
}

void
jpload(int n)
{
	if((n < 0) || (n > MAX_LIMIT))
		n = MAX_LIMIT;
	if(verbose && (n != pload))
		fprintf(stderr, "%d: pload: %d -> %d\n", NOW(), pload, n);
	pload = n;
}

#if defined(OS_FREEBSD)  || defined(OS_MACPPC)
#include	<sys/sysctl.h>
#endif

void
jpinit(void)
{
	int n, m;

	n = ng_ncpu();		/* number of online cpus */
	ng_bleat( 1, "reported cpus on this host: %d", n );	/* mostly for my verification */
	m = n*15;		/* maximum processes */
	n = n*15;		/* maximum load; was 6 but RH9 seems to have spuriously high loads */
	if(verbose > 1)
		fprintf(stderr, "setting load limit to %d\n", n);
	jpload(n);
	if(verbose > 1)
		fprintf(stderr, "setting process limit to %d\n", m);
	jpmax(m);
}

void
jrlimit(char *name, int n)
{
	Resource *r;

	r = resource(name);
	if((n < 0) || (n > MAX_LIMIT))
		n = MAX_LIMIT;
	if(r->limit != n){
		if(verbose)
			fprintf(stderr, "%d: resource %s limit: %d -> %d\n", NOW(), name, r->limit, n);
		r->limit = n;
	} else {
		if(verbose > 1)
			fprintf(stderr, "%d: resource %s: limit = %d\n", NOW(), name, n);
	}
}

int
load(int quick)
{
	static int load1;
	static time_t nextlook = 0;
	static int nrun;
	FILE *fp;
	char *cmd = "ng_loadave";		/* moral equivalent of "w -u"; */
	double l1;
	char *ss;
	char buf[NG_BUFFER];
	time_t t;

	if(quick)
		t = 0;
	else
		t = time(0);
	if(t >= nextlook){
		if(fp = popen(cmd, "r")){
			if(fgets(buf, sizeof buf, fp) == NULL){
				fprintf(stderr, "%s: eof from load cmd(%s): %s\n", argv0, cmd, ng_perrorstr());
				l1 = nrunning+1;
			} else if(((ss = strstr(buf, "load average")) == 0) || (sscanf(ss, "%*s %*s %lf", &l1) != 1)){
				fprintf(stderr, "%s: bad format from load cmd(%s): %s", argv0, cmd, ss);
				l1 = nrunning+1;
			}
			if(verbose > 1)
				fprintf(stderr, "read new load ave %.3lf\n", l1);
			load1 = l1;
			pclose(fp);
		} else {
			fprintf(stderr, "%s: attempt to get load(%s) failed\n", argv0, cmd);
			load1 = nrunning+1;	/* plausible guess? */
		}
		nextlook = t+LOAD_INTERVAL;
		if(verbose > 1)
			fprintf(stderr, "actual load set to %d\n", load1);
		nrun = nrunning;
	}
	/* approximate load between measurements */
	return load1 + (nrunning-nrun);
}

typedef struct
{
	int	pid;
	Node	*n;
} Nodemap;
static Nodemap *nmap;
static int nm_a, nm_n;

static Node *
pid_node(int pid)
{
	Nodemap *mp, *ep;

	mp = nmap;
	for(mp = nmap, ep = nmap+nm_n; mp < ep; mp++)
		if(mp->pid == pid)
			return mp->n;
	return 0;
}

static void
set_pid_node(int pid, Node *n)
{
	Nodemap *mp, *ep;

	if(n == 0){
		/* deleting a map */
		for(mp = nmap, ep = nmap+nm_n; mp < ep; mp++)
			if(mp->pid == pid){
				if(mp+1 < ep)
					memcpy(mp, mp+1, (ep-mp-1)*sizeof(*mp));
				nm_n--;
				return;
			}
	} else {
		/* adding a map */
		if(nm_n >= nm_a){
			nm_a += 100;
			if(nmap == 0)
				nmap = ng_malloc(sizeof(nmap[0]), "nmap");
			nmap = ng_realloc(nmap, nm_a*sizeof(nmap[0]), "nmap");
		}
		nmap[nm_n].pid = pid;
		nmap[nm_n].n = n;
		nm_n++;
	}
}

static void
statr(Syment *sym, void *arg)
{
	Resource *r = (Resource *)sym->value;
	FILE *fp = arg;

	fprintf(fp, " %s(%d/%d)", r->name, r->blocked, r->runable);
}

void
blocked(FILE *fp)
{
	int i, k;
	Node *n;

	fprintf(fp, "blocked: pmax(%d) pload(%d)", bmax, bload);
	symtraverse(syms, S_RESOURCE, statr, fp);
	fprintf(fp, "\n");
}
