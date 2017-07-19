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

#include <string.h>
#include	<signal.h>
#include	<stdlib.h>
#include <sfio.h>
#include "ningaui.h"
#include "ng_ext.h"
#include "repmgr.h"

static File *fileof(char *name, char *md5, off_t size, int noccur);
static void mvfile(char *name, char *newname);
static int seeninstances = 0;
static Node *node_map[MAX_NODES];
static int node_map_n = 0;
static int shedi = 0;
static void replace(char *name, char *md5, ng_int64 size, int noccur);
static void resync(Node *n);
static void resyncall(void);
static int replayon = 0;		/* when replay is set, various commands are ignored; see manpage */
static void dump5(Sfio_t *fp);
void obliterate_node(Node *n);

int
parse(char *ibuf, int n, Sfio_t *ofp)
{
	Sfio_t *in;
	char buf[NG_BUFFER], md5[NG_BUFFER], buf1[NG_BUFFER];
	ng_uint32 noccur;
	Syment *s;
	char *base;
	char *keyw;
	off_t size;
	int x;
	int ret;
	time_t a, b;

#define	isbuf(kw)	(strcmp(buf, keyw = kw) == 0)
#define	PERR		{ Sfoff_t zz = sftell(in);\
				ng_log(LOG_ERR, "parse error: expected arg after '%s'\n", keyw);\
				sfprintf(sfstderr, "parse error: expected arg after '%s' near char %I*d\n", keyw, sizeof zz, zz);\
				goto resync;\
			}

	ret = INPUT_OKAY;
	a = time(0);
	nrequests++;
	if(verbose){
		memcpy(buf, ibuf, (n > NG_BUFFER)? NG_BUFFER:n);
		if(n < 200){
			if(n > 0)
				buf[n-1] = 0;
		} else {
			strcpy(&buf[200], " ...");
		}
		sfprintf(sfstderr, "%u parsing('%s') %d@x%x\n", (ng_uint32)time(0), buf, n, ibuf);
	}
	in = sfnew(0, ibuf, n, -1, SF_STRING|SF_READ);
	while(sfscanf(in, " %s", buf) == 1){
		if(verbose > 1)
			sfprintf(sfstderr, "\tkeyword '%s'\n", buf);
		if(isbuf("copylist")){
			if(sfscanf(in, "%s", buf) == 1){
				seeninstances = 1;
				copylist(buf);
			} else
				PERR
		} else if(isbuf("file")){
			if(sfscanf(in, "%s %s %I*d %d", md5, buf, sizeof(size), &size, &noccur) == 4){
				if(!quiet)
					ng_log(LOG_INFO, "REPMGR FILE %s %s %I*d %d", md5, buf, sizeof(size), size, noccur);
				SETBASE(base, buf);
				newfile(base, md5, size, noccur);
			} else
				PERR
		} else if(isbuf("replace")){
			if(sfscanf(in, "%s %s %I*d %d", md5, buf, sizeof(size), &size, &noccur) == 4){
				if(!quiet)
					ng_log(LOG_INFO, "REPMGR FILE %s %s %I*d %d", md5, buf, sizeof(size), size, noccur);
				SETBASE(base, buf);
				replace(base, md5, size, noccur);
			} else
				PERR
		} else if(isbuf("hint")){
			if(sfscanf(in, "%s %s %d %s", md5, buf, &noccur, buf1) == 4){
				char *p, *q, c;
				Hint_type t;
				Hint *h;
				File *f;

				SETBASE(base, buf);
				if(f = fileof(base, md5, -1, DFT_OCCUR)){
					h = f->hints;
					p = buf1;
					while(*p){
						if(*p == '!'){
							t = H_off;
							while(*++p == ' ')
								;
						} else
							t = H_on;
						for(q = p; *q; q++)
							if(*q == ',')
								break;
						c = *q;
						*q = 0;
						f->hints = hint(t, p, noccur);
						f->hints->next = h;
						p = c? q+1 : q;
					}
				} else
					sfprintf(sfstderr, "hint couldn't find file n=%s md5=%s\n", base, md5);
			} else
				PERR
		} else if(isbuf("urgent")){
			if(sfscanf(in, "%s", buf) == 1){
				if(s = symlook(syms, buf, SYM_FILE, 0, SYM_NOFLAGS))
					((File *)s->value)->urgent = 1;
				else
					sfprintf(sfstderr, "urgent(%s): no such file\n", buf);
			} else
				PERR
		} else if(isbuf("unfile")){
			if(sfscanf(in, "%s", buf) == 1){
				if(s = symlook(syms, buf, SYM_FILE, 0, SYM_NOFLAGS))
					delfile(s->value);
				else
					sfprintf(sfstderr, "unfile(%s): no such file\n", buf);
			} else
				PERR
		} else if(isbuf("mv")){
			if(sfscanf(in, "%s %s", buf, buf1) == 2){
				if(!quiet)
					ng_log(LOG_INFO, "REPMGR MV %s %s", buf, buf1);
				mvfile(buf, buf1);
			} else
				PERR
		} else if(isbuf("instance")){
			if(sfscanf(in, "%s %s %s", buf1, md5, buf) == 3){
				if(shedi == 0){
					addinstance(0, buf1, buf, md5, -1, 0, 0);
					seeninstances = 1;
				}
			} else
				PERR
		} else if(isbuf("uninstance")){
			if(sfscanf(in, "%s %s %s", buf1, md5, buf) == 3){
				if(shedi == 0){
					if(s = symlook(syms, buf1, SYM_NODE, 0, SYM_NOFLAGS))
						dropinstance(s->value, buf, md5);
				}
			} else
				PERR
		} else if(isbuf("hood")){
			if(sfscanf(in, "%s %s %s", md5, buf, buf1) == 3){
				char *s, *q;
				File *f;
				int j;

				SETBASE(base, buf);
				if(!quiet)
					ng_log(LOG_INFO, "REPMGR HOOD %s %s %s", md5, base, buf1);
				f = fileof(base, md5, -1, DFT_OCCUR);
				if(f == 0)
					goto loopbottom;
		addhood:
				if(f->hood)
					ng_free(f->hood);
				if(strcmp(buf1, VAR_NULL) == 0){
					f->hood = ng_strdup("");
				} else {
					j = strlen(buf1)+2;
					f->nhood = j;
					f->hood = ng_malloc(f->nhood, "hood");
					for(s = f->hood, q = buf1; *q; q++){
						*s++ = (*q == ',')? 0 : *q;
					}
					*s++ = 0;
					*s = 0;
				}
			} else
				PERR
		} else if(isbuf("nhood")){
			File *f;

			if(sfscanf(in, "%s %s", buf, buf1) == 2){
				SETBASE(base, buf);
				if(!quiet)
					ng_log(LOG_INFO, "REPMGR NHOOD %s %s", base, buf1);
				if(s = symlook(syms, base, SYM_FILE, 0, SYM_NOFLAGS)){
					f = s->value;
					goto addhood;
				} else
					sfprintf(sfstderr, "nhood(%s=%s): no such file\n", buf, base);
			} else
				PERR
		} else if(isbuf("dump")){
			if(sfscanf(in, "%s", buf) == 1)
				dump(buf);
			else
				PERR
		} else if(isbuf("munge")){
			if(sfscanf(in, "%d", &x) == 1)
				enablemunge(x);
			else
				PERR
		} else if(isbuf("safemode")){
			if(sfscanf(in, "%d", &x) == 1){
				safemode = x;
				sfprintf(sfstderr, "reset safemode to %d\n", safemode);
			} else
				PERR
		} else if(isbuf("replay")){
			if(sfscanf(in, "%s", buf) == 1){
				if(strcmp(buf, "begin") == 0)
					replayon = 1;
				else if(strcmp(buf, "end") == 0)
					replayon = 0;
				else
					sfprintf(sfstderr, "replay(%s): unknown type\n", buf);
			} else
				PERR
		} else if(isbuf("resync")){
			if(sfscanf(in, "%s", buf) == 1){
				if(strcmp(buf, "all") == 0)
					resyncall();
				else if(s = symlook(syms, buf, SYM_NODE, 0, SYM_NOFLAGS))
					resync(s->value);
				else
					sfprintf(sfstderr, "resync(%s): unknown node\n", buf);
			} else
				PERR
		} else if(isbuf("obliterate")){
			if(sfscanf(in, "%s", buf) == 1){
				if(s = symlook(syms, buf, SYM_NODE, 0, SYM_NOFLAGS))
					obliterate_node(s->value);
				else
					sfprintf(sfstderr, "resync(%s): unknown node\n", buf);
			} else
				PERR
		} else if(isbuf("shed")){
			if(sfscanf(in, "%d", &x) == 1){
				shedi = x;
				sfprintf(sfstderr, "reset shed to %d\n", shedi);
			} else
				PERR
		} else if(isbuf("set")){
			if(sfscanf(in, "%s %s", buf, buf1) == 2){
				setvar(buf, buf1);
			} else
				PERR
		} else if(isbuf("unset")){
			if(sfscanf(in, "%s", buf) == 1){
				unsetvar(buf);
			} else
				PERR
		} else if(isbuf("trace")){
			if(sfscanf(in, "%s", buf) == 1){
				if(strcmp(buf, "off") == 0){
					sfprintf(sfstderr, "tracing turned off\n");
					if(trace_name){
						ng_free(trace_name);
						trace_name = 0;
					}
				} else {
					if(trace_name != 0)
						ng_free(trace_name);
					trace_name = ng_strdup(buf);
					sfprintf(sfstderr, "now tracing(%s)\n", trace_name);
				}
			} else
				PERR
		} else if(isbuf("sleep")){
			if(sfscanf(in, "%d", &x) == 1){
				if(x < 0) x = 0;
				else if(x > 3600) x = 3600;
				sfprintf(sfstderr, "sleeping %ds\n", x);
				sleep(x);
				sfprintf(ofp, "slept %ds\n", x);
			} else
				PERR
		} else if(isbuf("quiet")){
			if(sfscanf(in, "%d", &x) == 1){
				quiet = x;
				sfprintf(sfstderr, "reset quiet to %d\n", quiet);
			} else
				PERR
		} else if(isbuf("checkpoint")){
			checkpoint(0);
		} else if(isbuf("symstats")){
			symstat(syms, sfstderr);
		} else if(isbuf("explain")){
			switch(sfscanf(in, "%s %s", buf, buf1))
			{
			case 2:		explainf(isbuf("off")? 0:buf, buf1); break;
			case 1:		explainf(isbuf("off")? 0:buf, 0); break;
			default:	PERR
			}
		} else if(isbuf("verbose")){
			if(sfscanf(in, "%d", &x) == 1){
				verbose = x;
				sfprintf(sfstderr, "reset verbose to %d\n", verbose);
			} else
				PERR
		} else if(isbuf("comment")){
			int c;

			sfprintf(sfstderr, "comment: ");
			while((c = sfgetc(in)) != EOF){
				sfputc(sfstderr, c);
				if(c == '\n')
					break;
			}
		} else if(isbuf("active")){
			active(ofp);
		} else if(isbuf("dump1")){
			if(sfscanf(in, "%s", buf) == 1){
				if(s = symlook(syms, buf, SYM_FILE, 0, SYM_NOFLAGS)){
					prfile(s->value, ofp, 0);
					sfputc(ofp, '\n');
				} else {
					/* sfprintf(sfstderr, "dump1(%s): no such file\n", buf); */
					sfprintf(ofp, "not found: %s\n", buf);
				}
			} else
				PERR
		} else if(isbuf("dump2")){
			if(sfscanf(in, "%s %s", md5, buf) == 2){
				s = symlook(syms, buf, SYM_FILE, 0, SYM_NOFLAGS);
				if(s && (strcmp(((File *)s->value)->md5, md5) == 0)){
					prfile(s->value, ofp, 0);
					sfputc(ofp, '\n');
				} else {
					sfprintf(sfstderr, "dump2(%s,%s): no such file\n", md5, buf);
					sfprintf(ofp, "not found: %s %s\n", md5, buf);
				}
			} else
				PERR
		} else if(isbuf("dump5")){
			dump5(ofp);
		} else if(isbuf("exit")){
			ret = INPUT_EXIT;
		} else {
			int c;
			Sfoff_t wp;
			Sfio_t *fp;

			wp = sftell(in);
			sfprintf(sfstderr, "parse error: unknown cmd '%s'[%2.2x,...] at %I*d\n", buf, buf[0]&0xFF, sizeof(wp), wp);
			ng_log(LOG_ERR, "parse error: unknown cmd '%s'\n", buf);
			if(fp = sfopen(0, "/tmp/wunk", "w")){
				sfwrite(fp, ibuf, n);
				sfclose(fp);
			}
resync:
			/* can't afford to skip all input on startup; just go to newline */
			while((c = sfgetc(in)) != EOF){
				if(c == '\n')
					break;
			}
		}
loopbottom:;
	}
	b = time(0);
	if(verbose)
		sfprintf(sfstderr, "done parsing (%ds) ret=%d==================\n", b-a, ret);
	/* close of ofp presumed done by caller */
	return ret;
}

static void
xxprunef(Syment *s, void *v)
{
	Node *n = v;
	File *f = s->value;
	Instance **li, *i;

	for(li = &f->copies; i = *li; ){
		if(i->node == n){	/* skip/del this link */
			*li = i->next;
			delinstance(i);
		} else
			li = &((*li)->next);
	}
}

static void
prunef(Syment *s, void *v)
{
	Node *n = v;
	File *f = s->value;
	Instance *i;

	for(i = f->copies; i; i = i->next){
		if(i->node == n)	/* skip/del this link */
			i->deleted = 1;
	}
}

static void
xxprunei(Syment *s, void *v)
{
	Node *n = v;
	Instance *i, **li;

	for(li = (Instance **)&(s->value); i = *li; ){
		if(i->node == n){	/* skip/del this link */
			*li = i->next;
			delinstance(i);
		} else
			li = &((*li)->next);
	}
	if(s->value == 0){	/* removed them all */
		char *name;

		name = s->name;
		symdel(syms, s->name, s->space);
		ng_free(name);
	}
}

static void
prunei(Syment *s, void *v)
{
	Node *n = v;
	Instance *i;

	for(i = (Instance *)(s->value); i; i = i->next){
		if(i->node == n)	/* skip/del this link */
			i->deleted = 1;
	}
}

int
node_index(char *name, Node *n)
{
	int i;

	for(i = 0; i < node_map_n; i++)
		if(node_map[i] && (strcmp(node_map[i]->name, name) == 0)){
			if(n == 0)
				node_map[i] = n;
			return i;
		}
	for(i = 0; i < node_map_n; i++)
		if(node_map[i] == 0){
			node_map[i] = n;
			return i;
		}
	node_map[node_map_n++] = n;
	return node_map_n-1;
}

Node *
new_node(char *noden)
{
	Node *node;

	node = ng_malloc(sizeof (*node), "new node");
	node->avail = 0;
	node->total = 0;
	node->lease = time(0) + 600;
	node->name = ng_strdup(noden);
	node->nodelete = 1;		/* reset in munge */
	node->rlen = NG_BUFFER;
	node->rbuf = ng_malloc(node->rlen, "node buffer");
	node->rn = 0;
	node->mine = newsortlist();
	node->spacefslist = 0;
	node->nattr = 0;
	node->copies = 0;
	node->deletes = 0;
	node->renames = 0;
	node->hand = 0;
	node->purgeme = 0;
	symlook(syms, node->name, SYM_NODE, node, SYM_NOFLAGS);
	node->index = node_index(node->name, node);
sfprintf(sfstderr, "node %s at %x\n", node->name, node);
	return node;
}

void
obliterate_node(Node *n)
{
	purge(n);
	delsortlist(n->mine);
	node_index(n->name, 0);
	symdel(syms, n->name, SYM_NODE);
	ng_free(n->name);
	ng_free(n->rbuf);
	ng_free(n);
}

void
addinstance(Node *node, char *noden, char *filen, char *md5, ng_int64 size, int *pni, int *pnf)
{
	Syment *s;
	Instance *i, *ii;
	char *base;
	int tracing;

	/* first, get node */
	if(node == 0){
		if((s = symlook(syms, noden, SYM_NODE, 0, SYM_NOFLAGS)) == 0){
			node = new_node(noden);
			if(verbose)
				sfprintf(sfstderr, "new node %s from instance(%s)\n", node->name, filen);
		} else
			node = s->value;
	}
	SETBASE(base, filen);
	tracing = trace_name && (strcmp(base, trace_name) == 0);
	if(tracing)
		sfprintf(sfstderr, "tracing(%s, addi, %s):", trace_name, md5);
	s = symlook(syms, base, SYM_FILE, 0, SYM_NOFLAGS);
	if(verbose > 2)
		sfprintf(sfstderr, "%s->%s: sym=%d md5cmp=%d\n", filen, base, s, s? strcmp(md5, ((File *)s->value)->md5) : 0);
	if(s && (strcmp(md5, ((File *)s->value)->md5) == 0)){
		Instance *i;
		File *file = s->value;

		if(tracing)
			sfprintf(sfstderr, " found file:");
		/* first check to see if its there */
		for(i = file->copies; i; i = i->next)
			if(i->node == node){
				if(i->deleted){
					i->deleted = 0;
					i->size = size;
					if(tracing)
						sfprintf(sfstderr, " undel(%s)", i->node->name);
				}
				if(i->size < 0)
					i->size = size;
				if(tracing)
					sfprintf(sfstderr, " inst(found %s)", i->node->name);
				/* if not deleted, its a dup */
				break;
			}
		if(i == 0){
			/* gen a new instance */
			i = file->copies;
			file->copies = instance(node, filen, md5, size);
			file->copies->next = i;
			if(tracing)
				sfprintf(sfstderr, " inst(new %s)", node->name);
		}
		if(file->size < 0)
			file->size = size;
		if(pnf) (*pnf)++;
	} else {
		/* no file, enter it as an unassociated instance */
		ii = instance(node, filen, md5, size);
		if(s = symlook(syms, base, SYM_INSTANCE, 0, SYM_NOFLAGS)){
			/* make sure its not there already */
			for(i = s->value; i; i = i->next)
				if((strcmp(ii->md5, i->md5) == 0) && (i->node == ii->node)){
					if(i->size < 0)
						i->size = ii->size;
					delinstance(ii);
					break;
				}
			if(i == 0){
				ii->next = s->value;
				s->value = ii;
				if(tracing)
					sfprintf(sfstderr, " appending to unatt inst");
			} else {
				if(tracing)
					sfprintf(sfstderr, " didn't append to unatt inst(dup)");
			}
		} else {
			SETBASE(base, ii->path);
			symlook(syms, ng_strdup(base), SYM_INSTANCE, ii, SYM_NOFLAGS);
			if(tracing)
				sfprintf(sfstderr, " adding new unatt inst");
		}
		if(pni) (*pni)++;
	}
	if(tracing)
		sfprintf(sfstderr, "\n");
}

static void
resync(Node *n)
{
	if(verbose)
		sfprintf(sfstderr, "--resync(%s)\n", n->name);
	n->nodelete = 1;
}

static void
r1(Syment *s, void *v)
{
	resync(s->value);
}

static void
resyncall(void)
{
	if(verbose)
		sfprintf(sfstderr, "resync all nodes:\n");
	symtraverse(syms, SYM_NODE, r1, 0);
}

void
purge(Node *node)
{
	/* remove all instance info for this node */
	symtraverse(syms, SYM_FILE, prunef, node);
	symtraverse(syms, SYM_INSTANCE, prunei, node);
}

static Instance *freei = 0;

Instance *
instance(Node *node, char *path, char *md5, ng_int64 size)
{
	Instance *i;

	if(freei){
		i = freei;
		freei = freei->next;
	} else
		i = ng_malloc(sizeof(*i), "instance");
	i->node = node;
	i->path = ng_strdup(path);
	i->md5 = ng_strdup(md5);
	i->size = size;
	i->deleted = 0;
	i->next = 0;
	return i;
}

void
delinstance(Instance *i)
{
	ng_free(i->path);
	ng_free(i->md5);
	i->next = freei;
	freei = i;
}

void
dropinstance(Node *node, char *filen, char *md5)
{
	Syment *s;
	Instance *i, *ii;
	char *base;
	int tracing;

	SETBASE(base, filen);
	tracing = trace_name && (strcmp(base, trace_name) == 0);
	if(tracing)
		sfprintf(sfstderr, "tracing(%s, dropi):", trace_name);
	s = symlook(syms, base, SYM_FILE, 0, SYM_NOFLAGS);
	if(s && (strcmp(md5, ((File *)s->value)->md5) == 0)){
		Instance *i;
		File *file = s->value;

		if(tracing)
			sfprintf(sfstderr, " reg:");
		for(i = file->copies; i; i = i->next)
			if(i->node == node){
				i->deleted = 1;
				if(tracing)
					sfprintf(sfstderr, " del(%s)", i->node->name);
				break;
			}
	} else {
		/* no file, check as an unassociated instance */
		if(tracing)
			sfprintf(sfstderr, " no reg:");
		if(s = symlook(syms, base, SYM_INSTANCE, 0, SYM_NOFLAGS)){
			if(tracing)
				sfprintf(sfstderr, " insts:");
			for(i = s->value; i; i = i->next)
				if(i->node == node){
					i->deleted = 1;
					if(tracing)
						sfprintf(sfstderr, " del(%s)", i->node->name);
					break;
				}
		}
	}
	if(tracing)
		sfprintf(sfstderr, "\n");
}

static void
assigni(Syment *s, void *v)
{
	File *f = v;
	Instance *i, *j, **li;

	for(li = (Instance **)&(s->value); i = *li; ){
		if((strcmp(s->name, f->name) == 0) && (strcmp(i->md5, f->md5) == 0)){	/* skip/del this link */
			*li = i->next;
			/* instance chain repaired, now add to file */
			/* first check its not there */
			for(j = f->copies; j; j = j->next){
				if(j->node == i->node){
					/* is there!! fix size if needed */
					if(j->size < 0)
						j->size = i->size;
					break;
				}
			}
			if(j == 0){
				i->next = f->copies;
				f->copies = i;
			}
		} else
			li = &((*li)->next);
	}
	if(s->value == 0){	/* removed them all */
		char *name;

		name = s->name;
		symdel(syms, s->name, s->space);
		ng_free(name);
	}
}

File *
newfile(char *name, char *md5, ng_int64 size, int noccur)
{
	Syment *s;
	File *f;
	char *ss;
	Instance *i, *ni;

	SETBASE(ss, name);
	name = ss;
	if(s = symlook(syms, name, SYM_FILE, 0, SYM_NOFLAGS)){
		/* already there! */
		f = s->value;
		if(strcmp(f->md5, md5) == 0){
			/* same file, reset noccur and go away */
			f->noccur = noccur;
			f->size = size;
			ungoal(f);
			return f;
		} else {
			/* new file; release instances */
			Instance *i, *ni;

			ungoal(f);
			ng_free(f->md5);
			f->md5 = ng_strdup(md5);
			f->noccur = noccur;
			f->size = size;
			ni = f->copies;
			f->copies = 0;
			while(i = ni){
				ni = i->next;
				addinstance(i->node, i->node->name, i->path, i->md5, size, 0, 0);
				delinstance(i);
			}
			/* fall thru to get any current instances */
		}
	} else {
		/* create a new file */
		f = file(name, md5, size, noccur);
		symlook(syms, ng_strdup(f->name), SYM_FILE, f, SYM_NOFLAGS);
		if(verbose > 2)
			sfprintf(sfstderr, "new file '%s'\n", f->name);
	}
	/* we have f; go scavenge for instances */
	if(seeninstances){
		symtraverse(syms, SYM_INSTANCE, assigni, f);
	}
	return f;
}

static void
replace(char *name, char *md5, ng_int64 size, int noccur)
{
	Syment *s;
	File *f;
	char *ss;
	Instance *i, *ni;

	SETBASE(ss, name);
	name = ss;
	if(s = symlook(syms, name, SYM_FILE, 0, SYM_NOFLAGS)){
		/* already there! */
		f = s->value;
		if(strcmp(f->md5, md5) == 0){
			/* same file, reset noccur and go away */
			f->noccur = noccur;
			f->size = size;
			ungoal(f);
			return;
		} else {
			/* new file; release instances */
			Instance *i, *ni;

			ungoal(f);
			ng_free(f->md5);
			f->md5 = ng_strdup(md5);
			f->noccur = noccur;
			f->size = size;
			ni = f->copies;
			f->copies = 0;
			while(i = ni){
				ni = i->next;
				addinstance(i->node, i->node->name, i->path, i->md5, size, 0, 0);
				delinstance(i);
			}
			/* fall thru to get any current instances */
		}
	} else {
		/* create a new file */
		f = file(name, md5, size, noccur);
		symlook(syms, ng_strdup(f->name), SYM_FILE, f, SYM_NOFLAGS);
		if(verbose > 2)
			sfprintf(sfstderr, "new file '%s'\n", f->name);
	}
	/* we have f; go scavenge for instances */
	if(seeninstances){
		symtraverse(syms, SYM_INSTANCE, assigni, f);
	}
}

void
delfile(File *f)
{
	Instance *i, *ni;
	Syment *s;
	char *name;

	s = symlook(syms, f->name, SYM_FILE, 0, SYM_NOFLAGS);
	name = s->name;
	symdel(syms, s->name, SYM_FILE);
	ng_free(name);
	ni = f->copies;
	while(i = ni){
		ni = i->next;
		addinstance(i->node, i->node->name, i->path, i->md5, i->size, 0, 0);
		delinstance(i);
	}
	if(f->hood)
		ng_free(f->hood);
	ng_free(f->md5);
	ng_free(f->name);
	if(f->rename)
		ng_free(f->rename);
	ng_free(f);
}

static void
mvfile(char *fname, char *newfname)
{
	Syment *s;
	File *f, *n;
	char *name, *newname;

	/*
		strategy is straighforward
		a) generate a new file with same properties as teh old one
		b) set rename for this one
		c) let munge do the rest
	*/
	SETBASE(name, fname);
	SETBASE(newname, newfname);
	if((s = symlook(syms, name, SYM_FILE, 0, SYM_NOFLAGS)) == 0){
		sfprintf(sfstderr, "mv(%s): no such file\n", name);
		return;
	}
	f = s->value;
	/* if target already exists, expunge it; leave instances -- they might fit! */
	if((s = symlook(syms, newname, SYM_FILE, 0, SYM_NOFLAGS))){
		delfile(s->value);
	}
	n = newfile(newname, f->md5, f->size, f->noccur);
	n->matched = 0;
	if(f->rename)
		ng_free(f->rename);
	f->rename = ng_strdup(newname);
	n->nhood = f->nhood;
	n->hood = ng_malloc(n->nhood, "mvhood");
	memcpy(n->hood, f->hood, n->nhood);
}

File *
fileof(char *name, char *md5, off_t size, int noccur)
{
	Syment *s;
	File *f;
	char *b;

	SETBASE(b, name);
	if(s = symlook(syms, b, SYM_FILE, 0, SYM_NOFLAGS)){
		f = s->value;
		if(strcmp(f->md5, md5) != 0)
			return 0;
	} else {
		/* create a new file */
		f = file(b, md5, size, noccur);
		symlook(syms, ng_strdup(f->name), SYM_FILE, f, SYM_NOFLAGS);
	}
	return f;
}

File *
file(char *name, char *md5, off_t size, int noccur)
{
	File *f;

	f = ng_malloc(sizeof(*f), "file");
	f->name = ng_strdup(name);
	f->md5 = ng_strdup(md5);
	f->rename = 0;
	f->token = 0;
	f->size = size;
	f->matched = 0;
	f->stable = -1;
	f->noccur = noccur;
	f->copies = 0;
	f->attempts = 0;
	f->urgent = 0;
	f->hints = 0;
	f->hood = ng_strdup("");
	f->nhood = 1;
	f->goal = 0;
	f->ngoal = 0;
	return f;
}

void
ungoal(File *f)
{
	if(f->goal){
		ng_free(f->goal);
		f->goal = 0;
	}
	f->ngoal = 0;
}

static void
printh(Sfio_t *fp, Hint *h, char *pre, char sep)
{
	static char *lab[]= { "on", "not on" };

	for(; h; h = h->next){
		sfprintf(fp, "%shint: %s %s %u", pre, lab[h->type], h->node, h->lease);
		if(sep)
			sfputc(fp, sep);
	}
}

static void
printi(Sfio_t *fp, Instance *i, char *pre1, char *prerest, int prmd5, int prsize, char sep)
{
	for(; i; i = i->next){
		sfprintf(fp, "%s%s %s", pre1, i->node->name, i->path);
		if(prmd5)
			sfprintf(fp, " %s", i->md5);
		if(prsize)
			sfprintf(fp, " %I*d", sizeof(i->size), i->size);
		if(i->deleted)
			sfprintf(fp, " **deleted**");
		if(sep)
			sfputc(fp, sep);
		pre1 = prerest;
	}
}

static void
printa(Sfio_t *fp, Attempt *a, char *pre1, char *prerest, char sep)
{
	static char *lab[] = { "create", "destroy", "move" };

	for(; a; a = a->next){
		sfprintf(fp, "%sattempt to %s a copy from %s to %s lease=%u", pre1, lab[a->type], a->from->name, a->to? a->to->name:"<none>", a->lease);
		if(sep)
			sfputc(fp, sep);
		pre1 = prerest;
	}
}

static void
dumpv(Syment *s, void *v)
{
	Sfio_t *fp = v;

	sfprintf(fp, "%s := %s\n", s->name, s->value);
}

static void
dumpf(Syment *s, void *v)
{
	Sfio_t *fp = v;
	File *f = s->value;

	prfile(f, fp, '\n');
}

void
prgoal(char *label, char *g, Sfio_t *fp)
{
	int first = 1;
	int i;

	sfprintf(fp, "%s=", label);
	for(i = 0; i < MAX_NODES; i++)
		if(g[i] != Off){
			if(first)
				first = 0;
			else
				sfprintf(fp, ",");
			if(g[i] == Undecided)
				sfprintf(fp, "?");
			if(node_map[i] && node_map[i]->name)
				sfprintf(fp, "%s", node_map[i]->name);
			else
				sfprintf(fp, "<nonode>");
		}
}

void
prfile(File *f, Sfio_t *fp, char sep)
{
	sfprintf(fp, "file %s: md5=%s size=%I*d noccur=%d token=%d matched=%d stable=%d", f->name, f->md5, sizeof(f->size), f->size, f->noccur, f->token, f->matched, f->stable);
	if(f->urgent)
		sfprintf(fp, " urgent");
	if(f->hood[0]){
		char *s, sep = '=';

		sfprintf(fp, " hood");
		for(s = f->hood; *s; s = strchr(s, 0)+1){
			sfprintf(fp, "%c%s", sep, s);
			sep = ',';
		}
	}
	if(f->rename)
		sfprintf(fp, " rename=%s", f->rename);
	if(f->goal)
		prgoal("goal", f->goal, fp);
	if(sep)
		sfputc(fp, sep);
	printh(fp, f->hints, sep? "\t":"#", sep);
	printi(fp, f->copies, (sep? "\t":"#"), (sep? "\t":"#"), 0, 0, sep);
	printa(fp, f->attempts, (sep? "\t":"#"), (sep? "\t":"#"), sep);
}

static void
dumpi(Syment *s, void *v)
{
	Sfio_t *fp = v;

	printi(fp, s->value, "", "\t", 1, 1, '\n');
}

static void
dumpn(Syment *s, void *v)
{
	Sfio_t *fp = v;
	Node *n = s->value;

	sfprintf(fp, "node %s %u %dMB %dMB nodelete=%u\n", n->name, n->lease, n->total, n->avail, n->nodelete);
}

void
dump(char *filename)
{
	Sfio_t *fp;
	time_t t;
	char path[NG_BUFFER];

	sfsprintf(path, sizeof path, "%s__", filename);
	if((fp = sfopen(0, path, "w")) == 0){
		perror(path);
		return;
	}
	t = time(0);
	sfprintf(fp, "dump at %I*d\n", sizeof(t), t);
	sfprintf(fp, "files:\n");
	symtraverse(syms, SYM_FILE, dumpf, fp);
	sfprintf(fp, "\nvars:\n");
	symtraverse(syms, SYM_VAR, dumpv, fp);
	sfprintf(fp, "\nnodes:\n");
	symtraverse(syms, SYM_NODE, dumpn, fp);
	sfprintf(fp, "\nunattached instances:\n");
	symtraverse(syms, SYM_INSTANCE, dumpi, fp);
	sfclose(fp);
	if(rename(path, filename) < 0)
		perror(filename);
}

Hint *
hint(Hint_type type, char *node, ng_uint32 lease)
{
	Hint *h;

	h = ng_malloc(sizeof(*h), "hint");
	h->type = type;
	h->node = ng_strdup(node);
	h->lease = lease;
	h->next = 0;
	return h;
}

void
delhint(Hint *h)
{
	ng_free(h->node);
	ng_free(h);

}

static void
dumpf1(Syment *s, void *v)
{
	Sfio_t *fp = v;
	File *f = s->value;

	sfprintf(fp, "file %s %s %I*d %d\n", f->md5, f->name, sizeof(f->size), f->size, f->ngoal);
}

static void
dump5(Sfio_t *fp)
{
	symtraverse(syms, SYM_NODE, dumpn, fp);
	symtraverse(syms, SYM_FILE, dumpf1, fp);
	symtraverse(syms, SYM_FILE0, dumpf1, fp);
}
