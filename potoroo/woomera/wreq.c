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

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<signal.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"woomera.h"

#include	"wreq.man"

/* handle alarm signal - if alarm pops then we assume woomera has not responded 
   in a timely fashon and we exit with a message to stderr and log
*/
static char *alarm_op = "in unknown state; rhost=";		/* current state if we write an alarm abort message */
char	*sys = 0;						/* system name -- global so sigh can use it in message */
int	quiet = 0;						/* if set (-q) then we do not write to master log */

static void sigh( int x )
{
	sfprintf( sfstderr, "timeout waiting for response from woomera while: %s %s\n", alarm_op, sys  ? sys : "unknwon" );
	if( ! quiet )
		ng_log(LOG_ERR, "timeout waiting for response from woomera while: %s %s", alarm_op, sys ? sys : "unknwon" );
	exit( 1 );
}

/* we set alarm to timeout value prior to every read/write on the socket. This will 
   cause an abort if any read/write takes longer than timeout to complete yet will 
   not timeout if the dialogue requires more than timeout seconds. ng_rx will timeout  
   if it cannot get the session so we need not worry about preceding the connection 
   request with an alarm call.
*/
int
main(int argc, char **argv)
{
	int timeout = 300;		/* timeout (seconds) - we bail if we wait this long between responses */
	int ifd, ofd;
	char *fname;
	int eflag = 0;
	int nflag = 0;
	char *s;
	int n;
	int ret;
	char buf[NG_BUFFER];
	char *port = ng_env("NG_WOOMERA_PORT");
	FILE *ofp;
	FILE *ifp;

	if( (s = ng_env( "NG_WREQ_QUIET" )) != NULL )	/* before arg processing to allow -q to override it */
		quiet = atoi( s );

	ARGBEGIN{
	case 'e':	eflag = 1; break;
	case 'n':	nflag = 1; break;
	case 'p':	SARG(port); break;
	case 'q':	quiet = 1; break;
	case 's':	SARG(sys); break;
	case 't':	IARG( timeout ); break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
		fprintf(stderr, "Usage: %s [-v] [-p port] [-s system] [-e] [req]\n", argv0);
		exit(1);
		break;
	}ARGEND


	if( timeout < 0 )		/* prevent stupidity - 0 turns off timeout */
	{
		sfprintf( sfstderr, "invalid timeout value (%d sec) using 300 sec\n", timeout );
		timeout = 300;
	}

	if(sys == 0){
		if(ng_sysname(buf, sizeof buf))
			exit(1);
		sys = ng_strdup(buf);
	}

	if( strchr( sys, ' ' ) || strchr( sys, ':' ) )		/* invalid host name */
	{
		ng_bleat( 0, "dodgy host name: %s [FAIL]", sys );
		exit( 1 );
	}

	signal( SIGALRM, sigh );		/* call our handler when alarm pops */

	if( ! port )
	{
		ng_bleat( 0, "unable to determine woomera port (NG_WOOMERA_PORT) from environment, and not supplied with -p [FAIL]" );
		exit( 1 );
	}

	if(verbose)
		fprintf(stderr, "%s: attempting socket to %s:%s\n", argv0, sys, port);

	alarm_op = "connecting to";
	alarm( timeout );
	if(ng_rx(sys, port, &ifd, &ofd) == 0){	/* socket succeeded */
		alarm( 0 );
		ng_bleat( 1, "connection was successful" );
		ofp = fdopen(ofd, "w");
		fname = "socket";		/* don't start this with a slash */
	} else {				/* no server, write temp file */
		ng_bleat( 1, "connection failed" );
		alarm( 0 );
		ifd = -1;
		/*fname = nflag? tempnam(ng_rootnm(WOOMERA_NEST), REQ) : "/dev/null"; */

		if( nflag )
		{
			char fbuf[1024];
			sfsprintf( fbuf, sizeof( fbuf ), "%s/%s", WOOMERA_NEST, REQ );
			fname = ng_tmp_nm( fbuf );
		}
		else
			fname = ng_strdup( "/dev/null" );

		ofp = fopen(fname, "w");
		if(verbose)
			fprintf(stderr, "%s: socket failed: using file %s\n", argv0, fname);
		ng_free( fname );
	}
	if( timeout < 10 )
	{
		timeout = 120;			/* allow short connections, but tollerate read delays */
		ng_bleat( 1, "revised small timeout to: %d", timeout );
	}

	if(ofp == NULL){
		perror(fname);
		exit(1);
	}

	if(eflag){
		if(fname[0] == '/'){	/* don't leave exit if woomera is down */
			if(verbose)
				fprintf(stderr, "%s: unlinking %s because of -e (exit)\n", argv0, fname);
			unlink(fname);
		} else
		{
			alarm_op = "writing exit to";
			alarm( timeout  );
			fprintf(ofp, "exit\n");
		}
	} else {
		if(argc == 0){
			while(n = fread(buf, 1, sizeof buf, stdin))
			{
				alarm_op = "writing buffer to";
				alarm( timeout );			/* reset before each write */
				fwrite(buf, 1, n, ofp);
			}
			fprintf(ofp, "\n");
		} else {
			for(n = 0; n < argc; n++)
			{
				alarm_op = "writing cmdline buffer to";
				alarm( timeout );
				fprintf(ofp, "%s ", argv[n]);
			}

			alarm_op = "writing final newline to";
			alarm( timeout );
			fprintf(ofp, "\n");
		}
	}

	alarm_op = "writing error char to";
	alarm( timeout );
	fprintf(ofp, "%c\n", ERRCHAR);
	fflush(ofp);
	if(ferror(ofp)){
		fprintf(stderr, "%s: write failed: %s\n", argv0, ng_perrorstr());
		if( ! quiet )
			ng_log(LOG_ERR, "write failed: %s\n", ng_perrorstr());
		exit(1);
	}
	fclose(ofp);

	if((ifp = fdopen(ifd, "r")) == NULL){
		perror("read woomera output");
		exit(1);
	}

	alarm_op = "reading from";
	alarm( timeout );
	ret = 0;
	while(fgets(buf, sizeof buf, ifp)){
		s = buf;
		if(*s == ERRCHAR){
			s++;
			ret = 1;
			if(verbose){
				fprintf(stderr, "saw error >>%s", s);
			}
		}
		fputs(s, stdout);

		alarm_op = "reading from";
		alarm( timeout );
	}

	if(ferror(ifp)){
		fprintf(stderr, "%s: read from woomera failed: %s\n", argv0, ng_perrorstr());
		if( ! quiet )
			ng_log(LOG_ERR, "read failed: %s\n", ng_perrorstr());
		exit(1);
	}

	exit(ret);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_wreq:Give commands to ng_woomera)

&synop  ng_wreq [-v] [-p port] [-s system] [-e] [-q] [-t secs] [req ...]

&desc	&This passes requests for action to &ital(ng_woomera)
	and writes any output to its standard output. Requests are either
	supplied on the command line or on standard input.
	

&opts   &This unserstands the following options:

&begterms
&term 	-e : cause &ital(ng_woomera) to exit if it is running.

&space
&term 	-p port : call &ital(ng_woomera) on port number &ital(port).

&space
&term	-q : Quiet mode.  Faiures are NOT logged to the master log.
	If the environment contains the variable &lit(NG_WREQ_QUIET) the 
	quiet mode will be set if this variable contains a value of 1. The 
	-q option overrides the variable if it is set to 0. 
	
&space
&term 	-s sys : call &ital(ng_woomera) on system &ital(sys) (the default is the current system).

&space
&term	-t secs : Set the timeout value.  &this will abort and exit with a bad return code
	if an attempt to communicate with woomera (read or write) sits idle for more than 
	&ital(sec) seconds. A value of zero (0) may be supplied if &this should not timeout.
&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	The program will return with a code of zero, unless errors are detected
	(in which case it exits with a code of one).

&see	ng_woomera

&mods	
&lgterms
&term	09 Oct 1997 (ah) : Original code.
&term	05 Sep 2001 (sd) : Added 5 minute timeout
&term	24 Jul 2003 (sd) : Added timeout to connect attempt too so seneschal will not block.
&term	17 Oct 2003 (sd) : Had to turn off alarm after return from connection. Something was 
	causing it to pop before we got to our first write and could reset it when the 
	value was set low (from seneschal).
&term	19 Jul 2005 (sd) : Better error message on an alarm pop - now indicates the host that i
	it was trying to communicate with.
&term	23 Sep 2005 (sd) : Added -q option.
&term	23 Dec 2005 (sd) : Now looks for NG_WREQ_QUIET in the environment. If found sets quiet to 
	the value. This allows things like zookeeper to force all subsequent calls to wreq to use 
	the quiet flag without having to depend on the scripts that it invokes taking an option 
	to specifically do this. 
&term	16 Jun 2008 (sd) : Added check to ensure that system name does not have spaces or :. 
&endterms

&scd_end
*/
