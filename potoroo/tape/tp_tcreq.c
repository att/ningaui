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
#include	"changer.h"

#include	"tp_tcreq.man"

/* handle alarm signal - if alarm pops then we assume tape_mount has not responded 
   in a timely fashon and we exit with a message to stderr and log
*/
static char *alarm_op = "in unknown state";		/* current state if we write an alarm abort message */

static void sigh(int x)
{
	sfprintf(sfstderr, "timeout waiting for response from tape_mount while: %s\n", alarm_op);
	ng_log(LOG_ERR, "timeout waiting for response from tape_mount while: %s", alarm_op);
	exit(1);
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
	char *sys = 0;
	char *fname;
	char *s;
	int n;
	int ret;
	char *domain;
	char buf[NG_BUFFER];
	char *port = ng_env(TM_PORT);
	FILE *ofp;
	FILE *ifp;

	domain = 0;
	ARGBEGIN{
	case 'd':	SARG(domain); break;
	case 'p':	SARG(port); break;
	case 's':	SARG(sys); break;
	case 't':	IARG(timeout); break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
		fprintf(stderr, "Usage: %s [-v] [-p port] [-s system] [-d domain] [req]\n", argv0);
		exit(1);
		break;
	}ARGEND

	if(domain == 0){
		ng_bleat(2, "domain arg null\n");
		domain = ng_env(TM_LOCAL_DOMAIN);
		if(domain == 0){
			domain = ng_env(FANNOUNCE);
			ng_bleat(2, "env(%s) null, trying %s\n", TM_LOCAL_DOMAIN, FANNOUNCE);
		} else
			ng_bleat(2, "found env(%s) -> %s\n", TM_LOCAL_DOMAIN, domain);
	}
	if((sys == 0) && domain){
		sfsprintf(buf, sizeof buf, "FLOCK_tapedomain_%s", domain);
		sys = ng_env(buf);
		ng_bleat(2, "trying env(%s) -> '%s'\n", buf, sys? sys:"null");
	}

	if(timeout < 0){		/* prevent stupidity - 0 turns off timeout */
		sfprintf(sfstderr, "invalid timeout value (%d sec) using 300 sec\n", timeout);
		timeout = 300;
	}

	if(sys == 0){
		fprintf(stderr, "%s: no system found or specifed\n", argv0);
		exit(1);
	}

	signal(SIGALRM, sigh);		/* call our handler when alarm pops */

	if(verbose)
		fprintf(stderr, "%s: attempting socket to %s:%s\n", argv0, sys, port);

	alarm_op = "connecting";
	alarm(timeout);
	alarm(10);				/* keep repmgr from hanging when a node goes away */
	if(ng_rx(sys, port, &ifd, &ofd) == 0){	/* socket succeeded */
		alarm(0);
		ng_bleat(1, "connection was successful");
		ofp = fdopen(ofd, "w");
		fname = "socket";		/* don't start this with a slash */
	} else {				/* no server, write temp file */
		ng_bleat(1, "connection failed");
		alarm(0);
		ofp = fopen("/dev/null", "w");
		if(verbose)
			fprintf(stderr, "%s: socket failed: using file %s\n", argv0, fname);
	}
	if(timeout < 10){
		timeout = 120;			/* allow short connections, but tollerate read delays */
		ng_bleat(1, "revised small timeout to: %d", timeout);
	}

	if(ofp == NULL){
		perror(fname);
		exit(1);
	}

	if(argc == 0){
		while(n = fread(buf, 1, sizeof buf, stdin)){
			alarm_op = "writing buffer";
			alarm(timeout);			/* reset before each write */
			fwrite(buf, 1, n, ofp);
		}
		fprintf(ofp, "\n");
	} else {
		for(n = 0; n < argc; n++){
			alarm_op = "writing cmdline buffer";
			alarm(timeout);
			fprintf(ofp, "%s ", argv[n]);
		}

		alarm_op = "writing final newline";
		alarm(timeout);
		fprintf(ofp, "\n");
	}

	fflush(ofp);
	if(ferror(ofp)){
		fprintf(stderr, "%s: write failed: %s\n", argv0, ng_perrorstr());
		ng_log(LOG_ERR, "write failed: %s\n", ng_perrorstr());
		exit(1);
	}
	fclose(ofp);

	/*
		now copy to stdout
	*/
	alarm(0);
	ng_bleat(1, "copy output from socket");
	ofp = fdopen(ifd, "r");
	fname = "socket";		/* don't start this with a slash */
	alarm(timeout);			/* reset before each read */
	while(n = fread(buf, 1, sizeof buf, ofp)){
		alarm_op = "writing buffer";
		alarm(0);
		fwrite(buf, 1, n, stdout);
		alarm(timeout);			/* reset before each read */
	}
	if(ferror(ofp)){
		fprintf(stderr, "%s: read from tp_changer failed: %s\n", argv0, ng_perrorstr());
		exit(1);
	}

	exit(ret);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_tmreq:Give commands to ng_tape_mount)

&synop  ng_tmreq [-v] [-p port] [-s system] [-t secs] [req ...]

&desc	&This passes requests for action to &ital(ng_tape_mount)
	and writes any output to its standard output. Requests are either
	supplied on the command line or on standard input.
	

&opts   &This unserstands the following options:

&begterms
&term 	-p port : call &ital(ng_tape_mount) on port number &ital(port).

&space
&term 	-s sys : call &ital(ng_tape_mount) on system &ital(sys) (the default is the current system).

&space
&term	-t secs : Set the timeout value.  &this will abort and exit with a bad return code
	if an attempt to communicate with tape_mount (read or write) sits idle for more than 
	&ital(sec) seconds. A value of zero (0) may be supplied if &this should not timeout.
&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	The program will return with a code of zero, unless errors are detected
	(in which case it exits with a code of one).

&see	ng_tape_mount

&mods	
&lgterms
&term	29 Dec 2003 (ah) : Original code (copied from wreq)
&endterms

&scd_end
*/
