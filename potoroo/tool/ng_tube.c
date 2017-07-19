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
--------------------------------------------------------------------------
Mnemonic: tube.c
Abstract: This file contains routines that construct the tube utility.
The utility provides its caller with a pipeline for processes
that has better error notification than provided by the shells.
Date:     3 March 1998
Author:   E. Scott Daniels
--------------------------------------------------------------------------
*/

#include <stdarg.h>     /* required for variable arg processing -- MUST be EARLY to avoid ast issues */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/signal.h>

#include <sfio.h>

#include <ningaui.h>
#include "ng_tube.man"


/* tube does not use standard symbols to avoid quoting everything */
#define TUBE_SYM  '!'         /* tube command separator */
#define REDIR_SYM '@'         /* redirection symbol */
#define ESC_SYM   '^'         /* treat next char as is - no interpretation */

/* token types returned by getok */
#define T_END           0     /* no more tokens */
#define T_TUBE_SYM      1     /* end of tube segment */
#define T_WORD          2     /* word */
#define T_REDIR_OVERLAY 3     /* redirection - overlay file (@) */
#define T_REDIR_APPEND  4     /* redirection - append to file (@@) */

#define MAX_TRACK   125       /* max pids that we will track */


int pidlist[MAX_TRACK];       /* pid tracking array - kept in order of tube */

char *argv0 = 0;
int verbose = 0;
int firstec = 0;              /* return tube num of 1st process to fail */
int addfilter = 0;            /* add delay EOF filter between procs */

static int execute( char **argv, int sifd, int sofd, char *ename, int mode );

/*
-------------------------------------------------------------------------
Mnemonic: quit
Abstract: This routine is called when things have gotten so Donald that 
we must throw things into the fire and run. The routine will
try to put a message to stderr and then will exit. 
Parms:    ec - error number to dig system error message out - ignored if 0
fmt- Pointer to message format string
...- Variable args depending on format string contents.
------------------------------------------------------------------------
*/
void quit( int ec, char *fmt, ... )
{
	va_list argp;                        /* pointer at variable arguments */
	char mbuf[2048];                     /* buffer to build message in */

	strcpy( mbuf, "tube: " );

	va_start( argp, fmt );               /* point to first variable argument */
	vsprintf( mbuf+6, fmt, argp );       /* format the user message */
	va_end( argp );                      /* cleanup of variable arg stuff */


	if( ec )
		sfprintf( sfstderr, "%s : %s\n", mbuf, strerror( ec ) );
	else
		sfprintf( sfstderr, "%s\n", mbuf );
	
	exit( 127 );                                /* send it down the tubes */
}

/*
-----------------------------------------------------------------------
Mnemonic: pipe_trap
Abstract: trap signals generated when we loose the stdout pipe.
	   we will delay a bit, allowing the mother tube process to 
	   see the failing child before we potenitally propigate any
	   other failures by exiting.
	   NOTE:  Used only in a tube_filter process.
-----------------------------------------------------------------------
*/
void pipe_trap( )
{
	ng_bleat( 1, "broken pipe received in pid %d", getpid( )  );
	sleep( 2 );      /* dont sleep less than 2 or we could not sleep at all */
	exit( 0 );       /* filters always exit cleanly */
}

/*
-----------------------------------------------------------------------
Mnemonic: filter
Abstract: filter data between pipes in order to delay exit so that 
tube can detect the first non-zero exit before others. 
-----------------------------------------------------------------------
*/
void filter( )
{
	long i;
	char buf[4096];

	signal( SIGPIPE, pipe_trap );    /* catch broken pipes */

	while( (i=fread( buf, 1, 4096, stdin)) > 0 )
		fwrite( buf, 1, i, stdout );

	sleep( 2 );     /* incase uptube process failed rather than norm exit */

	exit( 0 );      /* filter never returns, never errors */
}

/* 
-------------------------------------------------------------------------
Mnemonic:  gettok
Abstract:  This routine will fish out the next token from the buffer
starting at bptr+bidx. Tokens are separated by white space or 
special character tokens (such as TUBE_SYM and REDIR_SYM).
All characters contained in single or double quotes are treated
as is and white space is preserved. 
Parms:     bptr - Pointer to buffer from which to dig.
bidx - Pointer to index into buffer that we last were digging.
tstring - Pointer to buffer into which a copy of the token 
found is placed.
Returns:   The token type from the T_ constant list
--------------------------------------------------------------------------
*/
int gettok( char *bptr, int *bidx, char *tstring )
{
	static int inc = 1;    /* inc value for b when tube symbol encountred */

	char *b;           /* pointer directly into the buffer */
	char *t;           /* pointer directly into token string buffer */
	char endquote;     /* type of quote mark to look for */
	int type = T_END;  /* end of string reached - no tokens */
	int ccount = 0; 	/* num chrs in current token */

	t = tstring;           
	b = bptr + *bidx;      /* use the index to point into the buffer */

	*t = 0;                   /* assume no string */

	while( *b && isspace( *b ) )
		b++;                           /* skip leading white space */

	while( *b && type == T_END )    /* until we reach end or find token */
	{
		if( isspace( *b ) )           /* terminates a word */
			type = T_WORD;
		else
		{
			switch( *b )                 /* base action on the current char */
			{
				case '"':
				case '\'':                 /* snatch all things in quotes */
					endquote = *b++;
					while( *b && *b != endquote )  
						*t++ = *b++;

					if( *b == endquote )
						b++;                          /* skip closing quote */
					break;

				case REDIR_SYM:            /* redirection symbol (assume sfstderr) */
					if( ccount )		/* if not the first character of the token */
					{
						ng_bleat( 3, "tok: ignored @" );
						*t++ = *b++;	/* then it is treated as part of the token */
						break;
					}

					ng_bleat( 3, "tok: redirect @" );
		
					if( t > tstring && *(t-1) != '2' )
					{
						t--;                               /* "remove" 2 from the word */
						type = T_WORD;                     /* it is a word separator */
					}
					else
						if( *(++b) == REDIR_SYM )          /* >> entered */
						{
							b++;
							type = T_REDIR_APPEND;
						}
						else
							type = T_REDIR_OVERLAY;
					break;

				case TUBE_SYM:
					if( t > tstring )        /* terminates a word */
						type = T_WORD;         /* set proper type */
					else
					{
						type = T_TUBE_SYM;
						if( addfilter     )        /* adding filters between procs? */
						inc = (! inc) & 0x01;  /* cannot guarentee that !0 == 1 */
						b += inc;
					}
					break;

				case ESC_SYM:           /* treat next character literally */
					if( *(b+1) )          /* provided that its not the last one */
						b++;                 /* up b past escape and FALL INTO DEFAULT */ 
				
				default: 
					*t++ = *b++;               /* just snatch things */
					break;
			}
		}

		ccount++;
	}                                /* end while *b */


	if( t > tstring )
		type = T_WORD;                  /* word located for caller */
	
	*t = 0;                          /* mark with end of string */
	*bidx = b - bptr;                /* set index for next call */

	return type;
}


/*
-------------------------------------------------------------------------
Mnemonic: arg2str
Abstract: Convert an argv type list of arguments into a single string.
arguments are separated by one blank.
Parms:    argv - Pointer to pointer of character - list.
Returns:  Pointer to the string 
-------------------------------------------------------------------------
*/
char *arg2str( char **argv )       /* convert argv list to string */
{                                  /* for easier tokenising */
	extern int errno;

	char *str = 0;                    /* pointer to string we will build */
	int strsize = 0;                  /* size of the string that has been alloc*/

	while( *argv )
	{
		if( ! str || strsize <= (strlen( str ) + strlen(*argv) + 2)) 
		{
			strsize += 2048;
			if( ! (str = ng_realloc( str, strsize, "tube: arg2string" ) ) )
				quit( errno, "cannot realloc arg2string str" );
			if( strsize == 2048 )
				*str = 0;		/* needs on first allocation */
		}
	
		strcat( str, *argv );
		strcat( str, " " );
		argv++;
	}

	ng_bleat( 3, "arg2str: %d %d (%s)", strsize, strlen( str ), str );
	return( str );
}

/*
------------------------------------------------------------------------
Mnemonic:  bld_cmd
Abstract:  This function will build tokens from the input string into 
a series of command (argc/argv) lists.  Commands are terminated
with a tube symbol  or end of string, and are executed in 
REVERSE order so as not to limit the number of processes 
started in a tube to some number smaller than the max file 
descriptor per process limit.
Parms:     str - Pointer to string that contains tokens for commands. 
popt- Create a pipe if true
Returns:   File descriptor caller should change stdout to 
Warning:   Routine is recursive.
------------------------------------------------------------------------
*/
int bld_cmd( char *str, int popt )      /* build a command to invoke */
{
	extern int errno; 

	static int level;              /* how deep are we in this doodoo */

	int idx = 0;                   /* index into string */
	int type;                      /* token type */
	int pid;                       /* child id */
	char token[1024];              /* token string found */
	int args = 0;                  /* number of args allocated */
	char **argv = 0;               /* arg list for the command */
	int argc = 0;                  /* arg count for the command */
	int sofd = 1;                  /* fd to use for standard out */
	int sifd = 0;                  /* stdin created by execute call */
	int pfd[2];                    /* file descriptors for pipe if creating */
	int append =  0;               /* set to 1 if sfstderr is appended to file */
	char *ename = 0;               /* pointer to stderr redirect file name */

	ng_bleat( 2, "bul_cmd: started level=%d", level );

	while( (type = gettok( str, &idx, token )) != T_END && type != T_TUBE_SYM ) 
	{
		switch( type )
		{
			case T_WORD:               /* add the word to the arg list */
				if( argc >= args )       /* ensure enough space for next argument */
				{
					args += 100; 
					argv = ng_realloc( argv, args * sizeof( argv[0] ), "tube: argv" );
				}

				ng_bleat( 2, "bld_cmd: add: (%s)", token );
				argv[argc++] = strdup( token );   /* add token to command list */
				break;

			case T_REDIR_APPEND:                           /* @@ entered */
				append=1;
					/* FALL INTO NEXT CASE TO FINISH */
			case T_REDIR_OVERLAY:                           /* @ entered */
				if( gettok( str, &idx, token ) != T_WORD )
					quit( 0, "missing file name on redirection segment %d", level + 1 );

				ng_bleat( 2, "redirect type=%s to %s", type == T_REDIR_OVERLAY ? "overlay" : "append", token );
				ename = strdup( token );         /* save std err file name */
				break;

			default:
				quit( 0, "internal error - unknown type: %d", type );
				break;
		}   /* end switch */
	}

	if( type == T_TUBE_SYM )          /* recurse to process remainder of str */
	{
		if( argv )                      /* prevent counting filters in return */
			level++;
		sofd = bld_cmd( str+idx, 1 );    /* get fd to write current cmd stdout to */
		if( argv )
			level--;
	}

	if( argv )                      /* argv will be NULL if !! */
		argv[argc] = 0;                /* nicely terminate the arg list */
	else
		firstec = 1;                   /* assume report on first to fail */

	pfd[0] = 0;              /* default reading end if we do not open */
	pfd[1] = 1;              /* default writing end if we do not open */

	if( popt )                                  /* create pipe if asked to */
		if( pipe( pfd ) )                          /* pipe backwards 0 is ok */
			quit( errno, "cant make pipe" );          /* die if we fail */

	pid = execute( argv, pfd[0], sofd, ename, append  ); /* send cmd into tube */

	if( verbose )
		sfprintf( sfstderr, "child %3d %5d %s\n", level+1, pid,  argv ? argv[0] : "tube_filter" );

	if( argv && level < MAX_TRACK )
		pidlist[level] = pid;                      /* save pid for tracking */

	if( sofd > 1 )            /* if redirected stdout, the child is now */
		close( sofd );           /* using it and we dont need to have open */

	if( ename )
		free( ename );

	return( pfd[1] );                   /* return fd caller should write to */
}

/*
-----------------------------------------------------------------------
Mnemonic: redirect
Abstract: This function will redirect the standard error to the named
file with the append option.
Parms:    fname - Pointer to file name to open
a ppend- Set to 1 if file is opened in append mode. 
-----------------------------------------------------------------------
*/
void redirect( char *fname, int append )
{
	extern int errno;

	int file;
	int option = O_WRONLY|O_CREAT;     /* file open mode */

	if( fname )                        /* null is ok - just exit */
	{
		if( append )
			option |= O_APPEND; 
		else
			option |= O_TRUNC;

		if(  (file = open( fname, option, 0666 )) >= 0 )
		{
			if( ! dup2( file, 2 ) )			/* move to stderr */
				quit( errno, "could not dup stderr for %s", fname );
			close( file );
		}
		else
			quit( errno, "could not open stderr %s", fname );
	}
}

/*
-----------------------------------------------------------------------
Mnemonic: execute
Abstract: This function will execute the command that is passed in 
in the argc, argv lists.  The assumption that the command is 
contained in argv[0] is made.
Parms:    argc  - Count of args in argv
argv  - Pointer to a list of pointers to ascii arguments
sifd  - FD for standard input after fork
sofd  - FD for standard out after fork
Returns:  The file descriptor number of the writing end of the pipe.
----------------------------------------------------------------------
*/
int execute( char **argv, int sifd, int sofd, char *ename, int mode )
{
	extern int errno; 

	int pid;                                  /* ssn of our new baby */
	int i;

	switch( (pid = fork( )) )                 /* make (test) tube babies */
	{
		case -1:   
			quit( errno, "cant reproduce (fork)" );   /* error and exit */
			break;

		case 0 :                               /* we are the child */
							/* order important incase 0 is closed on entry */
			if( sifd > 0 )                              /* change stdin? */
			{                                      
				if( dup2( sifd, 0 ) != 0 )              	/* change stdin */
					quit( errno, "offspring is unable to get dup to 0" );
				close( sifd );                             /* dont need extra fd */
			}

			if( sofd > 1 )                    /* should we change our stdout? */
			{
				if( dup2( sofd, 1 ) != 1 )                   /* change stdout to new file */
					quit( errno, "offspring is unable to get dup to 1" );
				close( sofd );                             /* drop duplication */
			}


			redirect( ename, mode );      /* redirect stderr if defined */

			for( i = 3; i < 255 ; i++ )    /* leave nothing but stdio stuff open */
				close( i );

			if( argv )                     /* something to exec */
			{
				signal( SIGPIPE, SIG_DFL );			/* woomera turns this to ignore, we dont want that */
				execvp( argv[0], argv );            
			}
			else                           /* nothing to exec, start filter */
				filter( );

			quit( errno, "unable to execute: %s", argv[0] );   
			break;

		default:                               /* the proud parent */
			if( sifd > 0 )
				close( sifd );        /* close child's reading end of pipe */
			break;
	}

	return pid;
}


/*
-----------------------------------------------------------------------
Mnemonic: main
Abstract: Like this is the main driver for the tublar tube program man.
-----------------------------------------------------------------------
*/
int main( int argc, char **argv )
{
	extern int errno;

	char *string;            /* pointer at string of tube commands */
	int stat = 0;            /* status of completing job */
	int end;                 /* process id of ending process */
	int ec = 0;              /* exit code */
	int i;                   /* loop index */
	char *version = "v2.3/09207";


	ARGBEGIN
	{
		case 'f':  addfilter++; break;
		case 'v':  OPT_INC(verbose); break;
		default:
usage:
		ng_bleat( 0, "ng_tube version %s", version );
		quit( 0, "Usage: %s [-fv] command [@[@]stderr_file] [!command [@[@] stderr_file]]...", argv[0] );
		break;
	}
	ARGEND

	ng_bleat( 1, "%s %s started", argv0, version );

/* this is fixed in ARGBEGIN */
/* open( "/dev/null", O_RDONLY, 0644 );	*/		/* odd things happen if we are invoked with a closed stdin */

	
	signal( SIGPIPE, SIG_DFL );			/* woomera turns this to ignore, we dont want that */

	memset( pidlist, sizeof( pidlist[0] ) * MAX_TRACK, 0 );
	
	string = arg2str( argv );    /* convert arg list to string */

	while( *string && (*string == TUBE_SYM || isspace( *string )) )		 /* something like !cmd !cmd  does not make sense, trash lead bangs */
		string++;

	bld_cmd( string, 0 );

	/* status codes when printed below will be xxyy where xx is the exit code, and yy is the signal/coredump indication */
	while( (end = wait( &stat )) > 0 || errno == EINTR )  /* wait for all */
	{
		ng_bleat( 2, "pid %d stat=0x%x WIFEXITED=%d WIFSIGNALED=%d  WEXITSTATUS=%d WCOREDUMP=%d  WTERMSIG=%d", end, stat,  WIFEXITED( stat ), WEXITSTATUS( stat ),  WEXITSTATUS( stat ), WCOREDUMP(stat), WTERMSIG(stat) );
	
		if( ! errno )                          /* skip along if interrupted */
		{
			if( verbose )
				sfprintf( sfstderr, "child finished pid=%d stat= 0x%x(%s) \n", end, stat, ((!WIFEXITED(stat)) || WEXITSTATUS(stat)) ? "Bad" : "Good" );

			if( (! WIFEXITED( stat )) ||  WEXITSTATUS( stat ) )     /* bad finish */
			{
				for( i = 0; i < MAX_TRACK && pidlist[i] != end; i++ );  	/* find pid */

				if( firstec )			/* exit code should correspond to first to fail rather than right most */
				{
					if( ! ec )           /* save tube number of first to fail */
						ec = i + 1;
#ifdef KEEP
					return ec; /* we seem to lock when pzip|filter|parser and parser fails - no broken pipe, this helps */
#endif
				}
				else
					if( ! ec ||  i + 1 > ec ) 
						ec = i + 1;                 /* exit code to show right most failing process */
				stat = 0;
			}
		}
	}

	return ec;          
}

#ifdef KEEP
/*
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_tube:Start commands directing stdout of each to next process in the tube)
&name &utitle

&space
&synop	ng_tube &ital(cmd_string) [-fv] [@[@]filename] [! &ital(cmd_string) [@[@filename] ...

&space
&desc	&ital(Nk_tube) provides an alternate method of starting several 
	commands and redirecting the standard output of each command to 
	the standard input of the next command in the tube.  The final
	command's standard output is redirected to the standard output 
	of the script that invokes &ital(ng_tube) or as redirected on 
	the command line. &ital(Nk_tube) is very similar to the shell
	command pipe construct, except the return code is set to the 
	"id" of the left most command in the tube that did not finish 
	with a "good" exit code. 
&space
	&This also provide the means to indicate which of the processes
	failed first (using the -f option) which might be more useful
	in determining the cause of an error than knowning the left-most
	process to fail. 

&subcat	Special Symbols
	To prevent the need to use quotation marks, or to escape 
	characters on a &ital(ng_tube) command line, the at sign (@) is 
	used to indicate redirection (of a process' standard error)
	and the bang  (!) is used to separate commands in the 
	tube. The rediection symbol functions in the same manner as 
	the shell redirection symbol; a single at sign will cause the 
	named output file to be overlayed, while two at signs (@@) will
	cause the standard error messages to be appended to the named
	file.

&subcat Standard Input
	The standard input of the first command in the tube may be 
	redirected such that it is read from a file, rather than the 
	default device. The redirection for standard input is done 
	using the normal input redirection character (<).

&subcat Standard Output
	If the standard output of the &ital(ng_tube) command is 
	redirected, then the output of the last command in the tube is 
	written to the named file.  Standard output is redirected 
	using the normal shell redirection symbols. 

&subcat Standard Error
	If standard error ( 2> or 2>>) is redirected for the &ital(ng_tube) 
	command, then the standard error for &stress(all) commands in 
	the tube is written to the named file. The only exception to this 
	is if the at sign redirection is used within a tube segment.

&subcat	Quoted and Escaped Characters
	Arguments that are passed to &ital(ng_tube) which are inclosed 
	within either single (') or double (") quotation marks cause
	all characters upto the matching quote to be treated literally.
	Additionally white space which is normally used to seperate
	tokens is ignored and placed into the token. Care must be 
	taken when quoting command input as it will probably be necessary
	to quote the quoted arguments so that the quote marks,  and 
	any necessary white space, are not "eaten" by the shell 
	as it prepares to invoke &ital(ng_tube).
&space
	The carrot (^^) can be used to escape special characters
	that appear in the &ital(ng_tube) command line. When a carrot
	character is encountered, the next character is added to the 
	token that is being constructed. 
&space
&opts	The following options are recognised on the &ital(ng_tube) 
	command line.
&begterms
&term	-f : Indicate first to fail. When this option is set, &ital(ng_tube)
	will exit with the tube segment number of the process that failed
	first, rather than the left-most process that failed. See "Notes" 
	at the end of this page.
&space
&term 	-v : Verbose mode. When in verbose mode &ital(ng_tube) writes 
	information regarding the processes that were created and their 
	exit values to the standard error device. 
&endterms

&space
&parms	The parameters required by &ital(ng_tube) are one or more 
	commands and parameters separated by the tube symbol (!). 
	The standard output of the left most command in the tube is 
	piped into the command to its right, and so on down the line. 

&space
&exit	An exit code of zero (0)  returned by &ital(ng_tube) indicates
	that all commands in the tube completed successfully.  An exit
	code of 127 indicates that the tube could not be properly 
	initialised and &ital(ng_tube) had to abort. 
&space
	Any other non-zero return code indicates which of the tube 
	segments failed (tube segments are numbered from left to right). 
	Normally this is the tube segment number of the right most 
	segment that failed. When the &lit(-f) option is supplied on the 
	&ital(ng_tube) command line, then the exit code indicates the 
tube segment number of the 
	&stress(first) process that did not succeed. 
	Knowing which segment failed first can assist the calling 
	script in ignoring any errors that were generated as a result
	of a failed tube segment.

&space
&examp
	The following illustrate how the &ital(ng_tube) command can 
	be used. The first example illustrates how a script can 
	determine which command in the tube failed.  It uses the 
	&lit(-f) option to determine which segment failed first. 
&space
&litblkb
ng_tube -f uncompress test.file ! sort -u ! add_entries
case $? in
1) echo uncompress failed ;;
2) echo sort failed ;;
3) echo add_entries failed ;;
*) echo unknown tube failure ;;
esac

&litblke
&space
	The next example shows how the standard error of each 	
	command in the tube can be redirected and how the standard
	output of the tube can also be redirected. 
&litblkb
ng_tube sosal program.sos @sosal.err.$$ ! build_script >/tmp/cmds.ksh
&litblke
&space
	The last example illustrates how the standard error of all commands
	is redirected to a single file, but the standard error of the second 
	command is redirected to its own file.
&litblkb
ng_tube a ! b @stderr.b ! c ! d 2>stderr.tube
&litblke


&space
&notes	
&subcat	Efficency with -f option
	In order to implement the &lit(-f) option, &ital(ng_tube) must 
	insert an additional process between processes listed in 
	the &ital(ng_tube) command line. These filter processes cause 
	a delay of one (1) second when an error is detected on either 
	side of it.  The delay allows &ital(ng_tube) to trap the first 
	process to fail and report it back to the user. It is important 
	to keep in mind that the tube does not execute as effeciently 
	when using this option.  
&space
&subcat	Shell pipefail mode
	It may seem that with the 'set -o pipefail' option provided by current
	shells, &this would not be necessary.
	&This was originally implemented before the shell(s) bundled with some 
	flavours of UNIX had incorporated the pipefail option, but
	even as recently as 2006, the implementation of the pipefail option in some shells has had 
	issues, and none provide the ability to determine which segment of the 
	pipeline failed first. So, &this continues to be supported. 


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Mar 1998 (sd) : Original code.
&term   26 Mar 1998 (sd) : To allow for filter insertion between processes 
	in order to enable first fail reporting. 
&term	21 Sep 2001 (sd) : Corrected initialisation problem in gettok.
&term	27 Mar 2003 (sd) : Converted to use ng_realloc and remove malloc.h dependency
&term	23 May 2003 (sd) : To identify status code as being hex.
&term	16 Jul 2003 (sd) : Fixed pesky little bug that showed under linux -- stdin was coming in 
	closed and that was throwing us off.
&term	17 Nov 2003 (sd) : Fixed realloc bug that caused commands greater than 2048 characters to fail.
&term	13 Sep 2004 (sd) : To move stdarg.h incude to top to avoid ast stdio issues.
&term	20 Sep 2007 (sd) : Now ignores any lead bang characters ("!cmd!cmd") as they dont make sense and
		cause unexpected behaviour. (v2.3)
&endterms
&scd_end
*/
#endif
