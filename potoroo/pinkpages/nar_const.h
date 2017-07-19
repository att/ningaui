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


/* constants for narbalek 
*/

			/* value and comment spaces are deprecated now that lumps are used to manage both */
#define VALUE_SPACE		1	/* sym tab name spaces */
#define COMMENT_SPACE		2	/* comments/values saved with same name */
#define LUMP_SPACE		3	/* new space for lumps which replace individual comments and values */

#define PEER_SPACE		1	/* for the peer hash table */

#define	DEF_END_MARK	"#end#\n"	/* sent as last buffer of data to mark its end -- lets ng_sendtcp know when to stop */
#define DEF_END_VAR	"#narbalek: end of table\n"	/* lets caller know that the last of the table was received */
#define ERROR_RESULT	"#ERROR#\n"	/* sent when we cannot look up a value because the table is not loaded */

#define T_COMMENT	1		/* types expected by set val */
#define T_VAL		2

#define NO_EXPANSION	0		/* return value without expansion (Get) */
#define DO_EXPANSION	1		/* return value expanding variables along the way */

#define COND_SEP_CH	';'		/* separates condition from name/value in an ifset statemen: name;cond;=value */

#define MIN_ADOPT_LEASE	10		/* tenths of seconds we wait for lease responses -- short lease for rejoining the tree */
#define LONG_ADOPT_LEASE	50	/* tenths of seconds we wait for lease responses -- long lease for initial insertion and after no parents found */
#define MIN_LOAD_LEASE	50		/* time we wait for an end of load message -- after this we assume loaded */
#define DLIST_LEASE_TSEC 3000		/* we should refresh nnsd node list if it is more than 5 minutes old */
#define PEER_LEASE_TSEC	4000		/* peer blocks are stale after 400s */
#define PRUNE_AFTER_SEC 3601		/* prune inactive lumps after they sit idle for an hour -- lump time is unix time so these are seconds */

#define LOG_IT		1		/* indicator to log an event */
#define DONT_LOG	0		/* option not to log the event */

#define	FL_ROOT		0x01		/* we are the root of all evil */
#define FL_ADOPTED	0x02		/* we have a parent */
#define FL_DELAY_AR	0x04		/* delay the adoption request */
#define FL_ROOT_KNOWN	0x08		/* we've noticed another node has declared that they are root */
#define FL_LOADED	0x10		/* we got an initialising load */
#define FL_DELAYED_LOAD 0x20		/* we need to ask for a reload, but after a random delay */
#define FL_NO_ROOT	0x40		/* we are not allowed to become the root */
#define FL_NO_MCAST	0x80		/* we are isolated and dont have multi cast -- use parrot to agent us into the group */
#define FL_NO_CHILD	0x100		/* do not accept adoption requests (testing so we don't propigate) */
#define FL_LOAD_PENDING 0x200		/* we are waiting to be loaded */
#define FL_HAVE_LIST	0x400		/* we have a peer list, though it is legit to be null */
#define FL_USE_NNSD	0x800		/* use the nnsd process for lookup rather than multicast */
#define FL_HAVE_AGENT	0x1000		/* we have an agent */
#define FL_ALLOW_MERGE	0x2000		/* allow a merge (send our data) if we were root, and we detect a collision and must cede */
#define FL_QUERY_PENDING 0x4000		/* we are waiting for query responses */
#define FL_OK2LOG	0x8000		/* ok to log -- only after logd has been started */

#define LF_ACTIVE	0x01		/* lump is alive -- if not set, then value was deleted at the indicated ts */

					/* session types */
#define ST_CHILD	1		/* node we adopted */
#define ST_PARENT	2		/* we were adopted by this node */
#define ST_PROP2	2		/* propigate to nodes of this type or less */
#define ST_ALIEN	3		/* some process that attach to get/put info -- dont propigate info to these */
#define ST_AGENT	4		/* our multicast agent */

#define ROOT_BCAST_FREQ	600		/* tenths of seconds -- bcast our leadershipness if we think we are the poobah*/
#define ALIVE_BCAST_FREQ 600		/* tenths of seconds -- send an alive frequently */
#define CKPT_FREQ	3000		/* tenths of seconds -- checkpoint frequency */
#define PRUNE_FREQ	1800		/* frequency of prune attempts */
#define REQ_PEER_FREQ	3600		/* request a peer list from nnsd every 6 min or so */

#define TG_IAMROOT	0		/* telegraph types -- announce that we are root */
#define TG_ADOPT	1		/* send adoption request */
#define TG_BUF		2		/* send a user formatted buffer -- telegraph adds udpid and \n */
#define TG_ALIVE	3		/* send alive message to all nnsd processes */
#define TG_QUERY	4		/* send query stats */



/* --------------------------------------------------------------------------------------------------------*/

typedef struct lump {			/* describes a bit of info that we are managing */
	char	*name;			/* lookup (hash) name */
	char	*comment;		/* fluffy commentary if user gave it */
	char	*data;			/* the meat */
	int	flags;			/* LF_ constants */
	time_t	ts;			/* time that the information was updated */
} Lump;

struct session {			/* used to mannage some kind of TCP session */
	struct session *next;
	struct session *prev;
	void	*flow;			/* pointer at flow management stuff for inbound buffer processing */
	int	sid;			/* si session id for writes */
	int	type;			/* we need to know which session is our next space buddy */
};

struct node_info {			/* keeps node info -- mostly to queue responses to our adopt request */
	struct	node_info *next;		
	char	*addr;			/* their ip address and port */
	int	caste;			/* their cast level */
	int	kids;			/* number of nodes they have as children already */
};

struct cached_msg {			/* message that we must forward to parent once we are adopted */
	struct cached_msg *next;
	char 	*msg;
};

