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
* -----------------------------------------------------------------------
*  Mnemonic: ng_log.c
*  Abstract: This is the main program that shell scripts interface with 
*            to get things into the master log.  It should not be confused
*            with the library function ng_log
*  Date:     16 July 1997
*  Author:   E. Scott Daniels
*
* ------------- self contained documentation at end ---------------------
*/
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sfio.h>
#include "ningaui.h"

#include "ng_log.man"

extern int verbose;
static char *version = "2.0/09228";

int main( int argc, char **argv )
{
	extern int ng_log_sync;     /* flag in ng_log library routine for synch */
	int pri = 6;                    /* priority */
	char	*bname;			/* pointer at caller's basename */
	char *nextp;
	char *id;                   /* id associated with the prioroty number */
	char *logname;              /* alternate log name to use */
	char *msg;                  /* pointer to user's message */
	char name[50];              /* buffer to build name and ppid in */

	ARGBEGIN
	{
		case 's': ng_log_sync = 1; break;   /* force sync before close of file */
		case 'v': OPT_INC(verbose); break;         /* we dont use it, but lib rtns do */
		default:
usage: 
		sfprintf( sfstderr, "%s %s\n", argv0, version );
		sfprintf( sfstderr, "usage: %s [-s] [-v] priority name \"message-text\"\n", argv0 );
		exit( 1 );
	}
	ARGEND

	if( argc >= 3 )           /* assume second parm is the name of the caller */
	{
		if( (bname = strrchr( argv[1], '/' )) )		/* suns have such long pathnames that things are chopping */
			bname++;
		else
			bname = argv[1];
	
		sfsprintf( name, 50, "%s[%d]",  bname, getppid( ) );
		/*sfsprintf( name, 50, "%s[%d]",  argv[1], getppid( ) ); */
		msg = argv[2];
	}
	else    /* assume priority msg  supplied instead of priority name message */
	{      
		sfsprintf( name, 50, "%s[%d]",  argv0, getppid( ) );
		msg = argv[1];
	}

	argv0 = name;          /* try to reflect who is logging as opposed to us */
	
	if( argc >= 2 )                        /* enough parameters */
	{
		if( strchr( argv[0], ':' ) )    /* user entered pri/log/id string */
		{
			pri = atoi( strtok_r( argv[0], ":", &nextp ) );
			logname = strtok_r( NULL, ":", &nextp );
			id = strtok_r( NULL, ":", &nextp );
			if( strcmp( msg, "/dev/fd/0" ) == 0 )		/* read a  bunch of messages from the stdin device */
			{
				while( (msg = sfgetr( sfstdin, '\n', SF_STRING )) != NULL )
					ng_alog( pri, logname, id, "%s", msg );
			}
			else
				ng_alog( pri, logname, id, "%s", msg );
		}
		else
		{
			pri = atoi( argv[0] );

			if( strcmp( msg, "/dev/fd/0" ) == 0 )		/* read a  bunch of messages from the stdin device */
			{
				while( (msg = sfgetr( sfstdin, '\n', SF_STRING )) != NULL )
					ng_log( pri, msg );
			}
			else
			{
				ng_log( pri, msg );        /* just log the message */
			}
		}
	}
	else
		exit( 1 );

	exit( 0 );
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_log:Write a Ningaui standard message to a Ningaui log.)

&space
&synop	ng_log [-s] [-v] msg-type name "message"
&break	ng_log [-s] [-v] msg-type name "/dev/fd/0" <msg-file

&space
&desc	The &ital(ng_log) binary provides shell programs with a safe
	interface to the master Ningaui log. The message type and message
	parameters are passed to the Ningaui library routine &ital(ng_log)
	and then control is returned to the calling script. The message
	is written according to the rules defined by the &ital(ng_log) 
	function.
&space
	If the "message" on the command line is exactly /dev/fd/0, then
	messages are read from standard input and written to the master
	log and broadcast.  The message type and programme name are 
	the same for each message generated.  This allows a quicker way 
	of dumping information into the master log rather than with 
	repeated invocations of this binary. 

&space
&opts	The following options are recognised by &ital(ng_log):
&begterms
&term	-s : Syncrhonise. When specified, this option causes the 
	process to wait until the log message has been written 
	to disk before it exits. It is suggested that this option 
	be used &stress(only) for log messages that are used to 
	track the progress of the datastore update cycle, or 
	are othewise used when restarting processes after a crash.	
&space
&term	-v : Verbose more. Request library functions to be verbose
	by using -v0x120.  
&endterms

&space
&parms	The binary requires three positonal parameters be passed to it
	via the command line. The following lists these parameters in the 
	order that they are expected.
&space
&begterms
&term	msg-type : This is a number or string that is used to 
	determine where the message is written, and the severity of
	the messages. If &lit(msg-type) is a number, then the 
	message is written to the master log and the msg-type
	is used as an index into a standard string set that will be 
	displayed with the message to indicate the severity of the
	message. These numbers coorespond to the &lit(PRI_) constants
	in the cartulary. If &lit(msg-type) can also 
	be supplied as a colon separated string containing the 
	priority value, log file name (as defined in the cartulary,
	and the ID string used to indicate the meaning of the 
	priority.  
&space
&term	name : Is the name of the process that will be placed in the 
	logfile with the PID.  If the name is missing then the name of 
	the log program (ng_log) will appear in the message. 

&space
&term	message : This is the text message that will be written to 
	the log.  The message &stress(must) be in quotes if it contains 
	spaces.  If the message text is exactly the string "/dev/fd/0" then
	&this reads messages from the standard input and writes them all 
	to the log. 
&endterms

&space
&exit	When the binary completes, the exit code returned to the caller 
	is set to zero (0) if the number of parameters passed in were 
	accurate, and to one (1) if an insufficent number of parameters
	were passed. 

&space
&examp	The following shell code provides an example of how to call this
	binary to write a critical message to the log.
&litblkb
ng_log $PRI_CRIT $0 "No space on device: $dev"
&litblke
&space
	The logfile defined by the pinkpages variable 
	&lit(STATS_LOG) will be the target of the message generated by 
	the next lines of shell code. 
&space
&litblkb
LOG_COMPRESSION="21:STATS_LOG:COMPRESS"


ng_log $LOG_COMPRESSION $0 "Ratio= $ratio file=$file"
&litblke


&space
&see	ng_log(lib)  ng_logd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Jul 1997 (sd) : Original code 
&term	05 Nov 1997 (sd) : To incorporate changes to ng_log
&term	12 Nov 1997 (sd) : To allow multiple logs to be written to 
&term	20 Apr 2001 (sd) : Conversion from Gecko.
&term	03 Nov 2003 (sd) : to shorten the name of the calling process to just the base -- sun names too long.
&term	26 Jan 2004 (sd) : Corrected man page -- removed Gecko ref (HBD GMM #100)
&term	26 Jun 2008 (sd) : Added ability to process slew of message strings from stdin. 
&term	22 Sep 2008 (sd) : Corrected issue with initialisation of pri var when reading a set of messages from '/dev/fd/0'.
&term	09 Jun 2009 (sd) : Corrected how verbose is read from arguments.
&term	16 Oct 2009 (sd) : Corrected man page. 
&endterms
&scd_end
*/
