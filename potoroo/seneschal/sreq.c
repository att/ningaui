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
 ---------------------------------------------------------------------------
 Mnemonic:	sreq
 Abstract:	seneschal request interface
 Date: 		20 July 2001
 Author:	E. Scott Daniels
 ---------------------------------------------------------------------------
*/

#include	<string.h>
#include	<time.h>
#include 	<inttypes.h>
#include	<syslog.h>
#include	<unistd.h>
#include	<stdio.h>
#include 	<sfio.h>
#include	<stdlib.h>
#include	<stdarg.h>     /* required for variable arg processing */
#include	<errno.h>
#include	<signal.h>

#include	<sys/socket.h>         /* network stuff */
#include	<net/if.h>
#include 	<netinet/in.h>
#include	<netdb.h>

#include	"ningaui.h"
#include	<ng_lib.h>

#include	"sreq.man"

#define 	FL_FORCE 0x01		/* force a command into seneschal */

static int 	 nfd = -1;		/* file descriptor of udp port */
static int	seq = 0;		/* message sequence number */
static int	flags = 0;		
static char	*version = "2.1/04056";


				/* actions */
/* do not define a zero (0) action!! */
#define	DUMP_COSTS	1
#define DUMP_FILES	2
#define TCP_DUMP	3		/* generic dump via tcp user supplies dump= as parms */
#define SEND_DICTUM	4
#define SEND_EXIT	5
#define SEND_JOB	6
#define SEND_TCP	7		/* send stuff from command line w/out any prefix */

#define NOWAIT		0
#define WAIT4COUNT	1		/* wait for a specified number of messages before closing connection */
#define WAIT4EOTM	2		/* wait for end of transmission message rather than a msg count */

/* ---------- globals for caller to set ------------------------ */

extern int verbose;
int	exit_on_to = 1;

static void sigh( int x )
{
	ng_bleat( 0, "timeout waiting for response" );
	exit( 20 );
}

/* hangout and listen for responses from seneschal. we expect that a message
   containing a single dot character indicates the end of the message stream 
*/
void eavesdrop( int nfd, int timeout )
{
	char	buf[2048];
	char	from[100];
	int 	len;

	ng_bleat( 1, "sreq: waiting for response..." );

	while( (len = ng_readudp( nfd, buf, sizeof( buf ) - 1, from, timeout )) )
	{
		if( *buf == '.' && *(buf+1) == 0 )
			return;					/* end of dump or what ever */

		buf[len] = 0;
		sfprintf( sfstdout, "%s\n", buf );
	}

	sfprintf( sfstderr, "sreq: timeout waiting for response from seneschal\n" );
	if( exit_on_to )
		exit( 1 );
}

/* return the next newline separated record to the caller */
char *tcp_rec( int fd )
{
	static void *flow = NULL;		/* 'handle' into our flow management stuff */
	static char buf[NG_BUFFER];		/* last bit of stream data read  flowmanager manages it from here so must be static */
	int	len = 0;			/* len of last buffer received from the session */
	char	*rb; 				/* pointer to string to return */

	if( ! flow )
	{
		if( (flow = ng_flow_open( NG_BUFFER )) == NULL )	/* setup a flow management environment for processing the tcp stream */
		{
			ng_bleat( 0, "internal error: unable to setup flow manager for tcp stream processing" );
			exit( 12 );
		}
	}

	if( (rb = ng_flow_get( flow, '\n' )) == NULL ) 		/* null returned when all data in the buffer has been processed, or before 1st ref call */
	{
		if( (len = recv( fd, buf, sizeof( buf ), 0 )) <= 0 )		/* read from stream; error we assume session disco */
		{
			ng_flow_close( flow );
			flow = 0;
			return NULL;
		}

		ng_flow_ref( flow, buf, len );		/* reference the new data in the buffer */

		rb =  ng_flow_get( flow, '\n' );
	}

	return rb;
}

/* read from someplace and send to seneschal via tcp.  if wait is not 0, then 
   hang round for status messages 
*/
int do_tcp( Sfio_t *src, char *node, char *port, int wait, char *msg, int timeout )
{
	Sfio_t	*input;		/* converted input */
	int	in;
	int	out;		/* fds from ng_rx */
	char	*buf;		/* input to send */
	long	count = 0;	/* number of status records to wait for */
	char	name[50];	/* we make a synthetic name */

	
	signal( SIGALRM, sigh );		/* call our handler when alarm pops */
	if( timeout )
		alarm( timeout * 2 );		/* double timeout for conection */

	if( ng_rx( node, port, &in, &out ) == 0 )
	{
		if( timeout )
			alarm( 0 );		/* incase wait is off, dont pop */

		if( wait )				/* if waiting, we must name session with seneschal */
		{
			sfsprintf( name, sizeof( name ), "name=sreq-%x-%d\n", ng_now( ), getpid( ) );
			write( out, name, strlen( name ) );
		}

		if( src )						/* send messages from the file */
		{
			while( (buf = sfgetr( src, '\n', 0 )) != NULL )
			{
				count++;				/* wait for a status of each job submitted */
				write( out, buf, sfvalue( src ) );
			}
		}
		else
		{
			count = 1;						/* number of eoms to wait for */
			if( msg )
				write( out, msg, strlen( msg ) );		/* write out the prefab message from option */
			else
				return 1;						/* this smells */
		}

		if( wait )
		{
			if( wait == WAIT4EOTM )			/* means wait for end mark not count */
				alarm( timeout );			/* must have a message every few seconds or we die */

			while( count && (buf = tcp_rec( in )) != NULL )			/* get next buffer from flow */
			{
				if( wait == WAIT4EOTM )			/* means wait for end mark not count */
				{
					if( buf[0] == '.' && buf[1] == 0 )
						if( --count <= 0 )
							break;				/* a time to depart as marcus might say */
						else
							continue;			/* dont write the . out */
				}
				else
				{
					if( buf[0] != '.' )				/* allow an eom to come back from job command */
						count--;
				}

				sfprintf( sfstdout, "%s\n", buf );
			}

			close( in );
		}

		close( out );
	}
	else
	{
		sfprintf( sfstderr, "unable to connect to %s:%s via tcp/ip\n", node, port );
		return 1;
	}

	return 0;
}

/*	build a message to send off. originally seneschal commands were tokens separated with 
	colons so we add whatever seperator the caller passes in. tcp messages to seneschal 
	must have a newline character, so we allow a termination string to be placed 
	into the buffer if supplied.
*/
char *mkmsg( int argc, char **argv, char *init, char *term, char *sep )
{
	char	*buf;
	char	ctok[255];			/* command token with space */
	char	*legit_cmds = "bid dump purge release hold load limit pause resume suspend explain extend verbose maxlease maxrequeue maxwork rmif stable ping verbose nattr node mpn ftrace "; 
	int	len = 0;

	buf = ng_malloc( sizeof( char ) * NG_BUFFER, "command buffer" );
	*buf = 0;

	if( init )
		strcat( buf, init );	/* initialisation string supplied */

	sfsprintf( ctok, sizeof(ctok), "%s ", *argv );
	if( (! (flags & FL_FORCE)) && strstr( legit_cmds, ctok ) == NULL )
	{
		ng_bleat( 0, "%s: unrecognised command: %s", argv0, *argv );
		ng_bleat( 0, "\tlegitimate commands are: %s", legit_cmds );
		exit( 1 );
	}

	while( argc-- )
	{
		if( (len += strlen( *argv ) + 2 ) >= NG_BUFFER )
		{
			sfprintf( sfstderr, "mkmsg: command length exceeded buffer size" );
			exit( 1 );
		}
		strcat( buf, *argv );
		strcat( buf, sep );		/* components of commands are separated by something */
		argv++;	
	}

	if( term )
	{
		if( (len += strlen( term ) + 1 ) < NG_BUFFER )
			strcat( buf, term );
		else
		{
			sfprintf( sfstderr, "mkmsg: adding termination string exceeded buffer len" );
			exit( 1 );
		}
	}

	return buf;
}

static void usage( )
{
	sfprintf( sfstderr, "ng_sreq -e\n" );
	sfprintf( sfstderr, "ng_sreq -c [-p port] [-s host] [-t seconds] [-T] [-v]\n" );
	sfprintf( sfstderr, "ng_sreq -C [-p port] [-s host] [-t seconds] [-T] [-v] audit comment\n" );
	sfprintf( sfstderr, "ng_sreq -f [-p port] [-s host] [-t seconds] [-T] [-v]\n" );
	sfprintf( sfstderr, "ng_sreq [-j] [-p port] [-s host] [-t seconds] [-T] [-w] [-v] job-file-name\n" );
	sfprintf( sfstderr, "ng_sreq [-p port] [-s host] [-t seconds] [-T] [-v] command\n" );
	sfprintf( sfstderr, "\nrun %s --man for a list and description of available commands\n", argv0 );
	exit( 1 );
}


int main( int argc, char **argv )
{
	Sfio_t	*inf = sfstdin;			/* input job file */
	char 	*buf;
	char	dhost[1024];			/* destination host */
	char	*shost = NULL;			/* user supplied service host name */
	char	to[1024];			/* full address */
	char	*port = NULL;
	char	*jobfn = NULL;			/* jobfile filename */
	char	*msg = NULL;			/* message to send to seneschal */
	int	len = 0;			/* length of buffer we are filling */
	int	setexit = 0;			/* send an exit message and bail */
	int	timeout = 10;			/* seconds to wait for a response from seneschal */
	int	i;
	int	action = 0;			/* what user wants to do */
	int	wait = 0;			/* user wants to wait for job status messages */
	int	rc = 0;

	if( ng_sysname( dhost, sizeof( dhost )) )		
		exit( 1 );

	shost = ng_env( "srv_Jobber" );		/* try to default to the right place, if not we assume dhost (localhost) */

	ARGBEGIN
	{
		case 'c':	action = DUMP_COSTS; break;
		case 'C':	action = SEND_DICTUM; break;
		case 'f':	action = DUMP_FILES; break;
		case 'F':	flags |= FL_FORCE; break;		/* force the command in */
		case 'd':	action = TCP_DUMP; break;
		case 'e':	action = SEND_EXIT; break;
		case 'j':	action = SEND_JOB; SARG( jobfn ); break;
		case 'J':	action = SEND_JOB; jobfn = "/dev/fd/0"; break;		/* same as -j, but from stdin */
		case 'm':	action = SEND_TCP; break;
		case 'p':	SARG( port ); break;
		case 's':	SARG( shost ); break;
		case 't':	IARG( timeout ); break;
		case 'T':	exit_on_to = 0; break;		/* tolerate timeouts and continue */
		case 'w':	wait = WAIT4COUNT; break;
		case 'v':	verbose++; break;
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	ng_bleat( 1, "%s version %s", argv0, version );

	if( ! port )
		port = ng_env( "NG_SENESCHAL_PORT" );

	if( ! port )
	{
		sfprintf( sfstderr, "cannot find port for seneschal (NG_SENESCHAL_PORT)" );
		exit( 1 );
	}

	sfsprintf( to, sizeof( to ), "%s:%s", shost ? shost : dhost, port );
	ng_bleat( 1, "sreq: sending command(s) to seneschal @ %s", to );


	switch( action )
	{
		case DUMP_COSTS:
			flags |= FL_FORCE;
			exit( do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, "dump=costs\n", timeout )  );
			break;

		case DUMP_FILES:
			flags |= FL_FORCE;
			exit( do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, "dump=files\n", timeout ) );
			break;

		case TCP_DUMP:
			break;

		case SEND_DICTUM:
			flags |= FL_FORCE;
			msg = mkmsg( argc, argv, "dictum=", "\n", " " );
			ng_bleat( 1, "sending: %s", msg );
			exit( do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, msg, timeout ) );
			break;

		case SEND_TCP:
			msg = mkmsg( argc, argv, NULL, "\n", " " );
			ng_bleat( 1, "sending: %s", msg );
			exit( do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, msg, timeout ) );
			break;

		case SEND_EXIT:
			flags |= FL_FORCE;
			exit( do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, "xit:\n", timeout ) );
			break;

		case SEND_JOB:
			if( jobfn )		/* assume file name of job file */
			{
				if( (inf = ng_sfopen( NULL, jobfn, "Kr" )) == NULL )
				{
					ng_bleat( 0, "unable to open job file: %s: %s", *argv, strerror( errno ) );
					exit( 1 );
				}

				exit( do_tcp( inf, shost ? shost : dhost, port, wait, NULL, timeout ) );
			}
			break;

		default:
			break;
	}

/* no specific action, build a udp command from the command line things (separated by :) or read from stdin and send */

	if( ! argc )		/* nothing on command line, read from stdin */
	{
		while( (buf = sfgetr( sfstdin, '\n', SF_STRING )) != NULL )
		{
			for( i = 0; buf[i]; i++ )
				if( buf[i] == ' ' )
					buf[i] = ':';			/* all commands have components separated */
			ng_bleat( 1, "sreq: sending: %s", buf );
			if( (rc = do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, msg, timeout ))  )
			{
				exit( rc );
			}
		}

	}
	else
	{
		msg = mkmsg( argc, argv, NULL, "\n", ":" );
		ng_bleat( 1, "sreq: sending: %s", msg );
		rc = do_tcp( NULL, shost ? shost : dhost, port, WAIT4EOTM, msg, timeout );

		free( msg );
	}
	
	return rc;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sreq:Seneschal request user interface)

&space
&synop	ng_sreq -e 
&break	ng_sreq -c [-p port] [-s host] [-t seconds] [-T] [-v]
&break	ng_sreq -C [-p port] [-s host] [-t seconds] [-T] [-v] audit comment
&break	ng_sreq -f [-p port] [-s host] [-t seconds] [-T] [-v]
&break	ng_sreq [-p port] [-s host] [-t seconds] [-T] [-v] command
&break	ng_sreq [-j] [-p port] [-s host] [-t seconds] [-T] [-w] [-v] job-file-name

.dv s ^&lit(ng_seneschal)
.dv S ^&lit(Ng_seneschal)

&space
&desc	&This provides a simple communication interface to submit
	job request records or commands to &s.
	If the job submission option (&lit(-j)) is supplied from 
	the command line, then either &ital(job.file) or standard input 
	is expected to contain the job request records that are to be 
	sent to &s.
	Otherwise, &this assumes that one or more commands are to be 
	transmitted to &s. If &ital(command) 
	is entered on the command line, then it is passed to &s
	and any response from &s is written to the standard output device. 
	If &ital(command) is omitted from the command line, then 
	&this reads commands from the standard input device and sends them to &s. 
	All input records, job request or commands, read from a file or the 
	standard input device are assumed to be newline terminated.
	
&space
&subcat	Commands
	The following is a list of commands that &this is capable of 
	passing to &s.
&lgterms
&term	dump n : Causes &s to dump various pieces of information regarding 
	the jobs that it is managing. The value of &ital(n) controls the 
	information type and amount that is generated. The &s documentation 
	should be consulted to determine the desired value of &ital(n).
&space
&term	explain <jobname>|all|<queuname> : Causes &s to give an explination of the state 
	that &ital(jobname) is currently in. Queue name is one of: &lit(pending, missing, complete,)
	or &lit(running.)
&space
&term	ftrace <filename> : Toggles the trace flag on the named file. The file 
	must be known to seneschal (one or more queued jobs must reference it).
	When the trace flag is set, a messages is written to seneschal's private log
	whenever seneschal sees dump1 information about the file, or the file reference
	is altered. These messages are written regardless of verbosity level and have
	the tag 'ftrace:' after the time/date stamp.
	
&space
&term	extend <jobname> seconds : Causes the lease for &ital(jobname) to be 
	changed to seconds. If seconds is zero, then the job is failed and
	possibliy requeued for another attempt. 
	
&space
&term	limit <resource> <value> : Sets the resource limit for &ital(resource)
	to &ital(value). If &ital(value) is suffixed with an &lit(h), then 
	the limit is a hard limit.

&space
&term	load <nodename> <value> : Sets a load value of &ital(value) for &ital(node), 
	afterwich &s will attempt to keep &ital(value) jobs running on the node. 
	It is anticipated that at some point in the future &ital(value) will be
	&ital(seconds of work) which directly relate the &ital(job cost) that is 	
	supplied on the job request record. 
&space
&term	mpn <resource> <value> : Sets the max per node value for the named resource.
	When set greather than 0, &this will start no more than &ital(value) jobs
	on any given node for jobs that reference (consume) &ital(resource). If the 
	value is less than or equal to 0, then the number of jobs which reference the 
	resource is constrained only by other resource limits and node bids. 

&space
&term	nattr <nodename> <attribute-list> : Sends the attributes for the indicated 
	node to &s.  The attributes are space separated tokens and must be quoted on 
	the command line to ensure that &this recrognises them as a single paramter.

&space
&term	pause : Toggles the master supension flag in &s.  If the flag is off when 
	this command is passed, then &s is placed into a paused or suspended 
	state.  The state of suspension is indicated on &s summary messages 	
	written to its log, or as output on a dump command.
&space
&term	purge <jobname> : Causes the job to be purged from &s. If the job is currently
	running, no effort is made to stop execution, but any satus returned 
	by the job after it is purged will be ignored.

&space
&term	resume all|<node> : Causes &s to resume scheduling jobs on &ital(node) 
	or on all nodes. 
&space
&term	rmif batch-size seconds : Sets the paramerters for communication with replicaiton 
	manager.  The batch size is the number of file information requests (dump1 requests)
	that are sent in a batch.  The default is 3500 and this should be good for nearly
	all situations.  The second value is the number of seconds between requests. 
	The default value is 15 seconds which should be increased if the batch size is increased. 
	There negative effects on both replication manager and &this if the batch size is 
	too much larger than the default. 
&space
&term	suspend all|<node> : Causes &s to suspend scheduling jobs on &ital(node)
	or on all nodes if &lit(all) is supplied. 
&space
&endterms

&space
&opts	The following options are recognised by &this when entered on the command line.
&begterms
&term	-c : Lists a series of job_type consumption data to the standard output 
	device.  Information listed is the job type name, the average time in 
	seconds/size that the type of job requires on average, and the number 
	of observations that were used to calculate the utiliisation. In general
	job size is measured in K bytes. The time information is based on the 
	&stress(total) time that was required for the job to execute and 
	for the status to be returned to &ital(senescha.) This includes 
	any time that the job was queued on the remote node by &ital(woomera)
	and thus is likely not accurate enough to determine the actual throughput
	of a node. 
&space
&term	-C : Causes the command line positional parameters to be sent as an audit
	comment and recorded by seneschal in the master log as such.
&space
&term	-e : Send an exit command to &s. 
&space
&term	-f : Request a list of files that pending jobs need. 
	The output written to stdout will consist of one record per file with the 
	syntax of:
&space
&term	-F : Force the command to be sent to seneschal even if it does not pass
	the legitimate command validation.  This allows a recovery mode if seneschal 
	has a command added, but &this is not upgraded.
&space
&litblkb
	needed_file: <filename> nodes={<node>,<node>,...} jobs={<jobname>,<jobname>}
&litblke
	The node list is the nodes on which the file currently resides, and the 
	job list is the list of jobs that require the file as input. 
&space
&term	-j filename : Assumes that job submission records are to be read from the named
	file and sent to &ital(seneschal). &ital(Filename) may be &ital(/dev/fd/0).
&term
&term	-p n : Use &ital(n) as the port number when communicating with &s rather 
	than the port number defined in the cartulary.
&space
&term	-s host : Attempt to communicate with &s on &ital(host) rather than with 
	&s on the local host.
&space
&term	-t sec : Sets the timeout value to &ital(sec) seconds. &This will wait up 
	to &ital(sec) for a response from &s, and consider the communication attempt
	to be in a timeout state if &ital(sec) seconds pass without a response. 
	Unless the tollerate timeouts option is set, a timeout will cause &this 
	to immediately exit with an error condition. If not supplied, the 
	default timeout value is ten (10) seconds.
&space
&term	-T : Tollerate timeouts from seneschal. If a timeout is encountered &this 
	will continue as though the end of the message set was received. When 
	reading commands from the standard input &this will read and submit the 
	next command rather than exiting when a timeout is noticed.
&space
&term	-v : Verbose mode.
&space
&term	-w : Wait for status reponses from jobs submitted with &lit(-j) option. &This
	assumes that there will be one status record for each record of input.
&endterms

&space
&parms	Parameters expected depend on the option flag provided.  If none of the 
	&lit(-c, -f,) or &lit(-C) options are provided, then
	all parameters entered after the options on the command line are considered 
	to be a single command string to pass to &s.

&space
&exit	Under normal circumstances, &this will exit with a zero (0) return code. If
	&this detects an error then a nonzero return code is detected.  If, after 
	sending a request to &s, &this has not recevied a response within ten (10)
	seconds, a timeout condition will be assumed and &this will exit with 
	a nonzero return code.

&space
&see	ng_seneschal

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 20 Jul 2001 (sd) : The first crumbs of the cookie.
&term 09 Oct 2001 (sd) : To support submitting jobs via tcp/ip.
&term 18 Jun 2002 (sd) : To support interface to Andrew's new daemon.
&term	06 Mar 2003 (sd) : To add dictum, and revamp logic from the tcp bandaids applied earlier.
&term	06 Oct 2003 (sd) : Added command token check to generte an error message for unrecognised
	commands.  -F overrides this check should seneschal get a new command that does not 
	get listed here. 
&term	03 Dev 2003 (sd) : To add node attribute support
&term	03 Feb 2004 (sd) : Added better timeout when sending non job commands. 
&term	11 Jun 2004 (sd) : Added support for mpn command.
&term	05 Apr 2006 (sd) : Converted to use flow manager (ningaui lib) instead of sfio to manage
		data coming in on the tcp session.  Flow manager is MUCH faster than sfgetr(). (v2.1)
&term	14 Sep 2006 (sd) : Added rmif to list of accepted commands. 
&endterms

&scd_end
------------------------------------------------------------------ */
