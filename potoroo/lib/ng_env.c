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
 Mnemonc:	ng_env
 Abstract: 	Fetches things from the cartulary resolving other
		references to cartulary things or environmental things.
		Complete revision of the original to improve performance.
 Returns:	A pointer to a null terminated string; caller expected to 
		free the associated storage.
 Date:		2 May 2001 
 Author: 	E. Scott Daniels
 Assumptions: 	Assumes that NG_ROOT (or what ever constant it is compiled
		with) contains the cartulary.
 ----------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_ext.h>
#include	<ng_lib.h>

#define	NAME		0	/* get the name from the buffer */
#define	VALUE		1	/* get the value from the buffer */

static time_t mtime = 0;	  /* last timestamp of the cartulary */
static Symtab *st = NULL; 	  /* symbol table where things are mapped */

/* called via symtraverse to delete all existing nodes */
static void launder( Syment *this, void *np )
{
	char *name;
	void *val;

	if( ! this )			/* we call it from outside of traverse too */
		return;
	
	name = this->name;
	val = this->value;

	symdel( st, name, 0 );

	ng_free( val );
	ng_free( name );

}

/* returns pointer to data in symtab or from env -- both should be treated as read only  buffers */
static char *lookup( char *what )
{
	char *stuff = NULL;
	Syment *se;

	if( st && (se = symlook( st, what, 0, NULL, SYM_NOFLAGS )) )	
		stuff = (char *) se->value;
	else
		stuff = getenv( what );			/* try env if not in cartulary */

	return stuff;
}

#ifdef OLD_WAY
deprecated 12/03/2007 when it was deemed that we will look up any variable in the cartulary first and 
then in the environment if it is not in the cartulary. if user wants this behavioour they can call env()
first and ng_env if env() returns null;

/* returns pointer to data in symtab or from env -- both should be treated as read only  buffers */
static char *lookup( char *what )
{
	char *stuff = NULL;
	Syment *se;

	if( strncmp( what, "NG_", 3 ) == 0 )		/* environment overrides cartulary if there */
	{
		if( (stuff = getenv( what )) == NULL )			/* puck from environment first */
			if( st && (se = symlook( st, what, 0, NULL, SYM_NOFLAGS )) )	/* try cartulary if ! env */
				stuff = (char *) se->value;
	}
	else		/* cartulary overrides the environment for all others */
	{
		if( st && (se = symlook( st, what, 0, NULL, SYM_NOFLAGS )) )	
			stuff = (char *) se->value;
		else
			stuff = getenv( what );			/* try env if not in cartulary */
	}

	return stuff;
}
#endif

static int need2load( char *cartf )
{
	struct	stat s;
	time_t	this_check = 0;

	if( stat( cartf, &s ) >= 0 )
		this_check = s.st_mtime;	/* time last modified */
	else
	{
		sleep( 1 );				/* might be that cartulary is in update -- pause a second */
		if( stat( cartf, &s ) >= 0 )
			this_check = s.st_mtime;	/* time last modified */
		else
		{
			sfprintf( sfstderr, "ng_env: cant stat: %s: %s\n", cartf, strerror( errno ) );
			return 0;
		}
	}

	if( !st || (this_check != mtime) )		/* the file has a different modification time than our last look */
	{
		mtime = this_check;

		if( ! st )
			st = syminit( 479 );		/* establish the symbol table */
		else
			symtraverse( st, 0, launder, NULL );		/* clean the table before reload */
		return 1;
	}
	
	return 0;		/* what is loaded will sufice */
}

/* trash comments (all characters after hash) return offset of end of string (0 if empty string left!) */
static int strip( char *buf )
{
	char	*i;

	if( (i = strchr( buf, '#' )) )
	{
		while( i != buf  &&  isspace( *(i-1) ) )    /* trash blanks preceding the # */
			i--;
		*i = 0;
		return i - buf; 	/* return offset of new end (false if empty string!) */
	}
	else
		return strchr( buf, 0 ) - buf;

}

static void getstuff( char *src, char *dest, int what )
{
	char *sep;		/* pointer at seperator (=) in the "equasion" */

	if( strncmp( src, "export", 6 ) == 0 )
		for( src += 6; *src && isspace( *src ); src++ );  /* skip whitespace after export */

	if( (sep = strchr( src, '=' )) )
	{
		if( what == NAME )
		{
			strncpy( dest, src, sep - src );
			dest[sep-src] = 0;
		}
		else
			strcpy( dest, sep + 1 );
	}
	else
		*dest = 0;		/* who knows what it is, but we cant tell */

	
}

/* expand the variable references in caller's buffer cb */
static char *expand( char *cb )
{
	char	*eb = NULL;	/* buffer expanded in */
	int	ebi = 0;	/* index into expansion */
	int	neb = 0;	/* number bytes allocated to eb */
	char	*src;		/* pointer to source for copies */
	char	*cbi;
	char	vname[256];	/* variable name to look up */
	char 	defstr[NG_BUFFER];	/* default (${name:-default}) */
	int	i;
	int 	skip;		/* flag to indicate ${...} format and to skip to closing } */
	int 	doit = 1;	/* flag: if !0 then we expand variables; turned off if in ' */ 
	
	cbi = cb;

	while( *cbi )
	{
		if( ebi > neb - 10 )	/* need more room */
		{
			neb += 1024;
			if( neb >= NG_BUFFER )			/* catch recursive expansion and blow off */
			{
				ng_bleat( 0, "ng_env: error expanding %s: resulting value too large or is recursive", vname );
				eb[ebi] = 0;
				return eb;
			}
			eb = ng_realloc( eb, neb, "ng_env: expansion buffer (1)" );
		}

		skip = 0;

		switch( *cbi )
		{
			case '\'':	
				doit = !doit; 	/* turn on/off var expansion depending on start/end quote */
				cbi++;
				break;

			case '$':
				if( doit )
				{
					if( *(cbi+1) == '{' )
					{
						skip = 1;
						cbi++;
					}
					i = 0;
					for( cbi++; *cbi  && (isalnum( *cbi ) || *cbi == '_'); cbi++ )		/* snag the variable name */
						vname[i++] = *cbi;
					vname[i] = 0;

					*defstr = 0;
					if( skip )					/* if we found ${ check for :- and get defstr if it is */
					{
						if( *cbi == ':' )
						{
							if( *(++cbi) == '-' )	/* ${name:-default} */
							{
								cbi++;
								for( i = 0; *cbi && *cbi != '}'; i++ )
									defstr[i] = *cbi++;
								defstr[i] = 0;
							}
							else				/* we support only :- so skip anything else */
								for( ; *cbi && *cbi != '}'; cbi++ );

						}

						if( *cbi != '}' )			/* syntax error */
							ng_bleat( 0, "ng_env: syntax error expanding value for %s: expected }", vname );	
						else
							cbi++;			/* skip closing } */
					}
	
					if( !(src = lookup( vname )) )
						src = defstr;			/* use default string if supplied */
	
					if( ebi > neb - strlen( src ) )		/* need more room */
					{
						int m = strlen( src );

						neb += m > 1024 ? m+100 : 1024;
						if( neb >= NG_BUFFER )			/* catch recursive expansion and blow off */
						{
							ng_bleat( 0, "ng_env: error expanding %s: resulting value too large or is recursive", vname );
							eb[ebi] = 0;
							return eb;
						}
						eb = ng_realloc( eb, neb, "ng_env: expansion buffer (2)" );
					}

					while( *src )
						eb[ebi++] = *src++;	/* copy in the expansion */
				}
				else
					eb[ebi++] = *cbi++;

				break;

			case '"':	cbi++; break;
			case '\\':	eb[ebi++] = *cbi++; 	/* save it, and fall into saving next one too */
			default:	eb[ebi++] = *cbi++; break;
		}		
		
		eb[ebi] = 0;
	}

	return eb;
}

/* fetch the cartulary if we have not yet, or if it has changed */
static void load( char *cartf )
{

	char	*buf;			/* points at input buffer */
	char 	name[1024];		/* variable name */
	char 	val[1024];		/* what it was set to in cartulary */
	char 	*exval;
	Sfio_t	*f = NULL;

	if( (f = ng_sfopen( NULL, cartf, "r" )) == NULL )
		sleep( 1 );					/* pause, cartulary update might be in progress */
	
	if( f || (f = ng_sfopen( NULL, cartf, "r" )) )		/* opened earlier, or open on retry */
	{
		while( (buf = sfgetr( f, '\n', SF_STRING )) )
		{
			while( isspace( *buf ) )	/* skip leading spaces */
				buf++;

			if( strip( buf ) )		/* something other than comments */
			{
				getstuff( buf, name, NAME );
				getstuff( buf, val, VALUE );
				exval = expand( val );							/* expand variables */
				launder( symlook( st, name, 0, 0, SYM_NOFLAGS ), 0 );		/* trash if it existed */
				symlook( st, ng_strdup( name ), 0, exval, SYM_NOFLAGS );
			}
		}

		if( sfclose( f ) )
		{
			ng_bleat( 0, "ng_env/load: error during read of %s: %s", cartf, strerror( errno ) );
		}
	}
	else
		sfprintf( sfstderr, "ng_env: cannot open %s: %s\n", cartf, strerror( errno ) );
}

/* return a constant pointer - user should not free */
const char *ng_env_c( char *cname )
{
	static char	*cartf =  NULL; 
	char	cfname[255];			/* space if we build the name from root/cartulary */
	char	*ecf;				/* file name for cartulary in environment */
	char	*cbuf;
	/*static	char ppbuf[NG_BUFFER]; */

	if( cartf == NULL )
	{
		if( (ecf = getenv( "NG_CARTULARY" )) )	/* allow user to override */
			cartf = ecf;
		else
		{
			if( (ecf = getenv( "NG_ROOT" )) )
			{
				sfsprintf( cfname, sizeof( cfname ), "%s/cartulary", ecf );
				cartf = ng_strdup( cfname );
			}
			else
				cartf = "/ningaui/cartulary";	/* default if not overridden in environment with NG_CARTULARY, or $NG_ROOT not defined  */
		}
	}

	if( need2load( cartf ) )
		load( cartf );				/* ensure its loaded/reload if changed */ 

	if( (cbuf = lookup( cname )) )		/* constant buffer in symtab/env */
		return cbuf;

#ifdef USE_PINKPAGES
	if( (cbuf = ng_ppget( cname )) != NULL  )
	{
		sfsprintf( ppbuf, sizeof( ppbuf ), "%s", cbuf );

		ng_free( cbuf );		/* pink pages dups the buffer coming back */
		return ppbuf;
	}
#endif

	return NULL;
	
}

char *ng_env( char *cname )
{
	const char *stuff = NULL;

	stuff = ng_env_c( cname );		/* if not here, may try pinkpages */
	return stuff ? ng_strdup( (char *) stuff ) : NULL;
}

/* define made on mkfile commmand line to build ng_env */
/* dumps values for the cartulary vars entered on the command line */
#ifdef SELF_TEST
main( int argc, char **argv )
{
	const char *p = NULL;
	int long_test = 0;		/* simulate a daemon to verify that we see cartulary changes */
	Sfio_t *f;
	extern int verbose;

	verbose = 3;

	if( strcmp( *(argv), "long" ) == 0 )
	{
		printf( "start repleated check of variable delay=60s\n" );
		++argv;
		do
		{
			p = ng_env_c( *argv );
			printf( "%s = (%s)\n", *argv, p ? p : "undefined?" );
			sleep( 60 );
		}
		while(  1 );
	}
	else
	{
		while( *(++argv) )
		{
			p = ng_env_c( *argv );
			printf( "%s = (%s)\n", *argv, p ? p : "undefined?" );
		}
	}
}

#endif

#ifdef KEEP
/*
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_env,ng_env_c:Get Information From Ningaui Cartulary)

&space
&synop	#inculde <ningaui.h>
&break	char *ng_env( char *name );
&break	const char *ng_env_c( char *name );

&space
.dv this ^&ital(ng_env) and ^&ital(ng_env_c)
.dv This ^&ital(Ng_env) and ^&ital(ng_env_c)
&desc	&This search the &n cartulary and environment for &ital(name) and return a pointer 
	to a string containing the value that was assigned to &ital(name), or a NULL pointer if 
	&ital(name) was not defined in the either the cartulary or the environment.
	The filename used to locate the cartulary
	is expected to be defined in the environment variable NG_CARTULARY.
	Should NG_CARTULARY not be defined, then the function assumes
	&lit($NG_ROOT/cartulary) as the cartulary's file name.

&space
&subcat	Search Order
	The cartulary is always searched first. This allows long running applications
	to continue to be able to fetch current values that have been updated in the 
	cartulary rather than reusing static environment variables set at the time 
	the process was started.  
&space
&subcat	Ng_env_c
	The only difference between &this is that &ital(ng_env_c) returns
	a pointer to a constant string that the caller is not expected 
	to free when it is no longer needed.  
	
&space
&subcat	Cartulary Format
	The cartulary is expected to contain shell variable
	assignment statements, blank lines, and comments.  The function 
	assumes that only one assignment per line is made, and allows
	the Korn shell keyword export to appear in front of the 
	variable name.  The function assumes that the value begins
	with the character immediatly following the equal sign (=)
	and continues to the first white space character.  Values
	may be quoted with single or double quote marks. If the 
	value is quoted then all of the tokens contained within 
	the quote markds are returned as they appear in the file.
	Lines within the cartulary may contain up to 1023 
	characters; &this does not allow lines to 
	be continued using the escape (\) character. 
	The following illustrate valid entries in the cartulary.
&space
&litblkb
        # Ningaui Cartulary
	export NG_ROOT=${NG_ROOT:-/ningaui}
	export TCM_READY=444
	export TCM_STALE=111
	export TCM_INUSE=660
&litblke
&space
&subcat	Variable Expsnaion 
	Simple shell variables (&lit($xxxx) or &lit(${xxxxx}) 
	encountered in the RHS of a cartulary entry
	will be expanded. The expansion assumes that
	the variable has been previously defined in the cartulary or 
	is a shell variable that can be found in the environment. 
	The same search order naming rules apply to expanding variables
	that are a part of the expression.
	If the variable cannot be found, then the shell method of 
	"empty substitution" will be used and the variable will be
	removed from the value string.  If the shell variable symbol
	($) is needed within a value then it should be escaped using the 
	backslash (\) character. Variable
	symbols inside of single quote marks (e.g. '$SCOOTER') 
	are not expanded.  
&space
&subcat	Complex Shell Variable Expansion
	Unset, default, variable expansion is also implemented as a 
	part of &this. Variables that have the syntax of
	&lit(${name:-string}) will be expanded to the value of 
	the variable &lit(name). If the variable is not set, then 
	the &lit(string) is used as though the &lit(string) had 
	been defined as the value for &lit(name).  Other 
	complex variable expansion allowed by the Korn shell
	are ignored and if the variable name is not defined they 
	result into a nil expansion. (i.e. ${name:+word} expands 
	only if &lit(name) is defined). No attempt has been made
	to even begin to support the &lit(${name%%pattern}) expansion 
	syntax family, and if included in the cartulary will likely produce
	results that are completely undesired.

&space
&parms
&begterms
&term	name : A pointer to a null terminated character string that 
	contains the name of the cartulary file variable to search
	for.
&endterms

&space
&return	The function will return a pointer to a null terminated string 
	containing the string assigned to the name in the cartulary 
	file. If the function is unable to find &ital(name) in the 
	cartulary file, then a NULL pointer is returned. 


&space
&files 	/ningaui/cartulary


&space
&mods
&owner(Scott Daniels)
&lgterms
&term   03 May 2001 (sd) : Original code; complete rewrite of gk_env to 
	make use of symbol table for speed. Added the &lit(ng_env_c)
	function.
&term	02 Oct 2001 (sd) : Modified to search environment first only if
	the var name starts with NG_.
&term	31 July 2002 (sd) : Added support for pink pages. 
&term	14 Mar 2003 (sd) : corrected a memory leak if var found in pinkpages.
&term	15 Mar 2003 (sd) : removed call to pinkpages
&term	02 Mar 2004 (sd) : Added second attempt to open cartulary file. 
&term	31 Jul 2005 (sd) : Added changes to prevent gcc warnings.
&term	01 Dec 2005 (sd) : There was no bug when expanding ${TMP:-default} string. The 
		problem is that we add trailing spaces/tabs when we read the cartulary
		and so it was looking like we were adding blanks when TMP was not defined.
&term	09 Oct 2006 (sd) : Fixed bug when expanding ${v:-foo} strings. The var name was not
		being properly terminated and could cause issues. 
&term	13 Nov 2006 (sd) : Converted to ng_sfopen() and to check close status now that it is meaningful.
&term	03 Dec 2007 (sd) : Ng_env now always looks in the cartulary first and then in the environment
	if the requested value is not in the cartulary. 
&term	06 Jan 2009 (sd) : Corrected bug that would cause an issue if "${VNAME\n"  were encountered.
		Added a recursion stop which should prevent a coredump if we encounter a loop in expansion.
&term	17 Jun 2009 (sd) : Correction to prevent small memory leak in ng_env_c(). 
&endterms
&scd_end
*/
#endif
