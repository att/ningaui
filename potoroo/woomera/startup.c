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
	Mod:	07 Oct 2005 (sd) : Added check to dump running to prevent core dump if 
			syment->val == NULL
*/
/*
#ifndef OS_SOLARIS
#include	<ast.h>
#else
#endif
*/

#include	<unistd.h>
#include	<string.h>
#include	<stdio.h>
#include	"ningaui.h"
#include	"woomera.h"
#define	puke
#ifdef	puke
#include	<dirent.h>
#endif

static int	eatfile(char *path);

void
startup(char *file, int doreq)
{
	char sbuf[NG_BUFFER];

	if(file == 0){
		file = sbuf;
		sprintf(file, "%s/%s", ng_rootnm( WOOMERA_NEST ), STARTUP);
		if(access(file, R_OK) < 0)
			file = 0;
	}
	if(file){
		if(eatfile(file))
			return;
	}

	if(doreq){
#ifdef	puke
		DIR *dirp;
		struct dirent *d;

		if(file){
			char *s;

			if(s = strrchr(file, '/'))
				*s = 0;
			else
				file = ".";
		} else
			file = ng_rootnm( WOOMERA_NEST );
		if((dirp = opendir(file)) == 0){
			perror(file);
			return;
		}
		while(d = readdir(dirp)){
			if(memcmp(d->d_name, REQ, strlen(REQ)) == 0){
				if(verbose > 1)
					fprintf(stderr, "scanning nest: found %s\n", d->d_name);
				sprintf(sbuf, "%s/%s", file, d->d_name);
				if(eatfile(sbuf))
					break;
			}
		}
		closedir(dirp);
#endif
	}
}

static int
eatfile(char *path)
{
	FILE *in, *out;

	if((in = fopen(path, "r")) == NULL){
		perror(path);
		return 1;
	}
	if((out = fopen("/dev/null", "r")) == NULL){
		perror("/dev/null");
		return 1;
	}
	if(verbose > 1)
		fprintf(stderr, "eatfile(%s)\n", path);
	if(grok(path, in, out) == 0){
		if(remove(path) < 0){
			perror(path);
			return 1;
		}
	}

	fclose(in);
	fclose(out);
	return 0;
}

static dumpflag;

static void
clr_d(Node *n)
{
	int i;

	n->delete = 0;
	for(i = 0; i < n->nprereqs; i++)
		clr_d(n->prereqs[i].node);
}

static void
dumpj(Node *n, FILE *fp)
{
	int i;

	switch(n->type)
	{
	case N_job:
		if(dumpflag)
			fprintf(fp, "job %s status=%d expires %d ready %d force %d onq %d pri %d", n->name, n->status, n->expiry, n->ready, n->force, n->onqueue, n->pri);
		else
			fprintf(fp, "job %s: status %d expires %d ready %d force %d pri %d", n->name, n->status, n->expiry, n->ready, n->force, n->pri);
		if(n->nrsrcs){
			for(i = 0; i < n->nrsrcs; i++)
				fprintf(fp, " %s", n->rsrcs[i]->name);
		}
		if(n->nprereqs){
			for(i = 0; i < n->nprereqs; i++){
				fprintf(fp, " after");
				if(n->prereqs[i].any)
					fprintf(fp, " anystatus");
				fprintf(fp, " %s,", node_name(n->prereqs[i].node));
			}
		}
		if(n->cmd)
			fprintf(fp, " cmd %s", n->cmd);
		fprintf(fp, "\n");
		break;
	case N_op:
		fprintf(fp, "operator %s: status %d expires %d ready %d (del %d)",
			opstr[n->op], n->status, n->expiry,n->ready,  n->delete);
		if(n->nprereqs){
			fprintf(fp, "; prereqs");
			for(i = 0; i < n->nprereqs; i++){
				fprintf(fp, " %s", node_name(n->prereqs[i].node));
				if(n->prereqs[i].any)
					fprintf(fp, "(any)");
			}
		}
		fprintf(fp, "\n");
		break;
	}
}

/* dump running job names */
static void dumprunning(Syment *sym, void *arg)
{
	Node *n = (Node *)sym->value;
	FILE *fp = (FILE *)arg;

	if( !n )			/* node block for the job name is gone */
		return;

	if( n->status == J_running )
		fprintf(fp, "running_job: %s \n", n->name );
}

static void
dumpr(Syment *sym, void *arg)
{
	Resource *r = (Resource *)sym->value;
	FILE *fp = (FILE *)arg;

	fprintf(fp, "resource %s: limit=%d paused=%d\n", r->name, r->limit, r->paused);
}

static void
dump_d(Node *n, FILE *fp)
{
	int i;

	if(n->delete == 0){
		dumpj(n, fp);
		n->delete = 1;
	}
	for(i = 0; i < n->nprereqs; i++)
		dump_d(n->prereqs[i].node, fp);
}

void
dump(FILE *fp)
{
	dumpflag = 1;
	symtraverse(syms, S_RESOURCE, dumpr, fp);
	clr_d(node_r);
	dump_d(node_r, fp);
}

dump_still_running( FILE *fp )
{
	symtraverse(syms, S_JOB, dumprunning, fp);
}

void
save(Node *n, char *file)
{
	char sbuf[NG_BUFFER];
	char *np;
	FILE *fp;

	np = ng_rootnm( WOOMERA_NEST );
	if(file == 0){
		file = sbuf;
		sprintf(file, "%s/%s", np, STARTUP);
	}
	if((fp = fopen(file, "w")) == NULL){
		/* bitch */
	}
	dumpflag = 0;
	symtraverse(syms, S_RESOURCE, dumpr, fp);
	clr_d(node_r);
	dump_d(node_r, fp);
	fclose(fp);
	free( np );
}
