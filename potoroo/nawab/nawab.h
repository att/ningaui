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

#define NAWAB	1			/* prevents unwanted things from repmgr.h */

#define CHECKPT_DELAY	3000		/* delay between check points - 5 min */

#define	PROGID_MAX	268435450	/* programme ids wrap at this point */
#define PROGID_STEP	10		/* space between to allow (progid + XXX_SPACE) in symlook calls */

#define RECOVERY_MODE	0		/* for routines that need to know if we are running a recovery file */
#define NORMAL_MODE	1		/* not in recovery */

					/* symbol table name spaces  used with prog id to scope things */
#define	TROUPE_SPACE	0
#define VAR_SPACE	1		/* variables */
#define VAL_SPACE	2		/* values */
#define IONUM_SPACE	3		/* translation of io name to a %in or %on number for seneschal */
/* these values must not exceed PROGIDS_STEP-1 (currently 9) */

/* absolute symbol table spaces, must be < 10 as progids start at 10 */
#define PROG_SPACE	9		/* programme names */
#define ID_SPACE	8		/* used to block the use of a progid until the programme is done and/or purged */

#define EX_ALL		0		/* explain options -- expain everything (potential disaster) */
#define EX_FAIL		1		/* explain progammes, and troupe when there are failed jobs (default) */
#define EX_PROG		2		/* explain programme info only; no troupe/job info explained */

						/* options passed to various functions */
#define OPT_VAR_STRING		0x01		/* rtn should return a string that user must free */
#define OPT_CONST_STRING	0		/* routine should return a constant string (user should not alter) */

						/* toupe state constants */
#define TS_PENDING	1			/* troupe is blocked on other troupes */
#define TS_ACTIVE	2			/* all jobs have been submitted to seneschal */
#define	TS_ERROR	3			/* all jobs were exected; some completed with errors -- programme retained for longer time */
#define TS_DONE		4 			/* all jobs have completed with zero errors -- program retained for a short time */
#define TS_SUBMIT	5			/* jobs are being submitted; submission interrupted to allow nawab to do other tasks */

						/* i/o block flags (byte!) */
#define FLIO_IGNORESZ	0x01			/* input file size to be ignored in sizing job */

						/* gflags constants */
#define GF_WORK_SUSP	0x01			/* some work suspended to check udp/tcp messages -- call prog_maint after short delay */
#define GF_NEW_PROG	0x02			/* new programme(s) received since last checkpoing -- force a chkpt sooner rather than later */

#define SHORT_DELAY	1			/* seconds to set alarm when in short delay mode */
#define NORM_DELAY	3			/* normal delay between calls to prog maint */

#define SESSION_RESET	2			/* need to send jobs to seneschal when have_seneschal set to this */
#define SESSION_OK	1			/* session wiht seneschal is ok */
#define SESSION_BAD	0			/* when we have lost the seneschal session */

						/* job states  code will use <RUNNING to submit a job, so all FIN_ must be > RUNNING */
#define UNSUBMITTED	0			/* job has not been submitted to seneschal */
#define RUNNING		1			/* job was passed to seneschal */
#define FIN_OK		2			/* job finished ok  -- this MUST be the lowest of the FIN_ cases. all > FIN_OK are errors */
#define FIN_BAD		3			/* job finished bad */

#define	RE_SUBMIT_ALL		-1			/* flags to resubmit job routine */
#define	RE_SUBMIT_BAD		-2			/* submit only those jobs that failed */
#define	RE_SUBMIT_UNSUB		-3			/* submit only those jobs that were not submittted */
#define	RE_SUBMIT_RUNNING	-4			/* submit only those jobs that we have not received a status for */

#define SYMTAB_FREE_VALUE SYM_USR1		/* when we purge from symtab, we free what an entry points at if this set */

					/* audit message constants */
#define AUDIT_ID	"NAWAB_AUDIT"		/* added as a label to message */
#define AUDIT_NEWPROG	"newprog"		/* programme received and started */
#define AUDIT_PROGEND	"progend"		/* programme is done, but no errors */
#define AUDIT_PROGERROR	"progerrors"		/* programme is done, but errors */

/* these enum names are picked up by mkfile and put into a string format in n_enum.h
   order is not important, but the fact that they begin with T_ is.  Nothing else should
   begin with T_ in this file that is not a part of this list.
*/
typedef enum {
	T_inodes, T_ipartitions, T_ipartnum, T_iint, T_ibquote, T_ifile, T_ipipe,		/* iterators */
	T_vname, T_vstring, T_vpname, T_vrange, T_bquote, T_rlist,	/* values */
	T_djob, T_dassign, T_dcall					/* descs */
} Type;

/* data types for conversion in add2mrad() */
typedef enum { 
	DT_progid, DT_type, DT_name, DT_value, DT_desc, DT_after, DT_range, 
	DT_input, DT_output, DT_cmd, DT_rsrcs, DT_attempts, DT_priority, DT_reqin,
	DT_comment, DT_woomera, DT_nodes, DT_consumes, DT_keep
} Datatype;

/* 	for descriptors  that share a common iteration, this structure will 
	be referenced multiple times. Thus each time it is referenced
	the ref counter must be incremented, and purge_iter should be 
	the only call used to delete this
*/
typedef struct {
	Type		type;		/* node or partition name, or partition number */
	char		*app;		/* name of the application (for partition determination) or filename etc */
	int		lo, hi;		/* for iint; lo..hi inclusive */
	char		*lostr;		/* strings for x .. y so that they can be var names */
	char		*histr;
	int		ref;		/* number of times referenced */
	char		*fmt;		/* format string if one is needed */
} Iter;

typedef struct {
	char		*pname;		/* parameter name to use as the iteration variable (e.g. %n or %p) */
	Iter		*iter;		
} Range;

typedef struct Rlist {
	Range		*r;
	struct Rlist	*next;
} Rlist;

typedef struct {
	Type		type;
	char		*sval;
	Range		*range;
} Value;

typedef struct Io {
	char		*name;				/* name - can be referenced by other i/o commands in programme */
	Value		*val;				/* file pattern string if supplied with name = string */
	char		*gar;				/* command needed to cause the file to be created */
	char		*rep;				/* replication manager rep information: Leader Default Every <symbolic> */
	Rlist		*rlist;				/* range list */
	int		count;				/* if >= 0, then this many instances starting at base */
	int		base;
	int		filecvt;			/* if nonzero,make a file */
	char		flags;				/* FLIO_ flags */
	struct Io	*next;
} Io;

typedef struct Desc {
	long		progid;				/* programme id - keeps vars & such scoped to the prog in symtab */
	Type		type;
	char		*name;
	Value		*val;
	char		*desc;
	char		*after;
	char		*woomera;			/* woomera resource info for command */
	Range		*range;
	char		*nodes;
	Io		*inputs;
	Io		*outputs;
	char		*cmd;
	char		*rsrcs;				/* list of resources */
	Value		*attempts;
	Value 		*priority;
	Value		*keep;				/* number of seconds to keep programme round after it ends */
	Value 		*reqdinputs;			/* number of input files that must be present to run job; 0 == all */
	struct Desc	*next;
} Desc;

/* input/output file name list for a job */
typedef struct Flist {
	int	nalloc;			/* number allocated */
	int	idx;			/* current insertion point */
	char	**list;			/* pointer to the strings (md5,name,rep) */
} Flist;

/* job specific info */
/* no need for a name; jobs will be numbered with a 'basename' that matches the troup/desc name */
typedef struct Job {
	int	status;			/* unsubmitted, good, bad, running */
	char	node[25];		/* specific node requirement (iteration node) */
	char	*info;			/* info returned by senschal (kept only on failure) */
	char	*cmd;			/* actual command submitted to seneschal; we keep only incase of error */
	char	*qmsg;			/* message to send to seneschal to mark the job as ready to queue */
	Flist	*ifiles;		/* input files */
	Flist	*ofiles;		/* output files */
} Job;

/* manages a collection of jobs active in seneschal */
typedef struct Troupe {
	struct Troupe *next;
	struct Troupe **blocked;	/* list of other troups waiting on this one */
	int	nblocked;		/* size of the waiting list */
	int	bidx;			/* index into the waiting list for next */
	int	waiting_on;		/* number of troups this troup is waiting on */
	Job	*jobs;			/* job specific info that cannot be gleaned from this */
	Desc	*desc;			/* description information that was picked up during parse */
	int	njobs;			/* total in the troupe */
	int	cjobs;			/* number completed */
	int	ejobs;			/* number errored */
	int	state;			/* state - pending, running etc TS_ constants*/
	ng_timetype	start;		/* statistics */
	ng_timetype	end;
} Troupe;

/* programme information  */
typedef struct Programme {			/* programmes are accessible only via the symtab */
	char 	*name;				/* programme name - used to create seneschal job name */
	long	progid;				/* id used to scope variables to this programme */
	Troupe	*troupes;			/* troupe list */
	ng_timetype purge_time;			/* when to purge */
	ng_timetype subtime;			/* when we received the programme */
	char	*subnode;			/* node that submitted the programme */
	char	*subuser;			/* user that submitted the programme */
	ng_timetype keep_tsec;			/* number of tseconds to keep programme round after it ends */
} Programme;


/* ------------    iteration  functions ----------------- */
extern Iter *newiter(Type);

/* ------------    range functions ----------------- */
extern Range *newrange(char *);
extern char *dumprange(Range *, char *);
extern void purge_range( Range *r );
extern Rlist *newrlist(Range *);

/* ------------    value functions ----------------- */
extern Value *newvalue(Type, char *);
extern char *dumpvalue(Value *, char *);
extern void purge_value( Value *v );

/* ------------    I/O functions ----------------- */
extern Io *newio(char *, Value *, char *, char);
extern void dumpio(Io *i, char *pre );
extern void purge_io( Io *i );					/* purge the whole chain */
extern void purge_1io( Io *i );					/* purge one io block */
extern Io *expand_io( Programme *p, Io *ip, long progid );

/* ------------    troupe functions ----------------- */
extern Troupe *new_troupe( Programme *p, Desc *d );
extern void purge_troupe( Troupe *t, int expired );
extern void block_troupe( Troupe *t, char *name );		/* block the troupe until name (troup) has finished */
extern void release_troupe( Troupe *t );			/* release all troups waiting on this troup to complete */
extern void dump_troupe( Troupe *t );
extern void explain_troupe( Programme *p, Troupe *tp );

/* ------------    programme functions ----------------- */
extern int set_blocks( Programme *p );				/* set all blocks for the programme */
extern void explain_programme( Programme *p, int *how_much );
extern int new_progid( );					/* find an unused progid */

/* ------------- job creation/management -------------- */
extern void parse_status( char *buf, int mode );		/* parse a seneschal status message */
extern void purge_programme( Programme *p );
extern void run_progs( );					/* run all programmes for maintenance */
extern void order_files( long progid, Io *head, char type );	/* sets order of files in the output list */
extern void submit_gar( Programme *p, Troupe *tp );		/* send gar cmd information to seneschal */
extern void switch_jobstate( Troupe *t, int curstate, int newstate );	/* change all jobs in a troupe with curstate to newstate */

/* ------------ descriptor functions ----------------- */
extern Desc *new_assignment(Type, char *name, Value *val );
extern Desc *newdesc(Type, char *name, char *desc, char *after, Range *range, char *rsrcs, Value *priority, Value *attempts, char *nodes, Io *inputs, Io *outputs, char *cmd, char *woomera, Value *reqdinputs );
extern void purge_desc( Desc *d );				/* this descriptor */
extern void purge_desc_list( Desc *d );				/* purge d and all descriptors pointed to by d */
extern void dumpdesc(Desc *d );
extern int parse_desc( Programme *p );				/* parse the list of descriptors to gen troupes and vars */
extern char *extend(char *base, char *add, char seperator );
extern Desc *appenddesc(Desc *chain, Desc *delta);
extern Desc *add2mrad( Desc *dlist, void *data, Datatype dt );			/* add something to the most recently added descriptor */
extern Desc *empty_desc( Type, char *name );
extern void ammend_cmd( Programme *p, Desc *d );				/* fix command up with woomera stuff it its there */

/* ---------------- misc functions ----------------- */
extern Programme *parse_pgm( char *fname, char *data );		/* parse a programme (setup and yyparse) */
extern void showvar( Syment *se, void *data );			/* show the variable pointed to by the symtab pointer */
extern void *lookup( char *name, int space );			/* lookup something in the symbol table */
extern int ilookup( char *name, long progid, int defval );	/* lookup in symtab and convert to int */
extern void hide( );						/* turns us into a daemon */
extern char *bquote(Value *v, char *cmd);
extern void sbleat( int urgency, char *fmt, ... );		/* bleats to session partner -- user, and to log via ng_bleat using urgency */
extern void siesta( int tenths )		;                /* short rests -- sleep in tenths of seconds increments */


/* ---------------- user interface related -------------- */
void explain_all( Syment *se, void *data );			/* expains all programmes */
void uif_resubmit( char *msg );					/* processes a user's resubmit request */
void uif_purge( char *msg );					/* processes a user's purge programme request */
void uif_explain( char *msg );					/* processes a user's explain request */
void uif_dump( char *msg );					/* processes a user's dump request */

/* ----------------- ckpt functions ----------------------*/
void ckpt_write( char *fname );
int ckpt_read( char *fname );
int ckpt_recover( char *fname );

/* ------------------ globals ---------------------- */
extern	Symtab	*symtab;
extern	Desc 	*descs;						/* list of descriptors generated during yyparse */
extern	int	have_seneschal;					/* 1 if a session with seneschal exists */
extern	char	*prog_name;					/* name supplied in programme */
extern	int	keep_time;					/* user specified a keep time on the programme line */
extern	Sfio_t	*bleatf;					/* where we want to bleat verbose things to */
