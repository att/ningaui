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
	stuff that directly supports nnsd
	This file should be included by narbalek.c early on.

	Mod:	28 Feb 2007 (sd) : Free of list in mk_dlist.
		02 Jun 2007 (sd) : Conversion to allow datestamping, pruning of peer list to 
			remove dups.
*/

typedef struct peer {		/* info about a narbalek peer to which we can request adoption etc */
	struct peer *next;
	char	*addr;
	ng_timetype expiry;	/* time at which point this peer entry becomes stale */
} Peer_t;

typedef struct nnsd {
	struct 	nnsd *next;
	char	*name;		/* node name where the nnsd lives */
	char	*addr;		/* address, well name:port */
} Nnsd_t;

char 	*nlist = NULL;			/* list of narbaleks from nnsd */
ng_timetype nlist_lease = 0;
Nnsd_t	*dlist = NULL;			/* list of nnsd structures from NG_NNSD_NODES */
Peer_t	*peer_list = NULL;

/* create a list of nnsd structures -- one for each listed in the list string
	vlist string is assumed to be from the NG_NARBALEK_NNSD variable and is expected 
	to have the syntax: <nodename> [<nodename>...]
*/
void mk_dlist( )
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
		ng_bleat( 0, "abort: no NG_NNSD_NODES list in environment/pinkpages" );
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

	ng_bleat( 2, "mk_dlist: making new daemon list: %s", list );

	p = strtok_r( list, " ", &nextp );
	while( p )
	{
		np = (Nnsd_t *) ng_malloc( sizeof( *np ), "nnsd_t block" );
		np->name = strdup( p );
		sfsprintf( buf, sizeof( buf ), "%s:%s", p, nnsd_port_str );
		np->addr = strdup( buf );				/* address, or really name:port */
		np->next = dlist;
		dlist = np;
		ng_bleat( 3, "alloctated nnsd block for %s", np->addr );
	
		p = strtok_r( NULL, " ", &nextp );
	}

	ng_free( list );
}


/*
	send to all nnsd processes that we know about
*/
void send2nnsd( char *buf )	
{
	Nnsd_t *np;
	int	len; 

	mk_dlist( );				/* ensure we have a current list of nnsd nodes */
	len = strlen( buf );

	for( np = dlist; np; np = np->next )
		send_udp( np->addr, buf, len );		
}

/*
	send to all known narbalek peers
*/
void send2peers( char *buf )
{
	Peer_t	*p;
	int	len;
	ng_timetype now;
	int	sent = 0;

	len = strlen( buf );

	now = ng_now();
	
	for( p = peer_list; p; p = p->next )
	{
		if( p->expiry >= now )
		{
			send_udp( p->addr, buf, len );
			sent++;
		}
	}

	ng_bleat( 2, "send2peers: msg sent to %d peers: %s", sent, buf );
}


/*
	request a peer list from nnsd
*/
void request_list( )
{
	Nnsd_t *np;
	char	buf[1024];
	int	len; 

	flags &= ~FL_HAVE_LIST;				/* turn off the list */

	sfsprintf( buf, sizeof( buf ), "0:list:0:List:!%s\n", league );	/* 0:list is the command to nnsd, 0:List is the prefix it should send back to us */
	ng_bleat( 2, "requesting peer list" );

	send2nnsd( buf );
}


/*
	save a list of peers from nnsd
	we get the tokenised list array.  the list comes to us from narbalek as:
	List:node[.addr] node[.addr]...  We are interested in nodes
*/
void save_list( char *buf )
{
	static 	Symtab	*peer_hash = NULL;	/* quick lookup hash of peer list */
	Syment	*se;
	Peer_t	 *p;
	Peer_t	*np;			/* pointer at next for delete */
	ng_timetype lease;		/* timestamp when the peer block becomes stale -- we never delete them, just stop using them */
	char	*tp;			/* pointer into token */
	char	*tok = NULL;		/* next token in buffer */
	char	*nextt;			/* for strtok_r */
	char	wbuf[1024];

	if( ! peer_hash )
		peer_hash = syminit( 1001 );

	flags |= FL_HAVE_LIST;					/* we have received a list; even if it results in nothing */
	
	if( buf )
	{
		ng_bleat( 2, "saving peer list: %s", buf );
		tok = strtok_r( buf, " ", &nextt );
	}
	else
		ng_bleat( 2, "peer list was empty" );

	lease = ng_now() + PEER_LEASE_TSEC;

	while( tok )
	{
		if( (tp = strchr( tok, '.' )) )
			*tp = 0;				/* name.address -- we lop off the address */
		
		if( strcmp( tok, my_name ) != 0 )		/* dont put us into the list */
		{
			if( (se = symlook( peer_hash, tok, PEER_SPACE, 0, SYM_NOFLAGS )) == NULL )	/* not one yet */
			{
				sfsprintf( wbuf, sizeof( wbuf ), "%s:%s", tok, port_str );	/* the name and port we expect narbs to be listening on */
				p = (Peer_t *) ng_malloc( sizeof( *p ), "peer_t block" );
				p->addr = strdup( wbuf );
				p->next = peer_list;
				p->expiry = lease;
				peer_list = p;

				symlook( peer_hash, tok, PEER_SPACE, p, SYM_COPYNM );			/* add */
				ng_bleat( 3, "alloctated peer block for %s", p->addr );
			}
			else
			{
				p = (Peer_t *) se->value;
				p->expiry = lease;
			}
		}

		tok = strtok_r( NULL, " ", &nextt );
	}
}
