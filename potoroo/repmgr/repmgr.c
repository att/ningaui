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
#include	<fcntl.h>
#include	<errno.h>

#include "ningaui.h"
#include "ng_ext.h"
#include "repmgr.h"

#include "repmgr.man"

Symtab *syms;
Stats stats;
ng_uint64 iobytes;
ng_uint64 delbytes;
int nrequests;
int nalloc;
int safemode = 0;
int quiet = 0;
Tumbler *t;
char *trace_name;

#define		BURBLE_DELTA		300
#define		MUNGE_DELTA		120
#define		PURGE_DELTA		6*3600		/* 6 hours */

Chunk *readchunk(int, int *);
static void npurge(Syment *s, void *v);
static int infile(int fd, char **buf, int *len);
static int send_bt_mark( char *buf );
static char *my_name;
static int numf;
static char chkpt_name[NG_BUFFER];
static char next_chkpt_name[NG_BUFFER];
static char *version = "v1.2/05217";

int
main(int argc, char **argv)
{
	char stderrbuf[NG_BUFFER];
	int i;
	int qend = 0;
	long nextd = 0;
	long incd = 300;
	time_t tburble;
	time_t tmunge;
	time_t tpurge;
	char *port = ng_env("NG_REPMGRS_PORT");
	int ndid;
	int iport;
	int done;
	char *chkpt = 0;
	char *statsfile = ng_env("NG_RM_STATS");
	char *init = 0;
	char *tumbler = "NG_RM_TUMBLER";
	Sfio_t *sfp;
	Chunk *c;
	int serv_fd;
	char *s;
	ng_int64 tot_in;
	ng_int64 recycle_target;

	/* Process arguments */
	ARGBEGIN{
	case 'i':	SARG(init); break;	/* example: /tmp/rms.2 	*/
	case 'p':	SARG(port); break;
	case 'q':	qend = 1; break;
	case 's':	SARG(statsfile); break;
	case 'S':	safemode = 1; break;
	case 't':	SARG(tumbler); break;
	case 'v':	OPT_INC(verbose); break;
	default:	goto usage;
	}ARGEND
	if(argc != 0){
usage:
		sfprintf( sfstderr, "%s %s\n", argv0, version );
		sfprintf(sfstderr, "Usage: %s [-vS] [-p port] [-s stats] [-t tumbler]\n", argv0);
		exit(1);
	}

	if(safemode)
		sfprintf(sfstderr, "set to safe mode\n");

	ng_srand();
	signal(SIGPIPE, SIG_IGN);

	/* Initialize symbol table (splay tree) */
	syms = syminit(2499997);		/* was 249989 */
	if((serv_fd = ng_serv_announce(port)) < 0)
		exit(1);

	/* Get leader which by default is set to where repmgr is running */
	ng_sysname(stderrbuf, sizeof stderrbuf);
	if(s = strchr(stderrbuf, '.'))
		*s = 0;
	my_name = ng_strdup(stderrbuf);
	setvar(VAR_LEADER, my_name);

	/* Initialize tumblers. */
	if((t = ng_tumbler_init(tumbler)) == 0){
		sfprintf(sfstderr, "%s: tumbler init (%s) failed\n", argv0, tumbler);
		exit(1);
	}

	if(verbose > 1)
		sfprintf(sfstderr, "entering main loop\n");
	tburble = time(0)+BURBLE_DELTA/2;	/* wait a little while */
	tpurge = time(0) + PURGE_DELTA;
	tmunge = 0;
	iobytes = delbytes = nrequests = 0;
	done = 0;

	if(init){
		char *buf;
		int len;
		int ifd;

		if((ifd = open(init, O_RDONLY)) < 0){
			perror(init);
			exit(1);
		}
		if(infile(ifd, &buf, &len) < 0)
			exit(1);
		if(parse(buf, len, 0) == INPUT_EXIT)
			done = 1;
		ng_free(buf);
		close(ifd);
	}
	/* Keep processing until we receive a message to stop */
	recycle_target = -1;
	while(!done){
		/* read as much input as we can */
		ndid = 0;
		tot_in = 0;
		while(c = readchunk(serv_fd, &done)){
			tot_in += c->len;
			ng_bleat(2, "readchunk = #%x", c);
			if(parse(c->buf, c->len, c->ofp) == INPUT_EXIT)
				done = 1;
			freechunk(c);
			ndid++;
			if(((ndid%5) == 0) || (tot_in > 200000)){
				if(tmunge < time(0))
					break;
			}
		}
		if(done)
			break;
		/* done absorbing inputs; now think about the world */
		if((ndid > 0) || (tmunge < time(0))){
			dump(ng_env("RM_DUMP"));
			munge(&stats);
			tmunge = time(0) + MUNGE_DELTA;
			if(recycle_target < 0){
				if(stats.pass_ut < 300)
					stats.pass_ut = 300;	/* make at leats 3s */
				recycle_target = 4*stats.pass_ut;
				sfprintf(sfstderr, "set recycle_goal to %I*d\n", sizeof(recycle_target), recycle_target);
			}
			if(stats.pass_ut >= recycle_target){
				sfprintf(sfstderr, "munge time (%I*d) >= recycle_goal(%I*d): exiting\n",
					sizeof(stats.pass_ut), stats.pass_ut, sizeof(recycle_target), recycle_target);
				ng_log(LOG_WARNING, "munge time (%I*d) >= recycle_goal(%I*d): exiting\n",
					sizeof(stats.pass_ut), stats.pass_ut, sizeof(recycle_target), recycle_target);
				done = 1;
			}
			if(sfp = sfopen(0, statsfile, "w")){
				prstats(&stats, sfp);
				sfclose(sfp);
			} else
				perror(statsfile);
			sleep(3);
		} else
			sleep(10);
		if(tburble < time(0)){
			tburble = time(0) + BURBLE_DELTA;
			sfprintf(sfstderr, "periodic burble\n");
			ng_log(LOG_INFO, "DATA MOVEMENT: cprm %.0fKB, %d requests\n", (iobytes+512)/1024.0, nrequests);
			ng_log(LOG_INFO, "DATA MOVEMENT: delrm %.0fKB\n", (delbytes+512)/1024.0);
			checkpoint(0);
		}
		if(tpurge < time(0)){
			tpurge = time(0)+PURGE_DELTA;
/*			symtraverse(syms, SYM_NODE, npurge, 0);		do something to the node; but why? */
		}
	}

	ng_serv_unannounce(serv_fd);
	if(verbose)
		sfprintf(sfstderr, "%s: unannouncing port, preparing to exit\n", argv0);
	if(qend){	/* mainly for timing runs */
		sfprintf(sfstderr, "nalloc=%d\n", nalloc);
		exit(0);
	}

	/* cleanup */
	checkpoint(1);
	if(verbose)
		sfprintf(sfstderr, "%s: exiting normally\n", argv0);

	exit(0);
}

/* send replay cache marker to bt 
   buf is expected to be the chkpt file name (it can be full path)
*/
static int
send_bt_mark( char *buf )
{
	char	*p;
	char	wbuf[2048];
	int	btfd = -1;
	char	*host; 				/* where bt is running */
	char	*port;

	if( (p = strrchr( buf, '/' )) )		/* basename */
		p++;
	else
		p = buf;

	if( (host = ng_pp_get( "srv_Filerbt", NULL )) == NULL  || !*host )
	 	host = "localhost";					/* Filerbt not defined assume it is running locally */

	if( (port = ng_pp_get( "NG_RMBT_PORT", NULL )) == NULL  || !*port )		/* it will soon be this to be less confusing */
	{
		if( (port = ng_pp_get( "NG_RM_PORT", NULL ))  == NULL || !*port )
		{
			ng_bleat( 0, "send_bt_mark: cannot find NG_RMBT_PORT or NG_RM_PORT in pinkpages" );
			return 0;
		}
	}

	if( ng_rx( host, port, &btfd, NULL ) == 0  )		/* we have both in and out on the same fd */
	{
		ng_bleat( 1, "rmbt mark set: %s", p );
		sfsprintf( wbuf, sizeof( wbuf ), "~rcmark~%s\n", p );	/* the mark command */
		write( btfd, wbuf, strlen( wbuf ) );		
		ng_testport( btfd, 10 );			/* wait up to 10 sec for an ack */
		close( btfd );					/* we ignore the ack and close up */
	}
	else
	{
		ng_bleat( 1, "unable to connect to rmbt: %s %s: marker not set: %s", host, port, p );
		return 0;
	}

	return 1;
}

static int
infile(int fd, char **buf, int *len)
{
	char *b;
	int n, a, k, i;
	int rcount = 0;
	int sent_ack = 0;
	char wbuf[256];

	a = 2*1024*1024;
	b = ng_malloc(a, "infile buf");
	n = 0;
	*b = 0;
	/*while((k = read(fd, b+n, a-n)) > 0){*/
	while((k = ng_read(fd, b+n, a-n, 5)) > 0){		/* block at most 5 seconds */
		rcount++;
#ifdef DO_DEBUG
		strncpy( wbuf, b+n, 50 );		/* debug */
		wbuf[k>50 ? 50 : k] = 0;				/* debug */
		if( verbose  )
			sfprintf( sfstderr, "%u infile: %d(fd) %d(bytes) (%s)\n", (ng_uint32)time(0), fd, k, k > 10 ? wbuf : "..." ); /* debug */
#endif

		for(i = 0; i < k; i++)
		{
			if( (0xFE & b[n+i]) == EOF_CHAR ){			/* end of data is either 0xbe or 0xbf; if bf then we send ack */
				if( (sent_ack = b[n+i] & 0x01 ) )				/* send hand shake back if 0x01 bit is on */
				{
#ifdef DO_DEBUG
					if( verbose )
						sfprintf( sfstderr, "%u infile: sent EOD ack\n", (ng_uint32)time(0) );
#endif
					
					write( fd, "\n", 1 );
				}

				n += i;
				goto done;
			}
		}

		n += k;
		if((a-n) < 1024){
			if(a < 16*1024*1024)
				a = 16*1024*1024;
			a *= 2;
			b = ng_realloc(b, a, "infile buf");
		}

	}
done:

	if( verbose )
		sfprintf( sfstderr, "%u infile: %d(fd) %d(bytes) %d(last) %s [%s]\n", (ng_uint32)time(0), fd, n, k, sent_ack ? "EOD_ACK" : "no-ack", k < 0 ? strerror( errno ) : "OK" ); /* debug */
	*buf = b;
	*len = n;
	return 0;
}

static void
npurge(Syment *s, void *v)
{
	Node *n = s->value;

	n->purgeme = 1;
}

Chunk *
readchunk(int sfd, int *fatal)
{
	Chunk *c = 0;
	int n, k;
	int fd;
	char buf[1];

	fd = ng_serv_probe_wait(sfd, 1);		/* block for at most 1 second */
	if(fd <= 0){
		if(fd < 0)
			*fatal = 1;
		return 0;
	}
	buf[0] = '\n';
	if((k = write(fd, buf, 1)) != 1)
		perror("handshake write");
	sfprintf(sfstderr, "handshake wrote %d bytes\n", k);

	c = ng_malloc(sizeof(*c), "chunk");
	c->ofp = sfnew(0, (Void_t*)1, SF_UNBOUND, fd, SF_WRITE);
	if(infile(fd, &c->buf, &c->len)){
		sfprintf(sfstderr, "infile error\n");
		freechunk(c);
		return 0;
	}
	return c;
}

void
freechunk(Chunk *c)
{
	if(c->buf)
		ng_free(c->buf);
	if(c->ofp)
		sfclose(c->ofp);
	ng_free(c);
}

static void
act1(Syment *s, void *v)
{
	Sfio_t *fp = v;
	File *f = s->value;
	Attempt *a;

	for(a = f->attempts; a; a = a->next){
		sfprintf(fp, "%s: ", f->name);
		switch(a->type){
		case A_create:
			sfprintf(fp, "copy from %s to %s lease=%u", a->from->name, a->to->name, a->lease);
			break;
		case A_destroy:
			sfprintf(fp, "delete from %s", a->from->name);
			break;
		case A_move:
			sfprintf(fp, "move to %s on %s lease=%u", f->rename, a->from->name, a->lease);
			break;
		}
		sfprintf(fp, "\n");
	}
}

void
active(Sfio_t *fp)
{
	symtraverse(syms, SYM_FILE, act1, fp);
	symtraverse(syms, SYM_FILE0, act1, fp);
}

/************************************************************************/
/* Checkpoint
 *
 *	Dump the symbol table and save to all nodes
 *
 * Public:  checkpoint()
 * Private: dumpf(), defile()
*/
/************************************************************************/

static void
dumpf(Syment *s, void *v)
{
	Sfio_t *fp = v;
	File *f = s->value;
	char c;
	char *p;

	sfprintf(fp, "file %s %s %I*d %d\n", f->md5, f->name, sizeof(f->size), f->size, f->noccur);
	if(f->hood[0]){
		sfprintf(fp, "hood %s %s", f->md5, f->name);
		c = ' ';
		for(p = f->hood; *p; ){
			sfprintf(fp, "%c%s", c, p);
			while(*p++)
				;
			c = ',';
		}
		sfprintf(fp, "\n");
	}
	if(f->rename)
		sfprintf(fp, "mv %s %s\n", f->name, f->rename);
	if(f->urgent)
		sfprintf(fp, "urgent %s\n", f->name);
	numf++;
}

static void
dumpv(Syment *s, void *v)
{
	Sfio_t *fp = v;

	sfprintf(fp, "set %s %s\n", s->name, s->value);
}

static void
defile(Syment *s, void *v)
{
	if(strcmp(s->name, chkpt_name) && strcmp(s->name, next_chkpt_name))
		s->space = SYM_UNUSED;
}

/************************************************/
/*	Dump symbol table and replicate it	*/      
/************************************************/
void
checkpoint(int final)
{
	Sfio_t *fp;
	char md5[NG_BUFFER], buf[NG_BUFFER], dest[NG_BUFFER];
	off_t size;
	char *b;
	Syment *s;
	File *f;
	Node *node;
	ng_timetype now;

	/* Get new checkpoint file name */
	numf = 0;
	strcpy(chkpt_name, ng_tumbler(t, 0, 0, 0));
	strcpy(next_chkpt_name, ng_tumbler(t, 0, 1, 1));

	/* this is bogus; must fix  */
	if(s = symlook(syms, my_name, SYM_NODE, 0, SYM_NOFLAGS))
		node = s->value;
	else
		node = 0;
	if(node && (b = ng_spaceman_rand(node->spacefslist, chkpt_name)))
		strcpy(dest, b);
	else
		sfsprintf(dest, sizeof dest, "/ningaui/fs00/00/00/%s", chkpt_name);
	if((fp = sfopen(0, dest, "w")) == NULL){
		perror(dest);
		return;
	}

	now = ng_now();
	sfprintf(fp, "comment creation time %I*d\n", sizeof(now), now);
	/* Traverse the symbol table for files */
	symtraverse(syms, SYM_FILE, dumpf, fp);
	symtraverse(syms, SYM_FILE0, dumpf, fp);
	symtraverse(syms, SYM_VAR, dumpv, fp);
	sfclose(fp);
	send_bt_mark(chkpt_name);
	sfsprintf(buf, sizeof buf, "cp %s /ningaui/site/rm/rm.cpt", dest);
	system(buf);
	sfsprintf(buf, sizeof buf, "md5sum %s | ng_rm_size", dest);
	fp = sfpopen(0, buf, "r");
	sfscanf(fp, "%s %*s %I*d", md5, sizeof(size), &size);
	sfclose(fp);
	ng_log(LOG_INFO, "REPMGR CHKPT %s %s (%d files) %s\n", chkpt_name, dest, numf, md5);
	sfprintf(sfstderr, "CHKPT %s %s (%d files)\n", chkpt_name, dest, numf);
	newfile(chkpt_name, md5, size, REP_DEFAULT);
	sfsprintf(buf, sizeof buf, "ng_flist_add %s %s &", dest, md5);
	system(buf);
	addinstance(0, my_name, dest, md5, 1, 0, 0);

	/* preclear next chkpoint file */
	SETBASE(b, next_chkpt_name);
	if(s = symlook(syms, b, SYM_FILE, 0, SYM_NOFLAGS)){
		f = s->value;
		f->noccur = 0;
	}
	if(final){
		/* need to force replication */
		symtraverse(syms, SYM_FILE, defile, 0);
		enablemunge(2);
		munge(&stats);
	}
}

void
setvar(char *name, char *value)
{
	if(verbose)
		sfprintf(sfstderr, "set %s to '%s'\n", name, value);
	symlook(syms, name, SYM_VAR, ng_strdup(value), SYM_COPYNM);
}

char *
getvar(char *name)
{
	Syment *s;

	if(s = symlook(syms, name, SYM_VAR, 0, SYM_NOFLAGS))
		return s->value;
	else
		return 0;
}

void
unsetvar(char *name)
{
	if(verbose)
		sfprintf(sfstderr, "unset %s\n", name);
	symdel(syms, name, SYM_VAR);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_repmgr:Perform replication manager functions.)

&space
&synop	ng_repmgr [-p port] [-v] [-s stats]


&space
&desc	The replication manager manages a set of files across the cluster.
	It works from two sets of inputs; the first are lists of existing files
	transmitted from nodes in the cluster, and the second are directives
	about what files are to replicated, how many copies, and optionally,
	nonbinding hints as to where, or where not, to put copies.

&space
&opts	The following options are recognised by &ital(ng_log):
&begterms
&term 	-p port : respond to calls on port number &ital(port).
	The default value is &cw($NG_RM_PORT).

&term 	-s stats : generate periodic statistical summaries on file &ital(stats).
	The default value is &cw($NG_RM_STATS).

&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&space
&parms	&ital(Nm_repmgr) responds to the following commands (supplied by &ital(ng_rmreq)):
&space
&begterms
&term	copylist &ital(file) :	This reads in a file containing the list of instances on a node.
	The format of this file is some general information:
		nodename
		timestamp of this list
		lease (when this list expires)
		available disk space (MB)

	followed by a line per file instance:
		filename md5 size
&space
&term	file &ital(md5) &ital(name) &ital(size) &ital(ncopies) :	This specifies a file
	to be managed. The size is in bytes, and the number of copies is interpreted
	as follows:
	> 0	the actual number of copies
	0	delete all copies
	-1	a copy on every node
	-2	the system default number of copies
	-4	however many exist

&space
&term	hint &ital(md5) &ital(name) &ital(lease) &ital(spec) : Specify a nonbinding
	hint as to where copies for the named file should be put. &ital(Spec)
	denotes which nodes the file should be on, unless the first character
	is &cw(!), which says &ital(not) those nodes. Nodenames can be seperated by a
	comma but &ital(spec) must have no embedded whitespace. The hint expires
	by time &ital(lease).

&space
&term	instance &ital(node) &ital(md5) &ital(name) : Identify an instance
	of a file. This is intended as a quick way announce instances;
	the preferred mechanism for bulk notices remains &cw(copylist).

&space
&term	hood &ital(md5) &ital(name) &ital(nhood) : Specify the neighborhood for the given file.
	This means that replications will be made only on the nodes specified
	(unless we need more replications than nodes in the neighborhood).
	&ital(Nhood) consists of node names separated by a comma with no embedded whitespace.
	If &ital(nhood) is teh string &cw(Null), then the file has no neighborhood.
&space
&term   mv &ital(old) &ital(new) :  Causes all instances of the file &ital(old) to be renamed to &ital(new.)
        The location of the files should remain unchanged. 

&space
&term	urgent name : Specify the given file is to be replicated urgently.

&space
&term	active : Dump out the list of current activities.

&space
&term	set var value : Set the symbolic name &ital(var) to contain &ital(value).
	&ital(Value) must contain no embedded whitespace and any symbolic names
	within &ital(value) are expanded immediately.
	Symbolic names must start with a capital letter but otherwise follow C variable
	name syntax. The symbolic name &cw(Leader) is preset; others must be defined by &cw(set).

&term	unset var : Remove the symbolic name &ital(var).

&space
&term	dump file : Dump the current state of the replication manager into the named file.

&space
&term	dump1 file : Dump the current state of the named file.

&space
&term	dump2 md5 file : Dump the current state of the named file.

&space
&term	dump5 : Dump the current state of the nodes and registered files.
	For files, the number of copies is given as the number used (instead of a symbolic value).
	(Suitable for doing space analysis.)

&space
&term	explain file : Write a detailed explanation of the current activity into the named file.
	A file name &cw(off) means stop explaining.

&space
&term	checkpoint : Generate a checkpoint file.

&space
&term	exit : Make &ital(ng_repmgr) exit as soon as it can.

&endterms

&space
&exit	When the binary completes, the exit code returned to the caller 
	is set to zero (0) if the number of parameters passed in were 
	accurate, and to one (1) if an insufficent number of parameters
	were passed. 

&space
&see	ng_rmreq(lib)  ng_hood

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	03 Aug 2001 (agh) : Original code 
&term	07 Oct 2005 (sd) : Added function to send marker to repmgrbt. 
&term	05 Mar 2007 (sd) : Added support to return an ack when the end of data marker has the 
		ack bit set. (v1.1/03217)
&term	21 May 2007 (sd) : Converted read() in infile() to ng_read() with a timeout of 5s.
&term	08 Oct 2008 (sd): Added mv command to man page. 
&endterms
&scd_end
*/
