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

#include	<pthread.h>

#define	MAX_NODES	128		/* maximum number of nodes */
typedef enum { Off, On, Undecided } Goal;
extern char *gname[];
#define		MALLOC_SUX	1
/* test case run1: 77915/291u (orig), 15143/291u (instance 1000), */
#define	EOF_CHAR	0xbe		/* this sucks, as do sockets in general; see rmreq.c */

typedef struct {
	char		**p;		/* pointers to lines */
	int		ap;		/* allocated ptrs */
	int		np;		/* used ptrs */
	char		*b;		/* line buf */
	int		ab;		/* allocated */
	int		nb;		/* used */
} Sortlist;
extern Sortlist *newsortlist(void);
extern void delsortlist(Sortlist *);
extern void addline(Sortlist *, char *);
extern void sortlist(Sortlist *);

typedef struct {
	char		*name;
	ng_uint32	nodelete;	/* set if we can't delete on this node */
	ng_uint32	lease;
	ng_uint32	total;		/* MB */
	ng_uint32	avail;		/* MB */
	int		purgeme;	/* purge on next copylist */
	int		index;		/* in node map */
	float		weight;		/* temporary weighting */
	int		deletes;	/* pending */
	int		copies;		/* pending */
	int		renames;	/* pending */
	int		hand;		/* where we stopped last time */
	int		nd, nc, nr;	/* this munge */
	int		xc, xd, xr;
	int		hint_type;	/* temporary work pads */
	int		count;		/* temporary work pads */
	char		*rbuf;
	int		rlen;		/* length of rbuf */
	int		rn;		/* next available rbuf */
	Sortlist	*mine;
	char		*spacefslist;	/* fs list for spaceman interface */
	char		*spacelist;	/* where the space is */
	char		*nattr;		/* attributes */
} Node;
#define	NCOPYLISTS	2		/* first is not really sent, allow second to settle */
#define	SPACEMANTRIES	3		/* times we'll try spaceman per node per munge */

typedef struct Instance {
	Node		*node;
	char		*path;
	char		*md5;
	char		deleted;
	ng_int64	size;
	struct Instance	*next;
} Instance;
extern Instance *instance(Node *node, char *path, char *md5, ng_int64);

typedef enum { A_create, A_destroy, A_move } Attempt_type;
typedef struct Attempt {
	Attempt_type	type;
	Node		*from, *to;
	ng_uint32	lease;
	ng_uint32	create;
	struct Attempt	*next;
} Attempt;
extern Attempt *attempt(Attempt_type type, Node *from, Node *to, ng_uint32 lease, Attempt *next);
extern void delattempt(Attempt *a);

/* precedence: most important hints have lowest numbers */
typedef enum { H_must_on, H_must_off, H_on, H_off, H_none  } Hint_type;
typedef struct Hint {
	Hint_type	type;
	char		*node;
	ng_uint32	lease;
	struct Hint	*next;
} Hint;
extern Hint *hint(Hint_type, char *node, ng_uint32 lease);
extern void delhint(Hint *h);

typedef struct {
	char		*name;
	char		*md5;
	char		*rename;	/* only nonzero if renaming it */
	off_t		size;
	int		urgent;
	ng_int16	matched;	/* at least teh right number */
	ng_int16	stable;		/* stable at correct number */
	int		ngoal;		/* target number */
	int		noccur;
	int		token;
	char 		*hood;		/* null sep and term */
	int		nhood;		/* length of hood */
	char		*goal;		/* goal for this node */
	Hint		*hints;
	Instance	*copies;
	Attempt		*attempts;
} File;
extern File *file(char *name, char *md5, off_t size, int noccur);
extern void delfile(File *f);

/* symbol table bnamespaces */
enum { SYM_UNUSED, SYM_NODE, SYM_FILE, SYM_INSTANCE, SYM_VAR, SYM_FILE0 };
/* input status */
enum { INPUT_OKAY, INPUT_EXIT, INPUT_NONE, INPUT_ERROR };

/* if you change this set, see nawab*.y */
#define		DFT_OCCUR	1	/* number of occurences when we don't know */
#define		REP_ALL		-1	/* all nodes */
#define		REP_DEFAULT	-2	/* ng_env("RM_NREPS") */
#define		REP_LDEFAULT	-3	/* ng_env("RM_NREPS") but include Leader */
#define		REP_WHATEVER	-4	/* however many there are */

#define		VAR_LEADER	"Leader"
#define		VAR_NULL	"Null"

typedef struct {
	int	instance_n;
	int	file_n;
	int	match_n;
	int	stable_n;
	int	delete_n;
	int	zerocopies_n;
	int	pass_t;
	int	pass_ut;		/* in .01s */
	int	itocopy_n;
	double	itocopy_b;
	int	itodelete_n;
	double	itodelete_b;
	int	itomove_n;
	double	itomove_b;
} Stats;

typedef struct Chunk {
	char		*buf;
	int		len;
	Sfio_t		*ofp;
	struct Chunk	*next;
} Chunk;
extern void freechunk(Chunk *);

extern Symtab *syms;
extern int parse(char *, int, Sfio_t *);
extern void munge(Stats *);
extern void prfile(File *s, Sfio_t *, char sep);
extern void prstats(Stats *, Sfio_t *);
extern const char *ng_spaceman_err(void);
extern void copylist(char *flist);
extern char *ng_spaceman_req(char *node, char *fname, ng_int64 size);
extern void addinstance(Node *node, char *noden, char *filen, char *md5, ng_int64, int *pni, int *pnf);
extern void dropinstance(Node *node, char *filen, char *md5);
extern void delinstance(Instance *i);
extern File *newfile(char *name, char *md5, ng_int64 size, int noccur);
extern void checkpoint(int final);
extern void *input_thread(void *);
extern void enablemunge(int yes);
extern void dump(char *filename);
extern ng_uint64 iobytes;
extern ng_uint64 delbytes;
extern int nrequests;
extern int quiet;
extern int safemode;
extern char *trace_name;
extern Node *new_node(char *noden);
extern void purge(Node *node);
extern char *getvar(char *name);
extern void setvar(char *name, char *value);
extern void unsetvar(char *name);
extern void explainf(char *file, char *name);
extern void active(Sfio_t *fp);

#define	SETBASE(base, path)	if(base = strrchr(path, '/')) base++; else base = path

/* workflow stuff */
extern void begin_attempt(Attempt *);
extern void end_attempt(Attempt *, File *, int complete);
extern void saw_copylist(Node *, int);
extern void prflow(Sfio_t *);
extern void ungoal(File *f);
extern void prgoal(char *label, char *g, Sfio_t *fp);

#ifdef DONTDEFINE
#define	syminit(nhash)	_syminit(nhash)
#define	symlook(st, name, space, install, flags)	_symlook(st, name, space, install, flags)
#define	symdel(st, sym, space)	_symdel(st, sym, space)
#define	symtraverse(st, space, fn, arg)	_symtraverse(st, space, fn, arg)
#define	symstat(st, fp)	_symstat(st, fp)
#endif
