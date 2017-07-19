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
Mnemonic:	equerry - filesystem mount manager
Abstract:	daemon to manage filesytem mounts as described by the various
		equerry narbalek variables.  The narbalek variable names have the 
		format:
			<daemonid>/<filesystemid>:[<hostname>.]<vartype>


		The <daemonid> is always equerry.  Not all types need a host name;
		variables such as the state are managed on a host basis as individual
		variables so as to avoid collisions with concurrent updates (narbalek
		does not provide for update locking).

		The target filesystems for equerry to manage are those connected
		via network/fibre channel and thus might be easiy switched to 
		another node.  Local filesystems are not indended to be managed
		with equerry as equerry cannot handle duplicate fs names. 
		
		we use the narbalek cache routines to access most of our vars in 
		narbalek.  this means that each loop we refresh the cache with all
		known equerry/.* variables , and read values from the cache rather 
		than sending messages to narbalek.  this can be very much faster 
		than making individual requests. 

Date:		06 July 2007
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<signal.h>
#include	<regex.h>
#include 	<errno.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>
#include 	<ng_socket.h>

#include	"equerry.man"




/* ----------------- globals  -------------------------------------------- */

#define SAFE_STR(a)	(a==NULL?"no-value":a)		/* safe string */
#define SAFE_STR_D(a,b)	(a==NULL?b:a)	/* safe string with default */

#define SPLIT		1			/* cat_value2 split options */
#define NO_SPLIT	0

#define SPACE_FS	0		/* symtab space id */

#define CYCLE_TSEC	300		/* how often (tenths) we check for changes to nar vars */
#define REVERIFY_TSEC	1500		/* force verify every 2.5 minutes */

#define MAX_FS		1024		/* max filesystems that we can handle */

#define ST_UNKNOWN	0		/* order is important -- we assume the higher the value the more significant the state */
#define ST_UNMOUNT	1		/* we should not have it mounted */
#define ST_MOUNTRO	2		/* mount in read only mode */
#define ST_MOUNTRW	3		/* r/w mount */


/* narbalek variable type constants and strings */
#define VT_PARM		0	
#define VT_STATE	1
#define VT_HOST		2

/* flag constants */
#define FL_FORCE_VERIFY	0x01		/* force verification of local state */
#define FL_GENTAB	0x02		/* force the generation of fstab as something has likely changed */

/* fsinfo flags */
#define FFL_IGNORE	0x01		/* unallocated noted in parm var -- we will do nothing with this */
#define FFL_NOACTION	0x02		/* we track and update the fstab for the filesystem, but do not mount/unmount it */
#define FFL_MDIRONLY	0x04		/* need to fixup mountpoint info as nar data has only parent and not real mountpoint */
#define FFL_PRIVATE	0x08		/* disk is 'locally attached' -- not capable of being mounted by another system */
#define FFL_PURGED	0x10		/* we re-generated fstab after this was marked unmounted */

char *vtstring[] = {		/* order must match VT_ const */
	"parms",
	"state",
	"host",
	(char *) 0
};

char *ststring[] = {		/* state strings, order must match ST_ constatnts */
	"unknown",
	"unmounted",
	"mounted-ro",
	"mounted-rw",
	"ignore",
	(char *) 0
};

typedef struct session {
	struct session *next;
	struct session *prev;
	int	fd;
	char	*addr;
	void	*flow;
} Session_t;

typedef struct fsinfo {			/* filesystem information */
	char	*name;
	int	remote_state;		/* 'highest' state that others are reporting it in */
	int	local_state;		/* state that we believe it to be in on this node */
	int	target_state;		/* state wed like it to be in */
	int	verified;		/* we have verified and/or set the nar state with system state */
	int	flags;			/* FFL_ constants */
	int	ua_lease;		/* unmount announce lease; must announce unmounted until this time is reached if this is not 0 */
	char	*type;			/* info for mount command */
	char	*label;			/* the device (fstab field 0); only needed if not name */
	char	*mtpt;			/* the actual mount point; old version kept just the directory name here */
	char	*opts;			/* rw and ro are ignored if supplied */
	char	*src;			/* phys location -- comment use */
} Fsinfo_t;


extern int errno;

int	verbose = 0;
int	flags = 0;			/* FL_ constants */
int	ok2run = 1;			/* turned off when it is no longer safe to execute */
char	*did_string = "equerry";	/* daemon id used in var names (can change for testing) */
char	*my_hname = NULL;		/* my hostname */
char	*ng_root = NULL;		/* root dir */
char	*system_mtdir = NULL;		/* default system mount dir (NG_SYSTEM_MTDIR) */
Session_t	*sessions = NULL;
void	*nar_cache = NULL;		/* narbalek cached data */
ng_timetype verified_expiry = 0;	/* we reset verification every so often so if someone manually changes we correct it */
ng_timetype nxt_do = 0;			/* next time we need to do something real */
char	*tmp_dir = NULL;		/* value of TMP */

Symtab	*fshash = NULL;	

char	*version = "3.2/04139"; 

/* ----------------------------------- prototypes ------------------------------- */

/* ---------------- utility ---------------------------------------------------- */
static void usage( )
{
	sfprintf( sfstderr, "equerry %s\n",  version );
	sfprintf( sfstderr, "usage: %s [-i did-string] [-p port] [-v] \n", argv0 );
	exit( 1 );
}


/* strip off name= and return value from name=value in tok */
char *suss_value( char *tok )
{
	char 	*p;

	if( p = strchr( tok, '=' ) )
		return ng_strdup( p+1 );
	return NULL;
}

/* 
	Add str to the front of vtok. If vtok is of the form name=value, and split
	is true, then name= is discarded and str is added to the 'front' of the 
	value portion of vtok.
*/
char *cat_value2( char *str, char *vtok, int split )
{
	char 	*p;
	char	buf[2048];

	if( split && (p = strchr( vtok, '=' )) != NULL )		
		p++;
	else
		p = vtok;
	
	sfsprintf( buf, sizeof( buf ), "%s%s", SAFE_STR_D( str, "" ), SAFE_STR_D( p, "" ) );
	return ng_strdup( buf );
}


/* build a variable name using the filesystem name */
char *build_vname( char *fsname, int type )
{
	char	wbuf[1024];

	switch( type )
	{
		case VT_STATE:			/* variables that need the host name */
			sfsprintf( wbuf, sizeof( wbuf ), "%s/%s:%s.%s", did_string, fsname, my_hname, vtstring[type] );
			break;

		default:			/* non-host oriented variables */
			sfsprintf( wbuf, sizeof( wbuf ), "%s/%s:%s", did_string, fsname, vtstring[type] );
			break;
	}

	return ng_strdup( wbuf );
}

/* 	set a narbalek variable for the fp 
	type is a VT_ constant

	if the type is a state variable, and the state is unmounted, we will set the variable only 
	while the fp->ua_lease is not expired.  After the lease expires, then we delete the var
	in order to reduce the number of variables in narbalek (one unmounted state per node per
	global filesystem is a LOT of vars).  We need to set the state var to unmounted for a small 
	amount of time after we actually unmount the disk so that if any other node is waiting for 
	the filesystem to free up so it can be mounted, the remote node will see the unmount.  We
	dont want remote nodes to assume no variable means unmounted as no var could be narbalek 
	down, or other net related failure. 
*/
void set_var( Fsinfo_t *fp, int type, char *val )
{
	char *vname;
	char buf[1024];

	if( fp->flags & FFL_IGNORE  )
	{
		ng_bleat( 4, "set_var: state is IGNORE: not setting var: %s", fp->name );
		return;
	}

	if( fp->ua_lease  && ng_now() > fp->ua_lease ) 		/* if unmount announce lease is set, and expired */
		fp->ua_lease = 0;				/* turn it off -- we'll now delete state var rather than announcing it unmounted */

	vname = build_vname( fp->name, type );
	if( type == VT_STATE  && fp->ua_lease == 0  &&  strcmp( val, "unmounted" ) == 0  )
		*buf = 0;				/* unset the var if unmount is the state */
	else
		sfsprintf( buf, sizeof( buf ), val );

	ng_bleat( 3, "set_var: %s = %s", vname, buf );
	ng_nar_set( vname, buf );

	ng_free( vname );
}

/* ---------------- fsinfo management ------------------------------------------- */
/* take a full narbalek var name, and fish out the filesystem part 
	equerry/fsname:var-type
*/
char *get_fsname( char *vname )
{
	char	*bp;
	char	*ep;		/* pointer at end */
	char	wbuf[256];

	
	if( (bp = strchr( vname, '/' )) != NULL )
	{
		bp++;
		if( (ep = strchr( bp, ':' )) != NULL && ep-bp < 255 )
		{
			memcpy( wbuf, bp, ep-bp );
			wbuf[ep-bp] = 0;
			return ng_strdup( wbuf );
		}
	}

	return NULL;
}

/* make a new filesystem reference and add to symtable */
Fsinfo_t *new_fsinfo( char *name )
{
	Syment	*se;
	Fsinfo_t	*fp;

	fp = (Fsinfo_t *) ng_malloc( sizeof( *fp ), "fsinfo block" );
	memset( fp, 0, sizeof( *fp ) );

	fp->name = ng_strdup( name );
	symlook( fshash, fp->name, SPACE_FS, fp, SYM_NOFLAGS );		/* add to hash */

	return fp;
}

/* fetch one by name and make a new one if not there */
Fsinfo_t *get_fsinfo( char *name )
{
	Syment	*se;
	Fsinfo_t 	*fp;

	if( (se = symlook( fshash, name, SPACE_FS, NULL, SYM_NOFLAGS )) != NULL )
		return (Fsinfo_t *) se->value;

	return new_fsinfo( name );
}

/* fetch a list of variable names that match fsname/type; fsname can be ".*" for all */
char **load_fslist( char *fsname, int type )
{
	char	**list = NULL;
	int	fscount;		/* number in table */
	char	wbuf[1024];

	list = (char **) ng_malloc( sizeof( char *) * 8192, "fslist" );
	memset( list, 0, sizeof( char *) * 8192 );

	if( fsname == NULL || ! *fsname )
		fsname = ".*";				/* if nothing provided, default to listing all */
	
	sfsprintf( wbuf, sizeof(wbuf), "^%s/%s:.*%s", did_string, fsname, vtstring[type] );
	ng_bleat( 3, "load_fslist: size=8192, %s", wbuf );
	if( (fscount = ng_nar_namespace( list, 8191, wbuf, 0 )) <= 0 )	/* if nar is disconnected, we should eventually reconnect */
	{								/* but we bail for now so we don't block */
		ng_free( list );
		ng_bleat( 2, "narbalek returned empty fslist: pat=%s  [WARN]", wbuf );		/* this might be ok if there really are no vars set */
		return NULL;
	}

	list[fscount] = NULL;

	return list;
}

/* dispose of a list */
void drop_fslist( char **list )
{
	int i;

	for( i = 0; list[i]; i++ )
	{
		ng_free( list[i] );
		list[i] = 0;
	}

	

	ng_free( list );
	list = 0;
}


/* ---------------- session management ------------------------------------------ */
Session_t *find_session( fd )
{
	Session_t *sp;

	for( sp = sessions; sp && sp->fd != fd; sp = sp->next );
	return sp;
}

void add_session( int fd, char *addr )
{
	Session_t	*sp;

	sp = (Session_t *) ng_malloc( sizeof( Session_t ), "session block" );
	memset( sp, 0, sizeof( *sp ) );

	sp->fd = fd;
	sp->flow = (void *) ng_flow_open( 1024 );
	sp->addr = ng_strdup( addr );

	if( (sp->next = sessions) != NULL )
		sp->next->prev = sp;
	sessions = sp;

	ng_siprintz( 0 );		/* dont send trailing null on siprintfs */
}


void drop_session( int fd )
{
	Session_t *sp;

	if( (sp = find_session( fd )) == NULL )		/* oops if null? */
		return;

	ng_flow_close( sp->flow );	/* drop flow manager stuff */
	if( sp->addr )
	{
		ng_free( sp->addr );
		sp->addr = 0;
	}

	if( sp->next )			/* unchain */
		sp->next->prev = sp->prev;
	if( sp->prev )
		sp->prev->next = sp->next;
	else
		sessions = sp->next;

	ng_free( sp );
	sp = 0;
}

void gen_fstab_entry( Syment *se, void *data )
{
	Fsinfo_t *fp;
	Sfio_t	*fd;

	if( (fp = (Fsinfo_t *) se->value ) == NULL )
		return;

	if( fp->flags & FFL_IGNORE || fp->target_state == ST_UNMOUNT || fp->target_state == ST_UNKNOWN  )
		return;
	
	if( ! data )
		return;

	fd = *((Sfio_t **) data);

	ng_bleat( 2, "gen_fstab: %s %s %s %s%s	0 2\n", fp->label, fp->mtpt, fp->type, fp->target_state == ST_MOUNTRW ? "rw" : "ro", SAFE_STR_D( fp->opts, "" ) );
	if( *fp->label == '/' )
		sfprintf( fd, "%s %s %s %s%s	0 2\n", fp->label, fp->mtpt, fp->type, fp->target_state == ST_MOUNTRW ? "rw" : "ro", SAFE_STR_D( fp->opts, "" ) );
	else
		sfprintf( fd, "LABEL=%s %s %s %s%s	0 2\n", fp->label, fp->mtpt, fp->type, fp->target_state == ST_MOUNTRW ? "rw" : "ro", SAFE_STR_D( fp->opts, "" ) );
}

/* generate the current fstab entries as we prepare to issue a mount */
void gen_fstab( char *fname )
{
	static	int count = 0;
	Sfio_t	*f;

	if( (f = sfopen( NULL, fname, "w" )) != NULL )
	{
		symtraverse( fshash, SPACE_FS, gen_fstab_entry, &f );		/* for each, generate an fstab if we should mount it */
		sfclose( f );
	}
	else
		ng_bleat( 0, "error opening fstab file: %s: %s", fname, strerror( errno ) );
}

/* -------------------- network callback routines ---------------------------*/
int cbsig( char *data, int sig )
{

	return SI_RET_OK;
}

int cbconn( char *data, int f, char *buf )
{
	add_session( f, buf );		/* assume alien until they tell us otherwise */
	ng_bleat( 2, "connection accepted: fd=%d from=%s", f, buf );
 	return( SI_RET_OK );
}

int cbdisc( char *data, int fd )
{
	struct session *sb;

	ng_bleat( 2, "session disconnect: %d", fd );
	drop_session( fd );
 
	return SI_RET_OK;
}


/*	
	udp message -- should not happen, but cover our bottle with small bit here
*/
int cbudp( void *data, char *buf, int len, char *from, int fd )
{
	char *p;	/* pointer past prefix */

	buf[len-1] = 0;					/* all buffers are \n terminated -- we require this */
	
	ng_bleat( 6, "udp buffer ignored: %s", buf );
	return SI_RET_OK;
}

/* tcp data  
	commands:
		list fs-pattern	(list info about all file systems whose name matches the pattern)
		xit		(cause us to die gracefully)

	we drop the session like a hot potato if the command is not legit. 
*/
int cbtcp( void *data, int fd, char *buf, int len )
{
	Session_t *sb;
	char	*rec;			/* pointer at next record to process */
	int	drop = 0;		/* if we must drop the session */


	ng_bleat( 5, "cbtcp: %d bytes = (%.90s)%s", len, buf, len > 90 ? "<trunc>" : "" );
	if(  (sb = find_session( fd )) == NULL )
	{
		ng_bleat( 2, "did not find session: %s", fd );
		return SI_RET_OK;
	}

	ng_flow_ref( sb->flow, buf, len );              	/* regisger new buffer with the flow manager */

	while( rec = ng_flow_get( sb->flow, '\n' ) )		/* get all complete (new line terminated) records */
	{
		if( *rec == '~' )			/* left over from version 1 external interface stuff -- we ignore */
			rec++;

		ng_bleat( 4, "received: %s", rec );
		switch( *rec )
		{
			case 'l':
				if( strncmp( rec, "list", 4 ) == 0 )	/* list fspattern*/
				{
					char	**list;
					char	*name;
					Fsinfo_t *fp;
					int	i;

					ng_siprintf( fd, "list: pattern=%s\n", rec+5 );
					if( (list = load_fslist( rec+5, VT_PARM )) )
					{
						for( i = 0; list[i]; i++ )
						{
							name = get_fsname( list[i] );	
							fp = get_fsinfo( name );
							if( fp->flags & FFL_IGNORE )
								ng_siprintf( fd, "[%d] %s reserved, available or ignored\n", i, name );
							else
							{
								ng_siprintf( fd, "[%d] %s target=%s current=%s remote=%s ", i, name, ststring[fp->target_state], ststring[fp->local_state], ststring[fp->remote_state] );
								ng_siprintf( fd, "%s type=%s opts=%s mount=%s label=%s src=%s\n", fp->flags & FFL_NOACTION ? "NOACT" : "", SAFE_STR( fp->type ), SAFE_STR( fp->opts ), SAFE_STR( fp->mtpt ), SAFE_STR( fp->label ), SAFE_STR( fp->src ) );
							}

							ng_free( name );
							name = 0;
						}
						drop_fslist( list );
					}
					else
						ng_siprintf( fd, "no list found\n" );
					
				}
				else
					drop = 1;
				break;

			case 'm':
				if( strncmp( rec, "mlist", 5 ) == 0 )	/* list just what we have mounted or target=mounted */
				{
					char	**list;
					char	*name;
					Fsinfo_t *fp;
					int	i;

					ng_siprintf( fd, "list: target=mountr[wo] | current=mountr[wo]\n", rec+5 );
					if( (list = load_fslist( rec+5, VT_PARM )) )
					{
						for( i = 0; list[i]; i++ )
						{
							name = get_fsname( list[i] );	
							fp = get_fsinfo( name );
							if( fp->target_state  == ST_MOUNTRW || fp->target_state == ST_MOUNTRO ||
								fp->local_state == ST_MOUNTRW || fp->local_state == ST_MOUNTRO )
							{
								ng_siprintf( fd, "[%d] %s target=%s current=%s remote=%s ", i, name, ststring[fp->target_state], ststring[fp->local_state], ststring[fp->remote_state] );
								ng_siprintf( fd, "%s type=%s opts=%s mount=%s label=%s src=%s\n", fp->flags & FFL_NOACTION ? "NOACT" : "", SAFE_STR( fp->type ), SAFE_STR( fp->opts ), SAFE_STR( fp->mtpt ), SAFE_STR( fp->label ), SAFE_STR( fp->src ) );
							}

							ng_free( name );
							name = 0;
						}
						drop_fslist( list );
					}
					else
						ng_siprintf( fd, "no list found\n" );
					
				}
				else
					drop = 1;
				break;

			case 'r':
				if( strncmp( rec, "reverify", 8 ) == 0 )
				{
					verified_expiry = 0;				/* force reverification now! */
					ng_siprintf( fd, "equerry: state verification will be forced for all filesystems during next pass.\n.\n" );
				}
				else
					drop = 1;
				break;

			case 'u':			/* nar vars were updated, force do now */
				if( strncmp( rec, "updated", 7 ) == 0 )
				{
					ng_bleat( 2, "updated received -- expiring do work timer" );
					nxt_do = 0;				/* force a pass through do-stuff now */
					verified_expiry = 0;			/* force reverification now! */
					ng_siprintf( fd, ".\n" );		/* no real response from this */
				}
				else
					drop = 1;
				break;
		
			case 'v':
				if( strncmp( rec, "verbose", 7 ) == 0 )
				{
					verbose = atoi( rec+8 );
					ng_siprintf( fd, "equerry: verbose level changed to: %d\n.\n", verbose );
					ng_bleat( 0, "verbose level changed to: %d", verbose );
				}
				else
					drop = 1;
				break;

			case 'x': 
				if( strncmp( rec, "xit4360", 7 ) == 0 )
				{
					ng_bleat( 0, "exit received; shutting down: fd=%d addr=%s", fd, sb->addr );
					ok2run = 0;
					ng_siprintf( fd, ".\n" );
				}
				else
					drop = 1;
				break;

			default:
				drop = 1;
				break;
		}

		if( drop )
		{
			ng_bleat( 0, "session dropped; probably bad command: (%s) fd=%d addr=%s", rec, fd, sb->addr );
			ng_siclose( fd );
			drop_session( fd );
			return SI_RET_OK;		/* bugger out now, dont process rest of their buffer */
		}
	}	

	ng_siprintf( fd, ".\n" );			/* end of data mark */
	return SI_RET_OK;
}


/* ---------------------- management things --------------------------------------------------*/

/* 
	called by symtraverse for each finfo block in the hash
	look at narbalek var and set the target state 
	we assume that this is driven by symtraverse; data is ignored
*/
void set_target_state( Syment *se, void *data )
{
	Fsinfo_t *fp;
	int	state = ST_UNMOUNT;
	int	rw_count = 0;		/* number of host:rw tokens found before we find our name in list */
	char	vname[256];
	char	*val = NULL;		/* value of var in narbalek */
	char	*tok;			/* at next token in the var string */
	char	*cp;			/* pointer into the token */
	char	*op = NULL;		/* owner of the unshared fs */
	char	*strtok_p = NULL;

	fp = (Fsinfo_t *) se->value;
	fp->target_state = ST_UNMOUNT;		/* default to unmount */

	sfsprintf( vname, sizeof(vname), "%s/%s:%s", did_string, fp->name, vtstring[VT_HOST] );
	if( (val = ng_nar_cache_get( nar_cache, vname )) == NULL )
	{
		ng_bleat( 2, "nothing in nar cache for: %s flags=%02x", vname, fp->flags );
		if( fp->flags & FFL_PRIVATE )
		{
			ng_bleat( 2, "set_target: private filesystem will be ignored: %s no owner found", fp->name );
			fp->flags |= FFL_IGNORE;					/* we cannot acces this, so don't do anything */
		}
		return;
	}

	tok = strtok_r( val, " \t", &strtok_p );	/* assume tokens are hostname:state  where state is rw, ro, i(gnore),  or u(nmount) */
	while( tok )
	{
		if( cp = strchr( tok, ':' ) )
		{
			*cp = 0;
			cp++;

			switch( *cp )
			{
				case 'r':		/* we expect rw or ro */
					if( *(cp+1) == 'w' )
					{
						op = tok;
						if( ! rw_count )
							state = ST_MOUNTRW;
						else
						{
							if( rw_count == 1 )	/* issue warning only on first */
							{
								ng_bleat( 0, "set_target: multiple r/w mount assignments in variable %s [WARN]", vname );
								ng_log( LOG_WARNING, "multiple r/w assignments for %s", fp->name ); 
							}
							state = ST_MOUNTRO;
						}

						rw_count++;
					}
					else
					{
						if( *(cp+1) != 'o' )
							ng_bleat( 0, "set_target: bad host assignment  %s:%s: assuming read/only mount: %s", tok, cp, fp->name );
						state = ST_MOUNTRO;
					}
					break;

				case 'u':			/* allow host:u  to specifically declare it unmounted */
					state = ST_UNMOUNT;
					break;

				case 'i':			/* allow host:ignore too */
					state = ST_UNMOUNT;
					break;

				default:
					/* ng_log( PRI_WARN, "bad state assignment for %s in %s", tok, vname ); */
					state = ST_UNMOUNT;
					ng_bleat( 3, "set_target: bad host assignment  %s:%s: assuming unmounted: %s", tok, cp, fp->name );
					break;
			}

			if( strcmp( tok, my_hname ) == 0 )	/* set the target state for our node */
			{
				fp->target_state = state; 	
				ng_bleat( 3, "set_target: target state for %s on %s is %s", fp->name, my_hname, ststring[state] );
				/*break; */
			}
		}

		tok = strtok_r( NULL, " \t", &strtok_p );
	}

	if( (fp->target_state == ST_UNMOUNT) && (fp->flags & FFL_PRIVATE) )
	{
		ng_bleat( 2, "set_target: unshared filesystem will be ignored: %s owner=%s", fp->name, op == NULL ? "unknown" : op );
		fp->flags |= FFL_IGNORE;					/* we cannot acces this, so don't do anything */
	}

	ng_bleat( 3, "set_target: fs=%s flags=%02x target-state=%d", fp->name, fp->flags, fp->target_state );
	ng_free( val );
	val = 0;
}

/* get the current remote state from narbalek vars 
   we set the remote state to the 'highest' state that any other node has reported.
*/
void get_state( Fsinfo_t *fp )
{
	char	vname[256];
	char	*tok;				/* at start of a token */
	char	*cp;				/* pointer into s token */
	char	*val = NULL;			/* at the narbalek var string */
	char	*strtok_p;			/* strtok_r pointer for reentrant safe stuff */
	char	**vlist;			/* list of state variables -- there is possibly one per host that has attached this puppy */
	int	fscount;
	int	i;
	int	state = ST_UNKNOWN;

	if( (vlist = load_fslist( fp->name, VT_STATE )) == NULL )		
		return;

	/*fp->local_state = ST_UNKNOWN;*/	/* we know what our state is -- only care about other nodes */
	fp->remote_state = ST_UNKNOWN;

	for( i = 0; vlist[i]; i++ ) 				/* var names are  <daemon-id>/<fs-name>:<host-name>.state */
	{
		if( (val = ng_nar_cache_get( nar_cache, vlist[i] )) != NULL )
		{
			if( (tok = strchr( vlist[i], ':' )) != NULL )
			{
				tok++;
				cp = strchr( tok, '.' );		/* at end of host name */
				*cp = 0;				/* tok now points at host name */
	
				if( strcmp( val, "mounted-rw" ) == 0 )
					state = ST_MOUNTRW;
				else
				if( strcmp( val, "mounted-ro" ) == 0 )
					state = ST_MOUNTRO;
				else
				if( strcmp( val, "unmounted" ) == 0 )
					state = ST_UNMOUNT;
				else
				if( strcmp( val, "unknown" ) == 0 )
					state = ST_UNKNOWN;
				else
					ng_bleat( 0, "unrecognised state for var %s.state: %s", vlist[i], val );
					
				if( strcmp( tok, my_hname ) != 0 )		/* some other node, keep most important state */
					if( fp->remote_state < state )		/* save the most important state */
						fp->remote_state = state; 
			}
	
			ng_free( val );
			val = 0;
		}
		else
			ng_bleat( 1, "no value in cache for var: %s", vlist[i] );
	}

	drop_fslist( vlist );
}

/* get the current state of the named filesystem -- according to the system (as opposed to 
   what narbalek variables have to say about things
*/
int get_sys_state( Fsinfo_t *fp )
{
	Sfio_t	*p;		/* pointer to sub process */
	char	buf[1024];
	char	*rbuf = NULL;		/* response */
	int	sstate = ST_UNKNOWN;
	
	sfsprintf( buf, sizeof( buf ), "ng_eqreq ck_state %s", SAFE_STR_D( fp->mtpt, ng_root) );
	
	if( (p = sfpopen( NULL, buf, "rw" )) != NULL )
	{
		if( (rbuf = sfgetr( p, '\n', SF_STRING )) != NULL )
		{
			ng_bleat( 2, "ck_state returned: %s is (%s)", fp->mtpt, rbuf );
			if( strcmp( rbuf, "unmounted" ) == 0 )
				sstate =  ST_UNMOUNT;
	
			if( strcmp( rbuf, "mounted-rw" ) == 0 )
				sstate =  ST_MOUNTRW;
	
			if( strcmp( rbuf, "mounted-ro" ) == 0 )
				sstate =  ST_MOUNTRO;
		}	

		sfclose( p );
	}

	return sstate;
}

void do_unmount( Fsinfo_t *fp )
{
	static 	int count = 0;		/* count for fstab name */
	char	fstab_name[1024];
	char	buf[1024];
	char	cmd[1024];

	ng_bleat( 2, "do_unmount: %s", fp->name );

	sfsprintf( buf, sizeof( buf ), "%s", fp->mtpt );
	switch( get_sys_state( fp ) )		/* what state does system show? */
	{
		case ST_UNMOUNT: 
			if( fp->flags & FFL_PURGED )						/* we already generated the fstab after this was marked unmount to remove entry */
				ng_bleat( 2, "do_unmount: fileystem already unmounted: %s", buf );
			else
			{
				fp->flags |= FFL_PURGED; 
				ng_bleat( 2, "do_unmount: filesystem (%s) seems unmounted, but may still be listed in fstab; fstab will be regenerated", buf );
				flags |= FL_GENTAB;

			}
			break;

		case ST_UNKNOWN:
			ng_bleat( 1, "attempting unmount: filesystem state is unknown: %s", buf );

		default:
			if( ! (fp->flags & FFL_NOACTION) )			/* allowed to take action on this fs */
			{

				sfsprintf( cmd, sizeof( cmd ), "ng_eqreq UMOUNT %s", buf );
				if( system( cmd ) )						/* !0 is bad from a shell script */
				{
					ng_bleat( 0, "unmount failed: %s [FAIL]", buf );
					return;
				}
				ng_bleat( 1, "filesystem successfully unmounted: %s", buf );
				fp->ua_lease = ng_now() + 3000;					/* we must announce the state for 5 min */
	
				sfsprintf( fstab_name, sizeof( fstab_name ), "%s/equerry.%d.ufstab", tmp_dir, count );
				gen_fstab( fstab_name );					/* generate the fstable to remove the unmounted one */
				sfsprintf( cmd, sizeof( cmd ), "ng_eqreq INSTALL_FSTAB %s", fstab_name );
                                if( system( cmd ) )
					ng_bleat( 0, "unable to remove fileystem from fstab: %s [FAIL]", buf );
				else
					ng_bleat( 1, "removed fileystem from fstab: %s [OK]", buf );

			}
			else
			{
				ng_bleat( 2, "do_unmount: no action allowed on: %s", fp->name );
				return;					/* dont change local state */
			}
			break;
	}

	fp->local_state = ST_UNMOUNT;			/* it is unmounted (or was already) set nar var to indicate this */
	set_var( fp, VT_STATE, "unmounted" );

	fp->verified = 1;				/* we set it so we know it to be true */
}

/* try to mount the fs using the target state as the desired mount state */
void do_mount( Fsinfo_t *fp )
{
	static	int count = 0;			/* counter for fstab filename */
	char	buf[1024];
	char	cmd[1024];
	char	fstab_fname[1024];
	int 	ss;				/* state system shows */

	sfsprintf( buf, sizeof( buf ), "%s", fp->mtpt );

	ss = get_sys_state( fp );		/* get current system state */
	if( ss == fp->target_state )		/* already at target state, set nar var and go */
		ng_bleat( 2, "do_mount: filsystem already mounted (%s): %s", ststring[ss], buf );
	else
	{
		if( !(fp->flags & FFL_NOACTION) )
		{
			fp->flags &= ~FFL_PURGED;			/* turn off purged flag as we add it back to fstab */

			ng_bleat( 1, "mounting %s target state=%s", fp->name, ststring[fp->target_state] );
			sfsprintf( fstab_fname, sizeof( fstab_fname ), "%s/equerry.%d.fstab", tmp_dir, count );
			gen_fstab( fstab_fname );
			sfsprintf( cmd, sizeof( cmd ), "ng_eqreq MOUNT %s %s %s", SAFE_STR_D( fp->label, fp->name), buf, fstab_fname );
			ng_bleat( 2, "mount command: %s", cmd );
	
			if( system( cmd ) )						/* !0 is bad from a shell script */
			{
				ng_bleat( 0, "mount failed: %s", buf );
				set_var( fp, VT_STATE, "unmounted" );			/* record as nar var */
				return;
			}
			ng_bleat( 1, "filesystem mounted: %s state=%s", fp->name, ststring[fp->target_state] );
		}
		else
		{
			ng_bleat( 2, "do_mount: no action allowed on: %s", fp->name );
			return;			/* abort here -- dont change local state */
		}
	}

	fp->local_state = fp->target_state;			/* now mounted (or already was before we got here) */
	set_var( fp, VT_STATE, ststring[fp->target_state] );
}

/* look at the current state, and the target state and, if possible, take action to move toward target
*/
void motivate( Syment *se, void *data )
{
	Fsinfo_t	*fp;
	char	vname[256];
	char	*tok;				/* at start of a token */
	char	*val;				/* at the narbalek var string */
	char	*strtok_p;			/* strtok_r pointer for reentrant safe stuff */
	int	ss;

	if( (fp = (Fsinfo_t *) se->value) == NULL )
		return;							/* protect against disaster */

	if( fp->flags & FFL_IGNORE )
		return;

	if( fp->target_state == fp->local_state )			/* we think things are as they should be */
	{
		if( !(flags & FL_FORCE_VERIFY)  &&  fp->verified )	/* if we believe nar var is right, and not time to force verification */
		{
			ng_bleat( 3, "target state met for %s: %s", fp->name, ststring[fp->target_state] );
			return;
		}
		else
		{					/* we have not verified that what the system knows is what we know */
			ss = get_sys_state( fp );	/* check system view and if nar var matches system view, then nothing to do */
			if( ss == fp->local_state )
			{
				ng_bleat( 2, "state verified: target state met for %s: %s", fp->name, ststring[fp->target_state] );
				fp->verified = 1;
				return;
			}
			ng_bleat( 2, "state verified: narbalek state (%s) does not match system state (%s), correcting: %s target=%s", ststring[fp->local_state], ststring[ss], fp->name, ststring[fp->target_state] );

			fp->local_state = ss;		/* adjust our view of the local state */
			set_var( fp, VT_STATE, ststring[ss] );
		}

	}

	get_state( fp );		/* get state as nar vars indicate */
	switch( fp->target_state )
	{
		case ST_MOUNTRW:
				if( fp->remote_state == ST_MOUNTRW )
					ng_bleat( 1, "cannot mount %s in r/w mode; remote system claims it has r/w mount",  fp->name );
				else
					do_mount( fp );
				break;

		case ST_MOUNTRO:
				do_mount( fp );
				break;

		case ST_UNMOUNT:
				do_unmount( fp );
				break;

		case ST_UNKNOWN:
				ng_bleat( 2, "determing local state: %s", fp->name );
				break;

		default:
				ng_bleat( 1, "dont understand value for target state %s: %d", fp->name, fp->target_state );
				break;
	}
}


/* called for each filesystem we have ever known about... pull up the current parms var for it
	and set the values in the fsinfo. 
	parms expected are type=fstype  opts=opt,opt,opt...  label=disklabel  mtpt=mountpoint-parentdir|mount=full-mountpt
	if label is omitted, name is assumed. if mtpt is omitted, $NG_LOCAL_MTDIR/name is assumed.
	label may be a name as appears on LABEL=foo in fstab, or can be the device name 
	if necessary. The device name makes for easy testing, and probably will not be used
	in the real world. 
*/
void get_parms( Syment *se, void *data )
{
	Fsinfo_t	*fp;
	char	vname[256];
	char	*tok;				/* at start of a token */
	char	*val;				/* at the narbalek var string */
	char	*strtok_p;			/* strtok_r pointer for reentrant safe stuff */

	if( (fp = (Fsinfo_t *) se->value) == NULL )
		return;					/* protect against disaster */

	if( fp->type )
		ng_free( fp->type );
	if( fp->opts )
		ng_free( fp->opts );
	if( fp->label )
		ng_free( fp->label );
	if( fp->mtpt )
		ng_free( fp->mtpt );
	if( fp->src ) 
		ng_free( fp->src );

	fp->type = fp->opts = fp->label = fp->mtpt =fp->src = NULL;

	fp->flags &= ~FFL_MDIRONLY;		/* turn off these flags in case nar var has changed */
	fp->flags &= ~FFL_IGNORE;		/* initial assumption is that it is live */

	
	sfsprintf( vname, sizeof(vname), "%s/%s:%s", did_string, fp->name, vtstring[VT_PARM] );
	if( (val = ng_nar_cache_get( nar_cache, vname )) == NULL )
		return;

	ng_bleat( 4, "get_parms: fs=%s got value: (%s)", fp->name, val );
	
	tok = strtok_r( val, " \t", &strtok_p );
	while( tok )
	{
		switch( *tok )
		{
			case 'U':
				if( strncmp( tok, "UNALLOC", 7 ) == 0 )			/* once was -- now dropped */
					fp->flags |= FFL_IGNORE;			/* we pay no attention, take no action */
				break;

			case 'P':
				if( strncmp( tok, "PRIVATE", 7 ) == 0 )		/* disk is private to the system that has name:rw */
					fp->flags |= FFL_PRIVATE;
				break;

			case 'R':
				if( strncmp( tok, "RESERVED", 8) == 0  )		/* place holder -- no meaning */
					fp->flags |= FFL_IGNORE;			/* we ignore */	
				break;

			case 'N':
				if( strncmp( tok, "NOACTION", 8 ) == 0 )
					fp->flags |= FFL_NOACTION;			/* do not mount/umount this one */
				break;

			case 'l':							/* label or device path */
				if( strncmp( tok, "label", 5 ) == 0 )
				{
					if( fp->label )
						ng_free( fp->label );
					fp->label = suss_value( tok );
				}
				break;

			case 'm':
				if( strncmp( tok, "mtpt", 4 ) == 0 )			/* old style - mount point parent dir we build /mtpt/name later */
				{
					if( fp->mtpt == NULL )				/* dont accept this if mount= supplied first */
					{
						fp->mtpt = suss_value( tok );
						fp->flags |= FFL_MDIRONLY;			/* flag the need to fix later */
					}
				}
				else
				if( strncmp( tok, "mount", 5 ) == 0 )			/* new style, full mount point name */
				{
					if( fp->mtpt != NULL )				/* we do want mount= to override if both supplied */
						ng_free( fp->mtpt );			
					fp->mtpt = suss_value( tok );
					fp->flags &= ~FFL_MDIRONLY;			/* no need to fix */
				}
				break;

			case 'o':
				if( strncmp( tok, "opts", 4 ) == 0 )			/* user supplied mount options */
				{
					if( fp->opts )
						ng_free( fp->opts );
					fp->opts = cat_value2( ",", tok, SPLIT );	/* we add lead comma here so that if str is empty %s%s in later printfs does right thing */
				}
				break;

			case 's':
				if(strncmp( tok, "src", 3 ) == 0 )
				{
					if( fp->src )
						ng_free( fp->src );
					fp->src = suss_value( tok );
				}
				break;

			case 't':
				if( strncmp( tok, "type", 4 ) == 0 )
				{
					if( fp->type )
						ng_free( fp->type );
					fp->type = suss_value( tok );
				}
				break;

			default:
				break;			/* we ignore things like ALLOC */
		}

		tok = strtok_r( NULL, " \t", &strtok_p );
	}

	if( fp->mtpt && fp->flags & FFL_MDIRONLY )			/* only have parent directory mtpt= rather than mount= */
	{
		char nbuf[1024];

		sfsprintf( nbuf, sizeof( nbuf ), "%s/%s", fp->mtpt, fp->name );		/* old style was to supply only the parent dir; we must smash here */
		ng_free( fp->mtpt );
		fp->mtpt = ng_strdup( nbuf );
	}

	if( !fp->mtpt )
	{
		char nbuf[1024];

		sfsprintf( nbuf, sizeof( nbuf ), "%s/%s", system_mtdir, fp->name );		/* if not supplied assume defaultdir/name is mount point */
		fp->mtpt = ng_strdup( nbuf );
	}

	if( (fp->flags & FFL_NOACTION) == 0 )		/* if we are allowed to mount/unmount, we must add noauto to opts */
	{
		char	*old;

		/* add noauto option; must have lead comma to make fstab record generation easier */
		if( (old = fp->opts) != NULL )					 
		{
			fp->opts = cat_value2( ",noauto", old, NO_SPLIT );	/* old options to ",noauto" keeping a leading comma (no split as some options might be n=val) */
			ng_free( old );
		}
		else
			fp->opts = ng_strdup( ",noauto" );		
	}

	if( ! fp->label )
		fp->label = ng_strdup( fp->name );

	if( val )
		ng_free( val );
	val = 0;

	ng_bleat( 4, "get_parms: after parse: %s flags=%02xx mount=%s ", fp->name, fp->flags, fp->mtpt );
}


/* 
	called now and again to actually do the chores that the daemon
	was started to do.
*/
void do_stuff( )
{
	char	**list;			/* list of variable names from narbalek */
	char	*nlist[MAX_FS];		/* list of fs names */
	int	i;
	ng_timetype now;

	memset( nlist, 0, sizeof( nlist ) );

	now = ng_now( );

	ng_bleat( 3, "do stuff starts" );
	if( (list = load_fslist( ".*", VT_PARM )) )		/* update our list of fsinfo blocks -- add, never delete */
	{
		if( verified_expiry < now )
		{
			verified_expiry = now + REVERIFY_TSEC;		/* reset lease */
			flags |= FL_FORCE_VERIFY;
		}

		for( i = 0; list[i] && i < MAX_FS-1; i++ )	/* convert to just fsnames */
		{
			nlist[i] = get_fsname( list[i] );	/* pull off just the fsname */
			get_fsinfo( nlist[i] );			/* ensure that there is an fs block in the hash for this puppy */
		}

		symtraverse( fshash, SPACE_FS, get_parms, NULL );		/* get current parms */
		symtraverse( fshash, SPACE_FS, set_target_state, NULL );	/* for each, lookup and set the target state */
		symtraverse( fshash, SPACE_FS, motivate, NULL );		/* comp target with current and do move toward getting to target*/

		if( flags & FL_GENTAB )						/* something in motivate turned this on and it stayed */
		{
			char	fstab_name[1024];
			char	cmd[1024];

			sfsprintf( fstab_name, sizeof( fstab_name ), "%s/equerry.fstab", tmp_dir );
			gen_fstab( fstab_name );					/* generate the fstable to remove the unmounted one */
			sfsprintf( cmd, sizeof( cmd ), "ng_eqreq INSTALL_FSTAB %s", fstab_name );
			system( cmd );

			flags &= ~FL_GENTAB;
		}

		drop_fslist( list );
		for( i = 0; nlist[i]; i++ )
		{
			ng_free( nlist[i] );
			nlist[i] = NULL;
		}

		flags &= ~FL_FORCE_VERIFY;			/* flag valid only for this iteration -- off it */
	}

	ng_bleat( 3, "do stuff ends" );
	return;
}


int main( int argc, char **argv )
{
	extern	int errno;
	extern	int ng_sierrno;

	int	init_tries = 10;
	int	status = 0;
	int	i;
	int	port = 0;
	ng_timetype now;
	char	buf[4096];			/* work buffer and work pointer */
	char	*p;	
	char	*port_str = NULL;
	char	*wdir_path = NULL;

	signal( SIGPIPE, SIG_IGN );		/* must ignore or it kills us */

	wdir_path = ng_rootnm( "site" );

	ARGBEGIN
	{
		case 'i':	SARG( did_string ); break;
		case 'p':	SARG( port_str ); break;
		case 'w':	SARG( wdir_path ); break;
		case 'v':	OPT_INC( verbose ); break;

usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND


	if( chdir( wdir_path ) != 0 )
	{
		ng_bleat( 0, "unable to switch to working directory: %s: %s", wdir_path, strerror( errno ) );
		exit( 1 );
	}

	ng_bleat( 0, "started: %s", version );
	ng_bleat( 1, "working directory is: %s", wdir_path );
	ng_free( wdir_path );
	wdir_path = NULL;

	ng_sysname( buf, sizeof( buf ) );
	my_hname = strdup( buf );
	if( p = strchr( my_hname, '.' ) )
		*p = 0;

	ng_root = ng_env( "NG_ROOT" );
	tmp_dir = ng_env( "TMP" );
	if( (system_mtdir = ng_env( "NG_SYSTEM_MTDIR" )) == NULL )
		system_mtdir = ng_strdup( "/l" );

	
	sfsprintf( buf, sizeof( buf ), "^%s/", did_string );
	ng_nar_cache_setsize( 8192 );
	nar_cache = ng_nar_cache_new( buf, 1200 );					/* force our narbalek vars to be cached locally */

	if( !port_str  && ((port_str = ng_pp_get( "NG_EQUERRY_PORT", NULL )) == NULL || !(*port_str)) )
	{
		ng_bleat( 0, "abort:  cannot find NG_EQUERRY_PORT and -p not supplied on command line" );
		exit( 1 );
	}

	fshash = syminit( 1777 );		/* small table to index filesystem info by */

	port = atoi( port_str );

	ng_sireuse( 1, 1 );						/* allow udp port reuse, and tcp address reuse */

	ng_bleat( 2, "initialising network interface for port %s", port_str );
	while( ng_siinit( SI_OPT_FG, port, port ) != 0 )		/* initialise the network interface */
	{
		init_tries--;
		if( init_tries <= 0  ||  errno != EADDRINUSE )
		{
			ng_bleat( 0, "abort: unable to initialise the network interface sierr=%d err=%d", ng_sierrno, errno );
			exit( 1 );
		}	

		ng_bleat( 0, "address in use (%I*d): napping 15s before trying again", Ii(port) );
		sleep( 15 );
	}

	ng_bleat( 1, "listening on port: %s", port_str );

	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    		/* register callbacks */
	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );    
	ng_sicbreg( SI_CB_CDATA, &cbtcp, NULL );
	ng_sicbreg( SI_CB_RDATA, &cbudp,  NULL );
	ng_sicbreg( SI_CB_SIGNAL, &cbsig, NULL );

	while( ok2run )
	{

		if( (now = ng_now( )) >= nxt_do )
		{
			if( ng_nar_cache_reload( nar_cache ) )
				do_stuff( );					/* do the chores this daemon was started for */
			else
				ng_bleat( 1, "equerry: narbalek cached data reload failed, no work done" );

			nxt_do = now + CYCLE_TSEC;
		}

		errno = 0;

		if( (status = ng_sipoll(  (nxt_do - now) * 10 )) < 0 )	/* poll for network and drive callbacks  -- wait max of time b4 nxt do */
		{
			if( errno != EINTR && errno != EAGAIN )
				ok2run = 0;
		}
	}

	while( sessions )
		drop_session( sessions->fd );

	ng_bleat( 1, "done. status=%d sierr =%d", status, ng_sierrno );
	return 0;
}             


#ifdef KEEP
/*
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_equerry:Filesystem mount management daemon)

&space
&synop	ng_equerry [-i daemon-id] [-p port] [-w work-dir] [-v]

&space
&desc	
	&This is a filesystem mount manager which attempts to ensure that filesystems are 
	either mounted nor not mounted on the local node.  
	The filesystems that &this is indended to manage are those that can easily be 
	moved between, or concurrently accessed from, multiple nodes in the cluster (e.g. iSCSI
	attached file systems).

&space
&subcat Narbalek Variables
	Narbalek variables are used to describe each filesystem in terms of mounting options, current state and 
	desired state.  The state descriptions are listed on a per host basis.  Three sets of these 
	variables are used by &this and are descirbed in the following sections. While the following descriptions
	provide the syntax of the various narbalek variables, the interface provided by &lit(ng_eqreq) allows these 
	variables to be created and managed without the need to know the syntax or to use the &lit(ng_nar_get) or
	&lit(ng_nar_set) commands. 
	

&subcat	Parameter Variables
	One variable per fliesytem will exist for each filesystem that is to be managed by &this. The variable names
	are of the form &lit(<id-string>/<fsname>^:parms.)  The value of these narbalek variables is a series of tokens
	that are either single keywords (e.g. ALLOC or UNALLOC) or keyword value pairs (e.g. label=rmfs0000).  These 
	tokens are currently recognised by &this:
&space
&begterms
&term	label= : Supplies the label that should be used when mounting the file system. The label needs to be supplied
	for a filesystem only if it differs from the filesystem name (fsname) used in the narbalek variable name. 
	If mounting a real device, and not by label, the device name can be supplied.
&space
&term	type= : Supplies the filesystem type and is required.
&space
&term	mount= : Supplies the full mountpoint path (e.g. /l/ng_site). If this option is missing, a default mountpoint 
	name is created using the value of &lit(NG_SYSTEM_MTDIR) and the filesystem name.  Omitting this parameter from 
	the narbalek variable is is no longer recommended.
	
&space
&term	mtpt= : 
	DEPECATED; maintained only to be backward compatable with narbalek variables created for versions earler
	than 2.2. Use mount.
&space
	Supplies the position in the file tree where the fileystem is to be mounted.  If not supplied, 
	&lit(NG_ROOT/fsname) is assumed. 
&space	
&term	opts= : Any mounting options, other than than read-only, read-write, and noauto (these are supplied automatically when needed). 
	The list supplied must be a comma separated list with no whitespace. 

&space
&term	src= : A single token that identifies the physical location (e.g. raidbox name) of the filesystem. Other than showing 
	when a list command is received, this data is not used. 
	
&space
&term	 NOACTION : This token indicates that &this is to manage the filesystem entry in &lit(fstab) but will not take
	any action with regard to mounting or unmounting the filesystem.  For these filesystems, the &lit(noauto) option 
	is &stress(omitted) from the fstab entry and thus it is expected that the operating system is to mount the file
	system at boot time.  This is generally used to allow equerry to include the management of fstab entries for file
	systems that must be mounted before it is realisticly possible for equerry to do so (NG_ROOT/site, NG_ROOT/tmp
	are examples of these kinds of filesystems).
&space
&term	PRIVATE : The filesystem is mounted on a disk that is internal to a spcecific node, or otherwise uncapable of 
	being accessed (mounted) by another system.  Any filesystems that have a common mount point name (e.g. /l/ng_fs00)
	&stress(MUST) be given this attribute, or it is likely that &this will attempt to unmount the filesystem on 
	all nodes thinking that another node is the 'owner' of the filesystem.

&space	
&term	ALLOC : Indicates that the fsname has been reserved. Not used by &this, but used to manage the addition of 
	new filesystems before they are actually incorporated into the environment. 

&space
&term	UNALLOC : Indicates that a once used filesystem name has been returned to the 'free pool' and may be assigned
	by the systems adminstration staff (using other equerry tools).
&space
&term	RESERVED : This keyword indicates that the filesystem name has been reserved. Until the parameters are added, 
	&this will ignore the filesystem. 
&endlist
&space

&subcat	Host Target State Variables
	One target state variable per filesystem name willl be allocated and is used to indicate to &this which nodes
	are to mount a filesystem and whether the mount should be read-only or read-write.  The variable names
	have the form: &lit(<id-string>/<fsname>^:host).  Values for these variables are a space separated list of 
	hostname and state pairs of the form: &lit(name^:state). Valid states are rw (read-write), ro (read-only), 
	or u (unmounted).  If the host name is removed from the list, &this will assume that the filesystem is to 
	be in an unmounted state. 

&subcat Current State Variables
	The current state of each filesystem is reported by each &this on each node.  In order to prevent collisions 
	in narbalek, each host/filesystem state is maintained as a single variable. Unlike the other two variable types
	which are likely to change infrequently enough not to worry about collisions, the current state variables may change
	more frequently and if all of the states were maintained using a single variable collisions could result in 
	inaccurate states. 

&space
	Therefore the state variable names contain a host name component and have the format: &lit(<id-string>/<fsname>^:state.<hostname>)
	The value of these variables is expected to be one of: mounted-rw, mounted-ro, or unmounted. 
	
&space
&subcat	Actions
	When &this detects that the target state for a given filesystem does not match the current state 
	of the filesystem as reflected in the narbalek variables, action will be taken to mount or unmount the 
	filesystem in order to cause the actual state to be the current state.  The value of the narbalek state variable
	is verified prior to attempting to cause a real change, and on a periodic basis the value of the state is 
	also verified to ensure that a filesystem has not been tinkered with manually.  The target state is calculated
	approximately once every minute, and the narbalek value of the current state is verified every three to five minutes. 

&space
&subcat Controlling The Daemon	
	The &lit(ng_eqreq) command can be used to communicate with &this.  The interface includes the ability to list the 
	perceived state of one or more filesystems, changing the verbose mode used for logging to the private log file (stderr), 
	and asking &this to gracefully terminate. The &lit(ng_eqreq) interface also provides an interface to maintain the 
	various narbalek variables that &this uses. 
	All of the commands that &this accepts are documented on the manual page for &lit(ng_eqreq.)

&opts   The following command line flags are recognised and alter the default behavour of &this.
&space
&begterms
&term	-i idstring : The identification string for this invocation of the daemon. &This expects
	all of its narbalek variables to begin with the string followed by a slant character (e.g. equerry/).
	If not supplied, equerry is used. 
&space
&term	-p port : Sets that port that &this will listen on.  If not supplied, the value of the 
	pinkpages variable &lit(NG_EQUERRY_PORT) is used. 
&space
&term	-v : Verbose mode.
&endterms
&space

&examp	

&see
	ng_eqreq
&space
&mods
&owner(Scott Daniels)
&lgterms
&term   06 Jul 2007 (sd) : Original code. 
&term	28 Feb 2008 (sd) : Added support for host:ignore as a mount state.
&term	04 Mar 2008 (sd) : Now sets a 2 in the fsck field of fstab entries.
&term	06 Mar 2008 (sd) : Changed some bleat levels to reduce the amount of junk at level 1.
&term	26 Mar 2008 (sd) : Increased table size for nar list.  (v1.2)
&term	29 Apr 2008 (sd) : We drop reporting unmounted state in narbalek.  Status of cache load now available from nar_if, so we check it. 
&term	06 Jun 2008 (sd) : Changes to allow for noaction management. 
&term	09 Jun 2008 (sd) : Changes to correct leaving the last filesystem in etc fstab if all are marked unmounted.
&term	15 Jul 2008 (sd) : Now checks for an empty port string from pp_get.
&term	14 Aug 2008 (sd) : When a filesystem is actually unmounted, equerry now sets the state for about 5 minutes in the narbalek variable
			rather than deleting the state variable straight away.  This allows a remote narblek waiting to mount the filesystem
			to see it go unmounted; we DONT assume that a missing variable means unmounted when we are waiting to mount it. 
			Also added PRIVATE flag support.
			(v3.0)
&term	26 Mar 2009 (sd) : Added a debugging messgae to help with issues when calling ng_eqreq to check filesytem state. (v3.1)
&term	13 Apr 2009 (sd) : Corrected bug that was leaving an fstab entry for a filesystem that was never successfully mounted and then 
			removed from the node. (V3.2)
&term	20 Apr 2009 (sd) : Fixed bug that prevented an option of the form x=y from being added correctly. (error=panic may now be 
			supplied as an option in the parameter list.
&term	09 Jun 2009 (sd) : Remved reference to deprecated ng_ext.h header file.
&endterms
&scd_end
*/
#endif
