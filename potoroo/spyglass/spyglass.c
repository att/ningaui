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
---------------------------------------------------------------------------------
Mnemonic:	spyglass -- look for problems!
Abstract:	This daemon runs on some/all nodes in a cluster. It causes tests
		to be conducted and sends alarms when a test fails.
		
Date:		24 June 2006
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<string.h>
#include	<errno.h>
#include	<ctype.h>

#include	<sfio.h>
#include	<ningaui.h>
#include 	<ng_lib.h>
#include	<ng_socket.h>

#include	"spyglass.man"

					/* various minute values in tenths of seconds */
#define MINT_5  3000
#define MINT_15	9000			
#define MINT_30 18000
#define MINT_45 27000
#define MINT_60 36000

#define	ST_OK 		0		/* last test returned that all is ok */
#define ST_ALARM	1		/* the last test resulted in a reportable condition -- we have alarmed */
#define ST_RETRY	2		/* test has asked for retry, or retry is coded in config file */
#define ST_BLOCK	3		/* test has sent this status back to cause dependant tests to block, but will not generate an alarm */
#define NST		4		/* no messages beyond this */

					/* tests can block if the system state is not right */
#define SST_STABLE	0		/* everything should be in good running order */
#define SST_BUSY	1		/* the node is busier than normal */
#define SST_NONET	2		/* we have degraded network performance, or none at all */
#define SST_CRISIS	3		/* things on the node are in dire straigts, probabyl nothing is to be tested */
#define SST_STARTUP	4		/* things run when we start */
#define NSST		5		/* number of system states */

#define SP_TEST	1			/* symbol table space divisions -- test blocks */
#define SP_NICK 2			/* nickname entries */
#define SP_VAR	3			/* variables */

#define	TF_PENDING	0x01		/* test is pending */
#define	TF_LOOPCK	0x02		/* set/reset to test for loops in the tree */
#define TF_SQUELCHED	0x04		/* alarms are squelched until lease pops, change or reset based on other flags */
#define TF_SQ_TILRESET	0x08		/* alarms are to be squelched until a normal state is received */
#define TF_SQ_TILCHANGE	0x10		/* alarms are to be squelched until the key changes */
#define TF_SQ_ALWAYS	0x20		/* alarms are always squelched */
#define TF_LOG_OK	0x40		/* alarm -> ok messages allowed to go to digest; turned off by logok = no */
#define TF_VERBOSE	0x80		/* log a bit more about this probe than usual */
#define TF_DEFAULTS	TF_SQ_TILCHANGE + TF_LOG_OK	/* default flags forced on when setting up a new test */


typedef struct Test_t {
	struct	Test_t	*next;		/* next in list of unblocked tests */
	struct	Test_t	*prev;		/* previous in list of unblocked tests */
	struct 	Test_t	*blocks;	/* points to a list of tests that can only be executed if this test is successful (after) */
	long	freq;			/* tenths of seconds between tests */
	long	squelch;		/* tenths of seconds that we are slient after the state goes into failure */
	long	nprobes;		/* number of probes weve made (for statistics) */
	long	nalert;			/* number of probes that resulted in an alert (even if we did not send mail) -- for stats */
	long	nquest;			/* number of probes that resulted in a questionable state (stats) */
	ng_timetype nxt_test;		/* time of next test */
	ng_timetype nxt_alarm;		/* time after which it is safe to send another alarm  (0 == ok to send now) */
	int	retry;			/* number of consecutive bad test results before we alarm */
	long	retry_freq;		/* seconds to pause between retries; if 0, then freq/2 */
	int	attempts;		/* attempts to get a good status */
	int	state;			/* state of the test ST_ constants */	
	int	flags;			/* TF_ constants */
	int	when[NSST];		/* when we can run based on system state */
	char	*after;			/* name of test that blocks this test -- we keep it so that tests order in config is not important */
	char 	*cmd;			/* command that is run to actually test things */
	char	*desc;			/* test description, mostly for the subject in email message */
	char	*id;			/* string given to the command so that status can be reported via TCP message */
	char	*name;
	char	*notify;		/* list of nicks or addresses to send alerts to for this test */
	char	*scope;			/* scope of the test node, cluster */
	char	*sub_data;		/* data to add to email subject when sending an alarm */
	char	*key;			/* alarm key -- if test puts key:string  into the output, then we can detect a content change */
} Test_t;

typedef struct Digest_t {
	struct Digest_t *next;		/* we chain them */
	char	*name;			/* digest name -- for creating the output file */
	char	*user;			/* user address or nickname */
	int 	count;			/* number of messages in the current digest */
	int	freq;			/* how often is it sealed and delivered (tsec) */
	ng_timetype nxt_send;		/* actual time of next send */
	char	*fname;			/* file where we are keeping tabs */
	char	*toc_fname;		/* table of contents */
} Digest_t;

char *state_text[] = {			/* match ST_ constants */
	"NORMAL",
	"ALARM",
	"RETRY/QUESTIONABLE",
	"BLOCKING",
	(char *) 0
};

char *sys_state_text[] = {		/* match SST_ constants */
	"STABLE",
	"BUSY",
	"NO-NET",
	"CRISIS",
	"STARTUP",
	(char *) 0
};

/* ------------- prototypes ------------------- */
Digest_t *new_digest( char *name, char *user, int freq, char *start );			
void add2digests( char *desc, char *tag, char *fname, char *hfname );

/* -------------  globals --------------------- */
Symtab *symbols = NULL;			/* symbol table (SP_ constants used to define pools) */
Test_t	*tlist = NULL;			/* master pointer to all tests */
Test_t	*startup_tests = NULL;		/* list of tests to drive only once at startup */
char	*this_node = NULL;		/* our node name */
int	ok2run = 1;			/* pull to stop */
char	*version = "v1.8/03119";	
char	*domain = NULL;			/* default domain for mail messages */
int 	ok2mail = 1;			/* if -n on cmd line, then we do not send mail */
int	limit_dump = 0;			/* limit what we explain */
int	cur_sst = SST_STABLE;		/* current system state -- set by an outside process */
char	*port = NULL;			/* tcp listen port */
ng_timetype pause_until = 0;		/* we do nothing until it is later than this */
Digest_t *digests = NULL;		/* list of them */

/* ------------------ symtab management ------------------- */

void st_insert( int space, char *name, void *data )
{
	ng_bleat( 2, "insert: %d %s", space, name );
	symlook( symbols, name, space, data,  SYM_COPYNM );
}

void *st_find( int space, char *name )
{
	Syment	*se;		/* at symtab pointer */

	if( ! name )
		return NULL;

	if( (se = symlook( symbols, name, space, 0, SYM_NOFLAGS )) != NULL )
		return se->value;

	return NULL;
}

/* blindly free a value in the symbol table -- assume value is pointer to string, 
   or that the datastructure has been cleaned, or does not need to be 
*/
void st_free( int space, char *name )
{
	Syment	*se;

	if( ! name )
		return;

	if( (se = symlook( symbols, name, space, 0, SYM_NOFLAGS )) != NULL )
	{
		ng_bleat( 2, "removed: %d %s", space, name );
		ng_free( se->value );
		se->value = NULL;
	}
} 


void st_del( int space, char *name )
{
	symdel( symbols, name, space );
}

/* ------------------ utility ----------------------- */
void unknown_token( char *fname, char *token, char *expected )
{
	char	*p;
	char	*tok;

	p = ng_strdup( token );
	tok = strtok( p, " \t" );
	
	if( expected )
		ng_bleat( 0, "%s: unrecognised token: expecting %s: found: %s", fname, expected, tok );
	else
		ng_bleat( 0, "%s: unrecognised token: %s", fname, tok );

	ng_free( p );
}

/* chop trailing whitespace and comment */
void chop( char *buf )
{
	char 	*cp;
	char	*last_ws = NULL;	/* last whitespace */
	int	in_quote = 0;

	if( ! buf )
		return;

	for( cp = buf; *cp; cp++ )
	{
		if( *cp == '"' )
		{
			in_quote = ~in_quote;
			last_ws = 0;
		}
		else
		{
			if( !in_quote && *cp == '#' )   /*&& last_ws )*/
			{
				if( last_ws )
					*last_ws = 0;		/* chop at start of last batch of whitespace */
				else
					*cp = 0;		/* no last whitespace, then chop here */
				return;
			}
			if( isspace( *cp ) )
			{
				if( !last_ws )
					last_ws = cp;			/* mark start of whitespace */
			}
			else
				last_ws = 0;
		}
	}

	if( last_ws )
		*last_ws = 0;
}

/* create a string for explain that indicates when the squelch status -- when the next alarm might be generated */
char *nxt_alarm_string( Test_t *tp )
{
	char	buf1[1024];
	char	buf2[1024];

	*buf2 = 0;
	if( tp->flags & TF_SQ_ALWAYS )
		sfsprintf( buf2, sizeof( buf2 ),  "squelched indefinately" );
	else
	{
		if( tp->flags & TF_SQUELCHED )
		{
			if( tp->flags & TF_SQ_TILRESET )
				sfsprintf( buf2, sizeof( buf2 ),  "squelched until reset" );
			else	
			{
				if( tp->flags & TF_SQ_TILCHANGE )
					sfsprintf( buf2, sizeof( buf2 ),  "squelched until change" );
				else
				{
					if( tp->nxt_alarm > ng_now()  )
					{
						ng_stamptime( tp->nxt_alarm, 1, buf1 );
						sfsprintf( buf2, sizeof( buf2 ),  "squelched until %s", buf1 );
					}
				}
			}
		}
		else
			sfsprintf( buf2, sizeof( buf2 ),  "on" );
	}

	return ng_strdup( buf2 );
}

/* search the buffer for all &vname occurances and expand them to something.
   \& are ignored
*/
char *expand_var( char *buf )
{
	static	int depth = 0;
	char	*p;			/* pointer at start of vname */
	char	*pe;			/* pointer at end of var name */
	char	*evp;			/* pointer into expanded variable string */
	char	*ep;			/* pointer into expansion buffer */
	char	*fp;			/* pointer to chunk to free */
	char	sep;			/* hold so we can mark their buffer with a temp end of string */
	char	*evalue;		/* pointer to environment or symtab value */
	char	ebuf[NG_BUFFER];	/* expansion buffer */
	int	elen =0;		/* length of string in ebuf */
	
	if( ++depth > 10 )
	{
		ng_bleat( 0, "recursion too deep expanding varaible: %s", buf );
		depth--;
		return NULL;
	}
	
	ep = ebuf;
	for( p = buf; *p; p++)
	{
		if( *p == '&' )
		{
			p += *(p+1) == '{' ? 2 : 1;			/* at the start of the name */
			for( pe = p+1; isalnum( *pe ) || *pe == '_'; pe++ );	/* extract name */
			
			sep = *pe;
			*pe = 0;
		
			if( (evalue = st_find( SP_VAR, p ))  == NULL )			/* look up locally first, and if not here, then from env */
				if( (evalue = (char *) ng_env_c( p )) == NULL )		/* returns a pointer to const buffer -- dont free */
					evalue = "";
			*pe = sep;						/* dont trash caller's buffer */
			evp = expand_var( evalue );				/* if variable contains &vname -- expand it */

			if( evp )
			{
				if( (elen += strlen( evp )) < sizeof( ebuf ) )	/* safe to add */
				{
					for( fp = evp; *evp; evp++ )
						*ep++ = *evp;
					ng_free( fp );
				}
				else
				{	
					*ep = 0;
					ng_bleat( 0, "buffer overrun detected: expand_var: buf=(%s)  ebuf=(%s) evp=(%s)", buf, ebuf, evp );
					exit( 20 );
				}
			}

			p = pe; 			/* loop will inc to next char */
			if( *p != '}' )
				p--;
		}
		else
		{
			if( *p == '\\' )			/* convert /& to just & */
				p++;
			if( ++elen < sizeof( ebuf ) )
			{
				*ep++ = *p;
			}
			else
			{
				*ep = 0;
				ng_bleat( 0, "buffer overrun detected: expand_var: buf=(%s)  ebuf=(%s) evp=(%s)", buf, ebuf, evp );
				exit( 20 );
			}
		}
	}

	*ep = 0;
	depth--;

	ng_bleat( 2, "expand_vname: %s -> %s", buf, ebuf );
	return ng_strdup( ebuf );
}



/* set the value of a named symbol in the SP_VAR pool */
void assign_var( char *name, char *value )
{
	st_free( SP_VAR, name );			/* free the value referenced in the symtab (if there) */
	st_insert( SP_VAR, name, value );		/* lump it in */
}

/* expand mailing list
	assume foo@bar.com stays the same
	if no @, then lookup name in symtab (SP_NICKNAME)
	if name is not a nickname, then add the default domain
*/
char *expand_mlist( char *list )
{
	static 	int depth = 0;		/* prevent deep recursion */
	char	**tokens;		/* do not make static as we are recursive */
	int	ntoks;
	char	wbuf[2048];		/* working buffer */
	char	lbuf[NG_BUFFER];	/* where we build the list */
	char	*p;
	int	i;
	int	llen = 0;		/* working lenght of string in lbuf */
	int	freep;			/* p points to allocated buffer that must be freed if this is set */
	
	if( ++depth > 20 )
	{
		ng_bleat( 0, "expand_mlist: recursion too deep -- circular nick names? %s", list );
		return ng_strdup( list );
	}

	if( ! list )
		return NULL;			/* legit for them to pass null string */

	wbuf[0] = 0;	
	if( strchr( list, ',' ) )				/* allow comma or space separated, but not both */
		tokens = ng_tokenise( list, ',', '^', NULL, &ntoks );
	else
		tokens = ng_tokenise( list, ' ', '^', NULL, &ntoks );

	if( ntoks == 0 )
	{
		ng_free( tokens );
		return NULL;
	}

	lbuf[0] = 0;
	for( i = 0; i < ntoks; i++ )
	{
		freep = 0;
		if( (p = strchr( tokens[i], '@'	)) != NULL )		/* just an address -- it goes as is */
		{
			p = tokens[i];
		}
		else
		{
			if( strchr( tokens[i], '&') ) 			/* allow &string or &{string} to expand to an internal or an environment var */
			{
				char *pp;

				ng_bleat( 2, "expand_mlist: invoke var expansion for: %s", tokens[i] );
				pp = expand_var( tokens[i] );
				if( (p = expand_mlist( pp )) != NULL )
				{
					if( ! *p )
					{
						ng_free( p );
						p = NULL;
					}
					else
						freep = 1;
				}

				ng_free( pp );
			}
			else
			if( strcmp( tokens[i], "nobody" ) == 0 )		/* skip this special token */
				p = NULL;
			else
			if( (p = st_find( SP_NICK, tokens[i] )) )		/* nick in the table -- expand and use */
			{
				p = expand_mlist( p );				/* expand the entry */
				freep = 1;
			}
			else
			if(  *tokens[i] )
			{
				sfsprintf( wbuf, sizeof( wbuf ), "%s@%s", tokens[i], domain );				
				p = wbuf;
			}
			else
				p = NULL;
		}

		if( p )
		{
			llen += strlen( p );
			if( llen > NG_BUFFER-1 )
			{
				ng_bleat( 0, "mailing list expanded beyond sane size limits" );
				if( freep )
					ng_free( p );
				depth--;
				ng_free( tokens );
				return ng_strdup( list );
			}

			strcat( lbuf, p );
			strcat( lbuf, " " );
			if( freep )
				ng_free( p );
		}
	}
	
	depth--;
	ng_free( tokens );

	if( *lbuf )
	{
		ng_bleat( 2, "mlist expansion: %s -> %s", list, lbuf );
		return ng_strdup( lbuf );		
	}

	ng_bleat( 2, "mlist expansion: %s -> result was empty", list );
	return NULL;
}

/* expain the named digest, or all if name is null */
void explain_digest( int sid, char *name )
{
	Digest_t	*dp;
	char	buf[1024];

	for( dp = digests; dp; dp = dp->next )
	{
		if( name == NULL || strcmp( name, dp->name ) == 0 )
		{
			ng_siprintf( sid, "   digest: %s\n", dp->name );
			ng_siprintf( sid, "recipiant: %s\n", dp->user );
			ng_siprintf( sid, "     freq: %I*ds\n", Ii( dp->freq/10 ) );
			ng_stamptime( dp->nxt_send, 1, buf );
			ng_siprintf( sid, "     next: %s\n", buf );
			ng_siprintf( sid, "    fname: %s %s\n", dp->fname, dp->toc_fname );
			if( name )
				return;
			ng_siprintf( sid, "\n" );
		}
	}
}

void explain_test( int sid, Test_t *tp )
{
	char buf[1000];
	char buf2[1000];
	char buf3[1000];
	char	*bufp;
	ng_timetype now;
	int i;
	Test_t *bp;		/* used to count number blocked */
	int blocks = 0;

	now = ng_now( );

	for( bp = tp->blocks; bp; bp = bp->next )
		blocks++;

	ng_siprintf( sid, "      test: %s\n", tp->name );
	ng_siprintf( sid, "      freq: %I*ds\n", Ii(tp->freq/10) );
	ng_siprintf( sid, "     state: %s\n", ok4node( tp ) ? state_text[tp->state] : "DISABLED -- not scopped for this node" );
	ng_siprintf( sid, "     scope: %s\n", tp->scope );
	ng_siprintf( sid, "    blocks: %d\n", blocks );
	ng_siprintf( sid, "    notify: %s\n", tp->notify ? tp->notify : "no list" );
	ng_siprintf( sid, "    digest: log alarm -> normal is %s\n", tp->flags & TF_LOG_OK ? "ON" : "off"  );
	ng_siprintf( sid, "      when: " );

	*buf2 = 0;
	for( i = 0; i <  NSST; i++ )
	{
		if(  tp->when[i] )
		{
			if( i )
				strcat( buf2, "," );
			strcat( buf2, sys_state_text[i] );	
		}
	}
	ng_siprintf( sid, "%s\n", buf2 );

	ng_stamptime( tp->nxt_test, 1, buf );		/* time we next fire the probe */
	bufp = nxt_alarm_string( tp );			/* time we next send an alarm or squelch info */

	ng_siprintf( sid, "  nxt_test: %s\n    alarms: %s\n",  buf, bufp );
	ng_free( bufp );

	if( tp->flags & TF_VERBOSE )
	{
		ng_siprintf( sid, "   verbose: on\n" );
		ng_siprintf( sid, "   nprobes: %I*d\n", Ii(tp->nprobes) );
		ng_siprintf( sid, "   nalerts: %I*d\n", Ii(tp->nalert) );
		ng_siprintf( sid, "    nquest: %I*d\n", Ii(tp->nquest) );
		ng_siprintf( sid, "   retries: %I*d/%I*d\n", Ii(tp->retry), Ii(tp->retry_freq) );
		ng_siprintf( sid, "     flags: 0x%I*x\n", Ii(tp->flags) );
		ng_siprintf( sid, "  alrm key: %s\n",  tp->key ? tp->key : "<unset>" );
	}
	else
		ng_siprintf( sid, "   verbose: off\n" );

	ng_siprintf( sid, "       cmd: %s\n\n", tp->cmd );
}

/* called by symtraverse to explain each and every in a very random order */
void dump_test( Syment *se, void *data )
{
	Test_t *tp;

	tp = (Test_t *) se->value;
	if( ! limit_dump || tp->state )		/* show only non-ok tests */
		explain_test( *((int *) data), tp );
}

/* allow the name of a test to be passed in and explained back to a tcp session 
	if the name is ALARM, then we explain all tests that are not OK
*/
void explain_named_test( int sid, char *name )
{
	Test_t	*tp;

	if( strcmp( name, "ALARM" ) == 0 )
	{
		limit_dump = 1;
		symtraverse( symbols, SP_TEST, dump_test, (void *) &sid );
		return;
	}
		
	limit_dump = 0;
	if( (tp = st_find( SP_TEST, name )) != NULL )
		explain_test( sid, tp ); 
	else
		ng_siprintf( sid, "explain: unknown test: %s\n", name );
}

/* lists the tests in the chain, blocked by */
void list_test_chain( int sid, Test_t *tp, int level )
{
	int i = 0;
	int fp = 0;			/* fali percentage */
	int qp = 0;			/* questionable percentage */

	if( level > 20 )
	{
		ng_bleat( 0, "too deep in test chain list" );
		exit( 2 );
	}

	if( ! level )
		ng_siprintf( sid, "%30s\t%10s %6s %10s %10s %s\n", "TEST", "STATE", "nPROBE", "---FAIL---", "----ODD---", "NOTIFY" );
	for( tp; tp; tp = tp->next )
	{
		if( tp->nprobes )
		{
			fp = (tp->nalert*100)/tp->nprobes;
			qp = (tp->nquest*100)/tp->nprobes;
		}
		else
			fp = qp = 0;				/* if disabled, don't use the last one */

		ng_siprintf( sid, "%30s\t%10.10s %6I*d %5I*d %3d%% %5I*d %3d%% %s\n", tp->name, ok4node(tp) ? state_text[tp->state] : "DISABLED", Ii(tp->nprobes), Ii(tp->nalert), fp, Ii(tp->nquest), qp, tp->notify );

		if( tp->blocks )
			list_test_chain( sid, tp->blocks, level+1 );
	}
}


/* cause all tests to be dumpped */
void dump_all( int sid )
{
	symtraverse( symbols, SP_TEST, dump_test, (void *) &sid );
}

/*
	get the next record. if need_tab is set, we only want the next record if it is
	started with a tab.  we expect that leading whitespace is stripped from the record.
*/
char *next_rec( Sfio_t *f, int need_tab )
{
	static char *next = NULL;		/* pointer at next buffer if we did a 'look ahead' */
	
	char	*buf;
	char	*p;
	int	is_tabbed;		/* set to 1 to indicate first char is a tab */

	if( next )
	{
		if( need_tab )
			return NULL;			/* if we have it in next, it did not have a lead tab */

		buf = next;
		next = NULL;	
		return buf;
	}

	while( (buf = sfgetr( f, '\n', SF_STRING)) )
	{
		chop( buf );	/* ditch comment and trailing whitespace */

		is_tabbed = *buf == '\t';
		p = buf;
		while( isspace( *p ) )
			p++;

		if( *p != 0  )		/* not a blank or comment line */
		{
			if( need_tab )
			{
				if( is_tabbed )
					return p;
				next = p;		/* hold for next call */
				return NULL;		/* not a tabbed line, return null to indicate end of section */
			}

			next = NULL;
			ng_bleat( 4, "next: it=%d nt=%d %s", is_tabbed, need_tab, p  );
			return p;
		}
	}

	ng_bleat( 4, "next: end of file" );

	return NULL;
}


/* compute the number of tenths of seconds until the next minute interval (5,10,15 etc) 
   the interval must divide evenly into 60 or there will be odd results 
*/
long tsec_until_interval( int interval )
{
	ng_timetype now;
	long	n;

	if( interval == 0 )
		interval = 60;

	now = ng_now( );
	
	n = interval * 600;		/* convert interval min to tsec */
	return n - (now % n);
}

/* compute the number of tenths of seconds until the specified hour:min 
*/
long tsec_until_time( int hr, int min )
{
	ng_timetype now;
	long 	v;
	long	n;

	v = (hr * 36000) + (min * 600);		/* number of tseconds past midnight hr:min is */
	now = ng_now( ) % 864000;		/* number of tsec past midngight right now is */
	n = v - now;
	return n < 0 ? n+864000 : n;
}

/* convert  hh:mm or mm to tenths of seconds */
long time2tsec( char *tstring )
{
	char *p;

	if( (p = strchr( tstring, ':' )) != NULL )
		return ((atol(tstring ) * 3600) + (atol( p+1 ) * 60)) * 10;

	return atol( tstring ) * 600; 
}

/* -------------------- test struct management ----------------------------- */
#define FREE_IT(a)	if((a)) ng_free((a))
int scratch_test( Test_t *tp )
{
	Test_t 	*btp;			/* pointer at test that blocks us */
	
	if( tp == NULL )
		return;

	if( tp->blocks )
	{
		ng_bleat( 0, "attempt to scratch test that blocks others, not scratched: %s", tp->name );
		return 0;
	}

	if( tp->prev )
		tp->prev->next = tp->next;
	else
	{
		if( tp->after )				/* the head pointer is a block pointer, not tlist */
		{
			if( (btp = st_find( SP_TEST, tp->after )) != NULL )
			{
				ng_bleat( 2, "test scratched from blocking list of: %s", btp->name );
				btp->blocks = tp->next;				/* remove from their head of list */
			}
			else
			{
				ng_bleat( 0, "error scratching %s: cannot find after: %s", tp->name, tp->after );
				return 0;				/* internal error */
			}
		}
		else
		{
			if( tp == startup_tests )
				startup_tests = tp->next;
			else
				if( tp == tlist )
					tlist = tp->next;
		}
	}

	if( tp->next )
		tp->next->prev = tp->prev;

	st_del( SP_TEST, tp->name );
	FREE_IT( tp->name );
	FREE_IT( tp->after );
	FREE_IT( tp->scope );
	FREE_IT( tp->desc );
	FREE_IT( tp->cmd );
	FREE_IT( tp->id );
	FREE_IT( tp->notify );
	FREE_IT( tp->key );
	FREE_IT( tp->sub_data );

	ng_free( tp );
	
	return 1;
}

/* 	create a new test block.  Inserts it into the symbol table and returns pointer. 
	parses lines from the config file (f), until a non-tabbed line, and fills 
	the data into the new block.
*/
Test_t *new_test( char *fname, Sfio_t *f, char *name )		
{
	Test_t *tp;
	char	*buf;				/* pointer at buffer from config file */
	char	*p;				/* pointer into buffer to parse */
	char	*strtok_p = NULL;		/* for strtok_r calls */
	int	count = 0;			/* count of records following test; if zero user probably did not tab indent */

	if( (tp = st_find( SP_TEST, name )) != NULL )
	{
		ng_bleat( 0, "test is being overlaid, duplicate name in config?: %s", name );
		scratch_test( tp );
	}

	tp = (Test_t *) ng_malloc( sizeof( Test_t ), "test_t block" );
	memset( tp, 0, sizeof( Test_t ) );

	tp->flags = TF_DEFAULTS;		/* default flags */
	tp->retry = 1;				/* 1 is actually 0 retries; we run the probe while attempt < retry */
	tp->state = ST_OK;
	tp->freq = MINT_15;
	tp->name = ng_strdup( name );
	tp->squelch = MINT_30;
	tp->scope = ng_strdup( "all" );		/* test runs on each node */
	tp->when[SST_STABLE] = 1;		/* default to running only when stable */

	if( f )					/* parse input lines up to next non-tabbed line and initialise the tp struct */
	{
		while( (buf = next_rec( f, 1 )) )
		{
			count++; 				
							/* dont use strtok as for some statements we want everything past cmd token */
			for( p = buf; *p && !isspace( *p ); p++ );		/* at first blank after keyword */
			for( p; *p && isspace( *p ); p++ );			/* at first non blank of data */
			switch( *buf )
			{
				case 'a':			/* after <string> */
					if( p )
						tp->after = ng_strdup( p );
					else
					{
						ng_bleat( 0, "%s: test not added: no data followed keyword 'after' for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;
					break;

				case 'c':			/* command <string> */
					if( p )
					{
						if( tp->cmd )
							ng_free( tp->cmd );
						tp->cmd = ng_strdup( p );
					}
					else
					{
						ng_bleat( 0, "%s: test not added: no command string following 'command' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;

				case 'd':			/* description <string> */
					if( p )
						tp->desc = expand_var( p );
					else
					{
						ng_bleat( 0, "%s: test not added: no description string following 'desc[ription]' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;

				case 'f':			/* freq {hh:mm|mm} */
					if( p )
						tp->freq = time2tsec( p );
					else
					{
						ng_bleat( 0, "%s: test not added: no data following 'freq[ency]' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;

				case 'l':			/* logok yes/no */
					if( p )
					{
						if( *p == 'n' || *p == 'N' )
							tp->flags &= ~TF_LOG_OK;		/* turn off if no */
					}
					else
					{
						ng_bleat( 0, "%s: test not added: no data following 'logok' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;

				case 'n':			/* notify <data> */
					if( p )
					{
						/*chop( p );*/
						tp->notify = expand_var( p );
					}
					else
					{
						ng_bleat( 0, "%s: test not added: no nickname/address list following 'notify' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;


				case 'r':			/* retry <n> [<sec>] */
					if( p )
					{
						tp->retry_freq = 0;			/* default to using freq/2 */
						tp->retry = atoi( p ) + 1;		/* this is an attempt value more than a retry */
						for( p++; *p && !isspace( *p ); p++ );	/* find blank after n */
						if( *p )
						{
							for( p++; *p && isspace( *p ); p++ );
							if( *p )					/* ahhh, at the retry freq */
								tp->retry_freq = atoi( p ) * 10;			/* retry frequency (tseconds) */
						}
					}
					else
					{
						ng_bleat( 0, "%s: test not added: no data following 'retry' keyword for test %s", fname, name );
						scratch_test( tp );
						return NULL;
					}
					break;

				case 's':			/* squelch {change|reset|always|hh:mm|mm} */
								/* scope  node|Leader|Filer|Jobber... */
					if( *(buf+1) == 'q' )
					{
						if( p )
						{
							switch( *p )
							{
								case 'a':
									tp->flags |= TF_SQ_ALWAYS;
									tp->flags &= ~TF_SQ_TILCHANGE;
									tp->flags &= ~TF_SQ_TILRESET;		
									break;

								case 'r':				/* squelch until alarm is reset */
									tp->flags &= ~TF_SQ_TILCHANGE;
									tp->flags &= ~TF_SQ_ALWAYS;
									tp->flags |= TF_SQ_TILRESET;		
									break;
								case 'c':					/* squelch until key changes */
									tp->flags &= ~TF_SQ_TILRESET;
									tp->flags &= ~TF_SQ_ALWAYS;
									tp->flags |= TF_SQ_TILCHANGE;
									break;
								default:
									tp->flags &= ~(TF_SQ_TILRESET + TF_SQ_TILCHANGE + TF_SQ_ALWAYS);
									tp->squelch = time2tsec( p );
									break;
							}
						}
						else
						{
							ng_bleat( 0, "%s: test not added: no data following 'squelch' keyword for test %s", fname, name );
							scratch_test( tp );
							return NULL;
						}
					}
					else
					{
						if( p )
						{
							char *p1;
							for( p1 = p+1; *p1 && !isspace( *p1 ); p1++ );
							*p1 = 0;
							if( tp->scope )
								ng_free( tp->scope );
							tp->scope = ng_strdup( p );
						}
						else
						{
							ng_bleat( 0, "%s: test not added: no data following 'scope' keyword for test %s", fname, name );
							scratch_test( tp );
							return NULL;
						}
					}
					break;			

				case 't':			/* time hh[:mm]  -- delay until this time */
					if( p )
					{
						int hr;
						int mn = 0;

						hr = atoi( p );
						if( (p = strchr( p, ':' )) != NULL )
							mn = atoi( p+1 );
						tp->nxt_test = ng_now( ) + (ng_timetype) tsec_until_time( hr, mn );	/* number to tenths until that time */
					}
					break;

				case 'v':
					tp->flags |= TF_VERBOSE;
					break;

				case 'w':				/* when ok to run based on system state:  when {stable|busy|crisis|nonet} */
					if( p )
					{
						char *tok;

						tok = strtok_r( p, " ", &strtok_p );
						memset( tp->when, 0, sizeof( tp->when ) );
						while( tok )
						{
							switch( *tok )
							{
								case 'c':	tp->when[SST_CRISIS] = 1; break;
								case 'b':	tp->when[SST_BUSY] = 1; break;
								case 'n':	tp->when[SST_NONET] = 1; break;
								case 's':		/* stable or startup */
									if( *(tok+3) == 'r' )
										tp->when[SST_STARTUP] = 1; 	/* when we order tests these put on special list */
									tp->when[SST_STABLE] = 1; 		/* startup tests get stable attr too */
									break;

								default:
									ng_bleat( 0, "invalid 'when' constant: %s->%s", fname, tp->name, tok );
									break;
							}
							tok = strtok_r( NULL, " ", &strtok_p );
						}

					}
					break;

				default:
					ng_bleat( 0, "%s: test not added: unrecognised keyword or phrase in definition of %s(test): %s", fname, name, buf );
					scratch_test( tp );
					return NULL;
					break;
 
			}
		}
	}

	if( !count )
	{
		ng_bleat( 0, "test %s: test not added: missing description: %s", fname, name );
		ng_bleat( 0, "\tprobable cause:  test description lines must begin with tab", name );
		scratch_test( tp );
		return NULL;
	}
	if( tp->cmd == NULL )			/* um, all tests should have a command; if not delete it */
	{
		ng_bleat( 0, "test %s: test not added: no command found in description: %s", name, tp->name );
		scratch_test( tp );
		return NULL;
	}

	ng_bleat( 1, "%s: test added: flags=0x%0x", tp->name, tp->flags  );
	st_insert( SP_TEST, tp->name, tp );			/* add tp to the symbol table -- only after everyting turned out ok */

	return tp;
}

/* unchain the test -- called by syntraverse and we expect that we are unchaining everything so 
   we do NOT worry about fixing prev->next pointers and the like.
*/
void unchain_tests( Syment *se, void *data )
{
	Test_t *tp;		/* pointer at test to place into the list */

	if( (tp = (Test_t *) se->value) != NULL )
		tp->next = tp->prev = tp->blocks = NULL;
}


/*	after all tests have been read from the config file, order them based on after statements 
	invoked via symtraverse()
*/
void order_tests( Syment *se, void *data )
{
	Test_t *tp;		/* pointer at test to place into the list */
	Test_t *atp;		/* pointer at test we run after */

	tp = (Test_t *) se->value;

	tp->flags &= ~TF_LOOPCK;		/* clear the loop check flag */

	if( tp->when[SST_STARTUP] )
	{
		ng_bleat( 2, "order tests: inserting %s to run at startup", tp->name );
		tp->next = startup_tests;
		if( tp->next )
			tp->next->prev = tp;
		startup_tests = tp;
		return;
	}
	else
	{
		ng_bleat( 2, "order tests: inserting %s to run after: %s", tp->name, tp->after ? tp->after : "no after specified" );
		if( tp->after )
		{
			if( (atp = st_find( SP_TEST, tp->after )) != NULL )
			{
				tp->next = atp->blocks;		/* push onto the blocks list for the test named as after */
				if( tp->next )
					tp->next->prev = tp;
				atp->blocks = tp;
				return;
			}
			else
				ng_bleat( 0, "warning: after (%s) specified for test %s is not found; %s will run as generic test", tp->after, tp->name, tp->name );
		}
	}

	tp->nxt_test = 0;
	tp->next = tlist;		/* push onto the list */
	if( tp->next )
		tp->next->prev = tp;
	tlist = tp;
}



/* 	ensure the after specifications did not create a circular list 
	we set the TF_LOOPCK flag on each test block we see. If we find
	one that has the flag already set we assume a loop.
*/
void loop_test( Test_t *list )
{
	char	*start;
	Test_t	*tp;

	if( ! list )
		return;

	tp = list;
	start = tp->name;


	for( ; tp; tp = tp->next )
	{
		ng_bleat( 2, "loop test: encountered %s 0x%02x", tp->name, tp->flags );
		if( tp->flags & TF_LOOPCK  )
		{
			ng_bleat( 0, "abort: after specification(s) cause loop that leads back to %s", tp->name );
			exit( 2 );
		}

		tp->flags |= TF_LOOPCK;
		if( tp->blocks )		/* if this test blocks some other test; run it before going on */
		{
			ng_bleat( 2, "loop_test: %s blocks tests, testing for loops starting with: %s", tp->name, tp->blocks->name );
			loop_test( tp->blocks );
			ng_bleat( 2, "loop_test: continuing with %s tree (%s %x/%x)", list->name, tp->next ? tp->next->name: "end of list", tp, tp->next );
		}

	}	

	ng_bleat( 2, "loop_test complete; no loops in chain that starts with: %s", start );
}

/* 	check to see if a test appears to be isolated from the main line -- invoked by symtraverse() 
   	we assume loop check has been run and all tests that are inserted into the tree should have
	their loopck flag set. If not, then assume the test is isolated
*/
void iso_test( Syment *se, void *data )
{
	Test_t *tp;		/* pointer at test to place into the list */

	tp = (Test_t *) se->value;
	if( tp->flags & TF_LOOPCK )			/* was touched by loop test */
		return;

	ng_bleat( 0, "test appears isolated; possible circular after clauses: %s", tp->name );
	loop_test( tp ); 
}

/* -------------------------- test execution --------------------------------------- */
/*	send an alert -- if digests_only == 1, then we format the header and add to digests 
	likely a state change back to the OK flavour. tag is written before the header 
	as a 'reason' for the message.
*/
void send_alarm( Test_t *tp, char *status, char *fname, char *tag, int digests_only )
{
	Sfio_t	*f;	
	char	buf[1024];
	char	hfname[1024];
	char	cmd[NG_BUFFER];
	char	*p;
	char	*send_to = NULL;		/* expanded list of recipients */


	if( tp )
	{
		sfsprintf( hfname, sizeof( hfname ), "%s.header", fname );		/* header information that will go into the mail */

		if( (f = ng_sfopen( NULL, hfname, "w" )) == NULL)
		{
			ng_bleat( 0, "send_alarm: unable to open header file: %s", hfname );
			return;
		}

		ng_stamptime( ng_now(), 1, buf);
		sfprintf( f, "       date: %s\n", buf );
	
		sfprintf( f, " probe host: %s (%s)\n", this_node, sys_state_text[cur_sst] );
		sfprintf( f, "    problem: %s (%s)\n", tp->desc, tp->name );
		sfprintf( f, "     status: %s [%s] %da/%dr %s\n", status, state_text[tp->state], tp->attempts, tp->retry, tp->key ? tp->key : ""  );
	
		if( digests_only || !tp->notify  || strcmp( tp->notify, "digests-only" ) == 0 )
			sfprintf( f, "     notify: %s\n", "digests-only"  );
		else
		{
			send_to = expand_mlist( tp->notify );
			sfprintf( f, "     notify: %s -> %s\n", tp->notify, send_to ? send_to : "EMPTY-LIST"  );
		}

		p = nxt_alarm_string( tp );
		sfprintf( f, " next alarm: %s\n", p );

		sfprintf( f, "   test cmd: %s\n", tp->cmd );
		sfprintf( f, "test output: %s\n", fname );
		sfprintf( f, "\n" );
		
		ng_free( p );
		if( sfclose( f ) )
			ng_bleat( 0, "write error to header file: %s: %s", hfname, strerror( errno ) );
	
	
		if( ok2mail && send_to )
		{
			sfsprintf( cmd, sizeof( cmd ), "cat %s %s | ng_mailer -s \"[SPY] %s: %s\" -T \"%s\" - ", hfname, fname, this_node, tp->sub_data ? tp->sub_data : tp->desc, send_to );
			ng_bleat( tp->flags & TF_VERBOSE ? 0 : 1, "%s: sending alert: %s", tp->name, cmd );
			system( cmd );
			ng_free( send_to );
		}
		else
			ng_bleat( 1, "%s: alert not sent: %s", tp->name, send_to ? "ok to send flag not set" : "send_to string was NULL" );

		add2digests( tp->desc, tag, fname, hfname );	/* add to digests even if notification was empty */
		if( tp->state != ST_ALARM )
		{
			unlink( hfname );		/* we keep files only if we alarm */
			unlink( fname );
		}
	}					/* tp was not null */
}

/* test the  state flags in tp and return 1 if we are allowed to run in the current state */
int okstate( Test_t *tp )
{
	if( tp )
	{
		if( ! tp->when[cur_sst] )
			ng_bleat( 2, "test blocked because of current state (%s): %s", sys_state_text[cur_sst], tp->name );
		return tp->when[cur_sst];
	}

	return 0;
}


/* return 1 if test should be run on this node */
int ok4node( Test_t *tp )
{
	static	char	**tokens = NULL;

	char	*attr;				/* attrs currently defined in cartulary */
	char	eattr[NG_BUFFER];		/* attribute list with spaces fore and aft */
	char	tbuf[256];			/* build needed attribute with some delimiter on each side for test */
	char	*p;
	char	*need;				/* needed attr (without ! if ! is set) */
	int	not_flag = 0;			

	if( strcmp( tp->scope, "all" ) == 0 )		/* general specification to run on all nodes */
		return 1;

	
	if( (attr = ng_env( "NG_CUR_NATTR" )) != NULL )
	{
		p = strrchr( attr, '~' ) + 1; 				/* format is timestamp~cmd~attrlist */
		sfsprintf( eattr, sizeof( eattr ), " %s ", p );		/* pluck from environment goo and surround with blanks */
		
		if( *tp->scope == '!' )
		{
			need = tp->scope + 1;
			not_flag = 1;
		}
		else
			need = tp->scope;

		sfsprintf( tbuf, sizeof( tbuf ), " %s ", need );
		if( strstr( eattr, tbuf ) )			/* if the needed attribute is in the list */
		{
			ng_bleat( 2, "ok4node: %s: %s(scope) found in list: %s", tp->name, tp->scope, p );
			ng_free( attr );
			return 1 - not_flag;
		}

		ng_bleat( 2, "ok4node: %s: %s(scope) was not found in list: %s", tp->name, tp->scope, p );


		ng_free( attr );
	}
	else
		ng_bleat( 0, "cannot get NG_CUR_NATTR from environment" );

	return not_flag;
}

/* 	run the specified chain of tests and submit any that need it.
*/
void drive_tests( Test_t *tp )
{
	ng_timetype now;
	char	cmd[NG_BUFFER];

	now = ng_now( );
	for( ; tp; tp = tp->next )
	{
		/* test is not pending, cur time > next test time, state is ok, and attributes for the node are ok, then run */
		if( !(tp->flags & TF_PENDING) && tp->nxt_test <= now ) 
		{
			if( okstate( tp ) && ok4node( tp ) )		
			{
				tp->nprobes++;
				tp->attempts++;				/* mark one more try */
				tp->flags |= TF_PENDING;
				tp->nxt_test = now + 36000;		/* should we not hear back from the test in an hour we pop */
				sfsprintf( cmd, sizeof( cmd ), "ng_spy_wrapper -p %s -i %s %s", port,  tp->name, tp->cmd );
				ng_bleat( 1, "kicking off test: %s", cmd );

				system( cmd );
			}
			else
				tp->nxt_test = now + tp->freq;		/* if blocked, try again later */
		}
		else
		{
			if( (tp->flags & TF_PENDING) && tp->nxt_test <= now ) 		/* looks like we missed a response? */
			{
				if( tp->flags & ST_RETRY )				/* assume something with previous probes is wedged */
				{
					ng_bleat( 0, "no responses from probe, migh be wedged: %s", tp->name );
					tp->nxt_test = now + 36000;			/* keep these bleats to a minimum in the log */
				}
				else
				{
					ng_bleat( 0, "test response took too long, issuing one retry: %s", tp->name );
					tp->flags &= ~TF_PENDING;
					tp->state = ST_RETRY;					/* simulate a retry */
				}
			}
		}

		if( tp->state == ST_OK && !(tp->flags & TF_PENDING) && tp->blocks )		/* test state ok, not pending and blocks something */
			drive_tests( tp->blocks );						/* kick those that are blocked */
	}
}

/* deal with a squelch command 
	we expect state to be one of:
		on - always squelch alarms until this is turned off
		off- reset the squelch timer
		change - squelch until change
		reset - squelch til reset
		reset- squelch alarms until we see a normal state from the test
		n - seconds that alarms will be squelched
*/
void process_squelch( char *tname, char *state )
{

	Test_t 	*tp;

	if( !state || ! tname || ! *state )			/* bail if empty state string */
		return;

	if( (tp = st_find( SP_TEST, tname )) != NULL )
	{
		switch( *state )
		{
			case 'o':		/* allow alarms again -- squelching if off */
				if( *(state+1 ) == 'f' )
				{
					tp->nxt_alarm = 0;
					tp->flags &= ~TF_SQUELCHED;
					tp->flags &= ~TF_SQ_TILRESET;
					tp->flags &= ~TF_SQ_TILCHANGE;
					tp->flags &= ~TF_SQ_ALWAYS;
				}
				else
				{
					tp->nxt_alarm = 0;
					tp->flags |= TF_SQ_ALWAYS;
				}
				break;

			case 'c':		/* squelch until alarm key has changed */
				tp->flags &= ~TF_SQ_TILRESET;
				tp->flags |= TF_SQ_TILCHANGE;
				tp->flags &= ~TF_SQ_ALWAYS;
				if( tp->state != ST_OK )
					tp->flags |= TF_SQUELCHED;
				break;
				
				
			case 'r':		/* squelch until reset -- turn on if in alarm state */
				if( tp->state != ST_OK )
					tp->flags |= TF_SQUELCHED;

				tp->flags &= ~TF_SQ_TILCHANGE;
				tp->flags |= TF_SQ_TILRESET;
				tp->flags &= ~TF_SQ_ALWAYS;
				break;	

			
			default:			/* assume number of seconds  or hh:mm */
				tp->nxt_alarm = ng_now() + time2tsec( state );
				tp->flags &= ~TF_SQUELCHED;
				tp->flags &= ~TF_SQ_TILRESET;
				tp->flags &= ~TF_SQ_ALWAYS;
				break;
		}
	}
}


/* handle verbose testname command from tcp 
	state may be: on | off  | 0 | 1
	if state is not recognised, then the default is on.
*/
void set_test_verbose( char *tname, char *state )
{
	Test_t *tp;
	int vstate = 0;

	if( !tname || !(*tname) || !(*state) )
		return;

	if( (tp = st_find( SP_TEST, tname)) != NULL )
	{
		if( tolower( *state ) == 'o' )
			vstate = tolower( *(state+1) ) == 'n';
		else
			vstate = !(*state == '0');

		if( vstate )
			tp->flags |= TF_VERBOSE;
		else
			tp->flags &= ~TF_VERBOSE;
	}
}

/* deal with a completed test 
   id is the test name that was sent back by spy_wrapper (given on the command line); just the probe name right now
   status is test state value (ST_ constants)
   fname is the file that the probe output is in 
   sdatea  is subject line data for alarm messages
*/
void process_status( char *id, char *status, char *fname, char *sdata, char *key )
{
	Test_t	*tp;
	char	*name;		/* name of the test -- pull from id */
	int	s;		/* converted status */
	int	prev_state;	/* the state of the test before this call */
	ng_timetype now;
	char	buf[1024];
	char	switchbuf[1024];

	now = ng_now( );
	name = id;		/* if we get more complex with the id, we will need to disect it to get the test name */

	if( ! *sdata )
		sdata = NULL;

	ng_bleat( 2, "process-status: %s(id) %s(status) %s(sdata) %s(key)", id, status, sdata ? sdata : "none", key ? key : "none" );

	if( (tp = st_find( SP_TEST, name )) != NULL  )
	{
		tp->nxt_test = now + tp->freq;			/* dont retry until later */
		tp->flags &= ~TF_PENDING;			/* test no longer pending */
		prev_state = tp->state;				/* we use this to detect a change in state */

		if( tp->sub_data )
			ng_free( tp->sub_data );

		if( sdata )
		{
			ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: process_status subject data= %s", tp->name, sdata );
			tp->sub_data = ng_strdup( sdata );
		}
		else
			tp->sub_data = NULL;
		

		s = atoi( status );
		if( s == 127 )				/* command not found, user probably buggered cfg file */
			sfsprintf( switchbuf, sizeof( switchbuf ), "%s ==> 127 (missing probe command?)", state_text[prev_state] );
		else
		{
			if( s > NST )
				sfsprintf( switchbuf, sizeof( switchbuf ), "%s ==> %s", state_text[prev_state], status );
			else
				sfsprintf( switchbuf, sizeof( switchbuf ), "%s ==> %s", state_text[prev_state], state_text[s] );
		}

		switch( s )				/* test can return s > 1 to indicate special reaction */
		{
			case ST_OK:
				tp->state = ST_OK;
				if( prev_state == ST_ALARM  && tp->flags & TF_LOG_OK && !(tp->flags & TF_SQ_ALWAYS) )	/* save a state change in the digests */
					send_alarm( tp, status, fname, switchbuf, 1 );

				if( tp->blocks )
					drive_tests( tp->blocks );			/* now drive the tests blocked by this one, if it is time */

				tp->attempts = 0;
				tp->flags &= ~TF_SQUELCHED;				/* reset squelched satatus and time info */
				tp->nxt_alarm = 0;
				unlink( fname );

				if( tp->key )						/* drop change key information */
				{
					ng_free( tp->key );
					tp->key = NULL;
				}
				break;

			case ST_BLOCK:					/* block any dependant tests, but do not alarm */
				tp->nquest++;				/* questionable state count */
				tp->state = ST_BLOCK;
				ng_bleat( tp->flags & TF_VERBOSE ? 0 : 3, "%s: test state set to block dependant probes" );
				unlink( fname );
				break;

			case ST_RETRY:			/* we should set next test to 1/2 usual frequency and retry, if not in this state */
				if( tp->state != ST_RETRY )
				{
					tp->nquest++;				/* questionable state count */
					tp->state = ST_RETRY;
					tp->nxt_test = now + (tp->retry_freq ? tp->retry_freq : (tp->freq/2));
					ng_stamptime( tp->nxt_test, 1, buf );
					ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: test state set to retry:  %s", tp->name, buf );
					unlink( fname );
					break;
				}
				/* already in a retry, so we assume its a hard failure and fall into alarm */

			case ST_ALARM:					/* usually if test sends a status of 1 */
			default:
				if( tp->attempts >= tp->retry )		/* we have had retry number of failures; time to alert */
				{
					int ok2alarm = 0;

					tp->nalert++;				/* alarm state state count (even if we do not send an alert */
					tp->state = ST_ALARM;

					if( tp->flags & TF_SQ_ALWAYS )
						ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: alarm not sent: always squelch is on", tp->name );
					else
					{
						if( !(tp->flags & TF_SQUELCHED) )				/* not currently squelched */
						{
							ok2alarm = 1;	
						}
						else				/* we've alarmed; send another if past time or there is a change to the key */
						{	
							if( !(tp->flags & TF_SQ_TILRESET) )			/* if squelched til reset, we cannot do anything */
							{
								if( tp->flags & TF_SQ_TILCHANGE  )
								{
									if( strcmp( tp->key ? tp->key : "yy", key  ? key : "xx" ) != 0 )
										ok2alarm = 1;				/* til change set and something in the output changed */
									else
										ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: alarm not sent: no change & squelched til change is on", tp->name );
								}
								else
								{
									if( now >= tp->nxt_alarm )
										ok2alarm = 1;
									else
										ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: alarm not sent: squelch lease not expired", tp->name );
								}
								
							}
							else
								ng_bleat( tp->flags & TF_VERBOSE ? 0 : 2, "%s: alarm not sent: squelched til reset is on", tp->name );
						}
					}

					if( ok2alarm )
					{
						send_alarm( tp, status, fname, switchbuf, 0 );
						tp->nxt_alarm = now + tp->squelch; 			/* dont alarm until at least this time */
						tp->flags |= TF_SQUELCHED;		
						if( tp->key )						/* save key so we can detect a change in alarm output */
							ng_free( tp->key );
						if( key )
							tp->key = ng_strdup( key );
						else
							tp->key = NULL;
					}
#ifdef ORIGINAL_AND_CONFUSING
					if( now >= tp->nxt_alarm && !(tp->flags & TF_SQUELCHED) )
					{
						if( !(tp->flags & TF_SQ_TILCHANGE) || strcmp( tp->key ? tp->key : "yy", key  ? key : "xx" ) != 0 )
							send_alarm( tp, status, fname, switchbuf, 0 );
						tp->nxt_alarm = now + tp->squelch; 		/* dont alarm until at least this time */
						if( tp->flags & TF_SQ_TILRESET )		/* if blocking alarms until after rest, */
							tp->flags |= TF_SQUELCHED;		/* indicate that we have squelched */

						if( tp->key )					/* save key only if we alarm */
							ng_free( tp->key );
						if( key )
							tp->key = ng_strdup( key );
						else
							tp->key = NULL;
					}
#endif
				}
				else
				{
					tp->nquest++;				/* questionable state count */
					tp->state = ST_RETRY;	
					tp->nxt_test = now + (tp->retry_freq ? tp->retry_freq : (tp->freq/2));
				}
				break;
		}

		ng_stamptime( tp->nxt_test, 1, buf );
		ng_bleat( 1, "test complete: %s status=%s %s flg=0x%x  next test: %s ", tp->name, status, state_text[tp->state], tp->flags, buf  );


		if( verbose > 3 )
			dump( tp );
	}
	else
		ng_bleat( 0, "status received for unknown test: %s(name) %s(status) %s(file)", id, status, fname );
}

/* ----------------------------------------------- ??? --------------------------------- */
/* returns 1 if successful */
int parse_config( char *fname )
{
	Sfio_t	*f;
	char	*buf;
	char 	*tok;
	char	*p;			/*  pointer into the buffer */
	char	*p1;
	int	old_verbose; 
	char	*strtok_p = NULL;
	Test_t	*nxt_tp = NULL;
	Test_t	*tp = NULL;

	if( (f = ng_sfopen( NULL, fname, "r" )) == NULL)
	{
		ng_bleat( 0, "unable to open config file: %s", fname );
		return 0;
	}

	startup_tests = NULL;				/* must release all pointers before parsing; we rebuild linkage at end */
	tlist = NULL;
	symtraverse( symbols, SP_TEST, unchain_tests, 0 );		/* unchain all of the tests first */

	old_verbose = verbose;

	ng_bleat( 1, "parsing config file: %s", fname );


	while( (buf = next_rec( f, 0 )) != NULL )
	{
		while( *buf == ' ' ) 
			buf++;

		ng_bleat( 4, "%s: %s", fname, buf );
		switch( *buf )
		{
			case 'd':			/* domain research.att.com or digest user hh:mm */
				if( strncmp( buf, "digest", 6 ) == 0 )
				{
					int	freq = 0;
					char	*p2 = NULL;
					char	*p3 = NULL;

					strtok_r( buf, " \t", &strtok_p );		
					tok = strtok_r( NULL, " \t", &strtok_p );	/* at digest name */
					p = strtok_r( NULL, " \t", &strtok_p );	/* at email addr or nickname */
					p2 = strtok_r( NULL, " \t", &strtok_p );	/* at user frequency */
					p3 = strtok_r( NULL, " \t", &strtok_p );	/* at 1st delivery time */
					if( tok && p && p2 )
					{
						freq = time2tsec( p2 );
						new_digest( tok, p, freq, p3 );		/* handles duplicates if we reparse the config */
					}
					else
					{
						ng_bleat( 0, "%s: badly constructed digest statement for: %s", fname, tok ? tok : "user name missing" );
					}
				}
				else
				if( strncmp( buf, "domain", 6 ) == 0 )
				{
					if( domain )
					{
						ng_free( domain );
						domain = NULL;
					}

					strtok_r( buf, " \t", &strtok_p );
					tok = strtok_r( NULL, " \t", &strtok_p );
					if( tok )
						domain = expand_var( tok );
				}
				else
					unknown_token( fname, buf, "domain or digest" );
				break;

			case 'i':				/* i[mbed] filename */
				if( strncmp( buf, "imbed", 5 ) == 0 )
				{
					strtok_r( buf, " \t", &strtok_p );
					if( (p = strtok_r( NULL, " \t", &strtok_p )) != NULL )
					{
						p1 = expand_var( p );
						ng_bleat( 2, "parsing include file: %s -> %s", p, p1 );
						parse_config( p1 );
						ng_free( p1 );
					}
					else
					{
						ng_bleat( 0, "%s: badly constructed include satement: missing filename", fname );
					}
				}
				else
					unknown_token( fname, buf, "imbed" );
				break;

			case 'n':                       /* nick[name]  name address [address..] */
				if( strncmp( buf, "nick", 4 ) == 0 )
				{
					strtok_r( buf, " \t", &strtok_p );
					p = strtok_r( NULL, " \t", &strtok_p );              /* point at name */
					p1 = expand_var( p );
					tok = strtok_r( NULL, " \t", &strtok_p );            /* point at address/nickname list */
					if( p1 && tok )				/* do NOT expand tok here; we want to expand when we use it */
					{
						st_free( SP_NICK, p1 );                  /* if there was a value, free it */
						st_insert( SP_NICK, p1, ng_strdup( tok ) );           /* add to symtab */
						ng_free( p1 );
					}
					else
						ng_bleat( 0, "%s: badly constructed nickname satement: missing name and/or value", fname, p ? p : "no-name" );
				}
				else
					unknown_token( fname, buf, "nick[name]" );
				break;

			case 's':				/* set vname=value */
				if( strncmp( buf, "set", 3 ) == 0 )
				{
					if( (p = strchr( buf, '=' )) != NULL )
					{
						*p = 0;
						for( p1 = p+1; isspace( *p ); p++ );		/* skip leading white space after = */
						strtok_r( buf, " \t", &strtok_p );		/* point at set */
						p = strtok_r( NULL, " \t", &strtok_p );         /* skip set and point right at point at name */
						if( p && p1 )
						{
							st_free( SP_VAR, p );                  /* if there was a value, free it */
							st_insert( SP_VAR, p, expand_var( p1 ) );           /* add to symtab */
						}
					}
					else
						ng_bleat( 0, "%s: badly formed set command: %s", fname, buf );
				}
				else
					unknown_token( fname, buf, "set" );
				break;

			case 't':			/* test name */
				if( strncmp( buf, "test", 4 ) == 0 )
				{
					strtok_r( buf, " \t", &strtok_p );
					tok = strtok_r( NULL, " \t", &strtok_p );			/* at name */
					new_test( fname, f, tok );
				}
				else
					unknown_token( fname, buf, "test" );
				break;

			case 'v':
				if( strncmp( buf, "verbose", 6 ) == 0 )
				{
					strtok_r( buf, " \t", &strtok_p );
					if( (p = strtok_r( NULL, " \t", &strtok_p )) != NULL )
						verbose = atoi( p );
					ng_bleat( 0, "verbose set to %d", verbose );
				}
				else
					unknown_token( fname, buf, "verbose" );
				break;
				

			default: 
				unknown_token( fname, buf, NULL );
				break;
		}
	}

	ng_bleat( 1, "config file parsed: %s", fname );
	if( sfclose( f ) )
		ng_bleat( 0, "read error: %s: %s", fname, strerror( errno ) );

	symtraverse( symbols, SP_TEST, order_tests, 0 );  		/* run the symbol table and put tests into order baed on after clauses */
	loop_test( tlist ); 						/* ensure no after loops */
	if( startup_tests )
		loop_test( startup_tests );
	symtraverse( symbols, SP_TEST, iso_test, 0 );	/* see if any tests appear to be isolated from main stream -- abort if so */

	verbose = old_verbose;

	return 1;
}

#include "spy_digest.c"		/* digest processing stuff */
#include "spy_netif.c"		/* networking stuff */

/* ------------------------------------------------------------------------------------------------------ */
void usage( )
{
	sfprintf( sfstderr, "%s version %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-c config-file] [-n] [-p port] [-v]\n", argv0 );
	exit( 1 );
}

int main( int argc, char **argv )
{
	char	*cfile = NULL;
	char	wbuf[NG_BUFFER];
	char	*p;
	char	*strtok_p = NULL;			/* for strtok calls */

	
	ng_sysname( wbuf, sizeof( wbuf ) );
	if( (p = strchr( wbuf, '.' )) != NULL )
		*p = 0;					/* make ningxx.research.att.com just ningxx */
	this_node = ng_strdup( wbuf );

	ARGBEGIN
	{
		case 'c':	SARG( cfile ); break;
		case 'p':	SARG( port ); break;
		case 'n':	ok2mail=0; break;
		case 's':	IARG( cur_sst ); break;		/* set current state to this value */
		case 'v':	OPT_INC( verbose ); break;
usage:
		default: 	usage( ); exit( 1 ); break;
	}
	ARGEND

	if( cur_sst >= NSST )
		cur_sst = NSST-1;					/* keep from coredumpping */

	ng_bleat( 1, "version %s started", version );
	if( ! cfile && ! (cfile = ng_env( "NG_SPYG_CONFIG" )) )
	{
		cfile = ng_rootnm( "/lib/spyglass.cfg" );
		ng_bleat( 1, "warning: NG_SPYG_CONFIG not set, and config file name not supplied with -c using %s", cfile );
	}

	if( ! port && ! (port = ng_env( "NG_SPYG_PORT" )) )
	{
		ng_bleat( 0, "abort: NG_SPYG_PORT not set, and -p port name not supplied on command line" );
		return 1;
	}

	symbols = syminit( 4099 );

	p = strtok_r( cfile, ", \t", &strtok_p );			/* allow multiple config files */
	while( p )
	{
		if( ! parse_config( p ) )
		{
			ng_bleat( 0, "abort: unable to parse the config file %s", p );
			return 1;
		}

		p = strtok_r( NULL, ",  \t", &strtok_p );
	}


	
	init_si( port );			/* initialise the socket interface -- register callbacks */

	if( startup_tests )
	{
		ng_bleat( 2, "running startup tests" );
		drive_tests( startup_tests );
	}

	while( ok2run )
	{
		if( ! pause_until || pause_until <= ng_now( ) )		/* if we are paused, then we skip tests */
		{
			pause_until = 0;
			drive_tests( tlist );
		}

		if( digests )
			send_digests( );
		if( ng_sipoll( 200 ) < 0 )	
		{
			if( errno != EINTR && errno != EAGAIN )
			{
				ng_bleat( 1, "poll returned control to main -- shutting down" );
				ok2run = 0;
			}
		}
	}

	if( digests )
		send_digests( );
	
	return 0;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spyglass:Node and cluster watchdog)

&space
&synop	ng_spyglass [-c configfile] [-n] [-p port] [-v]

&space
&desc	&This is a watchdog programme which drives a series of tests and causes alerts (usually email) 
	to be sent when a test fails.
	Test descriptions are supplied to &this via one or more control files, whose names are supplied 
	on the command line or via the &lit(ng_sgreq parse) command. 
	&This drives the tests asynchronously, based on a schedule determined for each test, such that 
	a long executing test does not block other tests from being started. 
	Tests executed can be limited based on overall system state, and/or based on the attributes
	of the node where &this is running.  
	The notification list is defined for each test, and pinkpages variables can easily be used
	to manage each notification list. 

&space
	In addition to immediatly delivered alert messages, digests can be defined to collect 
	a series of alert messages.  Each digest is defined with the list of recipiants, and the 
	frequency of publication. Typically an hourly and daily digests are collected and published. 

&space
&subcat The configuration file
	The tests that &this will execute are defined by the configuration file, as 
	are the nicknames (mailing lists) that are notified when an alert must be 
	sent.  In addition, status changes of tests (from normal to alert and from alert to 
	normal) can be recorded in one or more digests.  The digests are also defined
	in the configuration file. 
&space
&subcat Nicknames
	The &lit(nickname) statement is used to define a group of users. The nicknames
	then can be used on &lit(notify) statements in a test definition rather 
	than having to list individual users on each test.  Further, nicknames can 
	be 'stacked'  allowing for greater flexibilty when sending alerts. The following 
	is an example that illustrates the stacking of general users and administrators
	into an 'all' catigory:
&space
&litblkb
   nickname all       users,admin
   nickname users     amy,fred
   nickname admin     charlie,robert
&litblke
&space
	Nicknames are not evaluated until there is the need to send an alert, so the 
	order of definition is not important. 
&space
	The names listed are assumed to be nicknames, full email addresses (user@domain) or
	user names in the default domain. If a name does not contain an at sign (@), and does
	not match a defined nickname, then it is assumed to be a user in the default domain
	and an email address will be constructed using that combination. 

&space
&subcat The Default Domain
	The domain statement allows for the definition of the default domain for building 
	email addresses.  It is assumed that it is something of the form &lit(research.att.com).

&space
&subcat	Digest Definitions
	All test state changes are recorded in every defined digest. Digest email messages are 
	sent according to their frequency to the defined user list. Each digest statement has 
	the following syntax:
&space
	&lit(digest) &ital(name users frequency) [&ital(start)]
&space
	Where:
&begterms
&term	name : Is the name of the digest. 
&space
&term	users : Is a comma separated list of nicknames, or email addresses, of the users who 
	are sent the digest. 
&space
&term	frequency : Is the HH:MM formatted frequency of the digest.  The digest will be sent
	using this interval, but only if it is non-empty.
&space
&term	start : Is the HH:MM formatted time that the first digest should be sent. If omitted, 
	the first digest will be sent using the frequency from the time that &this is started. 
	The start parameter is mainly for daily oriented digests. 
&endterms


&space
&subcat The Test Definition
	Tests are defined in the configuation file using the &lit(test) statement. 
	The syntax of the statment is the word &lit(test) followed by the test name. 
	Following the test statement are definition statements.  Each definition 
	statement has the syntax of &lit(<tab><statement-name><whitespace><parms>).
	The following are recognised test definition statements:
&space
&begterms
&term	after : Causes the test to block until the named test has returned a normal state. 
&space
&term	command : Defines the command to be executed.  
	Any command can be executed as a test. Standard output and standard error are captured
	and sent with the alert message to the distribution list.  Further, the exit code 
	from the command is interpreted as follows:
	1 = send alert, 2 = retry the test sooner than frequency, 3 = 
	block dependant test (no alert), 0 = normal state (no alert). 
&space
&term desc : (description) Supplies the string that is sent as the subject when the test results
	in an alarm state. 
&space
&term	freq : Defines the frequency (in hh:mm format) of the test.  &This will execute the 
	test no more frequently than the value supplied.  If the test also has an &lit(after) statement,
	the test will also depend on the frequency of the test that is listed on the &lit(after)
	statement. 
&space
&term	logok : Indicates whether a state change from alarm to normal is logged in the digests being compiled. 
	By default, the state change for a probe is logged. Setting this parameter for a test to 
	'no' will cause the return to a normal state to &stress(not) be logged in any of the digests.
&space
&term 	notify : lists the user(s) that are to be sent an email message when the test
	results in an alert state. The user list (comma separated) can contain 
	literal email addresses (e.g. user@domain), nicknames, or names in the default domain.
	Names supplied in the list that are not nicknames, are assumed to be user names 
	that need the default domain string appended using the form: &lit(name@default-string). 
	In addition to sending email to the notify list, alarm data is recorded in all 
	defined digests. 
&space
	The special name &lit(digests-only) can be used on the notify statment to indicate 
	that the status information is to be added only to digests. 
&space
	The notify statement can be omitted from the test definition. Doing so results in 
	no email, or digest informatoin being generated when the test results in an alert
	state.  Tests of this type are generally used only to block other tests until 
	a known state is reached. 
&space
&term	retry : 
	By default, &this sends an alert when the first failure (exit code 1) is received from a 
	probe.  In some cases, it may be prudent to retry the probe one or more times before reporting
	the alarm (prevents spam email messages if the problems self correct in a short amount of 
	time). When this statement is provided in a test definition, the first parameter defines 
	the number of additional times the probe must fail before the alert is sent.
	If the state returns to normal during the retry attempts, the alert state is cancelled and the counters are rest.  
&space
	An optional value may follow the number of retries.  This is the number of seconds to pause
	between each retry.  If not supplied, the half of the normal test frequency is used.
&space 
&term	scope : Defines the attributes required to execute the test. If the node has the 
	listed attribute (or !attribute) then the test may be executed on the node. 
&space
&term 	squelch : Defines the amount of time (hh:mm) that alert notifications are surpressed
	after the initial alert is sent.  This is used to prevent 'flooding' users with 
	alerts.  
	The squelch token must  be followed by one of:
&begterms
&term	HH:MM : The amount of time before &this is permitted to send another alert message. 
&term	change : Alerts will be squelched until the probe command indicates an alarm condition with 
		an alarm key that is different from the results of previous executions of the test. 
&term	reset : All alerts are supressed until the test state is reset to 'normal' again. 
&term	always : Alert messages for this test are always supressed.  (This can be helpful when testing a 
	new probe command.)
&endterms
&space
&term	verbose : This command word causes does not take any parameters and causes the  verbose flag to be set for the test.  
	When the verbose flag is set, more details about the test are logged to stderr and the &lit(explain) command 
	causes more information to be generated. This can be useful to get added information 
	in the log about a new test without having to increase the general verbosity for the daemon.

&endterms
.if [ 0 ] 
&space
	For most test statements, variable names (e.g. ^&name) can be used. The variable names
	evaluate to internally set variables (those set in the configuration file) or 
	pinkpages variables if a configuration file variable with the given name is not set. These
	allow generic notification names like &lit(^&{NG_CLUSTER}-adm) to be used to 
	reference the administrator(s) for the current cluster. 
.fi

&space
&subcat	Defining and using variables
	With the &lit(set) command, 
	it is possible to define variables that are 'local' to the configuration file. 
	Both variables set in a configuration file, and pinkpages variables, are referenced
	using an apersand (^&) to dereference the name (e.g. ^&foo).
	Any local variable that duplicates a pinkpages variable name will silently shadow the 
	pinkpages value.  The set command has the following syntax:
&space
&litblkb
   set name = value
&litblke
&space
	White space before and after the equal sign is skipped.  The value consists of all tokens
	from the first non-whitespace character until a newline is reached.

&space
&subcat	Misc Configfile Statements
	The &lit(include) statement causes the named file to be read and processed as though 
	its contents were physically in the current configuration file being read. 
&space
	The &lit(verbose) statement changes the process's verbose level to the indicated value
	for the remainer of the processing of the configuration file. When the configuration 
	file processing is completed, the verbose level is returned to the value that was 
	set before the verbose command was encountered.  This should not be confused with 
	setting a test's verbose level using the verbose keyword as a part of a test definition.

&space
&subcat	Sample Configuration File
	The following is a sample configuration file. 
&litblkb

# ----------- digests ----------------------
# admin digest delivred every 30 min after spyglass starts
# daily digest delivered just past midnight each day
#
digest	admin cluster-admin      00:30
digest  daily daily-users        24:00 00:01

# ------------ mailing list stuff --------------------------------
domain	research.att.com
nickname daily-users	&NG_SPYG_DAILYDIGEST

nickname flock-admin	&NG_SPYG_NN_FLOCKADMIN
nickname cluster-admin	&NG_SPYG_NN_ADMIN


# ------------- test specifications ------------------------------
#
test green-light
        desc greenlight is not running
        freq 0:10
        scope all
        # no notify -- we use this to block everything if gl is down
        # wrapper -s is the safe to run check; good rc means it is safe
        command ng_wrapper -s


# squelch on core file checks is 0 on purpose
# we do not record alarm->normal state change in digest 
test core-files
        after   green-light
        desc    new core files found in NG_ROOT/site
        freq    1:05
        squelch 0
        scope   !Darwin
        logok   no 
        notify  cluster-admin
        command ng_spy_ckfage -d $NG_ROOT/site -a 3600 -n -p ".*core.*"

test net-conn
        after	green-light
        desc	cannot reach node via DNS cluster name: &NG_CLUSTER
        freq	0:10
        squelch 0:30
        notify	flock-adm
        when	stable
        command	ng_spy_cknet netif

test flist-test		# reports on old filelists in repmgr nodes directory
        after	green-light
        desc	one or more flist files may be old 
        when	stable no-network
        freq	0:30
        squelch	reset
        scope	Filer
        notify  cluster-admin
        command ng_spy_ckfage -d $NG_ROOT/site/rm/nodes -a 1800 -o -p ".*"

test narbalek
        after	green-light
        desc	narbalek is not up or responding
        when	stable no-network
        freq	0:30
        squelch	0:45
        scope	all
        # prevent false alarms; alert only after two additional attempts 60s apart
        retry   2 60
        notify  cluster-admin
        command ng_spy_ckfage -d $NG_ROOT/site/rm/nodes -a 1800 -o -p ".*"
&litblke


&subcat Realtime Control
	&This accepts TCP/IP sessions through which commands may be sent to control
	the behaviour in realtime. Commands are sent via the &lit(ng_sgreq) command 
	tool. The following are the commands that are recognised (as should be 
	supplied on the &lit(ng_sgreq) commandline):
&space
&lgterms
&term	delete {test|digest} name  
	Causes the named test, or digest to be deleted. 
	If a digest contains messages, the messages will be delivered before the 
	digest is delted. 
&space
&term	explain : Causes information about all tests and digests to be returned to the 
	requestor.
&space
&term	explain digest name : Causes information about the digest to be returned to 
	the requestor.
&spcae
&term	explain name : Causes information about the named test to be returned to the 
	requestor. When the test's verbose setting is on, more information about the 
	test is given than when it is off. 
&space
&item	parse file : Causes the config file named by &ital(file) to be parsed. If a 
	test name in &ital(file) duplicates an already defined test, the old test will
	be replaced.  The same replacement holds for digests, and nicknames.
	(Applications running on a Ningaui cluster can used the &lit(start) target of 
	an &lit(ng_rota) control file to insert application specific probes intto 
	spyglass at node start time. 
&space
&term	pause n : Causes &this to pause the execution of probes for &lit(n) seconds. 
	Status messages from currently executing probes are still accepted and if they 
	indicate an alarm the notification(s) are still sent. 
&space
&term	State n : Sets the current system/environment state to &lit(n).  This affects
	the probes that are executed based on the &lit(when) statement coded for the test.
&space
&term	squelch word : Allows the frequency of alarms to be controlled.  The next alarm 
	notification will be sent based on the value of &lit(word) which can be one of the 
	following:
&space
&begterms
&term	on : Alarms are turned on, the next non-normal state will produce an alarm message
	and alarms will then be squelched based on the squelch time set for the test in 
	the configuration file.
&space
&term	off : Alarms are squelched indefinately; no alarms for the test will be sent.
&space
&term	reset : Alarms are squelched until the test returns a normal state. After that alarms
		will be sent with the indicated frequency.
&space
&term	change : Alarms for the test will be squelched until the alarm key for the probe changes.  
	The alarm key is a string that the probe command may return to &this  &this via &lit(ng_spy_wrapper.)
	The key allows &this to determine if there has been a change to the state for a test
	that is already in alarm status allowing another alert message to be sent. 
	Generally this allows probes to drive the alert mechanism should the condition become 
	worse without waiting for the squelch timer to expire or the state to first return
	to normal. 
&space
	Please see the manual page for &lit(ng_spy_wrapper) if you are writing a probe 
	and need information on how to supply a change key to &this.
&space
&term	hh:mm : Alarms will be squelched until  the specified number of hours and minutes 
	have passed, or until the state of the probe has been reset to normal.
&space
&term	n : Alarms will be squelched until  &lit(n) seconds from the current time or until 
	the state of the probe returns to normal.  
&endterms
&space
&term	verbose n : Changes the verbose level to &lit(n). 
&term	verbose test-name {on|off} : Turns verbose mode for a test on or off.  When a test's 
	verbose setting is on, more information about the test, and each probe generated,
	is written to the daemon's standard error log.  In addition, when an explain 
	command for the test is given more information about the test is generated. 
&endterms
&space
	These commands are presented above as they would be entered on the command line when 
	using &lit(ng_sgreq).  The actual comand syntax that must be sent in the buffer
	is &lit(command-token):&lit(data).  Command buffers must be newline terminated. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c file : Inidicates the name of the configuration file that defines mailing 
	list information, digest information and test information.  If not supplied, 
	NG_SPYG_CONFIG is assumed to be set in pinkpages.  The value supplied with the 
	-c option, or via the &lit(NG_SPY_CONFIG) variable, can be one or more 
	filenames.  If more than one file name is supplied they should be space or 
	comma seperated and will be read in the order that they appear in the list. 
	If more than one test have the same name, the last one read will be used. 
	If this option is not supplied, and &lit(NG_SPY_CONFIG) is not defined, 
	the file &lit(spyglass.cfg) in &lit($NG_ROOT/lib) will be used. 
&space
&term	-n : No mail mode.  No email is sent. 
&space
&term	-p port : The TCP port that is opened for communication. If not set, the pinkpages
	variable NG_SPYG_PORT is assumed to be set. 
&space
&term	-v : Verbose mode. 
&endterms

&space
&exit	An exit code of 0 indicates &this terminated normally.  Any other code indicates 
	error. 

&space
&see	&seelink(spyglass:ng_sgreq) 
	&seelink(spyblass:ng_spy_wrapper)
	&seelink(spyglass:ng_spy_ckfsfull) 
	&seelink(spyglass:ng_spy_ckdaemon) 
	&seelink(spyglass:ng_spy_ckfage)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Jun 2006 (sd) : Its beginning of time. 
&term	20 Jul 2006 (sd) : Fixed problem when a state > max-state was received. 
		Added buffer overrun checking to expand variable.
&term	21 Jul 2006 (sd) : Added delete test/digest and ability to parse a 
		config file on the fly.
&term 	03 Aug 2006 (sd) : Added more info to explain messages, squelch command 
		to allow alarms squelch for a test to be manually squelched. 

&term	09 Aug 2006 (sd) : Fixed issue with not blocking dependant test while the test
		itslf is pending. Fixed issue with not actually squelcing until reset
		if flag set in config file. 
&term	14 Aug 2006 (sd) : Added ability to specify delay between retries on the retry
		statement in config file. Properly unlinks test chains before reparse
		of the config file rather than after. (v1.3/a)
&term	15 Aug 2006 (sd) : Retry value is now the actual retries that the user wants and 
		thus the user does not need to include the inital test in the count. 
&term	16 Aug 2006 (sd) : Fixed issue with retry freq.  fixed memory leak in ok4node().
&term	17 Aug 2006 (sd) : Fixed problem causing core dump in retry parse.
&term	19 Aug 2006 (sd) : Corrected fail/odd percentage so as not to pick up last one if probe is disabled.
&term	21 Aug 2006 (sd) : Added support for squelch until key change. 
&term	22 Aug 2006 (sd) : Fixed man page. 
&term	31 Aug 2006 (sd) : Added extra info into alarm header, fixed coredump when squelch command had less than 
		required number of tokens.  Handles the case where NG_SPYG_CONFIG is not defined (now defaults).
&term	06 Sep 2006 (sd) : Fixed cause of coredump; memory freed, but pointer was not nulled. 
&term	17 Nov 2006 (sd) : Converted sfopen() to ng_sfopen(). 
&term	02 Apr 2007 (sd) : Fixed memory leak in new_test().
&term	20 Mar 2008 (sd) : Added ability to not lot alarm to normal status in digests. 
&term	08 Apr 2008 (sd) : Tweek to properly truncate trailing whitespace from input buffers. 
&term	22 Sep 2008 (sd) : Corrected parsing of config to recognise comments that start at the beginning of the line. 
&term	01 Oct 2008 (sd) : Corrected problems with parsing config if test description lines are not started with a tab.
&term	10 Oct 2008 (sd) : A return to normal state is now considered a change and will turn off a probe's squelch state. 
&term	30 Oct 2008 (sd) : Added include for ng_socket.h to this file so that new def in socket.h does not bugger the compile. 
&term	12 Dec 2008 (sd) : Completely revampped the squelch logic. Also added concept of verbose at the test level and 
		added support for turning on/off using ng_sgreq. (v1.6)
&term	09 Jan 2009 (sd) : Corrected cause of protection exception in spy_netif (v1.7).
&term	11 Mar 2009 (sd) : Added even more protection against probes that have wedged. Now it retries the probe once, 
		and if there is a slow response the second time it blocks the probe and bleats to stderr. (v1.8)
&term	04 Jun 2009 (sd) : Corrected end flag of scd section.
&scd_end
------------------------------------------------------------------ */
