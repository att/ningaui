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


#include	<sfio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<signal.h>

#include	"ningaui.h"
#include 	"ng_ext.h"
#include	"repmgr.h"

#include	"rmreq.man"

static void sigh( int x )
{
	if( x == SIGPIPE )		/* we trap on this as it is thrown when writing to a dead socket */
	{
		ng_bleat( 0, "ng_rmreq: unidentified write error: session probably disconnected before writing completed" );
		exit( 21 );
	}

	ng_bleat( 0, "ng_rmreq: abort because of signal: %d", x  );		
	exit( 20 );
}

/*****************************************/
/* Send commands to ng_repmgr via socket */
/*****************************************/
int
main(int argc, char **argv)
{
	char	*version = "v2.0/01177";
	char 	*sys = 0;
	char 	*s;
	int 	ifd, ofd;		/* socket in/out file descriptors */
	int 	n;			/* number of bytes written to socket */
	int 	i, k;
	int	skip = 0;	/* number of bytes of first buffer to skip -- handshake */
	char 	buf[NG_BUFFER]; 
	char	tmp[NG_BUFFER];
	char 	*port = NULL;	/* port to connect to (repmgrbt by default) */
	Sfio_t 	*sout;		/* stream out; the output side of the connection is mapped to this */

	int timeout = 300;	/* -a mode timeout */
	int send_exit = 0;	/* -e sets to cause us to send an exit command and nothing else */

	ng_hrtime t_in;		/* time to read input */
	ng_hrtime t_out;	/* time to write to socket */
	ng_hrtime t_rx;		/* time to open socket */
	ng_hrtime ti, to;

	/* Process command line arguments */
	ARGBEGIN{
	case 'a':	port = ng_env("NG_REPMGRS_PORT"); skip = 1; break;
	case 'e':	send_exit++; break;
	case 'p':	SARG(port); break;
	case 's':	SARG(sys); break;
	case 't':	IARG(timeout); break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
		sfprintf( sfstderr, "%s %s\n", argv0, version );
		sfprintf(sfstderr, "Usage: %s [-a [-t seconds]] [-p port] [-s system] [-v] [req]\n", argv0);
		sfprintf(sfstderr, "       %s [-e]\n", argv0);
		exit(1);
		break;
	}ARGEND

	if(sys == 0){
		if( !(sys = ng_pp_get( "srv_Filer", NULL )) || !*sys ||  strncmp( sys, "#ERROR#", 7 ) == 0  )
		{
			if( !(sys = ng_env( "srv_Filer" )) )
			{
				ng_bleat( 0, "could not resolve srv_Filer in pinkpages" );
				exit( 1 );
			}
		}
	}
	else
	{
		if( strncmp( sys, "#ERROR#", 7 ) == 0 )
		{
			ng_bleat( 0, "host passed in with -s seems wrong: %s", sys );
			exit( 1 );
		}
	}
		

	if( ! port )						/* not supplied on command line */
	{
		if( (port = ng_env( "NG_RMBT_PORT" )) == NULL )		/* try to get new standard */
			if( (port = ng_env( "NG_RM_PORT" )) == NULL ) 	/* back compat to old name */
			{
				ng_bleat( 0, "unable to get port from environment (NG_RMBT_PORT)" );
				exit( 1 );
			}
	}

	ng_bleat( 1, "%s: attempting connection to %s:%s", argv0, sys, port);

	ti = ng_hrtimer();			 /* Get current time */

	signal( SIGPIPE, sigh );		/* writes fail with this signal if socket breaks */

						/* connect  */
	if(ng_rx(sys, port, &ifd, &ofd) != 0){
		ng_bleat( 0, "%s: unable to create socket or connect: %s:%s (%s)", argv0, sys, port, strerror( errno ) );
		exit(1);
	}
	t_rx = ng_hrtimer()-ti;				 /* Time it took to open socket */

	/* prepare message to send */
	if( (sout  = sfnew(0, (Void_t*)1, SF_UNBOUND, ofd, SF_WRITE)) == NULL )
	{
		ng_bleat( 0, "unable to create an sfio stream for socket" );
		exit( 2 );
	}

	ng_sfdc_error( sout , "socket out", 0 );		/* push our discipline onto the stream to check and save error info */

	n = 0;
	t_in = t_out = 0;

        if( send_exit )					/* send exit and bail */
        {
                char ebuf[100];
        
                i = sfsprintf( ebuf, sizeof(ebuf), "exit\n" );
                sfwrite( sout , ebuf, i );

		sfclose( sout  );
		close( ifd );
		exit( 0 );
        }

	if(argc == 0){					/* get buffers from stdin and send to repmgrbt */
		ti = ng_hrtimer();			 /* Get current time */

		while((i = sfread(sfstdin, buf, sizeof buf)) > 0){
			to = ng_hrtimer();
			t_in += to-ti;
			sfwrite(sout , buf, i);		 /* Write buffer on socket */

			ti = ng_hrtimer();
			t_out += ti-to;
			n += i;
		}

		to = ng_hrtimer();
		sfputc(sout , '\n'); 				/* Write newline to socket */
		n++;
	} else {
		to = ng_hrtimer();				 /* Get current time */
		for(i = 0; i < argc; i++){
			sfprintf(sout , "%s ", argv[i]);		 /* Write command line arguments to socket */
			n += strlen(argv[i])+1;
		}
		sfputc(sout , '\n');				 /* Write newline to socket */
		n++;
	}

					/* flush output after writing the end of data character */
	sfputc(sout , EOF_CHAR);		
	sfsync(sout );
	t_out += ng_hrtimer()-to;

	if( sfclose(sout ) )			/* a non-zero return indicates that we saw an  error at somepoint during processing */
	{
		ng_bleat( 0, "ng_rmreq: error reported at close: %s", errno ? strerror( errno ) : "errno was not set" );
		exit( 3 );
	}

	/* Log times it took to process request */
	ng_bleat( 1, "sending request of %d bytes: trx=%.1fms tin=%.1fms tout=%.1fms", n, t_rx*hrtime_scale*1e3, t_in*hrtime_scale*1e3, t_out*hrtime_scale*1e3);
	ng_log(LOG_INFO, "sending request of %d bytes: trx=%.1fms tin=%.1fms tout=%.1fms\n", n, t_rx*hrtime_scale*1e3, t_in*hrtime_scale*1e3, t_out*hrtime_scale*1e3);

										/* read anything that gets sent back and echo to stdout */
	if( (n = ng_read( ifd, buf, sizeof( buf ), timeout )) > 0 )		/* initial read to trash handshake */
	{
		write( 1, buf+skip, n-skip );					/* trash handshake chr if needed */		
		while( (n = ng_read( ifd, buf, sizeof( buf ), timeout )) > 0 )
			write( 1, buf, n );						
	}

	if( n < 0 )				/* ng_read sends -1 on error and -2 on timeout */
	{
		ng_bleat( 0, "ng_rmreq: %s", errno ? strerror( errno ) : "read timeout" );
		exit(  1 );
	}

	close( ifd );			/* input side of the socket */
	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_rmreq:Repmgr request interface)

&synop  ng_rmreq [-a [-t sec]] [-v] [-p port] [-s system] [req ...]
&space
	ng_rmreq -e

&desc	&ital(Ng_rmreq) passes requests for action to &ital(ng_repmgr) via 
	&lit(ng_remgrbt).
	Any output received back from repmgrbt is written to the standard output device. 
	Generally, output is only received when using the alternate connect mode (-a) which 
	sends the data directly to the replication manager. 
	

&opts   &ital(Ng_rmreq) understands the following options:

&begterms
&term	-a : Alternate connection mode.  Causes &this to connect directly to replication manager
	rather than using the normal interface via &ital(repmgrbt).  When using this option, the 
	-t option may also be used.  If the port is not supplied on the command line, then when 
	in this mode the pinkpages value NG_REPMGRS_PORT is used. 
&space
&term	-e : Sents an exit command to repmgr causing it to stop.  It will likely be restarted automaticly
	if the start script (ng_rm_start) is not stopped first. 
&space
&term 	-p port : Connect to  &ital(ng_repmgrbt) using the port number &ital(port). If not supplied, the pinkpages
	variable NG_RMBT_PORT is assumed to have the proper value. 

&space
&term 	-s system : call &ital(ng_repmgr) on the node &ital(system).
	If not supplied, the pinkpages variable srv_Filer is assumed to list the name of the node
	that has replication manager running on it. 
&space
&term	-t sec : Timeout period.  When the -a option is set, &this will wait seconds for data
	before giving up.
	
&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	A non-zero return code indicates an error.  Generally, if the return value is larger than 
	19, the error was caused by a signal that was trapped. 

&see	ng_rm_start, ng_repmgr, ng_repmgrbt, ng_rmbtreq

&mods	
&owner(Andrew Hume)
&lgterms
&term 	8/5/2001 (ah) : Original code.
&term	15 Jun 2003 (sd) : Check for null port number.
&term	02 Nov 2004 (sd) : Changed sfmove() to ng_read() and write().
&term	11 Mar 2004 (sd) : Now ignores the handshake byte if invoked with -a.
&term	30 Jun 2005 (sd) : Added -t option to allow commandline supplied timeout with -a.
&term	18 Jul 2005 (sd) : Timeout now applies to data read.
&term	05 Oct 2006 (sd) : Now defaults to sending to srv_Filer rather than local node. 
&term	17 Jan 2007 (sd) : Slight mods to ensure that it traps all possible errors on the TCP session. 
		Sigpipe errors were causing some slient failures.  (v2.0)
&term	01 Feb 2006 (sd) : Looked at problem with -e not sending exit. 
&term	25 Sep 2006 (sd) : Added error checking with regard to looking for srv_Filer in narbalek. If 
		narbalek (pinkpages) returns "#ERROR#" indicating that narbalek data is not stable, 
		this now digs it from the cartulary.
&term	29 Oct 2007 (sd) : Added yet another check to see about trapping #ERROR# strings.  This time
		to check what is passed in from the command line. 
&term	11 Jul 2008 (sd) : Added check for empty string from pp_get() call. 
&endterms
&scd_end
*/
