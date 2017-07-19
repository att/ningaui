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
	Mods:
		14 May 2008 (sd) : added sfclose() calls to read-library function if 
			error detected and early return is needed. 
*/


#include	<sfio.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>
#include	"ningaui.h"
#include	"changer.h"

char *tname[C_end] = { "-?notfound?-", "DTE", "slot", "portal" };

#define	POOLFILE	"/ningaui/site/tape/tapes"
Pool pool[NG_BUFFER];
int npool = 0;
static char *poolbuf;
static int npoolbuf, apoolbuf = 1024*1024;

Library changer;
static char *changerbuf;
static int nchangerbuf, achangerbuf = 1024*1024;

Devmap devmap;
static char *devmapbuf;
static int ndevmapbuf, adevmapbuf = 1024*1024;

static char *psavestr(char *);
static char *csavestr(char *);
static char *dsavestr(char *);

void
read_pools(void)
{
	int i, k, n, t;
	char *p, *q;
	char buf[NG_BUFFER], *cp[NG_BUFFER];
	Pool *pp;

	ng_bleat(1, "read pools");
	npool = 0;
	npoolbuf = 0;

	n = ng_pp_namespace(cp, NG_BUFFER);
	if(n <= 0){
		ng_bleat(1, "pp_namespace failed\n");
		return;
	}
	strcpy(buf, "TAPEPOOL_");
	for(i = 0; i < n; i++){
		if(strncmp(cp[i], buf, strlen(buf)) == 0){
			pp = &pool[npool++];
			pp->name = psavestr(cp[i]+strlen(buf));
			pp->delta = 0;
			pp->nvol = 0;
			p = ng_pp_get(cp[i], 0);
			if(*p == 0){
				ng_bleat(1, "weird: can't fetch %s", cp[i]);
				npool--;
				continue;
			}
			ng_bleat(2, "pool '%s' is '%s'", cp[i], p);
			if(strcmp(p, ",") != 0) do {
				q = strchr(p, ',');
				if(q)
					*q++ = 0;
				pp->vol[pp->nvol++] = psavestr(p);
				if(q)
					p = q;
			} while(q);
		}
	}
	ng_bleat(1, "read %d pools", npool);
}

void
write_pools(void)
{
	char buf[NG_BUFFER], buf1[NG_BUFFER];
	Pool *p;
	int i;

	for(p = pool; p < pool+npool; p++)
		if(p->delta){
			strcpy(buf, "TAPEPOOL_");
			strcat(buf, p->name);
			buf1[0] = 0;
			if(p->nvol){
				strcpy(buf1, p->vol[0]);
				for(i = 1; i < p->nvol; i++){
					strcat(buf1, ",");
					strcat(buf1, p->vol[i]);
				}
			} else
				strcpy(buf1, ",");
			ng_bleat(2, "set pool %s to '%s'", buf, buf1);
			ng_pp_set(buf, buf1, PP_FLOCK);
		}
}

static void
pool_dump1(Sfio_t *fp, Pool *p)
{
	int i;

	sfprintf(fp, "%s:", p->name);
	for(i = 0; i < p->nvol; i++)
		sfprintf(fp, " %s", p->vol[i]);
	sfprintf(fp, "\n");
}

void
pool_dump(Sfio_t *fp, char *poolid)
{
	int i, k, n, t;
	char *p, *q;
	char buf[NG_BUFFER], *cp[NG_BUFFER];

	read_pools();
	if(poolid == 0){
		for(n = 0; n < npool; n++)
			pool_dump1(fp, &pool[n]);
	} else {
		for(n = 0; n < npool; n++)
			if(strcmp(pool[n].name, poolid) == 0){
				pool_dump1(fp, &pool[n]);
				return;
			}
		sfprintf(fp, "pool '%s' unknown\n", poolid);
	}
}

void
pool_add(Sfio_t *fp, char *poolid, char *volid)
{
	Pool *p;
	int n;

	read_pools();
	for(n = 0; n < npool; n++)
		if(strcmp(pool[n].name, poolid) == 0){
			p = &pool[n];
			for(n = 0; n < p->nvol; n++)
				if(strcmp(volid, p->vol[n]) == 0)
					return;		/* we're done */
			p->vol[p->nvol++] = psavestr(volid);
			p->delta = 1;
			write_pools();
			return;
		}
	sfprintf(fp, "pool '%s' unknown\n", poolid);
}

void
pool_del(Sfio_t *fp, char *poolid, char *volid)
{
	int n;
	Pool *p;

	read_pools();
	for(n = 0; n < npool; n++)
		if(strcmp(pool[n].name, poolid) == 0){
			p = &pool[n];
			for(n = 0; n < p->nvol; n++)
				if(strcmp(volid, p->vol[n]) == 0){
					for(n++; n < p->nvol; n++)
						p->vol[n-1] = p->vol[n];
					p->nvol--;
					p->delta = 1;
					write_pools();
					return;
				}
			sfprintf(fp, "volid '%s' unknown in pool '%s'\n", volid, poolid);
			return;
		}
	sfprintf(fp, "pool '%s' unknown\n", poolid);
}

int
find_empty(Sfio_t *fp, int *val)
{
	int slot;

	for(slot = changer.min_slot; slot <= changer.max_slot; slot++)
		if(changer.slot[slot] == 0){
			if(fp)
				sfprintf(fp, "info: found empty slot %d\n", slot);
			ng_bleat(2, "unload: found empty slot %d", slot);
			*val = slot;
			return C_slot;
		}
	for(slot = changer.min_portal; slot <= changer.max_portal; slot++)
		if(changer.portal[slot] == 0){
			if(fp)
				sfprintf(fp, "info: found empty portal %d\n", slot);
			ng_bleat(2, "unload: found empty portal %d", slot);
			*val = slot;
			return C_portal;
		}
	return C_notfound;
}

void
mtx_load(Sfio_t* fp, int type, int slot, int dte)
{
	char buf[NG_BUFFER];

	sfprintf(fp, "info: moving %s %d to dte %d\n", tname[type], slot, dte);
	sfsprintf(buf, sizeof buf, "ng_tp_library -f %s load %s%d %d", changer.dev, (type==C_portal? "p":""), slot, dte);
	ng_bleat(1, "load(%d) via '%s'", dte, buf);
	system(buf);
	heartbeat();
}

void
mtx_unload(Sfio_t* fp, int dte, int type, int slot)		/* slot < 0 means i don't care */
{
	char buf[NG_BUFFER];

	if(slot < 0){
		type = find_empty(fp, &slot);
		if(type == C_notfound){
			sfprintf(fp, "error: couldn't find an empty slot\n");
			ng_bleat(2, "unload: couldn't find an empty slot");
			return;
		}
	}
	sfprintf(fp, "info: unloading tape from dte %d to %s %d\n", dte, tname[type], slot);
	sfsprintf(buf, sizeof buf, "ng_tp_library -f %s unload %d %s%d", changer.dev, dte, (type==C_portal? "p":""), slot);
	ng_bleat(1, "unload(%d) via '%s'", dte, buf);
	system(buf);
	heartbeat();
}

void
mtx_transfer(Sfio_t* fp, int src_t, int src_n, int dest_t, int dest_n)
{
	char buf[NG_BUFFER];

	sfprintf(fp, "info: transferring tape from %s %d to %s %d\n", tname[src_t], src_n, tname[dest_t], dest_n);
	sfsprintf(buf, sizeof buf, "ng_tp_library -f %s transfer %s%d %s%d", changer.dev, (src_t==C_portal? "p":""), src_n, (dest_t==C_portal? "p":""), dest_n);
	ng_bleat(1, "transfer(%s %d, %s %d) via '%s'", tname[src_t], src_n, tname[dest_t], dest_n, buf);
	system(buf);
	heartbeat();
}

void
dev_offline(Sfio_t* fp, int dte)
{
	char buf[NG_BUFFER];

	sfprintf(fp, "info: offlining tape in dte %d (%s on %s)\n", dte, devmap.dte[dte].dev, devmap.dte[dte].sys);
	sfsprintf(buf, sizeof buf, "ng_rcmd -t 100 %s mt -f %s offline", devmap.dte[dte].sys, devmap.dte[dte].dev);
	ng_bleat(1, "offline(%d) via '%s'", dte, buf);
	system(buf);
	heartbeat();
}

void
read_library(void)
{
	char *tfile = "ng_tp_library catalog";
	int k;
	char *s;
	int ns, nd, np;
	Sfio_t *fp;
	char buf[NG_BUFFER], buf1[NG_BUFFER];

	heartbeat();
	if((fp = sfpopen(0, tfile, "r")) == NULL){
		perror(tfile);
		return;
	}
	if(sfscanf(fp, "%s %d %d %d %d %d %d", changer.dev, &changer.min_dte, &changer.max_dte, &changer.min_slot, &changer.max_slot, &changer.min_portal, &changer.max_portal) != 7){
		ng_bleat(0, "syntax error, no min,max dte,slot in %s", tfile);
		sfclose( fp );
		return;
	}
	nchangerbuf = 0;
	s = csavestr(NO_NAME);
	for(k = changer.min_dte; k <= changer.max_dte; k++)
		changer.dte[k] = s;
	for(k = changer.min_slot; k <= changer.max_slot; k++)
		changer.slot[k] = 0;
	for(k = changer.min_portal; k <= changer.max_portal; k++)
		changer.portal[k] = 0;
	ns = nd = np = 0;
	while(sfscanf(fp, "%s %d %s", buf, &k, buf1) == 3){
		if(strcmp(buf1, EMPTY) == 0)
			s = 0;
		else
			s = csavestr(buf1);
		if(strcmp(buf, "dte") == 0){
			changer.dte[k] = s;
			nd++;
		} else if(strcmp(buf, "slot") == 0){
			changer.slot[k] = s;
			ns++;
		}else if(strcmp(buf, "portal") == 0){
			changer.portal[k] = s;
			np++;
		} else {
			ng_bleat(0, "syntax error, expected 'dte', 'portal' or 'slot'; got '%s'", buf);
			sfclose( fp );
			return;
		}
	}
	ng_bleat(1, "read %d dtes, %d slots[min=%d], %d portals[min=%d] from %s", nd,
		ns, changer.min_slot, np, changer.min_portal, tfile);
	sfclose(fp);
	heartbeat();
}

void
read_devmap(void)
{
	int i, k, n, t;
	char *p, *q;
	char buf[NG_BUFFER], buf1[NG_BUFFER], buf2[NG_BUFFER], *cp[NG_BUFFER];

	ng_bleat(1, "reading dev map");
	ndevmapbuf = 0;
	devmap.maxdte = 0;
	for(k = 0; k < sizeof(devmap.dte)/sizeof(devmap.dte[0]); k++)
		devmap.dte[k].sys = devmap.dte[k].dev = 0;

	n = ng_pp_namespace(cp, NG_BUFFER);
	if(n <= 0)
		return;
	ng_bleat(1, "scanning %d namespace entries", n);
	strcpy(buf, "DTE_");
	strcat(buf, domain);
	strcat(buf, "_");
	for(i = 0; i < n; i++)
		if(strncmp(cp[i], buf, strlen(buf)) == 0){
			k = strtol(cp[i]+strlen(buf), 0, 10);
			p = ng_pp_get(cp[i], 0);
			devmap.dte[k].tapeid = dsavestr(p);
			strcpy(buf1, "TAPE_");
			strcat(buf1, p);
			p = ng_pp_get(buf1, 0);
			if(*p == 0){	/* huh?? */
				ng_bleat(1, "pp_get failed for '%s'", buf1);
				continue;
			}
			q = strchr(p, ',');
			if(q == 0){	/* huh?? */
				ng_bleat(1, "malformed value for %s = '%s'", buf1, p);
				continue;
			}
			*q++ = 0;
			devmap.dte[k].sys = dsavestr(p);
			devmap.dte[k].dev = dsavestr(q);
			if(k > devmap.maxdte)
				devmap.maxdte = k;
			ng_bleat(2, "%d -> %s -> %s -> %s,%s", k, cp[i], devmap.dte[k].tapeid, devmap.dte[k].sys, devmap.dte[k].dev);
		}

	ng_bleat(1, "max dte = %d", devmap.maxdte);
}

static char *
psavestr(char *s)
{
	int k;

	k = strlen(s)+1;
	if(k+npoolbuf > apoolbuf){
		ng_bleat(0, "too little pool space (%d)", apoolbuf);
		exit(1);
	}
	if(poolbuf == 0)
		poolbuf = ng_malloc(apoolbuf, "poolbuf");
	memcpy(poolbuf+npoolbuf, s, k);
	npoolbuf += k;
	return poolbuf+npoolbuf-k;
}

static char *
csavestr(char *s)
{
	int k;

	k = strlen(s)+1;
	if(k+nchangerbuf > achangerbuf){
		ng_bleat(0, "too little changer space (%d)", achangerbuf);
		exit(1);
	}
	if(changerbuf == 0)
		changerbuf = ng_malloc(achangerbuf, "changerbuf");
	memcpy(changerbuf+nchangerbuf, s, k);
	nchangerbuf += k;
	return changerbuf+nchangerbuf-k;
}

static char *
dsavestr(char *s)
{
	int k;

	k = strlen(s)+1;
	if(k+ndevmapbuf > adevmapbuf){
		ng_bleat(0, "too little devmap space (%d)", adevmapbuf);
		exit(1);
	}
	if(devmapbuf == 0)
		devmapbuf = ng_malloc(adevmapbuf, "devmapbuf");
	memcpy(devmapbuf+ndevmapbuf, s, k);
	ndevmapbuf += k;
	return devmapbuf+ndevmapbuf-k;
}
