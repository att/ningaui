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
	mods:
		28 Dec 2005 (sd) : Changed type of expiry vars in structs to time_t; 
				eliminates warnings at compile time.
*/
#include	"ng_ext.h"
#include	"stdio.h"

#define	WOOMERA		"ng_woomera"
#define	REQ		"req"
#define	STARTUP		"startup"
#define	ERRCHAR		'!'
#define	WLOG		LOG_ERR
#define SOCKET_READ_TO	1800			/* socket read timeout -- seconds */

#define	NOW()		(time(0))		/* timestamps */

#define	LS_STDERR	0			/* log states -- to standard error */
#define LS_MASTER	1			/* log chatty messages to master log */

extern int LONG_TIMEOUT;
extern int SHORT_TIMEOUT;
#define	MAX_LIMIT		1000		/* max value for pmax, pload limit */

extern	int	scan_req(int);
extern	int	grok(char *file, FILE *in, FILE *out);
extern	Symtab	*syms;
extern	int	log_state; 

typedef struct Resource
{
	char		*name;
	int		limit;
	int		running;		/* actually running */
	int		runable;		/* could run */
	int		count;			/* in system */
	int		paused;
	int		blocked;		/* blocked because of this resource */
	time_t		expiry;
	struct Resource	*next;
} Resource;
extern Resource	*new_resource(char *name);
extern Resource	*resource(char *name);		/* the one to use normally */
extern void setpause(char *rname, int val);

typedef struct Channel
{
	FILE		*fp;
	int		count;
	struct Channel	*next;
} Channel;
extern Channel	*channel(FILE *);
extern void	inc_channel(Channel *);
extern void	dec_channel(Channel *);

#define	MAX_PRI		100			/* maximum priority */
#define	DEFAULT_PRI	20			/* default priority */

typedef enum { N_root, N_job, N_op, N_rsrc } Node_Type;
typedef enum {
	J_unknown, J_not_ready, J_running,
	J_finished_okay, J_finished_bad, J_purged
} Job_Status;
/* if you add ops, amend table in explain.c */
typedef enum {
	O_exit, O_explain, O_listr, O_lists, O_list, O_dump, O_save, O_push,
	O_pmin, O_pmax, O_pload, O_limit, O_purge, O_run, O_resume, O_pause,
	O_verbose, O_blocked, O_times
} Node_Op;
extern char *opstr[];

typedef struct
{
	int		any;			/* set if any finish okay */
	struct Node	*node;
} Prereq;
typedef struct Node
{
	char		*name;
	Node_Type	type;
	Job_Status	status;
	char		ready;
	char		force;
	char		delete;
	char		onqueue;
	time_t		expiry;			/* time after which you can prune */
	Node_Op		op;
	int		pri;			/* just fer jobs */
	char		*cmd;			/* just fer jobs */
	Channel		*out;
	Resource	*rsrc;			/* just fer resources */
	Prereq		*prereqs;
	int		nprereqs;		/* number used */
	int		aprereqs;		/* number allocated */
	Resource	**rsrcs;
	int		nrsrcs;			/* number used */
	int		arsrcs;			/* number allocated */
	char		**args;
	int		nargs;			/* number used */
	int		aargs;			/* number allocated */
	int		pid;			/* just for jobs; the pid while running */
	struct Node	*next;			/* just for delete chain */
} Node;
extern Node	*node_r;			/* root node */
extern Node	*new_node(Node_Type);
extern void	add_prereq(Node *n, Node *pnode, int pany);
extern void	nset(Node *dest, Node *src);		/* set contents of dest to src */
extern void	ndel(Node *n, int reuse);
extern char	*node_name(Node *n);
extern void	add_rsrc(Node *, Resource *);
extern void	pop_rsrc(Node *);
extern void	inc_rsrc(Node *, int running);
extern void	dec_rsrc(Node *, int running);
extern void	add_arg(Node *, char *);

#define	NAME(n)	(n->name? n->name:"<anon>")

typedef enum {
	S_JOB,		/* job name -> Node * */
	S_RESOURCE	/* resource name -> Resource * */
} Symspace;

typedef struct
{
	int	notready;		/* jobs not ready */
	int	inqueue;		/* ready but not running */
	int	running;		/* running */
	int	completed;		/* finished */
} Stats;
extern Stats wstat;

extern int	execute(Node *);
extern void	jadd(Node *);
extern void	jqueue(Node *, int head);
extern void	jdelete(Node *);
extern void	jpoke(void);
extern int	jcatch(int wait);
extern void	jpmin(int);
extern void	jpmax(int);
extern void	jpload(int);
extern void	jpinit(void);
extern void	jrlimit(char *, int);
extern void	jforce(char *jname, int promote);
extern int	load(int quick);
extern void	explain(int, char **, FILE *);
extern void	dump(FILE *);
extern void	prune(void);
extern void	startup(char *file, int doreq);
extern void	save(Node *root, char *path);
extern void	blocked(FILE *fp);
extern void	prtimes(char *s, FILE *fp);

extern int	pmax;
extern int	pload;


extern void	ng_ref(void *);
