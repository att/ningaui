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

typedef struct Ballot_t {
	struct Ballot_t *next;
	char	*nname;		/* node name */
	long	lag;		/* time between election start and vote timestamp */
	ng_timetype ts;		/* time the ballot was cast */
	ng_timetype expiry;	/* general expriy timer -- marks when ignore state is up */
	int	score;		/* the node's vote */
	int	flags;		/* BF_ constants */
} Ballot_t;

typedef struct Srv_info_t  {
        struct Srv_info_t *next;      /* we chain these */
        char	*name;                 /* service name */
        char	*start_cmd;            /* called to start the service on the node */
        char	*stop_cmd;             /* called if we believe we had the service and lost it */
        char	*score_cmd;            /* called to calculate the nodes score for the service */
	char	*query_cmd;		/* command used to check up on the service */
        char	*continue_cmd;         /* driven when we win the election and believe the service to be running*/

        ng_timetype lease; 		/* time after which we do something for the service -- what depends on state */
        ng_timetype lease_time;		/* seconds between elections (status checks) */
        ng_timetype election_start;	/* time election started so we dont count old votes */
	int	state;			/* ST_ constants; green, red, yellow */
	char	*qdata;			/* data returned after green/yellow/red:  node name or other query data */

        int	election_len  ;         /* time the election is held open for */
        int	score;               	/* our computed score for the service  (init to >101 to force calc) */
	Ballot_t  *ballots;		/* ballots we have received */
        int	active;  		/* service is active (in the pp srvm_list var) */
        char	*sscope;               	/* scope for this service as supplied by score calculator */
        char	*dscope;               	/* default scope for the service (from narbalek def, or command line) */
        int	nelect  ;               /* number of nodes to elect for the service */
} Srv_info_t ; 


#define	BF_ELECTED	0x01		/* this ballot was selected as a winner */
#define BF_IGNORE	0x02		/* this ballot is ignored -- probably a host that has had issues keeping a service up */

#define IGNORE_TIME	9000		/* 15 minute (tenths of sec) time that we will ignore ballots for a service from the last winner */
