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
#include	<stdlib.h>
#include	"ningaui.h"

#define		N	1000000
static void *using[N];
static inuse = 1;
static find(void *);
#define	slide_a_to_b(a,b)	if(inuse > (a)) memmove(&using[b], &using[a], (inuse-a)*sizeof(using[0]))

void *
ng_malloc(int n, char *s)
{
	void *x;
	int i;

	if((x = malloc(n)) == 0){
		fprintf(stderr, "%s: malloc(%d) failed while allocating %s\n", argv0, n, s);
		abort();
		exit(1);	/* in case the abort returns */
	}
	i = find(x);
	if(using[i] == x){
		fprintf(stderr, "reusing 0x%x!\n", x);
		abort();
		exit(1);	/* in case the abort returns */
	}
	if(inuse >= N){
		fprintf(stderr, "walloc arena exhausted (%d used)\n", inuse);
		abort();
		exit(1);	/* in case the abort returns */
	}
	slide_a_to_b(i+1, i+2);
	inuse++;
	using[i+1] = x;
	if(verbose > 2)
		fprintf(stderr, "alloc==>%x\n", x);
	return x;
}

void
ng_ref(void *p)
{
	int i;

	i = find(p);
	if(using[i] != p){
		fprintf(stderr, "using unallocated block %x\n", p);
		for(i = 0; i < inuse; i++)
			fprintf(stderr, "\t%d: %x\n", i, using[i]);
		abort();
		exit(1);	/* in case the abort returns */
	}
}

void
ng_free(void *p)
{
	int i;

	free(p);
	i = find(p);
	if(using[i] != p){
		fprintf(stderr, "freeing unallocated 0x%x!\n", p);
		for(i = 0; i < inuse; i++)
			fprintf(stderr, "\t%d: %x\n", i, using[i]);
		abort();
		exit(1);	/* in case the abort returns */
	}
	slide_a_to_b(i+1, i);
	inuse--;
	if(verbose > 2)
		fprintf(stderr, "free-->%x\n", p);
}

void *
ng_realloc(void *p, int n, char *s)
{
	void *x;
	int i;

	x = (p == 0)? malloc(n) : realloc(p, n);
	if(x == 0){
		fprintf(stderr, "%s: realloc(%d) failed while reallocating %s\n", argv0, n, s);
		abort();
		exit(1);	/* in case the abort returns */
	}
	if(p != 0){
		ng_free(p);
	}
	i = find(x);
	if(using[i] == x){
		fprintf(stderr, "reusing 0x%x!\n", x);
		abort();
		exit(1);	/* in case the abort returns */
	}
	if(inuse >= N){
		fprintf(stderr, "walloc arena exhausted (%d used)\n", inuse);
		abort();
		exit(1);	/* in case the abort returns */
	}
	slide_a_to_b(i+1, i+2);
	inuse++;
	using[i+1] = x;
	if(verbose > 2)
		fprintf(stderr, "alloc(reall)==>%x\n", x);
	return x;
}

static
find(void *x)
{
	int h, m, l;

	if(inuse <= 0)
		return 0;
	l = 0;
	h = inuse;
	while(l < h-1){
		m = (l+h)/2;
		if(x >= using[m])
			l = m;
		else
			h = m;
	}
	return l;
}
