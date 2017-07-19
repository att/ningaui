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
Mnemonic:	narbalek  - flock wide information map
Abstract:	daemon manages an in-core information map of name/value/comment tuples.
		This daemon runs on all hosts on the cluster and exchanges information
		in realtime with other nodes to keep the cartulary updated.

Date:		25 August 2004
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<signal.h>
#include	<regex.h>
#include 	<errno.h>
#include	<ctype.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>
#include	<ng_ext.h>
#include 	<ng_socket.h>

#include	"narbalek.man"

#include	"nar_const.h"


/* ----------------------------------- prototypes ------------------------------- */
static int match( char *str, char *pat );
static void send_udp( char *addr, char *buf, int len );
static int cbdisc( char *data, int f );


#ifdef OS_IRIX
#	ifndef isblank
#		define 	isblank __isblank
#	endif
#endif
/* 
	This seems to have been resolved -- at least on our most recent version of SunOS.
	Ive added a check for solaris versions 5.9 and earlier. for these we need to 
	define isblank as it seems missing on those systems.

	bloody solaris does not have an isblank function; erk 
	this must be early as it serves as a proto if defined. 
*/
#ifdef OS_SOLARIS 
#	if ( OS_MAJVER <= 5  && OS_MINVER <= 9 )
#		ifndef isblank
			int isblank( char c )
			{
				return (c == ' ') || (c == '\t'); 
			}
#		endif
#	endif
#endif


/* ----------------- globals (a few too many for my taste, but not so much to care about) ------------------ */
int	verbose = 0;
int	flags = 0;			/* FL_ constants */
int	ok2run = 1;			/* turned off when it is no longer safe to execute */
int	max_kids = 2;			/* max children we will allow -- variable so we can change on fly */
int	children = 0;			/* number of children sessions we have */
int	caste = 9999;			/* our position in the tree; once adopted it is parents caste +1 */
int	mcast_ttl = 2;			/* time to live for mcast messages */
char	*udp_id = NULL;			/* udp message id prefix -- uniquely identifies us to allow loopback bcast messages to be ignored */
char	*cluster = NULL;		/* the name of the cluster -- info map 'type' is controlled by the port */
char	*league = "flock";		/* the group of narbaleks that are bonded in the tree (could be multiple clusters) */
char	*mcast_group = "235.0.0.7";	/* all clusters can use the same group but we allow it to be supplied on the cmd line */
char	*mcast_ip = NULL;		/* full ip (group:port) for udp message sends */
char	*parent_id = NULL;		/* the name of our parent */
char	*my_ip = NULL;			/* ip address -- we dont care which interface */
char	*my_name = NULL;		/* my hostname */
char	*ckpt_file = NULL;		/* where we dump our sym table now and then */
char	*init_file = NULL;		/* used to initialise if ckpt file is not found */
char	*port_str = NULL;		/* the port we have a tcp listen on, and we expect all other narbs to listen on too */
char	*nnsd_port_str = NULL;		/* port that nnsd listens on -- if not supplied via -d we use mcast */
char	*version = "3.12/08109"; 

int	better_parents = 0;		/* count of nodes that responded to query and are better parents in the tree */
int	query_responses = 0;		/* number of nodes that responded to query */

ng_timetype nxt_shuffle = 0;		/* next time we will try to reposition ourselves up in the tree */

struct node_info *avail_list = NULL;	/* list of nodes that can possibly take us */
struct session *session_list = NULL;	/* all of the nodes we are in session with */
struct cached_msg *mcache_h = NULL;	/* head/tail: messages cached to send to parent when we are adopted */
struct cached_msg *mcache_t = NULL;	


/* --------- for now just a single compile; not enough to want to deal with a lib --------- */
#include	"nar_sym.c"		/* symbol table management routines */
#include	"nar_cond.c"		/* condtional set processing */
#include 	"nar_nnsd.c"		/* nnsd related functions and structures */

/* ---------------------------- utility functions -----------------------------------------*/
/*	chop port from the string 
	assumes xx.xx.xx.xx:port   or xx.xx.xx.xx.port
*/
void chop_port( char *buf )
{
	char	*d;			/* last dot (port) */

	if( (d = strrchr( buf, ':')) != NULL  ||  (d = strrchr( buf, '.' )) != NULL )
		*d = 0;

}

/*	figure out an ip address given a host name 
	convert name to a socket addr structure, then build the dotted decimal from that
*/
char *get_ip_str( char *name )
{
	char	*a = NULL;			/* addr string */
	struct	sockaddr_in *sa;

	if( (sa = ng_dot2addr( name )) )		/* do not embed in addr2 call or we have a leak */
	{
		if( (a = ng_addr2dot( sa )) )
		{
			chop_port( a );
			ng_free( sa );
		}
		else
			ng_bleat( 0, "could not convert socket addr to dotted decimal for: %s", name );
	}
	else
		ng_bleat( 0, "could not get socket addr for: %s", name );

	return a;			/* addr2dot returns a buffer that we must free, so no strdup */
}

/*	return 0 if str contains pattern; if pat is NULL assume we have established reg_stuff */
int match( char *str, char *pat )
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
                        ng_bleat( 0, "internal regcomp failure 1! (%s)\n", buf);
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

/*	convert an address such that the last .xxxxx (port) is the narbalek port
	Messages from UDP are likely to have some random port number, but we need
	to send conn messages back to 29055 (or NG_NARBALEK_PORT).
	If the old address is a name, we just add a port number to the name
*/
static char *cvt_addr( char *old )
{
	char buf[1024];
	char *p;

	sfsprintf( buf, sizeof( buf ), "%s", old );
	if( (p = strrchr( buf, ':' )) || (p = strrchr( buf, '.' )) )
		*(p+1) = 0;
	else
		strcat( buf, ":" );

	strcat( buf, port_str );	
	
	return strdup( buf );		/* likely not what they want, but we dont care at this point */
}

static void checkpoint( ) 		/* create checkpoint, in the main file, and in an hourly 'backup' file */
{
	char buf[1024];
	char cbuf[1024];
	ng_timetype now;

	now = ng_now( );

	if( flags & FL_LOADED )				/* a we did get a load from our parent */
	{
		sfsprintf( buf, sizeof( buf ), "%s.h%02d", ckpt_file, (int) ((now % 864000)/36000) );	/* append with current hour */
		if( mk_ckpt( buf,  -1, 1, 1 ) )
		{
			ng_bleat( 2, "checkpoint successful; written to %s", buf );
			sfsprintf( cbuf, sizeof( cbuf ), "cp %s %s 2>/dev/null",  buf, ckpt_file );
			system( cbuf );
		}
		else
			ng_bleat( 0, "checkpoint failed to %s", buf );
	}
}

/*
	send a udp message. if we are using an agent, even in nnsd mode, then we send udp traffic to the 
	agent to forward to the true destination.  this allows us through the firewall without having to 
	open a boatload of ports
*/
static void send_udp( char *addr, char *buf, int len )
{
	struct session *sb;			/* pointer into session list */
	char	mbuf[NG_BUFFER];
	
	if( flags & FL_HAVE_AGENT )		/* we have an agent, it must forward udp messages */
	{
		for( sb = session_list; sb && sb->type != ST_AGENT; sb = sb->next );
		if( sb )
		{
			sfsprintf( mbuf, sizeof( mbuf ), "~FORWARD %s %s", addr, buf );	
			ng_sisendt( sb->sid, mbuf, strlen( mbuf ) );
		}
	}
	else
		ng_sisendu( addr, buf, strlen( buf ) );
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
			if( ! flags & FL_USE_NNSD )
				return;				/* not needed with old mcast protocol */

			/*sfsprintf( buf, sizeof( buf ), "%s:alive:%s\n", udp_id, my_name );	*/
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

		case TG_QUERY:
			sfsprintf( buf, sizeof( buf ), "%s:query:%s\n", udp_id, league );	
			break;

		case TG_BUF:		/* prebuilt buffer */
			sfsprintf( buf, sizeof( buf ), "%s:%s\n", udp_id, ubuf );	
			break;

		default:
			return;
	}

	if( flags & FL_USE_NNSD )
	{
		if( to_nnsd )
			send2nnsd( buf );		/* send just to nnsd daemons */
		else
			send2peers( buf );		/* send to all narbaleks we know about */
	}
	else
	{
		if( flags & FL_NO_MCAST )		/* no multicast avail -- send to parrot as our agent */
		{					/* do NOT use send_udp for this as it does not do multicast */
			for( sb = session_list; sb && sb->type != ST_AGENT; sb = sb->next );
			if( sb )
				ng_sisendt( sb->sid, buf, strlen( buf ) );
		}
		else
			ng_sisendu( mcast_ip, buf, strlen( buf ) );		
	}
}

/* set verbose level for logging. expect: ~v[erbose]<space>value in buffer */
static void set_verbose( char *buf )
{
	char	*p;
	int	newv = 0;

	if( (p = strchr( buf, ' ' )) )
	{
		if( (newv = atoi( ++p )) >= 0 )
			verbose = newv;
		ng_bleat( 0, "verbose level changed, new setting is: %d", verbose );
	}
}

/* add msg to the queue of chached messages that we need to send when we get a parent */
static void cache_msg( char *msg )
{
	struct cached_msg *m;		

	m = (struct cached_msg *) ng_malloc( sizeof( *m ), "cached message" );

	m->next = NULL;
	m->msg = strdup( msg );

	if( mcache_t )				/* add to tail */
		mcache_t->next = m;
	mcache_t = m;
	if( ! mcache_h ) 
		mcache_h = m;
}

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
		ng_bleat( 1, "send cached message: %s", m->msg );
		if( ng_siprintf( sid, "%s\n", m->msg ) != SI_OK )		/* 1 or -1 indicate data queued or error */
			err = 1;					/* we should bail on the session in either case */
		ng_free( m->msg );
		ng_free( m );
	}

	mcache_h = mcache_t = NULL;
	if( count )
		ng_bleat( 1, "sent %d cached messages to %d", count, sid );
}	

/* send a state message to the address, or back to the TCP partner 
	these are assumed to go to alien sessions and not to another narbalek
*/
static void send_state( int sid, char *addr )
{
	char wbuf[2048];

	sfsprintf( wbuf,  sizeof( wbuf ), "STATE: %s %s %s(me) %s(mom) %s(cluster) %s(league) %d(kids) %d(caste) 0x%04x(flags) %s(ver)\n", 
			udp_id, 
			my_name ? my_name : "UNKNOWN", 
			my_ip ? my_ip : "UNKNOWN",  
			flags & FL_ROOT ? "I_AM_ROOT!" : (flags & FL_ADOPTED) ? parent_id : "ORPHANED", 
			cluster, league, children, caste, flags, version  );

	if( addr )
		send_udp( addr, wbuf, strlen( wbuf ) );
		/*ng_sisendu( addr, wbuf, strlen( wbuf ) );*/
	else
		ng_siprintf( sid, "%s", wbuf );

}

/* response to a query -- similar to state response but we assume its another narbalek that is getting the message so 
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

static void add_session( int sid, int type )
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
}


static void drop_session( struct session *sb )
{
	int cb4;		/* children before drop */

	cb4 = children;
	if( sb )
	{
		switch( sb->type )
		{
			case ST_PARENT:			/* parent lost, we must request another adoption */
				flags &= ~FL_ADOPTED;	/* no longer inserted in the tree */
				flags &= ~FL_LOADED;	/* we do not consider ourselves to be loaded if not adopted */
				flags |= FL_DELAY_AR;	/* we must delay the adoption request */
				if( parent_id )
				{
					ng_free( parent_id );
					parent_id = NULL;
				}

				ng_bleat( 1, "parent session droped: %d flags=0x%x", sb->sid, flags );
				if( ! children )	/* if we dont have any kids, re insert at any level */
					caste = 9999;
				break;

			case ST_CHILD:			/* child -- we can dec the child count */
				children--;
				ng_bleat( 1, "child session droped: %d c=%d flags=0x%x", sb->sid, children, flags );
				break;

			case ST_AGENT:			/* our agent is gone; we die too */
				ng_bleat( 0, "agent session has died -- we cannot survive without it" );
				ok2run = 0;
				break;

			default:			/* assume an alien session and we really dont care bout those */
				ng_bleat( 3, "alien session droped: %d", sb->sid );
				break;
		}

		ng_bleat( 3, "session dropped: id=%d type=%d children=%d/%d", sb->sid, sb->type, cb4, children );

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
		ng_siclear( sb->sid );			/* flush (trash) anything queued so session does not enter drain state */
		ng_siclose( sb->sid );			/* close her up */
		drop_session( sb );
		flags &= ~FL_ADOPTED;
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
			ng_siclear( sb->sid );		/* flush (trash) anything queued so session does not enter drain state */
			ng_siclose( sb->sid );		/* close the session */
			drop_session( sb );		/* removes from list and dec the child count */
		}
		
	}
	ng_bleat( 1, "all child sessions dropped c=%d", children );
}

static void close_sessions( )
{
	while( session_list )
	{
		ng_siclear( session_list->sid );		/* flush (trash) anything queued so session does not enter drain state */
		ng_siclose( session_list->sid );		/* close the session */
		drop_session( session_list );			/* remove from the queue */
	}
	ng_bleat( 1, "all sessions closed" );

	sleep( 1 );
}


/* 	open a tcp session with an agent (parrot) who will act on the mcast group 
	for us
*/
static int contact_agent( char *address, int port, char *group )
{
	int sid;
	if( (sid = ng_siconnect( address )) >= 0 )		/* open a tcp session to the agent */
	{
		add_session( sid, ST_AGENT );
		flags |= FL_NO_MCAST;						/* we do not mcast directly when this is set */
		if( group )
			ng_siprintf( sid, "~M %d %s", port, group );		/* put parrot into multi-cast agent mode */
		else
			ng_siprintf( sid, "~GENERALAGENT\n", port, group );	/* put parrot into general udp agent mode */
		return 0;
	}

	ng_bleat( 0, "connection to agent failed: %s, %s", address, strerror( errno ) );
	return 1;
}

/* -----------------  tree oriented routines ----------------------------- */

/* send message to all buddies (children and parent) */
static void propigate( int sid, char *buf )
{
	struct session	*sb;
	struct session 	*nsb = NULL;		/* next session; allows us to disc the current one */
	int	bail = 0;			/* flag to indicate that after dropping a session we cannot continue in this function */

	if( !(flags & FL_ADOPTED) && !(flags & FL_ROOT)  )			/* we dont have a parent and are not root */
		cache_msg( buf );						/* we must cache so when we get adopted we can send on */

	for( sb = session_list; sb; sb = nsb )
	{
		nsb = sb->next;
		if( sb->sid != sid && sb->type <= ST_PROP2 )	/* something we can send to, and msg did not come from them */
		{
			if( ng_siprintf( sb->sid, "%s\n", buf ) != SI_OK || ng_sierrno ==  SI_ERR_QUEUED )	/* error or session blocked */
			{
				if( sb->type == ST_PARENT )			/* if this is our parent, kids will be dropped and thus we bail */
					bail = 1;				/* must return imediately after cleaning out parent session */

				ng_bleat( 0, "propigate: session (%d) started to queue; dropped. type=%d", sb->sid, sb->type ); 
				ng_siclear( sb->sid );				/* clear the queue */
				ng_siclose( sb->sid );
				cbdisc( NULL, sb->sid );			/* emulate a disconnect */

				if( bail )				/* if parent session disc'd then the children went too and nsb likely */
					return;				/* is a bad pointer -- we must exit (bug 1219) */
			}
		}
	}

}

/*	periodic maintenance -- dump the info etc 
	a delayed reload is needed when an ansestor in the tree has dislodged and then 
	reattached. When it reattaches it asks for a reload from its new mamma and 
	thus it is wise for us to ask for one too.  We delay before asking to allow the 
	elder nodes to be reloaded first -- we dont want them to send to us before reloading.
*/
static void maint( )
{
	static ng_timetype nxt_root_bcast = 0;		/* if we are root we announce our rootness every now and again */
	static ng_timetype nxt_load = 0;		/* make a reload request to parent after this time */
	static ng_timetype nxt_ckpt = 0;
	/* nxt_shuffle is now global to support a force shuffle command */
	static ng_timetype load_lease = 0;		/* load pending lease; in case we attach to an old narbalek */
	static ng_timetype nxt_alive = 0;		/* next we send nnsd an alive message */
	static ng_timetype nxt_prune = 0;
	static ng_timetype nxt_list = 0;		/* next time we need to get a peer list from nnsd */

	char	buf[1024];
	char	cbuf[2048];
	int	delay;
	ng_timetype now;
	int 	status;

	now = ng_now( );

	if( (flags & FL_ROOT) && now > nxt_root_bcast )
	{
		nxt_root_bcast = now + ROOT_BCAST_FREQ;
		telegraph( TG_IAMROOT, NULL );			/* prevent isolated trees if flock switch fails */
	}

	if( (flags & FL_USE_NNSD) && now > nxt_alive )
	{
		nxt_alive = now + ALIVE_BCAST_FREQ;
		telegraph( TG_ALIVE, NULL );			
		if( now > nxt_list || !(flags & FL_HAVE_LIST) )		/* time to refresh, or we didn't get one quickly after last request */
		{
			nxt_list = now + REQ_PEER_FREQ;
			request_list( );			/* request a node list from nnsd */
		}
	}

	if( flags & FL_ADOPTED && now > nxt_ckpt )		/* ckpt only if adopted -- prevent dumping an empty file if we find no parent */
	{
		checkpoint( );
		nxt_ckpt = now + CKPT_FREQ;			/* reset even if we did not take one -- give time for a load */
	}

	if( flags & FL_ADOPTED && now > nxt_prune )		/* prune deletes if needed */
	{
		prune( );
		nxt_prune = now + PRUNE_FREQ;			
	}

	if( flags & FL_DELAYED_LOAD )			/* need to process a delayed load */
	{
		if( ! nxt_load )			/* timer was not set */
		{
			delay = (caste) * 20;		/* set our min delay value based on our standing in the food chain - lower longer */
			delay = get_rand( delay, delay + 20 );		/* random tenths of sec between min and min+2 seconds */
			ng_bleat( 1, "delayed load timer set for %d(tenths) @ %d(caste)", delay, caste );
			nxt_load = now + delay;				/* set timer -- we make requst when timer expires */
		}
		else					/* lease was set, is it time to make the request? */
		{
			if( nxt_load < now )		/* request one */
			{	
				/* we may want to clean the table: p_load( "/dev/null"); */
				ng_bleat( 1, "delayed reload timer expired -- v3 style reload  has been requested" );
				flags &= ~FL_DELAYED_LOAD;				/* now safe to turn off the flag */
				flags |= FL_LOAD_PENDING;
				ng_siprintf( get_parent_sid(), "Tloadme:v3\n" );		/* ask for a current view of the world */
			}
		}
	}

	if( now > nxt_shuffle )			/* see if we can reposition ourself higher in the tree -- only if we dont have kids */
	{

		if( caste > 1 && (flags & FL_ADOPTED) && children == 0 &&  nxt_shuffle )	/* @ startup (nxt == 0), delay before shuffling */
		{
			if( flags & FL_QUERY_PENDING )			/* if we previously sent round a query request */
			{
				if( better_parents > 0 || query_responses < 3 )		/* must have a good number of responses to assume nothing better */
				{
					ng_bleat( 2, "attempting to reposition ourself in the tree: caste was %d better=%d resp=%d", caste, better_parents, query_responses );
					drop_parent(  );		/* detach from the parental unit and seek better relatives */
					flags &= ~FL_DELAY_AR;		/* we dont want this one delayed */
					caste = 9999;
				}
				else
					ng_bleat( 2, "not worth trying reposition: better=%d resp=%d", better_parents, query_responses );

				flags &= ~FL_QUERY_PENDING;
				nxt_shuffle = now + get_rand( 18000, 36000 );			/* sometime 30 to 60 minutes (tenths of seconds) from now */
			}
			else
			{
				better_parents = 0;
				query_responses = 0;			/* reset counters before sending query */
				ng_bleat( 2, "sending query to determine if there are better parents: desired caste < %d", caste-1 );
				telegraph( TG_QUERY, NULL );		/* query other narbaleks for stats (caste, #kids etc) */
				flags |= FL_QUERY_PENDING;
				nxt_shuffle = now + 50;			/* 5s to allow for responses */
			}
		}
		else
			nxt_shuffle = now + get_rand( 18000, 36000 ); 	/* not ready/able, set delay and we will try again later */
	}

	if( flags & FL_LOAD_PENDING )
	{
		if( ! load_lease )				/* lease not set, then set and we will deal with it when it pops */
			load_lease = now + MIN_LOAD_LEASE;
		else
		{
			if( now > load_lease )			/* parent hung or running slow or we just missed the message; drop session */
			{
				ng_bleat( 1, "load lease popped; dropping parent session to force reattach" );
				drop_parent( );
				flags &= ~FL_LOAD_PENDING;
				flags &= ~FL_LOADED;			/* get a clean load when we find a new parent */
				load_lease = 0;
			}
		}
	}
	else
		load_lease = 0;			/* if flag  was shut off elsewhere */

	ng_bleat( 9, "maintenance: flags = %02x  league=%s children=%d caste=%d", flags, league, children, caste );
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

	We MUST NOT ever select a caste that is lower than ours.  If we still have a subfamily attached, this 
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
						/* we could sort and make one pass, but chances are good we succeed with first choice, so this is ok */
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
	
		caddr = cvt_addr( p->addr ); 		/* cvt the addr from use-me so that it has the narbalek tcp listen port */

		if( caddr && (sid = ng_siconnect( caddr )) >= 0 )		/* open a tcp session to the parent */
		{
			ng_siprintf( sid, "Ttype:child\n" );		/* officially join the family -- tell mom we are child */
			add_session( sid, ST_PARENT );	

			flags |= FL_ADOPTED;			
			caste = p->caste + 1;				/* we are one lower on the food chain than the parent node */
			ng_bleat( 1, "adopted, parent is %s; our caste is: %d", p->addr, caste );
			if( children )					/* might need to recaste the children */
			{
				ng_bleat( 1, "recasting children" );			/* children should schedule a reload from us */
				sfsprintf( buf, sizeof( buf ), "Tcaste:%d", caste );
				propigate( sid, buf );
			}

			ng_bleat( 1, "clear table by loading from /dev/null" );
			nar_load( "/dev/null" ); 			/* force our table to be completely cleaned */
			parent_id = ng_strdup( p->addr );
			chop_port( parent_id );				/* we only really want the ip and not port */
			send_cached( sid );				/* send them updates we got BEFORE they load us */
			ng_siprintf( sid, "Tloadme:v3\n" );		/* ask for a current view of the world -- ask now, no delay */

			flags |= FL_LOAD_PENDING;   /* wait for end msg or lease timeout -- lease set in maint when it notices pending flag*/

			ng_free( caddr );
			return 1;
		}

		ng_bleat( 0, "adoption, failed %s: %s", caddr, strerror( errno ) );
		p->caste = -1;			/* keeps us from trying this again */
		if( caddr )
			ng_free( caddr );
	}						/* end while */
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
			if we do not have a valid node list  
				send a node list request; set delay
			else (we do have a good list)
				send AR, set lease and return.  we will be recalled to check.

		adoption request has been broadcast
			if lease has expired
				check responses from potential parents and choose the most attractive.
			else
				return -- we will be recalled.



	are first on the group, and that we can become the root node. Should we collide with another process
	thst has claimed the root spot (network outage now brings us together again, etc.) there is logic in 
	the tree command parsing function that will determine which process has rightful ownership of the root
	position; the process which does not will cede its claim to root. 

*/
static void insert( )
{
	static	ng_timetype lease = 0;		/* time after which it is safe to continue in the current state */
	static 	int attempts = 0;			/* if we try more than 5 times, we disconnect from the kids and try at any level */
	static	int long_lease = 1;		/* if we've been in the tree before, we wait less time for the use-me messages */

	ng_timetype now;
	char	buf[1000];

	now = ng_now( );

	if( lease > now )			/* request still pending or a delay before issuing the request - bail now */
		return;

	if( flags & FL_DELAY_AR ) 	/* set an adoption delay, or the lease was set to mark end of an adoption delay */
	{
		if( lease )			/* the lease popped */
		{
			lease = 0;		/* cause us to make our request now */
			flags &= ~FL_DELAY_AR;
			ng_bleat( 1, "adoption request delay cleared" );
		} 
		else
		{
			long_lease = 1;					/* on next round wait longer for use-me requests */
			lease = now + get_rand( 10, 50 );		/* get a delay (tenths) between 1 and 5 seconds */
			ng_bleat( 1, "delaying adoption request %d tenths of seconds", (int) (lease - now) );
			request_list( );				/* request a node list from nnsd while we wait */
			return;
		}
	}
	
	if( lease == 0 )				/* we need to send out the adopt request */
	{
		if( !(flags & FL_USE_NNSD) || flags & FL_HAVE_LIST )		/* we can only do this if we've gotten a list of peers */
		{
			flags &= ~FL_ROOT_KNOWN;	
			telegraph( TG_ADOPT, NULL );		/* send a request for adoption -- if in nnsd mode only if valid list */
		
			drop_nilist( avail_list );		/* should be null, but lets make sure */
			avail_list = NULL;
			ng_bleat( 1, "adoption request broadcast; waiting for responses, lease=%s", long_lease ? "long" : "short" );
			lease = now + (long_lease ? LONG_ADOPT_LEASE : MIN_ADOPT_LEASE);			/* time to wait for use-me responses */
			long_lease = 0;
		}
		else
			ng_bleat( 1, "adoption request delayed pending nnsd list" );

		return;
	}

	lease = 0;
	if( avail_list == NULL )				/* end of lease and no prospects */
	{
		if( ! (flags & (FL_ROOT_KNOWN+FL_NO_ROOT)) )	/* we have not seen an iamroot message, and we can be root */
		{						/* assert ourselves as the grand-poobah (root) */
			ng_bleat( 1, "no prospects after adopt broadcast -- we are taking over ROOT!" );

			telegraph( TG_IAMROOT, NULL );		/* notify others that we are taking over */
			flags |= FL_ADOPTED + FL_ROOT;		/* ok we lie; its easier to pretend that we have parents like the rest of the kids*/
			caste = 0;				/* top of the food chain */

			if( ckpt_file && !(flags & FL_LOADED) )		/* if we were not already loaded */
			{
				int overbose;					/* save old verbose to reset later */
				overbose = verbose;			/* we flip verbose on high for load; nice to have what was loaded in log */
				verbose=3;

				ng_bleat( 1, "verbose set to 3 during checkpoint file load" );
				if( !nar_load( ckpt_file ) )		/* root must get a preload */
					nar_load( init_file );		/* default to initialisation file if ckpt is not there (passed as -i on cmd line)*/
				flags |= FL_LOADED;

				verbose = overbose;
			}

			if( children )			/* certainly need to restation our children's posisition in life */
			{
				ng_bleat( 1, "recasting children" );			/* children should schedule a reload from us */
				sfsprintf( buf, sizeof( buf ), "Tcaste:%d", caste );
				propigate( -1, buf );			/* we dont have a sid that we ignore */
			}
		}
		else
		{
			flags |= FL_DELAY_AR;						/* ensure we delay a bit */
			if( ++attempts > 5 )
			{
				attempts = 0;
				ng_bleat( 1, "still no prospective parents, dropping children and inserting anyplace, using long lease time" );
				/*close_sessions( );*/
				if( children )
				{
					drop_kids( );			/* dont ditch the agent if we are working that way */
					children = 0;
				}
				caste = 9999;
			}
			else
				ng_bleat( 1, "no prospects after adopt broadcast" );		/* we will try again later */
		}

		return;
	}

	if( ! attach2parent() )
	{
		if( ++attempts > 2 )		/* we make two adopt-me requests before we assume we cannot reinsert at our old caste or better */
		{
			ng_bleat( 1, "still no prospects, abandoning hope of keeping our sub-tree; releasing sessions" );
			attempts = 0;
			/*close_sessions( ); */		/* we must ditch our sub family if we dont get in at the same level */
			if( children )
			{
				drop_kids( ) ;
				children = 0;
			}
			caste = 9999;
			long_lease = 1;				/* also give more time for responses since we seem to be having issues */
		}
	}

	drop_nilist( avail_list );		
	avail_list = NULL;
}

/* process tree management command buffer T*  from another narbalek via either raw or cooked interface.
	expected commands are (after T is removed if buf received on cooked session):
		adopt:league:caste		(looking to be adopted by a parent in league <= caste)
		caste:value			(parent has a new caste, adjust ours and trickle to underlings)
		loadme:v3			(request a v3 chkpoint style dump be sent to the session, if parent is not v3 that is ok v2 is accepted)
		type:{child|???}		(type of session)
		use-me:caste:children:node-name	(node will accept us as a child)

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
					if( flags & FL_ROOT )
						telegraph( TG_IAMROOT, NULL );		/* should keep things honest */

					if( resp_timer < now )
						responses = 0;			/* assume they chose another parent */

					if( flags & FL_NO_CHILD )
					{
						ng_bleat( 2, "ignored adoption request: we cannot have children flags=0x%x", flags );
						break;
					}
					if( (!(flags & FL_ADOPTED)) || (!(flags & FL_LOADED)) )
					{
						ng_bleat( 1, "ignored adoption request: we are not adopted, or loaded flags=0x%x", flags );
						break;
					}

					if( (responses + children < max_kids)  )	/* responding should not put us over max */
					{
						rcaste = atoi( tlist[2] );			/* they need to be inserted at or above this caste */
						if( caste <= rcaste  && children < max_kids )	/* high enough in the food chain, and not at max kids */
						{
							/* send with @node-name: when  going via agent; tcp must come to us not agent node 
								this is not back compat with v2, but there should not be enough agented 
								narbaleks to care.
							*/
							if( flags & FL_HAVE_AGENT )
								send_udp( addr, wbuf, sfsprintf( wbuf, sizeof( wbuf ), "@%s:%s:use-me:%d:%d\n", my_name, udp_id, caste, children ) );
							else
								send_udp( addr, wbuf, sfsprintf( wbuf, sizeof( wbuf ), "%s:use-me:%d:%d\n", udp_id, caste, children ) );
							ng_bleat( 1, "responded to adoption request from %s: %s", addr, wbuf );
							responses++;
							resp_timer = now + 50;			/* clear response counter in 5 sec */
						}
						else
							ng_bleat( 2, "ignored adoption request: %s rcaste=%d/%s caste=%d  kids=%d", addr, rcaste, tlist[2], caste, children );
					}
					else
						ng_bleat( 2, "ignored adoption request: responses(%d) + kids(%d) > max", responses, children );
				}
				else
					ng_bleat( 2, "ignored adoption request: wrong league: %s", tlist[1] );
			}
			else
				ng_bleat( 0, "bad adopt command from %s: %s", addr ? addr : "address missing", buf );
				
			break;
	
		case 't':			/* type of session - should only be children checking in */
			if( tcount > 1 && sb )
			{
				if( *(tlist[1]) == 'c' )
				{
					sb->type = ST_CHILD;		/* a child picked us, we are now mommy and daddy */
					children++;			/* for tax purposes */
					ng_bleat( 1, "adoption confirmed on sid: %d  c=%d", sb->sid, children );
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
			if( tcount > 1  && (rcaste = atoi( tlist[1] ) + 1) > 0 ) 	/* our new caste */
			{
				ng_bleat( 1, "adjusting caste value to: %d", rcaste );
				caste = rcaste;
				sfsprintf( wbuf, sizeof( wbuf ), "caste:%d\n", caste );
				propigate( sb->sid, wbuf );			/* children need to recalculate their standing */
				flags |= FL_DELAYED_LOAD;			/* cause maint to schedule a load in a few seconds */
			}
			break;

		case 'e':			/* end of load -- will not come from older narbaleks */
			ng_bleat( 1, "end of load received; before reset flags=0x%x", flags );
			flags &= ~FL_LOAD_PENDING;
			flags |= FL_LOADED;
			break;

		case 'i':			/* another node has declared that they are root -- iamroot:league:node-name:udp-id */
			if( tcount >= 4 && strcmp( tlist[1], league ) == 0 )		/* enough parms, and for our league */
			{
				ng_bleat( flags & FL_ADOPTED ? 2 : 1, "iamroot message received: %s", buf );
				if( flags & FL_ROOT )				/* trouble in river city if we also think we are root */
				{
					if( strcmp( udp_id, tlist[3] ) < 0 )		/* we win! */
					{
						ng_bleat( 1, "root collision (we win) with: %s %s %s", tlist[1], tlist[2], tlist[3] );
						telegraph( TG_IAMROOT, NULL );			/* they should cease */
					}
					else
					{
						ng_bleat( 1, "root collision (we cede) with: %s %s %s", tlist[1], tlist[2], tlist[3] );
						flags &= ~FL_ROOT;
						flags &= ~FL_ADOPTED;
						flags |= FL_DELAY_AR;
						flags &= ~FL_LOADED;			/* get a clean load when we find a new parent */
						if( children ) 
							close_sessions( );		/* drop all of our sessions */
						merge_data( tlist[2] );				/* if allowed, send our current data to new tree */
						caste = 9999;				/* reset caste as we will rejoin */
					}
				}

				flags |= FL_ROOT_KNOWN;		/* we know that the root exists */
			}
			else
				ng_bleat( 4, "ignored iamroot message received: %s", buf );
			break;

		case 'l':			/* loadme: (if neighbour is v2) or loadme:v3 */
			if( sb )
			{
				ng_bleat( 2, "loadme %s request received", tcount > 1 ? tlist[1] : "v2" );
				if( tcount > 1  && (strcmp( tlist[1], "v3" ) == 0) )		/* version 3 style load for our child */
					mk_ckpt( NULL,  sb->sid, 1, 1 );		/* send to mate in v3 chkpt format, with deletes */
				else
					mk_ckpt( NULL,  sb->sid, 0, 0 );		/* send in old format, without delets */
			}
			break;

		case 'L':						/* list from nnsd */
			save_list( tlist[1] );
			break;

		case 'q':						/* query stats request */
			if( tcount > 1 && strcmp( tlist[1], league ) == 0 )		/* respond only if in our league */
			{
				send_qresponse( sb ? sb->sid : -1, from );
			}
			break;

		case 's':						/* stats -- response to query */
			/* expected tokens:  stats nodename ip cluster league chldren caste */
			if( strcmp( tlist[0], "stats" ) == 0 )
			{
				if( tcount > 5 )				/* should respond only if our league matched therirs */
				{
					int c;
					int k;
	
					query_responses++;	
					c = atoi( tlist[6] );		/* caste */
					k = atoi( tlist[5] );		/* current kid count */
					if( c < caste-1 && k < max_kids )		/*  node has room for a kid and is higher in the tree than our parent */
					{
						better_parents++;			/* so add to the count */
						ng_bleat( 2, "better parent: %s caste=%s kids=%s", tlist[0], tlist[6], tlist[5] );
					}
					else
						ng_bleat( 3, "unsuitable parent: %s caste=%s kids=%s", tlist[0], tlist[6], tlist[5] );
				}
			}
			else
			if( strcmp( tlist[0], "shuffle" ) == 0 )
			{
				ng_bleat( 1, "force shuffle request received" );
				nxt_shuffle = 1;			/* if it is 0 shuffle does not trigger -- startup safeguard against immediate shuffle*/
			}
			break;

		case 'u':			/* use me as a parent -- use-me:caste:children:node-name  */
			if( tcount >= 3 && addr )
			{
				n = ng_malloc( sizeof( struct node_info ), "do_tree_cmd: use me" );
				n->addr = ng_strdup( addr );
				n->caste = atoi( tlist[1] );
				n->kids  = atoi( tlist[2] );
				n->next = avail_list;			/* push on avail list -- we select our parent when adopt lease pops */
				avail_list = n;
				ng_bleat( 1, "added potential parent: %s %s(caste) %s(kids)", addr, tlist[1], tlist[2] );
			}
			else
				ng_bleat( 0, "bad use-me command from %s: %s", addr ? addr : "address missing", buf );
			break;

		case 'D':				/* broadcast state */
			if( addr )
				send_state( -1, addr );
			break;

		default:
			ng_bleat( 1, "unrecognised tree command received: %s", buf );
			break;
	}
}

/* ------------------- data maintenance functions --------------------------- */
/* invoke nar_sym routines to actually get real work done, but need basic setup first 
	we expect the command to be dump:name=value:pattern  
	where 
		name=value allows something like NG_ROOT to be supplied by the caller.  
			occurances of $name will be substituted with value as the table is dumped. 
			However, with the multied layered pinkpages names this is really useless, but 
			still supported. We really expect :: which will turn off expansion.

		pattern is a variable pattern (regex) to dump.  (e.g. pinkpages/flyingfox)
*/
static void dump_table( struct session *sb, char *buf )
{
	char	*n;			/* pointer at variable name */
	char	*v = NULL;		/* pointer at the value */
	char	*p = NULL; 		/* pointar at optional pattern */
	char	*old_root = NULL;	/* old root value if user sent in override */

	if( (n = strchr( buf, ':' )) )		
	{
		if( p = strchr( n+1, ':' ) )		/* end of replace value, beginning of pattern */
			*(p++) = 0;			/* make value a string */

		if( *(n+1) )				/* if :something: supplied */
		{
			if( (v = strchr( buf, '=' )) )			/* :name=value: supplied */
			{
				*v = 0;						/* make name a string */
				v++;						/* point at value */
				n++;
			}
			else				/* if just :value: supplied we assume  an implied :NG_ROOT=value: */
			{
				v = n+1;					
				n = "NG_ROOT";				
			}
	
			if( (old_root = lookup_value( n )) )		/* get its current self */
				old_root = ng_strdup( old_root );		/* lookup value is read only */

			add_value( time( NULL),  n, v );		/* during dump, name will take on the user supplied value */
			ng_bleat( 2, "%s set to %s during dump", n, v );
		}
	}

	send_table( sb->sid, p, v ? 1 : 0 );				/* if v is null, then no expansion */
	ng_sisendt( sb->sid, DEF_END_VAR, strlen( DEF_END_VAR ) );	/* send end of table marker indicating all of table received */

	if( v )					/* value was saved, we need to put it back the way it was, or delete it  */
	{
		add_value( time( NULL ), n, old_root );		
		ng_bleat( 3, "after dump NG_ROOT reset to: %s", old_root  ? old_root : "deleted" );
	}
	if( old_root )
		ng_free( old_root );

	ng_bleat( 3, "table dumped to: %d", sb->sid );
}

/* lookup a variable and return its value 
	assume buffer is:  <whitespace>varname[<ws>|<ws>name...][\n]
	returns the first value found
*/
static void get_var( struct session *sb, char *buf, int do_expand )
{

	char *val;
	char *ep;
	char *oeb;
	char *p;				/* pointer at beginning of var name token */
	char *e;				/* pointer at last non-blank in token */
	int depth = 0;
	char wrk[NG_BUFFER];
	int tcount = 0;
	char **tlist = NULL;
	int i = 0;

	tlist = ng_tokenise( buf, '|', '\\', NULL, &tcount );		/* do NOT use a static tlist -- we are reentrant */
	while( tcount-- )
	{
		for( p = tlist[i++]; (*p == ' ' || *p =='\t'); p++ );
		if( (e = strchr( p, ' ' )) )
			*e = 0;				/* truncate trailing whitespace */
		else
		{
			if( (e = strchr( p, '\n' )) )
				*e = 0;				/* cannot deal with \n either */
		}

		if( (val = lookup_value( p )) != NULL )
		{
			if( do_expand )
			{
				ep = expand( val );
				depth = 0;
				while( depth++ < 10 && strchr( ep, '$' ) )
				{
					oeb = ep;
					ep = expand( ep );
					ng_free( oeb );
				}
	
				if( *ep == '@' )				/* indirect reference */
					get_var( sb, ep+1, do_expand );		/* recurse to dig out indirection */
				else
				{
					ng_bleat( 3, "get_var: expanded look up: %s = %s", p, ep );
					ng_siprintf( sb->sid, "%s\n", ep );		/* must be new line terminated */
				}

				ng_free( ep );
				ng_free( tlist );
				return;
			}
			else
			{
				ng_bleat( 3, "get_var: unexpanded look up: %s = %s", p, val );
				ng_siprintf( sb->sid, "%s\n", val );		/* send back unexpanded */

				ng_free( tlist );
				return;
			}
		}
		else
			ng_bleat( 2, "get_var: look up: %s = (undefined)", p );
	}

	ng_free( tlist );
}

/* 
	add a timestamp (unix time NOT ningaui time because we want zulu time 
	in order to not play games with timezone conversion)
	we expect rec to be:
		token<whitespace>data
	We snarf just the first character of token and append @timestamp to generate:
		T@timestamp<blank>data

	Using this format we are back compatable with v1 and v2 narbaleks should they
	ever meet.  
*/
char *add_ts( char *rec )
{
	char	wrk[NG_BUFFER];
	char	*new;
	time_t 	ts;
	char	*data;			/* pointer at start of the data in rec */

	ts = time( NULL );
	if( (data = strpbrk( rec, " \t" )) == NULL )
		return ng_strdup( rec );			/* badly formed record, just spit it back */
	
	while( isspace( *++data ) );				/* skip to next non-blank */
	
	sfsprintf( wrk, sizeof(wrk), "%c@%I*d %s", *rec, Ii(ts), data );

	return ng_strdup( wrk );
}

/* assume buffer contains <cmd-token><whitespace>name=value 
	type is a T_ constant (comment or value indicator) 
	if the second character of the command token is an @, then
	we assume the rest of the command token is an ASCII timestamp.
*/
static void set_val( char *buf, int type, int logit )
{
	char	*p;
	char	*log_type = NULL;				/* type of message to the log */
	char	name[NG_BUFFER];
	char	val[NG_BUFFER];
	int	depth;
	time_t 	ts = 0;

	if( *(buf+1) == '@' )			/* message has timestamp */
		ts = strtoll( buf+2, NULL, 10 );

	if( (p = strchr( buf, ' ' )) )			/* skip the cmd token */
	{
		depth = strlen( p ) - 1;
		if( *(p+depth) == '\n' )		/* we really want a string from here */
			*(p+depth) = 0;

		getnv( p, name, val  );
		if( *name )
		{
			switch( type )
			{
				case T_VAL:
					add_value( ts, name, val );		/* right now we ignore return; we may use it to prevent propigation */
					log_type = "value";
					break;
				
				case T_COMMENT:
					add_comment( ts, name, val );
					log_type = "comment";
					break;
				
				default:
					break;
			}

			if( logit && log_type )			/* actually added something */
				ng_log( LOG_INFO, "%s %s", log_type, p);
		}
	}	

}



/* -------------------- callback routines ---------------------------*/
int cbsig( char *data, int sig )
{

	return SI_RET_OK;
}

int cbconn( char *data, int f, char *buf )
{
	add_session( f, ST_ALIEN );		/* assume alien until they tell us otherwise */
 	return( SI_RET_OK );
}

int cbdisc( char *data, int f )
{
	struct session *sb;

	ng_bleat( 3, "session disconnect: %d", f );
	for( sb = session_list; sb && sb->sid != f; sb = sb->next );
	if( sb )
	{
		drop_session( sb );
		if( ! (flags & FL_ADOPTED) )		/* if we are no longer adopted, then we drop kids too */
		{
			if( children )
			{
				drop_kids( );
				children = 0;
			}
		}
	}
	else
		ng_bleat( 0, "disc received, but no session: id=%d", f );
 
	return SI_RET_OK;
}


/*	udp data -- multicast data  (tree oriented commands)
	we must be aware that we will most likely get messages from ourself that 
	are sent to the multicast group. we need to filter them out. to do this we 
	prefix all udp messages with an id that we believe to be unique and 
	filter off  messages with our id.
	
*/
int cbraw( void *data, char *buf, int len, char *from, int fd )
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
			ng_bleat( 6, "udp buffer ignored: %s", buf );
			return SI_RET_OK;
		}
	}

	if( p = strchr( buf, ':'  ) )
	{
		ng_bleat( 6, "udp buffer from %s processed: %s", from, buf );
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
				my_ip = strdup( from );
				ng_bleat( 1, "we believe our ip to be: %s", my_ip );
				return SI_RET_OK;
			}
		}
	}
	
	ng_bleat( 6, "udp buffer ignored: %s", buf );
	return SI_RET_OK;
}

/* tcp data  (we support the old version leading ~ as a command indicator; we ignore it)
	commands:
		C[omment]	(from another narbalek -- percolation)
		c[omment]	(from the outside world -- log and then percolate)
		e[xamine]	(drop some details to the log)
		S[et]		(same as comments)
		s[et]
		d[ump]		(dump the table expansion depends on parms)
		D[ump]		(dump some information about us)
		g[et]		(get a value with expansion of $name)
		G[et]		(get a value without expansion)
		l[og]		(ok to start logging -- nglogd started)
		T[ree]		(tree maintenance (sessions with other narbalek) command
		%		(comment from a load-- no percolation, no log and assumed no timestamp)
		@		(name value from a load-- no percolation, no log and assumed no timestamp)
		v[erbose]	(set our verbosity to n)
		xit		(cause us to die gracefully)
*/
int cbcook( void *data, int fd, char *buf, int len )
{
	struct session *sb;
	char	*rec;			/* pointer at next record to process */
	char	*p;			/* generic pointer at a portion of the record */
	char 	*name = NULL;
	char	*cond = NULL;		/* pointer to conditional phrase in set statemen */
	int	rcount = 0;		/* records in the buffer */
	char	wbuf[NG_BUFFER];

	ng_bleat( 6, "cook: %d bytes = (%.90s)%s", len, buf, len > 90 ? "<trunc>" : "" );

	for( sb = session_list; sb && sb->sid != fd; sb = sb->next );
	if( ! sb )
		return SI_RET_OK;


	if( sb->flow == NULL )
		sb->flow = ng_flow_open( NG_BUFFER );		/* establish a flow input buffer for the session */
	           
	ng_flow_ref( sb->flow, buf, len );              	/* regisger new buffer with the flow manager */

	if( sb->type == ST_AGENT )				/* must treat as if it is a UDP message from mcast */
	{
		ng_bleat( 3, "cbcook: agent msg: (%s) len= %d", buf, len );
		while( rec = ng_flow_get( sb->flow, '\n' ) )		/* get all complete (new line terminated) records */
		{
			ng_bleat( 4, "cbcook: agent msg split: (%s)", rec );
			if( (p = strchr( rec, ':' )) )				/* assume address:message */
			{
				*p = 0;
				cbraw( data, p+1, strlen( p+1 )+1, rec, fd );		/* simulate the receipt of a udp message */
			}
		}

		return SI_RET_OK;
	}

                        
	/* three ways we get name=value update messages headed with: 
		'c' | 's' are from external process (e.g. nar_set): we must send back end of data, then propigate the msg to all non-aliens 
		'C' | 'S' are from another narbalek: we must propigate to all non-aliens
		'@' | '%' are from another narbalek that is the result of a load me request -- we dont propigate these messages 
	
		we assume that the command token (usually one letter) is separated from the name=value by at least one space/tab
	*/
	while( rec = ng_flow_get( sb->flow, '\n' ) )		/* get all complete (new line terminated) records */
	{
		rcount++;
		if( *rec == '~' )			/* left over from version 1 external interface stuff -- we ignore */
			rec++;

		ng_bleat( 6, "received: %s", rec );
		switch( *rec )
		{
			case '@':				/* stuff from a load me command - add but do not propigate */
			case '%':
				if( nar_load_rec( time( NULL ), rec ) && flags & FL_ROOT )  	/* if we replaced value and are root */
				{								/* it is merged data and we propigate */
					ng_bleat( 1, "proigate merge data: %s", rec );
					*rec = 'S';				/* change to S to  force this down the chain as an unlogged set */
					propigate( fd, rec );
				}
				break;
	
			case 'a':			/* set cluster affiliation */
				if( (p = strchr( rec, ' ' )) )
				{
					int vlevel = 3;			/* bleat at 1 only if value changes */

					if( cluster )
					{
						if( strcmp( cluster, p+1 ) )
							vlevel =1;
						ng_free( cluster );
					}
					else
						vlevel = 1;

					cluster = strdup( p+1 );
					ng_bleat( vlevel, "cluster affiliation changed to: %s", cluster );
				}
				else
					ng_bleat( 1, "cluster affiliation change failed: %s", rec );

				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;

			case 'C':			/* comment from a narbalek mate -- must propigate but dont send end of data */
				set_val( rec, T_COMMENT, DONT_LOG );		/* add, but dont log (assume timestamp added by sender) */
				propigate( fd, rec );				/* send to parent/child that did not send to us */
				break;
	
			case 'c':						/* set comment  (from local process) and propigate */
				*rec = 'C';					/* prevents neighbour from sending end marker back to us */
				rec = add_ts( rec );				/* we add a timestamp when we see the message */
				set_val( rec, T_COMMENT, flags & FL_OK2LOG ? LOG_IT : DONT_LOG );		/* add to our table and log */
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				propigate( fd, rec );				/* send info to our parent and children */
				ng_free( rec );
				break;

			case 'd':		/* dump table -- dump[:[name=]value[:pattern]]  expanding name to value. if name omitted NG_ROOT assumed */
				if( flags & FL_LOADED )		/* only if we have a load -- else the caller should recognise that the end */
					dump_table( sb, rec );	/* marker did not come down and they should do the right thing */
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
	
			case 'D':				/* dump info about our state */
				send_state( sb->sid, NULL );
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;

			case 'e':
				if( strncmp( rec, "examine", 7 ) == 0 )
					examine( );
				break;
	
	
			case 'G':						/* get value and send it back on the session */
				if( flags & FL_LOADED )
					get_var( sb, strchr( rec, ' ' ), NO_EXPANSION );	/* point past g[et] */
				else
				{
					ng_sisendt( sb->sid, ERROR_RESULT, sizeof( ERROR_RESULT ) -1 );
					ng_bleat( 1, "table not stable, unable to complete request: %s", rec );
				}

				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
	
			case 'g':					/* get value and send it back on the session */
				if( flags & FL_LOADED )
					get_var( sb, strchr( rec, ' ' ), DO_EXPANSION );
				else
				{
					ng_sisendt( sb->sid, ERROR_RESULT, sizeof( ERROR_RESULT ) -1 );
					ng_bleat( 1, "table not stable, unable to complete request: %s", rec );
				}

				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
	
			case 'i':		/* if -- conditionally set the value.  expect : i<name>:<condition>:=<value> */
				if( (p = strchr( rec, COND_SEP_CH )) != NULL )
				{
					name = rec + 1;
					*p = 0;			/* terminate name */
					cond = p+1;
					if( (p = strchr( cond, COND_SEP_CH )) != NULL )
					{
						p++;					/* p now @ value */

						if( test_value( name, cond ) )		/* passes the condition */
						{					/* build a dummy record and update it */
							sfsprintf( wbuf, sizeof( wbuf ), "S@%I*d %s%s", sizeof( time_t ), time( NULL ), name, p );			/* fix 06/08/2009 */
							set_val( wbuf, T_VAL, flags & FL_OK2LOG ? LOG_IT : DONT_LOG );
							propigate( fd, wbuf );
							ng_bleat( 2, "condition met: %s %s (%s)", name, cond, wbuf );
						}
						else
							ng_bleat( 2, "condition failed: %s %s", name, cond );
						
					}
					else
						ng_bleat( 0, "invalid conditional expression in record: %s", cond );
				}
				else
					ng_bleat( 0, "invalid name:condition:=value record received: %s", rec );
						ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;

			case 'l':					/* ok to log var sets */
				flags |= FL_OK2LOG;
				ng_bleat( 0, "logging turned on" );
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;

			case 'S':					/* set variable (from mate)  and propigate -- dont send end of data back */
				set_val( rec, T_VAL, DONT_LOG );	/* add -- dont log (assume timestamp added by sender) */
				propigate( fd, rec );			/* send info to our parent and children */
				break;
	
			case '!':			/* testing -- same as 'S', but sleep after adding the timestamp so we can try to force it to be old */
				*rec = 'S';					/* keep from them sending us end of transmission */
				rec = add_ts( rec );				/* we add a timestamp when we see the message */
				ng_bleat( 3, "got a ! record; sleeping: %s", rec );
				sleep( 5 );
				set_val( rec, T_VAL, LOG_IT );			/* add and log */
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				propigate( fd, rec );				/* send info to parent and children */
				ng_free( rec );
				break;

			case 's':						/* set variable (from local process)  and propigate */
				*rec = 'S';					/* keep from them sending us end of transmission */
				rec = add_ts( rec );				/* we add a timestamp when we see the message */
				set_val( rec, T_VAL, flags & FL_OK2LOG ? LOG_IT : DONT_LOG );			/* add and log */
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				propigate( fd, rec );				/* send info to parent and children */
				ng_free( rec );
				break;
	
			case 'T':				/* tree management command: sesison type, caste, use-me etc */
				do_tree_cmd( sb, NULL, rec+1 );
								/* tree cmds are from other narbaleks -- do not send end of transmission */
				break;
	
			case 'v':
				set_verbose( rec );
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
	
			case 'x':
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				if( strncmp( rec, "xit4360", 8 ) == 0 )
				{
					ng_bleat( 0, "valid exit command received" );
					ok2run = 0;
					return SI_RET_QUIT;
				}
				break;

			case 'X':
				ng_sisendt( sb->sid, ERROR_RESULT, sizeof( ERROR_RESULT ) -1 );
				ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
	
			default:
				ng_bleat( 0, "unrecognised command string from sid=%d: %s", sb->sid, rec );
				if( sb && sb->type == ST_ALIEN )
					ng_sisendt( sb->sid, DEF_END_MARK, sizeof( DEF_END_MARK ) - 1 );
				break;
		}
	}				/* while */

	ng_bleat( 3, "cook: buffer contained %d records", rcount );
	return SI_RET_OK;
}

static void usage( )
{
	sfprintf( sfstderr, "narbalek %s\n", version );
	sfprintf( sfstderr, "usage: %s [-A] [-a mcast-agent] [-c clustername] [-d nnsd-port] [-C ckpt-file] [-g mcast-group] [-i file] [-l league]  [-P] [-w dir] [-v]  -p port\n", argv0 );
	exit( 1 );
}


int main( int argc, char **argv )
{
	extern	int errno;
	extern	int ng_sierrno;

	char	*mcast_agent = NULL;		/* use this address to contact the multicast agent */
	int	status = 0;
	int	i;
	int	port = 0;
	char	buf[4096];			/* work buffer and work pointer */
	char	*p;	
	char	*wdir_path = NULL;
	int	use_full_domain = 0;		/* -f sets to cause full domain name to be sent to nnsd */

	signal( SIGPIPE, SIG_IGN );		/* must ignore or it kills us */

	ARGBEGIN
	{
		case 'a':	SARG( mcast_agent ); break;
		case 'A':	flags |= FL_NO_ROOT + FL_NO_CHILD; break;		/* we are not allowed to have any kids */
		case 'c':	SARG( cluster ); break;			/* we should not rely on the cartulary */
		case 'C':	SARG( ckpt_file ); break;
		case 'd':	SARG( nnsd_port_str ); break;		/* use nnsd for look up rather than mcast */
		case 'f':	use_full_domain = 1; break;
		case 'g':	SARG( mcast_group ); break;
		case 'i':	SARG( init_file ); break;		/* initialisation file if ckpt file not there */
		case 'l':	SARG( league ); break;			/* the group of narbaleks -- used for adoption/root msg */
		case 'm':	flags |= FL_ALLOW_MERGE; break;		/* if two trees have split, allow merging of data when one root cedes */
		case 'p':	SARG( port_str ); break;
		case 'P':	flags |= FL_NO_ROOT; break;		/* we can never be the root node -- satellites I suspect */
		case 't':	IARG( mcast_ttl ); break;
		case 'w':	SARG( wdir_path ); break;		/* home base - default is NG_ROOT/site/narbalek */
		case 'v':	OPT_INC( verbose ); break;

usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	if( ! wdir_path )
		wdir_path = ng_rootnm( "site/narbalek" );

	if( chdir( wdir_path ) != 0 )
	{
		ng_bleat( 0, "unable to switch to working directory: %s: %s", wdir_path, strerror( errno ) );
		exit( 1 );
	}

	ng_bleat( 1, "working directory is: %s", wdir_path );
	ng_free( wdir_path );
	wdir_path = NULL;

	ng_sysname( buf, sizeof( buf ) );
	if( (!use_full_domain) && (p = strchr( buf, '.' )) )	/* shorten to just node name if -f not turned on */
		*p = 0;
	my_name = strdup( buf );

	my_ip = get_ip_str( my_name );			/* we could get a NULL back and that is ok */

	srand48( getpid() * ng_now( ) );		/* just incase two nodes managed to come up *right* at the same instant ;) */

	sfsprintf( buf, sizeof( buf ), "%I*x%05x%02x", sizeof( ng_timetype),  ng_now( ), getpid( ), get_rand( 0, 255 ) );  /* our id - date/pid/random */
	udp_id = ng_strdup( buf );
	ng_bleat( 0, "narbalek v%s started, listening on port=%s group=%s id=%s", version, port_str, mcast_group, udp_id );

	if( !cluster )
		cluster = strdup( "unknown" );		/* we assume that site-prep or something will inform us of this later */

	if( !ckpt_file )
	{
		sfsprintf( buf, sizeof( buf ), "narbalek.%s", league );
		ckpt_file = ng_strdup( buf );
	}


	if( !port_str )			
	{
		ng_bleat( 0, "abort:  -p port-number not supplied on command line" );
		exit( 1 );
	}

	ng_bleat( 1, "will save/restore data using: ./%s", ckpt_file );
	port = atoi( port_str );

	if( nnsd_port_str )						/* we will be using nnsd rather than mcast */
	{
		ng_bleat( 2, "using nnsd processes listening on port: %s", nnsd_port_str );
		flags |= FL_USE_NNSD;
	}
	else
	{
		sfsprintf( buf, sizeof( buf ), "%s:%d", mcast_group, port );		/* build the full address:port for udp sends */
		mcast_ip = strdup( buf );
	}
		

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

	ng_bleat( 1, "listening on port: %s", port_str );

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    		/* register callbacks */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    
	ng_sicbreg( SI_CB_CDATA, &cbcook, NULL );
	ng_sicbreg( SI_CB_RDATA, &cbraw,  NULL );
	ng_sicbreg( SI_CB_SIGNAL, &cbsig, NULL );

	if( mcast_agent )		/* must create a direct tcp/ip session to an agent that will forward our mcast and/or udp traffic */
	{
		int msg_after = 0;
		int total_tries = 500;

		ng_bleat( 1, "using an agent to forward multicast and udp messages: %s", mcast_agent );
		while( (status = contact_agent( mcast_agent, port, flags & FL_USE_NNSD ? NULL : mcast_group )) > 0 )
		{
			if( msg_after <= 0 )
			{
				ng_bleat( 0, "unable to set up connection to agent (%s), retrying every 5s", mcast_agent );
				msg_after = 10;
			}

			msg_after--;
			if( total_tries-- <= 0 )
			{
				ng_bleat( 0, "abort: unable to set up connection to agent (%s), giving up.", mcast_agent );
				exit( 1 );
			}
			sleep( 5 );
		}

		ng_bleat( 1, "agent %s", status > 0 ? "not connected" : "connected" );
		flags |= FL_NO_ROOT | FL_HAVE_AGENT;				/* when using an agent we cannot be the root! */
	}

	if( !(flags & FL_USE_NNSD) && !(flags & FL_HAVE_AGENT) )	/* mcast, and no agent doing our mcasting, then join the group */
	{
		status = ng_simcast_join( mcast_group, -1, mcast_ttl );		/* join the narbalek mcast group, on any interface */

		if( status )
		{
			ng_bleat( 0, "abort: unable to join multicast group, or contact agent: %s", strerror( errno ) );
			exit( 4 );
		}

		ng_bleat( 1, "%s: %s ttl=%d status=%d errno=%d", mcast_agent ? "mcast via agent" : "joined mcast", mcast_ip, mcast_ttl, status, errno );
	}


	ng_bleat( 1, "initialise table as empty from /dev/null" );
	nar_load( "/dev/null" );			/* initialise symtab as empty table -- we load from parent, or chkpt file later */

	flags |= FL_DELAY_AR;			/* delay adoption request in case multi nodes powered up together */
	while( ok2run )
	{
		if( ! (flags & FL_ADOPTED) )	
			insert( );					/* insert us into the tree */

		maint( );					/* periodic maintenance (insert, ckpt etc) */

		errno = 0;
				/* poll for tcp/udp events (drive callbacks if data); short delay if we are delaying an adoption request; max 2s if not */
		if( (status = ng_sipoll( (flags & FL_DELAY_AR) ? 20 : 200 )) < 0 )	
		{
			if( errno != EINTR && errno != EAGAIN )
			{
				ng_bleat( 0, "sipoll returned fatal error: %d %s\n", errno, strerror( errno ) );
				ok2run = 0;
			}
		}
	}

	close_sessions( );
	ng_bleat( 1, "shutdown in progress, creating one last checkpoint " );
	checkpoint( );		/* save last data */

	ng_bleat( 1, "done. status=%d sierr =%d", status, ng_sierrno );
	return 0;
}             


#ifdef KEEP
/*
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_narbalek:Cluster Table Manager Daemon)

&space
&synop	ng_narbalek  [-C ckpt-file] [-c clustername] [-d nnsd-port | -g mcast-group] [-f]
&break	[-i file] [-l league] [-m] [-P] [-v] [-t ttl-count] [-w work-dir] -p port 

&space
&desc	
	&This maintains an in-core information map which consists of name value pairs, 
	with an optional comment string associated with each pair.
	Information in the map is shared between all active instances of &this which 
	are using the same &ital(port) and multi-cast address combination. 
	&This processes organise themselves into a communications tree and forward
	map changes to each neighbour allowing an up to date map to be accessable 
	from each instance. The communications tree is completely self organising
	and requires no outside defination or maintenance. 
&space
	Commands can be sent to &this from exteranal (non-narbalek) processes
	allowing name/value pairs to be manipulated, and to dump all or a portion 
	of the information map to the requestor.  TCP/IP sessions are used between 
	&this processes, and by external processes needing to update and/or query
	the map data. 
&space
	The information contained in the map may be extracted using one of three 
	mechanisms:
&space
&beglist
&item	individual keyword lookup
&item	pattern lookup
&item	complete dump of the map
&endlist
	
&space
	
	When a request to fetch the value associated with an individual keyword is received, 
	the value is written back to the requestor on the TCP/IP session. 
	Responses to both a pattern lookup and a map dump result in newline terminated
	records which have the following format:

&space
&litblkb
   name="value"  # comment text for name
&litblke
&space
&subcat Value Expansion
	As a default, the values returned to the requestor are unaltered unless 
	value expansion is indicated on the request.  One or two forms of variable
	expansion are applied to the value depending on the command that was sent.
	(Command descriptions later in this document describe the mechanism for 
	enabling expansion.)
&space
	For &lit(get) (individual lookup) and &lit(dump) requests, 
	all occurances of &lit($name) in the value are expanded using simple shell 
	style substitiution. The assumption is that &lit($name) is also defined 
	in the information map.  If the &lit($name) is not defined in the current 
	map, then it is not replaced in the value and left as &lit($name.)
&space
&subcat Value Indirection
	For &lit(get) commands only , after &lit($name) expansion is applied,  the 
	value is examined for a leading at sign (@). If present, this is assumed to 
	indicate indirection and the remainder of the value is taken to be a 
	variable name within the information map.  The name is searched for and its 
	value is returned.  
&space
	It may seem silly to support the &lit(@name) redirection given that &lit($name) 
	expansion is applied to values. However, &lit($name) expansion assumes that &ital(name)
	is a standard shell variable name.  &This does not place any restrictions on 
	variable names and as sucn the at-sign indirection allows for something like
	&lit(foo=@pinkpages/flock:bar) to be defined and to expand correctly.  If
	&lit(foo=$pinkpages/flock:bar) were defined, &this would attempt to expand 
	&lit($pinkpages) rather than the whole string which would result in the generation
	of the wrong answer. 

&space
&subcat Interface Scripts
	The &lit(ng_nar_get) and &lit(ng_nar_set) scripts should be used as the 
	most primitave interface to &this for communicating with &this.  
	Applications such as &ital(pinkpages) will provide higher level interfaces 
	to the &this data map through scripts like &lit(ng_ppget) and &lit(ng_ppset.)
	It is expected that several different "name spaces" will be maintained in 
	the &this map.
	
&space
&subcat Not A Distributed Hash
	&This is &stress(not) a distributed hash table (DHT); all of the managed
	information exists within each instance of &this rather than scatterd such that 
	more than one node must be queried in order to gather a complete list of the data.
	For applications like &ital(pinkpages,) it is more efficent
	to propigate changes to the managed information to all nodes rather than 
	make individual querries for information as it is needed.  This also allows 
	the map data to be accessable even if a node is down, or if the &this process 
	on a node is down. 


&space	
&subcat	Implementation Basis
	The implementation of &this is loosely based on the paper presented by Gupta et. al.
	at HotOS in  May, 2003.  The paper demonstrated that an in-core routing table
	could easily, and efficently, be managed in an environment where nodes were 
	joining and leaving the network frequently. In the case of &this, the routing 
	table is the information map that is being managed. Unlike the application 
	described by Gupta, &this only manages the information rather than using it 
	to route requests to another node. 
&space
	&This must manage two types of join/leave events: true node starts and stops, 
	as well as the additions, deletions, and updates to the information map.
	We expect that the former will happen rarely, and the latter will happen 
	very frequently (e.g. some pinkpages values are modified every 5 minutes by ng_rota).
	It is possible, with a bit more work, to extend
	&this in order to create a DHT, but at this time it is not necessary to support
	Ningaui.

&space
&subcat	Node Relationships
	Nodes running &this organise themselves into an information dissemination tree
	either through the use of a multicast group, or by using a specialised 
	name service daemon &lit(ng_nnsd.)  A newly started node will broadcast 
	a request for an existing node to provide it with an insertion point. The 
	new node will collect responses over a small amount of time, select the 
	best insertion point, and then attach itself to the tree.  Once established in 
	the tree, it requests a refresh of data from its parent node and can 
	begin to be a full functioning &this node.  
	When a node does not receive any responses to its insertion point (adoption) 
	requests, it declares itself the root of the communications tree. 

&space
	When a node fails, or &this is stopped on a node, the children of the failed 
	node start the process of inserting themselves back into the tree. 
&space
&subcat	Orphaned Nodes
	While a node is in an orphaned state (waiting for a node to adopt it back into 
	the tree) any updates to the information map that are received by the node
	are queued and passed along to the new parent once the node has been adopted.
	Similarly, after the cached messages are passed to the parent, the node will 
	ask the parent for a reload in order to receive any information that was 
	propigated through the rest of the tree while the node was not attached. 
&space
	If the tree's root node is lost, all of the &this processes are disconnected and
	go through their normal readoption process with one of the processes declaring it
	self the root. 
	In an attempt to keep the number of root collisions in this case to a minimum,
	a random delay is introduced between the time that a node notices its parent is
	missing, and its first adoption request is transmitted. 
	Should two, or more, nodes each decide that they are 
	the root, they will determine which has the right to be root, and the others
	will cede and reattach to the tree. 
&space
&subcat Communications
	Unlike most DHTs, &this communicates with other instances of &this using 
	TCP/IP sessions rather than UDP/IP datagrams.  We have experienced enough
	issues with UDP datagram loss that the small amount of extra overhead
	(three or four sessions per node) should not be noticed and keeps the 
	implementation of &this simpler because &this depends on the underlying TCP 
	for to provide 'keep alive' and retry processing.

&space
	Obviously, UDP messages are used for broadcasting 'adoption' requests
	and the responses to them.  Unlike messages that send map information, the 
	loss of an adoption request now and then is not harmful.  

&space
&subcat Narbalek Name Service Daemon
	Using a mulitcast group is the easiest method of allowing &this processes to 
	discover and insert themselves into the communications tree. 
	For political, or other reasons, the availibilty for some or all of the nodes in 
	a cluster to participate in a multicast group might not be possible. In these 
	cases one of two options can be used: a multicast agent, or the Narbalek 
	Name Service Daemon (nnsd).  
	If multicast is possible for all but one or two of the nodes, then using 
	&lit(ng_parrot) to act as a multicast agent for the 'remote' nodes is simple
	and recommended.  The agent information is supplied to &this via the &lit(-a) 
	command line flag and is of the form node:port. 
&space
	If more than a few (three or four) nodes are not able to use multicast, then
	using &lit(nnsd) is recommended.  This is nearly as simple as using multicast, but 
	does require starting and maintaining one or more &lit(nnsd) processes. When 
	&lit(nnsd) is used, each &this process registers with all available &lit(nnsd) 
	processes.  &This then requests a list of all &this processes from each &lit(nnsd)
	and sends an adoption request to each.  The &lit(-d) command line option turns on 
	the use of &lit(nnsd,) and when turned on &this expects to find a list of nodes 
	running &lit(nnsd) in the environment variable &lit(NG_NNSD_NODES.) The information 
	supplied with the &lit(-d) flag is the well known port number that &lit(nnsd) processes 
	are expected to be listening on. 

&space
&subcat	The League
	The league allows multiple instances of &this to be started on the same node, to 
	manage different data maps, and not require a second multicast group address or 
	a second set of &lit(nnsd) processes.  &This processes use the league setting to 
	filter adoption requests, and other messages received via UDP.  If the league is 
	not supplied, it defaults to &lit(flock.)

&space
&subcat Commands
	&This accepts commands from external processes, via TCP/IP 
	sessions, to set or retrieve values, control the verbosity level and 
	to get a complete dump of the information map being managed. 
	The syntax of each of these commands is listed below as well as a
	small description of the action taken by &this when the command is 
	received.
&space
	The shell scripts &lit(ng_nar_get) and &lit(ng_nar_put) implement the user
	interface to &this, and thus it should not be necessary for any user 
	process to directly send these commands to &this.  The commands are documented
	here for those maintaining the &lit(ng_nar_) scripts. 
&space
&litblkb

  get <variablename>
  Get <variablename>
  set <variablename>[=<value>]
  com <variablename>=<comment tokens>
  dump[:name=value[:pattern]]
  log
  verbose n

&litblke
&space
&subcat The Dump Command
	The dump command causes the entire information  map to be returned to the caller.  
	The syntax of the dump command is:
&space
&litblkb
	dump:[name=value:[pattern]]
&litblke
&space
	The map is maintained as hash table, and becuse of this the order
	that the entries are returned &stress(cannot) be guarenteed. 
	The syntax of the records returned is:
&space
&litblkb
	<name>="<value>"<whitespace>[#<comment>]
&litblke
&space
	The dump command may also include a name=value pair which causes all occurances 
	of &ital(name) in the values dumped to be replaced with &ital(value).  
	This allows something like &lit(NG_ROOT) to be specified by the requestor and 
	replaced in the map as the dump is produced. 
	If no replacement string is to be supplied, then an empty string (::) should be 
	sent on the command. 
&space
	Supplying a pattern (regular expression) after the second colon causes 
	only the variables whose name matches the expression to be returned. 

&space
&subcat The Set Command
	The set command is used to associate a value with a name in the information 
	map.  The syntax of the command is:
&litblkb
   set name=value
&litblke

&space
	Notice that quotation marks are not needed and may acutally cause problems 
	if supplied on the command. If no value is provided (e.g. set foo=), then the information 
	is cleared from the map.  
&space
&subcat The Get Command
	The get command fetches the current value associated with the name. The get 
	command may be specified with an upper case 'g' to indicate that expansion 
	is &lit(NOT) desired. When the command is specified with a lowercase 'g' then
	both &lit($name) and &lit(@name) expansion are applied to the variable. 
&space
	Multiple variable names may be placed on the get command if separated 	
	with a vertical bar (|) character.  When this is done, &this 
	will search the list left to right, and return the value for the first 
	variable name that is defined in the map. 
	This allows scripts like &lit(ng_ppget) 
	to send a single command to look up a variable name in a multilayered
	namespace.  

&space
&subcat The Comment Command
	The comment, or com, command associates a comment string with the name. 
	The syntax is the same as for the set command.  If a comment is supplied for
	a name, then it is listed, following a hash mark, along with the name=value
	string when a dump command is received. 

&subcat The log Command
	Initially logging to the mater log via &lit(ng_logd) is disabled.  This is 
	necessary as &this is likely to be started on a node well before all other 
	processes, most importantly before ng_logd, which can cause delays 
	waiting for ack messages.  The log command should be sent to &this by 
	the startup script (ng_node) after the ng_logd daemon has been started. 

&subcat The verbose Command
	This command can be used to dynamically change the verbose level of &this.



&space 
&subcat Checkpointing The Map
	The information map is written to a checkpoint file approximately every 
	five minutes.  The checkpoint file is created in 
	the current working directory (supplied on the command line or 
	in $NG_ROOT/site/narbalek by default). The checkpoint file is named with the following syntax:
	&lit(narbalek.)&ital(mapname).  A seond checkpoint file is also created
	that has &ital(.hnn) appended to the name where &ital(nn) is the current
	hour.  This provides a 24 hour backup scheme.  The checkpoint file name 
	can be overridden using the &lit(-C) option on the command line. 
&space
	On startup, &this will read, and initialise from the checkpoint file, &stress(ONLY)
	when it believes it is the first node to start and declares itself as the root process
	in the communications tree. 
	Otherwise, it expects to get an accurate map from its parent in the tree.

&space
	The syntax of the information map in the checkpoint file is as follows:

&space
&litblkb
  @<name>=<value><newline>
  %<name>=<comment><newline>
&litblke

&space
	If it becomes necessary to edit or create a checkpoint file, 
	values should &stress(NOT) be quoted; all spaces are retained when loaded. 
	Order of the name value, and name comment pairs is not important other than 
	comments should follow the value defintion.

&space
&subcat Starting &This
	The wrapper script &lit(ng_endless) can be used to start &this and to 
	keep restarting it should it crash.  The basic syntax for starting 
	is:
&space
&litblkb
   ng_endless -v ng_narbalek -v -c numbat -p $NG_NARBALEK_PORT
&litblke

&space
	The &lit(ng_init) script ensures that &this is started, with the proper command line 
	flags to use &lit(nnsd) and an agent if necessary. The script should be consulted, but 
	the assumption is that it will invoke &this with a &lit(-d) option when it finds the 
	&lit(NG_NNSD_NODES) variable in the environment, and with an agent option (-a) when 
	the file &lit($NG_ROOT/site/narbalek/agent) exists. 
&space

&opts   The following command line flags are recognised and alter the default behavour of &this.
&space
&begterms
&term	-a agent : Causes &this to use the agent for all UDP and multicast transmissions.  This is 
	necessary if &this is running on a node that is not capable of sending or receiving 
	multicast messages on the narbalek group, and/or when the node is isolated from the rest 
	of a cluster by a firewall which blocks UDP traffic on random ports.  
&space
	-A : No adopt mode.  Mostly for testing, this flag prevents &this from responding to 
	any adoption requests.  It then cannot become the root node, and will always be a leaf
	in the communications tree. 
&space
	&ital(Agent) is expected to be the name 
	or IP address, and port, of the process (ng_parrot) that will act as the agent. The 
	syntax is either name:port, or address:port.  The agent is expected to forward all 
	messages that are sent to the multicast group, and to return to this process all 
	messages that are recevied via UDP. 
&space
&term	-c cluster : Defines the name of the custer of &this processes will use when 
	listening for, and responding to, adoption and root messages.  This typically is 
	the cluster name, but can be any name such as &ital(flock) if a flock wide information 
&space
&term	-C filename : Defines the checkpoint filename that is used.  An hourly filename 
	will also be created using &ital(filename) with &lit(.h)&ital(nn) appended to it.
&space
&term	-d port : Defines the &lit(ng_nnsd) listening port.  This causes &this to use &lit(nnsd) 
	to locate and join the communications tree for the league rather than using multicast.
	When this option is supplied, any mulitcast information supplied on the command line is 
	ignored.
&space
&term	-f : Use full domain name. This causes the fully qualified domain name (e.g. rockliz.foo.com)
	to be used when registering with &lit(nnsd.) This value is subject to how the system name 
	is implemented, and might result in a short name if the system does not return any 
	domain information. 

&space
&term	-g net-addr : Supplies the network address of the multicast group that &this will use 
	to request adoptions and to annonce root information. If not supplied a group of
	235.0.0.7 is used. If this option is supplied with the -d option (port for nnsd), 
	then the multicast information is ignored. 
&space
&term	-i filename : Supplies a default initialisation file if the checkpoint file is not there. 
	This allows &this to be shipped with an initialisation file that should only be used once, or 
	in the event that the checkpoint file is lost. 
&space
&term	-l name : Supplies the &ital(league) name. The league is the group of &this processes
	that are bound in the tree. It allows multiple logical groupings of &this processes
	to share the same multicast group and port.  
&space
&term	-m : Allow merge when the communication tree is rejoined.  By default, the root &this process
	does not attempt to merget its data with the older tree. 
&space
&term	-p port : Sets that port that &this will listen on.  (This flag is required.)
&space
&term	-P : Peon mode. The instance of &this may never become the root node of the tree.
	Generally this option is supplied when starting on a Satellite node. 
&space
&term	-t n : This supplies the multicast time to live (ttl) value. If not supplied, a default of 2 is used when 
	multicast is emabled.
&space
&term	-w dir : The path of the working directory. If not supplied, then &lit($NG_ROOT/site/narbalek) 
	is assumed. 
&space
&term	-v : Verbose mode.
&endterms
&space

&see
	&seelink(pinkpages:ng_nar_get) 
	&seelink(pinkpages:ng_nar_set) 
	&seelink(pinkpages:ng_nar_map) 
	&seelink(pinkpages:ng_ppget) 
	&seelink(pinkpages:ng_ppset) 
	&seelink(pinkpages:ng_pp_build) 
	&seelink(pinkpages:ng_parrot) 
	&seelink(pinkpages:ng_members)
	
&space
&mods
&owner(Scott Daniels)
&lgterms
&term   25 Aug 2004 (sd) : Original code. (HBD RMH)
&term	19 Sep 2004 (sd) : Added shuffle to maint() -- attempt to balance tree over time.
&term	20 Sep 2004 (sd) : added multi variables on get support.
&term	24 Sep 2004 (sd) : Corrected bug in get_var bleat() call.
&term	25 Oct 2004 (sd) : Added multicast agent support. (v1.1)
&term	28 Oct 2004 (sd) : Added conditional set processing (v1.2). 
&term	21 Jan 2005 (sd) : Compensates now for the fact that use-me messages from UDP are likely
		not from the same port number as the TCP listen port. (v1.3)
&term	31 Jan 2005 (sd) : removed the call to load after an attach -- there is a bug in that 
		process when it washes the symtab.  Caused garbage to be saved/generated. (version 1.3/01315)
&term	02 Feb 2005 (sd) : Fixed bug in nar_sym that was causing symbol table corruption. Added version 
		info to the stats message for easier determination of running version of all narbaleks.
		(v1.4)
&term	04 Feb 2005 (sd) : Now saves real IP address (sans port) even when conneting via an agent. (1.4/02045)
&term	10 Mar 2005 (sd) : Fixed bug that allowed an empty name.
&term	18 Mar 2005 (sd) : Cleaned up comments.
&term	24 Mar 2005 (sd) : Corrected problem with how agent buffers were being handled. (v1.5)
&term	03 Apr 2005 (sd) : Now drops children when parent is lost in order to keep from missing data 
		when many nodes are bouncing. Corrected issue where a normal exit command caused
		a core dump. (v1.6)
&term	05 Apr 2005 (sd) : Fixed problem with dropping all children.
&term	17 Aug 2005 (sd) : Sends value only if table loaded; 
&term	05 Oct 2005 (sd) : Added code to try to prevent the effects of joining the multicast group 
	before the router starts.  This seems to cause problems with mcast "I am root" messages 
	from being received past the local router. We will rejoin the group every so often.
	(v2.0)
&term	31 Jan 2005 (sd) : Narbalek now switches to NG_ROOT/site/narbalek unless directed with a 
	command line option. (v2.1/01316)
&term	02 May 2006 (sd) : Detects when si starts to queue messages on a session and drops the session 
		like a hot potato.  We should not be in a state where the other end has gone off 
		for so long that we queue things. (v2.2)
&term	20 Jun 2006 (sd) : Adjusted verbose level on some bleat messages.
&term	17 Aug 2006 (sd) : Adjusted verbsoe level (again) on some bleat messages.
&term	01 Aug 2006 (sd) : Prevent passing a null pointer to ng_netif functions.
&term	28 Sep 2006 (sd) : Fixed cause of coredump if blocked session was detected. 
&term	07 Jun 2007 (sd) : Added timestampping of variable data; all invocations with an agent
		node:port will connect to the agent instead of just when multicasting. All UDP 
		traffic is routed through the agent in order to allow for access through 
		a firewall (assumes that the agent port is open through the router). (v3.0)
&term	08 Jun 2007 (sd) : Added support for merging two trees after a split.
&term	18 Sep 2007 (sd) : Fixed bug that allowd the response to an adoption message before we got
		a load from our parent. (v3.1)
&term	27 Sep 2007 (sd) : Change to flip off loaded and load pending flags when we pop the load lease. (v3.2).
&term	02 Jul 2008 (sd) : Ensured that all calls to siclose() are preceded with a call to siclear() (v3.3). 
&term	31 Jul 2008 (sd) : Fixed bug with data propigation of merged data. This breaks backwards compatability
		with 2.x and 1.x versions, but there should not be any of these out there. (v3.4)
&term	01 Aug 2008 (sd) : Fixed several instances where the end of data string was being sent with an end of string. 
		.** might have been the cause of the equerry timeout issues, but have no direct proof of this.
&term	06 Jan 2009 (sd) : Changes made to reduce the disruption caused (unavailablity) when a leaf node attempts to 
		find a better spot in the communication tree. Nodes now query other narbalek processes to see if
		a better spot might exist and only detach and insert if there is a spot that is better. (V3.5)
		.** In addition, the lease when waiting for 'use-me' responses during the adoption process is now 
		.** short (2s) when we are attempting to reposition ourself to a better spot in the tree.  During 
		.** an initial insertion, and if we atempt and find no potential parents, the longer lease (5s)
		.** is used.  This should reduce the number of request rejections during a reposition in the tree.
&term	20 Jan 2009 (sd) : Tweek to ensure that we have a list of peers from nnsd before sending an adoption request.
&term	21 Jan 2009 (sd) : Corrected a bug that was causing the substitution variable value to not be set/reset 
		under some circumstanses. (V3.6)
&term	04 Feb 2009 (sd) : Corrected an issue that caused a core dump when parent session dropped during the 
		propigation of data into the tree -- bug 1219.  (V3.7)
&term	09 Mar 2009 (sd) : Remvoed isblank() dependancy as it buggers on some sun systems and wasn't worth the 
		hastle. (v3.8).
&term	24 Mar 2009 (sd) : Reinstated the isblank() check -- still need to support SunOS 5.9 and the symtab 
		functions use isblank().  (v3.9)
&term	15 Apr 2009 (sd) : Changed request for nnsd list to every 60s until we get the first list.
&term	22 May 2009 (sd) : Change to prevent logging to the master log until it is enabled.  Narbalek is started
		well before ng_logd is started, and the delay waiting for acks that will not ever come 
		when running on a satellite, or sparsely populated cluster, can be maddening. (v3.10)
&term	08 Jun 2009 (sd) : Corrected issue with conditional update.  (v3.11)
&term	10 Aug 2009 (sd) : Changed to check for SI indication of queuing session via ng_sierrno. From the SI perspective 
		queuing is normal, and it does not return a hard error as was assumed by this code. (v3.11)
&endterms
&scd_end
*/
#endif
