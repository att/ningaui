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
Mnemonic:	rm_db_tcp
Abstract:	This is a daemon that manages the local (node) replication
		manager database. It listens on a wellknown port for 
		add, delete, fetch, rename and print commands.  The database
		is maintained in an in-core symbol table and dumpped to disk
		every few minutes.  The shell script rm_db_2tcp should be 
		used to interface with the database when being managed with 
		this.

		As of 09 Apr 2007 a new requirement needed to be supported:
			multiple basenames are allowed provided that they 
		have different md5 values. The original code deleted entries
		where the key (md5|base) was duplicated. This allowed, however,
		for two entries that represented a file with differing md5
		values which resided at the exact same path.  We now assume 
		that if we get a new filename md5 where the path is an exact 
		match for an exsiting entry, we will delete the reference to 
		the previous entry.  We must assume that the original file
		was overlaid without a delete being sent to us. 
	
		To accomplish this, file information is now kept in a struct
		which is chained in small basename chains.  The base-space
		name space in the symtab is used to access the head of the 
		chain which can be quickly searched for a matching path. The
		file info structs are directly accessed via the key symtab
		entries.  

Date:		1 April 2006 (Im no fool!)
Author:		E. Scott Daniels
---------------------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<sys/stat.h>

#include	<sfio.h>
#include	<ng_socket.h>
#include	<signal.h>

#include	<netinet/in.h>
#include	<ningaui.h>
#include	<ng_lib.h>

#include	<rm_dbd.man>


#define PTOKEN(a)	(a?a:"not-supplied")

#define	END_STRING	":end:"		/* string we send on the tcp session, with a trailing newline, to indicate end of data */
#define MAX_SESSIONS	128

					/* name spaces in symtab */
#define KEY_SPACE	1		/* md5|basename hash */
#define BASE_SPACE	2		/* basename only hash */

					/* types of records load_db() can handle */
#define RAW_RECS	0		/* load type is raw transaction records */
#define CKPT_RECS	1		/* load type is records from a ckpt file */

typedef struct session {	/* manages each session */
	int	fd;		/* file descriptor */
	void	*flow;		/* pointer to flow manager data used to parse tcp stream */
	char	*addr;		/* address of the other end */
} Session_t;

typedef struct finfo {
	struct finfo *next;		/* a string of these exists keyed by BASE_SPACE in symtab as multiple may exist with unique keys */
	struct finfo *prev;
	char	*key;			/* md5|base string KEY_SPACE symtab entries point to this string, BASE_SPACE point into this string at base */
	char	*path;			/* path of file on this node */
	char	*size;			/* we get it as a string, and do not do math on it, so we keep it as a string */
} Finfo_t;


Session_t *sessions[MAX_SESSIONS];	/* one for each session, indexed by file descriptor */

Symtab	*filedb = NULL;			/* the global thermo-nuclear all encompassing database */

char	*ckpt_fname = NULL;		/* checkpoint filename  from -c or NG_RMDB_CKPTFN */
char	*rollup_fname = NULL;		/* name of the file where we keep roll forward commands (ckpt_fn.rollup) */
Sfio_t 	*rollup = NULL;			/* access to the open file */
ng_timetype next_ckpt = 0;		/* time for next checkpoint */
char	*version = "v2.4/03288";

int	ok2run = 1;			/* turn off when time to exit -- allows for gracefull shutdown of tcp sessions */

int verbose = 0;


/* ------------ order eliminates need for all but a few prototypes ----------------*/
int parse_cmd( Session_t *sp, char *buf );

/* --------------------------------------------------------------------------------*/

/* send abort messages to stderr and to the log; then exit bad */
void abend( char *msg, char *data )
{
	extern int errno;

	ng_bleat( 0, "abort: %s: %s: %s", msg, data ? data : "", strerror( errno ) );
	ng_log( LOG_CRIT, "%s: %s: %s", msg, data ? data : "", strerror( errno ) );
	exit( 3 );
}

/* 	make key in user's buffer. Key is md5|filename where filename is the basename. This 
	allows us to keep only the path as the symtab value and reduces our memory footprint
	by the size of the all basenames.
*/
int mk_key( char *md5, char *fname, char *key, int klen )
{
	char *fp;

	if( *fname == '/' )
		fp = strrchr( fname, '/' ) +1;		/* allow a full path to come in */
	else
		fp = fname;

	return sfsprintf( key, klen, "%s|%s", md5, fp );	
}

/* 	look up a finfo chain based on the basename in fname and return the pointer to the finfo block
	if its path is a duplicate to the one in fname.  (we allow dup basenames as long as they 
	are in different paths. if we do not do this, we will keep two entries if the old entry is 
	not deleted and the new file, with a different md5, is placed in the same location as the old.)
*/
Finfo_t *find_dup_path(  char *fname )
{
	Syment 	*se;
	Finfo_t *fp = NULL;
	char	*base;		/* at basename */
	char	*path;		/* at path portion */


	path = ng_strdup( fname );
	if( (base = strrchr( path, '/' )) == NULL )
		return NULL;

	*base++ = 0;									/* make path into a string and point at base */
	if( (se =symlook( filedb, base, BASE_SPACE, NULL, SYM_NOFLAGS)) )		/* do we have a base reference? */
	{
		for( fp = (Finfo_t *) se->value; fp; fp = fp->next )
			if( strcmp( fp->path, path ) == 0 )
				break;					/* bingo, dupicate path for basename */
	}
	
	ng_free( path );

	return fp;
}

/* 	make a 'value' to save. The finfo block is pointed to by the syment that a
	key (md5|base) maps to.  the value contains the path and size of the file
	and is inserted into a list of finfo blocks that all share a common basename. 
	if the fname passed in has the same path as an existing finfo block, that 
	finfo block is reused; the old key and size are ditched first, and the base-space
	reference to the block (if it is the head of the chain) is adjusted. 
	The basename is NOT saved with the path; we save space by maintaining the basename 
	only in the key (experience has proven that some basenames are quite long and this 
	savings adds up when thousands of files are being managed.) 
	we separate md5 and basename in the key with vertical bar chars which should not be 
	found in a filename (we hope) 

	Before we create a new finfo block, we look for one on the base list that has the 
	EXACT path. If we find such a beast, we do the following:
		- del the sym entry that points at it (caller will make new)
		- del the sym entry in base-space if this is the head. necesssary because
			we replace the key which is used as the name in the symtab 
		- replace the key string
		- replace the size string
		- return the previously existing fp

	If we dont find one with an exact path match, then we create a new one.
*/
Finfo_t *mk_val( char *key, char *fname, char *size )
{
	char	buf[NG_BUFFER];
	char	*bp = NULL;			/* pointer at the start of the base name */
	Finfo_t *fp = NULL;
	Finfo_t *cp = NULL;			/* into the chain of finfo blocks */
	Syment	*se;
	char	*base;



	if( (fp = find_dup_path( fname )) != NULL )		/* overwriting without sending us a delete first! */
	{
		ng_bleat( 1, "mk_value: dup path found: new=(%s %s %s)   old=%x (%s:%s:%s)", key, fname, size, fp, fp->key, fp->path, fp->size );

		symdel( filedb, fp->key, KEY_SPACE );		/* dont reference old md5|base to this block as md5 may have changed */

		if( fp->prev == NULL )				/* if this is head, must alloc new base sym entry too */
		{
			bp = strchr( fp->key, '|' );
			symdel( filedb, bp+1, BASE_SPACE );	/* must delete before the key goes as base-space syment points at key */
		}

		ng_free( fp->key );				/* now that base is gone (if head) safe to trash key buffer */
		ng_free( fp->size );

		fp->key = ng_strdup( key );			/* overlay with new information and scoot */
		fp->size = ng_strdup( size );

		if( ! fp->prev )				/* once the new key is created, we replace the base-space entry */
		{
			bp = strchr( fp->key, '|' );		/* new pointer for the base hash entry */
			symlook( filedb, bp+1, BASE_SPACE, fp, SYM_NOFLAGS );
		}
		return fp;
	}

					/* no match -- create new and add it to the base chain */

	if( (bp = strrchr( fname, '/' )) != NULL )				/* chop name at last slash */
		*bp = 0;

	if( ! *fname || ! bp )			/* not legit to get a just basename here */
	{
		ng_bleat( 0, "cannot make value: fname is not full path: fname=%s bp=%s", fname ? fname : "null", bp ? bp : "null" );
		return NULL;	
	}

	fp = (Finfo_t *) ng_malloc( sizeof( Finfo_t ), "finfo block:mk_val" );
	memset( fp, 0, sizeof( Finfo_t ) );

	fp->key = ng_strdup( key );
	fp->path = ng_strdup( fname );
	fp->size = ng_strdup( size );

	if( bp )			/* fix user's buffer if we trashed it */
		*bp = '/';

	base = strchr( fp->key, '|' ) + 1;						/* point at basename of the key to save space */
	if( (se = symlook( filedb, base, BASE_SPACE, NULL, SYM_NOFLAGS)) )		/* do we have a base reference? */
	{
		for( cp = (Finfo_t *) se->value; cp->next != NULL; cp = cp->next );	/* find tail */
		cp->next = fp;								/* add at end */
		fp->prev = cp;
	}
	else
	{
		symlook( filedb, base, BASE_SPACE, fp, SYM_NOFLAGS);			
	}
	
	return fp;
}

/* throw away a file info struct; clean up/adjust the base space references  */
void ditch( Finfo_t *fp )
{
	Syment	*se;
	Finfo_t	*head = NULL;				/* new head if this is the first in the chain */
	char	*base;

	if( fp->prev || fp->next )			/* not the only one in the list */
	{
		if( fp->prev )					/* something before, chain past us */
			fp->prev->next = fp->next;
	
		if( fp->next )					/* something after, chain up, and if it is the new head change base reference */
		{
			fp->next->prev = fp->prev;
			if( !fp->prev )					/* new head; point base reference to it */
			{
				ng_bleat( 3, "ditch: repointing base to %s(key) %s(path) %(size)", fp->next->key, fp->next->path, fp->next->size );
				base = strchr( fp->next->key, '|' ) + 1;
				symdel( filedb, base, BASE_SPACE );
				symlook( filedb, base, BASE_SPACE, fp->next, SYM_NOFLAGS );  		/* base now references next in list */
			}
		}
	}
	else				/* only one, need to delete the base reference */
	{
		ng_bleat( 3, "ditch: trashing base for %s %s %s", fp->key, fp->path, fp->size );
		base = strchr( fp->key, '|' ) + 1;
		symdel( filedb, base, BASE_SPACE );
	}	

	ng_bleat( 3, "ditch: 0x%x(fp) %s(key) %s(path) %(size)", fp, fp->key, fp->path, fp->size );
	ng_free( fp->key );
	ng_free( fp->size );
	ng_free( fp->path );
	ng_free( fp );
}


/* write a human readable list of the whole basename chain -- indirectly invoked via traverse */
void dump_base_chain( Syment *se, void *data )
{
	Finfo_t *fp;
	Sfio_t	*of;

	if( data )
		of = (Sfio_t *) data;
	else
		of = sfstderr;

	if( (fp = (Finfo_t *) se->value) != NULL )
		sfprintf( of,  "%s\n", se->name );

	while( fp )
	{
		sfprintf( of, "\t%s %s %s\n", fp->key, fp->path, fp->size );
		fp = fp->next; 
	}
}

/* 	make a human readable buffer for the checkpoint file and dump/print output */
void mk_human( Finfo_t *fp, char *buf, int nbuf )
{
	char *base;		/* pointer to the basename in the key string */

	if( ! fp )
		return;

	*buf = 0;

	base = strchr( fp->key, '|' );

	if( buf && base )
	{
		*base = 0;			/* remember to fix these up later! */

		sfsprintf( buf, nbuf, "%s %s/%s %s", fp->key, fp->path, base+1, fp->size );	/* print md5 path/basename size */

		*base = '|';			
	}
}

/*	called indirectly by symtraverse to send the database onto the session 
	we send the entriy to the session pointed to by the session block (data) 
*/
void send_db( Syment *se, void *data )
{
	Session_t *sp;
	char	buf[NG_BUFFER];

	sp = (Session_t *) data;

	if( sp->fd >= 0 )
	{
		mk_human( se->value, buf, sizeof( buf ) );	/* convert to nice human format */
		if(  ng_siprintf( sp->fd, "%s\n", buf ) < 0 )		/* assume disco if bad send status */
		{
			ng_bleat( 1, "lost session during traverse: fd=%d dest=%s", sp->fd, sp->addr );
			sp->fd = -1;					/* and prevent any further attempts to send */
		}
	}
}


/* 	called indirectly by symtraverse to write the database to the open file 
	passed in as data (an sfio block).
	we write the entry pointed to by se.
*/
void write_db( Syment *se, void *data )
{
	Sfio_t	*ofile;
	char	buf[NG_BUFFER];

	ofile = (Sfio_t *) data;

	*buf = 0;
	mk_human( se->value, buf, sizeof( buf ) );	/* convert to nice human format */
	sfprintf( ofile, "%s\n", buf );
}

/* 	write a checkpoint file not more frequently than every 5 minutes
	once written, close and reopen the rollup file to clear it.
	if ckpt_fname is null, we skip checkpointing all together!
*/
void chkpt( )
{
	extern int errno;

	struct stat     s;

	ng_timetype now = 0;
	ng_timetype elapsed = 0;	/* used to compute time required */
	char	tcn[2048];		/* temporary checkpoint file name */
	char	bfn[2048];		/* backup chk pt filename */
	Sfio_t	*cf;

	now = ng_now( );

	if( ckpt_fname != NULL && now > next_ckpt )		/* time to do ? */
	{
		ng_bleat( 3, "starting checkpoint process" );

		sfsprintf( tcn, sizeof( tcn ), "%s.new", ckpt_fname );
		if( (cf = ng_sfopen( NULL, tcn, "w" )) == NULL )			/* write to *.new file */
			abend( "unable to open ckpt file", tcn );

		symtraverse( filedb, KEY_SPACE, write_db, (void *) cf );		/* hurl the information */
		if( sfclose( cf ) )
			abend( "write errors during creation of ckpt file", tcn );	/* no graceful way of dealing with this */

                if( stat( tcn, &s ) >= 0 )
		{
			if( s.st_size == 0 )			/* we had nothing to write, leave now, prevents zero len backup file */
			{
				ng_bleat( 1, "new checkpoint was empty; unlinking it" );
				unlink( tcn );
				return;
			}
		}


		sfsprintf( bfn, sizeof( bfn ), "%s.old", ckpt_fname );		/* move the old one to a backup spot */
		if( rename( ckpt_fname, bfn ) < 0 )
			abend( "unable to rename current ckpt file to",  bfn );

		if( rename( tcn, ckpt_fname ) < 0 )					/* slide the new one into place */
			abend( "unable to rename new ckpt file to",  ckpt_fname );

		sfclose( rollup );		/* close and remove the rollup file -- we don't care about error on this at this point */
		unlink( rollup_fname );
		if( (rollup = ng_sfopen( NULL, rollup_fname, "w" )) == NULL )
			abend( "unable to open rollup file", rollup_fname  );
		sfset( rollup, SF_LINE, 1 );					/* force a flush after each write */


		next_ckpt = now + 3000;			/* next one in 5 minutes */
		elapsed = ng_now( ) - now;
		ng_bleat( 2, "checkpoint successful: %s  (%I*ds)", ckpt_fname, Ii( elapsed ) );
	}
}

/* 	load stuff from an external file. file may be a checkpoint (human readable)
	format of md5 filename size (space separated tripples),  or may be raw records
	from a roll forward file (type = RAW passed in)
	return 1 if ok 
*/
int load_db( char *fname, int type )
{
	extern int errno;

	Sfio_t	*ifile;
	Syment	*se;

	char	*buf;
	Finfo_t	*val;
	char	key[NG_BUFFER];
	int	ntokens;	
	char	**tokens = NULL;
	long	count = 0;
	long	bad = 0;

	ng_bleat( 2, "loading starts from: %s", fname );

	if( (ifile = ng_sfopen( NULL, fname, "r" )) != NULL )
	{
		while( (buf = sfgetr( ifile, '\n', SF_STRING )) > 0 )
		{
			if( type == CKPT_RECS )
			{
				tokens = ng_tokenise( buf, ' ', '\\', tokens, &ntokens );
				if( ntokens >= 3 )
				{
					mk_key( tokens[0], tokens[1], key, sizeof( key ) );		
					if( (val = mk_val( key, tokens[1], tokens[2] )) != NULL )			/* make the 'value' */
					{
						if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) )
						{
							Finfo_t *dval = (Finfo_t *) se->value; 			/* holder so we can trash after symdel */

							symdel( filedb, key, KEY_SPACE );
							ditch( dval );
						}
	
						ng_bleat( 4, "load: %s(key) %s(path) %s(size)", val->key, val->path, val->size );
						symlook( filedb, val->key, KEY_SPACE, val, SYM_NOFLAGS );		/* add to key hash */
						count++;
					}
					else
						ng_bleat( 0, "load: parsing ckpt rec failed: %s(key) %s(t1) %s(t2)", key, PTOKEN(tokens[1]), PTOKEN(tokens[2]) );
				}
				else
				{
					ng_bleat( 0, "load: bad ckpt record: %s", buf );
					bad++;
				}
	
			}
			else				/* assume type is raw transaction records from roll forward */
			{
				parse_cmd( NULL, buf );
				count++;
			}
		}
		if( sfclose( ifile ) )
			abend( "unable to load, read errors on file: ", fname );
		ng_bleat( 2, "loading finished: %I*d records loaded, %I*d bad records in input", Ii( count ), Ii( bad ) );

		if( bad )
		{
			char dbuf[2048];
			sfsprintf( dbuf, sizeof( dbuf ), "unable to load from checkpoint: %d bad records", bad );
			abend( dbuf, NULL );
		}

		if( tokens )
			ng_free( tokens );
	}
	else
	{
		ng_bleat( 0, "unable to open load file: %s: %s", fname, strerror( errno ) );
		return 0;
	}

	return 1;
}


/*	log the transaction  in the roll up file */
void log_trans( char *buf )
{
	if( rollup )
		sfprintf( rollup, "%s\n", buf );
}

/*
	parse a command buffer received from session.  legit for sp to be NULL when processing a rollup file 
	return 0 if it seems to be a bogus command -- forces session disconn
*/
int parse_cmd( Session_t *sp, char *buf )
{
	extern	int errno;

	static	char	**tokens = NULL;	/* tokens from parsed buffer */
	int	ntokens;				/* number of tokens */
	int	ack=1;			/* some commands set no-ack to make things faster */
	char	*strtok_p;
	Finfo_t	*val;			/* value to put into the symbold table */
	char	key[NG_BUFFER];		/* build space for key: md5|basename */
	char	wbuf[NG_BUFFER];	/* for things like fetch that need to build something */
	Syment	*se;			/* entry in sym tab */
	Session_t	dummy;		/* dummy when called with a null sp */

	if( ! sp )			/* when called during the processing of a rollup file, sp does not exist; set a dummy for cleaner code */
	{
		sp = &dummy;
		sp->fd = -1;		/* si_printf() calls will not crash this way */
		sp->flow = sp->addr = NULL;
	}

	ng_bleat( 3, "parsing: %s", buf );

	tokens = ng_tokenise( buf, ':', '\\', tokens, &ntokens );	/* split buffer, and we count on the fact that buf is left unharmed */
	if( ntokens > 0 )
	{
		ng_bleat( 3, "parse_cmd: [0]=%s [1]=%s [2]=%s [3]=%s", PTOKEN(tokens[0]), PTOKEN(tokens[1]), PTOKEN(tokens[2]), PTOKEN(tokens[3]) );
		switch( *tokens[0] )
		{
			case 'A':			/* add but without ack -- assume they are doing a mass set */
				ack = 0;		/* turn off then fall into normal add stuff */
			case 'a':			/* add:md5:fname:size */
				if( strcmp( tokens[0], "add" ) == 0 || strcmp( tokens[0], "ADD" ) == 0 )
				{
					if( ntokens < 4 )
					{
						if( sp )
							ng_siprintf( sp->fd, "status: 1 FAIL add failed -- missing parameters\n%s\n", END_STRING );
				 		break;	
					}
					log_trans( buf );			/* save this transaction into the rollup log */

					mk_key( tokens[1], tokens[2], key, sizeof( key ) );	/* make key and value to put into symtab */
					if( (val = mk_val( key, tokens[2], tokens[3] )) != NULL )
					{
						if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) )
						{
							Finfo_t *dval = (Finfo_t *) se->value;
							symdel( filedb, key, KEY_SPACE );
							ditch( dval );
						}
		
						ng_bleat( 2, "add: %s(key) %s(path) %s(size)", val->key, val->path, val->size );
						symlook( filedb, val->key, KEY_SPACE, val, SYM_NOFLAGS );
						if( ack &&  sp->fd >= 0 )
							ng_siprintf( sp->fd, "status: 0 OK added: %s %s|%s\n%s\n", key, val->path, val->size, END_STRING );
					}
					else
						ng_siprintf( sp->fd, "status: 1 FAIL bad data: %s %s %s\n%s\n",  PTOKEN(tokens[1]), PTOKEN(tokens[2]), PTOKEN(tokens[3]), END_STRING );
				}	
				else
					return 0;
				break;

			case 'D':			/* dump to file */
				if( strcmp( tokens[0], "Dump" ) == 0 )
				{
					if( ntokens > 1 )
					{
						Sfio_t  *ofile;

						ng_bleat( 2, "dumping db to file: %s", PTOKEN(tokens[1]) );
						if( (ofile = ng_sfopen( NULL, tokens[1], "w" )) != NULL )
						{
							sfprintf( ofile, "key-space:\n" );
							symtraverse( filedb, KEY_SPACE, write_db, (void *) ofile );
							sfprintf( ofile, "base-space:\n" );
							symtraverse( filedb, BASE_SPACE, dump_base_chain, (void *) ofile );

							if( sfclose( ofile ) )
								ng_siprintf( sp->fd, "status: 1 FAIL error writing: %s: %s\n%s\n",  PTOKEN(tokens[1]), strerror( errno ), END_STRING );
							else
								ng_siprintf( sp->fd, "status: 0 OK created: %s\n%s\n",  PTOKEN(tokens[1]), END_STRING );
						}
					}
					else
						ng_siprintf( sp->fd, "status: 1 FAIL missing filename on request\n%s\n",  END_STRING );
				}
				else
					return 0;

				break;

			case 'd':			/* del:md5:filename[:junk] */
				if( strncmp( tokens[0], "del", 3 ) == 0 )
				{
					if( ntokens < 3 )
					{
						ng_siprintf( sp->fd, "status: 1 FAIL delete failed -- missing parameters\n%s\n", END_STRING );
				 		break;	
					}
	
					log_trans( buf );			/* save this transaction into the rollup log */
	
					mk_key( tokens[1], tokens[2], key, sizeof( key ) );
					if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) != NULL )
					{
						Finfo_t *fp = (Finfo_t *) se->value;
	
						ng_bleat( 2, "del: %s(key) %x(flags)", key, se->flags ); 
						symdel( filedb, key, KEY_SPACE );
						ditch( fp );				/* must do after symdel; symtab name is in sinfo */
	
						if( sp->fd >= 0 )
							ng_siprintf( sp->fd, "status: 0 OK deleted %s\n%s\n", key, END_STRING );
					}
					else
						if( sp->fd >= 0 )
							ng_siprintf( sp->fd, "status: 1 FAIL not found key=%s\n%s\n", key, END_STRING );
				}
				else
					return 0;
				break;

			case 'f':					/* fetch:md5:fname:junk */
				if( strcmp( tokens[0], "fetch" ) == 0 )
				{
					if( ntokens < 2 )
					{
						ng_siprintf( sp->fd, "status: 1 FAIL fetch failed -- missing parameters\n%s\n", END_STRING );
				 		break;	
					}

					mk_key( tokens[1], tokens[2], key, sizeof( key ) );
					if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) )
					{
						mk_human( se->value, wbuf, sizeof( wbuf ) );
						ng_siprintf( sp->fd, "%s\n", wbuf );
						ng_siprintf( sp->fd, "status: 0 OK\n%s\n", END_STRING );
					}
					else
						ng_siprintf( sp->fd, "status: 1 FAIL not found: key=%s\n%s\n", key, END_STRING );
		
				}
				else
					return 0;
				break;

			case 'L':					/* load from file */
				if( strcmp( tokens[0], "Load" ) == 0 && ntokens > 1 )
				{
					if( tokens[1] )
						if( load_db( tokens[1], CKPT_RECS ) )
							ng_siprintf( sp->fd, "status: 0 OK load from %s\n%s\n", PTOKEN( tokens[1] ), END_STRING );
						else
							ng_siprintf( sp->fd, "status: 1 FAIL load from %s\n%s\n", PTOKEN( tokens[1] ), END_STRING );
				}
				else
					return 0;
				break;

			case 'p':			/* print */
				if( strcmp( tokens[0], "print" ) == 0 )
				{
					ng_bleat( 3, "send of database starts..." );
					symtraverse( filedb, KEY_SPACE, send_db, (void *) sp );
					ng_bleat( 3, "...send of database complete" );
					if( sp->fd >= 0 )
						ng_siprintf( sp->fd, "%s\n", END_STRING );
				}
				else
					return 0;
				break;

			case 'P':			/* ping -- just send an end of data marker back */
				if( strcmp( tokens[0], "Ping" ) == 0 )
				{
					ng_bleat( 1, "ping: sending back end of data marker" );
					if( sp->fd >= 0 )
						ng_siprintf( sp->fd, "status: 0 OK ping\n%s\n", END_STRING );
				}
				else
					return 0;
				break;
				

			case 'r':							/* rename:md5:oldname:newname */
				if( strcmp( tokens[0], "rename" ) == 0 )
				{
					if( ntokens >= 4 )
					{
						char *val_size = NULL;			/* hold old value info */

						log_trans( buf );			/* save this transaction into the rollup log */
	
						mk_key( tokens[1], tokens[2], key, sizeof( key ) );
						if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) )	/* must exist to rename */
						{
							val = se->value;		
							ng_bleat( 2, "rename: %s(oldkey) %s(new) %s(path) %s(size)", key, tokens[3], val->path, val->size );
							val_size = val->size;		/* hold as it will be the same in new and is not passed in */
							val->size = NULL;		/* prevent free before we need it */

							symdel( filedb, key, KEY_SPACE );	/* del ref, and ditch will del base ref */
							ditch( val );				/* must do after symdel; symtab name is in sinfo */

							mk_key( tokens[1], tokens[3], key, sizeof( key ) );		/* key with new name */
							if( (val = mk_val( key, tokens[2], val_size )) != NULL )
							{
								if( (se = symlook( filedb, key, KEY_SPACE, NULL, SYM_NOFLAGS)) )
								{
									Finfo_t *dval = (Finfo_t *) se->value;
									symdel( filedb, key, KEY_SPACE );
									ditch( dval );
								}
		
								ng_bleat( 2, "rename: %s(key) %s(path) %s(size)", val->key, val->path, val->size );
								symlook( filedb, val->key, KEY_SPACE, val, SYM_NOFLAGS );
								ng_siprintf( sp->fd, "status: 0 OK renamed: %s %s|%s\n%s\n", key, val->path, val->size, END_STRING );
							}
							else
							{
								ng_bleat( 1, "rename: failed:  could not make value struct: %s %s", tokens[1], tokens[2] );
								ng_siprintf( sp->fd, "status: 1 FAIL rename: could not allocate struct\n", key, END_STRING );
							}
						}
						else
						{
							ng_bleat( 1, "rename: failed:  old name not found in db: %s", key );
							ng_siprintf( sp->fd, "status: 1 FAIL rename: cannot find old name (%s) in db\n%s\n", key, END_STRING );
						}
		
						if( val_size)
							ng_free( val_size );
					}
					else
						ng_siprintf( sp->fd, "status: 1 FAIL rename: invalid number of parameters\n%s\n", END_STRING );
				}
				else
					return 0;
				break;


			case 'v':			/* verbose */
				if( strcmp( tokens[0], "verbose" ) == 0 )
				{
					if( ntokens > 1 )
					{
						if( *tokens[1] == '+' || *tokens[1] == '-' )
							verbose += atoi( tokens[1] );
						else
							verbose = atoi( tokens[1] );
						if( verbose < 0 )
							verbose = 0;
						ng_siprintf( sp->fd, "status: 0 OK verbose level now: %d\n%s\n", verbose,  END_STRING );
					}
				} 
				else
					return 0;
				break; 

			case 'x':
				if( strcmp( tokens[0], "xit02031961" ) == 0 )		/* magic exit cookie */
				{
					symtraverse( filedb, BASE_SPACE, dump_base_chain, NULL );		/* hurl the information */
					ng_bleat( 0, "exit token received" );
					next_ckpt = 0;			/* force a checkpoint */
					chkpt( );
					ok2run = 0;
					return 1;
				}
				else
					return 0;
				break;
	
			default:
				ng_bleat( 1, "unrecognised command in buffer: %s", buf );
				return 0;
				break;
		}
	}

	chkpt( );			/* checkpoint if needed */
	return 1;
}

void release_session( int fd )
{
	Session_t	*sp;

	if( (sp = sessions[fd]) == NULL )
		return;

	if( sp->flow )
		ng_flow_close( sp->flow );

	if( sp->addr )
	{
		ng_bleat( 3, "release session fd=%d from %s", fd, sp->addr );
		ng_free( sp->addr );
	}
	else
		ng_bleat( 3, "release session fd=%d no IP address available", fd );

	ng_free( sp );
	sessions[fd] = NULL;
}

/* ------------------- socket call back functions ------------------------------ */
int cbconn( char *data, int fd, char *buf )
{
	Session_t	*sp;

	sp = (Session_t *) ng_malloc( sizeof( Session_t ), "cbconn: new session block" );
	memset( sp, sizeof( *sp ), 0 );
	sp->fd = fd;
	if( sessions[fd] )
		release_session( fd );		/* maybe disc did not get driven */

	sp->flow = ng_flow_open( NG_BUFFER );	/* establish streaming buffer parser */
	sp->addr = ng_strdup( buf );

	ng_bleat( 3, "connection received on fd=%d from %s", fd, buf );
	sessions[fd] = sp;

	return SI_OK;
}

int cbdisc( char *data, int f )
{
	release_session( f );
	return SI_OK;
}

int cbraw( void *data, char *buf, int len, char *from, int fd )
{
	static times = 1000;

	if( ++times > 1000 )
	{
		times = 0;
		ng_bleat( 0, "udp message(s) received -- this is not expected!" );
	}

	return SI_OK;
}

/* 
	buffer received from TCP/IP.  We register it with the flow manager and 
	then use the flow routines to get each newline separated record. 
	if parse_cmd() barfs (returns 0) then we force the session to disc. This 
	seems harsh, but as all messages come from rm_2dbd.ksh they should be well
	formed and a bad command is probably an attack of sorts. 
*/
int cbcook( void *data, int fd, char *buf, int len )
{
	Session_t *sp;
	char *b;		/* input buffer pointer */

	if( (sp = sessions[fd]) == NULL )
	{
	 	ng_bleat( 0, "TPC data received but no session block exists; fd=%d", fd );	
		return SI_OK;
	}

	ng_flow_ref( sp->flow, buf, len );		/* register the new buffer with the flow manager */

	while( (b = ng_flow_get( sp->flow, '\n' )) != NULL )      /* get each null separated buffer */
	{
		if( ! parse_cmd( sessions[fd], b ) )
		{
			ng_bleat( 0, "forcing session disconnect: %s buf=(%s)", sessions[fd]->addr, b );
			ng_siclose( fd );
			release_session( fd );
			return SI_OK;
		}
	}

	return ok2run ? SI_OK : SI_RET_QUIT;			/* graceful shutdown when quitting */
}

int cbsignal( void *data, int flags )
{
	return SI_OK;
}


void usage( )
{
	sfprintf( sfstderr, "version: %s\n", version );
	sfprintf( sfstderr, "usage: %s [-c ckpt-fname] [-l load_file] [-p port] [-r roll-file] [-v]\n", argv0 );
	exit( 1 );
}

int main( int argc, char **argv )
{
	extern int ng_sierrno;

	char	*loadf = NULL;		/* filename to load from */
	char	*port = NULL;
	char	*init_roll = NULL;	/* roll forward file from -r */
	int	status;
	char	buf[2048];


	signal( SIGPIPE, SIG_IGN );

	memset( sessions, sizeof( sessions ), 0 );

	port = ng_env( "NG_RMDB_PORT" );		/* if these are set in the environment then use them as defaults */
	ckpt_fname = ng_env( "NG_RMDB_CKPTNM" );		

	ARGBEGIN
	{
		case 'c':	
				if( ckpt_fname )
					ng_free( ckpt_fname );
				SARG( ckpt_fname ); 
				break;

		case 'l':	SARG( loadf ); break;

		case 'p':	
				if( port ) 
					ng_free( port ); 
				SARG( port ); 
				break;

		case 'r':	SARG( init_roll ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
				usage( );
				exit( 1 );
	}
	ARGEND

	if( ! port )
	{
		ng_bleat( 0, "ERROR: NG_RMDB_PORT not defined in environment and -p not used on command line; one is required" );
		exit( 2 ); 
	}

	filedb = syminit( 31223 );

	if( ckpt_fname )
	{
		rollup = ng_sfopen( NULL, ckpt_fname, "a" );		/* touch the checkpt file -- we fail if its not there later */
		sfclose( rollup );					/* if this fails we just fail later, so no error chk */

		sfsprintf( buf, sizeof( buf ), "%s.rollup", ckpt_fname );
		rollup_fname = ng_strdup( buf );				/* roll fowrward based on chkpt name */
		if( (rollup = ng_sfopen( NULL, rollup_fname, "w" )) == NULL )
			abend( "unable to initialise rollup file", rollup_fname  );
		sfset( rollup, SF_LINE, 1 );					/* force a flush after each write */
	}
	else
		ng_bleat( 0, "WARNING: checkpointing disabled: NG_RMDB_CKPTFN unset and  -c not supplied on cmd line" );

	/* must load and roll forward before opening the network door */

	if( loadf )
	{
		if( ! load_db( loadf, CKPT_RECS ) )
		{
			ng_bleat( 0, "abort: unable to do initial load from: %s", loadf );
			exit( 1 );
		}
	}

	if( init_roll )			/* inital roll forward file supplied */
	{
		ng_bleat( 0, "parsing roll forward file: %s", init_roll );
		if( ! load_db( init_roll, RAW_RECS ) )
		{
			ng_bleat( 0, "abort: unable to parse initial rollup file: %s", init_roll );
			exit( 1 );
		}

		chkpt( );			/* force a checkpoint with rolled data then trash the inital file */
		unlink( init_roll );
	}

	if( ng_siinit(  SI_OPT_FG, atoi( port ), 0 ) != 0 )		/* initialise the network interface */
	{
		sfprintf( sfstderr, "unable to initialise network interface: ng_sierrno = %d errno = %d\n", ng_sierrno, errno );
		exit( 1 );
	}
	ng_bleat( 2, "socket interface initialised ok" );

	

	ng_bleat( 2, "registering callback routines" );
 	ng_sicbreg( SI_CB_CONN, &cbconn, NULL );    		/* driven for connections */
 	ng_sicbreg( SI_CB_DISC, &cbdisc, NULL );   		/* driven for session disconnections */
 	ng_sicbreg( SI_CB_CDATA, &cbcook, (void *) NULL );	/* when cooked (tcp) data received */
 	ng_sicbreg( SI_CB_RDATA, &cbraw, (void *) NULL );	/* when udp data (raw) received -- we ignore */
 	ng_sicbreg( SI_CB_SIGNAL, &cbsignal, (void *) NULL );	/* register callback for signals */

	ng_bleat( 2, "invoking socket driver" );
	status = ng_siwait( );

	ng_bleat( status > 0 ? 0:1, "socket driver finished, status=%d", status );
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_dbd:Local Replication Manager Datbase Daemon)

&space
&synop	ng_rm_dbd [-c ckpt-fname] [-l load-fname] [-p port] [-r rollup-fname] [-v]

&space
&desc	&This manages the local file database for the replication manager.  The database
	is accessible via a TCP/IP interface using the ng_rm_db_2dbd script. There have 
	been several implementations of "ng_rm_db" which managed some form of disk 
	oriented database.  &This maintains the database in-core making access much
	quicker when systems are heavily loaded with I/O (Linux in particular). 
&space
	When started, &this reads its inital database from the load file supplied on 
	the command line. If a load file is not supplied, the database starts empty.
	If a rollup file is supplied on the command line, it is parsed and those 
	transactions are applied to the database.  The rollup file generally consists 
	of the add, delete and rename transactions that were received after the last 
	checkpoint was taken.  Once the two external files have been processed, if 
	supplied, the network interface is initialised and the daemon is ready to 
	receive and process requests.
&space
	Requests are sent via TCP/IP from the &lit(ng_rm_2dbd) script which accepts
	information from the command line and standard input as the older flavours of 
	&lit(ng_rm_db) did.  The information is converted into transactions and sent 
	to &this for processing.  Results are returned via the TCP/IP session to 
	&lit(ng_rm_2dbd) for presentation to the user. 
&space
	Transactions have the general syntax of:
&space
&litblkb
   command:data1:data2:data3
&litblke
	Where &lit(command) is one of the following:
&space
&begterms
&term	add : A record is added to the database.  Data 1 is the md5 value for the file, data2 is the fully quallified
	file pathname, and data3 is the size of the file in bytes. 
&space
&term	del : Causes the matching record to be deleted from the database.  Data1 is the md5 value, and data2 is the filename. 
	The filename may be either a fully qualified filename, or a basename.
&space
&term	fetch : Returns the information that is known about an md5 value, filename, pair. The md5 value, the fully qualified pathname, 
	and the filesize are returned. Data1 is the md5 value and data2 is the file's basename.
&space
&term	print : The print transaction causes information for each file in the database to be returned on the TCP/IP session. The 
	information returned is in md5value, filename, size order.  The order that the information is returned is completely 
	random.  Values for Data fields are ignored.
&space
&term	rename : The file in the database is renamed.  The new file is assumed to have the same directory path as it did before. 
	Data1 is the md5 value, data2 is the old filename, and data3 is the new file name. If either of the filenames is a 
	fully qualified pathname, the path portion will be ignored; no verification is made that the new file actually exists. 
&space
&term	verbose : Increase or decrease the verbosity level of the daemon. Data1 is either a +n or -n number which is added to the 
	current verbose setting. If the value does not have a lead +/- sign, the value is taken literally.  The verbose level 
	is not allowed to drop below zero.
&space
&term	ADD : Funcions the same as &lit(add), except that an acknowledgement message is not returned to the user. This is intended
	when adding files in bulk as the formatting and transmission of individual ack messages is very time consuming. When adding 
	files with this command, the list of transactions should be terminated with a PING transaction which will provide an ack 
	to the user programme.  The user programme can then dump the database and verify that all of the files in the list were added 
	correctly. 
&space
&term	DUMP : Like print, each file's information is formatted, but written to a local file instead of sent back to the 
	user on the TCP/IP session.  The datastore of file information is traversed from two perspectives: key mapping and basename
	mapping. Both are written to the named file. A dump of the basename namespace is also generated to the standard error file when &this is 
	stopped.  The data1 field contains the name of the file to write to. 
&space
&term	LOAD : Causes the file named by data1 to be loaded.  Records in the file are assumed to be newline terminated with 
	three space separated tokens on each record.  The tokens are: md5 value, file pathname, file size. 
&space
&term	
&endterms
&space
	All messages passed back from &this are newline terminated, ASCII, records. As a transaction might generate 
	multiple output messages, the end of data is marked with a record containing only the characters ":end:". 
	This string is easily recognised by programmes such as ng_sendtcp as the end of data marker and allow it to 
	continue with its processing. 

	Following each transaction, except ADD, a status message is returned to the user on the TCP/IP session. The 
	message has the format: STATUS: n {OK|FAIL} <message>, where &ital(n) is a zero indicating a good status and 
	any non-zero number to indicate an error. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c ckpt-fname : The name of the file that is to be used to write checkpoint data to. Two versions of the 
	checkpoint data are kept. The most recent is kept in &lit(ckpt-fname) and the previous checkpoint file is 
	moved to a file and given the name with a dash (-) character appended. 
	If the checkpoint filename is not supplied, no checkpointing is done. 
&space
	In addition to the checkpoint file, a roll-forward file is generated with each add, delete and rename 
	transaction that is recevied. The records in the file can be applied to the last checkpoint database to 
	bring the database to the point it was when &this was killed.  The name of this file is the same as the 
	checkpoint name given with this option with the string &lit(.rollup) appended. 
&space
&term	-l load-fname : The name of the file that should be loaded as the initial starting point database.  Generally
	this is the last checkpoint created, but may be any file that has the same format as a checkpoint file: 
	md5 value, file pathname, filesze.
&space
&term	-p port : The TCP/IP port number to listen on. If not supplied, &this assumes that the pinkpages variable &lit(NG_RMDB_PORT) 
	is defined and it will attempt to use that port for connection requests. 
&space
&term	-r roll-file : This file is assumed to have raw transaction messages that were captured either while &this was down,
	or after the last checkpoint file was created. 
&space
&term	-v : Verbose.  Each -v on the command line increases the verbosity of &this. 

&endterms


&space
&exit	Any exit code that is not zero indicates an error. 

&space
&see	ng_rm_2dbd, ng_repmgr, ng_rmbt

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Apr 2006 (sd) : Its beginning of time. 
&term	11 Apr 2006 (sd) : Prints elapsed time on ckpt bleat message.
&term	26 May 2006 (sd) : Checkpoint on exit was not always being driven. It is forced now.
&term	05 Jun 2006 (sd) : Corrected issue with usage string.
&term	16 Apr 2007 (sd) : Fixed memory leak in load_db. Made changes that ensure that a duplicate
		path does not result in two entries when the md5 values are different. (v2.0)
&term	26 Apr 2007 (sd) : Fixed cause of coredump when unrecognised command received. Added stronger checking
		to command parser.  (v2.1)
&term	03 May 2007 (sd) : Fixed bug in command parser that was returning bogus value when the exit command is received.
&term	22 May 2007 (sd) : Corrected potential cause of access storage after free (parse_cmd/add). 
&term	19 Jun 2007 (sd) : Fixed issue when renaming.  It was not releasing the base symtab entry which resulted in core dumps 
		(usually at shutdown).
&term	21 Mar 2008 (sd) : Now aborts if it finds any bad records in the checkpoint file (used to skip them). (v2.3)
&term	28 Mar 2008 (sd) : Unlinks the checkpoint 'new' file if it is empty. (v2.4)
&scd_end
------------------------------------------------------------------ */
