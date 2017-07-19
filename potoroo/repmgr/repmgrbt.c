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
--------------------------------------------------------------------------------------------
	Mnemonic:	repmgrbt -- threaded repmgr listen process
	Abstract:	This process listens on a wellknown port for TCP session requests
			When a request is received the data is read and then sent off to 
			replication manager via another port.  Repmgr is single threaded
			on its port, so when this process has data to send it must open
			write and close in order to release repmgr to do other things. 

			This process forks a child to do all of the real work. The parent 
			waits forever: until the child dies, or until the child sends a 
			signal (usr1) to the parent when an exit command has been received.  
			When an exit message is received, the child will close the listen port 
			but continue to drain any queued requests and will finish any sessions
			that are established.  The parent exits upon receipt of the signal 
			to allow the startup script the ability to immediately restart 
			another repmgrbt if needed.  

			The fork is done at the start of the process, rather than after the 
			exit message is received;  the threads are unable to continue
			to read from existing sessions after the fork.  

			Replay Cache
			Version 2 added a replay cache. Buffers that have been sent to 
			replication manager are now cached and a command interface allows
			them to be resent to repmgr (replayed).  Repmgr is expected to send
			marker commands which mark the cache replay locations.  When a 
			replay request is received, it specifies a marker name and all 
			buffers from that marker to the end of the cache are resent.  
			Buffers in the replay cache between markers X and Y are regarded
			as the cache set X. We keep just 5 sets in the replay cache. 

			Service Management of repmgrbt
			This process can be executed via service manager.  It must be 
			rememberd that this process does NOT have access to narbalek
			and/or pinkpages. In order to implement this under service 
			manager, the srv_repmgr (repmgr's service manager NOT bt's)
			and repmgr's start script MUST send the value of srv_Filer
			each time it is changed.  That is the ONLY way this will be able 
			to run on a different node than repmgr. The repmgrbt start script
			is also expected to start with a -s option that supplies srv_Filer
			to this process at startup. 

	Date:		08 Apr 2004
	Author:		Original: Andrew Hume, threaded enhancement: Scott Daniels
-------------------------------------------------------------------------------------------
*/
#define SANS_AST 1			
#include	<stdlib.h>
#include	"/usr/include/stdio.h"	/* full path to avoid the one created by sfio */
#include	<unistd.h>
#include	<syslog.h>
#include	<stddef.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>
#include	<pthread.h>
#include	<signal.h>
#include	<time.h>
#include	<sys/socket.h>
#include	<sys/ioctl.h>

#include	"../../include/ng_arg.h"	/* must do it this hardcoded way to avoid sucking in sfio defs in ng_src/include */

#define MAX_CONN_SEC	120		/* max seconds to stay connected while writing to replication manager */
#define MIN_CONN_SEC	10		/* min time we will hold. use as lower bound when conn is received */
#define MAX_WRITE	1000000		/* max bytes to write before forcing flush */
#define	MAX_IDLE	10		/* block for this many sec while waiting for conn. lets us test for a drop listener state */
#define MAX_FDS		256		/* max fds that we will allow to be connected  concurrently */

#define PAUSE_SEC	3		/* seconds to pause while trying to get repmgr connect, and after we close a repmgr session */
#define MIN_WAIT	15		/* seconds we wait to let things amass when our queue goes dry */


/* these EOF things must match values in repmgr.h -- too much stuff in that header to include for just these things */
#define EOF_CHAR	0xbe		/* sent by rmreq to indicate end of transmission */
#define EOF_ACK_CHAR	0xbf		/* send to repmgr to indicate end of data and require an ack back; we hold session until we see it */
/* ------------------------------------------------------------------------------------------------------*/

#define FD_CLOSED	-1		/* file descriptor is not valid */

#define FL_LOGM		0x01		/* log all user messages */
#define FL_LOGR		0x02		/* log all raw buffers */
#define FL_HOLD		0x04		/* hold data -- do not pass to repmgr until flag is off */
#define FL_OK2SEND	0x08		/* we expect to see an ok2send from rm_start; we turn this off as we send repmgr an exit */
#define FL_NEED_RBB	0x10		/* need to send the replay begin buffer */
#define FL_RESEND	0x20		/* resend the buffer from the current replay cache */


#define BF_CACHED	0x01		/* set when we add the buffer to a cache set; prevents us from adding it again */

#define VER_LOGM	1		/* verbose/10 == 1, then we log all msgs sent to repmgr */
#define VER_LOGR	2		/* verbose level 2x then we log raw input buffers */


struct buffer {				/* data collected to send to repmgr */
	struct buffer *next;
	char	*b;			/* actual buffer */
	long	n;			/* number of bytes of data we have in b*/
	long	a;			/* number of bytes allocated in b */
	time_t	t;			/* time buffer received */
	int	flags;
};

struct cache_set {			/* replay cache of buffers */
	struct cache_set *next;
	struct buffer	*fb;		/* first buffer */
	struct buffer	*lb;		/* last buffer */
	struct buffer	*cb;		/* current buffer when writing from cache set */
	long long	count;		/* number of buffers in this set */
	char		*id;		/* id string of the set (supplied by outside) */
};

int	ok2run = 1;			/* set to 0 to indicate the need for a graceful (draining) exit */
int	close_listen = 0;		/* we must close the listen port a few seconds before we turn off the ok2run flag */
int	fd_ready[MAX_FDS];		/* a list of fd's that are waiting for reader threads, in the order of session establishment */
int	fd_next = 0;			/* spot to take next fd from -- readers inc when holding fd mutex lock */
int 	fd_insert = 0;			/* index into fd_ready for fd of most recent accepted session; only listener adjusts */
int	open_listens = 0;		/* active reader threads */
int	need_ack = 0;			/* set with -a to require a connection ack from repmgr */
int	need_eod_ack = 1;		/* off with -A; causes end of data mark to be sent and us to wait for eod ack  from repmgr */
int	max_ack_wait = 240;		/* seconds to wait for an end of data ack before going on; can be changed via ~ackwait command */
int	flags = FL_OK2SEND;		/* we start in this mode as repmgr might be running when we start */
int	max_conn_time = 40;		/*  number of seconds max that we hold a conn to repmgr.  can be set with ~conn~ cmd */
char	*rmhost = NULL;			/* allow repmgr to move; this is tricky as we do NOT have access to pinkpages */

int	verbose = 0;
char	*argv0 = NULL;
char	*version = "v2.9/01059";

unsigned char eof_chr = EOF_CHAR;	/* byte in the inbound stream that flags the end -- all things after ignored */

char *port = NULL;			/* ng_env("NG_REPMGR_PORT"); */
char *rport = NULL;			/* ng_env("NG_REPMGRS_PORT"); */

char *log_fname = NULL;			/* where to write data messages that we received (file, set, unset, unfile, synch and hood) */

struct buffer *wq_head = NULL;		/* write queue head/tail */
struct buffer *wq_tail = NULL;
struct buffer *last_dump = NULL;	/* the tail at dump time -- prvent purge of things not dumped */

					/* replay cache management variables */
struct cache_set *rc_head = NULL;	/* replay cache pointers */
struct cache_set *rc_tail = NULL;	
struct cache_set *replay = NULL;	/* cache set that is currently being pulled from for replay */
struct buffer *replay_begb = NULL;	/* begin/end of replay buffers */
struct buffer *replay_endb = NULL;
int	rc_count = 0;			/* number of cache sets we have */
int	rc_max = 5;			/* number of cache sets that we keep */
int	rc_max_per = 10000;		/* max buffers per cache set -- keeps sane if repmgr stops/doesnt set markers */

pthread_mutex_t fd_mutex;			/* file descriptor management control mutex */
pthread_mutex_t q_mutex;			/* blocks queued for write */
pthread_mutex_t rc_mutex;			/* locks the replay cache */

/* ------------------------------------ general utility ---------------------------------------------- */
static void *bt_malloc( int n )
{
	void *p;

	if( (p = malloc( n )) == NULL )
	{
		fprintf( stderr, "could not allocate memory" );
		exit ( 1 );
	}

	return p;
}


static char *timestamp( )
{
	static char buf[100];
	time_t t;
	
	t = time( 0 );
	snprintf( buf, sizeof( buf ), "%ld", (long) t );
	return buf;
}

/* parent registers this handler to force an exit when child signals with a usr1 */
static void sigh( int s )
{
	fprintf( stderr, "%s control parent: signal received -- exit good\n", timestamp() );
	exit( 0 );				/* the parent will register sigusr1 with this */
}


/* -------------------------------- reader session management --------------------------------------------- */
/*	return the next active fd for a thread to work on 
	we block until the main sets an fd to pick up.
	The reader threads can snarf a fd from the ready list only when they hold 
	the lock.  The main thread can write to the ready list only when the 
	value to be overlaid is FD_CLOSED.  The main thread does not have to 
	lock the list.  Because the main does not need to lock the list, this
	funciton can hold the lock until the cows come home.
*/
static int getfd( )
{
	int	f = FD_CLOSED;
	
	pthread_mutex_lock( &fd_mutex );			/* we get the next fd once we get the lock */
	
	while( ok2run && (f = fd_ready[fd_next]) == FD_CLOSED )		/* until we accept a session and fill it in  */
	{
		sleep( 1 );					/* ok to stay, just hang loose */
	}

	fd_ready[fd_next] =  FD_CLOSED;		/* we used it, so reset it */

	if( f != FD_CLOSED )
	{
		if( ++fd_next >= MAX_FDS )		/* set next one to look at for next call */
			fd_next = 0;			/* at next to take, wrapping if necessary */
	}

	pthread_mutex_unlock( &fd_mutex );	/* let another have the comm */

	return f;
}

/* add an fd to the list at the insert point -- we block if the array is full */
static void putfd( int f )
{
	if( verbose > 3 )
		fprintf( stderr, "%s putfd: putting fd into array: %d\n", timestamp(), f );
	while( fd_ready[fd_insert] != FD_CLOSED )	/* block if insertion point occupied -- buffer of fds waiting is full */
	{
	if( verbose > 1 )
		fprintf( stderr, "%s putfd: blocked on insert at %d %d\n", timestamp(), fd_insert, fd_ready[fd_insert] );
		sleep( 1 );				
	}

	fd_ready[fd_insert] = f;
	if( ++fd_insert >= MAX_FDS )
		fd_insert = 0;			/* wrap insertion point if needed */
}

/* --------------------------------- buffer queue managment ------------------------------------- */
static struct buffer *dq_buf( )				/* get the next buffer */
{
	struct buffer *b;

	if( flags & FL_HOLD )			/* may not dq anything just now */
		return NULL;

	if( ! (flags & FL_OK2SEND) )
		return NULL;			/* we have sent an exit to repmgr and not seen it restart (ok2send from rm_start) */

	if( replay )				/* we are replaying -- dig next buffer from the cache set */
	{					/* so that we can 'push' a buffer back, the 'current' pointer is the last used */
		if( flags & FL_NEED_RBB )			/* need to send replay begin buffer */
		{
			flags &= ~FL_NEED_RBB; 
			return replay_begb;
		}

		pthread_mutex_lock( &rc_mutex );

		if( ! replay->cb )					/* no current buffer in this set */
			b = replay->cb = replay->fb;			/* select the first buffer */
		else
		{
			if( flags & FL_RESEND )			/* the buffer was 'pushed' back */
			{
				flags &= ~FL_RESEND;
				b = replay->cb;
			}
			else
				b = replay->cb = replay->cb->next;	/* load from next in the list */		
		}

		while( ! b  && replay )					/* last in this set, and still more cache sets follow */
		{
			if( (replay = replay->next) != NULL )		/* more cache sets */
			{
				b = replay->cb = replay->fb;		/* at the first buffer in next set */

				if( verbose > 1  )
					fprintf( stderr, "%s replay continues with: %s; %ldd buffers\n", timestamp(), replay->id, replay->count );
			}
		}

		pthread_mutex_unlock( &rc_mutex );

		if( b )
			return b;
		else
		{
			if( verbose > 1  )
				fprintf( stderr, "%s replay ends\n", timestamp() );
			return replay_endb;		/* must send end of replay marker */
		}
	}

						/* not in replay mode, fish the next one off of the unsent stack */
	pthread_mutex_lock( &q_mutex );
	
	if( (b = wq_head) )				/* allow it to be empty */
	{
		if( ! (wq_head = b->next) )
			wq_tail = wq_head;
		b->next = NULL;
	}

	pthread_mutex_unlock( &q_mutex );

	return b;
}

/* used when send fails, we put the buffer back on queue for next connection */
static void push_buf( struct buffer *b )			
{
	if( b->flags & BF_CACHED )
	{ 					/* cache pointers stick at the last buffer, no push needed */
		flags |= FL_RESEND;		/* resend the current buffer */
		return;				
	}

	pthread_mutex_lock( &q_mutex );

	b->next = wq_head;
	if( ! wq_tail )
		wq_tail = b;
	wq_head = b;
	
	pthread_mutex_unlock( &q_mutex );
}

/* queue buffer at the tail to preserve as close to first in first out order as possible */
static void q_buf( struct buffer *b )			
{
	if( verbose > 4 )
		fprintf( stderr, "%s queue buffer: buffer was: %x\n", timestamp(), b  );
	pthread_mutex_lock( &q_mutex );

	if( verbose > 4 )
		fprintf( stderr, "%s queue buffer: got lock\n", timestamp()  );

	if( wq_tail )
	{
		wq_tail->next = b;
		wq_tail = b;
	}
	else
		wq_head = wq_tail = b;		/* first and only on the queue */

	b->next = NULL;				/* parinoid */
	
	pthread_mutex_unlock( &q_mutex );
}

/* ------------------------------------ replay cache set management -------------------------------- */

/* setup begin/end replay buffers */
void setup_replay( )
{
	struct buffer *bp;

	bp = (struct buffer *) bt_malloc( sizeof( struct buffer ) );
	memset( bp, 0, sizeof( struct buffer ) );
	bp->flags = BF_CACHED;				/* prevents us from trying to cache this buffer */
	bp->b = strdup( "replay begin\n" );
	bp->a = strlen( bp->b )+1;
	bp->n = strlen( bp->b );
	replay_begb = bp;

	bp = (struct buffer *) bt_malloc( sizeof( struct buffer ) );
	memset( bp, 0, sizeof( struct buffer ) );
	bp->flags = BF_CACHED;				/* prevents us from trying to cache this buffer */
	bp->b = strdup( "replay end\n" );
	bp->a = strlen( bp->b )+1;
	bp->n = strlen( bp->b );
	replay_endb = bp;
}

/* create a new replay set and add to end of the list */
struct cache_set *new_cache_set( char *id )
{
	struct cache_set *cp;

	cp = (struct cache_set *) bt_malloc( sizeof( struct cache_set ) );
	memset( cp, 0, sizeof( struct cache_set ) );
	
	cp->id = strdup( id );

	pthread_mutex_lock( &rc_mutex );

	if( rc_tail )
	{
		rc_tail->next = cp;		/* add to end of list */
		rc_tail = cp;
	}
	else
		rc_head = rc_tail = cp;

	rc_count++;				/* number we are managing */
	pthread_mutex_unlock( &rc_mutex );

	if( verbose )
		fprintf( stderr, "%s added cache set: %s @ 0x%x\n", timestamp( ), id, cp );
	return cp;
}

/* purge a specific set, or the top if no id given */
void drop_cache_set( char *id )
{
	static int yelpped = 0;			/* prevent flood of replay in progress messages */

	struct cache_set *cp;
	struct cache_set *pcp = NULL;		/* previous for easy removal */
	struct buffer *bp;
	struct buffer *nbp;			/* next buffer to allow for free */

	pthread_mutex_lock( &rc_mutex );

	if( replay )				/* if we drop while replay is going we corrupt things big time */
	{
		if( verbose && ! yelpped )
			fprintf( stderr, "%s cache set not dropped: replay in progress\n", timestamp() );
		yelpped++;
	}
	else
	{
		yelpped = 0;

		if( id )
		{
			for( cp = rc_head; cp; cp = cp->next )
			{
				if( strcmp( cp->id, id ) == 0 )
					break;
	
				pcp = cp;
			}
		}
		else
			cp = rc_head;
	
		if(  cp )
		{
			if( verbose )
				fprintf( stderr, "%s cache set dropped: %s\n", timestamp( ), cp->id );
	
			for( bp = cp->fb; bp; bp = nbp )		/* free all of the buffers in this set */
			{
				nbp = bp->next;
				free( bp->b );
				free( bp );
			}
		
			if( pcp )				/* remove from the middle of the list */
				pcp->next = cp->next;
			else
				rc_head = cp->next;
		
			if( cp == rc_tail )			/* reset tail if this was the last */
				rc_tail = pcp;
		
			free( cp );
			rc_count--;
		}
		else
			if( verbose )
				fprintf( stderr, "%s drop cache set failed: none with matching id: %s", timestamp(), id ? id : "no-id-given" );
	}

	pthread_mutex_unlock( &rc_mutex );
}
	

/* find the cache set that matches the given id; null if not found 
	caller MUST have lock; if this function gets the lock
	the block may change before the caller can use it
*/
struct cache_set *find_cache_set( char *id )
{
	struct cache_set *cp = NULL;

	for( cp = rc_head; cp; cp = cp->next )
		if( strcmp( id, cp->id ) == 0 )
			break;
	return cp;
}

/* add buffer to the current cache set (pointed to by the tail) */
void add2_cache_set( struct buffer *bp )
{
	static 	int artificial_n = 0;
	char	buf[100];
	long 	long c;

	if( bp->flags & BF_CACHED )			/* this one already cached, must be in replay mode so just return */
		return;

	if( ! rc_tail )					/* first one -- create the epoch */
	{
		if( ! new_cache_set( "prime" ) )	/* create the beginning of time */
		{
			fprintf( stderr, "%s ERROR: add2_cache_set: new_cache_set returned NULL!\n", timestamp( ) );
			return;
		}
	}

	pthread_mutex_lock( &rc_mutex );	/* new_cache will lock, so we lock after above logic */

	bp->next = NULL;
	bp->flags |= BF_CACHED;			/* mark as belonging to a cache set */

	if( rc_tail->lb )
		rc_tail->lb->next = bp;
	else
		rc_tail->fb = bp;

	rc_tail->lb = bp;			/* its now the last one */

	rc_tail->count++;

	c = rc_tail->count;
	pthread_mutex_unlock( &rc_mutex );

	if( c > rc_max_per )			/* just too many; is repmgr not marking? */
	{
		fprintf( stderr, "%s cache_set too large; adding artifical set after %s\n", timestamp( ), rc_tail->id );
		snprintf( buf, sizeof( buf ), "artificial_%d", artificial_n++ );
		new_cache_set( buf );	
		if( rc_count > rc_max  )
			drop_cache_set( NULL );		/* toss the top one */
	}
}

/* start replaying at cache set with id */
int start_replay( char *id )
{
	struct cache_set *cp;
	int 	rc = 0;

	if( replay )					/* if one going, dont smash it */
	{
		fprintf( stderr, "%s replay: replay already in progress; cannot start at marker %s\n", timestamp( ), id );
		return 0;
	}


	pthread_mutex_lock( &rc_mutex );
	if( (cp = find_cache_set( id )) != NULL )
	{

		if( verbose )
			fprintf( stderr, "%s replay: started: %s %lld%s buffers cp=0x%x\n", timestamp( ), id, cp->count, cp->next ? "+" : "", cp  );

	
		flags |= FL_NEED_RBB;			/* must have a replay begin buffer to start */
		replay = cp;
		replay->cb = NULL;			/* current buffer pointer must be null to start */


		rc = 1;
	}
	else
		fprintf( stderr, "%s replay: cannot start replay: no cache set:  %s\n", timestamp( ), id );

	pthread_mutex_unlock( &rc_mutex );

	return rc;
}

/* send the cached buffers from marker to end to the tcpip session pointed to by fd */
void dump_replay( int fd, char *id, int dump_all )
{
	struct  cache_set *cp;
	struct	buffer *bp;

	pthread_mutex_lock( &rc_mutex );
	if( id )
		cp = find_cache_set( id );
	else
		cp = rc_head;

	if( ! cp )
	{
		fprintf( stderr, "%s dump_replay: did not find anything to dump: %s\n", id ? id : "no chache set id provided", timestamp( ) );
	}
	else
	{
		while( cp )
		{
			if( verbose > 1 )
				fprintf( stderr, "%s dump_replay: %s %lld buffers\n", timestamp( ), cp->id, cp->count );
	
				for( bp = cp->fb; bp; bp = bp->next )
				write( fd, bp->b, bp->n );
	
			cp = dump_all ? cp->next : NULL;
		}
	}
	pthread_mutex_unlock( &rc_mutex );
}

/* ------------------------------------ user interface ------------------------------------------------- */
/* simple state message to stderr */
static void dump_state( )
{
	fprintf( stderr, "%s state: ok2send=%s  hold=%s  logm=%s\n", timestamp( ), flags & FL_OK2SEND ? "ON" : "OFF", flags & FL_HOLD ? "ON" : "OFF", flags & FL_LOGM ? "ON" : "OFF"  );

	fprintf( stderr, "%s replay-cache: sets=%d\n", timestamp( ), rc_count );
}

/*  
	dump all information that is queued to the filename passed in 
	return success as true
*/
static int dump_innards( char *fname )	
{
	extern	int errno;

	FILE	*f;
	struct buffer *b;
	long	bytes = 0;		/* amount of data dumpped */
	long	bufs = 0;		/* number of buffers dumped */
	time_t	 start = 0;
	time_t	 end = 0;		/* to give a simblance of timespan that data covers */
	int	p;

	last_dump = NULL;

	if( p = strcspn( fname, "\n \t" ) )
		*(fname+p) = 0;

	if( (f = fopen( fname, "w" )) == NULL )
	{
		fprintf( stderr,  "%s dump_innards: file open failed: %s: %s\n", timestamp( ), fname, strerror( errno ) );
		return 0;
	}

	pthread_mutex_lock( &q_mutex );
	if( verbose  )
		fprintf( stderr, "%s dump_innards: have lock, writing to: %s\n", timestamp(), fname );
	
	if( (b = wq_head) )
		start = b->t;			/* time first buffer received */
	while( b )
	{
		fprintf( f, "%s", b->b );		/* buffer data is \n\0 terminated */
		bytes += b->n;
		bufs++;
		end = b->t;
		b = b->next;
	}

	last_dump = wq_tail;

	if( ferror( f ) )
	{
		fprintf( stderr,  "%s dump_innards: file error detected: %s: %s\n", timestamp( ), fname, strerror( errno ) );
		fclose( f );
		return 0;
	}

	if( fclose( f ) )
	{
		fprintf( stderr,  "%s dump_innards: file close failed: %s: %s\n", timestamp( ), fname, strerror( errno ) );
		return 0;
	}

	pthread_mutex_unlock( &q_mutex );
	if( verbose )
	{
		fprintf( stderr, "%s dump_innards: released lock\n", timestamp()  );
	}
	fprintf( stderr, "%s dump_innards: %ld bytes in %ld buffers received over %ld seconds written to: %s\n", timestamp( ), bytes, bufs, (long) (end-start), fname );

	return 1;
}

/*	purge all things cached */
static void *purge_innards( )
{
	extern	int errno;

	struct buffer *b;		/* current */
	struct buffer *p;		/* one to purge */
	long	bytes = 0;		/* amount of data dumpped */
	long	bufs = 0;		/* number of buffers dumped */
	time_t	 start = 0;
	time_t	 end = 0;		/* to give a simblance of timespan that data covers */

	if( ! last_dump )
		return;			/* if we dont think anything was saved, then dont purge anything */

	pthread_mutex_lock( &q_mutex );
	if( verbose  )
		fprintf( stderr, "%s purge_innards: have lock\n", timestamp( ) );

						/* quickly move the list so we can give it off to other readers for real work */
	if( (b = wq_head) )
		start = b->t;			/* time first buffer received */

	if( wq_tail != last_dump )		/* something added between dump call and invocation here */
	{
		wq_head = last_dump->next;
		last_dump->next = NULL;
	}
	else
	{
		wq_head = NULL;			
		wq_tail = NULL;
	}
	
	pthread_mutex_unlock( &q_mutex );
	if( verbose )
		fprintf( stderr, "%s purge_innards: released lock\n", timestamp( ) );
	
	/* we can take our time to free all of the buffers */
	while( p = b )
	{
		b = b->next;
		bytes += p->n;
		bufs++;
		end = p->t;

		free( p->b );		/* free the data */
		free( p );		/* and then it */
	}

	if( verbose )
	{
		fprintf( stderr, "%s purge_innards: %ld bytes in %ld buffers received over %ld seconds\n", timestamp( ), bytes, bufs, (long) (end-start) );
	}
}

/* act on a recognised ~cmd~ command. return true if the command was handled (caller then ignores buffer)
*/
static int parse_cmd( char *b, int len, int fd )
{
	int v;				/* new verbose value for sanity check */
	int f;				/* flag value */
	char	*t;			/* pointer at token in the buffer */

	/* buffer is NOT null terminated; each case must terminate if needed */
	switch( *(b+1) )		
	{
		case 'a':
			if( len > 5 && strncmp( b, "~ackeod~", 8 ) == 0 )
			{
				need_eod_ack = !need_eod_ack;		/* toggle eod ack */
				fprintf( stderr, "%s eod ack changed: %s \n", timestamp( ), need_eod_ack ? "ON" : "off"  );
				return 1;
			}
			else
			if( len > 5 && strncmp( b, "~ackwait~", 9 ) == 0 )
			{
				max_ack_wait = atoi( b+9 );
				fprintf( stderr, "%s eod ack wait changed to %ds; eodack is %s \n", timestamp( ), max_ack_wait, need_eod_ack ? "ON" : "off"  );
				return 1;
			}
			break;

		case 'e':
			if( len > 5 && strncmp( b, "~exit~", 6 ) == 0 )
			{
				close_listen = 1;		/* force main to shut listener a few seconds before we turn off ok2run */
				fprintf( stderr, "%s exit received; starting shutdown by turning close_listen flag ON\n", timestamp( ) );
				return 1;
			}
			break;

		case 'c':
			if( strncmp( b, "~conn~", 6 ) == 0 )
			{
				
				v = atoi( b+6 );
				if( v <= MAX_CONN_SEC  && v >= MIN_CONN_SEC )
				{
					fprintf( stderr, "%s max conn sec changed from %d to %d\n", timestamp( ), max_conn_time, v );
					max_conn_time = v;
				}
				else
					fprintf( stderr, "%s conn value out of range; must be: %d => val <= %d\n", timestamp( ), v, MIN_CONN_SEC, MAX_CONN_SEC );
				return 1;
			}
			break;

		case 'd':
			if( strncmp( b, "~dump~", 6 ) == 0 )		/* buf+6 expected to be a filename to dump stuff to */
			{
				dump_innards( b+6 );
				return 1;
			}
			break;
		
		case 'h':
			if( strncmp( b, "~hold~", 6 ) == 0 )
			{
				flags ^= FL_HOLD;			/* toggle hold flag */
				fprintf( stderr, "%s hold flag toggled to %s\n", timestamp( ), flags & FL_HOLD ? "ON" : "off" );
				return 1;
			}
			break;
		
		case 'o':
			if( strncmp( b, "~ok2send~", 9 ) == 0 )		/* from rm start -- turns  on flag, we will send again  */
			{
				flags |= FL_OK2SEND;			
				flags &= ~FL_HOLD;			/* this turns off hold */
				fprintf( stderr, "%s ok2send received\n", timestamp( ) );
				return 1;
			}
			break;

		case 'p':
			if( strncmp( b, "~pdump~", 7 ) == 0 )		/* buf+7 expected to be a filename to dump stuff to */
			{
				if( dump_innards( b+7 ) )		/* dump first and if all seems well, purge them */
					purge_innards( );			
				return 1;
			}
			break;
		
		
		case 'r':						/* replay cache commands */
			if( (t = strchr( b, '\n' )) != NULL )
			{
				if( *(t-1) == ' ' )			/* rmreq pads with a  blank for unknown reasons */
					*(t-1) = 0;
				else
					*t = 0;				/* end of string at first newline */
			}
			else
				b[len-1] = 0;				/* should not happen, but it MUST be a string */

			if( strncmp( b, "~rmhost~", 8 ) == 0 )
			{
				if( (t = strchr( b+1, '~' )) && *(++t) )
				{
					if( rmhost )
						free( rmhost );
					rmhost = strdup( t );
					fprintf( stderr, "%s rmhost reset: %s\n", timestamp( ), rmhost );
				}
				return 1;
			}
			else
			if( strncmp( b, "~rcmark~", 8 ) == 0 )		/* establish a new cache set */
			{
				if( (t = strchr( b+1, '~' )) && *(++t) )
				{
					new_cache_set( t );			
					if( rc_count > rc_max  )
						drop_cache_set( NULL );		/* toss the top one */
				}
				return 1;
			}
			else
			if( strncmp( b, "~rclist~", 8 ) == 0 )		/* list cache sets */
			{
				struct cache_set *cp;
				char	wbuf[NG_BUFFER];

				pthread_mutex_lock( &rc_mutex );
				for( cp = rc_head; cp; cp = cp->next )
					write( fd, wbuf, snprintf( wbuf, sizeof( wbuf ),  "cache set: %7lld buffers %s\n",  cp->count, cp->id ) );
				pthread_mutex_unlock( &rc_mutex );

				write( fd, "#end#\n", 6 );
				return 1;
			}
			else
			if( strncmp( b, "~rcfind~", 8 ) == 0 )		/* find cache set (return info if there)  output is: <status> <text> */
			{
				char	wbuf[NG_BUFFER];
				struct cache_set *cp;

				if( (t = strchr( b+1, '~' )) && *(++t) )
				{
					pthread_mutex_lock( &rc_mutex );
					if( (cp = find_cache_set( t )) )
						write( fd, wbuf, snprintf( wbuf, sizeof( wbuf ),  "1 cache set: %7lld buffers %s\n",  cp->count, cp->id ) );
					else
						write( fd, wbuf, snprintf( wbuf, sizeof( wbuf ),  "0 cache set: not found: %s\n", t ) );
					pthread_mutex_unlock( &rc_mutex );
				}
				write( fd, "#end#\n", 6 );
				return 1;
			}
			else
			if( strncmp( b, "~rcplay~", 8 ) == 0 )		/* play starting at named cache set */
			{
				char wbuf[NG_BUFFER];
				int  l;

				if( (t = strchr( b+1, '~' )) && *(++t) )
				{
					if( start_replay( t ) )
					{
						if( verbose )
							fprintf( stderr, "%s replay starts with: %s\n", timestamp(), t );
						l = snprintf( wbuf, sizeof( wbuf ), "replay starts with: %s\n#end#\n", t );
					}
					else
					{
						l = snprintf( wbuf, sizeof( wbuf ), "unable to start replay: %s\n#end#\n", t );
						if( verbose )
							fprintf( stderr, "unable to start replay with: %s\n", t );
					}
					write( fd, wbuf, l );
				}
				return 1;
			}
			if( strncmp( b, "~rcdump~", 8 ) == 0 )		/* dump some/all replay cache to fd */
			{
				char wbuf[NG_BUFFER];
				int  l;

				if( (t = strchr( b+1, '~' )) && *(++t) )
					dump_replay( fd, t, 1 );
				else
					dump_replay( fd, NULL, 1 );
				write( fd, "#end#\n", 6 );
				return 1;
			}
			else
			if( strncmp( b, "~rcpurge~", 9 ) == 0 )
			{
				if( (t = strchr( b+1, '~' )) && *(++t) )
					drop_cache_set( t );			/* trash a specific one */
				else
					drop_cache_set( NULL );			/* drop the head */
				write( fd, "#end#\n", 6 );
				return 1;
			}
			break;

		case 's':
			if( strncmp( b, "~stats~", 7 ) == 0 )		/* put state to stderr */
			{
				dump_state( );
				return 1;
			}
			break;	

		case 'v':
			if( len > 9 && strncmp( b, "~verbose~", 9 ) == 0 )		/* expect double digit nm where flag is n, value is m */
			{
				v = atoi( b+9 );
				flags &= ~(FL_LOGM|FL_LOGR);		/* turn verbose related flags off */
				switch ( v/10 )
				{
					case VER_LOGM:	flags |= FL_LOGM; break;
					case VER_LOGR:	flags |= FL_LOGR; break;
					default:	break;
				}
				
				v = v % 10;
		
				if( v >= 0 ) 
				{
					fprintf( stderr, "%s %s %s verbose changed from %d to %d flags=0x%02x\n", timestamp( ), argv0, version, verbose, v, flags );
					verbose = v;
				}
				else
					fprintf( stderr, "%s %s %s verbose change attempted; invalid value: %d\n",  timestamp( ), argv0, version, v );
				return 1;
			}
			break;
	}
		
	return 0;
}

/* -------------------------- reader/writer support ------------------------------------------ */
/*	read data from rmreq (or somesuch) and compile it all in a single buffer 
	queue buffer when end of data received. drop buffer if session ends
	before end of data.
*/
static int read_buf( int fd, struct buffer *buf )
{
	int i;
	int k;
	int s;
	int n = 0;			/* receive number -- when 0 we test for command */

	if( fd == FD_CLOSED || ! buf )
		return 0;

	while( (k = recv( fd,  buf->b+buf->n, (buf->a-buf->n)-1, 0 )) )    /* read data */
	{
		if( n++ == 0 && *(buf->b) == '~' )
			if( parse_cmd( buf->b, k, fd ) )
				return 0;				/* was a command -- close session and discard buffer */

		for(i = 0; i < k; i++)
		{
			if((0xFF&buf->b[buf->n+i]) == eof_chr){
				buf->n += i;
				buf->b[buf->n] = 0;		/* cvt eof-chr to null to make a string for logging/dump purposes */
				if( flags & FL_LOGR )				/* logging raw buffers */
					fprintf( stderr, "%s read_buf: raw = %s\n", timestamp( ), buf->b );
				return buf->n;
			}
		}

		buf->n += k;

		if((buf->a-buf->n) < 1024){
			buf->a *= 2;
			buf->b = (char *) realloc(buf->b, buf->a );
			if( ! buf->b )
			{
				fprintf( stderr, "%s realloc failed: %d bytes for buf->b\n", timestamp( ), buf->a );
				exit( 1 );
			}
		}
	}

	if( verbose )
		fprintf( stderr, "%s readbuf: session dropped or no end of marker seen in input\n", timestamp( ) );

	return 0;				/* we MUST see the end of data marker, else we discard the whole thing */
}

/* 	connect to repmgr  -- blocks until connection is established 
	we assume that repmgr is running on the loscal host (we DONT want to call ng_env because
	of ast and threads.)

	ng_rx1, ng_testport are both thread safe because we specially compile them into a no-ast library
	with using the SANS_AST define.
*/
static int connect2rm( )
{
	extern int errno;

	int notified=0;
	int	fd;		/* address to send to rx */
	char	buf[10];

	if( verbose > 2 )
		fprintf( stderr, "%s connect2: attempting connection to: %s: %s\n", timestamp(), rmhost, rport );

	while(  ng_rx1( rmhost, rport, &fd, NULL ) )		/* NULL says dont dup in ng_rx -- we only need one fd */
	{
		if( ! notified )
		{
			fprintf( stderr, "%s connect2: connect failed to %s:%s; entering retry loop\n", timestamp(), rmhost, rport );
			notified++;
		}

		sleep( ok2run ? PAUSE_SEC : 15 );		/* ease back if in shutdown mode */
	}

	if( verbose > 1 || notified )
		fprintf( stderr, "%s connect2: connection established to repmgr: %s:%s sid=%d\n", timestamp( ), rmhost, rport, fd );

	if( need_ack )
	{
		if( verbose > 3 )
			fprintf( stderr, "%s connect2: waiting for handshake from repmgr\n", timestamp( ) ); 

		errno = EWOULDBLOCK;				/* ng_read does not set errcond, so this helps it along */
		if( ng_read( fd, buf, 1, 120 ) <= 0 )		/* break with error (-2) if we block for more than 120 seconds */
		{
			fprintf( stderr, "%s  connect2: handshake after connection failed: %s\n", timestamp( ), strerror( errno ) );
			close( fd );
			return FD_CLOSED;
		}	

		if( verbose > 2 )
			fprintf( stderr, "%s connect2: handshake from repmgr ok: 0x%x\n", timestamp( ), *buf ); 
	}
	else
		if( verbose > 3 )
			fprintf( stderr, "not waiting for handshake: %d\n", need_ack );

	return fd;
}

/*	send the buffer to fd.  In order to detect disconnections, we must test the 
	fd for reading.  A disconnect is signaled to us by a positive return by 
	testport, and a zero read reeturn.  If we dont do this, then the first send
	after disconnect appears successful. 
*/
static int send_buf( int fd, struct buffer *b )
{
	int	status;
	char	buf[255];				/* if read flag is set, and there really is data to read, we put it here */
	int 	sent = 0;
	int 	st;
	int	slen = 0;			/* send length b->n count includes terminating null */


	if( fd == FD_CLOSED || !b )
		return -1;


	if( (status = ng_testport_m( fd, 0, 10000 )) != 0 )		/* testport 1 == read bit set; -1 error on port */
	{
		if( status < 0 || recv( fd,  buf, sizeof( buf )-1, 0 ) <= 0 )	/* 0 bytes read when read pending from test indicates disc */
		{
			if( verbose )
				fprintf( stderr, "%s session disconnected before write\n", timestamp( ) );
			return -1;
		}
	}

	if( verbose > 3 )
		fprintf( stderr, "%s send_buf: sending %d bytes  %x\n", timestamp( ), b->n, b );

	if( b->b[b->n-1] == 0 )
		slen = b->n - 1;		/* dont send last null */
	else
		slen = b->n;		
	while( sent < slen )
	{
		if( (st = send( fd, &b->b[sent], slen-sent, 0 ) ) < 0 )
		{
			fprintf( stderr, "%s send failure! sent %d total=%d\n", timestamp( ), st,sent );
			return -1;
		}
		sent += st;

		if( verbose > 2 )
			fprintf( stderr, "send: last send=%d total sent=%d remain=%d\n", st, sent, slen - sent );
		
	}

	return sent;
}


static void send_exit( int fd )		/* send an exit messge as the only message in the buffer to prevent dropped commands */
{
	struct buffer b;

	b.b = strdup( "exit\n" );
	b.n = strlen( b.b );
	b.a = strlen( b.b ) + 1;

	send_buf( fd, &b );
	free( b.b );
}

/* ----------------------------- private log management ------------------------------- */

/*	open a new log file 
	if one was previously opened, it is closed and placed into repmgr space for 
	safe keeping
*/
FILE *open_log( )
{
	static FILE *f = NULL;
	static char fname[1024];
	static	int seq = 0;			/* sequence number */

	char cmd[2048];
	
	if( f )
	{
		fclose( f );

#ifdef KEEP
		/* should we close a log file on a sync, then keep this to take care of the file */
		snprintf( cmd, sizeof( cmd ), "ng_wrapper --detach ng_rmbt_frag %s", fname );
		system( cmd );			/* run the command -- should run asynch, and we dont care about success */
#endif
	}

	/*snprintf( fname, sizeof( fname ), "%s.%d", log_fname, seq++ );*/
	snprintf( fname, sizeof( fname ), "%s", log_fname );		/* for now we cut one file per session; rmbt start rolls them */

	if( verbose )
		fprintf( stderr, "%s open_log: opening log: %s\n", timestamp( ),  fname );

	f = fopen( fname, "a" );

	return f;
}

/*	parse the buffer and write interesting things to our log 
	the log file is closed and reopened when a synch string is 
	received (before the synch is logged) to allow the file to 
	be moved if long running, w/out open/close for every call.
	We assume that the reader converted the end of data character
	from the 'client' into a null.
*/
void log_buf( struct buffer *buf )
{
	static FILE *f = NULL;		/* file des of log file */
	static int notified = 0;	/* prevent flood of unable messages */

	int	match = 0;
	int 	sync = 0;		/* we hit a sync command */
	char	*s;			/* start of string to write */
	char	*e;			/* end of string */
	char	b[2048];

	if( ! log_fname )		/* no name, no loggie */
		return;

	if( f || (f = open_log( )) )
	{
		notified = 0;

		s = buf->b;
		while(  s && *s )
		{
			sync = 0;
			switch( *s )			/* see if its one of the strings that we want to trace; set match if so */
			{
				case 'f':	match = strncmp( s, "file ", 5 ) == 0; break; 
				case 'h':	match = strncmp( s, "hood ", 5 ) == 0; break; 
				case 'm':	match = strncmp( s, "mv ", 3 ) == 0; break; 
				case 'u':	match = (strncmp( s, "unfile ", 7 ) == 0 ? 1 : strncmp( s, "unset ", 6 ) == 0); break; 
				case 'r':	match = strncmp( s, "replay", 6 ) == 0; break;
				case 's':	if( (match = strncmp( s, "set ", 4 )) != 0 )
							sync = match = strncmp( s, "synch ", 6) == 0; 		/* need to know sync too */
						break;

				default:	match = sync = 0; break;
			}
if( verbose > 3 )
match=1;

#ifdef KEEP
/* keep this if we want to roll the log on each sync */
			if( sync )
				f = open_log( );		/* on sync, we close and open the log file before writing sync */
#endif

			if( (e = strchr( s, '\n' )) );		/* find end of record */
			{
				if( match )						/* matched above as one we log */
					fwrite( s, sizeof( *s ), (e - s) +1, f );		/* write the record */
				
				if( flags & FL_LOGM )				/* log message mode */
				{
					memset( b, 0, sizeof( b ) );
					memcpy( b, s,  (e - s) < (sizeof( b ) -2) ? (e - s) : sizeof( b ) -2 );
					fprintf( stderr, "%s log_buf: total buf len=%d: %s\n", timestamp( ),  buf->n, b  );
				}
			}

			s = e ? e + 1 : NULL;
		}
	}
	else
		if( ! notified++ )
			fprintf( stderr, "%s log_buf: unable to log buffer!\n", timestamp( ) );

}

/* look at the buffer and if there is an exit command remove it and return with a nonzero value */
static int chk4exit( struct buffer *b )
{
	char	*p;		/* pointer into buffer (we assum \n terminated records and a null at end */
	int	rv = 0;		/* return value */

	p = b->b;
	while( p && *p )
	{
		if( strncmp( p, "exit", 4 ) == 0 && *(p+4) <= ' ' )		/* exit followed by blank, tab, newline, or 0 */
		{
			rv = 1;				/* flag as we cannot short circuit -- we must stomp out all of them */
			memset( p, '\n', 4 );		/* scribble over the command with newlines to remove */	
		}
		p++;					
	}

	return rv;
}
/* ------------------------ thread entry points for reader, writer --------------------- */

static void *reader( void *data )		 /* pthread entry point for reader thread */
{
	struct	buffer *buf;
	int	fd;				/* fd to work on */
	pthread_t 	me;			/* my thread id */
	int	len;
	
	me = pthread_self( );
	if( verbose )
		fprintf( stderr, "%s reader started: %x\n", timestamp( ), me );

	open_listens++;

	while( (fd = getfd( )) != FD_CLOSED )		/* while there are accepted sessions to process -- get blocks until there is one */
	{
		if( verbose > 2 )
			fprintf( stderr, "%s %x reader: working on fd %d ol=%d\n", timestamp( ), me, fd, open_listens );

		buf = (struct buffer *) bt_malloc( sizeof(  struct buffer ) );
		memset( buf, 0, sizeof( struct buffer ) );

		buf->a = 2048;
		buf->n = 0;
		buf->b = (char *) bt_malloc(buf->a);
		buf->t = time(0);

		if( (len= read_buf( fd, buf )) > 0 )			/* read all that is sent into one big buffer */
		{
			q_buf( buf );					/* queue buffer for writing to repmgr */
			if( verbose > 3 )
				fprintf( stderr, "%s %x reader: session received %d bytes of data: fd=%d\n", timestamp( ), me, len, fd );
		}
		else
		{
			if( verbose > 3 )
				fprintf( stderr, "%s %x reader: command received, session dropped, or no data: fd=%d\n", timestamp( ), me, fd );
			free( buf->b );
			free( buf );
		}

		/* if we need to send an ack or something, do it here */

		close( fd );
		if( verbose > 3 )
			fprintf( stderr, "%s %x reader: done with fd %d ol=%d\n", timestamp( ), me, fd, open_listens );
	}

	/* bad fd, must be time to close the thread */
	if( verbose )
		fprintf( stderr, "%s %x reader: closing thread\n", timestamp( ), me );

	open_listens--;
	pthread_exit( NULL );

	return NULL;			/* keep compiler happy */
}

static void *writer( void *data )
{
	struct buffer *b;
	time_t	force_flush = 0;		/* time where we force the session down */
	long 	written = 0;			/* bytes written on the current session */
	long	bwritten = 0;			/* buffers written on the current session */
	int	rfd= FD_CLOSED;			/* replication manager fd */
	int	wait_sec = MIN_WAIT;		/* seconds to wait before conneting to repmgr */
	int 	st;				/* close status */
	int	need_exit = 0;			/* exit was in buffer, need to send to repmgr alone */
	int	no_buffers = 0;			/* flag for verbose to indicate that we dropped write b/c no buffers rather than time */
	
	pthread_t me;

	me = pthread_self( );

	if( verbose )
		fprintf( stderr, "%s writer started: %x\n", timestamp( ), me );
	
	while( 1 )
	{
		if( wait_sec )
			sleep( wait_sec );

		no_buffers = 0;
		if( (b = dq_buf( )) )		/* get next buffer if one queued during our nap (we get NULL if on hold or !ok2send) */
		{
			wait_sec = 0;					/* no delay if we leave the session up */

			while( rfd == FD_CLOSED )			/* it blocks, but can return closed if something buggers up */
			{
				rfd = connect2rm( );				/* connect2 will block until its established */
				force_flush = time( NULL ) + max_conn_time; 	/* hold connection not more than this num of seconds*/
			}
	

			need_exit = chk4exit( b );		/* check for exit; remove if its there (we send later) */

			if( send_buf( rfd, b ) < b->n )		/* return will be > b->n if n was zero before above dec; this is ok */
			{
				if( errno )			/* system set errno, not a typical disconnect */
					fprintf( stderr, "%s writer: send to repmgr failed: %s\n", timestamp( ), strerror( errno ) );

				push_buf( b );				/* push buffer back on queue */
				force_flush = 0;			/* force close */
			}
			else
			{
				log_buf( b );				/* parse the buffer and log interesting things to our private log */
				written += b->n;			/* bytes written */
				bwritten++;				/* buffers written */
				add2_cache_set( b );			/* add to replay cache */

				if( need_exit )
				{
					fprintf( stderr, "%s sending exit; blocking further send attempts until repmgr restarts\n", timestamp( ) );
					send_exit( rfd );			/* send a single message with exit so nothing trails it */
					flags &= ~FL_OK2SEND;		/* stop sending until rm_start restarts and sends us an ok2send */
					force_flush = 0;		/* force a close now */
					wait_sec = MIN_WAIT * 2;	/* no need to try again for longer than usual */
				}
			}
		}
		else					/* nothing queued */
		{
			no_buffers = 1;
			wait_sec =  MIN_WAIT;		/* no data; we will close, then wait a min amount of time before next dq attempt */
			force_flush = 0;		/* exipre timer if rfd is open -- forces close */
		}

		if( rfd != FD_CLOSED && (time( NULL ) > force_flush || written >= MAX_WRITE) )
		{
			int val = 1;
			unsigned char ch = EOF_ACK_CHAR;

			if( verbose )
				fprintf( stderr, "%s writer: %s to repmgr: wrote %ld bytes from %ld buffers: %s\n", timestamp( ), need_eod_ack ? "sending eod" : "closing session", written, bwritten, written > MAX_WRITE ? "max bytes written" : (no_buffers ?  "queued data exhausted" : "time expired") );

			if( need_eod_ack ) 
			{
				long long t1;
				
				char ack_msg[1024];

				write( rfd, &ch, 1 );
				if( verbose > 1 )
					fprintf( stderr, "%s wrote end of data; waiting for eod ack\n", timestamp( ) );

				t1 = time( 0 );
				errno = EWOULDBLOCK;					/* ng_read does not set errcond, so this helps it along */
				if( ng_read( rfd, ack_msg, 10, max_ack_wait ) <= 0 )		/* wait up to 3 min for ack -- before closing session */
					fprintf( stderr, "%s timeout waiting for end of data handshake from repmgr: %s\n", timestamp(), strerror( errno ) );
				else
					if( verbose )
						fprintf( stderr, "%s got eod ack from repmgr; elapsed=%llds\n", timestamp( ), ((long long)time( 0 )) - t1 );
			}

			if( (st = close( rfd )) < 0 )
				fprintf( stderr, "%s writer: close of session to repmgr failed: %d %s\n", timestamp( ), st, strerror( errno ) );
			else
				if( verbose > 2 )
					fprintf( stderr, "%s writer: close of session to repmgr status: %d\n", timestamp( ), st );

			rfd = FD_CLOSED;			/* no session to repmgr any more */
			written = 0;
			bwritten = 0;

			wait_sec = wq_head ? PAUSE_SEC : MIN_WAIT;		/* shorter wait if things still queued */
		}

		if( !wq_head && !ok2run && open_listens <= 0 )	/* draining && no bufs && no listeners pending -- time to exit */
		{
			if( rfd != FD_CLOSED )
				close( rfd );				/* be polite */
			fprintf( stderr, "%s writer: thread closing open_listens=%d  ok2run=%d  wq_head=%x\n", timestamp( ), open_listens, ok2run, wq_head );
			pthread_exit( NULL );
		}
	}			/* end for-ever */

	return NULL;			/* keep compiler happy */
}

/* ------------------------------------------------------------------------------------------- */

static void usage( )
{
	/* -p -r are required now -- no access to ng_env() */
	fprintf( stderr, "%s %s\n", argv0, version );
	fprintf(stderr, "Usage: %s [-a] [-R nreaders] [-l logfile] [-v] -p port -r port\n", argv0);		
	exit( 1 );
}

int
main(int argc, char **argv)
{
	int	nreaders = 3;			/* number of reader threads to create */
	int	listen_fd = FD_CLOSED;		/* fd opened for listening */
	int	fd;				/* fd of a newly accepted session */
	int	i;	
	int	cstatus;		/* status for wait */
	int	ppid;			/* parent's id for kill */
	pthread_t w_thread;
	pthread_t *r_threads;


	ARGBEGIN{
		case 'a':	need_ack = 1; break;
		case 'A':	need_eod_ack = 0; break;
		case 'p':	SARG( port ); break;
		case 'l':	SARG( log_fname ); break;		/* place for logging selected messages */
		case 'R':	IARG( nreaders ); break;
		case 'r':	SARG( rport ); break;
		case 's':	SARG( rmhost ); break;			/* where repmgr is running */
		case 'v':	OPT_INC(verbose); break;
		default:	usage(); exit( 2 );
	}ARGEND

	if(argc != 0){
usage:
		usage( );
		exit(1);
	}

	if( ! rmhost )
		rmhost = strdup( "localhost" );
	else
		rmhost = strdup( rmhost );		/* we expect to be able to free this, dont keep pointer from argv */

	if( verbose )
	{
		fprintf( stderr, "%s %s\n", timestamp(),  version );
		if( ! log_fname )
			fprintf( stderr, "%s main: no log file name provided; messages will not be logged\n", timestamp( ) );
		fprintf( stderr, "%s repmgr host (Filer): %s\n", timestamp( ), rmhost );
	}

	switch( fork( ) )			/*  immediately create child to do the work; parent just relaxes */
	{
		case 0: break;				/* child -- keep on truck'n */
		case -1: 	fprintf( stderr, "%s main: could not fork --- abort\n", timestamp()  );
				exit( 2 );
				break;

		default:
				signal( SIGUSR1, sigh );			/* child should signal when it enters drain state */
				wait( &cstatus );
				fprintf( stderr, "%s main/parent: child seems dead -- abort\n", timestamp() );
				exit( 3 );			/* child must have died */
				break;
	}

	setup_replay( );		/* initialise the replay environment */

	if( !rport || !port )		/* these now must be set from cmd line */
	{
		usage( );
		exit( 1 );
	}

	signal( SIGPIPE, SIG_IGN ); 		/* if we miss a repmgr initiated disco, sigpipe is thrown on send. we trap send error */
	
	memset( fd_ready, FD_CLOSED, sizeof( fd_ready ) );

	pthread_mutex_init( &q_mutex, 0 );					/* mutex for fd_ready and write queue access */
	pthread_mutex_init( &fd_mutex, 0 );

	r_threads = (pthread_t *) bt_malloc( sizeof( pthread_t ) * nreaders );			/* initialise and start threads */

	fprintf( stderr, "%s starting %d readers\n", timestamp( ), nreaders );
	for( i = 0; i < nreaders; i++ )
	{
		if( pthread_create( &r_threads[i], NULL, reader, NULL ) != 0 )		
		{
			fprintf( stderr, "%s main: reader %d thread creation failed: %s\n", timestamp( ), i, strerror( errno ) );
			exit( 1 );
		}
	}

	if(  pthread_create( &w_thread, NULL, writer, NULL ) != 0 )
	{
		fprintf( stderr, "%s main: writer thread creation failed: %s\n", timestamp( ), strerror( errno ) );
		exit( 1 );
	}


	if( verbose )
		fprintf( stderr, "%s main: opening listing port: %s\n", timestamp( ), port );

	if( (listen_fd =  ng_serv_announce( port )) < 0 )
	{
		fprintf( stderr, "%s main: open of listen port failed: %s\n", timestamp( ), strerror( errno ) );
		exit( 1 );
	}


	while( ok2run && !close_listen ) 				/* main drives the listener until ok2run is turned off */
	{
		if( (fd = ng_serv_probe_wait( listen_fd, MAX_IDLE )) > 0 )	/* pick up a new connection */
		{
			if( verbose > 3 )
				fprintf( stderr, "%s main: session accepted on fd %d\n", timestamp( ), fd );
			putfd( fd );				/* insert fd for a reader to process */
		}
	}

	fprintf( stderr, "%s main: listen port is now CLOSED\n", timestamp( ) );
	close( listen_fd );			/* stop listening imediately so that another may start in its place asap */

	listen_fd = FD_CLOSED;

	ok2run = 10;
	while( ok2run-- > 0 && fd_ready[fd_next] != FD_CLOSED )		/* wait until all accepted sessions are read */
		sleep( 1 );							/* or a max of 10 seconds */

	ok2run = 0;				/* time to shut off listeners and start to bug out */
	flags &= ~FL_HOLD;			/* we will never send the data. with the listen port closed we'll never get a cts; so */
	flags |= FL_OK2SEND;			/* once the listen port is closed, we must force ok2send ON and hold OFF, if we dont  */
	fprintf( stderr, "%s main: continuing shutdown, setting ok2run OFF\n", timestamp( ) );

	ppid = getppid( );			/* get mama */
	if( ppid > 1 )				/* we were not orphaned */
	{
		fprintf( stderr, "%s main: signaled parent (%d) with drain inication\n", timestamp( ), ppid );
		kill( ppid, SIGUSR1 );		/* cause mama to pop off */
	}
	else
		fprintf( stderr, "%s main: ppid was 1 -- not expected but not fatal\n" );


	fprintf( stderr, "%s main: entering drain state: open listeners=%d wq=%x mypid=%d\n", timestamp( ), open_listens, wq_head, getpid( ) );

	if( wq_head )
		sleep( 10 );
	pthread_join( w_thread, NULL );		/* wait for writer to finish */

	fprintf( stderr, "%s main[%d]: drained: open listeners=%d wq=%x\n", timestamp( ), getpid(), open_listens, wq_head );

	/* reset tty to blocking; threads leave it unset and if someone is running this manually the tty will be buggered on exit if we dont */
	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL ) & ~O_NONBLOCK );	
	exit( 0 );				/* return in Linux hangs! */
}


#ifdef KEEP 
/* original -- reference */
void flushbuf(char *port);

	int k;
	int serv_fd, fd;
	int iport;
	int going;

	
	if(verbose > 1)
		fprintf(stderr, "%s: attempting ipc from %s\n", argv0, port);
	if((serv_fd = ng_serv_announce(port)) < 0){
		if(verbose)
			fprintf(stderr, "%s: announce failed: using port %s\n", argv0, port);
		exit(1);
	}

	buf.a = 250000;
	buf.n = 0;
	buf.b = ng_malloc(buf.a, "buffer");
	buf.t = time(0);

	ng_bleat(2, "entering main loop");
	for(;;){
		ng_bleat(2, "probing: ");
		fd = ng_serv_probe_wait(serv_fd, MAX_IDLE);
		ng_bleat(2, "probe returns %d", fd);
		if(fd < 0)
			return 0;
		if(fd){
			while((k = read(fd, buf.b+buf.n, buf.a-buf.n)) > 0){
				for(i = 0; i < k; i++)
					if((0xFF&buf.b[buf.n+i]) == EOF_CHAR){
						buf.n += i;
						goto done;
					}
				buf.n += k;
				if((buf.a-buf.n) < 1024){
					buf.a *= 2;
					buf.b = ng_realloc(buf.b, buf.a, "buffer");
				}
			}

		done:
			close(fd);
		} else
			flushbuf(rport);
		if(buf.t+MAX_FLUSH <= time(0))
			flushbuf(rport);
	}

	/* cleanup */
	ng_serv_unannounce(serv_fd);
	ng_bleat(1, "exiting normally");

#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_repmgrbt:Replication Manager Batch Input -- threaded)

&space
&synop	ng_repmgrbt [-R nreaders] [-v] -p port -r repmgr-port 

&space
&desc	&This listens on &ital(port) for sessions and reads data from remote 
	processes once they connect. Data from all connected processes is 
	buffered until the end of transmission character is received, and then 
	is sent to the &ital(replication manager).   Once the end of transmission 
	byte has been received, the data may be held by &this for as long as a 
	minute before a session is established with replication manager. 
&space
	If &this is not able to create a session with replication manager, then 
	it will patiently retry for ever.  While it retries, &this continues to 
	accept and buffer replication manager request data. 
&space
	The script &lit(ng_rmbt_start) should be used to start &this; &this should 
	not be started directly.  When the start script is run log messages are
	written to &lit($NG_ROOT/site/log/repmgrbt).  While running, the start 
	script waits for &this to terminate and when it does &this will be 
	restarted without delay. If the restart fails (Linux seems to have an
	issue with socket reuse), then the script will repeat its attempts to 
	bring &this back up after small delays. 
&space
	The &lit(ng_rmbt_start) script is also used to stop and cycle &this.
	Please refer to the manual page for &lit(ng_rmbt_start) for details on 
	the options that are needed to do this. 

&space
&subcat Threads
	&This is a treaded application, and because of the AST limitations on 
	thread support &this carefully avoids the use of any Ningaui library 
	routines and/or any AST functions.  This avoidance is especially 
	noticed in the fact that &this cannot and/or does not:
&beglist
&item	Use &lit(ng_env) to obtain cartulary variables. Variables that are exported 
	to the environment when &this is started are available, but their use
	is also avoided because unlike the values returned by &lit(ng_env) these
	values will not reflect changes made to the cartulary while &this is running.
&space
&item	Use &lit(ng_bleat) or &lit(ng_log) to write informational or log messages. 
&space
&ital	Include &lit(ngingaui.h) during compilation, and &stress(does) require a 
	special &lit(ng_arg.h) header file to exclude the AST function references
	that exist in the &lit(ARGBEGIN)/&lit(ARGEND) macros.

&endlist
&space
	There are three library routines (ng_rx1, ng_serv, and ng_testport) that 
	are needed and are compiled into &this when it is built. 

&space
&subcat	Draining queued data at shutdown
	When &this is started a child process is forked which actually does 
	all of the work.  When an exit message is received, the child process
	signals the parent and the parent exits with a good return code.
	The child process, now orphaned, stops listening on its well known port, 
	continues to read data on any open socket, and to send buffered data 
	to &ital(replication manager). It will exit when all data has been 
	sent to replication manager. 
&space
	This dual process model is used to allow the start script to immediately	
	restart &this if it is necessary reducing to a minimum the time that processes are 
	unable to send information to &ital(replication manager.) The fork is done 
	at the beginning of processing, rather than as the drain state is entered, 
	because there was an issue with reading from open sockets in the threads
	of the child process. 
&space
&subcat	Message Logging
	Each chunk of data that is forwarded to replication manager is parsed and 
	interesting messages are logged, if the &lit(-l logname) option was supplied
	on the command line. The messages are logged as is, and are expected to 
	be newline terminated strings which begin with one of the following 
	blank separated tokens: file, unfile, set, unset, hood, or synch.  The log 
	file is opened in append mode and if it must be trimmed, maintained, or rolled
	&this expects the start script to do so before this daemon is invoked. 

&space
&subcat Resending Data
	Buffers that have been sent to replication manager are now cached and a command interface allows
	them to be resent to repmgr (replayed).  Repmgr is expected to send
	marker commands which mark the cache replay locations.  When a 
	replay request is received, it specifies a marker name and all 
	buffers from that marker to the end of the cache are resent.  
	Buffers in the replay cache between markers X and Y are regarded
	as the cache set X. We keep just 5 sets in the replay cache. 
&space
	This allows replication manager, and/or its start script, to initalise
	from a checkpoint file, and then to roll forward from using data in the 
	replay cache if it is available. The user interface to &this for replay cache management is 
	supplied via &lit(ng_rmbtreq). 

	Date:		08 Apr 2004
&space
&subcat Commands
	Commands can be sent to &this via &lit(ng_rmbtreq) or via some other 
	mechanism that can send a TCP/IP message to &this.  Commands have the 
	syntax &lit(~commandname~[data]). While it may not be convenient to 
	place command names between tildas, it prevents legitimate replication 
	manager requests from being interpreted as commands by &this.  
	When using &lit(ng_rmbtreq) the tilda characters are added to the command
	parameters making it easier for the user (please refer to the ng_rmbtreq
	manpage for more information).  Before the introduction of &lit(ng_rmbtreq)
	commands were expected to arrive via &lit(ng_rmreq) which is a 'one way'
	interface and thus with older commands the results are written to the log
	file and not back to the user. 
	The following describes the commands recognised by &this in the form that 
	the messages are expected (with tildas):
&space
&begterms
&term	~ackeod~ : Toggles the need for &this to send an end of data received ack request
	after all data has been sent to replication manager. 
&space
&term	~ackwait~n : Allows the timeout (n seconds) to be set. This is the max amount of 
	time that &this will wait for an 'end of data received' ack message from replicaiton 
	manager. The default value is 240 seconds. 
&space
&term	~exit~ : Causes &this to gracefully exit. If the start script &lit(ng_rmbt_start) 
	has not been asked to exit prior to sending the exit command to &this, &this 
	will automatically be restarted. 
&space
&term	~verbose~n : Sets the verbose level of &this to n. A value of 0 turns verbosity
	off. A value of 1x turns on the logging of all commands that are not normally 
	traced. These are logged to stderr as unknown commands.
&space
&term	~hold~ : Toggles a hold flag within &this.  When the hold flag is set, no
	data is passed to replicaiton manager. The state of the hold flag after 
	this command is received is reflected in the log file regardless of the
	verbosity level. The hold setting is cleared when an &lit(ok2send) message
	is received. 
&space
&term	~dump~filename : Causes the current cache of replication manager information 
	to be written to filename.  The information is in the order and format 
	that it was receeived from &lit(ng_rmreq) and can be used to prevent information 
	loss in the event that &this becomes unable to communicate with replication 
	manager and is still responsive to commands (TCP/IP under linux has rendered
	this necessary).
&space
&term	~pdump~filename : Causes a dump as if the &lit(~dump~) command were supplied, then 
	purges all buffers that were dumpped. 
&space
&term	~ok2send~ : This command is expected from ng_rm_start and indicates that it is ok
	to once again start sending data to replication manager. When an exit command is 
	noticed in the stream of data forwarded to replication manger, the interal ok to 
	send flag is turned off.  &This will not attempt to send any data until it is 
	recycled, or this message is received.  This handshake is necessary as the processing
	of the exit command by replication manager may not be instantanious and might 
	allow &this to establish a connection and send data that would be lost. 
&space
&term	~rcmark~name : Starts a new cache set with the indicated &ital(name). 
&space
&term	~rcplay~name : Causes the buffers in the replay cache to be resent to the replication
	manager starting with the cache set &ital(name). All buffers from the indicated
	cache set until the end are resent.
&space
&term	~rclist~ : Causes a list of all replay cache set markers to be returned on the TCP/IP
	session. The number of buffers in each set is also listed. 
&space
&term	~rcdump~name : When this command is received, &this will write all buffers from the named
	cache set, to the end of the replay cache, to the TCP session.  Buffers are written in 
	the same order as they would be sent to the replication manager if the play command is
	used. 
&space
&term	~rcfind~name : Searches for the &ital(name) in the list of cache sets and writes
	information about that set to the TCP session.  The information is a leading 1 or 0 (character)
	indicating success or failure. The remainder of the buffer is a description of the 
	cache set which will likely be just the count of buffers in the set. 
&endterms

&space
&subcat Assumptions
	Several assumptions have been made:
&space
&beglist
&item	&ital(Replication manager) is running on the same host as &this.  
&space
&item	If &this is not able  to communicate with &ital(replication manager,)
	&this will keep trying with the assumption that &ital(replication manager)
	will be down only for a small number of minutes.  During busy periods,
	if &ital(replication manager) remains down for an extended period of time, 
	&this is likely to crash due to excessive memory use. (we might consider 
	putting in a 'pop-off' value to try to avoid the loss of all queued
	data, but at present if repmgr does not come back, things are lost. 
&endlist

&space
&opts	The following flags are recognised by &this when placed on the command line:
&begterms
&term	-a : Need clear to send ack.  When this flag has been set on the command line, 
	&this will wait for a one byte transmission from replication 
	manager before sending any data. This allows &this to be sure that the 
	remote process actually did an accept, and that the session was not establishe
	into thin air. 
&space
&term	-A : Surpress sending the end of data ack request to replication manager after
	all buffers have been sent.  This is probably needed only if communicating with 
	an older version of replication manager.
&space
&term	-l file : Specifies the name of he log file that will be used to log interesting 
	messages that are passed to replication manager. If not supplied, messages are
	not logged.
&space
&term	-R n : Sets the number of reader threads to &ital(n.) By default three
	reader threads are started. There must be at least two reader threads started or 
	shutdown may hang the port on some Linux flavours. 
&space
&term	-v : Verbose mode. The more -v flags, the more noise is generated.
&endterms

	These flags are required on the command line:
&begterms
&term	-p port : The port that &this will listen on.  Typically this is supplied 
	as &it(NG_RM_PORT).  This flag is &stres(required) because &this is not 
	able to pull the value of &lit(NG_RM_PORT) from the cartulary.
&space
&term	-r rm-port : The port that &ital(Replication Manager) is listening on.
	This is generally specified as &lit($NG_REPMGRS_PORT) and must be 
	supplied on the command line for the same reasons as the listen port is.

&endterms


&space
&exit	An exit state of zero (0) indicates that processing finished normally.
	Any non-zero exit state indicates that there was a problem and that a 
	message should have been written to the standard error device in an attempt
	to  explain the abnormal termination.
	

&space
&see	&ital(ng_rmbt_start), &ital(ng_repmgr), &ital(ng_rmreq), &ital(ng_sendtcp),
	&ital(ng_rmbtreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	14 Apr 2004 (sd) : The introduction of the multi-personality code and doc.
&term	03 May 2004 (sd) : Added -a option. 
&term	05 May 2004 (sd) : Added logging, bumped version to 1.2.
&term	07 Jul 2004 (sd) : Now logs all unrecognised commands when verbose > 5.
&term	30 Sep 2004 (sd) : Added hold, dump, pdump commmands. (HBD KDG)
&term	10 Jan 2005 (sd) : Now looks for exit in send buffer and blocks sending until an 
		ok to send message is received back from rm_start.
&term	14 Jan 2005 (sd) : Check for exit now allows for whitespace to follow the exit token.
&term	16 Jan 2005 (sd) : Can set max connect time via ~conn~sec command (between 10 and 60).
&term	22 Jan 2005 (sd) : Reroganised which messages are generated at which verbose levels.
&term	24 Mar 2005 (sd) : Changed the default connection time to 40s.
&term	25 Mar 2005 (sd) : Putzing to get the testport delay worked out. 
&term	06 Jun 2005 (sd) : Now uses rx1 to avoid the exponential back-off used by ng_rx.
&term	07 Jun 2005 (sd) : Corrected length issue on send. (v1.6)
&term	04 Aug 2005 (sd) : Dropped the extra newline when we dummy up an exit buffer. (Prevents
		repmgr from reporting unknown (null) commands.
&term	14 Sep 2005 (sd) : Fixed what I hope is causing the hanging bt processes after things 
	are cycled.
&term	18 Sep 2005 (sd) : The extra newline removed on 8/4/05 was not the problem causing 
	parse errors in repmgr.  It was the fact that the exit string was being sent with a 
	trailing null.  Removed. 
&term	09 Jun 2006 (sd) : Really fixed the problem causing the child process not to 
	exit on shutdown.  It was sticking in a loop that was not testing ok2run. (HBD SEZ)
&term	03 Oct 2006 (sd) : Added replay cache and associated user interface commands. 
		Added ability to execute &this on a node that is not the same as the filer
		(service manager support). 
		(V2.0 HBD SC)
&term	29 Jan 2007 (sd) : Fixed issue where port was being held open under some flavours of 
		Linux.  Cause was stopping all listener threads before all data was read from 
		the inbound sessions. 
		.** This caused at least one stuck CLOSE_WAIT which prevented
		.** the restart of bt if we were just bouncing the node.
&term	09 Feb 2007 (sd) : Fixed a bug with shutdown; writer was blocking when &this was sent
		an exit and there was queued data (it was expecting a cts which would never
		come). V2.2
&term	01 Mar 2007 (sd) : Corrected the counter that was reporting number of bytes sent. Was
		counting the end of data byte from rmreq that is NOT sent to repmgr. (V2.3)
&term	06 Mar 2007 (sd) : Added ability to send an end of data mark to repmgr, rather than just
		closing the session, and waiting for an ack byte back.  (v2.4)
&term	21 Mar 2007 (sd) : Changded -A to turn off the eod ack flag; added max_ack_wait 
		var and ability to set it via ng_rmdbdreq. (v2.4/03217)
&term	18 May 2007 (sd) : Finally may have fixed the problem that was causing threads to hang on linux. (v2.5)
&term	30 Oct 2007 (sd) : Made one more adjustment that was preventing threads from exiting properly under linux (v2.6). 
&term	27 Feb 2008 (sd) : Corrected a couple of potential locking issues with replay cache. (v2.7) 
&term	25 Sep 2008 (sd) : Corrected push logic when sending from a replay cache. (v2.8) 
&term	01 Oct 2008 (sd) : Added some extra debugging to try to get to the bottom of the occasional core dumps. 
&term	05 Jan 2009 (sd) : Drop of cache block now prevented during replay. Was causing hard to trace coredumps (v2.9).
&term	28 Jan 2009 (sd) : Ensured that the rmhost name is a dup'd string so that when we get an rmhost command 
		we dont get an error trying to free the argv string.
&endterms

&scd_end
------------------------------------------------------------------ */
