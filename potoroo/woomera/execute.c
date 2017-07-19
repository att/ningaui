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
#include	<ctype.h>
#include	"ningaui.h"
#include	"woomera.h"

static void rpurge(char *s);

int
execute(Node *n)
{
	int i, k, okay;
	Node *nn;
	Prereq *p;
	Resource *r;

	if(verbose > 2)
		fprintf(stderr, "execute(%s) status=%d [%d]\n", NAME(n), n->status, n);
	/* quick test for not done */
	if((n->type != N_rsrc) && (n->status != J_not_ready))
		return 0;
	switch(n->type)
	{
	case N_root:
		/* just do all our descendants */
		for(i = 0; i < n->nprereqs; i++)
			if(k = execute(n->prereqs[i].node))
				return k;
		break;
	case N_job:
		if(!n->ready){
			/* assume its okay to execute unless we find out otherwise */
			okay = 1;
			for(i = 0; i < n->nprereqs; i++){
				p = &n->prereqs[i];
				if(k = execute(p->node))
					return k;
				if(p->node->status < J_finished_okay)
					okay = 0;
				if((p->node->status == J_finished_bad) && (p->any == 0))
					okay = 0;
			}
			if(okay){
				n->ready = 1;
				jqueue(n, 0);
			}
		}
		/* already scheduled; so nothing else to do */
		break;
	case N_op:
		/* assume its okay to execute unless we find out otherwise */
		if(verbose > 1)
			fprintf(stderr, "exec op %d\n", n->op);
		okay = 1;
		for(i = 0; i < n->nprereqs; i++){
			p = &n->prereqs[i];
			if(k = execute(p->node))
				return k;
			if(p->node->status < J_finished_okay)
				okay = 0;
			if((p->node->status == J_finished_bad) && (p->any == 0))
				okay = 0;
		}
		if(okay == 0)
			break;
		n->status = J_running;
		if(verbose > 1)
			fprintf(stderr, "peforming op %d\n", n->op);
		/* by default, expire all ops quickly */
		n->expiry = time(0)+SHORT_TIMEOUT;
		switch(n->op)
		{
		case O_exit:
			n->status = J_finished_okay;
			dec_channel(n->out);
			n->expiry = time(0)+LONG_TIMEOUT;
			fprintf(stderr, "exiting: t=%d\n", time(0));
/*			explain(0, 0, stderr);	*/
			return 1;
			break;
		case O_explain:
			explain(n->nargs, n->args, n->out->fp);
			dec_channel(n->out);
			break;
		case O_blocked:
			blocked(n->out->fp);
			dec_channel(n->out);
			break;
		case O_pause:
			for(i = 0; i < n->nargs; i++){
				setpause(n->args[i], 1);
			}
			n->expiry = time(0)+LONG_TIMEOUT;
			dec_channel(n->out);
			break;
		case O_resume:
			for(i = 0; i < n->nargs; i++){
				setpause(n->args[i], 0);
			}
			dec_channel(n->out);
			break;
		case O_purge:
			for(i = 0; i < n->nargs; i++){
				if(isupper(n->args[i][0]))
					rpurge(n->args[i]);
				else
					jpurge(n->args[i]);
			}
			dec_channel(n->out);
			break;
		case O_run:
		case O_push:
			for(i = 0; i < n->nargs; i++){
				jforce(n->args[i], n->op == O_push);
			}
			n->expiry = time(0)+LONG_TIMEOUT;
			break;
		case O_limit:
			jrlimit(n->args[0], strtol(n->args[1], 0, 10));
			break;
		case O_pmin:
			jpmin(strtol(n->args[0], 0, 10));
			break;
		case O_pmax:
			jpmax(strtol(n->args[0], 0, 10));
			break;
		case O_pload:
			jpload(strtol(n->args[0], 0, 10));
			break;
		case O_verbose:
			i = strtol(n->args[0], 0, 10);
			if(verbose || i)
				fprintf(stderr, "****verbose changed from %d to %d\n", verbose, i);
			verbose = i;
			break;
		case O_list:
			list(n->out->fp);
			dec_channel(n->out);
			break;
		case O_listr:
			listr(n->out->fp);
			dec_channel(n->out);
			break;
		case O_lists:
			lists(n->out->fp);
			dec_channel(n->out);
			break;
		case O_dump:
			dump(n->out->fp);
			dec_channel(n->out);
			break;
		case O_save:
			save(node_r, n->args[0]);
			n->expiry = time(0)+SHORT_TIMEOUT;
			break;
		case O_times:
			prtimes(n->args[0], n->out->fp);
			dec_channel(n->out);
			break;
		default:
			fprintf(stderr, "%s: unimplemented exec op %d!\n", argv0, n->op);
			ng_log(LOG_ERR, "%s: unimplemented exec op %d!\n", argv0, n->op);
			break;
		}
		n->status = J_finished_okay;
		break;
	case N_rsrc:
		r = n->rsrc;
		if(verbose > 1)
			fprintf(stderr, "exec rsrc %s: count=%d run=%d lim=%d\n", r->name, r->count, r->running, r->limit);
		n->status = (r->count > 0)? J_not_ready:J_finished_okay;
		break;
	}
	return 0;
}

static void
purge1(Syment *sym, void *arg)
{
	int i;
	Node *n = (Node *)sym->value;
	Resource *r = (Resource *)arg;

	switch(n->type)
	{
	case N_job:
		for(i = 0; i < n->nrsrcs; i++){
			if(r == n->rsrcs[i]){
				jpurge(sym->name);
				break;
			}
		}
		break;
	}
}

static void
rpurge(char *s)
{
	symtraverse(syms, S_JOB, purge1, resource(s));
}
