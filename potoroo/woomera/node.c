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
	Mods:	08 Jul 2008 (sd) : Prevent resource running/count from going negative.
*/


#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	"ningaui.h"
#include	"woomera.h"

Node *
new_node(Node_Type t)
{
	Node *n;

	n = ng_malloc(sizeof *n, "alloc node");
	n->name = 0;
	n->type = t;
	n->delete = 0;
	n->status = J_not_ready;
	n->cmd = 0;
	n->out = 0;
	n->ready = 0;
	n->force = 0;
	n->onqueue = 0;
	n->expiry = 0;
	n->rsrc = 0;
	n->prereqs = 0;
	n->nprereqs = 0;
	n->aprereqs = 0;
	n->rsrcs = 0;
	n->nrsrcs = 0;
	n->arsrcs = 0;
	n->args = 0;
	n->nargs = 0;
	n->aargs = 0;
	n->pid = 0;
	return n;
}

void
ndel(Node *n, int reuse)
{
	int i;
	Syment *se;

	/* original code does not delete syment for job/resource nodes when the 
		node is deleted. I think it needs to if the reuse flag is 0.
	   This is called by prune() which actually frees the node.
	*/

	if(!reuse && n->name)				/* must take out of sym table if not reusing */
	{
		if( n->type == N_job  )
			symdel( syms, n->name, S_JOB );			
		else
			if( n->type == N_rsrc  )
				symdel( syms, n->name, S_RESOURCE );			

		ng_free(n->name);
	}

	if(n->cmd){
		ng_free(n->cmd);
		n->cmd = 0;
	}
	for(i = 0; i < n->nargs; i++)
		ng_free(n->args[i]);
	n->nargs = 0;
	if(reuse){
		n->nprereqs = 0;
		n->nrsrcs = 0;
	} else {
		if(n->args)
			ng_free(n->args);
		if(n->prereqs)
			ng_free(n->prereqs);
		if(n->rsrcs)
			ng_free(n->rsrcs);
	}
}

void
nset(Node *dest, Node *src)
{
	ndel(dest, 1);
	*dest = *src;
}

void
add_prereq(Node *n, Node *pnode, int pany)
{
	Prereq *p;

	if(n->aprereqs == 0){
		n->aprereqs = 3;
		n->prereqs = ng_malloc(n->aprereqs*sizeof(n->prereqs[0]), "node prereqs");
	} else if(n->nprereqs >= n->aprereqs){
		n->aprereqs *= 2;
		n->prereqs = ng_realloc(n->prereqs, n->aprereqs*sizeof(n->prereqs[0]), "node prereqs");
	}
	p = &n->prereqs[n->nprereqs++];
	p->any = pany;
	p->node = pnode;
}

void
add_rsrc(Node *n, Resource *rsrc)
{
	if(n->arsrcs == 0){
		n->arsrcs = 3;
		n->rsrcs = ng_malloc(n->arsrcs*sizeof(n->rsrcs[0]), "node rsrcs");
		if(n->nrsrcs != 0){
			fprintf(stderr, "AGH!!!! n=0x%x '%s' nrsrscs=%d: should be 0\n",
				n, n->name, n->nrsrcs);
			n->nrsrcs = 0;
		}
	} else if(n->nrsrcs >= n->arsrcs){
		n->arsrcs *= 2;
		n->rsrcs = ng_realloc(n->rsrcs, n->arsrcs*sizeof(n->rsrcs[0]), "node rsrcs");
	}
	if(rsrc == 0){
		fprintf(stderr, "AGH!!! adding 0 rsrc to 0x%x '%s' nrsrc=%d\n", n, n->name, n->nrsrcs);
	}
	n->rsrcs[n->nrsrcs++] = rsrc;
}

void
pop_rsrc(Node *n)
{
	int i;
	Resource *r;

	for(i = 0; i < n->nrsrcs; i++){
		r = n->rsrcs[i];
		r->count++;
		r->expiry = 0;
	}
}

void
inc_rsrc(Node *n, int running)
{
	int i;
	Resource *r;

	for(i = 0; i < n->nrsrcs; i++){
		r = n->rsrcs[i];
		if(running){
			r->running++;
			r->expiry = 0;
		} else
			r->runable++;
	}
}

void
dec_rsrc(Node *n, int running)
{
	int i;
	Resource *r;

	for(i = 0; i < n->nrsrcs; i++){
		r = n->rsrcs[i];
		if(running){
			if( (--r->running) < 0 )
				r->running = 0;				/* dont allow to go neg */
			if( (--r->count) < 0 )
				r->count = 0;
			if((r->count <= 0) && (r->expiry == 0))
				r->expiry = time(0) + LONG_TIMEOUT;
		} else
			if( (--r->runable) < 0 )
				r->runable = 0;
	}
}

void
add_arg(Node *n, char *arg)
{
	if(n->aargs == 0){
		n->aargs = 3;
		n->args = ng_malloc(n->aargs*sizeof(n->args[0]), "node args");
	}else if(n->nargs >= n->aargs){
		n->aargs *= 2;
		n->args = ng_realloc(n->args, n->aargs*sizeof(n->args[0]), "node args");
	}
	n->args[n->nargs++] = arg;
}

static Resource *resource_r;

Resource *
new_resource(char *name)
{
	Resource *r;

	for(r = resource_r; r; r = r->next)
		if(strcmp(r->name, name) == 0)
			return r;
	r = ng_malloc(sizeof(*r), "new resource");
	r->name = ng_strdup(name);
	r->limit = 1;
	r->running = 0;
	r->runable = 0;
	r->count = 0;
	r->expiry = 0;
	r->paused = 0;
	r->next = resource_r;
	resource_r = r;
	return r;
}

Resource *
resource(char *name)
{
	Resource *r;
	Syment *sym;

	if(sym = symlook(syms, name, S_RESOURCE, 0, SYM_NOFLAGS)){
		return sym->value;
	}
	r = new_resource(name);
	symlook(syms, r->name, S_RESOURCE, r, SYM_NOFLAGS);
	return r;
}

void
setpause(char *rid, int n)
{
	Resource *r;

	r = resource(rid);
	r->paused = n;
	if(verbose)
		fprintf(stderr, "%s: set %s.pause = %d\n", argv0, r->name, r->paused);
}

static Channel *channel_r;

Channel *
channel(FILE *fp)
{
	Channel *r, *r0;

	r0 = 0;
	for(r = channel_r; r; r = r->next){
		if(r->fp == fp)
			return r;
		if((r->fp == 0) && (r0 == 0))
			r0 = r;
	}
	if(r0)
		r = r0;
	else {
		r = ng_malloc(sizeof(*r), "new channel");
		r->next = channel_r;
		channel_r = r;
	}
	r->fp = fp;
	r->count = 0;
	return r;
}

void
inc_channel(Channel *c)
{
	c->count++;
}

void
dec_channel(Channel *c)
{
	c->count--;
	if(c->count < 0){
		fprintf(stderr, "%s: internal error: chan count %d < 0\n", argv0, c->count);
	} else if(c->count == 0){
		fclose(c->fp);
		c->fp = 0;
	}
}

void
dumpchannel(FILE *fp)
{
	Channel *r;

	for(r = channel_r; r; r = r->next){
		fprintf(fp, "chan %d: fp=%d(%d) count=%d\n", r, r->fp, (r->fp?r->fp->_file:0), r->count);
	}
}
