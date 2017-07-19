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

#include	<stdlib.h>
#include	<sfio.h>
#include	<string.h>
#include	<math.h>
#include	"ningaui.h"
#include	"ng_lib.h"

#include "admeasure.man"

/*
	timings on ningd0 for a3.in

	interior point = 25mins
	simplex = 90mins

	asnwer is 794581
*/

enum { A_no = 0, A_yes, A_maybe };

typedef struct Node {
	char		*name;
	int		nfiles;
	ng_uint64	tposs;
	ng_uint64	talloc;
	ng_uint64	size;
	int		junk;
	struct Node	*next;
} Node;
Node *nodes;
int nnodes;
extern Node *node(char *name);

typedef struct {
	char		*name;
	char		*fbm;
	int		nfiles;
	ng_uint64	tposs;
	ng_uint64	talloc;
	int		done;
	Node		*n;
} Group;
int ngroups;
Group *groups;

typedef struct {
	Node		*n;
	char		*path;
} Instance;
static void newi(Instance *i, char *noden, char *path, ng_uint64 s);

typedef struct File {
	char		*name;
	int		done;
	ng_uint64	size;
	int		ni;
	Instance	*i;
} File;
File *files;
int nfiles;
int afiles;

char *dname = "!!";
int prbase = 0;

static double solve(ng_uint64 gsize, int greedy, ng_uint64 *mean, int force);
static void setup(ng_uint64 t);
static void writeout(Sfio_t *);

static int
fcmp(const void *a, const void *b)
{
	File *ca = (File *)a, *cb = (File *)b;

	if(ca->size < cb->size)
		return 1;
	else if(ca->size > cb->size)
		return -1;
	return 0;
}

int
main(int argc, char **argv)
{
	int n, np, i, k;
	char *line, *base;
	char *oflist = 0;
	ng_uint64 m;
	ng_int64 tsize, gsize;
	char *p[256];
	File *f;
	Node *q;
	ng_uint64 t;
	Sfio_t *ofp;
	int force = 0;

	gsize = -1;
	ngroups = -1;
	ARGBEGIN{
	case 'b':	prbase = 1; break;
	case 'd':	SARG(dname); break;
	case 'f':	force = 1; break;
	case 'g':	I64ARG(gsize);
			switch(*_argip)
			{
			case 'g':	gsize *= 1000LL*1000*1000; break;
			case 'G':	gsize *= 1024LL*1024*1024; break;
			case 'k':	gsize *= 1000; break;
			case 'K':	gsize *= 1024; break;
			case 'm':	gsize *= 1000LL*1000; break;
			case 'M':	gsize *= 1024LL*1024; break;
			}
			break;
	case 'n':	IARG(ngroups); break;
	case 'o':	SARG(oflist); break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-vbf] [-d name] [-o flist] [-n ngroup] [-g gsize] prefix\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 1){
		goto usage;
	}
	ng_srand();
	ng_setfields(", \t");
	nfiles = 0;
	afiles = 10;
	files = ng_malloc(afiles*sizeof(File), "files");
	nodes = 0;

	n = 0;
	tsize = 0;
	while( (line = sfgetr(sfstdin, '\n', SF_STRING)) ){
		n++;
		np = ng_getfields(line, p, sizeof(p)/sizeof(p[0]));
		if(np != 3){
			sfprintf(sfstderr, "%s: bad line %d: #fields(%d) !=3 in '%s'\n", argv0, n, np, line);
			exit(1);
		}
		base = strrchr(p[1], '/')+1;
		if((nfiles == 0) || strcmp(base, files[nfiles-1].name)){
			if(nfiles >= afiles){
				afiles *= 2;
				files = ng_realloc(files, afiles*sizeof(File), "files");
			}
			f = &files[nfiles++];
			f->size = strtoll(p[2], 0, 10);
			if(f->size < 1){
				sfprintf(sfstderr, "%s: warning: bad size %I*d for %s reset to 1\n",
					argv0, sizeof(f->size), f->size, p[1]);
				f->size = 1;
			}
			tsize += f->size;
			f->ni = 1;
			f->i = ng_malloc(sizeof(f->i[0]), "inst1");
		} else {
			f = &files[nfiles-1];
			f->ni++;
			f->i = ng_realloc(f->i, f->ni*sizeof(f->i[0]), "inst2");
		}
		newi(&f->i[f->ni-1], p[0], p[1], f->size);
		if(f->ni == 1)
			f->name = strrchr(f->i[0].path, '/') + 1;
	}
	qsort(files, nfiles, sizeof(files[0]), fcmp);

	if(gsize > 0)
		ngroups = (tsize+gsize-1)/gsize;
	else if(ngroups <= 0)
		goto usage;
	if(verbose >= 1)
		sfprintf(sfstderr, "%d groups (tsize=%I*d gsize=%I*d)\n", ngroups, Ii(tsize), Ii(gsize));
	groups = ng_malloc(ngroups*sizeof(groups[0]), "groups");
	for(i = 0; i < ngroups; i++){
		char buf[NG_BUFFER];

		groups[i].fbm = ng_malloc(nfiles, "fbm");
		sfsprintf(buf, sizeof buf, "%s%05d", argv[0], i);
		groups[i].name = ng_strdup(buf);
	}
	sfprintf(sfstderr, "%s: read %d files over %d nodes; forming %d groups with prefix %s\n",
		argv0, nfiles, nnodes, ngroups, argv[0]);

	/*
		find an initial solution
	*/
	t = 0;
	for(i = 0; i < nfiles; i++){
		t += files[i].size;
		if(strcmp(dname, files[i].name) == 0){
			sfprintf(sfstdout, "debug(%s): size=%I*d, %d instances\n",
				dname, sizeof(files[i].size), files[i].size, files[i].ni);
			for(k = 0; k < files[i].ni; k++)
				sfprintf(sfstdout, "\t%s %s\n", files[i].i[k].n->name, files[i].i[k].path);
		}
	}
	t /= ngroups;
	if(verbose)
		sfprintf(sfstderr, "set target group size to %I*d\n", sizeof(t), t);
	setup(t);
	solve(t, 1, &m, force);
	if(oflist){
		if((ofp = sfopen(0, oflist, "w")) == NULL){
			perror(oflist);
			exit(1);
		}
	} else
		ofp = 0;
	writeout(ofp);

	exit(0);
	return 0;
}

static void
setup(ng_uint64 t)
{
	int g, lg, k;
	Node *n;
	ng_int64 tot;
	double d;

	tot = 0;
	for(n = nodes; n; n = n->next)
		tot += n->size;
	d = ngroups/(double)tot;
	lg = 0;
	for(n = nodes; n; n = n->next){
		g = n->size*d+.5;
		if(g+lg > ngroups)
			g = ngroups-lg;
		for(; g > 0; g--)
			groups[lg++].n = n;
	}
	while(lg < ngroups)
		groups[lg++].n = nodes;
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
	n->size = 0;
	n->nfiles = 0;
	nodes = n;
	nnodes++;
	return n;
}

static void
newi(Instance *i, char *noden, char *path, ng_uint64 s)
{
	i->n = node(noden);
	i->path = ng_strdup(path);
	i->n->size += s;
	i->n->nfiles++;
}

static double
headroom(Group *g, double targ)
{
	return fabs(g->n->tposs/(targ - g->talloc));
}

static void
assign(Group *g, int slot)
{
	File *j;
	Node *n;
	int i;
	static int did = 0;

	if(verbose > 1)
		sfprintf(sfstdout, "slot %d (%s) -> group %d\n", slot, files[slot].name, g-groups);
	for(i = 0; i < ngroups; i++)
		groups[i].fbm[slot] = A_no;
	g->fbm[slot] = A_yes;
	files[slot].done = 1;
	/* do book keeping */
	n = g->n;
	n->tposs -= files[slot].size;
	g->tposs -= files[slot].size;
	n->talloc += files[slot].size;
	g->talloc += files[slot].size;
	g->nfiles++;
	did++;
/*
	if(verbose && ((did%100) == 0)){
		sfprintf(sfstdout, "--- assigned(%d)\n", did);
	}
*/
}

static void
drain(Group *g)
{
	int k;

	if(verbose)
		sfprintf(sfstdout, "draining(%d)\n", g-groups);
	for(k = 0; k < nfiles; k++)
		if(g->fbm[k] == A_maybe)
			assign(g, k);
	g->done = 1;
}
	
static double
solve(ng_uint64 targ, int greedy, ng_uint64 *mean, int force)
{
	Group *g, *best;
	int i, k, f, n1, n0, ng;
	Node *n;
	double t, var, gm, m;
	ng_int64 hi, lo;
	int debug;
	int round = 0;
	int last_try;

again:
	last_try = ++round > 5;
	/*
		first initial alloc lists and counters
	*/
	for(n = nodes; n; n = n->next){
		n->tposs = n->size;
		n->talloc = 0;
	}
	for(i = 0; i < ngroups; i++){
		g = groups+i;
		memset(g->fbm, A_no, nfiles);
		g->nfiles = 0;
		g->tposs = g->n->tposs;
		g->talloc = 0;
		g->done = 0;
	}
	for(f = 0; f < nfiles; f++){
		files[f].done = 0;
		for(n = nodes; n; n = n->next)
			n->junk = A_no;
		for(i = 0; i < files[f].ni; i++)
			files[f].i[i].n->junk = A_maybe;
		for(i = 0; i < ngroups; i++)
			groups[i].fbm[f] = groups[i].n->junk;
	}
		
	if(verbose){
		sfprintf(sfstderr, "solve(%I*d, %d)\n", sizeof(targ), targ, greedy);
		sfprintf(sfstdout, "solve(%I*d)\n", sizeof(targ), targ);
		sfprintf(sfstdout, "nodes:\n");
		for(n = nodes; n; n = n->next)
			sfprintf(sfstdout, "\t%s: size %.3fGB, %d files\n", n->name, n->size/1e9, n->nfiles);
		sfprintf(sfstdout, "groups:\n");
		for(i = 0, g = groups; i < ngroups; i++, g++)
			sfprintf(sfstdout, "\t%d(%s): poss %I*u (%.3f%%), %d files\n", i, g->n->name,
				sizeof(g->tposs), g->tposs, (g->tposs-targ)*100.0/targ, g->nfiles);
	}

	/*
		now loop
	*/
	for(f = 0; f < nfiles; f++){
		debug = strcmp(dname, files[f].name) == 0;
		if(debug)
			sfprintf(sfstdout, "debug(%s) f=%d (done=%d):\n", dname, f, files[f].done);
		if(files[f].done)
			continue;
		best = 0;
		for(i = 0; i < ngroups; i++){
			g = groups+i;
			if(debug)
				sfprintf(sfstdout, "\tgrp %d: done=%d fbm=%d\n", i, g->done, g->fbm[f]);
			if(g->done)
				continue;
			if(g->fbm[f] != A_maybe)
				continue;
			if((g->tposs+g->talloc) <= targ){
				drain(g);
				continue;
			}
			if(greedy){
				if((best == 0) || (g->talloc < best->talloc))
					best = g;
			} else {
				if((best == 0) || (headroom(g, targ) < headroom(best, targ)))
					best = g;
			}
		}
		if(verbose > 1)
			sfprintf(sfstdout, "file %s(%I*d): best=%d\n", files[f].name, Ii(sizeof(files[f].size)), best);
		if(files[f].done)		/* via drain */
			continue;
		if(debug)
			sfprintf(sfstdout, "\t-- best = %d\n", best-groups);
		if(best == 0){
			for(i = 0; i < ngroups; i++){
				g = groups+i;
				if(g->fbm[f] != A_maybe)
					continue;
				if(greedy){
					if((best == 0) || (g->talloc < best->talloc))
						best = g;
				} else {
					if((best == 0) || (headroom(g, targ) < headroom(best, targ)))
						best = g;
				}
			}
			if(best == 0){
				sfprintf(sfstderr, "%s: internal error: can't allocate file %s\n", argv0, files[f].name);
				groups[ngroups-1].n = n = files[f].i[0].n;
				sfprintf(sfstderr, "resetting group %d's node to %s and trying again\n", ngroups-1, n->name);
				if(!last_try)
					goto again;
				if(!force)
					exit(1);
				if(force++ == 1)
					sfprintf(sfstderr, "forcing flag set: forcing orphans into neighbours\n");
				for(i = 0; i < ngroups; i++){
					g = groups+i;
					if(greedy){
						if((best == 0) || (g->talloc < best->talloc))
							best = g;
					} else {
						if((best == 0) || (headroom(g, targ) < headroom(best, targ)))
							best = g;
					}
				}
				if(best == 0){
					sfprintf(sfstderr, "%s: bad internal error: can't allocate file %s\n", argv0, files[f].name);
					abort();
				}
			}
		}
		assign(best, f);
	}
	if(verbose){
		sfprintf(sfstdout, "solve(%I*d) done\n", sizeof(targ), targ);
		sfprintf(sfstdout, "groups:\n");
		for(i = 0, g = groups; i < ngroups; i++, g++)
			sfprintf(sfstdout, "\t%d(%s): alloc %I*u (%.3f%%), %d files\n", i, g->name,
				sizeof(g->talloc), g->talloc, g->talloc*100.0/targ, g->nfiles);
	}
	/*
		evaluate solution
	*/
	t = 0;
	n1 = n0 = 0;
	lo = targ*(ng_int64)ngroups;
	hi = 0;
	for(g = groups; g < &groups[ngroups]; g++){
		if(g->nfiles == 1)
			n1++;
		else if(g->nfiles == 0)
			n0++;
		else {
			t += g->talloc;
			if(g->talloc > hi)
				hi = g->talloc;
			if(g->talloc < lo)
				lo = g->talloc;
		}
	}
	ng = ngroups - n1 - n0;
	t /= ng;
	var = 0;
	for(g = groups; g < &groups[ngroups]; g++)
		if(g->nfiles > 1)
			var += (t - g->talloc) * (t - g->talloc);
	var = sqrt(var/ng);
	sfprintf(sfstderr, "solve(%.3fMB, %d): excluding %d 1-groups and %d 0-groups, goal=%.3lfMB: sd=%.3lfMB(%.3f%%)",
		targ/1e6, greedy, n1, n0, t*1e-6, var/1e6, var*100/t);
	if(var < 1e3)
		sfprintf(sfstderr, "=%.1lfB", var);
	sfprintf(sfstderr, " range=%I*d(%.3f%%)\n",
		sizeof(hi-lo), hi-lo, 100.0*(hi-lo)/t);
	*mean = t;
	return hi-lo;
}

static void
writeout(Sfio_t *flist)
{
	int i, f, j;
	Group *g;
	Sfio_t *fp;

	for(i = 0, g = groups; i < ngroups; i++, g++){
		if(g->nfiles == 0)
			continue;
		if((fp = sfopen(0, g->name, "w")) == NULL){
			perror(g->name);
			exit(1);
		}
		if(flist)
			sfprintf(flist, "%s\n", g->name);
		sfprintf(fp, "%s\n", g->n->name);
		for(f = 0; f < nfiles; f++)
			if(g->fbm[f] == A_yes){
				for(j = 0; j < files[f].ni; j++)
					if(files[f].i[j].n == g->n)
						sfprintf(fp, "%s\n", prbase? files[f].name : files[f].i[j].path);
			}
		if(sferror(fp) || sfclose(fp)){
			perror(g->name);
			exit(1);
		}
	}
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_admeasure:Partition into sets.)

&space
&synop	ng_admeasure [-vbf] [-d name] [-o flist] [-n ngroup] [-g gsize] prefix

&space
&desc	&This solves the following set partitioning problem: We have a set of files
	which are replicated among a set of nodes. Each instance of a file on a node
	has an associated size. The problem is to partition the files into subsets
	whose size does not exceed a specified limit such that each is present in exactly
	one subset, all of the instances in a subset exist on one node,
	and that all the subsets are as equal as possible in size.
	The output is a set of files (one per subset) beginning with &ital(prefix); each files
	contains the node's name and the instances in that subset.

&space
	&This normally produces very equisized sets; it is common to see solutions
	for files totalling 100GB split into 50 subsets where the difference in size
	between the smallest and largest subset is under 100 bytes.
	Nevertheless, pathological distributions can cause the algorithms to fail
	to produce good subsets. In these cases, &this will exit with a failure
	unless the &cw(-f) (force) flag is set. Forced solutions will have subsets
	some instances don't appear on the designated node. For the normal uses
	of &this, file lists for &ital(ng_nawab) jobs, this won't be a hindrance.

&space
	The input format is simple: triples of (nodename, pathname, size)
	separated by white space and one triple per line.

&space
	Warning: the author considers the underlying C code to be wretched.
	Two independent rewrites yielded almost exactly the same code, so he
	has given up trying to make it better.

&space
&opts
&begterms
&term 	-b : print only the basename of the pathname in the output lists.
&space

&term 	-d name : generate debugging output related to file &ital(name).

&space
&term 	-f : force output sets to be generated even when the partitioning fails.

&space
&term 	-g size : generate subsets whose size does not exceed &ital(size), specifid as a number followed
	by a scale (&cw(k)=10^3, &cw(K)=2^10, &cw(m)=10^6, &cw(M)=2^20, &cw(g)=10^9, &cw(G)=2^30).
	The default value is &cw($NG_RM_STATS).

&space
&term 	-n num : generate &ital(num) subsets.

&space
&term 	-o olist : print the names of all the subset files generated into a file named &ital(olist).

&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&space
&exit	When the binary completes, the exit code returned to the caller 
	is set to zero (0) if the number of parameters passed in were 
	accurate, and to one (1) if an insufficent number of parameters
	were passed. 

&space
&see	ng_nawab

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	03 Aug 200? (agh) : Original code 
&endterms
&scd_end
*/
