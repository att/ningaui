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
#include "repmgrb.h"

void *
input_thread(void *v)
{
	Input_block *i = v;
	int fd;


	/* all this thread does is look for connections */
	for(;;){
		if(verbose > 1)
			sfprintf(sfstderr, "probing: ");
		fd = ng_serv_probe_wait(i->serv_fd, -1);	 /* -1 secs blocks forever */
		if(verbose > 1)
			sfprintf(sfstderr, "probe returns %d @ %d\n", fd, time(0));
		if(fd < 0)
			return 0;
		if(fd == 0)
			continue;
		putcon(i, fd);			/* put on list to serve */
	}
}

void *
con_thread(void *v)
{
	Input_block *i = v;
	int n;
	int fd;
	char *buf = 0;
	int nbuf, abuf;
	time_t start;
	ng_hrtime  t0, t1, t2, t3;
	char buf1[32];

	abuf = 2*NG_BUFFER;
	P();
	buf = ng_malloc(abuf, "input buffer");
	V();

	for(;;){
		fd = getcon(i);
		if(fd < 0){
			sleep(2);
			continue;
		}
		if(verbose > 1)
			sfprintf(sfstderr, "getcon=%d; about to do first read @%d\n", fd, start = time(0));
		t0 = ng_hrtimer();
		n = read(fd, buf, abuf/2);
		t1 = ng_hrtimer();
		if(verbose > 1)
			sfprintf(sfstderr, "\tfirst read returns %d @%d\n", n, time(0)-start);
		if(n == 0){
			close(fd);
			continue;
		}
		if(n > 0){
			nbuf = n;
			while((n = read(fd, buf+nbuf, abuf-nbuf)) > 0){
				nbuf += n;
				if(nbuf >= abuf){
					abuf = abuf*1.4;
					P();
					buf = ng_realloc(buf, abuf, "input buffer");
					V();
				}
			}
		}
		close(fd);
		t2 = ng_hrtimer();
		if(n < 0){
			if(verbose)
				sfprintf(sfstderr, "\tread returned -1; ignoring\n");
			continue;
		}
		buf[nbuf++] = 0;
		putchunk(i, buf, nbuf);
		t3 = ng_hrtimer();
		if((fd = nbuf-1) >= 32)
			fd = 31;
		memcpy(buf1, buf, fd);
		buf1[fd] = 0;
		if(verbose)
			sfprintf(sfstderr, "\trequest done: %d bytes(%s), %.1fms (1r=%.1fms, rt=%.1fms, put=%.1fms)\n", nbuf, buf1,
			(t3-t0)*hrtime_scale*1e3, (t1-t0)*hrtime_scale*1e3, (t2-t1)*hrtime_scale*1e3, (t3-t2)*hrtime_scale*1e3);
	}
}

void
putcon(Input_block *i, int fd)
{
	Con *c, *cb;

	P();
	cb = ng_malloc(sizeof(*cb), "con");
	cb->fd = fd;
	cb->next = 0;
	if(i->cons){
		for(c = i->cons; c->next; c = c->next)
			;
		c->next = cb;
	} else
		i->cons = cb;
	V();
}

int
getcon(Input_block *i)
{
	Con *c;
	int fd;

	P();
	if(c = i->cons){
		fd = c->fd;
		i->cons = c->next;
		ng_free(c);
	} else
		fd = -1;
	V();
	return fd;
}

void
putchunk(Input_block *i, char *buf, int len)
{
	Chunk *c;

	/* don't do dups */
	P();
	for(c = i->top; c; c = c->next)
		if((len-1 == c->len) && (memcmp(buf, c->buf, c->len) == 0)){
			if(verbose)
				sfprintf(sfstderr, "discard dup '%.20s'\n", buf);
			break;
		}
	V();
	if(c)
		return;

	/* not a dup, so just do it */
	P();
	c = ng_malloc(sizeof(*c), "chunk");
	c->buf = ng_malloc(len, "chunk buf");
	V();
	memcpy(c->buf, buf, len);
	c->len = len-1;					/* don't include the null */
	c->next = 0;
	if(verbose)
		sfprintf(sfstderr, "add input request %d bytes at x%x\n", len, c->buf);

	P();
	if(i->last == 0)	/* empty? */
		i->top = c;
	else
		i->last->next = c;
	i->last = c;
	V();
}

Chunk *
getchunk(Input_block *i)
{
	Chunk *c;

	P();
	if(c = i->top){
		i->top = c->next;
		if(i->last == c)
			i->last = 0;
	}
	V();
	return c;
}

void
freechunk(Chunk *c)
{
	ng_free(c->buf);
	ng_free(c);
}
