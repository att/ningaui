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
#include	<signal.h>
#include	<stdlib.h>
#include	<string.h>
#include	"ningaui.h"
#include	"changer.h"

#include	"tp_changer.man"

static int proc(char *b, int fd);
static void readin(int fd, char **b);
static void tdelete(char *volid, char *why);
static int load(char *volid, char *cluster, char *drive);
static void status(Sfio_t *fp, char *what);
static void mapdev(Sfio_t *fp, char *dev);
static void transfer(Sfio_t *fp, char *from, char *to);
static void sortmedia(Sfio_t *fp, char *from, char *to);
static void decant(Sfio_t *fp, char **first, int n);
static void identify(char *volid, char *drive);

static void loadvol(Sfio_t *fp, char *volid, char *driveid);
static void unload(Sfio_t *fp, char *driveid);
static void move1(Sfio_t *fp, int src_t, int src_n, int dest_t, int dest_n);

char *domain;
char *heartbeat_file;

#define	RETRY	(5*60)		/* 5 minutes to let physical things settle */

int
main(int argc, char **argv)
{
	int serv_fd, ifd, ofd;
	char *port = ng_env(TM_PORT);
	char stderrbuf[BUFSIZ];
	char buf[NG_BUFFER], me[NG_BUFFER];
	char *sfile = 0;
	int i;
	long nextd = 0;
	long incd = 300;
	char *b;

	setvbuf(stderr, stderrbuf, _IOLBF, sizeof stderrbuf);
	ARGBEGIN{
	case 'p':	SARG(port); break;
	case 'v':	OPT_INC(verbose); break;
	default:	goto usage;
	}ARGEND
	if(argc < 1){
usage:
		fprintf(stderr, "Usage: %s [-p port] [-v] tape-domain ...\n", argv0);
		exit(1);
	}
	setfields(" \t\n");
	domain = argv[0];		/* favoured name */

	if(!port){
		ng_bleat( 0, "%s:fatal: no port (%s) in cartulary or command line", argv0, TM_PORT);
		exit(1);
	}

	ng_bleat(2, "%s: using port %s", argv0, port);
	if((serv_fd = ng_serv_announce(port)) < 0)
		exit(1);
	if(ng_sysname(me, sizeof me) < 0)
		exit(1);
	for(i = 0; i < argc; i++){
		sfsprintf(buf, sizeof buf, "ng_ppset -f FLOCK_tapedomain_%s %s", argv[i], me);
		system(buf);
		ng_bleat(1, "announce node: %s", buf);
	}

        signal(SIGPIPE, SIG_IGN);

	for(;;){
		ifd = ng_serv_probe_wait(serv_fd, 1);		/* block for at most 1 second while waiting for connect */
		ng_bleat(3, "probe returns %d", ifd);
		if(ifd < 0){
			ng_log(LOG_CRIT, "server died: %s\n", ng_perrorstr());
			if(verbose)
				fprintf(stderr, "%s: server died\n", argv0);
			break;
		}
		if(ifd == 0){
			heartbeat();
			continue;
		}
		ofd = dup(ifd);
		readin(ifd, &b);
		if(proc(b, ofd))
			break;
		close(ifd);
	}

	ng_bleat(1, "%s: preparing to exit", argv0);
	/* cleanup */
	ng_serv_unannounce(serv_fd);
	ng_bleat(1, "%s: exiting normally", argv0);

	exit(0);
}

static void
readin(int fd, char **b)
{
	static char *buf = 0;
	int n, k, i;
	static int na = 4096;

	if(buf == 0)
		buf = ng_malloc(na, "readin buffer");
	n = 0;
	while((k = read(fd, buf+n, na-n)) > 0){
		for(i = 0; i < k; i++)
			if(buf[n+i] == '!'){
				n += i;
				goto done;
			}
		n += k;
		if((na-n) < 1024){
			na *= 2;
			buf = ng_realloc(buf, na, "readin buffer");
		}
	}
done:
	if((n > 0) && (buf[n-1] == '\n'))
		n--;
	buf[n] = 0;
	*b = buf;
	ng_bleat(2, "read '%s'", buf);
}

static int
proc(char *buf, int fd)
{
	int k, x, np;
	char *bp[NG_BUFFER];
	Sfio_t *fp;
	int ret = 0;

	if((fp = sfnew(0, 0, 0, fd, SF_WRITE)) == NULL){
		perror("ifd dup");
		exit(1);
	}
	ng_bleat(1, "proc(%s)", buf);
	np = getmfields(buf, bp, NG_BUFFER);
	for(k = 0; k < np; k++){
		heartbeat();
		if(strcmp(bp[k], "pooldump") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				pool_dump(fp, (strcmp(bp[k+1], "all") == 0)? 0:bp[k+1]);
				k++;
			}
		} else if(strcmp(bp[k], "pooladd") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				pool_add(fp, bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "pooldel") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				pool_del(fp, bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "loadvol") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				loadvol(fp, bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "mapdev") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				mapdev(fp, bp[k+1]);
				k += 1;
			}
		} else if(strcmp(bp[k], "identify") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				identify(bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "heartbeat") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				if(strcmp(bp[k+1], "same") == 0)
					;	/* leave alone */
				else {
					if(heartbeat_file)
						ng_free(heartbeat_file);
					if(strcmp(bp[k+1], "-") == 0)
						heartbeat_file = 0;
					else
						heartbeat_file = ng_strdup(bp[k+1]);
				}
				k += 1;
			}
		} else if(strcmp(bp[k], "unload") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				unload(fp, bp[k+1]);
				k += 1;
			}
		} else if(strcmp(bp[k], "transfer") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				transfer(fp, bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "status") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				status(fp, bp[k+1]);
				k += 1;
			}
		} else if(strcmp(bp[k], "sort") == 0){
			if(k+2 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				sortmedia(fp, bp[k+1], bp[k+2]);
				k += 2;
			}
		} else if(strcmp(bp[k], "sleep") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				sfsscanf(bp[++k], "%d", &x);
				ng_bleat(0, "sleeping for %d seconds\n", x);
				while(x-- > 0){
					sleep(1);
					heartbeat();
				}
			}
		} else if(strcmp(bp[k], "decant") == 0){
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for %s command", bp[k]);
			else {
				decant(fp, &bp[k+1], np-(k+1));
				k = np;
			}
		} else if(strcmp(bp[k], "verbose") == 0){
			x = -1;
			if(k+1 >= np)
				ng_bleat(0, "syntax error, missing value for verbose command");
			else
				sfsscanf(bp[++k], "%d", &x);
			if(x >= 0){
				verbose = x;
				ng_bleat(0, "verbose changed to %d", verbose);
			}
		} else if(strcmp(bp[k], "exit") == 0){
			ng_bleat(1, "asked to exit");
			ret = 1;
		} else {
			ng_bleat(0, "syntax error, unknown command '%s'", bp[k]);
			sfprintf(fp, "syntax error, unknown command '%s'\n", bp[k]);
		}
	}
	sfprintf(fp, "~done\n");
	sfclose(fp);
	heartbeat();
	return ret;
}

static int
dfind(char *id)
{
	int i, dest;
	char buf[NG_BUFFER];

	if(strchr(id, ':')){
		char *sys, *dev;

		strcpy(sys = buf, id);
		dev = strchr(sys, ':');
		*dev++ = 0;
		dest = -1;
		for(i = 0; i <= devmap.maxdte; i++)
			if(devmap.dte[i].sys && (strcmp(devmap.dte[i].sys, sys) == 0)
					&& devmap.dte[i].dev && (strcmp(devmap.dte[i].dev, dev) == 0)){
				dest = i;
				break;
			}
	} else if(isalpha(id[0])){
		dest = -1;
		for(i = 0; i <= devmap.maxdte; i++)
			if(devmap.dte[i].tapeid && (strcmp(devmap.dte[i].tapeid, id) == 0)){
				dest = i;
				break;
			}
	} else if(isdigit(id[0])){
		dest = strtol(id, 0, 10);
	} else {
		ng_bleat(1, "bad tapeid spec '%s'", id);
		return -1;
	}
	return dest;
}

static int
vfind(char *id, int *val)
{
	int i;

	for(i = changer.min_dte; i <= changer.max_dte; i++)
		if(changer.dte[i] && (strcmp(changer.dte[i], id) == 0)){
			*val = i;
			return C_dte;
		}
	for(i = changer.min_slot; i <= changer.max_slot; i++)
		if(changer.slot[i] && (strcmp(changer.slot[i], id) == 0)){
			*val = i;
			return C_slot;
		}
	for(i = changer.min_portal; i <= changer.max_portal; i++)
		if(changer.portal[i] && (strcmp(changer.portal[i], id) == 0)){
			*val = i;
			return C_portal;
		}
	return C_notfound;
}

static Pool *
pfind(char *id)
{
	int i;

	for(i = 0; i < npool; i++)
		if(strcmp(pool[i].name, id) == 0)
			return &pool[i];
	return 0;
}

static void
unload(Sfio_t *fp, char *driveid)
{
	int target;
	
	read_library();
	read_devmap();
	target = dfind(driveid);
	if(target < 0){
		sfprintf(fp, "error: can't map '%s' to a DTE\n", driveid);
		return;
	}
	dev_offline(fp, target);
	mtx_unload(fp, target, C_slot, -1);
}

static void
mapdev(Sfio_t *fp, char *driveid)
{
	int target;
	
	read_library();
	read_devmap();
	target = dfind(driveid);
	if(target < 0){
		sfprintf(fp, "error: can't map '%s' to a DTE\n", driveid);
		return;
	}
	sfprintf(fp, "okay: %d %s %s %s\n", target, devmap.dte[target].tapeid, devmap.dte[target].sys, devmap.dte[target].dev);
}

static void
transfer(Sfio_t *fp, char *from, char *to)
{
	int target;
	int a, b;
	
	read_library();
	a = strtol(from, 0, 10);
	b = strtol(to, 0, 10);
	mtx_transfer(fp, C_slot, a, C_slot, b);
}

static void
loadvol(Sfio_t *fp, char *volid, char *driveid)
{
	int val, target;
	int ntries;

	ntries = 5;
again:
	if(--ntries <= 0){
		sfprintf(fp, "error: couldn't make progress (%s, %s)\n", volid, driveid);
		return;
	}
	
	read_library();
	read_devmap();
	target = dfind(driveid);
	if(target < 0){
		sfprintf(fp, "error: can't map '%s' to a DTE\n", driveid);
		return;
	}
	switch(vfind(volid, &val))
	{
	case C_dte:
		ng_bleat(1, "found volid '%s' in dte %d (= %s:%s)", volid, val, devmap.dte[val].sys, devmap.dte[val].dev);
		if(val != target){
			sfprintf(fp, "info: volid '%s' in wrong dte (%d); put back in slot\n", volid, val);
			dev_offline(fp, val);
			mtx_unload(fp, val, C_slot, -1);
			goto again;
		}
		sfprintf(fp, "okay: %s %d %s %s %s\n", volid, target, devmap.dte[val].tapeid, devmap.dte[val].sys, devmap.dte[val].dev);
		break;
	case C_slot:
		ng_bleat(1, "found volid '%s' in slot %d", volid, val);
		if(changer.dte[target]){
			sfprintf(fp, "info: volid '%s' in slot, but target dte %d has a tape in it\n", volid, target);
			dev_offline(fp, target);
			mtx_unload(fp, target, C_slot, -1);
		}
		mtx_load(fp, C_slot, val, target);
		goto again;
		break;
	case C_portal:
		ng_bleat(1, "found volid '%s' in portal %d", volid, val);
		if(changer.dte[target]){
			sfprintf(fp, "info: volid '%s' in portal, but target dte %d has a tape in it\n", volid, target);
			dev_offline(fp, target);
			mtx_unload(fp, target, C_portal, -1);
		}
		mtx_load(fp, C_portal, val, target);
		goto again;
		break;
	default:
		sfprintf(fp, "error: volid '%s' not found\n", volid);
		break;
	}
}

static void
status(Sfio_t *fp, char *what)
{
	int i;
	enum { DO_drives = 0x01, DO_media = 0x02 } todo;
#define	S(x)	((strcmp(what, x) == 0) || (strcmp(what, "all") == 0))

	todo = 0;
	if(S("drives"))		todo |= DO_drives;
	if(S("media"))		todo |= DO_media;
	ng_bleat(1, "status %x", todo);
	read_library();
	read_devmap();

	if(todo&DO_drives){
		sfprintf(fp, "Drives:\n");
		for(i = changer.min_dte; i <= changer.max_dte; i++){
			if(changer.dte[i] && (strcmp(changer.dte[i], NO_NAME) == 0))
				continue;
			sfprintf(fp, "dte %d:%7s", i, changer.dte[i]? changer.dte[i]:"");
			if(devmap.dte[i].sys)
				sfprintf(fp, " (%s %s:%s)", devmap.dte[i].tapeid, devmap.dte[i].sys, devmap.dte[i].dev);
			sfprintf(fp, "\n");
		}
	}
	if(todo&DO_media){
		sfprintf(fp, "Media:\n");
		for(i = changer.min_slot; i <= changer.max_slot; i++)
			sfprintf(fp, "slot %4d: %s\n", i, changer.slot[i]? changer.slot[i] : "<empty>");
		for(i = changer.min_portal; i <= changer.max_portal; i++)
			sfprintf(fp, "portal %4d: %s\n", i, changer.portal[i]? changer.portal[i] : "<empty>");
	}
}

static int smap[MAX_N];

static int
cmp1(const void *x, const void *y)
{
	int *a = (int *)x;
	int *b = (int *)y;
	char *s1, *s2;
#define	LOW	"!!!!!!!!"

	s1 = changer.slot[*a];
	if(s1 == 0)
		s1 = LOW;
	s2 = changer.slot[*b];
	if(s2 == 0)
		s2 = LOW;
	return strcmp(s1, s2);
}

static void
sortmedia(Sfio_t *fp, char *from, char *to)
{
	int a, b, i, k, j, nz;

	read_library();
	a = strtol(from, 0, 10);
	b = strtol(to, 0, 10)+1;
	ng_bleat(1, "sorting from %d to %d\n", a, b);
	nz = 0;
	for(i = a; i < b; i++)
		if(changer.slot[i] == 0)
			nz++;
	if(nz == 0){
		sfprintf(fp, "error: no empty slots\n");
		ng_bleat(1, "no empty slots: failing");
		return;
	}
	for(i = a; i < b; i++)
		smap[i-a] = i;
	qsort(smap, b-a, sizeof(smap[0]), cmp1);
	for(i = a; i < b; i++)
		sfprintf(sfstderr, "[%03d]: %10s -> %10s  (%d -> %d)\n", i, changer.slot[i], changer.slot[smap[i-a]], smap[i-a], i);
	k = 5;
	k--;
	for(;;){
		/* find first out of place slot */
		for(i = b-1; i >= a; i--)
			if(i != smap[i-a])
				break;
		if(i >= b){
			ng_bleat(1, "%d: think we're done", k);
			break;
		}
		j = i;
		/* find an empty slot? */
		if(changer.slot[j]){
			for(i = a; i < b; i++)
				if(changer.slot[i] == 0)
					break;
			ng_bleat(1, "move %d: %d -> %d (empty slot)", k, j, i);
			changer.slot[i] = changer.slot[j];
			changer.slot[j] = 0;
			mtx_transfer(fp, C_slot, j, C_slot, i);
			if(--k <= 0)
				break;
		}
		/* now follow this chain of slots */
		while(smap[j-a] != j){
			/* fill slot j from slot i */
			i = smap[j-a];
			ng_bleat(1, "move %d: %d -> %d", k, i, j);
			if(changer.slot[i] == 0)
				break;
			{
				mtx_transfer(fp, C_slot, i, C_slot, j);
				changer.slot[j] = changer.slot[i];
				changer.slot[i] = 0;
				if(--k <= 0)
					break;
			}
			j = i;
		}
		ng_bleat(1, "%d: chain done", k);
		for(i = a; i < b; i++)
			smap[i-a] = i;
		qsort(smap, b-a, sizeof(smap[0]), cmp1);
	}
	read_library();
	nz = 0;
	for(i = b-1; i >= a; i--)
		if(i != smap[i-a])
			nz++;
	if(nz == 0){
		sfprintf(fp, "okay: %d..%d sorted\n", a, b-1);
		ng_bleat(1, "sort succeeded");
	} else {
		sfprintf(fp, "error: not finished (%d out of place)\n", nz);
		ng_bleat(1, "sort: %d out of place", nz);
	}
}

/*
	unload a portal slot, but handle its contents correctly
*/
static void
unload1(Sfio_t *fp, char **list, int n, int i)
{
	int type, k;

	if(changer.portal[i]){
		/* is portal[i] in the list? */
		for(k = i-changer.min_portal+1; k < n; k++)
			if(strcmp(list[k], changer.portal[i]) == 0)
				break;
		if(k < n){	/* found it */
			k += changer.min_portal;
			unload1(fp, list, n, k);
			move1(fp, C_portal, i, C_portal, k);
		} else {
			type = find_empty(fp, &k);
			move1(fp, C_portal, i, type, k);
		}
	}	
}

static void
move1(Sfio_t *fp, int src_t, int src_n, int dest_t, int dest_n)
{
	char **s, **d;

	mtx_transfer(fp, src_t, src_n, dest_t, dest_n);
	switch(src_t)
	{
	case C_portal:	s = &changer.portal[src_n]; break;
	case C_slot:	s = &changer.slot[src_n]; break;
	default:	s = 0; break;
	}
	switch(dest_t)
	{
	case C_portal:	d = &changer.portal[dest_n]; break;
	case C_slot:	d = &changer.slot[dest_n]; break;
	default:	d = 0; break;
	}
	if((s == 0) || (d == 0)){
		ng_bleat(1, "internal error: move1(%d,%d -> %d,%d)", src_t, src_n, dest_t, dest_n);
		sfprintf(fp, "error: internal move1 error\n");
		return;
	}
	*d = *s;
	*s = 0;
}

static void
decant(Sfio_t *fp, char **list, int n)
{
	int i, j, k, err, ty, np, p;

	/*
		decanting is easy given unload1 above.

		first we check all the volumes are there in slots or portals.
		now we can use list as the set (of n names).
		then, go through the list one by one, and load the volume
		into the corresponding portal slot. we clear the portal slot with unload1,
		which ensures that if the slot to be cleared will be needed later,
		it gets loaded into the correct spot directly.
	*/

	read_library();
	err = 0;
	for(i = 0; i < n; i++){
		switch(vfind(list[i], &k))
		{
		case C_slot:
		case C_portal:
			break;
		case C_dte:
			sfprintf(fp, "error: volume %s in DTE, not slot/portal\n", list[i]);
			ng_bleat(1, "volume %s in DTE, not slot/portal", list[i]);
			err++;
			break;
		default:
			sfprintf(fp, "error: volume %s not found\n", list[i]);
			ng_bleat(1, "volume %s not found", list[i]);
			err++;
			break;
		}
	}
	if(err){
		ng_bleat(1, "%d decant errors found; cancelling decant", err);
		return;
	}
	np = changer.max_portal - changer.min_portal + 1;
	if(n > np){
		sfprintf(fp, "info: can't decant more than there are portals (%d); proceeding\n", np);
		ng_bleat(1, "can't decant more than there are portals (%d); proceeding", np);
		n = np;
	}
	for(i = 0; i < n; i++){
		p = i + changer.min_portal;
		sfprintf(fp, "info: moving %s (%d/%d) into portal %d\n", list[i], i+1, n, p);
		ng_bleat(1, "\tinfo: moving %s (%d/%d) into portal %d", list[i], i+1, n, p);
		if((changer.portal[p] == 0) || (strcmp(changer.portal[p], list[i]) != 0)){
			if(changer.portal[p])
				unload1(fp, list, n, p);
			ty = vfind(list[i], &k);
			move1(fp, ty, k, C_portal, p);
		}
	}
	if(n < np){
		sfprintf(fp, "info: clearing blank portal %d\n", n+1);
		ng_bleat(1, "\tinfo: clearing blank portal %d", n+1);
		unload1(fp, list, n, n+changer.min_portal);
	} else {
		sfprintf(fp, "info: portals full; no blank terminator\n");
		ng_bleat(1, "\tinfo: portals full; no blank terminator");
	}
	ng_bleat(1, "decant done");
	sfprintf(fp, "okay: decant done\n");
}

void
heartbeat(void)
{
	int fd;

	if(heartbeat_file == 0)
		return;
	fd = creat(heartbeat_file, 0666);
	if(fd < 0)
		perror(heartbeat_file);
	else
		close(fd);
	ng_bleat(1, "--heartbeat(%s) fd=%d", heartbeat_file, fd);
}

static void
identify(char *volid, char *drive)
{
	int dte;
	char buf[NG_BUFFER];

	read_library();
	if(vfind(volid, &dte) != C_dte){
		sfprintf(sfstderr, "identify(%s, %s) fails: vfind doesn't return dte\n", volid, drive);
		return;
	}
	sfsprintf(buf, sizeof buf, "ng_ppset -f DTE_%s_%d %s\n", domain, dte, drive);
	system(buf);
	ng_bleat(1, "identify: %s", buf);
}

static void
tdelete(char *volid, char *why)
{
	int i;
	Pool *p;

	ng_bleat(1, "deleting %s (why=%s) from %d pools", volid, why, npool);
	for(p = pool; p < pool+npool; p++){
		ng_bleat(3, "\tconsidering %d from pool %s", p->nvol, p->name);
		for(i = 0; i < p->nvol; i++){
			ng_bleat(3, "\t\t%d: %s", i, p->vol[i]);
			if(strcmp(volid, p->vol[i]) == 0){
				ng_bleat(1, "found %s, eliding", volid);
				for(i++; i < p->nvol; i++)
					p->vol[i-1] = p->vol[i];
				p->nvol--;
				write_pools();
				return;
			}
		}
	}
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_tp_changer -- tape library manager
&name   &utitle

&synop  ng_tp_changer [-v] [-p &ital(port)] tape_domain ...

&desc	&This is a server accepting various requests via &ital(ng_tpreq)
to perform various functions to do with managing tapes in the flock's
various tape libraries. Although there is normally only one changer,
there has been and may again be multiple changers; distinguished by &itail(tape_domain).
It is intended that requests come from outside the cluster;
accordingly, &this publishes its location as &cw(to be done).
&space
All requests are bidirectional.
&This has a simple parser; null arguments must be passed as the string &lit(-).
Supported requests can be divided into several groups:

&space
General
&beglist

&item	verbose &ital(level)
&break
Set the verbose value to &ital(level).

&space
&item	status &ital(what)
&break
Various status information about the tape changer is written on stdout.
&ital(What) is one of &cw(drives), &cw(media), and &cw(all).

&space
&item	mapdev &ital(drive)
&break
Return the DTE, driveid, node and device for the given drive.

&space
&item	identify &ital(volid) &ital(drive)
&break
This command asserts that the given volid is currently mounted
on the specified drive.
Nothing is done to teh drive or the volume;
this is part of the self-discovery process to determine which tape devices
correspond to which DTEs in the changer.

&space
&item	exit
&break
Cause &this to exit.

&endlist

&space
Media manipulation
&break
Generally, the commands that cause media to be manipulated will emit intermediate informative
output prior to their final output. All output lines will start with one of
&cw(info:) - this line is purely informative (for human consumption) and normally precedes
operations that may take a while,
&cw(error:) - the command failed and the rest of teh line explains what failed,
&cw(okay:) - the command succeeded and the format of teh rest of the line
is described below with the command.
&beglist
&item	loadvol &ital(volid) &ital(drive)
&break
The volume will be loaded the specified drive.
Currently, the drive can specified as a tape drive id, such as &cw(IBM_3580_822309)
(returned by &ital(ng_tp_getid)), or a number (dte).
The output will specify the dte, tape drive id, node and device.
The node will have some attributes set; perhaps &lit(Tapepool_)&ital(pool) and &lit(Tapevol_)&ital(volid).

&space
&item	unload &ital(drive)
&break
The volume in the specifed drive, if possible, will be unloaded back into a slot.

The node will have &lit(Tapepool_)&ital(pool) and &lit(Tapevol_)&ital(volid) attributes set.

&space
&item	transfer &ital(from) &ital(to)
&break
Transfer the medium in slot &ital(from) to the slot &ital(to).

&space
&item	decant &ital(volid ...)
&break
Transfer the specified volumes to a contiguous set of export/import slots,
ending with the largest numbered export/import slot.
If at all possible, an empty slot will precede this set
(to help mark physically the beginning of the set in the changer).

&space
&item	sort &ital(slota) &ital(slotb)
&break
Sort teh volumes in slots&ital(slota) to &ital(slotb) inclusive alphabetically by volid.
Even though the number of moves made is minimised, this can take quite a while.
Because of this, only 5 media moves are done per invocation.
(This allows other requests to get serviced.)
Simply repeat the sort command with teh same arguments until it succeeds.
If the set is not sorted, an error is returned reporting how many volumes are out of place.
When the set is sorted, an &cw(okay) status is returned.

&endlist
&space
&opts   &This understands the following options:

&begterms
&term 	-p &ital(port) : accept requests on port &ital(port).

&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&examp
See what media is currently loaded:
&litblkb
$ ng_tpreq status drives
Drives:
dte 0:        (IBM_3580_822309 bat08:/dev/nst0)
dte 1: B00002 (IBM_3580_82230c ant01:/dev/nst0)
dte 2: B00176 (IBM_3580_82230f ant04:/dev/nst0)
dte 3: B00023 (IBM_3580_822312 ant05:/dev/nst0)
&litblke
&space

Load a nonexistant tape:
&litblkb
$ ng_tpreq loadvol B002xx 3
error: volid 'B002xx' not found
&litblke
&space

Here is what can happen when loading some media:
&litblkb
$ ng_tpreq loadvol B00300 3
info: volid 'B00300' in slot, but target dte 3 has a tape in it
info: offlining tape in dte 3 (/dev/nst0 on ant05)
info: unloading tape from dte 3 to slot 58
info: moving slot 279 to dte 3
okay: B00300 3 IBM_3580_822312 ant05 /dev/nst0
&litblke
&space
&litblkb
$ ng_tpreq loadvol B00300 1
info: volid 'B00300' in wrong dte (3); put back in slot
info: offlining tape in dte 3 (/dev/nst0 on ant05)
info: unloading tape from dte 3 to slot 146
info: volid 'B00300' in slot, but target dte 1 has a tape in it
info: offlining tape in dte 1 (/dev/nst0 on ant01)
info: unloading tape from dte 1 to slot 147
info: moving slot 146 to dte 1
okay: B00300 1 IBM_3580_82230c ant01 /dev/nst0
&litblke

&mods
&owner(Andrew Hume)
&lgterms
&term	15 Jan 2004 (agh) : Original code
&term	12 Oct 2004 (agh) : major rewrite
&term	1 Feb 2006 (agh) : major revamp; actually useful now
&endterms
&scd_end
*/
