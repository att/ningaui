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
Mnemonic:	nnsd - narbalek name service daemon
Abstract:	Provides a way for narbaleks to lookup other narbaleks for possible
		adoption.  Also tracks the tree root and will help to prevent 
		multiple trees.  Nnsd also supports processes using the ng_dptree
		library functions. The same set of nnsd process can support multiple
		trees; lists can be limited with the league specifier. The discussion 
		here and in the man pages refers to narbalek, but any dptree process
		can be substituted.

		As a narbalek is started, it requests a list of other narbaleks
		that are active. The narbalek can then send adoption requests to 
		any/all of the nodes in the list.  Once adopted, the narbalek 
		must send alive messages to the nnsd in order for it to be listed.
		These messages should be sent at least every 5 minutes.
		When an alive message is received, it is sent to all other known
		nnsd processes to keep their list current.

		We expect the root of a tree to send a 'Im root' message every 5 minutes 
		or so.  These root declarations are forwarded to all nodes that are 
		active in the list.  It is expected that the underlying processes
		will know how to deal with root collisions -- nnsd makes no effort 
		to track the root processes or to resolve conflicts. 
		
Date:		31 October 2005 (Happy Halloween!)
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>

#include <sfio.h>

#include <ningaui.h>
#include <ng_basic.h>
#include <ng_lib.h>

#include <ng_socket.h>

#include "nnsd.man"

#define NODE_SPACE	1	/* symtab space constants -- node stuff */

#define NODE_LEASE_TSEC	650	/* lese time of 65 sec; a node is out of date if we dont have an alive message this frequently */
#define ROOT_LEASE_TSEC 650	/* lease of 65 sec for nodes that have declared they are root */
#define DLIST_LEASE_TSEC 3000	/* we get a new dlist from environment (pinkpages) every 5 minutes */

typedef struct node {		/* info on each narbalek registered with us */
	struct	node *next;
	char	*name;		/* node name -- we hash to this name which is name.address */
	char	*lname;		/* name of the node put into the list */
	char	*addr;		/* their address */
	char	*league;	/* the node may associate with a league -- we then can dump just league members */
	int	nlen;		/* length of list name -- quicker list building, buf overrun checking */
	ng_timetype lease;	/* we require a heartbeat -- after this time we do not list them */
	ng_timetype rlease;	/* we will believe they are the root until this time passes. */
} Node_t;

typedef struct nnsd {
	struct 	nnsd *next;
	char	*name;		/* node name where the nnsd lives */
	char	*addr;		/* address, well name:port */
} Nnsd_t;


Node_t	*nlist = NULL;		/* list of narbalek processes */
Nnsd_t	*dlist = NULL;		/* list of nns daemons */

char	*version="v2.2/06047";
char	*dport = 0; 		/* nnsd port */
Symtab	*stable = NULL;		/* we hash into the list to quickly process alive messages */
int	alarm_time = 0;		/* if we need to set an alarm -- num of seconds before alarm -- 0 is off */

extern int verbose;
extern int ng_sierrno;

void dump( char *addr )
{
	char	buf[NG_BUFFER];
	char	wbuf[256];
	int	used = 0;
	int	len;
	Node_t	*np;

	if( ! addr )
		return;

	*buf = 0;
	for( np = nlist; np; np = np->next )
	{
		len = sfsprintf( wbuf, sizeof( wbuf ), "%s %s %s\n", np->lname, np->addr, np->league ? np->league : "no-league" );
		if( used + len >= NG_BUFFER-1 )
		{
			ng_sisendu( addr, buf, used );
			used = 0;
			buf[0] = 0;
		}
		strcat( buf, wbuf );
		used += len;
	}	

	strcat( buf, "#end#\n" );
	used += 6;
	ng_sisendu( addr, buf, used );
}

/*
	any node block on the list that is old will be trashed.  called 
	when we build a list and there are more than a handfull to trim
	away.
*/
void prune( )
{
	Node_t *np;		/* into list */
	Node_t *nnp;		/* at next when we find one to trash */
	Node_t *pnp = NULL;		/* at prev when we find one to trash */
	ng_timetype ts;

	ts = ng_now( );

	for( np = nlist; np; np = nnp )
	{
		nnp = np->next;
		if( np->lease < ts )		/* time to ditch this one */
		{
			if( pnp )
				pnp->next = nnp;
			else
				nlist = nnp;
			
			ng_bleat( 2, "prune: symdel"  );
			symdel( stable, np->name, NODE_SPACE );
			ng_bleat( 2, "prune: free np: %I*x %I*x(addr), %I*x(name), %I*x(league), %I*x(lname),", Ii(np), Ii(np->addr), Ii(np->name), Ii(np->league), Ii(np->lname) );
			ng_free( np->addr );
			ng_free( np->name );
			ng_free( np->league );
			ng_free( np->lname );
			ng_free( np );
		}
		else
			pnp = np;		/* did not trash so this becomes the previous */
	}
}

/* create a list of nnsd structures -- one for each listed in the list string
	vlist string is assumed to be from the NG_NARBALEK_NNSD variable and is expected 
	to have the syntax: <nodename> [<nodename>...]

	Currently nnsd processes do not exchange info. this and send2nnsd() are unused, but 
	here on the off chance that we might need to swap some information.  in order to 
	guarentee that root messages are received by all processes, narbalek must send the
	iamroot message to all nnsd proceses that are listed.  
*/
void mk_dlist( )
{
	static	ng_timetype expires = 0;	/* we will update the list every few minutes */
	static char *my_name = NULL;		/* name of this node so we dont add  us to the list */

	char 	*p;				/* pointer into list */
	char	*nextp = NULL;			/* for strtok_r call */
	char	*list;
	char	buf[1024];
	ng_timetype ts;				/* current time stamp */
	Nnsd_t	*np;
	Nnsd_t	*next;

	if( ! my_name )
	{
		my_name = (char *) ng_malloc( sizeof( char ) * 1024, "mk_dlist:myname" );
        	if( ng_sysname( my_name, 1024 ) )
                	exit( 1 );   
	}

	ts = ng_now( );
	if( (expires > ts) && dlist )		/* not time to refresh the list */
	{
		return;
	}

	expires = ts + DLIST_LEASE_TSEC;			

	if( (list = ng_env( "NG_NNSD_NODES" )) == NULL )	/* no list -- that is not a good thing, but we can play without it */
	{							/* keep old list incase it is a bad cartulary or what */
		return;	
	}

	for( np = dlist; np; np = next )		/* free the old list */
	{
		ng_bleat( 2, "free old dlist %I*x %I*x(name) %I*x(addr)", Ii(np), Ii(np->name), Ii(np->addr) );
		next = np->next;
		ng_free( np->name );
		ng_free( np->addr );
		ng_free(  np );
	}

	dlist = NULL;

	ng_bleat( 2, "mk_dlist: making new daemon list" );


	p = strtok_r( list, " ", &nextp );	/* prime the pump */

	while( p )
	{
		if( strcmp( p, my_name ) != 0 )				/* we do not add ourselves to the list */
		{
			np = (Nnsd_t *) ng_malloc( sizeof( Nnsd_t ), "nnsd_t block" );
			memset( np, 0, sizeof( Nnsd_t ) );

			np->name = ng_strdup( p );
			sfsprintf( buf, sizeof( buf ), "%s.%s", p, dport );
			np->addr = ng_strdup( buf );				/* address, or really name:port */
			np->next = dlist;
			dlist = np;

			ng_bleat( 3, "alloctated nnsd block for %s", np->name );
		}

		p = strtok_r( NULL, " ", &nextp );
	}

	ng_free( list );
}

/*
	add/update a node in the node list; received a new listing or an alive message
*/
void update_node( char *nname, char *addr, char *league )
{
	Node_t	*np;
        Syment	*se;
	char	hname[1024];	/* name we use to hash to its block */
 
	sfsprintf( hname, sizeof( hname ), "%s.%s", nname, addr );		/* hash name is name:address */

        if( se = symlook( stable, hname, NODE_SPACE, NULL, SYM_NOFLAGS ) )
	{
                np = (Node_t *) se->value;
	}
	else						/* new node */
	{
		np = (Node_t *) ng_malloc( sizeof( *np ), "node block" );
		memset( np, 0, sizeof( Node_t ) );

		np->name = ng_strdup( hname );
		np->lname = ng_strdup( nname );		/* for the list, send back what they sent us as a name */
		if( league )
			np->league = ng_strdup( league );
		else
			np->league = ng_strdup( "flock" );	/* default so that new (v3) narbalek processes (they set their league) can play with old */

		np->next = nlist;
		np->nlen = strlen( nname );
		np->addr = ng_strdup( addr );
		nlist = np;

		symlook( stable, np->name, NODE_SPACE, (void *) np,  SYM_COPYNM );		
		ng_bleat( 1, "update_node: new listname=%s hashname=%s addr=%s league=%s", np->lname, np->name, np->addr, league ? league : "no-league" );
	}

	np->lease = ng_now() + NODE_LEASE_TSEC;
}

/* 
	build a list of nodes in a single buffer; space separated.
	add a prefix if given, and terminate with a newline.
	if league is not null, then dump only those nodes that have indicated
	they are playing in that league.
*/
char *node_list( char *prefix, char *league )
{
	char	buf[NG_BUFFER];
	Node_t 	*np;
	char	*bp;			/* pointer where we insert into buffer */
	ng_timetype ts; 
	int	blen = 0;		/* len of string in buffer -- prevent overrun */
	int	pcount = 0;		/* number that need to be pruned */
	
	ts = ng_now( );

	if( prefix )			/* if user wants a prefix on the string, add it */
	{
		sfsprintf( buf, sizeof( buf ), "%s ", prefix );
		bp = buf + strlen( prefix ) + 1;
	}
	else
		bp = buf;

	ng_bleat( 2, "generating list: prefix=(%s) league=(%s)", prefix ? prefix : "no-prefix", league ? league : "no-league" );
	for( np = nlist; np; np = np->next )
	{ 
		*bp = 0;

		if( (blen += np->nlen) > NG_BUFFER - 10 )		/* stop adding if we get close */
			break;

		/* if good lease, and entry belongs to the specified league or no specific league requested */
		if( np->lease > ts  && (league == NULL || strcmp( league, np->league ? np->league : "KaR020361" ) == 0) )
		{
			if( (bp + np->nlen + 1 ) < (buf + sizeof( buf )) )
			{
				strcat( bp, np->lname );
				bp += np->nlen;
				*(bp++) = ' ';
			}
		}
		else
			pcount++;			/* candidate for pruning */
	}

	if( bp > buf )				/* finish it nicely */
	{
		*(bp-1) = '\n';
		*bp = 0;
	}
	else
		sfsprintf( buf, sizeof( buf ), "\n" );		/* just an empty buffer I guess */

	if( pcount > 5 )
		prune( );

	return ng_strdup( buf );
}

/* 
	send a  message to all other nnsds
*/
void send2nnsd( char *msg )
{
	Nnsd_t *np;			/* into list of peers */

	mk_dlist( );					/* if its time to get a new dlist -- do it now */
	ng_bleat( 3, "send2nnsd: (%s)", msg );
	for( np = dlist; np; np = np->next )
	{
		ng_sisendu( np->addr, msg, strlen( msg ) ); 
	}
}

/* 
	handle an iamroot message.  We track which narbaleks have indicated that
	they are root, and forward these messages to any of them that have 
	asserted they are root within the last n seconds. They know how to 
	resolve conflicts.  We dont want to deal with conflicts, and narbalek/dptree
	processes need to so they can support multicast mode still.
*/
void root_notice( char *buf, char *addr )
{
	Node_t	*np;
	ng_timetype ts;

	ng_bleat( 2, "root_notice: %s", buf );

	ts = ng_now( );

	for( np = nlist; np; np = np->next )
	{
		if( strcmp( np->addr, addr ) == 0 )				/* msg from this dude */
		{
			ng_bleat( 2, "setting root lease for: %s", addr );
			np->rlease = ts + ROOT_LEASE_TSEC;		/* extend lease */
		}

		if( np->lease > ts )					/* node is active */
			ng_sisendu( np->addr, buf, strlen( buf ) );
	}
}

/* 
	process a buffer we received.  addr is the senders address.port in case we need to generate info back
	Narbalek messages are of the form:  udpid:command-token:parms (parms should be bang separated)
	These command tokens and data are recognised:
		a[live]:nodename[!league] - narbalek keep alive in list (other nnsd processes forward these to us)
		i[amroot]		- narbalek believes that it is the root of the tree
		l[ist]:[prefix[!league]]		- send a list of active nodes prefixed with the supplied string
		v[erbose]:n		- change verbose setting
		
*/
int parse( char *buf, char *addr, int len )
{
	static	char **tokens = NULL;
	int	ntoks;

	char	*p1;			/* pointer into buffer */
	char	*w;			/* pointer into work buffer at the command token */
	char	wbuf[NG_BUFFER];

	if( ! buf || len > NG_BUFFER-1 )
		return;					/* wise to handle this */

	memcpy( wbuf, buf, len );			/* convert to a string chopping newline if there */
	if( wbuf[len-1] == '\n' )
		wbuf[len-1] = 0;
	else
		wbuf[len] = 0;

	/*strcpy( wbuf, buf );*/				/* need a working buffer to trash */

	if( (w = strchr( wbuf, ':' )) == NULL )		/* past udp id */
	{
		ng_bleat( 1, "bad message, no : after id token" );
		return;					/* nothing after id, so junk it */
	}


	w++;							/* at the command token */
	if( (p1 = strchr( w, '\n' )) != NULL )
		*p1 = 0;
	p1 = strchr( w, ':' );					/* at the start of the parm string */
	if( p1 )						/* tokenise the parm string based on ! seperator */
		tokens = ng_tokenise( p1+1, '!', '\\', tokens, &ntoks );
	else
		tokens = ng_tokenise( "", '!', '\\', tokens, &ntoks );		/* generates a set of null tokens */

	ng_bleat( 4, "parse: ntok=%d (%s)", ntoks, buf );
	
	switch( *w )
	{
		case 'a':				/* alive 	0:alive:name[!league]  */
			if( ntoks )	
			{
				ng_bleat( 2, "processing alive: (%s) (%s)", tokens[0], tokens[1] );
				update_node( tokens[0], addr, tokens[1] );	
			}
			break;

		case 'd':
			ng_bleat( 3, "dumping guts" );
			dump( addr );
			break;

		case 'e':				/* exit */
			ng_bleat( 1, "exit received from: %s", addr );
			return SI_RET_QUIT;		/* cause socket interface driver to shut and return control to main */
			

		case 'i':				/* it thinks it is root (id:iamroot:league:name:udpid) */
			root_notice( buf, addr );
			break;

		case 'l':						/* send a list:   0:list:[prefix[!league]] */
			p1 = node_list( tokens[0], tokens[1] );			/* make a list with their prefix */
			ng_bleat( 3, "sending list=(%s)", p1 );
			ng_sisendu( addr, p1, strlen( p1 ) );
			ng_free( p1 );
			break;
			

		case 'v':
			if( ntoks )
			{
				verbose = atoi( tokens[0] );
				ng_bleat( 0, "verbose level changed to %d", verbose );
			}	
			break;


		default:
			ng_bleat( 0, "unrecognised message: %s", buf );
			break;
	}

	return SI_RET_OK;
	
}


/* ---------------- socket callback stuff ------------------------------------ */


/* right now we do not accept tcp/ip sessions so these functions do nothing but send back a return value */
cbconn( char *data, int f, char *buf )
{
	return SI_ERROR;		/* cause session to be rejected */
}

cbdisc( char *data, int f )
{
	return SI_RET_OK;
}

cbcook( void *data, int fd, char *buf, int len )
{
	return SI_RET_OK;
}



/*	udp message; add end of string and pass to parse 
	we expect that all messages are newline or null terminated
*/
int cbraw( void *data, char *buf, int len, char *from, int fd )
{
	/*buf[len-1] = 0;*/
	return parse(	buf, from, len );	/* parse sets the SI_RET_* value */
}




/*	deal with a signal -- if ever the need to do something based on an alarm, it triggers here 
	other signals that we could be driven for: SI_SF_USR[12]
*/
int cbsignal( void *data, int flags )
{
	if( flags & SI_SF_ALRM  )
	{
		if( alarm_time )
		{
			alarm( alarm_time );		/* wake again in 3 seconds to do maint */
		}
		else
			sfprintf( sfstdout, "cb_signal: alarm received, not reset\n" );
	}
	
	return SI_RET_OK;		
}

void usage( )
{
	sfprintf( sfstderr, "%s %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-p port] [-v]\n", argv0 );
	exit( 1 );
}


main( int argc, char **argv )
{
	int	status = 0;
	int	port;

	ARGBEGIN
	{
		case 'p': SARG( dport ); break;
		case 'v': OPT_INC( verbose ); break;
		default:
usage:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND


	ng_bleat( 1, "%s starting %s", argv0, version );
	stable = syminit( 171 );
	if( ! dport )
	{
		if( (dport = ng_env( "NG_NNSD_PORT" )) == NULL )
		{
			ng_bleat( 0, "abort: cannot find NG_NNSD_PORT in environment and -p port not supplied" );
			exit( 1 );
		}
	}

	port = atoi( dport );
 	ng_bleat( 2, "initialising network interface: port = %d", port, port );
 	if( ng_siinit(  SI_OPT_FG, port, port ) != 0 )	
 	{
		ng_bleat( 0, "unable to initialise network interface: ng_sierrno = %d errno = %d", ng_sierrno, errno );
		exit( 1 );
 	}


 	ng_bleat( 2, "registering callbacks" );
 	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    		/* driven for tcp/ip connections */
 	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    		/* disconnection */
 	ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );	/* data on tcp session (cooked)  */
 	ng_sicbreg( SI_CB_RDATA, &cbraw, (void *) NULL );	/* udp data (raw) */
 	/*ng_sicbreg( SI_CB_SIGNAL, &cbsignal, (void *) NULL );	*/


 	status = ng_siwait( );			/* set socket package do the driving -- listens and makes the callbacks */

 	ng_sishutdown( );          /* for right now just shutdown */
 	ng_bleat( 2, "siwait status = %d sierr =%d", status, ng_sierrno );
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nnsd:Narbalek Name Service Daemon)

&space
&synop	ng_nnsd [-p port] [-v]

&space
&desc	&This provides a simple focal point for narbalek processes to register 
	their existance, and to request a list of other registered narbalek
	processes.  The purpose of ng_nnds is to allow a narbalek process to 
	discover other narbalek processes without depending on multicast
	(which seems to be more unreliable than UDP). Narbalek processes need to 
	discover other nodes in order to find an insertion point into the 
	communication tree. 

&space
	NOTE: Throughout this manual page narbalek is used as an example programme which makes use of the 
	&lit(nnsd) service as it was the process for which $this was written.  
	As the ng_dptree (distributed process tree) library 
	functions were introduced, any process which uses the dptree functions
	may use &this to join its communication tree.  Thus, for the discussion 
	here, narbalek can be taken to mean "any dptree based process."

&space
	&This processes are found by narbalek processes using the &lit(NG_NNSD_NODES) 
	pinkpages variable. The value of the variable lists the nodes that 
	should be running &this processes.  It is assumed that &this processes are 
	listening on the TCP/IP port defined by the &lit(NG_NNSD_PORT) pinkpages variable.

&space
&subcat Protocol
	To minimise the impact on narbalek in order to make use of &this, messages 
	sent to &this are expected to have this general syntax:
&space
&litblkb
   <narbalek-id>:<action-token>[:<data>]
&litblke
&space
	The message format is the same as messages exchanged between narbalek processes. 
	When messages are received, the narbalek ID is ignored.  The action is 
	one of the following:
&space
&begterms
&term	alive : Indicates that the narbalek process is running and able to process
	adoption requests from other narbalek processes.  Alive messages are expected
	to be received at least every 5 minutes.  If &this does not receive an alive
	message from a narbalek process, or the last alive message was received more 
	than 5 minuts prior, the process is not placed into the list when 
	a list is requested.	The data portion of the message is expected to be the 
	human readable host name and optionally the league name. These tokens are expected
	to be separated with a bang (!) character (e.g. ningd1!srvm_ningdev). 
&space
	If a league name is supplied, it allows a list request to be limited only to the 
	nodes that are supporting a desired league. This is particlarly advantagous when 
	multiple leagues (process groups) are sharing the same &this set.  If no league name
	is supplied, then "flock" is used as the default.
&space
&term	exit : Causes &this to gracefully terminate. 
&space
&term	iamroot : The narbalek process that is currently the root of the communciation 
	tree is expected to send this message to every &this process.  These messages
	are forwarded to every registered process. The registered processes are expected
	to interpret and ignore the &lit(iamroot) messages from other leagues, and 
	to be able to resolve collisions if more than one node for a league beliees that 
	it is root. 
&space
&term	list : Requests that &this send a list of nodes (space separated, newline terminated)
	from which  narbalek processes have sent a recent 'alive' message to &this.  
	The data portion of the message may contain an optional prefix token that is added to 
	the start of the list, and an optional league name (e.g. prefix!league).  
	Narbalek processes use the prefix to properly parse the data returned by &this. 
	If a league name is supplied, then only the information for processes that have 
	a matching league name are added to the list. 
&space
	The list command that a flock wide
	narbalek might send is: 
&litblkb
   0:list:1:List!flock_narbalek.  
&litblke
&break	The message breaks downs into these pieces:
&space
&begterms
&term	0:list: : The command to &this requesting a list be sent back. The 0 is the 
	narbalek id of the sender.
&space
&term	1:List : This is the prefix (everything after the command tokens terminating colon up to 
	the first bang) that is added to the beginning of the buffer returned to the requestor. 
&space
&term	flock_narblaek : This is the list limiting string. The list generated will contain only 
	those processes having registered as beloging to the &lit(flock_narbalek)
	league. If the league name was omitted, a list of all nodes currently registered would 
	have been returned. 
&endterms
&space
&term	verbose : Allows remote setting of the verbosity level of &this. The data portion 
	is expected to contain the new verbose level.
&endterms

&space
	All messages are sent to and from &this via UDP/IP.  It is expected that alive 
	messages are sent to all &this processes that are listed on the &lit(NG_NNSD_NODES)
	variable in order to ensure that the node is not 'missed' if one of the &this processes
	goes down. 
	


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p port : The port that we should be listening on.  If not supplied, &lit(NG_NNSD_PORT) 
		is used.
&space
&term	-v : Verbose mode. The more the chattier to stderr.
&endterms


&space
&parms	No positional parameters are expected on the command line.

&space
&exit	An exit code that is not zero indicates an error. 

&space
&see	ng_narbalek, ng_dptree(lib)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Nov 2005 (sd) : Sunrise.
&term	17 Jun 2006 (sd) : Added support to accept league info and dump lists of just league participating nodes. (v2.0)
&term	20 Jun 2006 (sd) : Added a dump command to better get a grip on what is registered. 	
&term	14 Aug 2006 (sd) : changed bleat level for root lease message -- too chatty with these esp since the lease is 
		pretty meaningless for right now.
&term	26 Aug 2006 (sd) : Added a memset after malloc of nnsd_t block.
&term	30 Aug 2006 (sd) : Corrected small memory leak when sending list. (v2.1)
&term	02 Jun 2007 (sd) : Now sets default league to "flock." Needed to support v3 narbalek which now sends its league on 
		alive messages and needed the default to work with older v2 narbalek processes. 
&endterms

&scd_end
------------------------------------------------------------------ */
