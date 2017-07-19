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
 Mnemonc:	nar_sym.c
 Abstract: 	Manages the symbol table stuff for narbalek.
 Date:		7 Mar 2003; renamed to nar_sym September 2004
 Author: 	E. Scott Daniels

 Mods:		01 Feb 2005 (sd) : Fixed calls to symdel not to assume space of 0 matches any space!
		18 Mar 2005 (sd) : cleaned up launder a bit.
		16 Nov 2006 (sd) : Converted sfopen() calls to ng_sfopen()
		27 Feb 2007 (sd) : Fixed memory leak in add comment (name in symtab)
		04 Jun 2007 (sd) : Added support for lumps and timestampping
		08 Jun 2007 (sd) : Added support for root merging
		11 Jun 2008 (sd) : Corrected a bleat statement to prevent null passed for %s
		17 Jul 2008 (sd) : Now set 0 timestamp when reading chkpt file
		15 Jan 2009 (sd) : Change > to >= such that we only accept a new value if the 
			current data's timesamp is < received data's. This was not a problem, but 
			means less data will be accepted and propigated during a merge.
		21 Jan 2009 (sd) : Discovered that using >= as documented above prevents the substitution
			value variable not to be set/reset.  This value is set internally during a 
			dump operation and then reset.  If the dump op takes less than 1s, then the 
			value is not reset to its previous value because the lump value is still 
			equal to the new value.  So much for reducing the merge traffic :)
		24 Mar 2009 (sd) : added a comment to clarify logic
 ----------------------------------------------------------------------

 chkpt file:
	<whitespace>@name=value
	<whitespace>%name=comment

----------------------------------------------------------------------
*/


#ifdef SELF_TEST
#include "ningaui.h"
#include "nar_const.h"
int match( char *str, char *pat );		/* declared here during self test */
#endif

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

#define SYM_TAB_SIZE	1001		/* prime please */

struct trav_info {		/* traverse info -- things needed when we traverse the symtab */
	Sfio_t *f;		/* fd to write to */
	int sid;		/* session to write to */
	int match;		/* check varname for pattern */
	int expand;		/* expand vars? */
	int new_fmt;		/* send in new format */
	int send_inact;		/* if set, we send inactive (empty) lumps as deletes */
};

static Symtab *st = NULL; 	  /* symbol table where things are mapped */

static char *expand( char *cb );

/* returns pointer to data in symtab - should be treated as read only buffer 
	if the lump is not active, we return null
*/
static Lump *lookup( char *what, int where )
{
	Lump *stuff = NULL;
	Syment *se;

	if( se = symlook( st, what, where, NULL, SYM_NOFLAGS ) )	
	{
		stuff = (Lump *) se->value;
		if( !(stuff->flags & LF_ACTIVE ) )		/* value was deleted (we keep for proper merging) so we return nothing */
			stuff = NULL;
	}

	return stuff;
}

static Lump *new_lump( char *name )
{
	Lump	*lp;

	lp = (Lump *) ng_malloc( sizeof( *lp ), "lump" );
	memset( lp, 0, sizeof( *lp ) );

	lp->name = ng_strdup( name );

	symlook( st, lp->name, LUMP_SPACE, lp, SYM_NOFLAGS );

	return lp;
}

/* trash the lump and the symtab entry that references it */
static void ditch_lump( char *name, Lump *lp )
{
	if( name )
		lp = lookup( name, LUMP_SPACE );

	if( !lp )
		return;

	symdel( st, lp->name, LUMP_SPACE );		

	if( lp->data )
		ng_free( lp->data );

	if( lp->comment )
		ng_free( lp->comment );

	if( lp->name )
		ng_free( lp->name );

	ng_free( lp );
}

/* fetch a value -- caller should treat as read only */
static char *lookup_value( char *name )
{
	Lump *lp;

	if( (lp = lookup( name, LUMP_SPACE )) != NULL )
		return lp->data;

	return NULL;
}

/* we change the comment based on the timestamp that the comment caries (ts parm) and the 
   timestamp in the lump, but we do not update the lump timetamp as that it reflects
   the last update of the data only.
	returns 1 if we added/replaced the comment; 0 if it was old and ignored
*/
static int add_comment( time_t ts, char *name, char *comment )
{
	Syment	*cse = NULL;
	Lump	*lp = NULL;

	if( (cse = symlook( st, name, LUMP_SPACE, NULL, SYM_NOFLAGS ))  )		
	{
		lp = (Lump *) cse->value;
		if( !lp || ts < lp->ts )		
			return 0;

		if( lp->comment )
			ng_free( lp->comment );
		lp->comment = NULL;
	}
	else
		return 0;

	if( comment && *comment )
	{
		if( ! lp )
			lp = new_lump( name );

		lp->comment = ng_strdup( comment );
		ng_bleat( 3, "comment added: %s=%s", name, comment );	
	}
	else
		ng_bleat( 3, "comment deleted for: %s", name );

	return 1;
}

/*	return 1 if we made a change; return 0 if symtab data was newer (delayed message) 
*/
static int add_value( time_t ts, char *name, char *value )
{
	Syment	*se = NULL;
	Lump	*lp = NULL;

	if( !name || *name == 0 )	/* immediate end of string is not good */
	{
		ng_bleat( 0, "value not added, null or empty name; value=%s", value ? value : "null value" );
		return 0;
	}

	if( se = symlook( st, name, LUMP_SPACE, NULL, SYM_NOFLAGS ) )			/* find existing */
	{
		lp = (Lump *) se->value;

		if( lp->ts > ts )	/* value we have is more recent, toss this; dont use >= or internally modified vars might not be reset */
		{
			ng_bleat( 2, "old value ignored: %I*d(lump) %I*ds(ts) %s(name) %s(value)", Ii(lp->ts), Ii(ts ),  name, value ? value : "" );
			return 0;
		}
	}
	else
		lp = new_lump( name );		/* must get, even for a 'delete' as we track those too */

	if( value && *value )			/* null value/null string means we inactivate it */
	{
		if( lp->data )
			ng_free( lp->data );

		lp->flags |= LF_ACTIVE;				/* lump is active and can be listed */
		lp->data = ng_strdup( value );
		lp->ts = ts;
		ng_bleat( 3, "value added: %I*d %s=(%s)", Ii(lp->ts), lp->name, lp->data );	
	}
	else
	{
		ng_bleat( 2, "variable inactivated: %s", name );	
		lp->flags &= ~LF_ACTIVE;			/* deactivate lump -- keep it, but dont include in lists/gets */
		lp->ts = ts;					/* if we get an update that was earlier than the delete we dont add it back */
		if( lp->data )
			ng_free( lp->data );
		lp->data = NULL;
	}

	return 1;
}

/* called via symtraverse to delete all existing entries */
static void launder( Syment *this, void *np )
{
	Lump *lp;

	lp = (Lump *) this->value;				/* capture before we let the symtab entry go */

	if( lp )
		ditch_lump( NULL, lp );				/* will trash this entry */
	else
		symdel( st, this->name, this->space );		/* unlikely, but ensure sym data is trashed */
}

/* called via symtraverse to prune the table all inative entries */
static void prune_entry( Syment *this, void *np )
{
	Lump *lp;
	time_t *now;

	lp = (Lump *) this->value;			/* capture before we let the symtab entry go */
	now = (time_t *) np;

	if( lp )
	{
		if( !(lp->flags & LF_ACTIVE) && ((lp->ts + PRUNE_AFTER_SEC) < *now) )
		{
			ng_bleat( 2, "inactive variable pruned: %s age=%I*d", lp->name, sizeof(*now), (time_t) ((lp->ts + PRUNE_AFTER_SEC)-*now) );
			ditch_lump( NULL, lp );				/* will trash this entry */
		}
	}
	else
		symdel( st, this->name, this->space );		/* unlikely, but ensure sym data is trashed */
}

/* invoke traverse to clear old inactive entries */
static void prune( )
{
	time_t now;

	now = time( NULL );			/* lump time is zulu time so we dont have to deal with tz */

	symtraverse( st, LUMP_SPACE, prune_entry, (void *) &now );		/* clean the table before reload */
}

/* called via symtraverse to prune the table all inative entries */
static void examine_entry( Syment *this, void *np )
{
	Lump *lp;

	lp = (Lump *) this->value;			/* capture before we let the symtab entry go */

	if( lp )
	{
		ng_bleat( 0, "exam: lump: %I*d %s %s=(%s) (%s)", Ii(lp->ts), lp->flags & LF_ACTIVE ? "active" : "inact ", lp->name, lp->data ? lp->data : "NO-VALUE", lp->comment ? lp->comment : "NO-COMMENT" );
	}
}

/* invoke traverse to examine our environment and dump some stats */
static void examine( )
{

	ng_bleat( 0, "exam: verbose level=%d  version =%s", verbose, version );
	symtraverse( st, LUMP_SPACE, examine_entry, NULL );		
}

/*	called by symtraverse to make a 'cartulary' entry of the form:
	name="value" [# comment]
	we never include inactive lumps in a cartulary listing
*/
static void mk_centry( Syment *this, void *np )
{
	char	obuf[NG_BUFFER];
	char	*eb = NULL;				/* value after expansion */
	char	*oeb = NULL;
	int	*fd;				/* output descriptor */
	char	*comment = NULL;
	int	len;
	int 	depth = 0;
	struct	trav_info *tinfo;
	Lump	*lp;

	if( ! np  || ! this )
		return;

	tinfo = (struct trav_info *) np;

	if( tinfo->match && match( this->name, NULL ) != 0 )		/* var name did not match the pattern */
		return;

	if( (lp = (Lump *) this->value) )
	{
		if( lp->data == NULL  || !(lp->flags & LF_ACTIVE) )		/* no data, or lump is inactive, we dont include in the list */
			return;		
		
		if( tinfo->expand )
		{
			eb = expand( lp->data );
			while( depth++ < 10 && strchr( eb, '$' ) )
			{
				oeb = eb;
				eb = expand( eb );
				ng_free( oeb );
			}
		}
		else
			eb = ng_strdup( lp->data );	/* no more expensive than expansion, and keeps code simple later */
	}
	else
		return;				/* should not happen, but bail out if it does */

#ifdef SELF_TEST
	printf(  
#else
	ng_siprintf( tinfo->sid, 
#endif
		/*"%s=\"%s\"%s%s\n", this->name, eb, lp->comment ? "\t#" : "", lp->comment ? lp->comment : "" );*/
		"%s=\"%s\"\t# %s %I*d\n", this->name, eb, lp->comment ? lp->comment : "", Ii(lp->ts) );
	ng_free( eb );
}


/* called by symtraverse to make a checkpoint file entry 
	checkpoint entries are placed onto disk for recovery, and are also used to load
	a new child narbalek. If the new_fmt flag in the trav_info struct is set, then 
	we create the checkpoint entry with the timestamp when sending to a mate.
*/
static void mk_cptentry( Syment *this, void *t )
{
	char	obuf[NG_BUFFER];
	char	*comment = NULL;
	struct trav_info *tp;
	Lump 	*lp;

							/* prevent accidents tho these should not happen */
	if( (tp = (struct trav_info *) t) == NULL )
		return;						

	if( (lp = (Lump *) this->value) == NULL ) 
	 	return;

	if( !tp->send_inact && (lp->data == NULL || !(lp->flags & LF_ACTIVE)) )		/* dont send inactive (deleted) values unless asked */
		return;

	if( tp->new_fmt )
		sfsprintf( obuf, sizeof( obuf ),  "@@%I*d %s=%s", Ii(lp->ts), this->name, lp->data ? lp->data : "" );		
	else
		if( lp->data )
			sfsprintf( obuf, sizeof( obuf ),  "@%s=%s", this->name, lp->data );		/* old style may still be used for disk files */

	if( tp->f )			
	{
		sfprintf( tp->f, "%s\n", obuf );
		if( lp->comment )
			if( tp->new_fmt )
				sfprintf( tp->f, "%%@%I*d %s=%s\n", Ii(lp->ts),  this->name, lp->comment );	/* %@time name=value */
			else
				sfprintf( tp->f, "%%%s=%s\n", this->name, lp->comment );		/* old format:  %name=value */
	}
	else
	{
		if( tp->sid >= 0 )
		{
			ng_siprintf( tp->sid, "%s\n", obuf );
			if( lp->comment )
				ng_siprintf( tp->sid, "%%%s=%s\n", this->name, lp->comment );	
		}
	}
}


/* split src (<whitespace>name=value  into name and value  components */
static void getnv( char *src, char *name, char *value )
{
	char *sep;		/* pointer at seperator (=) in the "equasion" */
	char *w;		/* working copy */

	for( ; *src && isspace( *src ); src++ );  	/* skip whitespace before name */

	*name = 0;		/* set end of string mark in case we cannot do it */
	*value = 0;

	if( (sep = strchr( src, '=' )) )		/* find first = */
	{
		strncpy( name, src, sep - src );
		name[sep-src] = 0;
		strcpy( value, sep + 1 );
	}
}

/* expand the variable references in caller's buffer cb; leaves original buffer unharmed */
static char *expand( char *cb )
{
	char	*eb;		/* buffer expanded in */
	int	ebi;		/* index into expansion */
	int	neb = 0;	/* number bytes allocated to eb */
	char	*src;		/* pointer to source for copies */
	char	*cbi;
	char	vname[100];	/* variable name to look up */
	char 	defstr[100];	/* default (${name:-default}) */
	int	i;
	int 	skip;		/* flag to indicate ${...} format and to skip to closing } */
	int 	doit = 1;	/* flag: if !0 then we expand variables; turned off if in ' */ 
	
	ebi = 0;
	eb = NULL;
	cbi = cb;

	while( *cbi )
	{
		if( ebi > neb - 10 )	/* need more room */
		{
			neb += 1024;
			eb = ng_realloc( eb, neb, "narbalek: expansion buffer (1)" );
		}

		skip = 0;

		switch( *cbi )			/* switch entered with room for at least one chr into the expand buf */
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
					for( cbi++; *cbi  && (isalnum( *cbi ) || *cbi == '_'); cbi++ )
						vname[i++] = *cbi;

					*defstr = 0;
					if( skip )	/* if we found ${ check for :- and get defstr if it is */
					{
						if( *cbi == ':' )
							if( *(++cbi) == '-' )	/* ${name:-default} */
							{
								cbi++;
								for( i = 0; *cbi && *cbi != '}'; i++ )
									defstr[i] = *cbi++;
								defstr[i] = 0;
							}
							else
								for( ; *cbi && *cbi != '}'; cbi++ );
						cbi++;
					}
	
					vname[i] = 0;
					if( !(src = lookup_value( vname )) )
					{
						if( *defstr )
							src = defstr;			/* have to use default */
						else
						{
							eb[ebi++] = '$';
							src = vname;		/* leave $name in the output */
						}
					}
	
					if( ebi > neb - strlen( src ) )		/* need more room */
					{
						neb += 1024;
						eb = ng_realloc( eb, neb, "nar_sym: expansion buffer (2)" );
					}

					while( *src )
						eb[ebi++] = *src++;	/* copy in the expansion */
				}
				else
					eb[ebi++] = *cbi++;

				break;

			case '"':	cbi++; 				/* we ignore quotes */
					break;

			case '\\':	*cbi++; 			/* skip  it, and fall into saving next one too */
			default:	eb[ebi++] = *cbi++; 
					break;
		}		
		
		eb[ebi] = 0;
	}

	return eb;
}

/*	called by nar_load, or pouch_load to insert a value/comment from file or other 
	narbalek into the table. With version 3, we now may find an optional timestamp
	after the @ or % lead character. The optionality (if that is a word) allows 
	us to mix v3 narbaleks into an environment still running v2 flavours as it might 
	not be possible to take them all down at the same time. 

	return: 1 if we added the value, 0 if it was ignored (old)
*/
static int nar_load_rec( time_t ts, char *buf )
{
	char	name[1024];
	char	val[2048];
	char	*data;
	int	stat = 0;

	while( isspace( *buf ) )	/* skip leading spaces */
		buf++;

	if( *(buf+1) == '@' )		/* %@time or @@time */
	{
		data = strchr( buf, ' ' ) + 1;
		ts = strtoll( buf+2, NULL, 10 );			/* use the one supplied in the record */
		ng_bleat( 3, "load_rec: v3 message: %s", buf );
	}
	else
		data = buf + 1;		/* @name=value or %name=comment */

	if( data == NULL || *data == 0 )
		return 0;				/* prevent accidents */

	switch( *buf )
	{
		case 0: break;			/* end of string -- not expected, but ok */

		case '%':			/* comment for a variable */
			getnv( data, name, val );
			stat = add_comment( ts, name, val );					/* add or replace */
			break;

		case '#':			/* comment -- ignore */
			break;

		case '@':
			getnv( data, name, val  );
			stat = add_value( ts, name, val );					/* add or replace */
			break;
			
		default:
			getnv( buf, name, val  );					/* unknown, maybe a version 1 format? */
			stat = add_value( ts, name, val );					/* add or replace */
			break;
	}

	return stat;
}

/* load the symtab from the named file */
static int nar_load( char *fn )
{
	char	*buf;			/* points at input buffer */
	char 	name[1024];		/* variable name */
	char 	val[1024];		/* what it was set to in cartulary */
	Sfio_t	*f;
	time_t	ts;

	ts = 0;			/* if record does not have timestamp, then default to 0 to allow any running narbalek's to be more recent */

	if( f = ng_sfopen( NULL, fn, "r" ) )
	{
		if( ! st )
			st = syminit( SYM_TAB_SIZE );		/* establish the symbol table */
		else
		{
			symtraverse( st, LUMP_SPACE, launder, NULL );		/* clean the table before reload */
		}

		ng_bleat( 1, "loading from %s", fn );
		while( (buf = sfgetr( f, '\n', SF_STRING )) )
			nar_load_rec( ts, buf );			/* put it in */

		if( sfclose( f ) )
		{
			ng_bleat( 0, "nar_sym/load: read failure: %s: %s", fn, strerror( errno ) ); 
			return 0;
		}
	}
	else
	{
		ng_bleat( 0, "nar_sym/load: cannot open %s: %s\n", fn, strerror( errno ) );
		return 0;
	}

	return 1;
}

/* add things from a recovery file
	syntax:
		junk {value|comment}<whitespace>name=stuff
*/
static int nar_recover( char *fn )
{
	char	*buf;			/* points at input buffer */
	char	*p;			/* pointer at token */
	char 	name[1024];		/* variable name */
	char 	val[1024];		/* what it was set to in cartulary */
	char 	*exval;
	Sfio_t	*f;

	if( ! fn )
		return 1;

	if( f = ng_sfopen( NULL, fn, "r" ) )
	{
		if( ! st )				/* not likely, but dont barf */
			st = syminit( SYM_TAB_SIZE );		/* establish the symbol table */

		ng_bleat( 1, "recovering from %s", fn );
		while( (buf = sfgetr( f, '\n', SF_STRING )) )
		{
			if( (p = strchr( buf, '=' )) != NULL )
			{
				while( p > buf && ! isblank( *p ) )
					p--;

				getnv( p+1, name, val );
				while( p > buf && isblank( *p ) ) p--;	/* find end of either comment or value token */
				
				switch( *p )				/* looking at last ch of first token t=comment e=value */
				{
					case 0: break;			/* end of string -- not expected, but ok */
	
					case 't':			/* comment for a variable */
						ng_bleat( 1, "recover/comment: %s(name) %s(val)", name, val );
						symlook( st, name, LUMP_SPACE, strdup( val ), SYM_COPYNM );
						break;

					case 'e':
						ng_bleat( 1, "recover/value: %s(name) %s(val)", name, val );
						symlook( st, name, LUMP_SPACE, strdup( val ), SYM_COPYNM );
						break;

					default:
						ng_bleat( 2, "unrecognised recovery record: (%s)", buf );
						break;
				}
			}
		}

		if( sfclose( f ) ) 
		{
			ng_bleat( 0, "nar_sym/recover: read error: %s: %s\n", fn, strerror( errno ) );
			return 1;
		}
	}
	else
	{
		ng_bleat( 0, "nar_sym/recover: cannot open: %s: %s\n", fn, strerror( errno ) );
		return 0;
	}

	return 1;
}


/* create a checkpoint file 
	with v3 we introduced a timestamp; if the new_fmt flag is set, then generate a timestamp with the data
*/
static int mk_ckpt( char *fname, int sid, int new_fmt, int send_inact )
{
	int state = 0;
	struct trav_info tinfo;

	tinfo.f = NULL;
	tinfo.expand = 0;
	tinfo.sid = -1;
	tinfo.new_fmt = new_fmt;
	tinfo.send_inact = send_inact;

	if( fname )
	{
		ng_bleat( 2, "mk_ckpt: saving chkpoint in: %s", fname );
		if( tinfo.f = ng_sfopen( NULL, fname, "w" ) )
		{
			symtraverse( st, LUMP_SPACE, mk_cptentry,  (void *) &tinfo );	
			state = sferror( tinfo.f );

			if( sfclose( tinfo.f ) )
			{
				ng_bleat( 0, "nar_sym/mk_ckpt: write error: %s: %s", fname, strerror( errno ) ); 
				return 1;
			}
		}
		else
			ng_bleat( 0, "nar_sym/mk_ckpt: cannot open %s: %s\n", fname, strerror( errno ) );
	}
	else
	{
		tinfo.sid = sid;
		ng_bleat( 2, "mk_ckpt: sending chkpoint on session: %d", sid );
		symtraverse( st, LUMP_SPACE, mk_cptentry, (void *) &tinfo );
		ng_siprintf( sid, "Tendofload:\n" );		/* indicate they are loaded */
	}

	return !state;
}


/* send the symbol table in the form of cartulary entries to a tcp session (fd) */
static void send_table( int fd, char *pat, int expand_flag )
{
	struct trav_info tinfo;

	if( pat && *pat )		/* not an immediate end of string */
		tinfo.match = 1;
	else
		tinfo.match = 0;

	tinfo.sid = fd;
	tinfo.f = NULL;
	tinfo.expand = expand_flag;
	tinfo.send_inact = 0;

	if( fd )
	{
		if( pat )
			match( NULL, pat );			/* set up regex stuff */
		symtraverse( st, LUMP_SPACE, mk_centry, (void *) &tinfo );	
	}
}


/* if allowed, send our stuff to the root to merge our tree into theirs */
static void merge_data( char *node )
{
	int	sid = -1;
	char	addr[1024];

	if( ! flags & FL_ALLOW_MERGE )
		return;

	sfsprintf( addr, sizeof( addr ), "%s:%s", node, port_str );
	ng_bleat( 1, "merge_data: sending our values to %s", addr );
	if( node && (sid = ng_siconnect( addr )) >= 0 )		/* open a tcp session to the elder root */
	{
		mk_ckpt( NULL,  sid, 1, 1 );		/* send to mate in v3 chkpt format, include inactive lumps for delete */
	}
	else
		ng_bleat( 0, "merge_data: invalid node name, or unable to connect to %s", node ? node : "null-name" );

	ng_siclose( sid );
}



/* ---------------------------------------------------------------------------------------------*/
/* dumps values for the cartulary vars entered on the command line */
#ifdef SELF_TEST
int match( char *str, char *pat )
{
	return 1;
}

void scan_tab( char *str )
{
	int z = 0;
	int nz = 0;
	int i;
		for( i = 0; i < SYM_TAB_SIZE; i++ )
			if( st->syms[i] )
			{
				/*printf( "[%d] = %x  next=%x\n", i, st->syms[i], st->syms[i]->next ); */
				nz++;	
			}
			else 
				z++;
		ng_bleat( 0, "%s: zerod entries = %d   non-zeroed = %d", str, z, nz );
}

main( int argc, char ** argv )
{
	char *p = NULL;
	char *fn = "/tmp/nar_sym.pp.cpt";
	int i;

	verbose = 1;
	if( argc > 1 )
		fn = argv[1];

	nar_load( "/dev/null" );
	scan_tab( "after dummy load" );
	
	if( nar_load( fn )  )
	{
		scan_tab( "after real  load" );
		ng_bleat( 0, "load successful" );

		ng_bleat( 0, "check point to /tmp/nar_sym.pp.cpt2" );
		mk_ckpt( "/tmp/nar_sym.pp.cpt2", -1, 1 );
		send_table( 1, NULL, 0  );			/* simulate a cartulary dump */

		ng_bleat( 0, "laundering symtab" );
		nar_load( "/dev/null" );
		scan_tab( "after launder" );
		ng_bleat( 0, "check point to /tmp/nar_sym.pp.cpt3" );
		mk_ckpt( "/tmp/nar_sym.pp.cpt3", -1, 1 );
	}
}

#endif


