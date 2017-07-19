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

static void
copylist_v1(char *fname, Sfio_t *fp, char *noden)
{
	char filen[NG_BUFFER], md5[NG_BUFFER], buf[NG_BUFFER], spaceman[NG_BUFFER];
	char attr[NG_BUFFER];
	char key[NG_BUFFER], val[NG_BUFFER];
	ng_uint32 lease, diskt, diska, ts, idle_d;
	Node *node;
	Syment *s;
	Instance *i, *ii;
	char *base;
	int ni, nf;
	int nents;
	int cmp;
	ng_int64 fsize;
	Sortlist *sl, *osl;
	int old, new;
	int n_in, n_add, n_drop;

	ts = lease = diskt = diska = idle_d = 0;
	spaceman[0] = 0;
	strcpy(attr, "  ");
	while(sfscanf(fp, " %[^=]=%s", key, val) == 2){
		if(verbose > 1)
			sfprintf(sfstderr, "key=%s val=%s\n", key, val);
		if(strcmp(key, "ts") == 0){
			if(sfsscanf(val, "%u", &ts) != 1){
				sfprintf(sfstderr, "%s: parse error: val(%s) for %s should be a number\n", fname, val, key);
				goto err;
			}
		} else if(strcmp(key, "lease") == 0){
			if(sfsscanf(val, "%u", &lease) != 1){
				sfprintf(sfstderr, "%s: parse error: val(%s) for %s should be a number\n", fname, val, key);
				goto err;
			}
		} else if(strcmp(key, "totspace") == 0){
			if(sfsscanf(val, "%u", &diskt) != 1){
				sfprintf(sfstderr, "%s: parse error: val(%s) for %s should be a number\n", fname, val, key);
				goto err;
			}
		} else if(strcmp(key, "freespace") == 0){
			if(sfsscanf(val, "%u", &diska) != 1){
				sfprintf(sfstderr, "%s: parse error: val(%s) for %s should be a number\n", fname, val, key);
				goto err;
			}
		} else if(strcmp(key, "idle") == 0){
			if(strstr(val, "delete"))
				idle_d = 1;
		} else if(strcmp(key, "fslist") == 0){
			char *p;

			for(p = val; *p; p++)
				if(*p == '!')
					*p = ' ';
			strcpy(spaceman, val);
		} else if(strcmp(key, "nattr") == 0){
			char *p;

			for(p = val; *p; p++)
				if(*p == ',')
					*p = ' ';
			attr[0] = ' ';
			strcpy(attr+1, val);
			strcat(attr, " ");
		} else if(strcmp(key, "instances") == 0){
			break;
		} else {
			sfprintf(sfstderr, "%s: parse error: unknown key %s (val=%s)\n", fname, key, val);
			goto done;
		}
	}
	/* first, update disk knowledge */
	if((s = symlook(syms, noden, SYM_NODE, 0, SYM_NOFLAGS)) == 0){
		node = new_node(noden);
		if(verbose)
			sfprintf(sfstderr, "new node %s from copylist(%s)\n", node->name, fname);
	} else {
		node = s->value;
	}
	node->avail = diska;
	node->total = diskt;
	node->lease = lease;
	if(spaceman[0]){
		if(node->spacefslist)
			ng_free(node->spacefslist);
		node->spacefslist = ng_strdup(spaceman);
		if(verbose)
			sfprintf(sfstderr, "set %s->spacefslist to %s\n", node->name, node->spacefslist);
	}
	if(attr[2]){
		if(node->nattr)
			ng_free(node->nattr);
		node->nattr = ng_strdup(attr);
		if(verbose)
			sfprintf(sfstderr, "set %s->nattr to %s\n", node->name, node->nattr);
	}
	if(node->purgeme){
		node->purgeme = 0;
		purge(node);
		delsortlist(node->mine);
		node->mine = newsortlist();
	}
	if(idle_d && (node->nodelete > 1) && (node->nodelete < ts)){
		/* node is idle, nodelete flag is on, and this copylist is after the no delete timeout */
		node->nodelete = 0;
	}
	/* generate a new sortlist */
	n_in = n_add = n_drop = 0;
	sl = newsortlist();
	while(sfscanf(fp, "%s %s %I*d", md5, filen, sizeof(fsize), &fsize) == 3){
		sfsprintf(buf, sizeof buf, "%s %s %I*d", md5, filen, sizeof(fsize), fsize);
		addline(sl, buf);
		n_in++;
	}
	sortlist(sl);
	/* compare the two */
	ni = nf = 0;
	osl = node->mine;
	if(trace_name){
		sfprintf(sfstderr, "tracing(%s in node %s):", trace_name, node->name);
		for(old = 0; old < osl->np; old++)
			if(strcmp(trace_name, osl->p[old]) == 0){
				sfprintf(sfstderr, " old[%d]", old);
				break;
			}
		for(new = 0; new < sl->np; new++)
			if(strcmp(trace_name, sl->p[new]) == 0){
				sfprintf(sfstderr, " new[%d]", new);
				break;
			}
		sfprintf(sfstderr, "\n");
	}
	for(old = new = 0; (old < osl->np) || (new < sl->np);){
		if(old >= osl->np)
			cmp = 1;
		else if(new >= sl->np)
			cmp = -1;
		else
			cmp = strcmp(osl->p[old], sl->p[new]);
		if(cmp == 0){	/* match, step over both */
			new++;
			old++;
		} else if(cmp < 0){	/* old instance not in new; delete */
			sfsscanf(osl->p[old], "%s %s", md5, filen);
			dropinstance(node, filen, md5);
			old++;
			n_drop++;
		} else {		/* new instance not in old; add */
			sfsscanf(sl->p[new], "%s %s %I*d", md5, filen, sizeof(fsize), &fsize);
			addinstance(node, node->name, filen, md5, fsize, &ni, &nf);
			new++;
			n_add++;
		}
	}
	if(verbose)
		sfprintf(sfstderr, "%s: %d entries, %d adds, %d drops; %d new instances, %d files updated\n", fname, n_in, n_add, n_drop, ni, nf);
	delsortlist(node->mine);
	node->mine = sl;
done:
	sfclose(fp);
	return;
err:
	ng_log(LOG_ERR, "%s: parse error\n", fname);
	goto done;
}

void
copylist(char *fname)
{
	Sfio_t *fp;
	int ver;
	char noden[NG_BUFFER];

	if((fp = sfopen(0, fname, "r")) == NULL){
		perror(fname);
		return;
	}
	if(sfscanf(fp, "%s %u", noden, &ver) != 2){
		ng_log(LOG_ERR, "%s: parse error: bad header\n", fname);
		return;
	}
	switch(ver)
	{
	case 1:		copylist_v1(fname, fp, noden); break;
	default:	sfprintf(sfstderr, "copylist(%s) has unsupported version %d\n", fname, ver);
			sfclose(fp);
			break;
	}
}

static int bguess = 64*1024;
static int lguess = 128;

Sortlist *
newsortlist(void)
{
	Sortlist *s;

	s = ng_malloc(sizeof(*s), "sortlist");
	s->ab = bguess;
	s->nb = 0;
	s->b = ng_malloc(s->ab, "sortlist buffer");
	s->ap = s->ab/lguess;
	s->np = 0;
	s->p = ng_malloc(sizeof(s->p[0])*s->ap, "sortlist ptr vec");
	return s;
}

void
delsortlist(Sortlist *s)
{
	ng_free(s->p);
	ng_free(s->b);
	ng_free(s);
}

void
addline(Sortlist *s, char *l)
{
	int n;

	/* check buf for room */
	n = strlen(l)+1;
	if(s->nb+n > s->ab){
		char *ob = s->b;
		int i;

		s->ab *= 2;
		s->b = ng_realloc(s->b, s->ab, "sortlist buffer");
		for(i = 0; i < s->np; i++)
			s->p[i] = s->b + (s->p[i]-ob);
	}
	/* check room for ptr */
	if(s->np+1 > s->ap){
		s->ap *= 2;
		s->p = ng_realloc(s->p, s->ap*sizeof(s->p[0]), "sortlist ptr vec");
	}
	/* assign ptr */
	s->p[s->np++] = s->nb+s->b;
	/* copy line */
	memcpy(s->b+s->nb, l, n);
	s->nb += n;
}

static int
cmp(const void *a, const void *b)
{
	char **ap = (char **)a;
	char **bp = (char **)b;

	return strcmp(*ap, *bp);
}

void
sortlist(Sortlist *s)
{
	int i;

	if(s->np == 0)
		return;
	/* first, update guesses */
	bguess = (bguess+s->nb)/2 + 16*1024;
	lguess = (lguess + (s->nb/s->np))/2 + 256;
	/* now sort! */
	qsort(s->p, s->np, sizeof(s->p[0]), cmp);
}
