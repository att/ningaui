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
# ---------------------------------------------------------------------------------
# Mnemonic:	ng_slate -- formerly a3
# Abstract:	Generats a list of job/node pairs which is the optimal 
#		node assignment for each job which minimses the makespan
#		for the bevy ofjobs.
#		
# Date:		10 December 2004
# Author:	Andrew Hume
# ---------------------------------------------------------------------------------

*/

#include	<stdlib.h>
#include	<sfio.h>
#include	<math.h>
#include	<string.h>
#include	"ningaui.h"
#include	"slate.man"

/*
	answer is 794581 for a3.in
*/

typedef struct Node {
	char		*name;
	int		done;
	int		fjob;
	int		njobs;
	int		weird;
	double		tposs, totposs, postdrain;
	double		talloc;
	struct Cost	*jlist;
	struct Node	*next;
} Node;
Node *nodes;
int nnodes;
extern Node *node(char *name);

typedef struct Cost {
	union {
		Node		*n;
		struct Job	*j;
	} u;
	float		cost;
} Cost;

typedef struct Job {
	char		*name;
	Node		*n;
	Cost		*costs;
	int		ncosts;
} Job;
Job *jobs;
int njobs;
int ajobs;

static double solve(double targ, Node **weird, int greedy, int crit, double *err);
static void spew(void);
static enum { stats_none, stats_verbose, stats_paper } dostats = stats_none;
static int nsolve = 0;
static double glob_targ;
static double scale = 1;

#define	NMC	50 	/* number of monte-carlo attempts */
#define	WEIRD	0.05	/* err >= WEIRD is bad */

static int
cmp_bad(const void *a, const void *b)
{
	Node **ap = (Node **)a;
	Node **bp = (Node **)b;

	if((*ap)->postdrain < (*bp)->postdrain)
		return -1;
	if((*ap)->postdrain > (*bp)->postdrain)
		return 1;
	return 0;
}

int
main(int argc, char **argv)
{
	int n, np, i, k, bi;
	int montecarlo = 0;
	char *line;
	char *p[256];
	Job *j;
	Node *q, **bad, *b;
	double t, lt, err, lerr, x, y, beste, bestt, newt, allt;
	int once = 0;
	int greedy = 0;

	ARGBEGIN{
	case '1':	once = 1; break;
	case 'g':	greedy = 1; break;
	case 'm':	montecarlo = 1; break;
	case 'p':	dostats = stats_paper; break;
	case 's':	dostats = stats_verbose; break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf( sfstderr, "Usage: %s [-vmsp]\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 0){
		goto usage;
	}
	ng_srand();
	ng_setfields(", \t");
	njobs = 0;
	ajobs = 10;
	jobs = ng_malloc(ajobs*sizeof(Job), "jobs");

	n = 0;
	while(line = sfgetr(sfstdin, '\n', SF_STRING)){
		n++;
		np = ng_getfields(line, p, sizeof(p)/sizeof(p[0]));
		if((np%2) != 1){
			sfprintf(sfstderr, "%s: bad line %d: even(%d) number of fields '%s'\n", argv0, n, np, line);
			exit(1);
		}
		if(njobs >= ajobs){
			ajobs *= 2;
			jobs = ng_realloc(jobs, ajobs*sizeof(Job), "jobs");
		}
		j = &jobs[njobs++];
		j->name = ng_strdup(p[0]);
		j->ncosts = np/2;
		j->costs = ng_malloc(j->ncosts*sizeof(Cost), "costs");
		for(i = 0; i < j->ncosts; i++){
			j->costs[i].u.n = node(p[2*i+1]);
			j->costs[i].cost = strtod(p[2*i+2], 0);
		}
	}
	for(q = nodes; q; q = q->next)
		q->jlist = ng_malloc(ajobs*sizeof(q->jlist[0]), "jlist");
	ng_bleat(1, "%s: read %d jobs over %d nodes", argv0, njobs, nnodes);
	scale = 1e6;
	bad = ng_malloc((nnodes+1)*sizeof(bad[0]), "bad node list");
	bi = 0;
	bad[bi] = 0;

	/*
		run greedy first, with a crit analysis to figure out who is relevent
	*/
	for(q = nodes; q; q = q->next)
		q->weird = 0;
	allt = solve(1.0, bad, 1, 1, &err);
	ng_bleat(1, "initial target estimate: %.1f (scale=%.1g)", allt, scale);
	for(q = nodes; q; q = q->next)
		if(q->weird){
			bad[bi++] = q;
			bad[bi] = 0;
			ng_bleat(1, "\tnode %s weird (poss %.3f < targ %.3f)", q->name, q->totposs/scale, allt/scale);
		}
	qsort(bad, bi, sizeof(bad[0]), cmp_bad);
	/*
		run greedy again, with the noncrit list from before, to get an accurate estimate
	*/
	allt = solve(allt, bad, 1, 0, &err);
	ng_bleat(1, "second estimate: %.1f", allt/scale);
	/*
		allt should be quite good now; reset criticality and we're done
	*/
	bi = 0;
	for(q = nodes; q; q = q->next){
		q->weird = 0;
		if(q->postdrain < (1-WEIRD)*allt){
			bad[bi++] = q;
			bad[bi] = 0;
			q->weird = 1;
			ng_bleat(1, "\tnode %s noncrit (poss %.3f < targ %.3f)", q->name, q->postdrain/scale, allt/scale);
		}
	}

	beste = 1e8;
	bestt = 1;
	lerr = beste;
	t = allt;
	for(;;){
		newt = solve(t, bad, greedy, 0, &err);
		ng_bleat(1, "\tt=%.3f err=%.4f best=(%.3f %.4f)", t, err*100, bestt, beste*100);
		if((err >= lerr) || (beste*bestt < 0.01))
			break;
		if(err < beste){
			bestt = t;
			beste = err;
		}
		t = newt;
		lerr = err;
		if(once)
			break;
	}
	/*
		just check we're not weird
	*/
	if(0 && !once && (err > WEIRD)){
		sfprintf(sfstderr, "detected quite nonuniform solution (err >= %.3f%%); lets try something different\n", WEIRD*100);
		for(bi = 0; err > WEIRD; bi++){
			b = 0;
			for(q = nodes; q; q = q->next){
				if(q->weird)
					continue;
				if((b == 0) || (q->talloc < b->talloc))
					b = q;
			}
			sfprintf(sfstderr, "weird round %d: choosing node %s\n", bi, b->name);
			b->weird = 1;
			bad[bi] = b;
			bad[bi+1] = 0;
			beste = 1e8;
			bestt = 1;
			lerr = beste;
			t = allt*(nnodes-bi-1)/nnodes;
			for(;;){
				newt = solve(t, bad, greedy, 0, &err);
				ng_bleat(1, "\tt=%.3f err=%.4f best=(%.3f %.4f)", t, err*100, bestt, beste*100);
				if((err >= lerr) || (beste*bestt < 0.5))
					break;
				if(err < beste){
					bestt = t;
					beste = err;
				}
				t = newt;
				lerr = err;
				if(once)
					break;
			}
		}
	}

	if(montecarlo){
		/* monte carlo around to find a better one */
		if(dostats)
			sfprintf(sfstderr, "--- try %d random targets of %.3lf +- %.3lf\n", NMC, bestt, beste*bestt);
		x = bestt*(1-beste);
		for(k = 0; k < NMC; k++){
			t = x + 2*bestt*beste*drand48();
			y = solve(t, bad, greedy, 0, &err);
			if(err < beste){
				beste = err;
				bestt = t;
				if(dostats)
					sfprintf(sfstderr, "\tnew best=(%.3f, %.4f)\n", bestt, beste*100);
			}
		}
	}

	/*
		redo for the best targ value we found
	*/
	if(dostats)
		sfprintf(sfstderr, "\nfinal solution: total %d solves\n", nsolve+1);
	if(!once)
		solve(bestt, bad, greedy, 0, &err);
	spew();

	exit(0);
	return 0;
}

/* dump the assignments: job node */
void
spew(void)
{
	int i;

	for(i = 0; i < njobs; i++)
		sfprintf(sfstdout, "%s %s\n", jobs[i].name, jobs[i].n->name);
}

Node *
node(char *name)
{
	Node *n;

	for(n = nodes; n; n = n->next)
		if(strcmp(name, n->name) == 0){
			return n;
		}
	n = ng_malloc(sizeof(Node), "node");
	n->name = ng_strdup(name);
	n->next = nodes;
	n->weird = 0;
	nodes = n;
	nnodes++;
	return n;
}

static int
jcmp(const void *a, const void *b)
{
	Cost *ca = (Cost *)a, *cb = (Cost *)b;

	if(ca->cost < cb->cost)
		return 1;
	else if(ca->cost > cb->cost)
		return -1;
	return 0;
}

static double
headroom(Node *n, double targ)
{
	return fabs(n->tposs/(targ - n->talloc));
}

static void
assign(Node *n, int slot)
{
	Job *j;
	Node *nn;
	int k;

	j = n->jlist[slot].u.j;
	/* do node book keeping */
	n->tposs -= n->jlist[slot].cost;
	n->talloc += n->jlist[slot].cost;
	/* job book keeping */
	j->n = n;
	/* other nodes */
	for(k = 0; k < j->ncosts; k++){
		nn = j->costs[k].u.n;
		if(nn == n)
			continue;	/* don't do me! */
		nn->tposs -= j->costs[k].cost;
	}

	if((n->talloc > 0.97*glob_targ) && (verbose > 2)){
		sfprintf(sfstderr, "assigned slot %d to %s; cost=%.4lf\n", slot, n->name, n->jlist[slot].cost);
		for(n = nodes; n; n = n->next)
			sfprintf(sfstderr, "\t%s: alloc %.3f poss %.3f\n", n->name, n->talloc, n->tposs);
	}
}

static void
drain(Node *n)
{
	int k;

	if(verbose > 1)
		sfprintf(sfstderr, "draining(%s)\n", n->name);
	for(k = 0; k < n->njobs; k++)
		if(n->jlist[k].u.j->n == 0)
			assign(n, k);
	n->done = 1;
}
	
static double
solve(double targ, Node **weird, int greedy, int crit, double *err)
{
	Job *j;
	int i, k, nn;
	Node *n, *best;
	double t, var, gm, am, delta;
	float hi, lo;

	/*
		first initial alloc lists and counters
	*/
	glob_targ = targ;
	nsolve++;
	for(n = nodes; n; n = n->next){
		n->njobs = 0;
		n->tposs = 0;
		n->done = 0;
		n->fjob = 0;
	}
	for(i = 0; i < njobs; i++){
		j = &jobs[i];
		j->n = 0;
		for(k = 0; k < j->ncosts; k++){
			n = j->costs[k].u.n;
			n->jlist[n->njobs].u.j = j;
			n->jlist[n->njobs].cost = j->costs[k].cost;
			n->tposs += j->costs[k].cost;
			n->njobs++;
		}
	}
	for(n = nodes; n; n = n->next){
		n->talloc = 0;
		n->totposs = n->tposs;
		qsort(n->jlist, n->njobs, sizeof(n->jlist[0]), jcmp);
	}
	if(verbose > 1){
		sfprintf(sfstderr, "solve(%.3lf)\n", targ/scale);
		for(n = nodes; n; n = n->next)
			sfprintf(sfstderr, "\t%s: poss %.1f (%.3f%%), %d jobs (weird=%d)\n", n->name,
				n->tposs/scale, (n->tposs-targ)*100/targ, n->njobs, n->weird);
	}
	if(weird[0]){
		if(verbose > 1)
			sfprintf(sfstderr, "drain weird list:\n");
		for(i = 0; weird[i]; i++)
			drain(weird[i]);
		if(verbose > 1)
			sfprintf(sfstderr, "done drain weird list\n");
		for(n = nodes; n; n = n->next)
			n->postdrain = n->weird? n->talloc : n->tposs;
		if(verbose > 1)
			for(n = nodes; n; n = n->next)
				sfprintf(sfstderr, "\t%s: poss %.1f (%.3f%%), %d jobs\n", n->name,
					n->tposs/scale, (n->tposs-targ)*100/targ, n->njobs);
	} else {
		for(n = nodes; n; n = n->next)
			n->postdrain = n->tposs;
	}

	/*
		now loop
	*/
	if(greedy){
		for(;;){
			best = 0;
			for(n = nodes; n; n = n->next){
				if(n->done)
					continue;
				if((best == 0) || (n->talloc < best->talloc))
					best = n;
			}
			if(best == 0)
				break;
			for(k = best->fjob; k < best->njobs; k++)
				if(best->jlist[k].u.j->n == 0)
					goto done1;
			if(verbose > 1)
				sfprintf(sfstderr, "emptied(%s)\n", best->name);
			best->done = 1;
			continue;
		done1:
			assign(best, k);
			best->fjob = k+1;
		}
	} else {
		for(;;){
			best = 0;
			for(n = nodes; n; n = n->next){
				if(n->done)
					continue;
				if((n->tposs+n->talloc) <= targ){
					drain(n);
					continue;
				}
				if((best == 0) || (headroom(n, targ) < headroom(best, targ)))
					best = n;
			}
			if(best == 0)
				break;
			for(k = best->fjob; k < best->njobs; k++)
				if(best->jlist[k].u.j->n == 0)
					goto done;
			if(verbose > 1)
				sfprintf(sfstderr, "emptied(%s)\n", best->name);
			best->done = 1;
			continue;
		done:
			assign(best, k);
			best->fjob = k+1;
		}
	}
	if(verbose > 1){
		sfprintf(sfstderr, "solve done\n");
		for(n = nodes; n; n = n->next)
			sfprintf(sfstderr, "\t%s: alloc %.1f (%.3f%%) postdrain=%.1f weird=%d\n", n->name, n->talloc/scale, (n->talloc-targ)*100/targ, n->postdrain/scale, n->weird);
	}
	/*
		do crit screening
	*/
	if(crit){
		best = nodes;
		for(n = nodes; n; n = n->next){
			if(n->talloc > best->talloc)
				best = n;
		}
		lo = (1-WEIRD)*best->talloc;
		for(n = nodes; n; n = n->next)
			n->weird = n->talloc < lo;
	}
	/*
		evaluate solution
	*/
	lo = 1e20;
	hi = -lo;
	nn = 0;
	for(n = nodes; n; n = n->next){
		if(n->weird)
			continue;
		nn++;
		if(n->talloc > hi)
			hi = n->talloc;
		if(n->talloc < lo)
			lo = n->talloc;
	}
	var = 0;
	gm = 1;
	am = 0;
	for(n = nodes; n; n = n->next){
		if(n->weird)
			continue;
		var += n->talloc * n->talloc;
		am += n->talloc;
		gm *= n->talloc;
	}
	var -= am*am/nn;
	var = sqrt(var/nn);
	gm = exp(log(gm)/nn);
	am /= nn;
	t = gm;
	delta = hi-t;
	if(t-lo > delta)
		delta = t-lo;
	*err = delta/t;

	switch(dostats)
	{
	case stats_verbose:
		sfprintf(sfstderr, "solve(%.3f): newtarg=%.3lf sd=%.3lf(%.4f%%) hi=%.3lf hi-lo=%.3f(%.4f%%) err=%.4lf%% am=%.3f gm=%.3f nn=%d\n", targ/scale,
			t/scale, var, var*100/t, hi, hi-lo, (hi-lo)*100/t, *err*100, am/scale, gm/scale, nn);
		break;
	case stats_paper:
		if(nsolve == 1)
			sfprintf(sfstderr, "Iteration Target SD Error NewTarget\n");
		sfprintf(sfstderr, "%d %.3f %.6f%% %.6lf%% %.3lf\n",
			nsolve, targ, var*100/t, *err*100, t);
		break;
	}
	return t;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_slate:Generate An Optimised Scheduling Solution)

&space
&synop	&this [-v] [-m] [-sp]

&space
&desc	&This reads standard input for a list of job descriptions, and outputs
	an optimal schedule for running the jobs.
&space
	Jobs are described by lines with an odd number of fields. The first field is the
	job name, and the remaining fields are nodename and cost (of running that job on that node)
	pairs. The output schedule consists of jobname, nodename pairs, one per line.
	The schedule is optimal in the sense of minimising the makespan (or more simply,
	the time taken on the node with the longest schedule).
&space
	The fields above are simply strings or floating point numbers, seperated by
	blanks, tabs or commas. The input and output formats match those emitted and
	needed by &ital(seneschal).

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-m : Monte Carlo. Try to improve the result by randomised attempts close
	to the solution from the first pass. This option is believed ineffective.
&space
&term	-p : Paper. Produce abreviated statistics.
&space
&term	-s : Statistics. Print on stderr various statistics about the solutions being generated.
	This will tell how close to optimal the solution is. The normal stopping rule is
	to halt when the difference between the longest and shortest node schedules is less than
	0.5 cost units (which are typically seconds).
&space
&term	-v : Verbose. The more, the merrier.
&endterms


&space
&parms

&space
&exit

&space
&see

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	10 Dec 2004 (agh) : Fresh from the oven.
&endterms

&scd_end
------------------------------------------------------------------ */
