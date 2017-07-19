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
#include	<stdlib.h>
#include <sfio.h>
#include "ningaui.h"
#include "ng_ext.h"
#include "repmgr.h"

typedef struct
{
	ng_int64	size;
	ng_uint32	start, finish;
	Attempt		*a;
} Activity;

typedef struct Work
{
	Node		*node;
	Activity	*inprogress;
	int		inp_n;
	int		inp_a;
	Activity	*done;
	int		done_n;
	int		done_a;
	int		read_n;
	struct Work	*next;
} Work;
static Work *works = 0;

Work *
work(Node *n)
{
	Work *w;

	for(w = works; w; w = w->next)
		if(w->node == n)
			return w;
	w = ng_malloc(sizeof(*w), "work");
	w->node = n;
	w->inp_n = 0;
	w->inp_a = 10;
	w->inprogress = ng_malloc(w->inp_a*sizeof(Activity), "inprogress activities");
	w->done_n = 0;
	w->done_a = 10;
	w->done = ng_malloc(w->done_a*sizeof(Activity), "done activities");
	w->next = works;
	w->read_n = 0;
	works = w;
	return w;
}

#define	ensure(base, n, a)	if((n+1) >= a){a *= 2; base = ng_realloc(base, (a)*sizeof(Activity), "realloc base"); }

void
begin_attempt(Attempt *a)
{
	Work *w;

	w = work(a->to);
	ensure(w->inprogress, w->inp_n, w->inp_a);
	w->inprogress[w->inp_n].a = a;
	w->inprogress[w->inp_n].start = a->create;
	w->inp_n++;
}

void
end_attempt(Attempt *a, File *f, int complete)
{
	Work *w;
	int i;

	w = work(a->to);
	/* find in progress guy */
	for(i = 0; i < w->inp_n; i++)
		if(w->inprogress[i].a == a)
			break;
	if(i >= w->inp_n){
		sfprintf(sfstderr, "+++++++oops can't workflow inprogress a=x%x\n", a);
		return;
	}
	ensure(w->done, w->done_n, w->done_a);
	w->done[w->done_n] = w->inprogress[i];
	w->done[w->done_n].size = f->size;
	w->done[w->done_n].finish = time(0);
	w->done_n++;
	/* now remove in progress */
	if(i < w->inp_n-1){
		memcpy(&w->inprogress[i], &w->inprogress[i+1], (w->inp_n-i-1)*sizeof(Activity));
	}
	w->inp_n--;
}

void
saw_copylist(Node *node, int n)
{
	Work *w;

	w = work(node);
	w->read_n = n;
}

void
prflow(Sfio_t *fp)
{
	Work *w;

	for(w = works; w; w = w->next){
		sfprintf(fp, "node %s: %d in progress, %d done; last read %d\n", w->node->name, w->inp_n, w->done_n, w->read_n);
	}
	sfprintf(fp, "FLOW:");
	for(w = works; w; w = w->next){
		sfprintf(fp, " %s=%d[%d]", w->node->name, w->inp_n, w->read_n);
	}
	sfprintf(fp, "\n");
}
