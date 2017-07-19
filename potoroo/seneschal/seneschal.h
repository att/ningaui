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
  ------------------------------------------------------------------------
	hash table used to access part and node blocks

	each part block has an array of part2node blocks that point to the node
	blocks for nodes that might have the partition.

	when a node sends a list of partitions, the pointers and revision 
	number for that node in each part2node block in the list is updated.

	when it is necessary to map a partition to a node, its nodes list 
	is consulted. If the revision number in the part2node block matches
	that contained in the node block then the last report from the node 
	contained the partition and the job can be assigned to that node. if
	the revison numbers do not match, the reference is stale, and the 
	pointer is nulled. This method of stale checking is used to avoid
	having to clear the node reference in every partition each with 
	each partition list that is received. 


  ------------------------------------------------------------------------
*/
#define REPMGR_DUMPF	"repmgr.dump"	/* file where we will ask for a dump (in the pwd) */

#define MAX_NODES	150		/* maximum nodes we support */
#define MAX_COST_DATA	100		/* number of jobs tracked to figure job type costing */
#define MAX_UDP_POLL	2		/* max poll time if no data - seconds */
#define MAX_PERGAR	10		/* max files per node per gar type */
#define MAX_PRI_CREEP	90		/* we let priorities creep only this far */

					/* work queue names */
#define PRIORITY_Q	0		/* priority - user commands and deletes */
#define STATUS_Q	1		/* status work from completing jobs -- priority too, but interleave with priority */
#define NORMAL_Q	2		/* all other work */

				/* these should be high, but less than 127 */
#define MAX_ATT_ERROR	126		/* max attempts status back to nawab */
#define LEASE_ERROR	125		/* lease exipired on job */

#define EXEC_COST	0		/* cost types to fetch on a job -- time to execute */
#define WAIT_COST	1		/* time spent waiting for execution on a node */

					/* remember times are in 1/10ths of seconds */
#define SUM_DELAY	600		/* write summary every 60 seconds */
#define FILE_DELAY	600		/* request dump1 info for files from repmgr every 60 seconds */
#define GAR_DELAY	18000		/* 30 minutes between attempt to gar the same job or file */
#define CKPT_FREQ	3000		/* how often we checkpoint node info */

#define	NO_QUEUE	0		/* add is not allowed to queue the new job */
#define QUEUE		1		/* job may be queued */

#define	NODE_SPACE	1		/* symbol table name spaces */
#define FILE_SPACE	2
#define JOB_SPACE	3
#define RSRC_SPACE	4
#define	TYPE_SPACE	5
#define GAR_SPACE	6
#define NATTR_SPACE	7		/* has its own table, but lets make it in the list here anyway */
#define MPN_SPACE	8		/* max per node mappings -- current running on resource:node combination */

#define CREATE		1		
#define NO_CREATE	0

#define NOT_QUEUED	-1		
#define	PENDING		0		/* job states (indicate what queue they are on) */
#define RESORT		1		/* job needs to be re examined and possibly moved to pending */
#define MISSING		2		/* jobs where not all files present/not a single node with all files */
#define	RUNNING		3
#define COMPLETE	4		/* job completed successfully - status reported to nawab */
#define FAILED		5		/* job moved to complete queue because max attemps exceeded */
#define DETERGE		6		/* job successful, but waiting for final housekeeping job to run */
#define RUNNING_ADJ	7		/* job is running, must be dequeued w/o dropping files/resources */

					/* remember, ningaui times are in 1/10s of seconds! */
#define DEL_LEASE	72000		/* time afterwhich jobs are removed from completed queue (2 hrs) */
#define FINFO_LEASE	9000		/* file information lease (15 min) */
#define PRI_BUMP_BASE	18000		/* tenths of sec after which job priority is bumped (30min) */

#define	BID		1 		/* work block types */
#define STATUS		2
#define DUMP		3
#define	DELETE		4		/* purge a job (purge command received) */
#define	SUSPEND		5
#define	LIMIT		6		/* set a resource limit */
#define EXPLAIN		7
#define PUSH		8		/* make the job run now, with limits */
#define RUN		9		/* make job runable (remove limits) but leave priority */
#define PAUSE		10		/* toggle the suspension state  - for single button in zookeeper*/
#define	JOB		11		/* job submission */
#define LOAD		12		/* a nodes desired work load */
#define GAR		13		/* gar command that might help us make some files */
#define	EXTEND		14		/* extend a job's lease */
#define	LISTFILES	15		/* dump a list of needed files to a tcp session */
#define	NODECOSTS	16		/* dump the jobtype costs per node to tcp session */
#define	IJOB		17		/* job specification, but its missing input or output info */
#define CJOB		18		/* sender has sent all info for job, it may be queued */
#define INPUT		19		/* add input to a job */
#define INPUTIG		20		/* add input with file size ignore flag to the job */
#define	OUTPUT		21		/* add output to a job */
#define RELEASE		22		/* release something, expect buffer with type:name */
#define OBSTRUCT	23		/* prevent a job from running even if bids/files/resources are ok */
#define NATTR		24		/* receive a node's attributes */
#define NODE		25		/* hard assign a job to a node */
#define MPN		26		/* max per node limit for a resource */
#define RMBUF		27		/* buffer from repmgr needs to be parsed */
#define USER		28		/* buffer contains user data for the named job */

#define	DUMP_RSRC	1		/* dump constants 10/100s places: nnx */
#define DUMP_TYPES	2
#define	DUMP_NODES	3
#define	DUMP_GAR	4

#define AUDIT_ID	"SENESCHAL_AUDIT"	/* added as a label to message */
#define AUDIT_NEWJOB	"newjob"	/* audit message strings for log */
#define AUDIT_QUEUE	"queue"		/* queue changed */
#define AUDIT_SCHED	"scheduled"	/* job scheduled */
#define AUDIT_STATUS	"status"	/* job status received EJ started */
#define AUDIT_COMPLETE  "complete"	/* job completed */
#define AUDIT_DICTUM	"usermsg"	/* user comment */

#define BATCH_SIZE	100		/* number of jobs to batch up before submitting to woomera */
#define	MAIN_SLEEP	2		/* num seconds main loop sleeps between go rounds */

#define FSIZE_INVALID	-1		/* file size has not been determined */
#define FSIZE_IGNORE	-2		/* file size for the file is not used to compute job cost */

#define GF_SEP_ALARM	0x01		/* general/global flags - seperator alarm logged for this input file */
#define GF_NEWJOBS	0x02		/* new jobs were added */
#define GF_ALLOW_GAR	0x04		/* gar checking allowed - new repmgr dump parsed */
#define GF_MUSTBSTABLE	0x08		/* needed input files must be stable to start a job */
#define GF_NEED_NATTR	0x10		/* must have a node's attributes to consider it for attribute test */
#define GF_WORK		0x20		/* work queue might not be exhaused -- cause a quick check of tcp states */
#define GF_QSIZE	0x40		/* queue jobs based on priority and then size; priority only if off */
#define GF_DEFAULTS	GF_MUSTBSTABLE|GF_QSIZE

#define JF_ANYNODE	0x01		/* job flags */
#define	JF_HOUSEKEEPER	0x02		/* housekeeping stage of the job */
#define JF_GAR		0x04		/* gar job */
#define	JF_OBSTRUCT	0x08		/* job is blocked -- cannot run */
#define JF_MUSTBESTABLE 0x10		/* job requires stable files regardless of global setting */
#define JF_VARFILES	0x20		/* job needs only to have one input file in order to be released */
#define JF_IPENDING	0x40		/* investigation into actual job state started for the job */
#define JF_IGNORESZ	0x80		/* we are allowed to ignore file sizes for job size computation */

					/* job cannot be scheduled reason flags */
#define JNS_STABLE	0x01		/* one or more files unstable */
#define JNS_RESOURCES	0x02		/* no resources for job */
#define JNS_FILES	0x04		/* all input not on same node */
#define JNS_ATTRS	0x08		/* node with correct attributes not avail */
#define JNS_BIDS	0x10		/* no node with bids avail */

#define NF_SUSPEND	0x01		/* node flags - suspend jobs to this node */
#define NF_GAR_OK	0x02		/* still room on node for a gar file */
#define NF_RELEASED	0x04		/* user has released the node -- excluded from dumps and chkpoints, but we must keep struct */

#define RF_HARD		0x01		/* resource is a hard resource (job count) */
#define RF_IGNORE	0x02		/* ignore resource if unreferenced (dumps etc) */

#define FF_TRACE	0x01		/* trace changes to this file */
#define FF_D1REQ	0x02		/* we sent a dump1 for this file */
#define FF_IGNORESZ	0x04		/* ignore size when computing job size */

#define TF_WRAPPED	0x01		/* job type flags - cost info has wrapped */

					/* work block flags */
#define	WF_TCP		0x01		/* workblock was created by a tcp requrest (dont trash session) */

					/* location of tokens in the job record */
#define NAME_TOK	0
#define ATTEMPT_TOK	1
#define SIZE_TOK	2
#define PRIORITY_TOK	3
#define RESOURCE_TOK	4
#define NODE_TOK	5
#define NINF_NEED_TOK	6		/* number of input files that are needed to release job (0==all files) */
#define INFILE_TOK	7		/* inf:outf:cmd   should always be last as it is easiest for nawab to build this way*/
#define OUTFILE_TOK	8
#define CMD_TOK		9
#define	REQ_JOB_TOKENS	9		/* number of tokens that we need to see to consider it valid */

#define START_TIME_CMD	"export START_TS=`ng_dt -i`"		/* command that we use to flag the time woomera started a job */
/* -----------------------------------------------------------------------------------------*/


typedef struct	Work_t {		/* work block - queued by port listener thread  */
	struct	Work_t *next;		/* single linked list as these are removed only from head */
	int	type;			/* type of work represented */
	char	*string;		/* stuff */
	void	*misc;			/* pointer at misc data that might be needed */
	int	value;			/* value */
	char	*to;			/* address to send result to (udp) or session name (tcp) for job status */
	int	fd;			/* udp fd to send over (-1 if 'to' contians tcp session name) */
	int	flags;			/* WF_ constants */
	char	*dyn_buf;		/* dynamically allocated buffer if data is larger than static buffer */
	char	static_buf[2048];
} Work_t;

typedef	struct	Resource_t {		/* resource information */
	struct	Resource_t *next;	/* master list chain */
	char	*name;
	int	limit;			/* value used to limit the number of jobs run under this resource (-1 unlimited) */
	int	mpn;			/* max per node (if > 0) */
	int	active;			/* jobs that are currently running with this */
	int	flags;			/* RF_ constants */
	long	ref_count;		/* number of jobs pointing at this one */
	double	target;			/* target percentage based on limit sum of all resources */
} Resource_t;

typedef struct	Node_t {
	struct	Node_t *next;		/* these are chained on the master node list */
	struct	Node_t *prev;
	char	*name;
	char	*nattr;			/* node attributes as sent by the node */
	int	bid;			/* outstanding bid count (jobs wanted that we have not assigned) */
	int	desired;		/* work level we should try to keep the node at (automatic setting of bid by seneschal if >0) */
	struct	Job_t	*jobs[BATCH_SIZE];	/* batch of jobs to submit to woomera for the node */
	int	jidx;			/* at next slot for job */
	int	flags;			/* see NF_ constants */
	int	suspended;		/* if zero then we send jobs, else we dont */
	int	rcount;			/* running job count  -- what WE think is running */
	ng_timetype	lease;		/* lease for auto suspensions etc */
} Node_t;

typedef struct	File_t {		/* information we need to keep on a file */
	struct	File_t *next;		/* queue chains */
	struct	File_t *prev;
	char	*name;
	int	flags;			/* FF_ flags */
	unsigned char stable;		/* stablity according to repmgr */
	int	nnodes;			/* number allocated in nodes (and paths) */
	int	nidx;			/* index for insertion into nodes and paths */
	Node_t	**nodes;		/* pointers to nodes that have this file */
	char	**paths;		/* path that this file has on each node; nnodes is also count for this */
	ng_timetype gar_lease;		/* we dont try to re-gar the file until after this time has passed */
	int	ref_count;		/* number of jobs that reference this file */
	int	nref;			/* number of pointers allocated in ref_list */
	struct Job_t	**reflist;		/* jobs that reference this file (makes recalc of job size easier) */
	char	*md5;			/* md5 chksum for the file */
	char	*gar;			/* pointer to name (symtab GAR_SPACE) that maps to command used to gar the file */
	ng_int64	size;		/* we use the filesize to calculate job size and ultimately job cost */
} File_t;

typedef struct	Jobtype_t {		/* learned info about a job type */
	struct	Jobtype_t *next;
	char	*name;
	int	nextc;			/* insertion point into cost */
	int	flags;			/* TF_ constants */
	double	cost[MAX_COST_DATA]; 	/* cost for the last n jobs of this type (cost is tenths-of-seconds/size_units) */
	double	total;			/* sum of the values we have */
	double	mean;			/* the mean of the observed values */
} Jobtype_t;

typedef struct	Job_t {
	struct	Job_t *prev;		/* chains */
	struct	Job_t *next;
	struct	Job_t *prime;		/* primary job (expounge jobs have pointer to the main job) */
	char	*name;			/* assumed to be:  <jobtype>_<stuff> */
	char	*wname;			/* name given to the woomera job started for this job */
	Node_t	*node;			/* node that the job was, or is to be, assigned to */
	Node_t	*notnode;		/* node that this job cannot be assigned to */
	char	*nattr;			/* attributes required of the node that runs the job */
	ng_timetype mod;		/* last mod time. pending jobs are purged if older than the last read time */
	ng_timetype pri_bump;		/* time at which point we bump the priority of the job */
	ng_timetype started;		/* timestamp when the job was submitted */
	ng_timetype lease;		/* time at which point we assume failure */
	ng_timetype gar_lease;		/* we dont try to gar files until after this time has passed */
	ng_timetype run_delay;		/* set to timestamp when job is allowed to run again */
	int	attempt;		/* number given to next job assigned */
	int	delay;			/* num sec to delay after a failure */
	int	state;			/* current state of this job (also indicates queue for dqing) */
	long	size;			/* size of job computed from input file sizes of files that are ! marked ignore */
	long	isize;			/* amount of input file size that was ignored (not used in computing the job cost) */
	long	cost;			/* estimated cost of the job based on size and history tracked by job type blocks */
	int	priority;	
	int	max_pri;		/* highest we will let this job go when bumpped up */
	int	max_att;		/* max number of attempts that are allowed before reporting "hard" failure */
	int	flags;			/* see JF_ constants */
	int	nrsrc;			/* number of resource pointers allocated in *rsrc */
	Resource_t	**rsrc;		/* pointers to the resources used by this job */
	int	ninf;			/* number of input file slots allocated */
	int	iidx;			/* index into in files */
	int	ninf_need;		/* number of input files needed if JF_VARFILES is set */
	File_t	**inf;			/* pointers to input files for job */
	char	**flist;		/* pointers to paths in File_t block (filled in after node selected to run job) */
	int	exit;			/* exit code returned by woomera */
	int	hexit;			/* exit code from the housekeeper */
	char	*ocmd;			/* submitted command string -- potentially with %o and %i things */
	char	*cmd;			/* command string to submit with filenames expanded */
	int	noutf;			/* number of output file pointers */
	int	oidx;			/* index into outfiles */
	char	**outf;			/* output file pointers (pointers to output file names) */
	char	**olist;		/* output file pointers to filenames w/ attempt number added */
	char	**rep;			/* replication param for s_mvfile processing */
	char	*sbuf;			/* status buffer to write for nawab */
	char	*username;		/* the user name that the job should run under */
	char	response[50];		/* session name to send status to */
} Job_t;

typedef struct	Gar_t {			/* manages information used to set up jobs to gar input files */
	struct Gar_t	*next;
	int	pending;		/* number of fstirngs queued for nodes here */
	int	idx;			/* index into fstrings/nodes - next slot to use */
	char	*fstrings[MAX_NODES];	/* buffer containing file name list passed on the next gar command */
	Node_t	*nodes[MAX_NODES];	/* node to which the corresponding fstring applies */
	int	nfiles[MAX_NODES];	/* number of files in fstrings */
	char	*cmd;			/* the static command string */
	char	name[128];		/* the gar name passed in from nawab */
} Gar_t;


/* --------- global prototypes -----------------------------------------------------*/

void *port_listener( void *ptr );		/* tread entry point */

Work_t *get_work( );
long any_work( );
void put_work( Work_t *b );
void queue_work( int qname, Work_t *b );
long work_qsize( int qname );			/* return the number of things queued on the named work queue */

void *stats_writer( void *data );
void exhale( );
void put_stats( char *buf );
int stats_init( char *fn );

void q_job( Job_t *j, int qname );
char *dq_job( Job_t *j );
char *replace( char *src, char *target, char *value );
char *vreplace( char *src, char *target, char **things, int nthings );
char *freplace(char *nodename, char *cmd, char **inf, int ninf, char **outf, int noutf);

int explain_job( int fd, char *name, Job_t *j, ng_timetype now);
int explain_resource( int fd, char *what );
int explain_file( int fd, char *what );
void explain_all( Work_t *w );			/* returns 1 (EX_MORE) if there is more to do and work has been requeued */
void exd_adjust( Job_t *j );			/* adjust pending explain data pointers that reference job j */

void file_trace( File_t *f, char *action );	/* bleat file info if its trace bit is set */
int file_trace_toggle( char *name );		/* find and toggle the trace bit */
/* --------------------------------------------------------------------------*/
