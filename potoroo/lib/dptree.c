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
---------------------------------------------------------------------------------
Mnemonic:	dptree - distributed process communication tree
Abstract:	Allows a process to join a tree of processes which are 'linked' via
		TCP/IP sessions in a binary tree fashion. These routines manage all
		of the socket communicaitons and establish a listen port for outsiders
		to communicate with the process.  All TCP/IP messages received that 
		begin with an equal sign (=) are assumed to be 'tree management' data
		and are processed by routines here. All other buffers are passed to 
		the 'dpt_user_data' funciton for processing. 

Date:		25 August 2004
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/

#include	<stdio.h>
#include 	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<signal.h>
#include	<regex.h>
#include 	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_lib.h"
#include	"ng_ext.h"
#include 	"../socket/ng_socket.h"

#define MIN_ADOPT_LEASE	50		/* tenths of seconds we wait for lease responses */
#define DLIST_LEASE_TSEC 3000		/* we should refresh nnsd node list if it is more than 5 minutes old */


#define ST_CHILD	1		/* node we adopted */
#define ST_PARENT	2		/* we were adopted by this node */
#define ST_PROP2	2		/* propigate to nodes of this type or less */
#define ST_ALIEN	3		/* some process that attach to get/put info -- dont propigate info to these */
#define ST_AGENT	4		/* our multicast agent */

#define ROOT_BCAST_FREQ	600		/* tenths of seconds -- bcast our leadershipness if we think we are the poobah*/
#define ALIVE_BCAST_FREQ 600		/* tenths of seconds -- send an alive frequently */

#define TG_IAMROOT	0		/* telegraph types -- announce that we are root */
#define TG_ADOPT	1		/* send adoption request */
#define TG_BUF		2		/* send a user formatted buffer -- telegraph adds udpid and \n */
#define TG_ALIVE	3		/* send alive message to all nnsd processes */
#define TG_QUERY	4		/* send a querry message -- looking for a better parent */



/* ----------------------------------- structures ------------------------------- */
struct session {
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

struct cached_msg {
	struct cached_msg *next;
	char 	*msg;
};

/* ----------------------------------- prototypes ------------------------------- */
static int match( char *str, char *pat );
static void send_udp( char *addr, char *buf, int len );
static	void mk_dlist( );
static	int can_be_root( );
static	void send2nnsd( char *buf );
static	void send2peers( char *buf );
static	void request_list( );
static	void save_list( char *buf );
static void send_udp( char *addr, char *buf, int len );
static void chop_port( char *buf );
static char *get_ip_str( char *name );
static int get_rand( int min, int max );
static char *cvt_addr( char *old );
static void telegraph( int what, char *ubuf );
static void cache_msg( char *msg );
static void send_cached( int sid );
static void send_state( int sid, char *addr );
static int get_parent_sid( );
static struct session *get_parent_sb( );
static struct session *add_session( int sid, int type );
static void drop_session( struct session *sb );
static void drop_parent( );
static void drop_kids( );
static void close_sessions( );
static int contact_agent( char *address, int port, char *group );
static void maint( );
static void drop_nilist( struct node_info *l );
static int attach2parent( );
static void insert( );
static void do_tree_cmd( struct session *sb, char *from, char *buf );
static int cbsig( char *data, int sig );
static int cbconn( char *data, int f, char *buf );
static int cbdisc( char *data, int f );
static int cbudp( void *data, char *buf, int len, char *from, int fd );
static int cbtcp( void *data, int fd, char *buf, int len );
static void usage( );


/* ----------------- globals (a few too many for my taste, but not so much to care about) ------------------ */
extern int errno;

static int	flags = 0;			/* DPT_FL_ constants */
static int	ok2run = 1;			/* turned off when it is no longer safe to execute */
static int	max_kids = 2;			/* max children we will allow -- variable so we can change on fly */
static int	children = 0;			/* number of children sessions we have */
static int	caste = 9999;			/* our position in the tree; once adopted it is parents caste +1 */
static int	mcast_ttl = 2;			/* time to live for mcast messages */
static int	max_depth = 1000;		/* max depth before we think something is off */
static int	better_parents = 0;		/* number of peers who might make better a better parent -- tree balancing */
static int	query_responses = 0;		/* number of responses we got to the better parent query */
static char	tree_cmd_flag = '=';		/* all tree command messages must start with this -- user programme can change if it sends msgs with lead = */
static char	*udp_id = NULL;			/* udp message id prefix -- uniquely identifies us to allow loopback bcast messages to be ignored */
static char	*cluster = NULL;		/* the name of the cluster -- info map 'type' is controlled by the port */
static char	*league = "indi";		/* the group of us that are bonded in the tree (could be multiple clusters) */
static char	*mcast_group = "235.0.0.7";	/* all clusters can use the same group but we allow it to be supplied on the cmd line */
static char	*mcast_ip = NULL;		/* full ip (group:port) for udp message sends */
static char	*parent_id = NULL;		/* the name of our parent */
static char	*my_ip = NULL;			/* ip address -- we dont care which interface */
static char	*my_name = NULL;		/* my hostname */
static char	*port_str = NULL;		/* the port we have a tcp listen on, and we expect all other peers to listen on too */
static char	*nnsd_port_str = NULL;		/* port that nnsd listens on -- if not supplied via -d we use mcast */
static ng_timetype nxt_shuffle = 0;		/* next time we will try to reposition ourselves up in the tree */

static char	*version = "dptree: v2.1/05049"; 

static struct node_info *avail_list = NULL;	/* list of nodes that can possibly take us */
static struct session *session_list = NULL;	/* all of the nodes we are in session with */
static struct session *agent_sb = NULL;		/* quick reference to an agent */
static struct cached_msg *mcache_h = NULL;	/* head/tail: messages cached to send to parent when we are adopted */
static struct cached_msg *mcache_t = NULL;	



/* ------------- stuff that directly supports nnsd interface -------------------------------- */

typedef struct peer {		/* peer info from the nnsd list */
	struct peer *next;
	char	*addr;
} Peer_t;

typedef struct nnsd {
	struct 	nnsd *next;
	char	*name;		/* node name where the nnsd lives */
	char	*addr;		/* address, well name:port */
} Nnsd_t;

static	char 	*nlist = NULL;			/* list of peers from nnsd */
static	ng_timetype nlist_lease = 0;
static	Nnsd_t	*dlist = NULL;			/* list of nnsd structures from NG_NNSD_NODES */
static	Peer_t	*peer_list = NULL;

/* create a list of nnsd structures -- one for each listed in the list string
	vlist string is assumed to be from the NG_NNSD_LIST variable and is expected 
	to have the syntax: <nodename> [<nodename>...]
*/
static	void mk_dlist( )
{
	static	ng_timetype expires = 0;	/* we will update the list every few minutes */

	char 	*p;				/* pointer into list */
	char	*nextp = NULL;			/* for strtok_r call */
	char	*list;
	char	buf[1024];
	ng_timetype ts;				/* current time stamp */
	Nnsd_t	*np;
	Nnsd_t	*next;

	ts = ng_now( );
	if( (expires > ts) && dlist )		/* not time to refresh the list */
	{
		return;
	}

	expires = ts + DLIST_LEASE_TSEC;			

	if( (list = ng_env( "NG_NNSD_NODES" )) == NULL )	/* no list -- that is not a good thing and we cannot play without one */
	{							
		ng_cbleat( CBLEAT_ALL, 0, "abort: no NG_NNSD_NODES list in environment/pinkpages" );
		exit( 1 );
	}

	for( np = dlist; np; np = next )		/* free the old list */
	{
		next = np->next;
		ng_free( np->name );
		ng_free( np->addr );
		ng_free(  np );
	}

	dlist = NULL;

	ng_cbleat( CBLEAT_LIB, 2, "mk_dlist: making new daemon list" );

	p = strtok_r( list, " ", &nextp );
	while( p )
	{
		np = (Nnsd_t *) ng_malloc( sizeof( *np ), "nnsd_t block" );
		np->name = ng_strdup( p );
		sfsprintf( buf, sizeof( buf ), "%s:%s", p, nnsd_port_str );
		np->addr = ng_strdup( buf );				/* address, or really name:port */
		np->next = dlist;
		dlist = np;
		ng_cbleat( CBLEAT_LIB, 3, "alloctated nnsd block for %s", np->addr );
	
		p = strtok_r( NULL, " ", &nextp );
	}

	ng_free( list );
}


/*
	return true if we can be the root of the communications tree
	we can be root if
		DPT_FL_NO_ROOT is not set in flags 
		and
		DPT_FL_ROOT_KNOWN is not set in flags
		and
		if in nnsd mode, we have received an empty list
*/
static	int can_be_root( )
{
	if( flags & DPT_FL_NO_ROOT )			/* regardless, we are not allowed to be root */
		return 0;

	if( flags & DPT_FL_USE_NNSD )
	{
		if( flags & DPT_FL_HAVE_LIST && peer_list == NULL )		/* we got a list that was empty so we can be it */
			return 1;
	}
	else
	{
		if( ! (flags &  DPT_FL_ROOT_KNOWN) )			/* in a mcast environment, if we have not heard a root announce */
			return 1;					/* then it is ok */
	} 

	return 0;
}

/*
	send to all nnsd processes that we know about
*/
static	void send2nnsd( char *buf )	
{
	Nnsd_t *np;
	int	len; 

	mk_dlist( );				/* ensure we have a current list of nnsd nodes */
	len = strlen( buf );

	for( np = dlist; np; np = np->next )
		send_udp( np->addr, buf, len );
}

/*
	send to all known peers (broadcast)
*/
static	void send2peers( char *buf )
{
	Peer_t	*p;
	int	len;

	len = strlen( buf );
	
	for( p = peer_list; p; p = p->next )
		send_udp( p->addr, buf, len );
}


/*
	request a peer list from nnsd
*/
static	void request_list( )
{
	Nnsd_t *np;
	char	buf[1024];
	int	len; 

	flags &= ~DPT_FL_HAVE_LIST;				/* turn off the list */

	sfsprintf( buf, sizeof( buf ), "0:list:0:List:!%s\n", league );	/* 0:list is the command to nnsd, 0:List: is the prefix it should send back to us */
	ng_cbleat( CBLEAT_LIB, 2, "requesting peer list %s", buf );

	send2nnsd( buf );
}


/*
	save a list of peers from nnsd
	we get the tokenised list array.  the list comes to us from nnsd as:
	List:node:addr:node:addr...  We are interested in nodes, which are odd numbers
*/
static	void save_list( char *buf )
{

	Peer_t	 *p;
	Peer_t	*np;			/* pointer at next for delete */
	char	*tp;			/* pointer into token */
	char	*tok = NULL;		/* next token in buffer */
	char	*nextt;			/* for strtok_r */
	char	wbuf[1024];

	for( p = peer_list; p; p = np )
	{
		np = p->next;
		ng_free( p->addr );
		ng_free( p );
	}

	peer_list = NULL;

	flags |= DPT_FL_HAVE_LIST;					/* we have received a list; even if it results in nothing */
	
	if( buf )
	{
		ng_cbleat( CBLEAT_LIB, 2, "saving peer list: %s", buf );
		tok = strtok_r( buf, " ", &nextt );
	}
	else
		ng_cbleat( CBLEAT_LIB, 2, "peer list was empty" );

	while( tok )
	{
		if( (tp = strchr( tok, '.' )) )
			*tp = 0;				/* name.address -- we lop off the address */

		
		if( strcmp( tok, my_name ) != 0 )		/* dont put us into the list */
		{
			sfsprintf( wbuf, sizeof( wbuf ), "%s:%s", tok, port_str );	/* the name and port we expect peers to be listening on */
			p = (Peer_t *) ng_malloc( sizeof( *p ), "peer_t block" );
			p->addr = ng_strdup( wbuf );
			p->next = peer_list;
			peer_list = p;
			ng_cbleat( CBLEAT_LIB, 3, "alloctated peer block for %s", p->addr );
		}

		tok = strtok_r( NULL, " ", &nextt );
	}
}

/* ---------------------------- utility functions -----------------------------------------*/

/*
	send a udp message. if we are using an agent, even in nnsd mode, then we send udp traffic to the 
	agent to forward to the true destination.  this allows us through the firewall without having to 
	open a boatload of ports
*/
static void send_udp( char *addr, char *buf, int len )
{
	char	mbuf[NG_BUFFER];
	
	if( flags & DPT_FL_HAVE_AGENT )		/* we have an agent, it must forward udp messages */
	{
		if( agent_sb )
		{
			sfsprintf( mbuf, sizeof( mbuf ), "~FORWARD %s %s", addr, buf );	
			ng_sisendt( agent_sb->sid, mbuf, strlen( mbuf ) );
		}
	}
	else
		ng_sisendu( addr, buf, strlen( buf ) );
}

/*	chop port from the string 
	assumes xx.xx.xx.xx:port   or xx.xx.xx.xx.port
*/
static void chop_port( char *buf )
{
	char	*d;			/* last dot (port) */

	if( (d = strrchr( buf, ':')) != NULL  ||  (d = strrchr( buf, '.' )) != NULL )
		*d = 0;

}

/*	figure out an ip address given a host name 
	convert name to a socket addr structure, then build the dotted decimal from that
*/
static char *get_ip_str( char *name )
{
	char	*a;			/* addr string */
	struct	sockaddr_in *sa;

	sa = ng_dot2addr( name );		/* do not embed in addr2 call or we have a leak */
	a = ng_addr2dot( sa );
	chop_port( a );
	ng_free( sa );

	return a;			/* addr2dot returns a buffer that we must free, so no strdup */
}

/*	return 0 if str contains pattern; if pat is NULL assume we have established reg_stuff */
static int match( char *str, char *pat )
{
	int k;
	static regex_t reg_stuff;		/* goo that the regex processor needs */
	static int ok = 0;			/* we have valid info in reg_stuff if not zero */

	if( pat && ! *pat )			/* empty pattern matches always */
	{
		if( ok )
			regfree( &reg_stuff );
		ok = 0;
		return 1;
	}


	if( pat )			/* re compute the pattern */
	{
		if( ok )
			regfree( &reg_stuff );

                if((k = regcomp(&reg_stuff, pat, REG_EXTENDED)) != 0){
                        char buf[4000];

                        regerror(k, &reg_stuff, buf, sizeof buf);
                        ng_cbleat( CBLEAT_ALL, 0, "dptree: internal regcomp failure 1! (%s)\n", buf);
                        exit(3);
		}

		ok = 1;
	}

	if( !str )
		return 1;
	if( ! ok )			/* no pattern given -- always match */
		return 0;

	return regexec( &reg_stuff, str, 0, 0, 0 );
}

/*	calculate a random value between min and max */
static int get_rand( int min, int max )
{
	double d;

        d = (drand48( ) * (double) (max-min)) + (double) min;

	return (int) d;
	
}

/*	convert an address such that the last .xxxxx (port) is the srvm port
	Messages from UDP are likely to have some random port number, but we need
	to send conn messages back to 29055 (or NG_SRVM_PORT).
*/
static char *cvt_addr( char *old )
{
	char buf[1024];
	char *p;

	sfsprintf( buf, sizeof( buf ), "%s", old );
	if( (p = strrchr( buf, ':' )) || (p = strrchr( buf, '.' )) )
	{
		*(p+1) = 0;
		strcat( buf, port_str );	
	}
	
	return ng_strdup( buf );		/* likely not what they want, but we dont care at this point */
}


/* 	send one of our canned messages on the multicast medium 
	if we are isolated, the message is sent to our parrot session 
*/
static void telegraph( int what, char *ubuf )
{
	struct	session *sb = NULL;
	char	buf[2048];
	int	to_nnsd = 0;		/* set if we are sending to nnsd */

	switch( what )
	{
		case TG_ALIVE:
			if( ! flags & DPT_FL_USE_NNSD )
				return;				/* not needed with old mcast protocol */

			sfsprintf( buf, sizeof( buf ), "%s:alive:%s!%s\n", udp_id, my_name, league );	
			to_nnsd = 1;
			break;

		case TG_IAMROOT:
			sfsprintf( buf, sizeof( buf ), "%s:iamroot:%s:%s:%s\n", udp_id, league, my_name, udp_id );	
			to_nnsd = 1;
			break;

		case TG_ADOPT:
			sfsprintf( buf, sizeof( buf ), "%s:adopt:%s:%d\n", udp_id, league, caste );	
			break;

                case TG_QUERY:							/* determine if there are any better parents in the tree */
                        sfsprintf( buf, sizeof( buf ), "%s:query:%s\n", udp_id, league );
                        break;

		case TG_BUF:		/* prebuilt buffer */
			sfsprintf( buf, sizeof( buf ), "%s:%s\n", udp_id, ubuf );	
			break;

		default:
			return;
	}

	if( flags & DPT_FL_USE_NNSD )
	{
		if( to_nnsd )
			send2nnsd( buf );		/* send just to nnsd daemons */
		else
			send2peers( buf );		/* send to all srvm we know about */
	}
	else
	{
		if( flags & DPT_FL_NO_MCAST )		/* no multicast avail -- send to parrot as our agent */
		{
			/*for( sb = session_list; sb && sb->type != ST_AGENT; sb = sb->next );*/
			if( agent_sb )
				ng_sisendt( agent_sb->sid, buf, strlen( buf ) );
		}
		else
			ng_sisendu( mcast_ip, buf, strlen( buf ) );			/* do NOT use send_udp for this one */
	}
}



/* add msg to the queue of chached messages that we need to send when we get a parent */
static void cache_msg( char *msg )
{
	struct cached_msg *m;		

	m = (struct cached_msg *) ng_malloc( sizeof( *m ), "cached message" );

	m->next = NULL;
	m->msg = ng_strdup( msg );

	if( mcache_t )				/* add to tail */
		mcache_t->next = m;
	mcache_t = m;
	if( ! mcache_h ) 
		mcache_h = m;
}

/* send any data that has been cached while we were attempting to attach to the tree */
static void send_cached( int sid )
{
	struct cached_msg *m;		
	struct cached_msg *n;		
	int 	count = 0;
	int	err = 0;

	for( m = mcache_h; m; m = n )
	{
		count++;
		n = m->next;			/* at next now so we can delete m when done */
		ng_cbleat( CBLEAT_LIB, 2, "dptree: send cached message: %s", m->msg );
		if( ng_siprintf( sid, "%s\n", m->msg ) != SI_OK )		/* 1 or -1 indicate data queued or error */
			err = 1;					/* we should bail on the session in either case */
		ng_free( m->msg );
		ng_free( m );
	}

	mcache_h = mcache_t = NULL;
	if( count )
		ng_cbleat( CBLEAT_LIB, 1, "dptree: sent %d cached messages to %d", count, sid );
}	

/* response to a query -- similar to state response but we assume its another dptree process that is getting the message so
        we need to ensure that the format has our id and is a legit tree commmand
*/
static void send_qresponse( int sid, char *addr )
{                   
        char wbuf[2048];     
                        
                    
        if( addr )
        {
                sfsprintf( wbuf, sizeof( wbuf ), "%s:stats:%s:%s:%s:%s:%d:%d\n", udp_id, my_name ? my_name : "UNKNOWN", my_ip ? my_ip : "UNKNOWN",  cluster, league, children, caste );
                send_udp( addr, wbuf, strlen( wbuf ) );
        }
        else
                if( sid >= 0 )
                        ng_siprintf( sid, "%Tstats:%s:%s:%s:%s:%d:%d\n", my_name ? my_name : "UNKNOWN", my_ip ? my_ip : "UNKNOWN",  cluster, league, children, caste );
}                
      

/* format a state message in caller's buffer */
static void send_state( int sid, char *addr )
{
	char wbuf[2048];

	sfsprintf( wbuf,  sizeof( wbuf ), "STATE: %s %s %s(me) %s(mom) %s(cluster) %s(league) %d(kids) %d(caste) 0x%x(flags) %s(ver)\n", 
			udp_id, 
			my_name ? my_name : "UNKNOWN", 
			my_ip ? my_ip : "UNKNOWN",  
			flags & DPT_FL_ROOT ? "I_AM_ROOT!" : (flags & DPT_FL_ADOPTED) ? parent_id : "ORPHANED", 
			cluster, league, children, caste, flags, version  );

	if( addr )
		/*ng_sisendu( addr, wbuf, strlen( wbuf ) );*/
		send_udp( addr, wbuf, strlen( wbuf ) );
	else
		ng_siprintf( sid, "%s", wbuf );

}

/* ---------------------------- session management functions ------------------------------ */
static int get_parent_sid( )
{
	struct session *sb;

	for( sb = session_list; sb && sb->type != ST_PARENT; sb = sb->next );
	return sb ? sb->sid : -1;
}

static struct session *get_parent_sb( )
{
	struct session *sb;

	for( sb = session_list; sb && sb->type != ST_PARENT; sb = sb->next );
	return sb;
}

static struct session *add_session( int sid, int type )
{
	struct session *sb;

	sb = (struct session *) ng_malloc( sizeof( struct session ), "session blk" );

	memset( sb, 0, sizeof( struct session ) );

	sb->next = session_list;
	if( session_list )
		session_list->prev = sb;
	session_list = sb;

	sb->type = type;
	sb->sid = sid;

	return sb;
}


/* cleanup a session block; does NOT close the actual session with si */
static void drop_session( struct session *sb )
{
	int cb4;		/* children before drop */

	cb4 = children;
	if( sb )
	{
		switch( sb->type )
		{
			case ST_PARENT:			/* parent lost, we must request another adoption */
				flags &= ~DPT_FL_ADOPTED;	/* no longer inserted in the tree */
				flags |= DPT_FL_DELAY_AR;	/* we must delay the adoption request */
				if( parent_id )
				{
					ng_free( parent_id );
					parent_id = NULL;
				}

				ng_cbleat( CBLEAT_LIB, 1, "dptree: parent session droped: %d", sb->sid );
				if( ! children )	/* if we dont have any kids, re insert at any level */
					caste = 9999;
				break;

			case ST_CHILD:			/* child -- we can dec the child count */
				children--;
				ng_cbleat( CBLEAT_LIB, 1, "dptree: child session droped: %d c=%d", sb->sid, children );
				break;

			case ST_AGENT:			/* our agent is gone; we die too */
				ng_cbleat( CBLEAT_ALL, 0, "dptree: agent session has died -- we cannot survive without it" );
				agent_sb = NULL;	/* prevent accidents */
				ok2run = 0;
				break;

			default:			/* assume an alien session and we really dont care bout those */
				ng_cbleat( CBLEAT_LIB, 3, "dptree: alien session droped: %d", sb->sid );
				break;
		}

		ng_cbleat( CBLEAT_LIB, 2, "dptree: session dropped: id=%d type=%d children=%d/%d", sb->sid, sb->type, cb4, children );

		if( sb->flow )
			ng_flow_close( sb->flow );
		if( sb->next )
			sb->next->prev = sb->prev;
		if( sb->prev )
			sb->prev->next = sb->next;
		else
			session_list = sb->next;

		ng_free( sb );
	}
}

static void drop_parent( )
{
	struct session *sb;

	if( (sb = get_parent_sb()) )
	{
		ng_siclose( sb->sid );			/* close her up */
		drop_session( sb );
		flags &= ~DPT_FL_ADOPTED;
	}
}

		/* agents, parent and aliens are left alone */
static void drop_kids( )
{
	struct session *sb;
	struct session *next = NULL;

	for( sb = session_list; sb; sb = next )
	{
		next = sb->next;
		if( sb->type == ST_CHILD )
		{
			ng_siclose( sb->sid );		/* close the session */
			drop_session( sb );		/* removes from list and dec the child count */
		}
		
	}
	ng_cbleat( CBLEAT_LIB, 1, "dptree: all child sessions dropped c=%d", children );
}

static void close_sessions( )
{
	while( session_list )
	{
		ng_siclose( session_list->sid );		/* close the session */
		drop_session( session_list );			/* remove from the queue */
	}
	ng_cbleat( CBLEAT_LIB, 1, "dptree: all sessions closed" );

	sleep( 1 );
}


/* 	open a tcp session with an agent (parrot) who will act for us on the mcast group  or as a general agent
*/
static int contact_agent( char *address, int port, char *group )
{
	int sid;

	if( (sid = ng_siconnect( address )) >= 0 )		/* open a tcp session to the agent */
	{
		agent_sb = add_session( sid, ST_AGENT );

		if( flags & DPT_FL_USE_NNSD )			/* using nnsd, and agent info supplied, so we use as a general agent; put into that mode */
		{
			ng_cbleat( CBLEAT_LIB, 1, "dptree: agent connected; putting into general mode" );
                        ng_siprintf( sid, "~GENERALAGENT\n" );     /* put parrot into general udp agent mode; all udp responses are sent to parrot */
		}
		else
		{
			flags |= DPT_FL_NO_MCAST;
			ng_cbleat( CBLEAT_LIB, 1, "dptree: agent connected; putting into multicast mode" );
			ng_siprintf( sid, "~M %d %s", port, group );		/* put parrot into multcast mode */
		}

		flags |= DPT_FL_NO_ROOT + DPT_FL_HAVE_AGENT;		/* cannot be root &  causes all udp messages to be sent to agent for forwarding */
		return 0;
	}

	ng_cbleat( CBLEAT_ALL, 0, "connection to agent failed: %s, %s", address, strerror( errno ) );
	return 1;
}

/* -----------------  tree oriented routines ----------------------------- */

/* allow user to control the flag character */
void ng_dpt_set_tflag( unsigned int f )
{
	tree_cmd_flag = (char) (f & 0x7f);
	ng_cbleat( CBLEAT_LIB, 1, "tree command flag set to: 0x%x", tree_cmd_flag );
}


/* send message to all buddies (children and parent) except the one that matches the session id sid */
void ng_dpt_propigate( int sid, char *buf )
{
	struct session	*sb;
	struct session	*nxt_sb;

	if( !(flags & DPT_FL_ADOPTED) && !(flags & DPT_FL_ROOT)  )			/* we dont have a parent and are not root */
		cache_msg( buf );						/* we must cache so when we get adopted we can send on */

	for( sb = session_list; sb; sb = nxt_sb )
	{
		nxt_sb = sb->next;				/* in case we have to drop the session */

		if( sb->sid != sid && sb->type <= ST_PROP2 )	/* something we can send to, and msg did not come from them */
		{
			if( ng_siprintf( sb->sid, "%s\n", buf ) != SI_OK )	/* error or session blocked */
			{
				ng_cbleat( CBLEAT_ALL, 0, "dptree: propigate: session (%d) started to queue; dropped. type=%d", sb->sid, sb->type ); 
				ng_siclear( sb->sid );				/* clear the queue */
				ng_siclose( sb->sid );
				cbdisc( NULL, sb->sid );			/* emulate a disconnect -- this frees sb! */
				sb = NULL;
			}
		}
	}

}

/* send a ping message to children to test for circular tree */
static void send_ping( )
{
	char msg[NG_BUFFER];
	

	if( children  &&  flags & DPT_FL_ADOPTED )
	{
		sfsprintf( msg, sizeof( msg ), "%cping:%s:%s", tree_cmd_flag, udp_id, my_ip );
		ng_cbleat( CBLEAT_LIB, 2, "dptree: init ping: %s", msg );
		ng_dpt_propigate( get_parent_sid( ), msg );		/* send only to kids */
	}
	else
		ng_cbleat( CBLEAT_LIB, 2, "dptree: no ping: flags=%x children=%d", flags, children );
}

/*	periodic maintenance -- 
	perform tree oriented maintenance (shuffling and root bcast), and drive dpt_user_maint() for user things like
	checkpointing and reloads.
*/
static void maint( )
{
	static ng_timetype nxt_root_bcast = 0;		/* if we are root we announce our rootness every now and again */
	/*static ng_timetype nxt_shuffle = 0; now global to support forced shuffle request */		/* next time we will try to reposition ourselves up in the tree */
	static ng_timetype nxt_alive = 0;		/* next we send nnsd an alive message */

	char	buf[1024];
	char	cbuf[2048];
	int	delay;
	ng_timetype now;
	int 	status;

	now = ng_now( );

	if( (flags & DPT_FL_ROOT) && now > nxt_root_bcast )
	{
		nxt_root_bcast = now + ROOT_BCAST_FREQ;
		telegraph( TG_IAMROOT, NULL );				/* lay claim to our rootness every so often */
	}

	if( now > nxt_alive )						/* nnsd if using, and ping to prevent accidental loops in the tree */
	{
ng_cbleat( CBLEAT_LIB, 0, "nxt_alive=%I*d", Ii(nxt_alive) );
		nxt_alive = now + ALIVE_BCAST_FREQ;
		if( flags & DPT_FL_USE_NNSD )
			telegraph( TG_ALIVE, NULL );			/* keep us alive in the nnds world */
		send_ping();
	}


	if( now > nxt_shuffle )			/* see if we can reposition ourself higher in the tree -- only if we dont have kids */
	{
		if( flags & DPT_FL_QUERY_PENDING )                  /* if we previously sent round a query request */
		{
			if( better_parents > 0 || query_responses < 3 )         /* must have a good number of responses to assume nothing better */
			{
				ng_cbleat( CBLEAT_LIB, 1, "attempting to reposition ourself in the tree: caste was %d better=%d resp=%d", caste, better_parents, query_responses );
				drop_parent(  );                /* detach from the parental unit and seek better relatives */
				flags &= ~DPT_FL_DELAY_AR;          /* we dont want this one delayed */
				caste = 9999;
			}
			else
				ng_cbleat( CBLEAT_LIB, 1, "not worth trying reposition: better=%d resp=%d", better_parents, query_responses );
                 
			flags &= ~DPT_FL_QUERY_PENDING;
			nxt_shuffle = now + get_rand( 18000, 36000 );                   /* sometime 30 to 60 minutes (tenths of seconds) from now */
		}
		else
		{
			better_parents = 0;
			query_responses = 0;                    /* reset counters before sending query */
			ng_cbleat( CBLEAT_LIB, 1, "sending query to determine if there are better parents: desired caste < %d", caste-1 );
			telegraph( TG_QUERY, NULL );            /* query other narbaleks for stats (caste, #kids etc) */
			flags |= DPT_FL_QUERY_PENDING;
			nxt_shuffle = now + 50;                 /* 5s to allow for responses */
		}
	}


	if( ! dpt_user_maint( flags ) )		/* drive user */
	{
		ok2run = 0;
		ng_cbleat( CBLEAT_LIB, 1, "dptree: dpt_user_maint() funciton returned 0 -- starting termination" );
	}
	flags &= ~DPT_FL_COLLISION;			/* collision flag must be reset after user maint call */

	ng_cbleat( CBLEAT_LIB, 9, "dptree: maint: flags = %02x  league=%s children=%d caste=%d", flags, league, children, caste );
}

/* free all blocks on the passed in node info list */
static void drop_nilist( struct node_info *l )
{
	struct node_info *p;

	while( l )
	{
		p = l;
		l = l->next;			
		ng_free( p->addr );
		ng_free( p );
	}
}

/*	start a session  with a potential parent -- returns pointer to the node info if we connect; null if not 
	we will try all in the list that responded to our adoption request 

	We MUST NOT ever select a caste that is lower than ours.  If we still have a subfamily attached this 
	prevents us from attaching to one of our (grand)children -- worse than incest!  If we give up on 
	attaching at the same or better station in the tree, then we will drop the subfamily, and set our caste to
	something high that will allow us to attach.
*/
static int attach2parent( )
{
	int	low_kids;			/* water marks for finding the best parent that can adopt us */
	int	low_caste;
	int	sid;				/* seesion id of the new session */
	struct	node_info *p;			/* points at next node info block to try */
	struct	node_info *n;			/* list walking pointer */
	char	*caddr; 			/* the address we will try to connect to */
	char	buf[1000];

	while( 1 )
	{
		low_caste = caste;
		low_kids = 99;
		p = NULL;
		for( n = avail_list; n; n = n->next )		/* select potential parent highest in the chain with the fewest kids */
		{
			if( n->caste >= 0 )			/* not one we tried already */
			{
				if( n->caste < low_caste  )		/* highest in tree  that we have seen */
				{
					p = n;
					low_kids = n->kids;
					low_caste = n->caste;
				}
				else
				{
					if( n->caste == low_caste && n->kids < low_kids  )	/* same level as prev seen, but fewer kids */
					{
						p = n;
						low_kids = n->kids;
					}
				}
			}
		}

		if( ! p )
			return 0;			/* bail out now -- nothing left to try */
	
		caddr = cvt_addr( p->addr ); 		/* cvt the addr from use-me so that it has the srvm tcp listen port */

		if( caddr && (sid = ng_siconnect( caddr )) >= 0 )		/* open a tcp session to the parent */
		{
			ng_siprintf( sid, "%ctype:child\n", tree_cmd_flag );		/* officially join the family -- tell mom we are child */
			add_session( sid, ST_PARENT );	

			flags |= DPT_FL_ADOPTED;			
			caste = p->caste + 1;				/* we are one lower on the food chain than the parent node */
			ng_cbleat( CBLEAT_LIB, 1, "dptree: adopted, parent is %s; our caste is: %d", p->addr, caste );
			if( children )			/* might need to recaste the children */
			{
				ng_cbleat( CBLEAT_LIB, 1, "dptree: recasting children" );			/* children should schedule a reload from us */
				sfsprintf( buf, sizeof( buf ), "%ccaste:%d", caste, tree_cmd_flag );
				ng_dpt_propigate( sid, buf );
			}

			parent_id = ng_strdup( p->addr );
			chop_port( parent_id );				/* we only really want the ip and not port */
			send_cached( sid );				/* send them updates we got BEFORE they load us */

			ng_free( caddr );
			return 1;
		}

		ng_cbleat( CBLEAT_ALL, 0, "dptree: adoption, failed %s: %s", caddr, strerror( errno ) );
		p->caste = -1;			/* keeps us from trying this again */
		if( caddr )
			ng_free( caddr );
	}						/* end while */

	return 0;				/* should never get here */
}

/*	-------------------------------------------------------------------------------------
	insert ourself into  the tree.  Until we are in the tree (adopted) this routine is 
	invoked with each main loop pass. There are several states that we deal with here:
		
		delay before sending adoption request:
			if( lease is set )
				return -- we will be called again later to check
			else
				set lease and return.

		adoption request has not been broadcast
			if we do not have a valid node list -- send a node list request; set delay
			else (we do have a good list)
			send AR, set lease and return.  we will be recalled to check.

		adoption request has been broadcast
			if lease has expired
				check responses from potential parents and choose the most attractive.
			else
				return -- we will be recalled.


	If we send an AR and hear nothing back, and do not hear an 'iamroot', we assume that we 
	broadcast on the group, then we can become the root node. We must try to be careful not 
	to collide with another node taking over root -- there is logic in tree-command that 
	deals with a collision. 

*/
static void insert( )
{
	static	ng_timetype lease = 0;		/* time after which it is safe to continue in the current state */
	static 	int attempts = 0;			/* if we try more than 5 times, we disconnect from the kids and try at any level */

	ng_timetype now;
	char	buf[1000];

	now = ng_now( );

	if( lease > now )			/* request still pending or a delay before issuing the request - bail now */
		return;

	if( flags & DPT_FL_DELAY_AR ) 	/* set an adoption delay, or the lease was set to mark end of an adoption delay */
	{
		if( lease )			/* the lease popped */
		{
			lease = 0;		/* cause us to make our request now */
			flags &= ~DPT_FL_DELAY_AR;
			ng_cbleat( CBLEAT_LIB, 1, "dptree: adoption request delay cleared" );
		} 
		else
		{
			lease = now + get_rand( 10, 50 );		/* get a delay (tenths) between 1 and 5 seconds */
			ng_cbleat( CBLEAT_LIB, 1, "dptree: delaying adoption request %d tenths of seconds", (int) (lease - now) );
			if( flags & DPT_FL_USE_NNSD )
				request_list( );				/* request a node list from nnsd while we wait */
			return;
		}
	}
	
	if( lease == 0 )				/* we need to send out the adopt request */
	{
		flags &= ~DPT_FL_ROOT_KNOWN;	
		telegraph( TG_ADOPT, NULL );		/* send a request for adoption -- if in nnsd mode only if valid list */
		
		drop_nilist( avail_list );		/* should be null, but lets make sure */
		avail_list = NULL;
		ng_cbleat( CBLEAT_LIB, 1, "dptree: adoption request broadcast; waiting for responses" );
		lease = now + MIN_ADOPT_LEASE;				/* min time we will wait to collect responses */
		return;
	}

	lease = 0;
	if( avail_list == NULL )				/* end of lease and no prospects */
	{
	/*	if( can_be_root() )			*/	/* if we can become root empty node list and DPT_FL_NO_ROOT is not set */
		if( ! (flags & (DPT_FL_ROOT_KNOWN+DPT_FL_NO_ROOT)) )	/* we have not seen an iamroot message, and we can be root */
		{						/* assert ourselves as the grand-poobah (root) */
			ng_cbleat( CBLEAT_LIB, 1, "dptree: no prospects after adopt broadcast -- we are taking over ROOT!" );

			telegraph( TG_IAMROOT, NULL );		/* notify others that we are taking over */
			flags |= DPT_FL_ADOPTED + DPT_FL_ROOT;		/* ok we lie; its easier to pretend that we have parents like the rest of the kids*/
			caste = 0;				/* top of the food chain */

			if( children )			/* certainly need to restation our children's posisition in life */
			{
				ng_cbleat( CBLEAT_LIB, 1, "dptree: recasting children" );			/* children should schedule a reload from us */
				sfsprintf( buf, sizeof( buf ), "%c=caste:%d", caste, tree_cmd_flag );
				ng_dpt_propigate( -1, buf );			/* we dont have a sid that we ignore */
			}
		}
		else
		{
			flags |= DPT_FL_DELAY_AR;						/* ensure we delay a bit */
			if( ++attempts > 5 )
			{
				attempts = 0;
				ng_cbleat( CBLEAT_LIB, 1, "dptree: still no prospective parents, dropping children and inserting anyplace" );
				/*close_sessions( );*/
				if( children )
				{
					drop_kids( );			/* dont ditch the agent if we are working that way */
					children = 0;
				}
				caste = 9999;
			}
			else
				ng_cbleat( CBLEAT_LIB, 1, "dptree: no prospects after adopt broadcast" );		/* we will try again later */
		}

		return;
	}

	if( ! attach2parent() )
	{
		if( ++attempts > 2 )		/* we make two adopt-me requests before we assume we cannot reinsert at our old caste or better */
		{
			ng_cbleat( CBLEAT_LIB, 1, "dptree: still no prospects, abandoning hope of keeping our sub-tree; releasing sessions" );
			attempts = 0;
			/*close_sessions( ); */		/* we must ditch our sub family if we dont get in at the same level */
			if( children )
			{
				drop_kids( ) ;
				children = 0;
			}
			caste = 9999;
		}
	}

	drop_nilist( avail_list );		
	avail_list = NULL;
}

/* process tree management command buffer =*  from another srvm via either raw or cooked interface.
	expected commands are (after = is removed if buf received on cooked session):
		adopt:league:caste		(looking to be adopted by a parent in league <= caste)
		caste:value			(parent has a new caste, adjust ours and trickle to underlings)
		stats:nodename:ip:cluster:league:children:caste   (response to a query for better parent request)
		shuffle				(forces a shuffle attempt)
		type:{child|???}		(type of session)
		use-me:caste:children		(node will accept us as a child)


	when an adoption request is recevied, we will always respond if our current
	brood is less than max-children.  This might result in more children than 
	the law allows, but it will not be that many more and its easier than 
	managing leases on our responses or requiring children to nack rejected
	potential parents. 
*/
static void do_tree_cmd( struct session *sb, char *from, char *buf )
{
	static char	**tlist = NULL;		/* pointer to tokens split from input buffer */
	static int responses = 0;
	static ng_timetype resp_timer = 0;	/* timer that we block responses */
	
	struct	node_info *n;
	char 	*addr = NULL;
	int	rcaste = 0;		/* the cast they are looking to join */
	char	wbuf[1024];
	int	tcount;			/* number of tokens in the buffer -- tokens expected to be : separated */
	ng_timetype now;


	tlist = ng_tokenise( buf, ':', '\\', tlist, &tcount );		/* make token list, reuse previous if weve been here before */

	if( ! sb )			/* we get sb for cooked data only */
		addr = from;		

	now = ng_now( );

	switch( *(tlist[0]) )
	{
		case 'a':				/* adoption request from some other node */
			if( tcount >= 3 )		/* must be adopt:league:caste */
			{
				if( strcmp( league, tlist[1] ) == 0 ) 		/* same league as us */
				{	
					if( flags & DPT_FL_ROOT )
						telegraph( TG_IAMROOT, NULL );		/* should keep things honest */

					if( resp_timer < now )
						responses = 0;			/* assume they chose other parent */

					if( ! (flags & DPT_FL_ADOPTED) )		/* we cannot respond unless we have a parent */
					{
						ng_cbleat( CBLEAT_LIB, 2, "dptree: ignored adoption request: we are not adopted" );
						break;
					}

					/*if( (responses + children < max_kids) && (flags & DPT_FL_LOADED) ) */
					if( (responses + children < max_kids) )
					{
						rcaste = atoi( tlist[2] );
						if( caste <= rcaste  && children < max_kids )	/* high enough in the food chain, and not at max kids */
						{
							/* so messages come directly back to this process, not through the agent, we use              */
							/* @addr  as it causes dptree receiver to use addr as the sender and not what udp/ip supplies */
							if( flags & DPT_FL_HAVE_AGENT )	
								send_udp( addr, wbuf, sfsprintf( wbuf, sizeof( wbuf ), "@%s:%s:use-me:%d:%d\n", my_name, udp_id, caste, children ) );
							else
								send_udp( addr, wbuf, sfsprintf( wbuf, sizeof( wbuf ), "%s:use-me:%d:%d\n", udp_id, caste, children ) );

							ng_cbleat( CBLEAT_LIB, 2, "dptree: responded to adoption request from %s: %s", addr, wbuf );
							responses++;
							resp_timer = now + 50;			/* clear response counter in 5 sec */
						}
						else
							ng_cbleat( CBLEAT_LIB, 3, "dptree: ignored adoption request: rcaste=%d caste=%d  kids=%d", rcaste, caste, children );
					}
					else
						ng_cbleat( CBLEAT_LIB, 3, "dptree: ignored adoption request: responses+kids > max: %s", tlist[1] );

				}
				else
					ng_cbleat( CBLEAT_LIB, 3, "dptree: ignored adoption request: wrong league: %s", tlist[1] );
			}
			else
				ng_cbleat( CBLEAT_ALL, 0, "dptree: bad adopt command from %s: %s", addr ? addr : "address missing", buf );
				
			break;
	
		case 't':			/* type of session - should only be children checking in */
			if( tcount > 1 && sb )
			{
				if( *(tlist[1]) == 'c' )
				{
					sb->type = ST_CHILD;		/* a child picked us, we are now mommy and daddy */
					children++;			/* for tax purposes */
					ng_cbleat( CBLEAT_LIB, 1, "dptree: adoption confirmed on sid: %d  c=%d", sb->sid, children );
					responses--;
				}
				else
					sb->type = ST_ALIEN;		/* parents dont connect to their children so we assume unknown */
			}
			else
				if( sb )
					sb->type = ST_ALIEN;
			break;

		case 'c':								/* parent has a new caste -- adjust ours */
			if( tcount > 1  && (rcaste = atoi( tlist[1] ) + 1) > 0 ) 	/* our new caste is one more than moms */
			{
				if( rcaste > max_depth )
				{
					ng_cbleat( CBLEAT_ALL, 0, "abort: received caste value that smells: %d; (%s)", rcaste, buf );
					exit( 20 );
				}
				ng_cbleat( CBLEAT_LIB, 1, "dptree: adjusting caste value: new=%d old=%d", rcaste, caste );
				caste = rcaste;
				sfsprintf( wbuf, sizeof( wbuf ), "%ccaste:%d\n", caste, tree_cmd_flag );
				ng_dpt_propigate( sb->sid, wbuf );			/* children need to recalculate their standing */
			}
			break;

		case 'i':			/* another node has declared that they are root -- iamroot:league:node-name:udp-id */
			ng_cbleat( CBLEAT_LIB, flags & DPT_FL_ADOPTED ? 4 : 1, "dptree: iamroot message received: %s", buf );
			if( tcount >= 4 && strcmp( tlist[1], league ) == 0 )		/* enough parms, and for our league */
			{
				if( flags & DPT_FL_ROOT )				/* trouble in river city if we also think we are root */
				{
					flags |= DPT_FL_COLLISION;			/* this to let our user know there was a collision detected */

					if( strcmp( udp_id, tlist[3] ) < 0 )		/* we win! */
					{
						ng_cbleat( CBLEAT_LIB, 1, "dptree: root collision (we win) with: %s %s %s", tlist[1], tlist[2], tlist[3] );
						telegraph( TG_IAMROOT, NULL );			/* they should cease */
					}
					else
					{
						ng_cbleat( CBLEAT_LIB, 1, "dptree: root collision (we cede) with: %s %s %s", tlist[1], tlist[2], tlist[3] );
						flags &= ~DPT_FL_ROOT;
						flags &= ~DPT_FL_ADOPTED;
						flags |= DPT_FL_DELAY_AR;
						if( children ) 
							close_sessions( );		/* drop all of our sessions */
					}
				}

				flags |= DPT_FL_ROOT_KNOWN;		/* we know that the root exists */
			}
			else
				ng_cbleat( CBLEAT_LIB, 3, "dptree: ignored iamroot message received: %s", buf );
			break;

		case 'L':						/* list from nnsd */
			save_list( tlist[1] );
			break;

		case 'p':						/* ping message */
			if( strcmp( tlist[1], udp_id ) == 0 )		/* we should never get a ping sourced from us, if we do it means a circular path in the tree */
			{
				ng_cbleat( CBLEAT_LIB, 0, "dptree: circular path detected in the tree: %s", buf );
				ng_cbleat( CBLEAT_LIB, 0, "dptree: dropping parent session in order to break circular path" );
				drop_parent();
			}
			else
			{
				char m[NG_BUFFER];

				sfsprintf( m, sizeof( m ), "%c%s %s", tree_cmd_flag, buf, my_ip );		/* send ping along adding our ip address */
				ng_cbleat( CBLEAT_LIB, 2, "dptree: propigating ping: %s", m );
				ng_dpt_propigate( sb->sid, m );
			}
			break;

                case 'q':								/* query stats request */
                        if( tcount > 1 && strcmp( tlist[1], league ) == 0 )		/* respond only if in our league */
                        {
                                send_qresponse( sb ? sb->sid : -1, from );
                        }
                        break;

                case 's':                                               /* stats -- response to query */
                        /* expected tokens:  stats nodename ip cluster league chldren caste */
                        if( strcmp( tlist[0], "stats" ) == 0 )
                        {
                                if( tcount > 5 )                                /* should respond only if our league matched therirs */
                                {
                                        int c;
                                        int k;

                                        query_responses++;
                                        c = atoi( tlist[6] );           /* caste */
                                        k = atoi( tlist[5] );           /* current kid count */
                                        if( c < caste-1 && k < max_kids )               /*  node has room for a kid and is higher in the tree than our parent */
                                        {
                                                better_parents++;                       /* so add to the count */
                                                ng_cbleat( CBLEAT_LIB, 1, "better parent: %s caste=%s kids=%s", tlist[0], tlist[6], tlist[5] );
                                        }              
                                        else
                                                ng_cbleat( CBLEAT_LIB, 2, "unsuitable parent: %s caste=%s kids=%s", tlist[0], tlist[6], tlist[5] );
                                }
                        }
                        else    
                        if( strcmp( tlist[0], "shuffle" ) == 0 )		/* printf "shuffle:\n" | ng_sendtcp -t 1 localhost:xxxx */
                        {
                                ng_cbleat( CBLEAT_LIB, 1, "force shuffle request received" );              
                                nxt_shuffle = 1;                        /* if it is 0 shuffle does not trigger -- startup safeguard against immediate shuffle*/
                        }
                        break;

		case 'u':			/* use me as a parent -- use-me:caste:children  */
			if( tcount >= 3 && addr )
			{
				n = ng_malloc( sizeof( struct node_info ), "do_tree_cmd: use me" );
				n->addr = ng_strdup( addr );
				n->caste = atoi( tlist[1] );
				n->kids  = atoi( tlist[2] );
				n->next = avail_list;			/* push on avail list -- we select our parent when adopt lease pops */
				avail_list = n;
				ng_cbleat( CBLEAT_LIB, 1, "dptree: added potential parent: %s %s(caste) %s(kids)", addr, tlist[1], tlist[2] );
			}
			else
				ng_cbleat( CBLEAT_ALL, 0, "dptree: bad use-me command from %s: %s", addr ? addr : "address missing", buf );
			break;

		case 'D':				/* broadcast state */
			if( addr )
				send_state( -1, addr );
			break;

		default:
			ng_cbleat( CBLEAT_LIB, 1, "dptree: unrecognised tree command received: %s", buf );
			break;
	}
}

/* -------------------- callback routines ---------------------------*/
static int cbsig( char *data, int sig )
{
	return SI_RET_OK;
}

static int cbconn( char *data, int f, char *buf )
{
	add_session( f, ST_ALIEN );		/* assume alien until they tell us otherwise */
 	return( SI_RET_OK );
}

static int cbdisc( char *data, int f )
{
	struct session *sb;

	for( sb = session_list; sb && sb->sid != f; sb = sb->next );
	if( sb )
	{
		drop_session( sb );
		if( ! (flags & DPT_FL_ADOPTED) )		/* if we are no longer adopted, then we drop kids too */
		{
			if( children )
			{
				drop_kids( );
				children = 0;
			}
		}
	}
	else
		ng_cbleat( CBLEAT_ALL, 0, "dptree: disc received, but no session: id=%d", f );
 
	return SI_RET_OK;
}


/*	udp data -- multicast data  (tree oriented commands)
	we must be aware that we will most likely get messages from ourself that 
	are sent to the multicast group. we need to filter them out. to do this we 
	prefix all udp messages with an id that we believe to be unique and 
	filter off  messages with our id.
	
*/
static int cbudp( void *data, char *buf, int len, char *from, int fd )
{
	char *p;	/* pointer past prefix */

	buf[len-1] = 0;					/* all buffers are \n terminated -- we require this */
	if( *buf == '@' )			/* message was sent by an mcast agent (parrot) , @ipaddr:id:message is expected */
	{					/* replace from with ipaddr (the real sender) and move on */
		if( p = strchr( buf, ':' ) )
		{
			*p = 0; 
			from = buf+1;
			buf = p+1;
		}
		else
		{
			ng_cbleat( CBLEAT_LIB, 5, "dptree: udp buffer ignored: %s", buf );
			return SI_RET_OK;
		}
	}

	if( p = strchr( buf, ':'  ) )
	{
		ng_cbleat( CBLEAT_LIB, 5, "dptree: udp buffer from %s processed: %s", from, buf );
		*(p++) = 0;
		if( strcmp( buf, udp_id ) )		/* buffer is not from us */
		{
			do_tree_cmd( NULL, from, p );			/* only tree commands (adopt responses) come in from udp */	
			return SI_RET_OK;
		}
		else
		{
			if( ! my_ip )				/* cheat, but its easy  */
			{
				my_ip = ng_strdup( from );
				ng_cbleat( CBLEAT_LIB, 1, "dptree: we believe our ip to be: %s", my_ip );
				return SI_RET_OK;
			}
		}
	}
	
	ng_cbleat( CBLEAT_LIB, 5, "dptree: udp buffer ignored: %s", buf );
	return SI_RET_OK;
}

/* tcp data  
	we assume that cooked data will be newline terminated records. We 
	parse each record out of the buffer and if it begins with an equal
	sign (=) we assume it is for this layer. All other records are 
	given to the user data routine; one at a time. 
*/
static int cbtcp( void *data, int fd, char *buf, int len )
{
	struct session *sb;
	char	*rec;			/* pointer at next record to process */
	char	*p;			/* generic pointer at a portion of the record */
	char 	*name = NULL;
	char	*cond = NULL;		/* pointer to conditional phrase in set statemen */
	int	rcount = 0;		/* records in the buffer */
	char	wbuf[NG_BUFFER];

	ng_cbleat( CBLEAT_LIB, 5, "dptree: cook: %d bytes = (%.90s)%s", len, buf, len > 90 ? "<trunc>" : "" );

	for( sb = session_list; sb && sb->sid != fd; sb = sb->next );
	if( ! sb )
		return SI_RET_OK;


	if( sb->flow == NULL )
		sb->flow = ng_flow_open( NG_BUFFER );		/* establish a flow input buffer for the session */
	           
	ng_flow_ref( sb->flow, buf, len );              	/* regisger new buffer with the flow manager */

	if( sb->type == ST_AGENT )		/* must treat as if it is a UDP message from mcast */
	{
		ng_cbleat( CBLEAT_LIB, 3, "dptree: cbtcp: agent msg: (%s) len= %d", buf, len );
		while( rec = ng_flow_get( sb->flow, '\n' ) )		/* get all complete (new line terminated) records */
		{
		ng_cbleat( CBLEAT_LIB, 3, "dptree: cbtcp: agent msg split: (%s)", rec );
			if( (p = strchr( rec, ':' )) )				/* assume address:message */
			{
				*p = 0;
				cbudp( data, p+1, strlen( p+1 )+1, rec, fd );
			}
		}

		return SI_RET_OK;
	}

                        
	/* 
		we look at each record. if it is a tree command (lead =), then we process it. All other records we 
		drive the 'user' network interface function to deal with the buffer 
	*/
	while( rec = ng_flow_get( sb->flow, '\n' ) )		/* get all complete (new line terminated) records */
	{
		rcount++;

		switch( *rec )
		{
			case '=':				/* tree management command: sesison type, caste, use-me etc */
				do_tree_cmd( sb, NULL, rec+1 );
								/* tree cmds are from other srvm -- do not send end of transmission */
				break;

			default:
				if( ! dpt_user_data( sb->sid, rec, flags ) )
				{
					ng_cbleat( CBLEAT_LIB, 1, "dbtree: dpt_user_data() function returned 0 -- starting termination" );
					ok2run = 0;
					return SI_RET_OK;
				}
				break;
		}
	}				/* while */

	ng_cbleat( CBLEAT_LIB, 3, "dptree: cook: buffer contained %d records", rcount );
	return SI_RET_OK;
}

static void usage( )
{
	sfprintf( sfstderr, "usage: %s [-a mcast-agent] [-d nnsd-port] [-C ckpt-file] [-g mcast-group] [-l league]  [-P] [-v]  [-c cluster-name]  -p port\n", argv0 );
	exit( 1 );
}

/* -------------------------- public routines ----------------------------- */
void ng_dpt_set_noroot( int state )
{
	if( state )
		flags |= DPT_FL_NO_ROOT;
	else
		flags &= ~DPT_FL_NO_ROOT;
}

void ng_dpt_set_ttl( int n )
{
	if( n > 0 )
		mcast_ttl = n;
}

void ng_dpt_set_max_depth( int n )
{
	if( n > 10 )
		max_depth = n;
}

void ng_dpt_set_max_kids( int n )
{
	max_kids = n;
}

/* set up and drive */
int ng_dptree( char *pleague, char *pcluster, char *pport_str, char *mcast_agent, char *pmcast_group, char *pnnsd_port_str )
{
	extern	int errno;
	extern	int ng_sierrno;

	int	status = 0;
	int	i;
	int	port = 0;
	char	buf[4096];			/* work buffer and work pointer */
	char	*p;	

	signal( SIGPIPE, SIG_IGN );		/* must ignore or it kills us */

	nnsd_port_str = pnnsd_port_str;			/* globalise things */
	cluster = pcluster;
	port_str = pport_str; 
	mcast_group = pmcast_group;
	league = pleague;

	ng_sysname( buf, sizeof( buf ) );
	if( (p = strchr( buf, '.' )) )
		*p = 0;
	my_name = ng_strdup( buf );
	my_ip = get_ip_str( my_name );

	srand48( getpid() * ng_now( ) );		/* just in case two nodes managed to come up *right* at the same instant ;) */

	sfsprintf( buf, sizeof( buf ), "%I*x%05x%02x", sizeof( ng_timetype),  ng_now( ), getpid( ), get_rand( 0, 255 ) );  /* our id - date/pid/random */
	udp_id = ng_strdup( buf );

	if( !cluster )
		cluster = ng_strdup( "unknown" );		/* we assume that site-prep or something will inform us of this later */

	if( !port_str )				/* globalise what the user gave to us */
	{
		ng_cbleat( CBLEAT_ALL, 0, "dptree: abort:  port-number not supplied" );
		return( 1 );
	}
	port = atoi( port_str );

	if( nnsd_port_str )						/* we will be using nnsd rather than mcast */
	{
		ng_cbleat( CBLEAT_LIB, 1, "dptree: using nnsd processes listening on port: %s", nnsd_port_str );
		flags |= DPT_FL_USE_NNSD;
	}
	else
	{
		sfsprintf( buf, sizeof( buf ), "%s:%d", mcast_group, port );		/* build the full address:port for udp sends */
		mcast_ip = ng_strdup( buf );
	}
		
	ng_cbleat( CBLEAT_ALL, 0, "dptree: %s started, listening on port=%s group=%s id=%s", version, port_str, mcast_group ? mcast_group : "no-group", udp_id );

	ng_sireuse( 1, 1 );						/* allow udp port reuse, and tcp address reuse */
	if( ng_siinit( SI_OPT_FG, port, port ) != 0 )			/* port here is expected to be the mcast group port which MUST be opened first */
	{
		sfprintf( sfstderr, "unable to initialise network interface: ng_sierrno = %d errno = %d\n", ng_sierrno, errno );
		exit( 1 );
 	}


	ng_sireuse( 0, 0 );		/* turn off reuse and open a udp port with a unique number -- we use this  */
	ng_siopen( 0 );			/* UDP port to write from (default ng_si behaviour is to write to the last one opened */
					/* this ensures that responses come back to us and not some random 29055 port if 	*/
					/* multiple processes have it open for group activity */

	ng_cbleat( CBLEAT_LIB, 1, "dptree: listening on port: %s", port_str );

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    		/* register callbacks */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    
	ng_sicbreg( SI_CB_CDATA, &cbtcp, NULL );
	ng_sicbreg( SI_CB_RDATA, &cbudp,  NULL );
	ng_sicbreg( SI_CB_SIGNAL, &cbsig, NULL );

	if( (flags & DPT_FL_USE_NNSD) )						/* if using nnsd, must open agent session if asked for */
	{
		if( mcast_agent )						/* must create a direct tcp/ip session to an agent for general udp forwarding */
			if( contact_agent( mcast_agent, port, mcast_group ) > 0 )
				ng_cbleat( CBLEAT_ALL, 0, "dptree: unable to connect to agent: %s:%d", mcast_agent, port );   /* this maybe should be fatal */
	}
	else
	{							/* if not using nnsd, must open agent or mcast in order to join tree */
		if( mcast_agent )				/* must create a direct tcp/ip session to an agent that will mcast */
		{
			status = contact_agent( mcast_agent, port, mcast_group );
		}
		else
		{
			status = ng_simcast_join( mcast_group, -1, mcast_ttl );		/* join the srvm mcast group, on any interface */
		}

		if( status )
		{
			ng_cbleat( CBLEAT_ALL, 0, "dptree: abort: unable to join multicast group, or contact agent: %s", strerror( errno ) );
			exit( 4 );
		}

		ng_cbleat( CBLEAT_LIB, 1, "dptree: %s: %s ttl=%d status=%d errno=%d", mcast_agent ? "mcast via agent" : "joined mcast", mcast_ip, mcast_ttl, status, errno );
	}

	flags |= DPT_FL_DELAY_AR;			/* delay adoption request in case multi nodes powered up together */
	while( ok2run )
	{
		if( ! (flags & DPT_FL_ADOPTED) )	
			insert( );					/* insert us into the tree */

		maint( );					/* periodic maintenance (insert, ckpt etc) */

		errno = 0;
		/* poll for tcp/udp events; short delay if we are delaying an adoption request; max 2s if not */
		if( (status = ng_sipoll( (flags & DPT_FL_DELAY_AR) ? 20 : 200 )) < 0 )	
		{
			if( errno != EINTR && errno != EAGAIN )
				ok2run = 0;
		}
	}

	ng_cbleat( CBLEAT_LIB, 1, "dptree: shutdown in progress, closing sessions" );

	ng_siclose( ng_sigetlfd( ) );		/* close the listening port */
	ng_siclose( -1 );			/* close the UDP listen port */
	close_sessions( );			/* close up any paren/child sessions */

	ng_cbleat( CBLEAT_LIB, 1, "dptree: done. status = %d sierr =%d errno = %d", status, ng_sierrno, errno );
	return 0;
}             


#ifdef KEEP
/*
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_dptree:Distributed Process Tree Functions)

&space
&synop	
	#include <ningaui.h>
&break	#include <ng_lib.h>
&space
.** &break	int dpt_user_data( int sid, char *rec, int flags );		
.** &break	int dpt_user_maint( int flags );			
&break	int ng_dptree( char *league, char *cluster, char *port_str, 
&break	char *mcast_agent, char *mcast_group, char *nnsd_port_str );
&space	void ng_dpt_propigate( int sid, char *buf );
&break	void ng_dpt_set_ttl( int n );		
&break	void ng_dpt_set_tflag( char flag );		
&break	void ng_dpt_set_max_depth( int n );
&break  void ng_dpt_set_max_kids( int n );

&space
	link with libng_si or libng_si-g

&space
&desc	
	The distributed process tree functions allow a series of user programmes 
	to organise themselves into a binary tree that is then used to share information 
	between processes. After initialisation, the user programm invokes the &lit(ng_dptree()) 
	funciton which takes over control and provides the following functions:
&space
&beglist
&item	Joins the communication tree.
&space
&item	Allows other processes to join the tree with this process as its parent.
&space
&item	Manages tree reorganisation.
&space
&item	Drives a user callback function on a periodic basis for the user programme to 
	do work.
&space
&item	Drives a user callback function when data is received on one of the communicaiton 
	sessions to a peer programme. 
&endlist
&space
	Communication with peer processes is maintained via TCP/IP sessions.  Data received 
	on each TCP/IP session is assumed to be for the user programme &stress(UNLESS) it 
	begins with a lead equal (=) sign.  Data is also assumed to be newline terminated 
	and is passed to the user application one buffer at a time based on the newlines.
	The dptree functions handle partial buffers split across multiple TCP/IP messages. 
&space
&subcat	Insertion into the tree
	Joining the communications tree is accomplished either by broadcasting an 'adoption
	request' to a multicast group, or getting a list of registered peer processes and 
	sending an adoption request directly to each peer.  If using the latter method, 
	a narbalek name service daemon (nnsd) is used to obtain the list of node names, and 
	to register the process. The nnsd is also used to track which process is the root 
	of the communications tree.  
&space
	Peer processes that are able to accept this process as a child, respond to the 
	adoption request, and this process will select one of the responding processes 
	based on its position in the tree (as close to the top as possible). Once a process
	has joined the tree, it is capable of responding to adoption requests. 
&space
&subcat Tree reorganisation
	As nodes stop and start it is possible for the tree to become lopsided, and for 
	it to split.  When a parent process stops, all of the child processes below the 
	parent detach themselves from their parent and rejoin the tree.  Leaf processes
	will also detach from the tree on a periodic basis and rejoin if it appears that
	there is a better station in the tree.  This 'pruning' helps to keep the tree 
	balanced over time. 
&space
&subcat ng_dptree()
	The ng_dptree() function is invoked after the user programme has completed initialisation.
	Once invoked, ng_dptree() initalises the network socket interface (ng_si library)
	and begins its attempts to insert into the tree.  The parameters passed to this 
	function are:
&space
&begterms
&term	league : A pointer to a null terminated character string that identifies the group 
	of processes using the tree to communicate.  It is possible for multiple leagues to 
	use the same &lit(nnsd) processes to discover other peers, or to share the same 
	multicast group.  Responses to adoption requests, and root conflict resolution, are
	made based on the league name.

&space
&term	cluster : A pointer to a null terminated character string that names the cluster that 
	the node belongs to. 
&space
&term	port_str : A pointer to a null terminated character string that contains the TCP port 
	number that this process will open for listening.  The process will also open a
	UDP socket using the same port number.  It is expected that all process peers will 
	open TCP/UDP sockets using the same port number.
&space
&term	mcast_agent : A character string that indicates the node and port (name:port) of the 
	process that will be acting as a multicast agent for the process.  This allows the 
	process to be started in an environment that does not support multicast, yet still 
	allows the tree to be formed via multicast. A TCP/IP session is established between 
	the agent and this process.  Ng_parrot processes are capable of acting as multicast
	agents for processes using this library of functions.
&space
&term	mcast_group : This is a null terminated character string containing the multicast 
	group address (e.g. 207.0.0.1) that is to be used.  
&space
&term	nnsd_port_str : Is the null terminated string that contains the port number that 
	&lit(nnsd) procsses will be listening on.  If this value is NULL, then multicast 
	will be used to join the tree.  The list of &lit(nnsd) hosts is expected to be available
	via the &lit(NG_NNSD_LIST) pinkpages variable.  The list is refreshed about every 
	five minutes so that if the &lit(nnsd) hosts change running processes do not need to 
	be restarted. 
&endterms
&space
&subcat	ng_dpt_set_ttl()
	This function allows the user programme to specify the time to live (ttl) setting used
	on multicast messages.  The single parameter should be a multiple of 2 in the range of
	0 through 255. If used, this function must be invoked prior to invoking ng_dptree().
&space
&subcat ng_dpt_set_tflag()
	The dptree environment inetrcepts all TCP and UDP messages. By default all UDP 
	traffic to the process is considered to be oriented towards managing the 
	status within the communications tree.  Most TCP messages are passed through to 
	the user programme, but there are a few communications tree oriented messages
	that must be sent using the established TCP sessions.  These messages are all
	expected to start with a single character ID 'flag' which by default is the 
	equalsign (=).  If the user invokes this funciton, it allows the flag character
	to be changed.  This is necessary only if the user application will be sending 
	messages that begin with a leading equalsign.  The flag must be in the range of 
	0x01 through 0x7f inclusive. 
&space
&subcat ng_dpt_propigate( )
	This function allows the user programme to send a null terminated ascii message to 
	other peers in the communication tree.  The paramters to this function are:
&space
&beglist
&item	sid : A session id number. The message will NOT be sent to this session making 
	it easy to relay a buffer received from one peer to the others.  If the sid 
	passed to the function is -1, then the buffer is sent to all peers. 
&space
&item	buf : A null terminated character string that contains the message to send. The 
	ng_dpt_propigate() function will add the trailing newline to the message. 
&endlist
&space
&subcat	User Supplied Callback Functions
	The user must define two callback functions that will be invoked by the dptree functions.
	The prototypes for these functions are:
	
&space
&litblkb
   int dpt_user_data( int sid, char *rec, int flags );		
   int dpt_user_maint( int flags );			
&litblke
&space
	The dpt_user_data() function is invoked for each message that is received on any TCP/IP session
	when the first character of the message does not begin with an equal (=) sign. The parmeters
	passed in are:
&space
&begterms
&term	sid : The session id that the data was received from. This can be used when invoking 
	ng_tpd_propigate() in order to prevent the message from being returned to the sender, or
	can be used as the first parameter to an ng_siprintf() function call if a message is to 
	be sent back to the process. 
&space
&term	rec : A null terminated character string of data that was received on the session. 
&space
&term	flags : A set of bit flags that inidicate the state of the distributed process tree. 
	These flags are the DPT_FL_ constants defined in &lit(ng_lib.h,) and are listed below. 
&endterms
&space
	The function should return 1 if ng_dptree processing is to continue, and 0 if processing 
	should stop (control will be returned to the user programme as the dptree() function 
	will return). 

&subcat User_maint()
	The user_maint() function is invoked every few seconds and allows the user programme to 
	do work outside of processing messages.  The same state flags are passed in with each 
	invocation so that the user programme knows the state of the process with regard to 
	the communications tree. The funciton should return a value of 1 in order to indicate 
	that processing should continue. A return value of 0 indicates that the ng_dptree environment 
	should close sessions and return control to the user programme. 
&space
	It should be noted that execution of dpt_user_maint() and dpt_user_data() blocks all other processing
	of inbound UDP and TCP messages. 

&space
&subcat	Flags
	The following are the constants that can be used to check flags that are passed into 
	each of the two user callback functions.

&space
&begterms
&term	DPT_FL_ROOT	: This process is the root of the communications tree. 
&term	DPT_FL_ADOPTED	: This process has successfully joined the tree and has at least a TCP/IP session with its parent.
&term	DPT_FL_DELAY_AR	: The process needs to send an adoption request. The request is being delayed a few seconds in a backoff/retry state.
&term	DPT_FL_ROOT_KNOWN : The root of the treek is known.
&term	DPT_FL_NO_ROOT	: This procss is not allowed to become root.
&term	DPT_FL_NO_MCAST	: This process may not use multicast. 
&term	DPT_FL_HAVE_LIST : A list of peers from the &lit(nnsd) process was received. 
&term	DPT_FL_USE_NNSD	: An &lit(nnsd) process should be used to determine which hosts to send adoption requests to. 
&term	DPT_FL_COLLISION : This process, and another in the tree both thought they were the root process. One of the 
		processes ceded and gave up their claim. If the user process needs to take action based on a collision,
		then this flag should be tested for by the maintenance function.  The state of the root flag indicates
		wether or not the process still has claim to being the root of the communications tree.  The flag is 
		set after the user maintenance function is invoked, and could be set for multiple invocations of the 
		message processing function (not safe to test for/react to this flag in the user message processing 
		function).
&endterms
&space

&subcat Alien Sessions
	All TCP/IP connection requests are accepted by the dptree interface.  Sessions that are not 
	identified as tree oriented are considered to be 'alien.' This allows the user programme to 
	be connected with other processes that are not a part of the communication tree. All data, whether 
	received from a tree oriented session, or from an alien, will be delivered via the dpt_user_data() 
	callback function. 

&subcat Communication with the dptree environment
	All UDP messages, and TCP messages that begin with an equal sign (=) are assumed to be 
	messages that the dptree environment should process. UDP messages have the following 
	syntax: <process-id>:<data>  which allow multicast messages received by the sender to 
	easily be ignored.  The process-id is a unique string that is generated by the dptree
	environment when it is initialised.  
	The details of these messages can be ignored unless an external interface is being 
	written which will possibly send these messages to a process using the dptree environment.
	The following messages are recognised by the dptree environment:
&space
&begterms
		adopt:league:caste		(looking to be adopted by a parent in league <= caste)
		caste:value			(parent has a new caste, adjust ours and trickle to underlings)
		type:{child|???}		(type of session)
		use-me:caste:children		(node will accept us as a child)
&term	adopt:<league>:<caste> : This is an adoption request.  The league is supplied and only processes that
	are a member of the same league will potenitally reply to the request. The caste is a maximum 
	number 
&space
&term	caste:<value> : The process's parent has a new caste value and this process is expected to adjust 
	its caste to value+1.  The message, with the adjusted value, should be propigated to the children 
	of this process. 
&space
&term	iamroot:<league>:<node>:<id> : This message is broadcast by the process that believes it is the root
	of the communications tree. It is ignored by all processes that are not of the same league, and used
	to resolve collisions if more than one process believes that it is the tree's root.  
&space
&term	List:node... : This is a list of nodenames from an &lit(nnsd) process.  These are nodes that 
	have registered with &lit(nnsd) and &ital(might) be capable of responding to an adoption request.
	When not using multicast mode (nnsd_port_string supplied to ng_dptree()) adoption requests are
	sent directly to the nodes listed as a limited broadcast. 
&space
&term	type:child : Defines the type of session (expected only from a TCP/IP session). Currently the only 
	supported type is child. If this message is not recevied, the sesion is considered alien and the 
	disconnection of the session does not affect the management of the tree sessions. 
&space
&term	use-me:<caste>:<nchild> : This message is sent as a response to an adoption request. The local process
	will collect thes messages for a small amount of time following the broadcast of an adoption request, 
	and then will select the process that has the lowest caste number and the fewest number of children. 
&endterms

&see
	ng_srvmgr, ng_parrot
&space
&mods
&owner(Scott Daniels)
&lgterms
&term   25 Aug 2004 (sd) : Original code (as a part of narbalek). (HBD RMH)
&term	30 May 2006 (sd) : Conversion to a standalone set of libraries.
&term	12 Jun 2006 (sd) : Changed includes so that they pull from this directory first.
&term	17 Jun 1006 (sd) : Added support to send league info to nnsd processes. 
&term	19 Jun 2006 (sd) : Corrected the list prefix string to include trailing :. 
&term	21 Jun 2006 (sd) : Corrected propigation of caste message (missing lead =).
&term	20 Jul 2006 (sd) : Added ng_dpt_set_max_depth() function to allow us to check for bougus values.
&term	30 Aug 2006 (sd) : Converted strdup() calls to ng_strdup().
&term	28 Nov 2006 (sd) : Added ng_dpt_set_max_kids() function.
&term	28 Feb 2007 (sd) : Fixed memory leak in mk_dlist.
&term	03 Oct 2007 (sd) : Fixed manual page.
&term	24 Apr 2008 (sd) : Changed the propigate function to prevent core dump if we detect a wedged partner. (v1.1)
&term	06 May 2008 (sd) : Corrected error checking if doing mcast via agent; was skipping check.
&term	10 Mar 2009 (sd) : Embarassingly, fixed the declaration on static functions to be static. 
&term	12 Mar 2009 (sd) : Added prototypes for all static functions. 
&term	23 Apr 2009 (sd) : Now sends/supports the query logic that allows us to test for the possibility of 
		better parents so that we don't drop only to join at the same caste. Keeps the process in the 
		tree when the tree is optimally balanced.  Added ability for user programme to set the tree command
		flag (v2.0)
&term	04 May 2009 (sd) : Added root collision flag so that user processes will know when we've collided. 
&term	19 Oct 2009 (sd) : Added ping capability to detect circles in the tree. 
&endterms
&scd_end
*/
#endif

