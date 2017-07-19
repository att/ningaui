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
# Mnemonic:	ng_nar_if	 - narbalek interface
# Abstract:	provides a C interface to get/put narbalek variables
#		and to pinkpages variables.
#		
# Date:		06 January 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include 	<errno.h>

#include	"sfio.h"

#include	"ningaui.h"
#include	"ng_lib.h"


static char	*nar_if_version = "narif v2.4/08018";
static int	nar_fd = -1;		/* file des we have open with narbalek */
static char	*me = NULL;		/* my hostname */
static char	*cluster = NULL;	/* my cluster name */
static int 	timeout = 2;		/* wait for response timeout */
static int	set_retries = 2;	/* number of times to retry after a nar-set failure */
static int	get_retries = 2;	/* num retries after nar_get failure */
static int	nar_cache_size = 4096;	/* default size, can be changed with call to nar_cache_setsize() */

int ng_nar_namespace( char **utable, int unum, char *pattern, char skipto );



typedef struct nar_cache {
	Symtab 	*table; 
	char	*pat;
	char	**list;
	int	size;
	int	refresh;
	ng_timetype expiry;
} Nar_cache_t;


/* --------------- caching routines ------------------- */
void ng_nar_cache_setsize( int size )
{
	nar_cache_size = size;		/* applies to all future creations */
}

/* returns 0 if load failed */
int ng_nar_cache_reload( void *vcp )
{
	Nar_cache_t *cp;
	int	fscount;
	int 	j;
	int	i;
	char	*tok;		/* pointer at value */

	cp = (Nar_cache_t *) vcp;

	ng_bleat( 4, "nar_cache_reload: reload for pat: %s", cp->pat );
	symclear( cp->table );					/* purge the table */
	for( i = 0; i < cp->size && cp->list[i]; i++ )
	{
		ng_free( cp->list[i] );
		cp->list[i] = NULL;
	}

	fscount = ng_nar_namespace( cp->list, cp->size-1, cp->pat, '=' );	/* the = indicates the list should be name=value and not just name */
	cp->expiry = ng_now( ) + cp->refresh;		/* assume a successful load */
	if( fscount < 0 )
	{
		ng_bleat( 2, "nar_if/cache_reload: unable to fill cache from narbalek; pausing 1s before retry" );
		fscount = ng_nar_namespace( cp->list, cp->size-1, cp->pat, '=' );	/* the = indicates the list should be name=value and not just name */
		if( fscount < 0 )
		{
			fscount = 0;
			ng_bleat( 1, "nar_if/cache_reload: unable to fill cache from narbalek on retry; table is empty" );
			cp->expiry = 0;			/* force an immediate retry */
		}
	}
	cp->list[fscount] = NULL;

	for( i = 0; i < fscount && cp->list[i]; i++ )
	{
		if( (tok = strchr( cp->list[i], '=' )) )
		{
			*tok = 0;
			tok++;
			if( *tok == '"' )
			{
				tok++;
				for( j = 1; tok[j] && tok[j] != '"'; j++ )
					if( tok[j] == '\\' && tok[j+1]  )
						j++;
				tok[j] = 0;
			}
			symlook( cp->table, cp->list[i], 0, tok, SYM_NOFLAGS );
		}
	}

	return fscount > 0 ? 1 : 0;
}

void *ng_nar_cache_new( char *pat, int refresh )
{
	Nar_cache_t *cp;

	cp = (Nar_cache_t *) ng_malloc( sizeof( Nar_cache_t ), "nar cache" );
	cp->table = syminit( 4997 );
	cp->pat = ng_strdup( pat );
	cp->size = nar_cache_size;
	cp->refresh = refresh;
	if( refresh )
		cp->expiry = ng_now( ) + refresh;		/* expires based on user request */
	else
		cp->expiry = ng_now( ) + 1200;			/* fetch new every two min */

	cp->list = (char **) ng_malloc( sizeof( char *) * cp->size, "fslist" );
	memset( cp->list, 0, sizeof( char *) * cp->size );
	ng_nar_cache_reload( cp );

	return (void *) cp;
}

void ng_nar_cache_close( void *vcp )
{
	Nar_cache_t *cp;
	int	i;

	cp = (Nar_cache_t *) vcp;

	symclear( cp->table );
	for( i = 0; i < cp->size && cp->list[i]; i++ )
	{
		ng_free( cp->list[i] );
		cp->list[i] = NULL;
	}

	ng_free( cp->list );
	ng_free( cp->pat );
	ng_free( cp->table );
	ng_free( cp );
}

char *ng_nar_cache_get( void *vcp, char *vname )
{
	Nar_cache_t *cp;
	Syment 	*se;
	char	*val = NULL;

	cp = (Nar_cache_t *) vcp;
	if( cp->expiry < ng_now( ) )
		ng_nar_cache_reload( cp );

	if( (se = symlook( cp->table, vname, 0, 0, SYM_NOFLAGS )) != NULL )
		val = ng_strdup( (char *) se->value );
	

	return val;
}


/* --------------------------- */



/* find an alternate node with a running narbalek -- used if ours does not answer */
static char *alt_node( )
{
	char	*buf;
	char	*rb = NULL;		/* pointer to buffer to return */
	Sfio_t	*f;

	if( (f = sfpopen( NULL, "ng_nar_get -b bootstrap_node", "r" )) != NULL )
	{
		buf = sfgetr( f, '\n', SF_STRING );
		if( buf )
		{
			ng_bleat( 2, "nar_if: alternate node: (%s)", buf );
			rb = ng_strdup( buf );		/* must dup before close */
		}

		sfclose( f );
	}

	if( rb && ! *rb )			/* we could get an empty string back */
	{
		ng_free( rb );
		return NULL;
	}

	return rb;
}

/* simple wait for the narbalek end string (#end#). we boldly assume that it will not be split 
   and so other than from the nar_set() funciton, this function probably should not be used.
*/

static void wait4end( char *mode )
{
	char	buf[NG_BUFFER];
	int	n = 0;
	int	empty_count = 0;
	int	tcount = 0;		/* things tossed out */

	if( nar_fd < 0 )
		return;

	while( n >= 0 && ng_testport( nar_fd, timeout ) > 0 )
	{
		if( (n = recvfrom( nar_fd, buf, sizeof( buf ) -1, 0, NULL, NULL )) > 0 )
		{
			buf[n] = 0;
			if( strstr( buf, "#end#\n" ) )
				return;

			tcount++;
		}
		else
		{
			if( empty_count++ > 25 ) 		/* we should not get empty buffers from narbalek; bail if we do */
			{
				ng_bleat( 0, "nar_if: wait4end: warning: too many empty buffers received from narbalek; resetting connection" );
				n = -2;
			}
		}
	}

	ng_bleat( 0, "nar_if: error or timeout waiting for end/ack msg from narbalek: %s mode=%s tcount=%d", errno ? strerror( errno ) :  "timeout", mode, tcount );
	close( nar_fd );
	nar_fd = -1;
}

/* allow user to control the number of retries */
void ng_nar_retries( int s, int g )
{
	if( s >= 0 )
		set_retries = s;
	if( g >= 0 )
		get_retries = g;
}

/* allow user to set the timeout value */
void ng_nar_to( int to )
{
	if( to > 0 )	
		timeout = to;
}

	/* auto called by get/put, but user may call directly to specify a port other than NG_NAR*_PORT.
		if user calls with a specific port, that port will be used if the session is
		is dropped and get/put reinvokes this function to re-establish the session (get/put call with
		a NULL port string to force the pp variable to be used.
	*/
int ng_nar_open( const char *port )
{
	static char	*rport = NULL;		/* we save the port we end up using in case we are called to reconnect with NULL */
	char	*cname = NULL;		/* cluster name -- if open on local host fails we try here */
	int	tries = 50;		/* number of times we will try to connect */
	

	if( ! port )
	{
		if( rport )
			port = rport;		/* use port we saved on the very first call (port from command line) */
		else
		{
			if( (port = ng_env_c( "NG_NARBALEK_PORT" )) == NULL )
			{
				ng_bleat( 0, "nar_open: cannot find NG_NARBALEK_PORT" );
				return -1;
			}

			rport = ng_strdup( (char *) port );
		}
	}
	else
	{
		if( rport )			/* save the port string so caller can invoke with NULL to use the last one */
			ng_free( rport );
		rport = ng_strdup( (char *) port );	/* if sess drops and we are called with null by read, use what we originally got */
	}

	while( nar_fd < 0 )
	{
		ng_bleat( 3, "nar_open: opening session with narbalek using port: %s", port );
		if( ng_rx( "localhost", (char *) port, &nar_fd, NULL ) < 0 )		/* we have both in and out on the same fd */
		{
			if( (cname = alt_node( )) != NULL ) 				/* try an alternate node's narbalek */
			{
				ng_bleat( 3, "nar_open: cannot start session with narbalek (localhost:%s); trying alternate node: %s", port, cname );

				if( *cname == 0 || ng_rx( cname, (char *) port, &nar_fd, NULL ) < 0 )
					ng_bleat( 0, "nar_open: unable to start session with narbalek on localhost or alt(%s): will %s", cname ? cname : "no-alt)", tries > 0 ? "retry in a few seconds" : "not retry" );

				ng_free( cname );
				cname = NULL;
			}
			else
				ng_bleat( 0, "nar_open: unable to start session with narbalek on localhost (no alt found): will %s", tries > 0 ? "retry in a few seconds" : "not retry" );
		}

		if( nar_fd >= 0 )
			ng_bleat( 2, "nar_open: session established with narbalek: %s:%s", cname ? cname : "localhost", port );
		else
		{
			if( --tries <= 0 )
			{
				if( port && strtol( port, 0, 10 ) == 0 )		/* port buffer exists, but is not numeric -- corruption somewhere */
				{
					ng_bleat( 0, "nar_open: abort: memory corruption detected; port(%s) and/or rport(%s) are not sane ", port, rport );
					abort( );
				}
				ng_bleat( 0, "nar_open: unable to connect to narbalek; giving up!" );
				return -1;
			}

			sleep ( 10 );			/* pause before retry */
		}
	}

	return nar_fd; 		/* in case user wants to do direct i/o with narbalek */
}

void ng_nar_close( )
{
	if( nar_fd >= 0 )
		close( nar_fd );
	nar_fd = -1;
}

/* flush anything that is stacked for read */
void ng_nar_flush( )
{
	char buf[NG_BUFFER]; 
	int len;

	if( nar_fd < 0 )
		return;

	buf[0] = 0;
	while( ng_testport( nar_fd, 0 ) >0 )
	{
		if( (len=read( nar_fd, buf, sizeof( buf ) ))  <= 0 )		/* seems we get >0 from testport if the session disconns  */
		{
			ng_bleat( 3, "nar_flush: narbalek session dropped" );
			close( nar_fd );			/* assume error, prevent getting the wrong value next time */
			nar_fd = -1;
			return;
		}
	}
}

	
/* does the real work -- called by ng_nar_get which manages the retries */
static char *XXnar_get( char *vname )
{
	char	buf[NG_BUFFER];
	char	*s;
	char	*e;
	int	n;
	int	found_end = 0;

	if( !vname )
		return NULL;

	ng_nar_flush( );		/* clear anything that wandered in after last call -- yes we do before open check */

	if( nar_fd < 0 )
		if( ng_nar_open( NULL ) < 0 )	
			return NULL;

	sfsprintf( buf, sizeof( buf ), "Get %s\n", vname );
	ng_bleat( 4, "nar_get: sending to %d (%s)", nar_fd, buf );


	write( nar_fd, buf, strlen( buf ) );
	if( (n = ng_testport( nar_fd, timeout )) > 0 ) 	
	{
		if( (n = read( nar_fd, buf, sizeof( buf ) -1 )) > 0 ) 
		{
			buf[n] = 0;

	ng_bleat( 2, "nar_get:  (%s)", buf );
			if( strstr( buf, "#end#\n" ) )
				found_end = 1;

			if( (s = strchr( buf, '\n' )) )			/* all buffers are newline terminated from narbalek */
				*s = 0;					/* convert to string */

			if( strlen( buf ) > n && ! found_end )
				ng_bleat( 1, "nar_get: extra characters in buffer, but not complete #end# (%s)", s+1 );
			s = buf;

			if( ! found_end )
				wait4end(  "get" );

			if( strcmp( s, "#end#" ) == 0  || strcmp( s, "#ERROR#" ) == 0 )		/* no value, just the end of data or error marker */
				return NULL;

			return ng_strdup( s );
		}
		else
		{
			ng_bleat( 2, "nar_get: session dropped/read error" );
			close( nar_fd );			/* assume error, prevent getting the wrong value next time */
			nar_fd = -1;
		}
		
	}
	else
	{
		ng_bleat( 2, "nar_get: error while testing socket" );
		close( nar_fd );			/* assume error, prevent getting the wrong value next time */
		nar_fd = -1;
	}

	return NULL;
}
/* does the real work -- called by ng_nar_get which manages the retries */
static char *nar_get( char *vname )
{
	static	void *flow = NULL;		/* handle for flow data */
	char	buf[NG_BUFFER];
	char	*rec;				/* record contained in buf */
	char	*value = NULL;			/* value returned by narbalek */
	int	n;
	int	ecount = 0;			/* empty buffer count; prevent hang if narbalek/tcp buggers */

	if( !vname )
		return NULL;

	ng_nar_flush( );		/* clear anything that wandered in after last call -- yes we do before open check */

	if( nar_fd < 0 )
		if( ng_nar_open( NULL ) < 0 )	
			return NULL;

	if( !flow && (flow = ng_flow_open( NG_BUFFER / 2 )) == NULL )	/* we let flow manager manage multi records per message */
	{
		ng_bleat( 0, "ng_get: unable to open flow" );
		return NULL;
	}

	ng_flow_flush( flow );

	sfsprintf( buf, sizeof( buf ), "Get %s\n", vname );
	ng_bleat( 4, "nar_get: sending to %d (%s)", nar_fd, buf );

	write( nar_fd, buf, strlen( buf ) );				/* send request */

	while( ecount < 25 )
	{
		if( (n = ng_testport( nar_fd, timeout)) <= 0 )
		{
			ng_bleat( 2, "nar_get: error/timeout while testing socket (%d)", n );
			close( nar_fd );			/* assume error, prevent getting the wrong value next time */
			nar_fd = -1;
			return value;			/* we still return value if timeout waiting for end */
		}

		if( (n = read( nar_fd, buf, sizeof( buf ) -1 )) > 0 ) 
		{
			ng_flow_ref( flow, buf, n );			/* register the new buffer with flow manager */
			while( rec = ng_flow_get( flow, '\n' ) )	/* get each newline separated record in buffer */
			{
				ng_bleat( 3, "nar_get: received (%s)", rec );
				if( strcmp( rec, "#end#" ) == 0  )		/* thats it */
					return value;

				if( strcmp( rec, "#ERROR#" ) == 0 )		/* problem; send back null */
				{
					if( value )
						ng_free( value );
					return NULL;
				}
	
				if( ! value )
					value = ng_strdup( rec );		/* should not get more than one, but guard against */
			}
		}
		else
			ecount++;			/* empty buffer, count to prevent hang */
	}

	return NULL;
}

/* return the value for the named narbalek variable. If we detect a read error (fd <0 after call) then we retry */
char *ng_nar_get( char *vname )
{
	int	r = get_retries;
	char	*value = NULL;


	while( (value = nar_get( vname )) == NULL && --r > 0 )
	{
		if( nar_fd >= 0 )			/* it can be null if its not set, so if fd is legit we assume it is not set */
			return value;

		ng_bleat( 3, "ng_nar_get: failed to get value, retry in 2s: %s (%d)", vname, r );
		sleep( 2 );
	}

	return value; 
}


/* this does the real work. ng_nar_set invokes this and retries if needed */
static int nar_set( char *vname, char *value )
{
	char buf[NG_BUFFER];

	if( !vname || !value )
		return 0;

	if( nar_fd < 0 )
		if( ng_nar_open( NULL ) < 0 )
			return 0;

	sfsprintf( buf, sizeof( buf ), "set %s=%s\n", vname, value );
	if (write( nar_fd, buf, strlen( buf ) ) > 0 )
	{
		wait4end( "set" );			/* narbalek sends an end as confirmation */
		return nar_fd >= 0 ? 1 : 0;
	}

	wait4end( "set" );			
	return 0;
}

/* set the variable and return 1 if successful */
int ng_nar_set( char *vname, char *value )
{
	int	r;		/* retries if we encounter a failure */
	int	status = 0;

	r = set_retries;		

	while( (status = nar_set( vname, value )) <= 0  &&  --r > 0 )
	{
		ng_bleat( 3, "ng_nar_set: set failed, retrying in 2s: %s", vname );
		sleep( 2 );
	}

	return status;
}

static void set_names( )
{
	char *p;

	if( ! me )
	{
		me = ng_malloc( 200, "hostname buffer" );
		ng_sysname( me, 199 );
		if( (p = strchr( me, '.' )) )
			*p = 0;				/* we want foo from foo.domain.com */
	}

	if( !cluster )
	{
		if( (cluster = ng_env( "NG_CLUSTER" )) == NULL )
		{
			ng_bleat( 0, "pp_get: cannot find NG_CLUSTER in environment/cartulary" );
			cluster = "unknown";
		}
	}
}


	/* where is a return value of where (scope) we found the value */
char *ng_pp_scoped_get( char *vname, int scope, int *where )
{

	char buf[NG_BUFFER];
	char *p;

	if( ! me || ! cluster )
		set_names( );

	if( where )
		*where = scope;			/* let them know where we found it so they can change it if needed */

	switch( scope )
	{
		case PP_FLOCK:	sprintf( buf, "pinkpages/%s:%s", "flock", vname ); break;

		case PP_NODE: sprintf( buf, "pinkpages/%s:%s", me, vname ); break;

		case PP_CLUSTER: sprintf( buf, "pinkpages/%s:%s", cluster, vname ); break;

		default:
				if( (p = ng_pp_scoped_get( vname, PP_NODE, where )) )			/* search in node, clu, flock order */
					return p;
				if( (p = ng_pp_scoped_get( vname, PP_CLUSTER, where )) )
					return p;
				return ng_pp_scoped_get( vname, PP_FLOCK, where );

	}

	return ng_nar_get( buf );
}

/* get a value as it applies to a specific node -- allows ng_ppget -h name  type funcitonality from programme */
char *ng_pp_node_get( char *vname, char *node )
{

	char buf[NG_BUFFER];
	char *p;

	sprintf( buf, "pinkpages/%s:%s", node, vname );
	return ng_nar_get( buf );
}

/* try to find the value of the named var. Yes, some things depend on getting a buffer every time, even if 
   empty so we must NOT return NULL. Im not pleased with this, but c'est la vie.
*/
char *ng_pp_get( char *vname, int *where )
{
	char *p;
	if( (p = ng_pp_scoped_get( vname, PP_NCF_ORDER, where )) == NULL )
		return ng_strdup( "" );

	return p;
}

	/* set a variable in pp namespace using scope */
int ng_pp_set( char *vname, char *value, int scope )
{

	char buf[NG_BUFFER];

	if( ! me || ! cluster )
		set_names( );

	switch( scope )
	{
		case PP_FLOCK:	sprintf( buf, "pinkpages/%s:%s", "flock", vname ); break;

		case PP_NODE: sprintf( buf, "pinkpages/%s:%s", me, vname ); break;

		case PP_CLUSTER: sprintf( buf, "pinkpages/%s:%s", cluster, vname ); break;

		default: 	return 0; break;
	}

	return ng_nar_set( buf, value );
}


/* send a dump command to narbalek and skim off the variables that are returned. If skipto is 
   not 0, then we skip to the first occurrance of that character in the variable name and 
   report the name starting with the next character (allowing pinkpages/scope: to be skipped
   with a ':' skipto character.
*/
int ng_nar_namespace( char **utable, int unum, char *pattern, char skipto )
{
	static	void *flow = NULL;	/* flowmanager handle */
	static	Symtab	*syms = NULL;	/* used to list names only once */

	int	i = 0;			/* insert point into user buffer -- do not let this get larger than unum */
	int	n=1;
	int	oos = 0;		/* ran out of space in user buffer flag */
	int	empty_count = 0;	/* number of reads that we got nothing -- too many and we bail */
	char	*sp;			/* pointer into var=val buffer we get */
	char	ncmd[NG_BUFFER];	/* command to send to narbalek */
	char	buf[NG_BUFFER];		/* buffer of records from tcp */
	char	*rec;			/* newline sep record inside of buffer */
	Syment	*se;

	if( nar_fd < 0 )
	{
		if( ng_nar_open( NULL ) < 0 )
			return -1;
	}

	sfsprintf( ncmd, sizeof( ncmd ), "d::%s\n", pattern );
	send( nar_fd, ncmd, strlen( ncmd ), 0 );			/* send dump which returns name="value" #comment */
	ng_bleat( 4, "nar_namespace: sending to %d (%s)", nar_fd, ncmd );

	if( ! flow )
	{
		if( (flow = ng_flow_open( NG_BUFFER * 2 )) == NULL )	/* we let flow manager manage multi records per message */
		{
			ng_bleat( 0, "nar_if:pp_namespace: flow manager returned null handle" );
			return -1;
		}
	}

	if( !syms ) 
		syms = syminit( 1023 );			/* sym tab to track unique names */
	else
		symclear( syms );			/* clear the table if it existed */

	while( ng_testport( nar_fd, timeout ) > 0 )
	{
		if( (n = recvfrom( nar_fd, buf, sizeof( buf ), 0, NULL, NULL )) > 0 )
		{
			ng_flow_ref( flow, buf, n );		/* register the new buffer with flow manager */
			while( rec = ng_flow_get( flow, '\n' ) )	/* get each newline separated record in buffer */
			{
				if( strcmp( rec, "#end#" ) == 0 )	/* that's all folks */
				{
					if( oos )
						ng_bleat( 2, "nar_if: namespace: user table too small (%I*d) (%s)pat, %d names missed", Ii(unum), pattern, oos );
					return i;
				}

				if( *rec != '#' )		/* narbalek sends comments to */
				{
					if( i < unum )			/* we must read all even if out of space in userland */
					{
						if( skipto && skipto == '=' )		/* vars cannot have =, this means return name=value */
						{
							utable[i++] = ng_strdup( rec );	/* no pruning in this case; they get everything */
						}
						else
						{
							if( (sp = strchr( rec, '=' )) )		/* we only return the name */
								*sp = 0;
							if( skipto && (sp = strchr( rec, skipto )) )		/* skip leading portion if set */
								sp++;
							else
								sp = rec;			/* if we dont find, assume its all the var name */
									
							if( (se = symlook( syms, sp, 0, 0, 0 )) == NULL )	/* first time */
							{
								utable[i++] = ng_strdup( sp );
								symlook( syms, sp, 0, (void *) 1, SYM_COPYNM );
							}
						}
					}
					else
						oos++;			/* nuber of things missed */
				}
			}
		}
		else
		{
			if( empty_count++ > 25 ) 		/* we should not get empty buffers from narbalek; bail if we do */
			{
				ng_bleat( 0, "nar_if: namespace: warning: too many empty buffers received from narbalek; resetting connection" );
				break;
			}
		}
	}

	ng_bleat( 2, "nar_if: namespace: error or timeout waiting for end from narbalek: %s", errno ? strerror( errno ) : "timeout" );
	close( nar_fd );
	nar_fd = -1;
	return -1;
}

ng_pp_namespace( char **utable, int unum )
{
	return ng_nar_namespace( utable, unum, "pinkpages/", ':' );	
}
	

#ifdef SELF_TEST
extern verbose;
main( int argc, char **argv )
{
	char *b;
	verbose = 3;
	int where = 0;

/*		ng_nar_retries( 0, 0 ); */

	argv0="nar-if-test";

	if( strcmp( argv[1], "-v" ) == 0 )
	{
		verbose++;
		argv++;
	}

	if( argc <= 1 || strcmp( argv[1], "-?" ) == 0 )
	{
		printf( "%s {namespace | loop port varname [delay] | set nar-varname value | ppget varname |ppset-scope scope var value| ppset var value | nar_var_name}\n", argv[0] );
		exit( 1 );
	}


	if( strcmp( argv[1], "loop" ) == 0 )			/* loop to test session reset if lost */
	{							/* assume cmd line:  loop port varname */
		int delay = 2;

		if( argc < 4 )
		{
			printf( "must enter: 'loop' port pinkpages-var-name [delay-seconds]\n" );
			exit( 1 );
		}

		delay = argc > 4 ? atoi(argv[4]) : 2;

		ng_nar_open( argv[2] ); 
		while( 1 )
		{
			ng_nar_open( argv[2] ); 	/* open specific port */
			b = ng_nar_get( argv[3] );
			if( ! b )
				ng_bleat( 0, "No value returned for %s", argv[3] );
			else
			{
				printf( "nar_get returned: %s=%s\n", argv[3], b );
				if( strcmp( b, "quit" ) == 0 )
				{
					printf( "quitting!\n" );
					break;
				}
				
				free( b );
			}
			
			sleep( delay );
		}
		free( b );

		exit( 0 );
	}

	if( strcmp( argv[1], "cache" ) == 0 )
	{
		void *cache = NULL;
		int count = 100;
		int i;
		ng_timetype start, end;

		if( argc < 5 )
		{
			printf( "must enter: 'cache' port var-pattern varname \n" );
			exit( 1 );
		}

		ng_nar_open( argv[2] );

		cache = ng_nar_cache_new( argv[3], 600 );

		b = ng_nar_cache_get( cache,  argv[4] );
		sfprintf( sfstdout, "after cache load %s = %s\n", argv[4], b ? b : "NULL" );

		i = count;
		start = ng_now();
		while( i-- > 0  )
		{
			if( b )
				ng_free( b );
			b = ng_nar_cache_get( cache,  argv[4] );
		}
		end = ng_now();

		sfprintf( sfstdout, "fetching var %d times took %ds, last=%s\n", count, (int) (end-start)/10, b ? b : "NULL" );
		sfprintf( sfstdout, "pausing 7s to allow var to be manually changed: %s\n", argv[4] );
		
		sleep( 7 );
		/*ng_nar_cache_reload( cache );*/
		b = ng_nar_cache_get( cache,  argv[4] );
		sfprintf( sfstdout, "after cache reload %s = %s\n", argv[4], b ? b : "NULL" );
		ng_nar_cache_close( cache );
		exit( 0 );
	}

	if( strcmp( argv[1], "speed" ) == 0 )			/* loop getting a var n times to test speed */
	{							/* assume cmd line:  speed port varname [count] */
		int count = 100;
		int i;
		ng_timetype start, end;

		if( argc < 4 )
		{
			printf( "must enter: 'speed' port pinkpages-var-name [count]\n" );
			exit( 1 );
		}

		ng_nar_open( argv[2] ); 
		i = count;
		start = ng_now();
		while( i-- > 0  )
		{
			b = ng_nar_get( argv[3] );
			if( b )
				ng_free( b );
		}
		end = ng_now();
		sfprintf( sfstdout, "fetching var %d times took %ds\n", count, (end-start)/10 );

		exit( 0 );
	}

	if( strcmp( argv[1], "set" ) == 0 )
	{
		if( ng_nar_set( argv[2], argv[3] ) <= 0 )
		{
			printf( "nar_set failed\n" );
			exit( 1 );
		}
		else
		{
			/* close( nar_fd ); */		/* this forces an error on the first read to test retry logic */
			b = ng_nar_get( argv[2] );
			printf( "value that was set: %s\n", b ? b : "nothing was retreieved -- value unset?" );
			exit( 0 );
		}

		exit( 2 );			/* huh? */
	}
	else
	if( strcmp( argv[1], "ppget" ) == 0 )
	{
		b = ng_pp_get( argv[2], &where );
		printf( "pp_get returned: where=%d (%s)\n", where, b ? b : "NULL!" );
		free( b );
		exit(  0 );
	}
	else
	if( strcmp( argv[1], "ppset-scope" ) == 0 )
	{
		int scope = PP_CLUSTER;
		if( strcmp( argv[2], "flock" ) == 0 )
			scope = PP_FLOCK;
		else
		if( strcmp( argv[2], "node" ) == 0 )
			scope = PP_NODE;

		ng_pp_set( argv[3], argv[4], scope );
		/* close( nar_fd );  force error and retry test */
		b = ng_pp_get( argv[3], &where );
		printf( "pp_get returned (default): where=%d: (%s)\n", where, b ? b : "no-value (nil)" );
		free( b );
		exit( 0 );
	}
	else
	if( strcmp( argv[1], "ppset" ) == 0 )
	{
		ng_pp_set( argv[2], "node-value", PP_NODE );
		ng_pp_set( argv[2], "cluster-value", PP_CLUSTER );
		ng_pp_set( argv[2], "flock-value", PP_FLOCK );

		b = ng_pp_get( argv[2], &where );
		printf( "pp_get returned (default): where=%d: (%s)\n", where, b );
		free( b );

		b = ng_pp_scoped_get( argv[2], PP_CLUSTER, NULL );
		printf( "pp_get_scoped returned (cluster): (%s)\n", b );
		ng_free( b );

		b = ng_pp_scoped_get( argv[2], PP_FLOCK, NULL );
		printf( "pp_get_scoped returned (flock): (%s)\n", b );
		ng_free( b );
		exit(  0 );
	}

	if( strcmp( argv[1], "namespace" ) == 0 )
	{
		char *names[NG_BUFFER];
		int i;
		int k;
		int delay = 5;
		int loop = 1;			/* default to once */
		char *pat = NULL;

		if( argc < 2 )
		{
			ng_bleat( 0, "usage: %s namespace [loop n] pattern", *argv );
		}

		ng_bleat( 0, "namespace test started" );
		verbose = 3;

		if( strcmp( argv[2], "loop" ) == 0 )
		{
			if( argc < 4 )
			{
				ng_bleat( 0, "usage: %s namespace [loop n] pattern", *argv );
				exit( 1 );
			}
			loop = atoi( argv[3] );
			pat = argv[4];
		}
		else
			pat = argv[3];


		do
		{
			k = ng_nar_namespace( names, NG_BUFFER, pat, '/' );
			ng_bleat( 0, "ng_pp_namespace returned pat=%s k=%d", pat, k );
			if( verbose > 3 )
				for( i = 0; i < k; i++ )
				{
					sfprintf( sfstdout, "%s\n", names[i] );
					ng_free( names[i] );
				}
			if( --loop )
			{
				sleep( delay );
				printf( "===================================\n" );
			}
		}
		while( loop );


		exit( 0 );
	}
	else
	{
		while( --argc > 0 )
		{
			b = ng_nar_get( argv[1] );
			printf( "main %s = (%s)\n", argv[1], b ? b : "no-value-returned" );
			argv++;
			sleep( 1 );
		}
		exit( 0 );
	}

	printf( "main got: %s\n", b );
	free( b );
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_if:Narbalek C Interface Functions)

&space
&synop	
	#include "ningaui.h"
	#include "ng_lib.h"

&break	char *ng_nar_get( char *vname );
&break	int ng_nar_set( char *vname, char *value );
&break	int ng_nar_open( char *port );
&break	void ng_nar_close( );
&break  void ng_nar_to( int timeout );
&break	void ng_nar_retries( int s, int g );
&space
&break	void *ng_nar_cache_new( char *pattern, int refresh )
&break	char *ng_nar_cache_get( void *cache, char *vname )
&break  void ng_nar_cache_close( void *cache )
&break	int  ng_nar_cache_reload( void *cache )
&break  void ng_nar_cache_setsize( int size )
&space
&break	int ng_nar_namespace( char **utable, int ulen, char *pattern, char skipto );
&break	int ng_pp_namespace( char **utable, int ulen );
&space
	char *ng_pp_get( char *vname, int *where );
&break	char *ng_pp_scoped_get( char *vname, int scope, int *where );
&break	int  ng_pp_set( char vname, char *value, int scope );

&space
&desc	These functions provide an interface to narbalek allowing a C programme to 
	fetch or set a narbalek variable. The open routine is called automatically
	on the first get/set call.  It is needed only if the programme will be 
	communicating directly with narbalek and bypasing the other functions.
&space
	The pinkpages (pp) functions allow a C programme to set/get values from 
	narbalek that are maintained in the pinkpages name space (with node, cluster
	and flock name scoping).  Scope constants are:
&space
&indent
&begterms
&term	PP_FLOCK : Set/get the name based on the flock namespace.
&space
&term	PP_CLUSTER : Set/get the name based on the cluster namespace.
&space
&term	PP_NODE : Set/get the name based on the node namespace.
&endterms
&uindent
&space
	For the &lit(ng_pp_scoped_get)  command, the scope &lit(PP_NCF_ORDER) may be 
	supplied to search for the variable in all three namespaces (node, cluster, flock)
	returning the first non-null value found.  The &lit(ng_pp_get) function 
	is the same as calling the scoped_get() function with this option.
&space
	When &lit(PP_NCF_ORDER) is specified, and &ital(where) is not NULL, &ital(where)
	will be set to the name space (PP_FLOCK, PP_NODE, or PP_CLUSTER) that cooresponds
	to where the variable value was found.  This value can be passed as the scope value
	on a &lit(ng_pp_set) function call if the value needs to be changed in the same
	breakscope as it was found. 
&space
	If a matching variable name is not defined, the result will be a NULL pointer from 
	&lit(ng_scoped_get). The &lit(ng_pp_get) function will return an empty string if 
	the named variable is not set.  
&space
&subcat ng_nar_retries
	Allows the user to specify the number of retries that will be attempted when a set and 
	get operation fails.  The retries when setting variables, and when getting values, 
	are managed independantly.
	The assumption is that narbalek is in the process of being cycled,
	or reloading and is not accepting requests. A pause of two seconds is implemented 
	between retries and if the user programme does not invoke this function the default
	value is established to 2. 	Retries can be >= 0. 

&space
&subcat Cache functions
	The caching funcitons allow a series of variable value pairs to be cached and 'looked up'
	using the &lit(ng_nar_cache_get) function.  This can drasticly speed up the access to 
	narbalek data as only one TCP request for the variables is made for each refresh 
	period (seconds) passed to the &lit(ng_nar_cache_new) funciton.   Multiple caches can 
	be active concurrently if the application is not able to address all of its narbalek 
	variables with a single pattern. Because of scoping limitations, the pinkpages functions
	do NOT make use of caching, and thus pinkpages lookups are subject to the overhead of 
	sending a TCP/IP message to narbalek for each lookup.

&space
	The cache get/read functions return NULL if the requested variable was not set. The 
	cache reload function returns 0 if the load failed. 

.** caching was added primarly to support equerry and applications like equerry which manage
.** a large number of narbalek variables.

&space
&subcat	ng_pp_namespace
	Accepts a pointer to an array of character pointers of size &lit(ulen).  A query to 
	narbalek will be made, and the array will be populated with pointers to variable
	name strings.  It is the responibility of the calling programme to free these 
	strings. 
&space
	The return value is the number of variable name pointers placed into the array; a 
	value < 0 indicates that narbalek could not be communicated with, or did not return 
	a complete table dump. A value of 0 indicates that &ital(narbalek) does not have a 
	stable state. In the case where narbalek is not stable, a retry should be made after
	a small delay.
&space
	Because the pinkpages namespace is a scopped name space, it is possible for 
	multiple 'basenames' to exist.  The list returned will be a list of unique names 
	across the entire pinkpages namespace. Just because a variable name is returned, there
	is no guarentee that a call to &lit(ng_pp_get) will return a value as it might not 	
	be scopped for the current node. 

&space
&subcat	ng_nar_namespace
	Similar to the pinkpages namespace function, except that this function accepts 
	a regular expression pattern and a 'skipto' character as additional parameters. 
	The pattern is used to match narbalek variable names.  If the &ital(skipto) character
	is not 0, then that character is searched for in the variable name, and the name
	is considered to begin with the character immediately following. As an example, 
	pinkpages variables can be located with a pattern of "pinkpages/.*" and a skipto
	character of ':'.   
&space
	The return values are the same as for &lit(ng_pp_namespace). If the skipto character
	is an equal sign (illegal for a variable name), the implication is that the desired
	output strings are variable-name=value strings rather than a list of variable names.

&exit	Generally, a return code of 0, or NULL, indicates an error, or that the value 
	was not found.  A non-zero return code indicates success.

&space
&see	ng_nar_set, ng_nar_get, ng_narbalek, ng_ppset, ng_ppget

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Jan 2005 (sd) : Fresh from the oven.
&term	07 Jan 2005 (sd) : Added pp functions.
&term	09 Jan 2005 (sd) : Added where parm to get functions so caller knows where the 
	value came from in the pp namespace.
&term	17 Feb 2005 (sd) : Added timeout value.
&term	24 Feb 2005 (sd) : nar_open now saves port if given one so that if a caller opens the session
		on a specific port, and the session drops, reattempts are made on the same port
		rather than on the default narbalek port. Corrected issue with detecting dropped session.
&term	21 Mar 2005 (sd) : Changed a strdup() call to ng_strdup().
&term	30 Mar 2005 (sd) : Now waits hard for and end ack from narbalek rather than flushing before an 
		operation. 
&term	02 May 2005 (sd) : Added a bit of a long retry to nar_open.
&term	24 May 2005 (sd) : Corrected read loop in wai4end to exit on error and not just timeout.
&term	31 May 2005 (sd) : If we cannot attach to the local narbalek, then we use the bootstrap node
	feature in ng_nar_get to find a node with a running narbalek and use it. (Old code used 
	the netif node if localhost failed).
&term	31 Jul 2005 (sd) : changes to prevent gcc warnigns.
&term	23 Dec 2005 (sd) : Fixed cause of core dump in alt_node. Sfgetr() return was not checked.
&term	13 Jan 2006 (sd) : Added ng_nar_namespace and ng_pp_namespace functions.
&term	23 Jan 2006 (sd) : Fixed problem in alt-node function; not closing pipe after finished.
&term	22 May 2006 (sd) : Added check in alt_node() to handle the case where ng_nar_get returns an empty
		string (not a NULL pointer).  This caused warnings from ng_rx() that host lookup failed twice.
&term	15 Jun 2006 (sd) : If write is successful, but wait4end() times out we return an error as it might
		mean that narbalek did not get the buffer. 
&term	14 Jul 2006 (sd) : Added retries to nar_set and nar_get. Also added the ng_nar_retries() function to allow the 
		user programme to change the number of retries. 
&term	01 Aug 2006 (sd) : Added check in ppg_get to return an empty string rather than a null pointer when the 
		value is not found in pinkpages space. 
&term	11 Oct 2006 (sd) : Comment cleanup.
&term	28 Oct 2006 (sd) : Corrected problem with alternate connect host name buffer (freed but not nulled).
&term	16 Nov 2006 (sd) : Added a bleat when giving up connection to narbalek attempt. 
		Ng_nar_get() now checks for the narbalek error string that can come back and returns a null pointer
		when it sees this.
&term	28 Feb 2007 (sd) : Fixed memory leak in alt_node().
&term	02 Apr 2007 (sd) : Added an abort if the port info looks trashed.
&term	13 Apr 2007 (sd) : Fixed memory stomping when trying to reconnect to narbalek (finally).
&term	01 Aug 2007 (sd) : If namespace is called with = as skip to, then we return the whole buffer name=value 
		and not just variable names. 
		Added caching routines to help applications speed up multiple lookups that can be sussed out 
		with an initial pattern search.
&term	04 Sep 2007 (sd) : Corrected problem in cache reload; not expecting return value of -1 from namespace(). 
&term	11 Sep 2007 (sd) : Changed bleat level for error message if end/ack not recevied; now screams regardless of 
		verbose setting.
&term	01 Oct 2007 (sd) : Corrected problem that was causing a core dump in cache reload.
&term	26 Mar 2008 (sd) : Corrected bug with setting the refresh expiration for the cache. 
		Added ng_nar_cache_setsize() function. 
		
&term	31 Mar 2008 (sd) : Found and fixed bug in cache new function.  It was not using the cache size variable. V2.3
&term	01 Apr 2008 (sd) : Changed strdup() calls to ng_strdup() calls.
&term	29 Apr 2008 (sd) : Cache reload returns 0 on failure. 
&term	11 Jul 2008 (sd) : Corrected nar_get() such that it checks for #ERROR# in all cases. 
&term	01 Aug 2008 (sd) : Tweeked nar_get() to avoid missing end of data indicator if string split between buffers. (v2.4)
&endterms

&scd_end
------------------------------------------------------------------ */
