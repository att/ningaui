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


/* -----------------------------------------------------------------------------------
	Mnemonic:	s_funnel - funnel messages to seneschal (one way)
	Abstract:	Establishes a session with seneschal and sends the local status
			messages from s_eoj back.  The idea is to eliminate the large
			number of TCP connect/disc at the jobber when there are lots of
			jobs being dispatched that have an execution time of a very small
			amount.
	Date:		05 Oct 2007
	Author: 	E. Scott Daniels
   -----------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sfio.h>
#include <ng_socket.h>
#include <signal.h>

#include <netinet/in.h>
#include <ningaui.h>

#include <s_funnel.man>

struct msg {
	struct msg *next;
	char *buf;
	int len;
};

char	*version = "v1.0/0a057"; 
int	verbose = 0;
int	seneschal_fd = -1;		/* we only track the fd for seneschal */
int	ok2run = 1;
char	*sene_port = NULL;

struct msg *mq_head = NULL;
struct msg *mq_tail = NULL;

void push_msg( char *buf, int len )
{
	struct msg *mp;

	mp = (struct msg *) ng_malloc( sizeof( struct msg), "msg" );
	mp->buf = (char *) ng_malloc( len, "buf" );
	memcpy( mp->buf, buf, len );
	mp->len = len;
	mp->next = NULL;
	if( mq_tail == NULL )
	{
		mq_head = mq_tail = mp;
	}
	else
	{
		mq_tail->next = mp;
		mq_tail = mp;
	}
}

void pop_msg( )
{
	struct msg *mp;

	if( (mp = mq_head) != NULL )
	{
		mq_head = mp->next;
		free( mp->buf );
		free( mp );
	
		if( mq_head == NULL )
			mq_tail = NULL;
	}
}

int send2sene( )
{
	char	*sport = NULL;				/* port number for agent */
	char	*shost = NULL;				/* host name srv_D1agent is currently on */
	char	addr[1024];

	if( seneschal_fd < 0 )					/* no connection -- try to get */
	{
		
        	if( (sport = ng_env( "NG_SENESCHAL_PORT" )) == NULL )          /* get his well known port */
        	{
                	ng_bleat( 3, "conn2_agent: cannot find seneschal port (NG_SENESCHAL_PORT) in environment/cartulary" );
			return -1;
        	}

        	if( (shost = ng_pp_get( "srv_Jobber", NULL )) == NULL || !*shost )    /*get each time we connect; allows seneschal to move */
        	{
			ng_bleat( 3, "send2seneschal: cannot determine srv_Jobber host" );
			return -1;
        	}
        
        	sfsprintf( addr, sizeof( addr ), "%s:%s", shost, sport );
		ng_bleat( 1, "attempting session with seneschal: %s", addr );
        	if( (seneschal_fd = ng_siconnect( addr )) >= 0 )
        	{
			ng_siprintf( seneschal_fd, "name=funnel\n" );
                	ng_bleat( 1, "send2seneschal: session established with seneschal (%s) on fd %d", addr, seneschal_fd );
        	}
        	else
        	{
                	ng_bleat( 4, "send2seneschal: connection failed: %s:  %s", addr, strerror( errno ) );
			return -1;
        	}

		ng_free( sport );
		ng_free( shost );
	}


	while( mq_head && seneschal_fd >= 0 )
	{
		if( ng_sisendt( seneschal_fd, mq_head->buf, mq_head->len ) == SI_RET_ERROR )
		{
			ng_bleat( 1, "seneschal session write error; dropping session" );
			ng_siclose( seneschal_fd );
			seneschal_fd = -1;
		}
		else
		{
			mq_head->buf[mq_head->len-1] = 0;
			ng_bleat( 1, "buffer sent: %s", mq_head->buf );
			pop_msg( );			/* pop the message */
		}
	}
}


/* accept all connections; we do not track fd because we dont need to */
int cbconn( char *data, int f, char *buf )
{
	return SI_RET_OK;
}

int cbdisc( char *data, int f )
{
	if( f == seneschal_fd )
	{
		seneschal_fd = -1;
		ng_bleat( 1, "session to seneschal was disconnected" );
	}
	return SI_RET_OK;
}


/* deal with inbound messages -- we drop any session that sends us crap */
int cbtcp( void *data, int fd, char *buf, int len )
{
	int drop_session = 0;

	if( len < 7 )
	{
		if( fd )
			ng_siclose( fd );			/* drop them like a hot potato */
		return;
	}

	switch( *buf )
	{
		case 'j':						/* job status to forward back */
			if( strncmp( buf, "jstatus=", 8 ) == 0 )
			{
				push_msg( buf, len );				/* push on send queue */
				send2sene(  );					/* send this and any other queued messages */
				if( fd >= 0 )
					ng_siprintf( fd, "msg received\n.\n"  ); 
			}
			else
				drop_session = 1;
			break;

		case 's':
			if( strncmp( buf, "status r", 8 ) == 0 )
				ng_bleat( 2, "ack from seneschal" );
			else
				drop_session = 1;
			break;

		case 'v':
			if( strncmp( buf, "verbose ", 8 ) == 0 )
			{
				char 	*cp;

				if( (cp = strchr( buf, ' ' )) != NULL )
				{
					verbose  = atoi( cp );
					if( fd >= 0 )
						ng_siprintf( fd, "verbose level now %d\n.\n", verbose ); 
					ng_bleat( 0, "verbose level rest to: %d", verbose );
				}
			}

		case 'x':						/* time to go away */
			if( strncmp( buf, "xit4360", 7 ) == 0 )
				ok2run = 0;
			else
				drop_session = 1;
			break;

		default:
			drop_session = 1;
			break;
	}

	if( drop_session &&  fd >= 0 )
		ng_siclose( fd );

	return SI_RET_OK;
}

int cbudp( void *data, char *buf, int len, char *from, int fd )
{
	cbtcp( data, -1,  buf, len );
}

void usage( )
{
	sfprintf( stderr, "%s %s\nusage: [-p port] [-v]\n", argv0, version );
	exit( 1 );
}

int main( int argc, char **argv )
{
	extern int errno;
	extern int ng_sierrno;

	int 	status;
	int 	i;
	char 	*port = NULL;
	int	init_tries = 20;	/* number of times we retry if port is in use */

	ARGBEGIN
	{
		case 'p':	SARG( port ); break;
		case 'v':	OPT_INC( verbose ); break;

		default:
			sfprintf( stderr, "unrecognised option\n" );
usage:
			usage( );
			exit( 1 );
	}
	ARGEND



	if( ! port )
	{
		if( (port = ng_env( "NG_SENEFUNNEL_PORT" )) == NULL )
		{
			ng_bleat( 0, "unable to find NG_SENEFUNNEL_PORT in cartulary and -p not supplied" );
			exit( 1 );
		}
	}
	
	while( ng_siinit( SI_OPT_FG, atoi(port), atoi(port) ) != 0 )	
	{
		init_tries--;
		if( init_tries <= 0  ||  errno != EADDRINUSE )
		{
			ng_bleat( 0, "unable to initialise the network interface sierr=%d err=%d", ng_sierrno, errno );
			exit( 1 );
		}	

		ng_bleat( 0, "address in use (%s): napping 15s before trying again", port );
		sleep( 15 );
	}

	ng_bleat( 1, "version %s listening on port: %s", version, port );

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    /* register connect cb */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    /* register connect cb */
	ng_sicbreg( SI_CB_CDATA, &cbtcp, (void *) NULL );
	ng_sicbreg( SI_CB_RDATA, &cbudp, (void *) NULL );


	while( ok2run )
	{
		status = ng_sipoll( 300 );
		if( mq_head )
			send2sene();			/* prevent blocked data because session dropped and no more jobs came in */
	}

	ng_sishutdown( );          /* for right now just shutdown */
	ng_bleat( 0, "exiting normally" );
}





#ifdef SCD_SECTION
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_s_funnel:Funnel status messages to seneschal)

&space
&synop	ng_s_funnel [-p port] [-v]

&space
&desc	&This waits on status messages from &lit(ng_s_eoj) processes and funnels them 
	to seneschal.  The idea is that rather than n processes concurrently trying to 
	connect to seneschal to send one status message, each node will maintain one 
	constant connection for status messages and reduce the load on the jobber node. 
	The s_eoj processes still need to connect and send their message, but the 
	connections are made to the local host rather than to the jobber.
&space
	Currently, &this sends only job status messages back to seneschal. The verbose
	level can be modified by sending "verbose n" where n is a small integer or 0 to turn
	verbose logging off.  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p port : The port that &this should listen on. If not supplied, NG_SENEFUNNEL_PORT
	is epected to be defined in the cartulary.
&space
&term	-v : Verbose mode. 
&endterms


&space
&see	ng_seneschal, ng_s_eoj, ng_sreq

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Oct 2007 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

#endif
