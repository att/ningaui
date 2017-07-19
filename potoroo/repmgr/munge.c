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

#include	<stdarg.h> /* must be BEFORE everything to prevent ast stdio errors on operon */
#include <sfio.h>
#include <string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/times.h>
#include "ningaui.h"
#include "ng_ext.h"
#include "repmgr.h"
#include	<ctype.h>
#include	<math.h>

static Node **nodes, **np;
static int nnodes;
static int nreps;
static time_t now;
static Sfio_t *explainfp = 0;
static char *explain_name = 0;
static int explaining = 0;
static Stats *stp = 0;
static Stats zerostats;
static int nattempts;
static int ndeletes;
static int nrenames;
static int sawtoken;
static int global_hand;
static int nodeletes;		/* set if no deletes */
static clock_t cscale;
extern char *basename(char *);
char *gname[] = { "off", "on", "?" };
static char *hname[] = { "must_on", "must_off", "on", "off", "none" };

#define	LEASE_COPY	8*60*60		/* minutes */
#define	LEASE_DELETE	60*60		/* minutes */
#define	LEASE_MOVE	30*60		/* minutes */
#define	NODE_COPIES	50		/* per node */
#define	NODE_DELETES	50		/* per node */
#define	NODE_RENAMES	200		/* per node */
#define	MAX_ACTIONS	2		/* maximum number of copies per file */
#define	PLIM		2		/* even emptiest node hets a weight of PLIM */
#define	WOOMERA_TIMEOUT	8		/* timeout for wreq */

static void
set_stable(File *f, int st)
{
	if((f->stable == 0) && (st > 0)){
		/* log this change */
		ng_log(LOG_INFO, "REPMGR STABLE %s %s\n", f->name, f->md5);
	}
	f->stable = st;
}

void
enablemunge(int yes)
{
#ifdef	CRAP
	if(yes){
		mode = HANDS_ON;
		if(yes > 1)
			nattempts = 0;		/* allow enough slots to finish */
		sfprintf(sfstderr, "enabling munge actions\n");
	} else {
		mode = NO_HANDS;
		sfprintf(sfstderr, "disabling munge actions\n");
	}
#endif
}

void
rprintf(Node *node, char *fmt, ...)
{
	va_list	argp;
	char buf[NG_BUFFER];
	int n;
 
	va_start(argp, fmt);			/* point to first variable arg */
	n = vsnprintf(buf, sizeof buf, fmt, argp);  
	va_end(argp);				/* cleanup of variable arg stuff */
	if(node->rn+n >= node->rlen){
		node->rlen = ((node->rn+n+1024)/1024)*1024;
		node->rbuf = ng_realloc(node->rbuf, node->rlen, "node buffer");
	}
	memcpy(node->rbuf+node->rn, buf, n);
	node->rn += n;
	node->rbuf[node->rn] = 0;
}

/*
	prune's job is to remove outdated copies, attempts, and hints
*/
static void
prune(File *f)
{
	Hint *h, **lh;
	Attempt *a, **la;
	int foundf, foundt;
	int done;
	Instance *i, **li;

	if(f->rename){
		Syment *s;
		File *q;

/* todo0 */
		s = symlook(syms, f->rename, SYM_FILE, 0, SYM_NOFLAGS);
		q = s? s->value:0;
		if((s == 0) || (q && q->matched)){
			if(verbose)
				sfprintf(sfstderr, "%s renaming to %s has matched == 1\n", f->name, f->rename);
			if(explaining)
				sfprintf(explainfp, "\trenaming to %s has matched == 1\n", f->rename);
			for(a = f->attempts; a; a = a->next)
				a->lease = 0;
			f->noccur = 0;
			ng_free(f->rename);
			f->rename = 0;
		}
	}

	for(li = &f->copies; i = *li; ){
		if(i->deleted){	/* skip/del this link */
			*li = i->next;
			delinstance(i);
		} else
			li = &((*li)->next);
	}
	for(lh = &f->hints; h = *lh; ){
		if(h->lease < now){
			*lh = h->next;
			delhint(h);
		} else
			lh = &((*lh)->next);
	}
	for(la = &f->attempts; a = *la; ){
		foundt = foundf = 0;
		for(i = f->copies; i; i = i->next){
			if(i->node == a->from)
				foundf = 1;
			if(i->node == a->to)
				foundt = 1;
		}
		if(a->type == A_create)
			done = foundt || !foundf;	/* if !foundf, can't work */
		else
			done = !foundf;
		if((a->lease < now) || done){
			*la = a->next;
			switch(a->type)
			{
			case A_create:	a->from->copies--; break;
			case A_move:	a->from->renames--; break;
			case A_destroy:	a->from->deletes--; break;
			}
			end_attempt(a, f, a->lease >= now);
			delattempt(a);
		} else {
			la = &((*la)->next);
			switch(a->type)
			{
			case A_create:	a->from->xc++; break;
			case A_move:	a->from->xr++; break;
			case A_destroy:	a->from->xd++; break;
			}
		}
	}
}

static void
makecopy(File *f, Node *src, Node *dest)
{
	char buf[NG_BUFFER];
	static int id = 0;
	static int pid = 0;
	char *dpath;
	Instance *c;

	if(pid == 0)
		pid = getpid();
	stp->itocopy_n++;
	stp->itocopy_b += f->size;
	if(src->copies < NODE_COPIES){
		if(dest->spacefslist == 0){
			dest->spacefslist = ng_strdup("/ningaui/fs00 1 1");
			if(verbose)
				sfprintf(sfstderr, "set %s->spacefslist to %s\n", dest->name, dest->spacefslist);
		}
		dpath = ng_spaceman_rand(dest->spacefslist, basename(f->name));
		iobytes += f->size;
		sfprintf(sfstderr, "%d/%d initiate copy of %s:%s to %s:%s\n", src->copies, NODE_COPIES, src->name, f->name, dest->name, dpath);
		if(explaining)
			sfprintf(explainfp, "%d/%d initiate copy of %s:%s to %s:%s\n", src->copies, NODE_COPIES, src->name, f->name, dest->name, dpath);
		rprintf(src, "job rm%d_%d: RM_SEND RM_FS pri 50 cmd ng_rm_send_file %s %s %s %s\n", pid, id, f->name, f->md5, dest->name, dpath);
		rprintf(src, "job: after rm%d_%d, cmd ng_wreq -t %d -s %s 'job: RM_RCV_FILE RM_FS pri 30 cmd ng_rm_rcv_file %s'\n", pid, id, WOOMERA_TIMEOUT, dest->name, dpath, pid, id);
		id++;
		dest->count++;
		src->copies++;
		src->nc++;
		f->attempts = attempt(A_create, src, dest, now+LEASE_COPY, f->attempts);
	} else {
		if(explaining)
			sfprintf(explainfp, "would have copied %s:%s to %s but slots full %d/%d\n", src->name, f->name, dest->name, src->copies, NODE_COPIES);
	}
}

static void
movecopy(File *f, Node *node)
{
	stp->itomove_n++;
	stp->itomove_b += f->size;
	if(node->deletes < NODE_RENAMES){
		sfprintf(sfstderr, "%d/%d rename copy of file %s on node %s\n", node->renames, NODE_RENAMES, f->name, node->name);
		if(explaining)
			sfprintf(explainfp, "%d/%d rename copy of file %s on node %s\n", node->renames, NODE_RENAMES, f->name, node->name);
		rprintf(node, "job: RM_RENAME pri 40 cmd ng_rm_rename_file %s %s %s\n", f->name, f->md5, f->rename);
		f->attempts = attempt(A_move, node, 0, now+LEASE_MOVE, f->attempts);
		node->count--;
		node->renames++;
		node->nr++;
	} else {
		if(explaining)
			sfprintf(explainfp, "rename copy of file %s on node %s not done (%d>=%d)\n", f->name, node->name, node->renames, NODE_RENAMES);
	}
}

static void
delcopy(File *f, Node *node, char *reason)
{
	stp->itodelete_n++;
	stp->itodelete_b += f->size;
	if((node->deletes < NODE_DELETES) && (nodeletes == 0)){
		delbytes += f->size;
		sfprintf(sfstderr, "%d/%d delete copy of file %s on node %s\n", node->deletes, NODE_DELETES, f->name, node->name);
		if(explaining)
			sfprintf(explainfp, "%d/%d delete copy of file %s on node %s\n", node->deletes, NODE_DELETES, f->name, node->name);
		rprintf(node, "job: RM_DELETE pri 45 cmd ng_rm_del_file %s %s repmgr=%s\n", f->name, f->md5, reason);
		f->attempts = attempt(A_destroy, node, 0, now+LEASE_DELETE, f->attempts);
		node->count--;
		node->deletes++;
		node->nd++;
		ndeletes++;
	} else {
		if(explaining)
			sfprintf(explainfp, "would delete %s on %s but slots full (%d/%d) or nodeletes set(%d)\n", f->name, node->name, node->deletes, NODE_DELETES, nodeletes);
	}
}

static int
choose_node(char *vec, int highest, int zero_ok)
{
	int j, k, nnn;
	double tot, m;
	struct { Node *n; int i; ng_uint32 avail; } nn[MAX_NODES];
#define	PFAC	1.5

	nnn = 0;
	for(j = 0; j < nnodes; j++)
		if(vec[j]){
			if(nodes[j]->avail > 0)
				nn[nnn].avail = nodes[j]->avail;
			else if(zero_ok)
				nn[nnn].avail = 100;
			else
				continue;
			nn[nnn].n = nodes[j];
			nn[nnn].i = j;
			nnn++;
		}
	if(nnn == 0)
		return -1;
	/*
		selection method: set nodes weight in range 0..tot
	*/
	/* first set weights */
	m = 0;
	k = 0;
	for(j = 0; j < nnn; j++){
		m += nn[j].avail;
		if(nn[j].avail > nn[k].n->total)
			k = j;
	}
	m /= nnn;
	if(m < .001) m = 0.001;
	if(highest){
		for(j = 0; j < nnn; j++)
			nn[j].n->weight = pow(nn[j].avail/m, PFAC);
	} else {
		for(j = 0; j < nnn; j++)
			nn[j].n->weight = pow((nn[k].avail-nn[j].avail)/m, PFAC);
	}
	for(j = 0; j < nnn; j++)
		if(nn[j].n->weight > PLIM)
			nn[j].n->weight = PLIM;
	for(j = 1; j < nnn; j++)
		nn[j].n->weight += nn[j-1].n->weight;
	if(0){
		sfprintf(sfstderr, "w:");
		for(j = 0; j < nnn; j++)
			sfprintf(sfstderr, " %s:%.1f", nn[j].n->name, nn[j].n->weight);
	}

	tot = nn[nnn-1].n->weight*drand48();
	for(j = 0; j < nnn; j++)
		if(tot < nn[j].n->weight){
			if(0){
				sfprintf(sfstderr, "; rand=%.1f found=%d(%s)\n", tot, nn[j].i, nn[j].n->name);
			}
			return nn[j].i;
		}
	return nn[nnn-1].i;
}

static void
m0(Syment *s, void *v)
{
	File *f = s->value;

	prune(f);
}

static int
nfind(Node *n)
{
	int j;

	for(j = 0; j < nnodes; j++)
		if(nodes[j] == n)
			return j;
	return -1;
}

static void
mgoal(File *f, char *src, char *vsrc, char *goal, char *must)
{
	Hint *h;
	Node *n;
	Instance *i;
	Attempt *a;
	int j, k;
	int found;
	int noccur, ncopies;
	int n_on, n_off, n_on_src, n_off_src;
	int real_noccur;
	char *p, *q;
	char junk[MAX_NODES];
	char tmp[NG_BUFFER];

	if(explaining){
		sfprintf(explainfp, "determine goal for file %s\n", f->name);
		prfile(f, explainfp, '\n');
	}
	if(verbose > 1)
		sfprintf(sfstderr, "determine goal for file %s: md5=%s noccur=%d\n", f->name, f->md5, f->noccur);
	/* initialise scratch areas */
	for(j = 0; j < nnodes; j++){
		n = nodes[j];
		n->hint_type = H_none;
		n->count = 0;
		src[j] = 0;
		goal[j] = Undecided;
		must[j] = Undecided;
		vsrc[j] = 0;
	}
	n_on = 0;
	n_off = 0;
	n_on_src = 0;
	n_off_src = 0;
	/* set up desired count */
	switch(f->noccur)
	{
	case REP_ALL:		noccur = nnodes; break;
	case REP_DEFAULT:	noccur = nreps; break;
	case REP_LDEFAULT:	noccur = nreps; break;
	case REP_WHATEVER:	noccur = -1; break;
	default:		noccur = f->noccur; break;
	}
	if(noccur > nnodes)
		noccur = nnodes;
	if(explaining)
		sfprintf(explainfp, "\tgoal is %d copies\n", noccur);
	f->ngoal = noccur;

	/* first do hood */
	for(p = f->hood; *p;){
		int htype;

		htype = H_must_on;
		if(*p == '!'){
			htype = H_must_off;
			p++;
		}
		if(explaining)
			sfprintf(explainfp, "\thood %s %s\n", hname[htype], p);
		if(isupper(*p))
			q = getvar(p);
		else
			q = p;
		if(explaining)
			sfprintf(explainfp, "\tgetvar(%s) = %x(%s)\n", p, q, (q? q:""));
		if(q == 0){		/* not a var, see if its an attr */
			tmp[0] = ' ';
			strcpy(tmp+1, p);
			strcat(tmp, " ");
			for(j = 0; j < nnodes; j++){
				if(nodes[j]->nattr && (strstr(nodes[j]->nattr, tmp) != 0)){
					if(nodes[j]->hint_type > htype)
						nodes[j]->hint_type = htype;
/*					break;*/
				}
			}
		} else {
			for(j = 0; j < nnodes; j++){
				if(strcmp(nodes[j]->name, q) == 0){
					if(nodes[j]->hint_type > htype)
						nodes[j]->hint_type = htype;
					break;
				}
			}
		}
		while(*p++)
			;
	}
	/* then hints */
	for(h = f->hints; h; h = h->next){
		for(j = 0; j < nnodes; j++)
			if(strcmp(nodes[j]->name, h->node) == 0){
				if(nodes[j]->hint_type > h->type)
					nodes[j]->hint_type = h->type;
				break;
			}
	}
	/* add in the copies */
	for(ncopies = 0, i = f->copies; i; i = i->next){
		if(i->node->count){
			if(explaining)
				sfprintf(explainfp, "\t\tskipping dup copy on node %s\n", i->node->name);
			continue;
		}
		i->node->count = 1;
		if(explaining)
			sfprintf(explainfp, "\t\tfound copy on node %s\n", i->node->name);
		ncopies++;
		if((j = nfind(i->node)) >= 0){
			src[j] = 1;
			vsrc[j] = 1;
		}
	}
	if(explaining)
		sfprintf(explainfp, "\t%d actual copies\n", ncopies);
	if(noccur < 0){
		noccur = ncopies;
		if(explaining)
			sfprintf(explainfp, "\tste noccur to %d\n", noccur);
	}
	/* finally, implement on-leader (overrides hints,hood) */
	if((f->noccur == REP_LDEFAULT) && (p = getvar(VAR_LEADER))){
		for(j = 0; j < nnodes; j++)
			if(strcmp(nodes[j]->name, p) == 0){
				nodes[j]->hint_type = H_must_on;
				break;
			}
	}
	/* add in the pending attempts */
	for(a = f->attempts; a; a = a->next){
		switch(a->type)
		{
		case A_create:
			if((j = nfind(a->to)) >= 0)
				vsrc[j] = 1;
			break;
		case A_destroy:
			if((j = nfind(a->from)) >= 0)
				vsrc[j] = 0;
			break;
		case A_move:
			break;
		}
	}
	if(explaining){
		sfprintf(explainfp, "\tactual and virtual (assume add/deletes work) copies\n", ncopies);
		for(j = 0; j < nnodes; j++)
			sfprintf(explainfp, "\t\t%s: a=%d v=%d goal=%s hint=%s\n", nodes[j]->name, src[j], vsrc[j], gname[goal[j]], hname[nodes[j]->hint_type]);
	}
	if((noccur == 0) || f->rename){
		for(j = 0; j < nnodes; j++)
			goal[j] = Off;
		return;		/* src, vsrc, goal is correct! */
	}

	/*
		what follows here is a surprising sequence of 15(!) passes to enforce the hoods/hints
		i'm amazed too.

		first, some macros. they're fairly self-explanatory.
		note that n_{on|off}_src means number of {On|Off}s where there is a src copy
	*/
#define	SET_ON(g) { switch(goal[g]){						\
	case On: break;								\
	case Off: n_off--; if(src[g]) n_off_src--;				\
	case Undecided: goal[g] = On; n_on++; if(src[g]) n_on_src++; break;	\
	} }
#define	SET_OFF(g) { switch(goal[g]){						\
	case Off: break;							\
	case On: n_on--; if(src[g]) n_on_src--;					\
	case Undecided: goal[g] = Off; n_off++; if(src[g]) n_off_src++; break;	\
	} }
	/* run over every node */
#define	forallnodes(expr, seton)				\
	for(j = 0; j < nnodes; j++){				\
		if(expr)					\
			if(seton) SET_ON(j) else SET_OFF(j)	\
	}
	/* loop while we make progress */
#define	loop(e1, e2, seton, zero_ok)					\
	{ for(found = 1; found && (e1); ){			\
		found = 0;					\
		for(j = 0; j < nnodes; j++) junk[j] = 0;	\
		for(j = 0; j < nnodes; j++)			\
			if(e2) junk[j] = 1;			\
		if((j = choose_node(junk, seton, zero_ok)) >= 0){	\
			found = 1;				\
			if(seton) SET_ON(j) else SET_OFF(j)	\
		}						\
	} }
#define	splat(str)	if(explaining){				\
		sfprintf(explainfp, "\tafter %s, on=%d off=%d ons=%d offs=%d", str, n_on, n_off, n_on_src, n_off_src); \
		for(j = 0; j < nnodes; j++)			\
			sfprintf(explainfp, " %s(a=%d v=%d goal=%s)", nodes[j]->name, src[j], vsrc[j], gname[goal[j]]); \
		sfprintf(explainfp, "\n");			\
		sfsync(explainfp);				\
	}

	/* set must_offs, first doing ones that are off, then others */
	forallnodes((!vsrc[j] && (nodes[j]->hint_type == H_must_off) && (n_off < (nnodes-noccur))), 0)
	loop((n_off < (nnodes-noccur)), ((goal[j] != Off) && (nodes[j]->hint_type == H_must_off)), 0, 1)
	splat("hood is off")

	/* set must_ons, first doing ones that are on, then others. note that must_on's override off's by coming after */
	forallnodes((vsrc[j] && (nodes[j]->hint_type == H_must_on) && (n_on < noccur)), 1)
	loop((n_on < noccur), ((goal[j] != On) && (nodes[j]->hint_type == H_must_on)), 1, 1)
	splat("hood is on")
	for(j = 0; j < nnodes; j++)
		must[j] = goal[j];

	/* set hint_ons, first doing ones that are on, then others. note that on's override off's by coming first here */
	forallnodes((vsrc[j] && (nodes[j]->hint_type == H_on) && (goal[j] == Undecided) && (n_on < noccur)), 1)
	loop((n_on < noccur), ((goal[j] == Undecided) && (nodes[j]->hint_type == H_on)), 1, 1)
	splat("hint is on")

	/* set hint_offs, first doing ones that are off, then others */
	k = n_off;	/* record how many we want to turn off */
	forallnodes((!vsrc[j] && (nodes[j]->hint_type == H_off) && (goal[j] == Undecided) && (n_off < (nnodes-noccur))), 0)
	loop((n_off < (nnodes-noccur)), ((goal[j] == Undecided) && (nodes[j]->hint_type == H_off)), 0, 1)
	k = n_on + (k-n_off);
	if(explaining)
		sfprintf(explainfp, "--n_on=%d n_off=%d k=%d\n", n_on, n_off, k);
	forallnodes(((n_on < k) && !src[j] && (goal[j] == Undecided)), 1)
	splat("hint is off + speculative copies")

	/* copy so we can implement hint_on's */
	loop(1, ((goal[j] == Undecided) && (nodes[j]->hint_type == H_on)), 1, 0)
	splat("speculative copies following on hints")

	/* ensure we always have noccur real copies; first easy way, then hard way */
	loop((n_on_src < noccur), (src[j] && (goal[j] == Undecided)), 1, 1)
	k = n_off;	/* need to make up any we reset */
	loop((n_on_src < noccur), (src[j] && (goal[j] == Off)), 1, 1)
	k = n_on + (k-n_off);
	if(explaining)
		sfprintf(explainfp, "--n_on=%d n_off=%d k=%d\n", n_on, n_off, k);
	forallnodes((n_on < k) && (vsrc[j] && (goal[j] == Undecided)), 1)
	loop((n_on < k), (goal[j] == Undecided), 1, 0)
	splat("ensure we have enough real copies")

	/* final, desperate, attempts to get our target (ignores hints and hoods which are already done) */
	loop((n_on < noccur), (vsrc[j] && (goal[j] == Undecided)), 1, 1)
	loop((n_on < noccur), (goal[j] == Undecided), 1, 1)
	loop((n_on < noccur), (goal[j] == Off), 1, 1)
	splat("final attempts to achieve target")

	/* nothing left to set, turn everthing else off */
	forallnodes((goal[j] == Undecided), 0)
	splat("set undecideds to off")
}

static void
mperf(File *f, char *src, char *vsrc, char *goal, char *must)
{
	int j, k, m, nactions, mand;
	char buf[NG_BUFFER], *p;

	if(explaining){
		sfprintf(explainfp, "\tperform changes(src,vsrc,goal):");
		for(j = 0; j < nnodes; j++)
			sfprintf(explainfp, " %s(%d,%d,%s)", nodes[j]->name, src[j], vsrc[j], gname[goal[j]]);
		sfprintf(explainfp, "\n");
	}
	p = buf;
	for(j = 0; j < nnodes; j++){
		sfsprintf(p, &buf[sizeof(buf)]-p, "_%s=%d,%d,%d", nodes[j]->name+4, src[j], vsrc[j], goal[j]);
		p = strchr(p, 0);
	}

	/* odd wart; dunno where this should go */
	if(f->copies == 0){
		stp->zerocopies_n++;
		if(explaining)
			sfprintf(explainfp, "\tno copies for file %s; bye\n", f->name);
		if(verbose > 1)
			sfprintf(sfstderr, "\tno copies for file %s; bye\n", f->name);
		if(f->noccur == 0){
			stp->match_n++;
			f->matched = 1;
			set_stable(f, 1);
			sfprintf(sfstderr, "deleting registration for %s; ncopies==0\n", f->name);
			delfile(f);
			stp->delete_n++;
		} else {
			f->matched = 0;
			set_stable(f, 0);
		}
		return;
	}
	/* second wart: check for case of rename */
	if(f->rename){
		if(f->attempts == 0){
			if(explaining)
				sfprintf(explainfp, "\trename of file %s; no attempts, so go!\n", f->name);
			/* if nothing pending, fork off renames */
			for(j = 0; j < nnodes; j++)
				if(nodes[j]->count > 0){
					movecopy(f, nodes[j]);
				}
		} else {
			if(explaining)
				sfprintf(explainfp, "\trename of file %s; attempts pending, so do nothing\n", f->name);
		}
		goto done;
	}
	/*
		for now, do it safe and slow
		do copies first, and only when all done, do we do deletes
	*/
	nactions = 0;
	for(j = 0; (j < nnodes) && (nactions < MAX_ACTIONS); j++){
		if(!src[j] && !vsrc[j] && (goal[j] == On)){
			/* choose least busy node k with src[k]==1 */
			k = -1;
			for(m = 0; m < nnodes; m++)
				if(src[m] && ((k < 0) || (nodes[m]->copies < nodes[k]->copies)))
					k = m;
			if(k >= 0){
				makecopy(f, nodes[k], nodes[j]);
				nactions++;
			}
		}
	}
	/* okay to do deletes/moves? */
	if(nactions == 0){
		int actual;

		/* How many instances do we have? */
		actual = 0;
		for(j = 0; j < nnodes; j++) {
			if(src[j] && vsrc[j])
				actual++;
		}

		/* Can we delete them without removing too many copies */
		for(j = 0; j < nnodes; j++) {
			if(src[j] && vsrc[j] && (goal[j] == Off) && (actual > f->ngoal)){
				delcopy(f, nodes[j], buf+1);
				actual--;
			}
		}
	}

done:
	/* do matched/stable calculation */
	k = 0;
	m = 1;
	mand = 1;
	for(j = 0; j < nnodes; j++){
		if(src[j])
			k++;
		if((goal[j] == On) && !src[j])
			m = 0;
		if((goal[j] == Off) && src[j])
			m = 0;
		if((must[j] == On) && !src[j])
			mand = 0;
		if((must[j] == Off) && src[j])
			mand = 0;
	}
	f->matched = (k >= f->ngoal) && mand;
	set_stable(f, f->matched && m);
	if(f->matched)
		stp->match_n++;
	if(f->stable)
		stp->stable_n++;
	if(explaining)
		sfprintf(explainfp, "\tstable=%d matched=%d (k=%d mand=%d m=%d)\n", f->stable, f->matched, k, mand, m);
}

static void
m1(Syment *s, void *v)
{
	int *pri = v;
	File *f = s->value;
	char src[MAX_NODES], vsrc[MAX_NODES], goal[MAX_NODES], must[MAX_NODES];

	if(f->urgent != (*pri > 0))
		return;
	explaining = explainfp && ((explain_name == 0) || (strcmp(f->name, explain_name) == 0));
	if(explaining){
		sfprintf(explainfp, "m1 file %s\n", f->name);
	}
	stp->file_n++;
	mgoal(f, src, vsrc, goal, must);
	mperf(f, src, vsrc, goal, must);
	if(explaining){
		sfprintf(explainfp, "---m1 file %s done\n", f->name);
	}
}

static void
m2(Syment *s, void *v)
{
	Node *n = s->value;

	/* first do nodelete stuff */
	if(n->nodelete == 1)
		n->nodelete = time(0)+1;
	if(n->nodelete)
		nodeletes = 1;
	/* check to see if this node has expired */
	if(n->lease < now){
		sfprintf(sfstderr, "expiring node %s: lease=%d now=%d\n", n->name, n->lease, now);
		symdel(syms, s->name, s->space);
		purge(n);
		delsortlist(n->mine);
		n->mine = 0;
		ng_free(n->name);
		if(n->rbuf)
			ng_free(n->rbuf);
		if(n->spacefslist)
			ng_free(n->spacefslist);
		return;
	}
	if(verbose > 1)
		sfprintf(sfstderr, "adding node %s\n", n->name);
	n->nd = n->nc = n->nr = 0;
	n->xc = n->xd = n->xr = 0;
	n->rn = 0;
	*np++ = n;
}

static void
m3(Syment *s, void *v)
{
	Node *n = s->value;
	char buf[NG_BUFFER];
	Sfio_t *fp;

/*
	if(n->nd)
		rprintf(n, "job: after RM_DELETE, cmd ng_flist_send\n");
*/
	if(n->rn > 0){
		sfsprintf(buf, sizeof buf, "ng_wreq -t %d -s %s", WOOMERA_TIMEOUT, n->name);
		if(!safemode && (fp = sfpopen(NULL, buf, "w"))){
			sfprintf(sfstderr, "closing %s req buf: %d deletes (%d new, %d", n->name, n->deletes, n->nd, n->xd);
			if(n->nodelete)
				sfprintf(sfstderr, ", no=%u", n->nodelete);
			sfprintf(sfstderr, "), %d copies (%d new, %d), %d renames (%d new, %d old)\n", n->copies, n->nc, n->xc, n->renames, n->nr, n->xr);
			sfwrite(fp, n->rbuf, n->rn);
			sfclose(fp);
		}
		if(safemode){
			sfprintf(sfstderr, "%s: would have done %d deletes (%d new, %d), %d copies (%d new, %d), %d renames (%d new, %d old)\n", n->name, n->deletes, n->nd, n->xd, n->copies, n->nc, n->xc, n->renames, n->nr, n->xr);
		}
		n->rn = 0;
	}
}

static void
counti(Syment *s, void *v)
{
	Instance *i, **li;

	for(li = (Instance **)&(s->value); i = *li; ){
		if(i->deleted){	/* skip/del this link */
			*li = i->next;
			delinstance(i);
		} else {
			li = &((*li)->next);
			stp->instance_n++;
		}
	}
	if(s->value == 0){	/* removed them all */
		char *name;

		name = s->name;
		symdel(syms, s->name, s->space);
		ng_free(name);
	}
}

void
munge(Stats *stptr)
{
	int pri;
	char *s;
	struct tms at, bt;
	clock_t a, b;

	stp = stptr;
	*stp = zerostats;
	now = time(0);
	a = times(&at);
	symtraverse(syms, SYM_INSTANCE, counti, 0);
	sfprintf(sfstderr, "munging: now=%u\n", now);
	if(explainfp)
		sfprintf(explainfp, "======= new munge now=%u\n", now);
	if(s = ng_env("RM_NREPS"))
		nreps = strtol(s, 0, 10);
	else
		nreps = 1;
	if(nodes == 0)
		nodes = ng_malloc(MAX_NODES*sizeof(nodes[0]), "node vec");
	np = nodes;
	ndeletes = 0;
	nodeletes = 0;
	/* first prep nodes */
	symtraverse(syms, SYM_NODE, m2, 0);
	nnodes = np - nodes;
	if(nreps > nnodes)
		nreps = nnodes;
	/* next prune stuff */
	symtraverse(syms, SYM_FILE, m0, 0);
	symtraverse(syms, SYM_FILE0, m0, 0);
	/* run through twice, once for each priority */
	sawtoken = 0;
	pri = 1;			/* urgent */
	symtraverse(syms, SYM_FILE, m1, &pri);
	symtraverse(syms, SYM_FILE0, m1, &pri);
	pri = 0;			/* not urgent */
	symtraverse(syms, SYM_FILE, m1, &pri);
	symtraverse(syms, SYM_FILE0, m1, &pri);
	symtraverse(syms, SYM_NODE, m3, 0);
	b = times(&bt);
	stp->pass_t = b-a;
	stp->pass_ut = bt.tms_utime - at.tms_utime;
	if(explainfp){
		sfprintf(explainfp, "==============================munge done\n");
		prstats(stp, explainfp);
		sfsync(explainfp);
	}
	prstats(stp, sfstderr);
	if(verbose > 1)
		prflow(sfstderr);
}

Attempt *
attempt(Attempt_type type, Node *from, Node *to, ng_uint32 lease, Attempt *next)
{
	Attempt *a;

	a = ng_malloc(sizeof(*a), "attempt");
	a->type = type;
	a->from = from;
	a->to = to;
	a->lease = lease;
	a->create = time(0);
	a->next = next;
	begin_attempt(a);
	return a;
}

void
delattempt(Attempt *a)
{
	ng_free(a);
}

void
explainf(char *file, char *name)
{
	if(file == 0){
		if(explainfp)
			sfclose(explainfp);
		if(explain_name)
			ng_free(explain_name);
		sfprintf(sfstderr, "explain off\n");
		explainfp = 0;
	} else {
		explain_name = name? ng_strdup(name) : 0;
		explainfp = sfopen(0, file, "w");
		sfprintf(sfstderr, "explaining %s to '%s'\n", name? name:"everything", file);
	}
}

void
prstats(Stats *stp, Sfio_t *sfp)
{
	if(cscale == 0)
		cscale = sysconf(_SC_CLK_TCK);
	sfprintf(sfp, "STATS[%d]: instance_n=%d file_n=%d match_n=%d stable_n=%d delete_n=%d zcps_n=%d copy(%d,%.2fMB) delete(%d,%.2fMB) move(%d,%.2fMB) pass_t=%.1fu,%.1fr nod=%d\n", time(0),
		stp->instance_n, stp->file_n, stp->match_n, stp->stable_n, stp->delete_n, stp->zerocopies_n,
		stp->itocopy_n, stp->itocopy_b*1e-6, stp->itodelete_n, stp->itodelete_b*1e-6,
		stp->itomove_n, stp->itomove_b*1e-6, stp->pass_ut/(float)cscale, stp->pass_t/(float)cscale,
		nodeletes);
}
