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

#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<time.h>
#include	"ningaui.h"
#include	"woomera.h"

/*
	pruning is straightforward. it is driven completely by expiry date.
	first (prune1), mark all delete fields either 0 or 1 (1== past expiry).
	second (prune2) depth-first, for all nodes where delete==0, set parent's delete=0
	third (prune3), add deleted nodes to del, remove from dag and set delete=2
	finally, free nodes in del
*/

static void	prune1(Node *n, time_t t);
static void	prune2(Node *n);
static int	prune3(Node *n);
static Node	*del;

void
prune(void)
{
	time_t t;
	int i;
	Node *n, *nn;

	t = time(0);
	if(verbose > 2){
		fprintf(stderr, "--pruning(t=%d)\n", t);
		explain(0, 0, stderr);
	}
	/* first, mark all potential delete candidates to go */
	if(verbose > 2)
		fprintf(stderr, "--prune1\n");
	prune1(node_r, t);
	/* second, mark all non-delete node's parents as not to go */
	node_r->delete = 0;
	if(verbose > 2)
		fprintf(stderr, "--prune2\n");
	prune2(node_r);
	/* thirdly, delete links */
	del = 0;
	if(verbose > 2)
		fprintf(stderr, "--prune3\n");
	prune3(node_r);
	/* finally, delete them */
	i = 0;
	for(n = del; n; n = nn){
		nn = n->next;
		if(verbose > 2)
			fprintf(stderr, "\tndel(%d) %s\n", n, node_name(n));
		ndel(n, 0);
		ng_free(n);
		i++;
	}
	if(i && (verbose > 1))
		fprintf(stderr, "--pruned %i nodes\n", i);
	if(verbose > 2)
		fprintf(stderr, "--pruning done\n");
}

static void
prune1(Node *n, time_t t)
{
	int i;

	n->delete = 0;
	if((n->expiry) && (n->expiry <= t)){
		n->delete = 1;
		if(verbose > 2)
			fprintf(stderr, "prune1(%d) del=%d\n", n, 1);
	}
	for(i = 0; i < n->nprereqs; i++)
		prune1(n->prereqs[i].node, t);
}

static void
prune2(Node *n)
{
	int i;
	int can_elide;
	Node *nn;

	for(i = 0; i < n->nprereqs; i++){
		nn = n->prereqs[i].node;
		prune2(nn);			/* depth first */
		if(nn->delete){
			can_elide = (nn->status == J_finished_okay) || ((nn->status == J_finished_bad) && n->prereqs[i].any) || (nn->status == J_purged);
			if(!can_elide){
				nn->delete = 0;
				if(verbose > 2)
					fprintf(stderr, "\tundelete(%d) because of %s(%d)\n", nn, node_name(n), n);
			}
		}
		if(nn->delete == 0){		/* a prereq is not ready to delete */
			n->delete = 0;
			if(verbose > 2)
				fprintf(stderr, "\tundelete(%d) because of %s(%d)\n", n, node_name(nn), nn);
		}
	}
}

/* return 1 iff we can prune this node */

static int
prune3(Node *n)
{
	int i, j;
	Node *nn;

	j = 0;
	for(i = 0; i < n->nprereqs; i++){
		if(prune3(n->prereqs[i].node) == 0)		/* if not pruned, add */
			n->prereqs[j++] = n->prereqs[i];
	}
	if((verbose > 2) && (n->nprereqs != j))
		fprintf(stderr, "\tprune3(%d) trimmed prereqs from %d to %d\n", n, n->nprereqs, j);
	n->nprereqs = j;

	if(n->delete == 1){
		n->next = del;
		del = n;
		n->delete = 2;
		if(verbose > 2)
			fprintf(stderr, "\tdelete(%d\n", n);
	}
	i = n->delete != 0;
	if(verbose > 2)
		fprintf(stderr, "\tprune3(%d) = %d (del = %d)\n", n, i, n->delete);
	return i;
}
