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
 ----------------------------------------------------------------------
  Mnemonic:	s_netif.c
  Abstract:	These routines are the seneschal network interface routines.
		This is designed to execute as a pthread and implements
		a callback paradigm ontop of the ng_netif library 
		functions. 
		The port listener (thread entry point) establishes all of the 
		callback routines and then invokes the polling process to 
		listen for new TCP sessions and drive the appropriate 
		routines when TCP/UDP data is received and TCP sessions 
		are established/disconnected.
		When data is received a work block is queued for the main 
		thread to work on.  

		The original version of seneschal used threads; the funcitons
		here made up the network interface thread which 'owns' the 
		work queues.  Thus the work queues management functions are 
		also in this file. 

  Date: 	27 June 2001
  Author: 	E. Scott Daniels

  Mod:		24 Jan 2002 - To prevent core dump if missing parm on verbose command 
		15 Jun 2003 - To convert all command interface to tcp/ip 
		udp messages are now logged and ignored.
		03 Feb 2004 - accepts node: command.
		11 May 2003 - added priority queue for batter response time on 
			interactive commands.
		17 Aug 2004 - dump costs now goes on the priority queue
		18 Apr 2006 (sd) - revampped the command parsing in eatmsg. Too many
			things to have in a nested if; now augmented with a switch
			which should make things a bit better.
		27 Aug 2006 (sd) - New bleat when cjob received.
		24 Oct 2006 (sd) - Fixed an issue with adding the repmgr name to the sesion block
		27 Oct 2006 (sd) - Return value from pp_get() now checkd for empty string.
		18 Jan 2006 (sd) - Added support to receive user info. Also added a status queue
			so that job status messages do not block behind gobs of repmgr dump1
			responses, yet do not completely block user interactive requests.
		29 Mar 2007 (sd) - Fixed memory leak in conn2_rm().
		13 Apr 2007 (sd) - Changed rmif command such that if 0 is given as first value,
			it acts as a query.
		25 Apr 2007 (sd) - Added connect to d1agent function. 
		17 Sep 2007 (sd) - Changed bleat levels on some messages
		26 Oct 2007 (sd) - Modified the way we deal with a disconnect and pending work blocks.
			we now run just the priority queue to reset fd values as these are the 
			sessions that are expecting data back. 
		28 Jan 2009 (sd) - Added dynamic buffer support to the work block. If the incoming 
			buffer is larger than the static buffer (2k) we allocate a dynamic one to 
			do the trick.
 ------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<pthread.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>

#include	<ng_socket.h>		/* socket callback driver lib */

#include	"seneschal.h"

					/* symbol table name spaces */
#define NO_HANDLE	-1		/* ignore handle data */
#define	BY_HANDLE	1		/* sessions hashed using the network handle */
#define BY_NAME		2		/* sessions hashed using name */


#define SF_RM_SESS	0x01		/* session is a repmgr session */

typedef struct	Session_t {
	int	handle;			/* the network session handle */
	int	flags;			/* SF_ flag constants */
	char	*name;			/* session name if supplied by sender */
	char	hname[25];		/* ascii version of handle for symtab lookup */
	char	partial[NG_BUFFER];	/* buffer if we receive partial record (no newline) */
} Session_t;

extern int	max_lease_sec;		/* vars declared by seneschal main, but we compile seperately */
extern int	max_work;
extern int	max_resort;
extern int	gflags;	
extern int	suspended;
extern int	max_rm_req;		/* max number of repmgr dump1 requests we will send in a batch */
extern int	freq_rm_req;		/* number of tenths of seconds between requests sent to repmgr */

/*static Work_t	*wq_last = NULL; */

static Work_t	*free_work = NULL;	/* list of available free work blocks */
static Work_t	*wq = NULL;		/* low priority work blocks */
static Work_t	*wq_tail = NULL;	
static Work_t	*pwq = NULL;		/* high priority work queue */
static Work_t	*pwq_tail = NULL;	
static Work_t	*swq = NULL;		/* job status work queue */
static Work_t	*swq_tail = NULL;

static long	wq_size = 0;		/* work queue counters */
static long	pwq_size = 0;
static long	swq_size = 0;

static Symtab	*symbols = NULL;	/* hashes session name to a session block */
static char	*wbuf = NULL;		/* work buffer */

#ifdef DO_THREADS
static pthread_mutex_t	work_mutex;	/* mutex for locking the queues */
#endif 

/* ------------- local prototypes -------------------- */
/* callbacks - driven when ng_siwait noitces an event */
static int cb_tcpdata( void *data, int handle, char *buf, int len );	/* buffer received on a tcp session (handle) */
static int cb_udpdata( void *data, char *buf, int len, char *from, int fd );    /* buffer received via UDP */
static int cb_newsess( void *data, int handle, char *from );		/* new connection */
static int cb_disc( void *data, int handle );				/* session blew away */

/* general manipulation functions */
static int eat_msg( Session_t *s, char *msg );
static Session_t *get_session( int handle, char *name );
static void deref_session( int handle, char *name );
static void add_session( Session_t *s, int handle, char *name );
static Work_t *new_work( int type, char **strtok_p );
static Work_t *new_unparsed_work( char *to, int fd, int type, char *msg );
/*static void queue_work( int qname, Work_t *w );*/
static void add2partial( char *partial, int plen, char *msg, int mlen );
static void disc_work( int handle );


/* initialise and get lost.  Used when not creating a thread to do network I/O in */
void *port_init( char *port )
{
	extern 	int ng_sierrno;
	extern	int ok2run;

	int	init_tries = 10;	/* number of tires we give the network when it claims our port is in use */
	

	sleep( 2 );

	while( ng_siinit( SI_OPT_FG, atoi( port ), atoi( port ) ) != 0 )		/* initialise the network interface */
	{
		init_tries--;
		if( init_tries <= 0  ||  errno != EADDRINUSE )
		{
			ng_bleat( 0, "port_listener: unable to initialise the network interface sierr=%d err=%d", ng_sierrno, errno );
			ok2run = 0;			/* cause other threads to abort too */
			exit( 1 );
		}	

		ng_bleat( 0, "address in use (%s): napping 15s before trying again", port );
		sleep( 15 );
	}

	wbuf = (char *) ng_malloc( NG_BUFFER, "port_listener: work buffer" );	/* general working buffer */

					/* we use our OWN table so not to have to lock a sharred one */
	symbols = syminit( 127 );	/* should be a nice size for our small private idaho */

								/* register the routines driven by ng_siwait() when events happen */
	ng_sicbreg( SI_CB_CONN, &cb_newsess, NULL );   		/* called when connection is received */
	ng_sicbreg( SI_CB_DISC, &cb_disc, NULL );    		/* disconnection */
	ng_sicbreg( SI_CB_CDATA, &cb_tcpdata, (void *) NULL );	/* cooked (tcp) data recieved */
	ng_sicbreg( SI_CB_RDATA, &cb_udpdata, (void *) NULL );	/* raw (udp) data received */

	return;
}
/* ----------------------------------------------------------------------------
	Main entry for the network interface thread
	initialises the network interface (ng_si)
	registers our callback routines
	invokes ng_siwait() to listen to network and drive our callbacks

	*data is assumed to be a character pointer to our port number string.
   ---------------------------------------------------------------------------- 
*/
void *port_listener( void *data )
{
	extern 	int ng_sierrno;
	extern	int ok2run;

	char	*port;			/* port string - converted from void pointer */
	int	init_tries = 10;
	
	port = (char *) data;		/* we will open this on both tcp and udp */

#ifdef KEEP
	sleep( 2 );

	while( ng_siinit( SI_OPT_FG, atoi( port ), atoi( port ) ) != 0 )		/* initialise the network interface */
	{
		init_tries--;
		if( init_tries <= 0  ||  errno != EADDRINUSE )
		{
			ng_bleat( 0, "port_listener: unable to initialise the network interface sierr=%d err=%d", ng_sierrno, errno );
			ok2run = 0;			/* cause other threads to abort too */
			exit( 1 );
		}	

		ng_bleat( 0, "address in use (%s): napping 15s before trying again", port );
		sleep( 15 );
	}

	wbuf = (char *) ng_malloc( NG_BUFFER, "port_listener: work buffer" );	/* general working buffer */

	symbols = syminit( 127 );	/* should be a nice size for our small private idaho */

						/* register the routines driven by ng_siwait() whene events happen */
	ng_sicbreg( SI_CB_CONN, &cb_newsess, NULL );   		/* called when connection is received */
	ng_sicbreg( SI_CB_DISC, &cb_disc, NULL );    		/* disconnection */
	ng_sicbreg( SI_CB_CDATA, &cb_tcpdata, (void *) NULL );	/* cooked (tcp) data recieved */
	ng_sicbreg( SI_CB_RDATA, &cb_udpdata, (void *) NULL );	/* raw (udp) data received */
#endif

	port_init( port );

	errno = 0;
	ng_siwait( );				/* loops waiting for input and drives our callback; returns when net shuts */
	ng_bleat( 1, "port_listener: ng_siwait finished: sierr=%d err=%d", ng_sierrno, errno );
	ok2run = 0;				/* cause other threads to panic and leave now that net has gone away */

	return;
}

/* ------- callback routines driven at various times by ng_siwait( ) ---------------- */

/* called when we receve a connection request 
  	- create session block
	- register it by the handle - must wait for a message to register name
*/
static int cb_newsess( void *data, int handle, char *from )
{
	Session_t	*s;

	s = (Session_t *) ng_malloc( sizeof( Session_t ), "cb_newsess: session mgt block" );
	memset( s, 0, sizeof( Session_t ) );

	s->handle = handle;

	sfsprintf( s->hname, sizeof( s->hname ), "%d", handle );
	add_session( s, handle, NULL );				/* install pointer to session block in the symtab */

	ng_bleat( 4, "session connection establshed from: %s (%d)", from, handle );

	return 0;
}

/* called when someone terminates a session 
	dereference the session block and free memory
*/
static int cb_disc( void *data, int handle )
{
	Session_t	*s;

	if( (s = get_session( handle, NULL )) )
	{
		ng_bleat( 4, "seesion disconnected: %d name=%s", handle, s->name ? s->name : "unnamed" );
		deref_session( handle, s->name );		/* trash both symtab references to this block */
		if( s->name )
			ng_free( s->name );
		ng_free( s );
	}
	else
		ng_bleat( 0, "disc: could not find session for handle: %d", handle );

	disc_work( handle );			/* remove references to this handle from all work blocks to indicate disco */
	return 0;
}


/* called when data received on TCP session
  we assume that complete messages are newline terminated and may be
  broken up across multiple TCP/IP datagrams. Partial messages 
  are queued on the session block until the whole message is received. 
*/
static int cb_tcpdata( void *data, int handle, char *buf, int len )
{
	Session_t *s;
	char 	*msg;		/* at the start of the message in the buffer */
	char	*mend;		/* at the end of the current message in the buffer */
	char	*end;		/* end of the buffer */
	long	mcount = 0;	/* msg contained in the tcp buffer */
	int	rc = SI_RET_OK;	/* return code in case eat msg wants to quit */

	end = buf + len;	/* mark end */
	mend = buf;		/* start searching with the beginning of the buffer */

	if( (s = get_session( handle, NULL )) )
	{
		while( mend < end )
		{
			mcount++;
			msg = mend;			/* start looking where we left off */
			if( s->partial[0] == 0 )							/* only if nothing queued */
				for( ; msg < end && (*msg == ' ' || *msg == '\t'); msg++ );	/* skip leading white space */

			for( mend = msg; mend < end && *mend != '\n'; mend++ );			/* skip to end of message */

			if( mend < end  && *mend == '\n' )					/* complete message */	
			{
				*mend = 0;			/* make string */
				mend++;				/* where to start to dig next one */

				if( s->partial[0] )		/* last time in for this session we had a partial buffer */
				{
					if( (sizeof( s->partial ) - strlen( s->partial )) < (mend - msg) )	/* buffer too big */
					{
						s->partial[100] = 0;
						ng_bleat( 0, "s_netif: msg received from %s too big; ignored (%s....)", s->name ? s->name : "unnamed-session", s->partial );
						s->partial[0] = 0;
					}
					else
					{
						add2partial( s->partial, sizeof( s->partial ), msg, mend-msg );
						rc = eat_msg( s, s->partial );			/* eat the reconstruction */
					}
				}
				else						/* message is complete as is */
					rc = eat_msg( s, msg );			/* deal with it */

				s->partial[0] = 0;		/* nothing partial any more */
			}
			else					/* partital message -- assuming we reached the buffer end */
			{
				add2partial( s->partial, sizeof( s->partial ), msg, mend-msg );		/* tack it into the hold buffer */
			}
		}

		ng_bleat( 4, "cb_tcpdata: tcp buffer contained %d messages %s (sid=%d)", mcount, s->partial[0] ? "incomplete" : "complete", handle );
	}
	else
		ng_bleat( 0, "cb_tcpdata: message received for non-existant session: %d(sid)", handle );


	return rc;
}


/* 
	Original version used UDP for commands. This shifted to TCP sessions in order
	to guarentee delivery of all output when dump/explain commands were introduced.
	UDP messages are logged and ignored. 
*/
static int cb_udpdata( void *data, char *buf, int len, char *from, int handle )
{
	extern int ok2run;	/* from main */
	extern int ng_sierrno;
	int stat;

	Work_t *work;				/* work block to add to queue */
	int	timeout = MAX_UDP_POLL;		/* max time to wait - large on first try, 0 after that */
	int	rc = 3;				/* return value - set to SI_RET_QUIT when we want to shut off */
	char	*strtok_p;			/* cache for strtok_r */
	char	*tok;				/* for parsing stuff */
	char	*msg;				/* pointer to the message to use (may become a copy) */
	


	if( buf[len-1] < ' ' )		/* we depend on this buffer being a string; ensure it is */
	{
		buf[len-1] = 0;			/* make a string and just point msg at it - faster */
		msg = buf;
		ng_bleat( 1, "WARNING: udpdata: got %d bytes; msg ignored: (%s)", len, msg );		
	}
	else
		ng_bleat( 1, "WARNING: udpdata: got %d bytes; msg ignored: (unformated data)", len );		

	return SI_RET_OK;
}

/* ----------------- general support routines -------------------------- */

/* adds msg to the paritally filled buffer */
static void add2partial( char *partial, int plen, char *msg, int mlen )
{
	int  nend;			/* new end of message */

	if( partial == NULL || msg == NULL )	
		return;
		
	nend = mlen + strlen( partial );
	if( (strlen( partial ) + mlen  + 1) < plen )		/* room? */
	{
		memcpy( partial + strlen( partial ), msg, mlen );		/* tack on the message then deal with it */
		partial[nend] = 0;
	}
	else
	{
		msg[mlen-1] = 0;
		ng_bleat( 0, "cb_tcpdata:buffer overrun filling partial: %.100s(had...) %s(got)", partial, msg );
		*partial = 0;				/* trash the bufer */
	}
}

/*
	driven when a complete message has been constructed from TCP datagrams
	expected messages are assumed to be strings and have a id=value 
	syntax. All messages that do not have an id=value syntax are assumed
	to be some form of job submission record and are passed to the 
	main thread via a work block.  

	As support for the original UDP messages is added to allow the commands to 
	come in from TCP sessions, and to support an expected repmgr interface, 
	keyword value pairs may be separated by colons in addition to the equal sign.

	sreq validates the list of commands (mostly the command:stuff things listed 
	below.  If a new command is added, and is to be supported by sreq, then 
	sreq must be modified. Unless deleting a command, it is generally safe to 
	modify sreq and ship it early to ensure that the correct version is loaded. 

	Currently supported id=value records are:
		Added to the normal work queue:
		name=<session-name>			(name the session)
		gar=<gar-name>:<command tokens>		(schedules work: supply named gar command)
		purge=<jobname>				(schedules work: causes a job to be purged )
		dump=files				(causes a dump of files that are needed for jobs)
		dump=costs				(causes a dump of job type costs by node to tcp session)
		ijob=<job-record>			(job record, but not all input/output files are listed; added to incomplete queue)
		input=<jobname>:			(add input to incomplete job)
		inputig=<jobname>:			(add input with ignore size flag to incomplete job)
		output=<jobname>:			(add output to incomplete job)
		user=<jobname>:				(add user info to incomplete job)
		cjob=<jobname>				(job description is complete, it may be moved to reschedule queue)
		dictum=<text>				(comment written as an audit remark to the master)
		jstatus=<status-info>			(status info from s_eoj)

		These messages are placed on the priority work queue:
		dump:value				(wallace some information about our state)
		explain:name[:name...]			(say something about name -- job, resource or queue)
		extend:jobname:value			(extend the lease, value of 0 kills the job and puts on pending queue)
		hold:job-name				(put a job on hold)
		limit:name:value			(set a resource limit)
		load:name:value				(set desired node load)
		maxlease:seconds			(change the value used as the max lease; must be > 1800)
		maxwork:n				(max number of workblocks allowed to be processed per pass)
		mpn:name:value				(set max per node limit for name (resource))
		nattr:node:string			(node attributes)
		node:jobname:nodename			(hard assignment of the job -- if pending)
		pause:value				(toggle suspend/resume)
		push:jobname				(push job to head of pending queue)
		release:type:name			(types supported: resource, job)
		resume:value				(allow jobs to run)
		stable:1|0				(files must be stable to run job if set to 1)
		suspend:all|node-name			(suspend job starts)
		xit					(shutdown seneschal)

	ijob,input/output,cjob commands were added as jobs with a long list of files could not easily be 
	described in one message.

	Most things cause a work block to be created and queued for main.  Somethings, like flag 
	changes, are done here. If we ack the receipt of the command straight away, then we do not save
	the session info in the work block as no additional output is expected to be sent to the 
	process at the other end. When sending an immediate ack, we must remember to send the end of 
	data flag so that user knows not to expect anything else. 
*/
static int eat_msg( Session_t *s, char *msg )
{
	extern	 char *version;

	Work_t	*work;				/* work block to be queued as a result of the message */
	char 	*strtok_p;
	char	*tok;				/* pointer at token in the message if we process locally */
	int	v;				/* temp holder for new verbose value */
	int 	rc = SI_RET_OK;			/* set to quit if we get exit message */


	if( s->flags & SF_RM_SESS )		/* if this is from repmgr, we dont chew, pass to repmgr if function to deal with */
	{
		ng_bleat( 4, "eat_msg: repmgr message: %s", msg );
		work = new_unparsed_work( s->name, s->handle, RMBUF, msg );
		queue_work( NORMAL_Q, work );
		return SI_RET_OK;
	}

	ng_bleat( 4, "eat_msg: %s", msg );
	/* if we process the command then we short circuit and return - one catchall error msg if we fall out of case */
	switch( *msg )				/* assume compiler will build a jump table to make this a bit more efficent */
	{
		case 'b':
			if( strncmp( msg, "bid:", 4 ) == 0 )
			{	
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( BID, &strtok_p );
				if( work && work->value >= 0 )
				{
					queue_work( PRIORITY_Q, work );
				}
				else
					ng_siprintf( s->handle, "seneschal: missing value on bid message\n" );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			break;

		case 'c':
			if( strncmp( msg, "cjob=", 5 ) == 0 )			/* indicates all data for job has been sent */
			{
				ng_bleat( 3, "eat_msg: cjob received: %s", msg+5 );
				work = new_unparsed_work( s->name, s->handle, CJOB, msg+5 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'd':
			if( strncmp( msg, "dump=files", 10 ) == 0 )	
			{
				work = new_unparsed_work( NULL, s->handle, LISTFILES, NULL );
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "dump:", 5 ) == 0 )		/* assume dump:value */
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_unparsed_work( NULL, s->handle, DUMP, msg+5 );
				work->to = NULL;
				work->fd = s->handle;
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			else						
			if( strncmp( msg, "dump=costs", 10 ) == 0 )		
			{
				work = new_unparsed_work( NULL, s->handle, NODECOSTS, NULL );
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "dictum=", 7 ) == 0 )			/* add an audit comment to the master log */
			{
				ng_log( LOG_INFO, "%s %s %s", AUDIT_ID, AUDIT_DICTUM, msg+7 );
				ng_sisendt( s->handle, ".\n", 3 );
				return rc;
			}
			break;

		case 'e':
			if( strncmp( msg, "explain:", 8 ) == 0 )			
			{
				work = new_unparsed_work( NULL, s->handle, EXPLAIN, msg+8 );
				work->fd = s->handle;
				work->to = NULL;
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "extend:", 7 ) == 0 )			
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( EXTEND, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			break;

		case 'f':
			if( strncmp( msg, "ftrace:", 7 ) == 0 )			/* ftrace:filename */
			{
				strtok_r( msg, ":", &strtok_p );
				tok = strtok_r( NULL, ":", &strtok_p );
				if( tok && *tok )
				{
					if( file_trace_toggle( tok ) )
						ng_siprintf( s->handle, "file trace toggled for: %s\n.\n", tok );
					else
						ng_siprintf( s->handle, "file trace: file not known: %s\n.\n", tok );
				}
				return rc;
			}
			else
			if( strncmp( msg, "file ", 5 ) == 0 )			/* response from dump1 command -- from agent */
			{
				ng_bleat( 4, "eat_msg: repmgr message: %s", msg );
				work = new_unparsed_work( s->name, s->handle, RMBUF, msg );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'g':
			if( strncmp( msg, "gar=", 4 ) == 0 )
			{
				ng_bleat( 1, "gar received: %s", msg );
				work = new_unparsed_work( NULL, s->handle, GAR, msg+4 );		
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'h':
			if( strncmp( msg, "hold", 4 ) == 0 )			/* hold:job-name from sreq;  hold=jobname from nawab*/
			{
				if( *(msg+4) == ':' )
					ng_siprintf( s->handle, "job now on hold: %s\n.\n", msg+5  );	/* remember end of data */
				tok = strtok_r( msg, ":=", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( OBSTRUCT, &strtok_p );
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			break;
	
		case 'i':
			if( strncmp( msg, "ijob=", 5 ) == 0 )			/* accept an incomplete job */
			{
				ng_bleat( 3, "eat_msg: ijob record (partial job data, files to follow); work queued for: (%s)", msg+5 );
				work = new_unparsed_work( s->name, s->handle, IJOB, msg+5 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "input=", 6 ) == 0 )			/* add an input file to the list */
			{
				work = new_unparsed_work( NULL, s->handle, INPUT, msg+6 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "inputig=", 8 ) == 0 )			/* add an size ignored input file to the list */
			{
				work = new_unparsed_work( NULL, s->handle, INPUTIG, msg+8 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'j':
			if( strncmp( msg, "jstatus=", 7 ) == 0 )
			{
				strtok_r( msg, "=", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( STATUS, &strtok_p );
				work->fd = -1;					/* we dont want to keep since we ack right away */
				work->to = NULL;
				if( work && work->value >= 0 )
					queue_work( STATUS_Q, work );
				else
					ng_bleat( 0, "udpdata: missing value on job status message from: %s", s->name ? s->name : "unknown" );
		
				ng_siprintf( s->handle, "status received\n.\n" );	/* confirm and send end of transmission */
				return rc;
			}
			else
			if( strncmp( msg, "job=", 4 ) == 0 )
			{
				ng_bleat( 3, "eat_msg: job record; work queued for: (%s)", msg+4 );
				work = new_unparsed_work( s->name, s->handle, JOB,  msg+4 );
				queue_work( NORMAL_Q, work );	
				return rc;
			}
			break;
		
		case 'l':
			if( strncmp( msg, "limit:", 6 ) == 0 )
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( LIMIT, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "load:", 5 ) == 0 )
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( LOAD, &strtok_p );
				if( work )
				{
					if( work->value < 0 )
						work->value = 0;
					queue_work( PRIORITY_Q, work );
				}
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			break;


		case 'm':	
			if( strncmp( msg, "mpn:", 4 ) == 0 )
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( MPN, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "maxlease:", 9 ) == 0 )			/* set the lid for lease times */
			{
				v = atoi( msg+9 );
				if( v > 1800 )
				{
					max_lease_sec = v * 10;		/* convert to tenths of seconds */
					ng_bleat( 0, "max_lease changed to: %d", max_lease_sec );
					ng_siprintf( s->handle, "max lease time accepted\n.\n" );	/* remember end of data */
				}	
				else
					ng_siprintf( s->handle, "max lease time invalid (<1800), not changed\n.\n" );	/* remember end of data */
				return rc;
			}
			else
			if( strncmp( msg, "maxwork:", 8 ) == 0 )			/* new max number of work blocks worked on per pass */
			{
				v = atoi( msg+8 );
				if( v > 20000 )
					v = 20000;			/* enforce some sanity */
				else
					if( v < 25 )			/* silly, but must have some work go through */
						v = 25;
				max_work = v;
				ng_siprintf( s->handle, "max work limit accepted: %d\n.\n", max_work );	/* remember end of data */
				return rc;
			}
			else
			if( strncmp( msg, "maxresort:", 8 ) == 0 )			/* new max number of work blocks worked on per pass */
			{
				v = atoi( msg+8 );
				if( v >= 100 && v <= 5000 )
				{
					max_resort = v;
					ng_siprintf( s->handle, "max resort limit accepted: %d\n.\n", max_resort );	/* remember end of data */
				}
				else
					ng_siprintf( s->handle, "max resort limit out of range: it must be 100 <= m <= 5000\n.\n" );
			}
			break;
		
		case 'n':
			if( strncmp( msg, "node:", 5 ) == 0 )
			{
				work = new_unparsed_work( NULL, s->handle, NODE, msg+5 );
				work->fd = -1;				/* we dont want to keep if we ack right away */
				work->to = NULL;
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* ack right away */
				return rc;
			}
			else
			if( strncmp( msg, "name=", 5 ) == 0 )		/* no work, this is a local (netif) command */
			{
				if( s->name )					/* allow sender to rename themselves */
				{
					deref_session( NO_HANDLE, s->name );		/* trash pointer to this block by old name */
					ng_free( s->name );
				}
		
				s->name = ng_strdup( msg+5 );
				ng_bleat( 3, "naming session: %s", s->name );
				add_session( s, NO_HANDLE, s->name );
				return rc;
			}
			else
			if( strncmp( msg, "nattr:", 6 ) == 0 )		
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( NATTR, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, "attribute accepted\n.\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "not found:", 10 ) == 0 )			/* not found message for watched file */
				return rc;					/* for now we ignore this */
			break;

		case 'o':
			if( strncmp( msg, "output=", 7 ) == 0 )			/* add an output file to the list */
			{
				work = new_unparsed_work( NULL, s->handle, OUTPUT, msg+7 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'p':
			if( strncmp( msg, "purge", 5 ) == 0 )		/* purge a job comes purge= from nawab, purge: from sreq*/
			{
				if( *(msg+5) == ':' )
					ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
		
				work = new_unparsed_work( NULL, s->handle, DELETE, msg+6 );
				queue_work( PRIORITY_Q, work );
				return rc;
			}
			else
			if( strncmp( msg, "push:", 5 ) == 0 )			
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( PUSH, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "pause:", 6 ) == 0 )	
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( PAUSE, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, "seneschal: pause accepted, state will change to: %s\n.\n", suspended? "RUNNING" : "SUSPENDED"  );
				return rc;
			}
			else
			if( strncmp( msg, "ping:", 5) == 0 )			/* cause immediate output to our log */
			{
				ng_bleat( 4, "%s", msg );
				ng_siprintf( s->handle, "seneschal %s: Is anybody there? I could swear Im not alone (Midnight Oil)\n.\n", version  );
				return rc;
			}
			break;
		
		case 'q':
			if( strncmp( msg, "qsize:", 6 ) == 0 )	
			{
				gflags ^= GF_QSIZE;
				ng_bleat( 0, "queue jobs based on size  changed to: %s", gflags & GF_QSIZE ? "ON" : "OFF" );
				ng_siprintf( s->handle, "seneschal: queue jobs based on size changed to: %s\n.\n", gflags & GF_QSIZE ? "ON" : "OFF" );
				return rc;
			}
			break;

		case 'r':
			if( strncmp( msg, "run:", 4 ) == 0 )		
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( RUN, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "resume:", 7 ) == 0 )			
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( SUSPEND, &strtok_p );		/* this works based on value; suspend action does both sus and resume */
				work->value = 0;
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "release:", 8 ) == 0 )			/* release:type:name */
			{
				work = new_unparsed_work( NULL, s->handle, RELEASE, msg+8 );
				queue_work( PRIORITY_Q, work );				/* when we parse the work we send msgs and the .\n */
				return rc;
			}
			else
			if( strncmp( msg, "rmif:", 5 ) == 0 )		/* rmif parms:  rmif:max-request:frequency-seconds */
			{
				strtok_r( msg, ":", &strtok_p );		/* skip the command */
				tok = strtok_r( NULL, ":", &strtok_p );		/* point at value */
				if( *tok == '0' )				/* just query if we see this */
					ng_siprintf( s->handle, "rmif values: batch size=%d  freq=%ds\n.\n", max_rm_req, freq_rm_req/10 );	
				else
				{
					if( (max_rm_req = atoi( tok )) < 100 )
						max_rm_req = 100;			/* keep them honest if not sane */
					else
						if( max_rm_req > 30000 )
							max_rm_req = 30000;		/* we have a hard time reading more than 15k/min so be safe */
					if( tok = strtok_r( NULL, ":", &strtok_p ) )
						if( (freq_rm_req = atoi( tok ) * 10) < 150 )		/* remember to convert to tenths */
							freq_rm_req = 150;
					ng_siprintf( s->handle, "rmif values changed: batch size=%d  freq=%ds\n.\n", max_rm_req, freq_rm_req/10 );	
				}
				return rc;
			}
			break;

		case 's':
			if( strncmp( msg, "suspend:", 4 ) == 0 )			
			{
				strtok_r( msg, ":", &strtok_p );		/* setup for new_work to get next token in the buffer */
				work = new_work( SUSPEND, &strtok_p );
				queue_work( PRIORITY_Q, work );
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			else
			if( strncmp( msg, "stable:", 7 ) == 0 )				/* set/reset the stable flag */
			{
				if( *(msg+7) == '0' || *(msg+8) == 'f' )		/* turn off */
					gflags &= ~GF_MUSTBSTABLE;
				else
					gflags |= GF_MUSTBSTABLE;
		
				ng_bleat( 0, "stablity changed to: %s", gflags & GF_MUSTBSTABLE ? "ON" : "OFF" );
				ng_siprintf( s->handle, "seneschal: stability changed to: %s\n.\n", gflags & GF_MUSTBSTABLE ? "ON" : "OFF" );	/* remember end of data */
				return rc;
			}
			else
			if( strncmp( msg, "symstats:", 9 ) == 0 )
			{
				symstat( symbols, sfstderr );
			}
			break;

		case 'u':
			if( strncmp( msg, "user=", 5 ) == 0 )			/* user data for an incomplete job */
			{
				ng_bleat( 3, "eat_msg: user info; work queued for: (%s)", msg+5 );
				work = new_unparsed_work( s->name, s->handle, USER, msg+5 );
				queue_work( NORMAL_Q, work );
				return rc;
			}
			break;

		case 'v':
			if( strncmp( msg, "verbose:", 8 ) == 0 )			/* add an audit comment to the master log */
			{
				strtok_r( msg, ":", &strtok_p );		/* skip command */
				tok = strtok_r( NULL, ":", &strtok_p );		/* point at value */
				if( (v = atoi( tok )) >= 0 )
				{
					verbose = v;
					ng_siprintf( s->handle, "seneschal: version %s verbose changed to: %d\n", version, verbose );
					ng_bleat( 0, "seneschal: version %s verbose changed to: %d", version, verbose );
				}
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			break;

		case 'x':
			if( strncmp( msg, "xit", 3 ) == 0 )			
			{
				ng_bleat( 0, "received exit message" );
				rc = SI_RET_QUIT;			/* cause network interface to shut off */
				ng_siprintf( s->handle, ".\n" );	/* from sreq we need to send an end of message flag */
				return rc;
			}
			break;
			
		default:
			break;
	}


	
	ng_bleat( 0, "eat_msg: unrecognised command: %s", msg );
	ng_siprintf( s->handle, "seneschal %s: unrecognised command: %s\n.\n", version, msg );	
	return rc;
}

/*	send status message (buf) back to the job owner (name) 
	we dont queue things that dont have an owner as the owner might log in and resubmit 
	the job... we will just send the first status back to them
	We actully use this to send more than just stats message.
*/
void send_stats( char *name, char *buf )
{
	Session_t 	*s;

	if( name == NULL || *name == 0 )			/* if sender did not send a name; they lose */
		return;

	if( (s = get_session( NO_HANDLE, name )) )
		ng_sisendt( s->handle, buf, strlen( buf ) );
}

/* add a session block to the hash table; sessions are referenced by their 'handle'
   and their name if it is supplied by the other end on a name= message
 */
static void add_session( Session_t *s, int handle, char *name )
{
	if( s )
	{
		if( name )
			 symlook( symbols, name, BY_NAME, s, SYM_NOFLAGS );

		if( handle != NO_HANDLE )	
		{
			sfsprintf( s->hname, sizeof( s->hname ), "%d", handle );
			symlook( symbols, s->hname, BY_HANDLE, s, SYM_NOFLAGS );
		}
	}
}

/* dereference a session block from the hash table */
static void deref_session( int handle, char *name )
{
	Session_t *s;
	char	buf[100];

	ng_bleat( 4, "deref_session: name=%s sid=%d", name ? name : "unnamed", handle );

	if( name )
		symdel( symbols, name, BY_NAME );

	if( handle != NO_HANDLE )
	{
		sfsprintf( buf, sizeof( buf ), "%d", handle );
		symdel( symbols, buf, BY_HANDLE );
	}
}

/* find the session block based on either name or handle */
static Session_t *get_session( int handle, char *name )
{
	Session_t	*s = NULL;
	Syment	*se;
	char	buf[100];

	if( name )
	{
		if( (se = symlook( symbols, name, BY_NAME, NULL, SYM_NOFLAGS )) )
			return (Session_t *) se->value;
	}
	else
	{
		sfsprintf( buf, sizeof( buf ), "%d", handle );
		if( (se = symlook( symbols, buf, BY_HANDLE, NULL, SYM_NOFLAGS )) )
			return (Session_t *) se->value;
	}
	
	return NULL;
}

/* -------------------- repmgr session management stuff ---------------------------- */
/* return 1 if we successfully connect. If we have a session with repmgr we return 0
   as we do not want to overlap bursts of messages as repmgr is single threaded 
   with networking stuff 
*/
int conn2_rm( )
{
        Session_t *s;                   /* session block */

        int     fd;                     /* session file descritor if successful */
        char    *rm_host;             	/* host repmgr lives on */
	char	*rm_port;		/* well knonw port for repmgr (not rmbt) */
	char	rm_address[1024];	/* address string host:port */
	ng_timetype start = 0;		/* time connection request sent (concerned about blocking too long) */
        
	if( (s = get_session( NO_HANDLE, "repmgr" )) != NULL )
		return -1;						/* one already connected */

        if( (rm_port = ng_env( "NG_REPMGRS_PORT" )) == NULL )          /* get his well known (direct) port (not rmbt's port) */
        {
                ng_bleat( 0, "conn2_rm: cannot find repmgr port (NG_REPMGRS_PORT) in environment/cartulary" );
		return -1;
        }

        if( (rm_host = ng_pp_get( "srv_Filer", NULL )) == NULL || !*rm_host )    /*get each time we connect; allows repmgr to move */
        {
		if( rm_host )
			ng_free( rm_host );					/* if an empty string returned */
		ng_bleat( 0, "conn2_rm: cannot determine srv_Filer host" );
		return -1;
        }
        
        sfsprintf( rm_address, sizeof( rm_address ), "%s:%s", rm_host, rm_port );
	start = ng_now( );
        if( (fd = ng_siconnect( rm_address )) >= 0 )
        {
		start = ng_now( ) - start; 
                ng_bleat( 3, "conn2_rm: session established with repmgr (%s) on fd %d took %I*dts", rm_address, fd, Ii( start ) );
                s = (Session_t *) ng_malloc( sizeof( Session_t ), "conn2_rm: session mgt block" );
                memset( s, 0, sizeof( Session_t ) );

                s->handle = fd;
                sfsprintf( s->hname, sizeof( s->hname ), "%d", fd );
		s->flags = SF_RM_SESS;
		s->name = ng_strdup( "repmgr" );
                add_session( s, fd, s->name );                              /* install pointer to session block in the symtab */
        }
        else
        {
                ng_bleat( 4, "conn2_rm: connection failed: %s:  %s", rm_address, strerror( errno ) );
		return -1;
        }

	ng_free( rm_port );
	ng_free( rm_host );
	return fd;
}

/* try to connect to the d1 agent for remgr rather than repmgr itself. this is preferred if the agent is running as
   the agent will watch our files and send us only info that has changed.  this reduces our workload by a large amount
   when lots of files are needed. 
*/
int conn2_d1agent( )
{
	Session_t *s;
	char	*aport = NULL;				/* port number for agent */
	char	*host = NULL;				/* host name srv_D1agent is currently on */
	char	addr[1024];
	int	fd;

	if( (s = get_session( NO_HANDLE, "d1agent" )) )			/* agent session is left up, so it should ususlly be there */
		return s->handle;

        if( (aport = ng_env( "NG_D1AGENT_PORT" )) == NULL )          /* get his well known port */
        {
                ng_bleat( 3, "conn2_agent: cannot find d1 agent port (NG_D1AGENT_PORT) in environment/cartulary" );
		return -1;
        }

        if( (host = ng_pp_get( "srv_D1agent", NULL )) == NULL || !*host )    /*get each time we connect; allows repmgr to move */
        {
		if( host )
			ng_free( host );					/* if an empty string returned */
		ng_bleat( 3, "conn2_agent: cannot determine srv_D1agent host" );
		return -1;
        }
        
        sfsprintf( addr, sizeof( addr ), "%s:%s", host, aport );
        if( (fd = ng_siconnect( addr )) >= 0 )
        {
		ng_siprintf( fd, "name seneschal\n" );
                ng_bleat( 1, "conn2_agent: session established with d1agent (%s) on fd %d", addr, fd );
                s = (Session_t *) ng_malloc( sizeof( Session_t ), "conn2_agent: session mgt block" );
                memset( s, 0, sizeof( Session_t ) );

                s->handle = fd;
                sfsprintf( s->hname, sizeof( s->hname ), "%d", fd );
		s->name = ng_strdup( "d1agent" );
                add_session( s, fd, s->name );                              /* install pointer to session block in the symtab */

		ng_siprintf( fd, "lease 3600\n" ); 
        }
        else
        {
                ng_bleat( 4, "conn2_agent: connection failed: %s:  %s", addr, strerror( errno ) );
		return -1;
        }

	ng_free( aport );
	ng_free( host );
	return fd;
}

/* find the repmgr session and close it */
void drop_rm( )
{
	Session_t	*s;

	if( (s = get_session( NO_HANDLE, "repmgr" )) != NULL )
	{
		ng_siclose( s->handle ); 
		ng_bleat( 3, "closing repmgr session" );
		deref_session( s->handle, "repmgr" );		/* drop both references to s */
		ng_free( s->name );
		ng_free( s );
	}
}

/* ------------------- work queue management functions ----------------------------- */
void load_free_work( int n )
{
	int	i;
	Work_t	*wp;

	ng_bleat( 1, "preloading free work queue begins" );
	for( i = 0; i < n; i ++ )
	{
		wp = ng_malloc( sizeof( Work_t ), "port_listener: load free work queue" );
		wp->next = free_work;
		free_work = wp;
	}
	ng_bleat( 1, "preloading free work queue complete" );
}

/* 
	original senschal commands were simple and all fit a single format
	that could be parsed here and a work block built. this routine does
	that. for input that is more complex, new_unparsed_work() is used 
	to create a workblock that contains a buffer that the main routine(s)
	 must decypher. 

	create a new work block from input. input syntax expected to be
   		<cmd-string>[:string][:value:[misc-string]]
	
   	if misc-string is missing, then the misc string is set to the value string
   	(allows name=2h to be recognised for limits)
   	if value is omitted -1 is used as default
*/
static Work_t *new_work( int type, char **strtok_p )
{
	Work_t	*work = NULL;
	char	*tok;
	char	*val_str;

	if( free_work )
	{
		work = free_work;
		free_work = work->next;
	}
	else
		work = ng_malloc( sizeof( Work_t ), "port_listener: new_work" );

	memset( work, 0, sizeof( Work_t ) );

	work->next = NULL;
	work->type = type;
	work->value = -1;
	work->misc = NULL;
	work->to = NULL;
	work->dyn_buf = NULL;			/* dynamic buf if data too large for static */
	work->fd = -1;

	if( tok = strtok_r( NULL, ":", strtok_p ) )   			/* caller must set strtok_p up for us, skipping cmd: */
	{
		work->string = strdup( tok );				/* point at first of three possible tokens */

		if( (tok = strtok_r( NULL, ":", strtok_p)) )		/* @  value token */
		{
			int used;

			val_str = tok;					/* hold if we need to dup later */
			work->value = atoi( tok );

			work->misc = work->static_buf;
			if( tok = strtok_r( NULL, "", strtok_p ) )
				used = sfsprintf( work->static_buf, sizeof( work->static_buf ), "%s", tok ); 
			else
				used = sfsprintf( work->static_buf, sizeof( work->static_buf ), "%s", val_str ); 

			if( used  >= sizeof( work->static_buf ) )		/* buffer larger than our static size */
			{
				ng_bleat( 2, "allocated dynamic work buffer for large data: type=%d", type );
				if( tok = strtok_r( NULL, "", strtok_p ) )
					work->misc = work->dyn_buf = ng_strdup( tok );
				else
					work->misc = work->dyn_buf = ng_strdup( val_str );
			}
			/*	work->static_buf[sizeof(work->static_buf)-1] = 0;*/
		}
	}
	
	return work;
}

/* create a work block with unparsed data in misc and nothing in string */
static Work_t *new_unparsed_work( char *to, int fd, int type, char *msg )
{
	Work_t	*work = NULL;
	char	*tok;

	if( free_work )
	{
		work = free_work;
		free_work = work->next;
	}
	else
		work = ng_malloc( sizeof( Work_t ), "port_listener: new_unparsed_work" );

	memset( work, 0, sizeof( Work_t ) );

	work->next = NULL;
	work->type = type;
	work->value = -1;

	if( to )
		work->to = ng_strdup( to );			/* who to send status back to */
	else
		work->to = NULL;
	work->fd = fd;

/*
	work->misc = msg ? ng_strdup( msg ) : NULL;
*/
	if( msg )
	{
		int used;

		work->misc = work->static_buf;
		if( (used = sfsprintf( work->static_buf, sizeof( work->static_buf ), "%s", msg )) >= sizeof( work->static_buf ) )
			work->static_buf[sizeof(work->static_buf)-1] = 0;
	}

	return work;
}


/* should not have to lock the queue just to query it, but if it becomes necessary
   to do so in the future we will have a centralised place to do it from the start.
*/
long any_work( )
{
	return wq_size + pwq_size + swq_size > 0;
}

/* return 1 if number queued is greather than threshold */
long work_qsize( int qname )
{
	switch( qname )
	{
		case STATUS_Q:		
			return swq_size; 
			break;

		case PRIORITY_Q:	
			return pwq_size; 
			break;

		case NORMAL_Q:		
		default:		
			return wq_size; 
			break;
	}

	return 0;
}

/* lock work queue, return the next work block 
	the priority queue and the job status queue are given equal priority over 
	the regular work queue.  5 messages will be pulled from the pri/status queue
	for every one low priority message. 
*/
Work_t *get_work( )
{
	Work_t *w = NULL;
	static int lp_count = 5;			/* we service at least one low priority block for every 5 high priority ones */
	static int sq_flag = 0;				/* high priority alternates between pri queue and status queue */

#ifdef DO_THREADS
	pthread_mutex_lock( &work_mutex );		/* own it before the check to prevent it changing from under us */
#endif

	if( lp_count && (pwq || swq) )			/* if priority/status queue ! empty, and not time to service regular work queue */
	{
		lp_count--;
		sq_flag = !sq_flag;			/* toggle */

		if( swq && (sq_flag || !pwq) )		/* time for status and something there, or nothing on pri queue */
		{
			w = swq;
			swq = w->next;
			if( ! swq )
				swq_tail = NULL;
			swq_size--;
		}
		else
		{
			w = pwq;
			pwq = w->next;
			if( ! pwq )
				pwq_tail = NULL;
			pwq_size--;
		}
	}
	else
	{
		lp_count = 4;		/* reset regardless of low queue state */

		if( wq )
		{
			w = wq;
			wq = w->next;
			if( ! wq )
				wq_tail = NULL;
			wq_size--;
		}
		else					/* no low work, send back next high priority/status if there */
		{
			if( (w = swq) )			/* preference to status in this case */
			{
				swq = w->next;
				if( ! swq )
					swq_tail = NULL;
				swq_size--;
			}
			else
			if( (w = pwq) )
			{
				pwq = w->next;
				if( ! pwq )
					pwq_tail = NULL;
				pwq_size--;
			}
		}
	}

#ifdef DO_THREADS
	pthread_mutex_unlock( &work_mutex ); 
#endif

	return w;
}

/*
	generic function to add a work block to the requested 
	queue.  Adds to the tail and ajusts pointers
*/
void queue_work( int qname, Work_t *w )
{
	Work_t **qhead = NULL;
	Work_t **qtail = NULL;

	if( ! w )
		return;

#ifdef DO_THREADS
	pthread_mutex_lock( &work_mutex );
#endif
	switch( qname )
	{
		case STATUS_Q:
			qhead = &swq;
			qtail = &swq_tail;
			swq_size++;
			break;

		case PRIORITY_Q:
			qhead = &pwq;
			qtail = &pwq_tail;
			pwq_size++;
			break;

		default:
			ng_bleat( 0, "queue_work: invalid queue specified: %d", qname );
			/* just put it on the normal work queue */

		case NORMAL_Q:
			qhead = &wq;
			qtail = &wq_tail;
			wq_size++;
			break;
	}

	w->next = NULL;
	if( *qtail )
		(*qtail)->next = w;
	else
		*qhead = w;
	*qtail = w;

#ifdef DO_THREADS
	pthread_mutex_unlock( &work_mutex ); 
#endif
}

#ifdef KEEP
/* add a work block to the standard work queue (to the tail) */
void put_work( Work_t *w )
{

	if( !w )
		return;

	w->next = NULL;

#ifdef DO_THREADS
	pthread_mutex_lock( &work_mutex );
#endif
	
	if( wq_tail )
		wq_tail->next = w;
	else
		wq = w;			/* previously empty */	

	wq_tail = w;

#ifdef DO_THREADS
	pthread_mutex_unlock( &work_mutex );
#endif
}

/* add a work block to the priority work queue (to the tail) */
void put_priority_work( Work_t *w )
{

	if( !w )
		return;

	w->next = NULL;

#ifdef DO_THREADS
	pthread_mutex_lock( &work_mutex );
#endif
	
	if( pwq_tail )
		pwq_tail->next = w;
	else
		pwq = w;			/* previously empty */	

	pwq_tail = w;

#ifdef DO_THREADS
	pthread_mutex_unlock( &work_mutex );
#endif
}
#endif

/*	run the work blocks and reset any with the handle to -1 to indicate that session is gone 
	we cannot simply remove the work blocks because it is legit to send a bunch of work, 
	get an ack, and then disco.
	
*/
static void disc_work( int handle )
{
	Work_t *w = NULL;

#ifdef DO_THREADS
	pthread_mutex_lock( &work_mutex );		/* own it before the check to prevent it changing from under us */
#endif

	/* we must check the priority queue as these sessions expect something back and we need to prevent 
		sending the output for the disconnected session to someone else 
	*/
	for( w = pwq; w; w = w->next )		/* priority queue */
		if( w->fd == handle )
			w->fd = -1;

#ifdef BADMOVE
/* when weve got a lot to do, and are getting swampped with disconnects this is suicide */
	for( w = wq; w; w = w->next )		/* normal work queue */
		if( w->fd = handle )
			w->fd = -1;
#endif

#ifdef DO_THREADS
	pthread_mutex_unlock( &work_mutex ); 
#endif
}

void trash_work( Work_t *w )
{
	if( w->dyn_buf )
		ng_free( w->dyn_buf );		/* free dynamic buffer if allocated */
	w->dyn_buf = NULL;
	w->misc = NULL;

	if( w->string )
		ng_free( w->string );

	if( w->to )
		ng_free( w->to );

	w->next = free_work;			/* push back on free queue */
	free_work = w;
/*	ng_free( w );*/
}
