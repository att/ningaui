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
	Mods:	09 Oct 2005 (sd) : Added some code to prevent coredump if symtable
			entry has a null value
*/

#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<time.h>
#include	"ningaui.h"
#include	"woomera.h"

static void	explain_r(char *, char *);
static void	explain_n(Node *, char *);
static void	traverse_j(Node *, char *);
/*static FILE	*outfp = stderr; */
static FILE	*outfp = NULL;

#define		NPRINT		10		/* max number of prereqs to print */

void
explain(int nargs, char **args, FILE *out)
{
	int i;

	outfp = out;
	fprintf(outfp, "explaining");
	if(nargs == 0)
		fprintf(outfp, " everything!\n");
	else {
		for(i = 0; i < nargs; i++)
			fprintf(outfp, " %s", args[i]);
		fprintf(outfp, "\n");
	}
	if(nargs == 0){
		traverse_j(node_r, "");
	} else {
		for(i = 0; i < nargs; i++){
			if(isupper(args[i][0]))
				explain_r(args[i], "");
			else {
				Syment *sym;

				sym = symlook(syms, args[i], S_JOB, 0, SYM_NOFLAGS);
				if(sym == 0)
					fprintf(outfp, "job %s: currently unknown to %s\n", args[i], argv0);
				else
					explain_n(sym->value, "");
			}
		}
	}
}

static char *ststr[] = {
	"unknown",
	"not ready to run",
	"running",
	"finished (success)",
	"finished (failure)",
	"purged"
};
char *opstr[] = {
	"exit",
	"explain",
	"listr",
	"lists",
	"list",
	"dump",
	"save",
	"push",
	"pmin",
	"pmax",
	"pload",
	"limit",
	"purge",
	"run",
	"resume",
	"pause",	
	"verbose",
	"blocked",	
	"times"
};

static void
explain_n(Node *n, char *pre)
{
	int i;
	char *p;
	char buf[NG_BUFFER];

	if( !outfp )
		outfp = stderr;

	if( !n )			/* prevent core dump */
	{
		fprintf(outfp, "%sjob name maps to a null symbol table value!\n", pre );
		return;
	}

	if(n->expiry){
		p = ctime(&n->expiry);
		p[24] = 0;
		sprintf(buf, "%s (%d)", p, n->expiry);
		p = buf;
	} else
		p = "<none>";
	switch(n->type)
	{
	case N_job:
		fprintf(outfp, "%sjob %s[%d] status %s[%d] expires %s (del %d) %d\n", pre, n->name, n, ststr[n->status], n->status, p, n->delete, n->pid );
		fprintf(outfp, "%s   %d resources:", pre, n->nrsrcs);
		for(i = 0; i < n->nrsrcs; i++)
			fprintf(outfp, " %s", n->rsrcs[i]->name);
		fprintf(outfp, "\n");
		fprintf(outfp, "%s   %d prereqs:", pre, n->nprereqs);
		for(i = 0; i < n->nprereqs; i++){
			fprintf(outfp, " %s", node_name(n->prereqs[i].node));
			if(n->prereqs[i].any)
				fprintf(outfp, "(any)");
		}
		fprintf(outfp, "\n");
		break;
	case N_root:
		fprintf(outfp, "%sroot node[%d]:\n", pre, n);
		fprintf(outfp, "%s   %d prereqs:", pre, n->nprereqs);
		for(i = 0; i < n->nprereqs; i++){
			fprintf(outfp, " %s", node_name(n->prereqs[i].node));
			if(n->prereqs[i].any)
				fprintf(outfp, "(any)");
			if(i >= NPRINT-1){
				fprintf(outfp, " ... (%d others elided)\n", n->nprereqs-i-1);
				break;
			}
		}
		fprintf(outfp, "\n");
		break;
	case N_op:
		fprintf(outfp, "%soperator(%s)[%d]: status %s expires %s (del %d)\n", pre, opstr[n->op], n, ststr[n->status], p, n->delete);
		fprintf(outfp, "%s   %d prereqs:", pre, n->nprereqs);
		for(i = 0; i < n->nprereqs; i++){
			fprintf(outfp, " %s", n->prereqs[i].node->name);
			if(n->prereqs[i].any)
				fprintf(outfp, "(any)");
		}
		fprintf(outfp, "\n");
		break;
	case N_rsrc:
		fprintf(outfp, "%sresource(%s): %d references, %d running, limit=%d\n", pre,
			n->name, n->rsrc->count, n->rsrc->running, n->rsrc->limit);
		break;
	}
}

static void
explain_r(char *name, char *pre)
{
	Resource *r;
	Syment *sym;

	if( ! outfp )
		outfp = stderr;

	if((sym = symlook(syms, name, S_RESOURCE, 0, SYM_NOFLAGS)) == 0){
		fprintf(outfp, "%sno resource '%s' known\n", pre, name);
		return;
	}
	if( (r = sym->value) != NULL )
	{
		fprintf(outfp, "resource %s: limit=%d running=%d all=%d runable=%d paused=%d expiry=%d\n",
			r->name, r->limit, r->running, r->count, r->runable, r->paused, r->expiry);
	}
	else
		fprintf(outfp, "%sresource '%s' has null symtab value\n", pre, name);
}

static void
traverse_j(Node *n, char *pre)
{
	char buf[NG_BUFFER];
	int i;

	sprintf(buf, "%s\t", pre);
	explain_n(n, pre);
	for(i = 0; i < n->nprereqs; i++)
		traverse_j(n->prereqs[i].node, buf);
}

char *
node_name(Node *n)
{
	static char buf[NG_BUFFER];

	switch(n->type)
	{
	case N_root:
	case N_job:
	case N_rsrc:
		return n->name;
	case N_op:
		sprintf(buf, "op=%s", opstr[n->op]);
		return buf;
	}
	return "<que?>";
}
