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

/* ----------------------------------------------------------------------------------
	Mnemonic:	ng_meld
	Abstract: 	combine n sorted files based on separated key fields
	Date:		20 Apr 2005  (HBD MCD)
	Author:		E. Scott Daniels
-------------------------------------------------------------------------------------
*/

#include <string.h>
#include	<unistd.h>
#include	<stdlib.h>
/*#include <stdio.h>*/
#include <errno.h>


/*#include        <sfdcgzip.h>   this seems not to exist in contrib/kpv stuff */
#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_tokenise.h>

#include	"meld.man"


typedef struct	Bufblk {			/* one per open file; listed in sorted order head by bufs */
	struct	Bufblk *next;		
	struct	Bufblk *prev;
	char	*fname;			/* only used for diagnostics */
	char	*buf;			/* current buffer from this input source */
	Sfio_t	*fd;			/* pointer for this open file */

	char	**fields;		/* pointers to fields set once on read */
	int	*lens;			/* lengths of each */
} Bufblk;


/* ------------------------------------------------------------------------------------------ */

extern	int verbose;

Bufblk	*bufs = NULL;			/* pointer to the sorted buffers after last read */
int	nfields = 0;			/* number of fields to key on */
int	def_flist[3] = {3,0,2};		/* by default, the cJ fields */
int	*flist = def_flist;
char	field_sep = '|';
char	*version = "v1.0/05025";


/* ------------------------------------------------------------------------------------------ */
	/*	set the field markers in the buf block; done once as the buffer
		is read so that we dont need to suss things out for each test
	 */
void mark_fields( Bufblk *b )
{
	int 	i;
	int	j;
	char	*start;

	if( ! b->buf )
		return;

	for( i = 0; i < nfields; i ++ )
	{
		start = b->buf;
		for( j = 0; j < flist[i] && (start = strchr( start+1, field_sep )); j++ );

		if( start && j == flist[i] ) 
		{
			if( start == b->buf && *start == field_sep )		/* null first field -- special case */
			{
				b->fields[i] = b->buf;			/* gotta point at something, zero len prevents comp */
				b->lens[i] = 0;
			}
			else
			{
				if( *start == field_sep )
				{
					b->fields[i] = start + 1;
					}
				else
					b->fields[i] = start;
	
					if( (start = strchr( start + 1, field_sep )) )
					b->lens[i] = start - b->fields[i];
				else
					b->lens[i] = strlen( b->buf ) - (b->fields[i] - b->buf) + 1;
			}
		}
	}
}

	/* test to see if b1 < b2; returns <0 if b1 < b2;  0 if they are == and >0 if b1>b2 */
int test( Bufblk *b1, Bufblk *b2 )
{
	int	i;
	int 	rc = -1;

	for( i = 0; i < nfields; i++ )
	{
		if( b1->lens[i] != b2->lens[i] )    /* != key len; comp up to smallest len; if == return l1-l2 else return result of memcmp */
		{
			if( ! b1->lens[i] || ! b2->lens[i] )   		/* one key is missing */
				return b1->lens[i] - b2->lens[i];

			if( (rc = memcmp( b1->fields[i], b2->fields[i], b1->lens[i] < b2->lens[i] ? b1->lens[i] : b2->lens[i] )) == 0  )
				return b1->lens[i] - b2->lens[i];
			else
				return rc;
		}
		else
			if( b1->lens[i] && (rc = memcmp( b1->fields[i], b2->fields[i], b1->lens[i] )) )
				return rc;
	}

	return strcmp( b1->buf, b2->buf );		/* punt if all of them are the same; or no fields supplied */
}

	/* get the next record for the block -- return 0 if at end */
char *refill( Bufblk *b )
{
	b->buf = sfgetr( b->fd, '\n', SF_STRING );
	if( b->buf )
		mark_fields( b );		/* set up field offsets for this buffer */

	return b->buf;
}


	/* delete the block; close the file; assumes pulled from list */
void trash( Bufblk *b )
{
	ng_bleat( 1, "end of file reached for: %s", b->fname );
	if( sfclose( b->fd ) )
	{
		ng_bleat( 0, "abort: error on write or close; %s: %s", b->fname, strerror( errno ) );
		exit( 1 );
	}
	ng_free( b->fields );
	ng_free( b->lens );
	ng_free( b->fname );
	ng_free( b );
}

	/* add a block to the existing sorted list (bufs) */
void meld( Bufblk *new )
{
	Bufblk	*b = NULL;		/* pointer into the list */
	Bufblk	*last = NULL;		/* pointer at the last block if we run off */

	if( !new->buf )
		return;

	for( b = bufs; b && test( b, new ) < 0; b = b->next )
		last = b;				/* hold last in list if we run off */

	if( b )
	{
		if( b->prev )
			b->prev->next = new;
		else
			bufs = new;

		new->prev = b->prev;
		new->next = b;
		b->prev = new;
	}
	else
	{
		if( last )
		{
			last->next = new;
			new->next = NULL;
			new->prev = last;
		}
		else
		{
			new->prev = new->next = NULL;
			bufs = new;
		}
	}
}


	/*	prime things by creating a bufblk, 
		opening the file, reading the frist 
		rec and inserting it into the list 
	*/
void prime( char *fname )
{
	Bufblk	*new = NULL;
	char 	*p;

	new = (Bufblk *) ng_malloc( sizeof( *new ), "prime:bufblock" );
	memset( new, 0, sizeof( *new ) );

	new->fields = (char **) ng_malloc( sizeof( char * ) * nfields, "mark_fields: fields" );
	new->lens = (int *) ng_malloc( sizeof( int ) * nfields, "mark_fields: lens" );
	memset( new->fields, 0, sizeof( char * ) * nfields );
	memset( new->lens, 0, sizeof( int ) * nfields );
	new->fname = ng_strdup( fname );

	ng_bleat( 2, "setting up input file: %s", fname );
	if( *fname == '-' )
		new->fd = sfstdin;
	else
	{
		if( (new->fd = ng_sfopen( NULL, fname, "r" )) == NULL )
		{
			ng_bleat( 0, "prime: open failed: %s: %s", fname, strerror( errno ) );
			exit( 1 );
		}

		if( (p = strstr( fname, ".gz" ))  && *(p+3) == 0 )
		{
			ng_bleat( 2, "prime: adding gzip line dicipline for input file: %s", fname );
			sfdcgzip( new->fd, 0);
		}
	}

	if( ! refill( new ) )			/* get the first buffer from the file  and mark fields */
	{
		trash( new );			/* immediate end of file */
		return;
	}

	ng_bleat( 2, "first buf: %s: %s", fname, new->buf );
	meld( new );			/* insert into sorted list of buffers */
}




void flush_it( Sfio_t *fp )
{
	char *buf;

	while( (buf = sfgetr( fp, '\n', SF_STRING )) )
		sfprintf( sfstdout, "%s\n", buf );
}

	/* 	convert a set of field numbers (e.g. 3,4,1) into 
		the fields list we will use; set nfields
	*/
void set_keys( char *kstr )
{
	char	**toks;
	int	i;

	toks = ng_tokenise( kstr, ',', '\\', NULL, &nfields );
	if( nfields )
	{
		flist = ng_malloc( sizeof( int ) * nfields, "set_keys: field list" );
		for( i = 0; i < nfields; i++ )
		{
			flist[i] = atoi( toks[i] );
			ng_bleat( 2, "key read: search order=%d  field=%d", i, flist[i] );
		}
	}

	ng_bleat( 2, "nkeys=%d", nfields );
	ng_free( toks );
}

/* ------------------------------------------------------------ */
void usage( )
{
	sfprintf( sfstderr, "%s version %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-k field[,field...]] [-t field-sep] [-v] file [file...]\n", argv0 );
	exit( 1 );
}

int main( int argc, char **argv )
{
	char	*cp;			/* generic pointer */
	int	compress = 0;		/* gzip the output */
	int 	i;
	Bufblk	*b;
	char	*sepp = NULL;		/* user supplied sep token */


	verbose = 0;

	ARGBEGIN
	{
		case 'c':	compress = 1; break;			/* compress output */
		case 'k':	SARG( cp ); set_keys( cp ); break;
		case 't':	SARG( sepp ); field_sep = *sepp; break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default: 	usage( ); exit( 1 ); break;
	}
	ARGEND

	ng_bleat( 1, "version %s", version );

	for( i = 0; i < argc; i++ )
		prime( argv[i] );
	
	
	if( compress )
	{
		ng_bleat( 2, "adding gzip line dicipline for output" );
		sfdcgzip( sfstdout, 5);
	}

	while( bufs )
	{
		if( bufs->buf )
			sfprintf( sfstdout, "%s\n", bufs->buf );		/* write the smallest one */

		b = bufs;
		if( (bufs = bufs->next) )				/* pull off of the list */
			bufs->prev = NULL;
		b->next = b->prev = NULL;

		if(  refill( b ) )					/* get next and insert into the list */
			meld( b );		
		else
			trash( b );	
	}

	return 0;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_meld:Blend sorted files)

&space
&synop	ng_meld [-c] [-k field[,field...]] [-t seperator] [-v] file file [file...]

&space
&desc	&This reads the files supplied on the command line and blends them based on the 
	key fields that are indicated on the command line.  Input files are expected to 
	be in sorted order based on the same key field and key field ordering.
	The melded data is written to the standard output device. 
&space
	Standard input can be read as one of the input files and must be identified 
	in the file list as '-'. 
&space
	If an input file has a name ending with a &lit(^.gz) suffix, the file is 
	assumed to be compressed with gzip and will be decompressed as it is read. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Compress.  The output is compressed using gzip.  It seems faster to compress
	by piping the output to gzip as this allows for some paralellisation.
&space
&term	-k field,field : Supplies the fields that are used to determine sort order across
	the input streams.  Fields are suplied as &stress(zero based) numbers, in the order 
	that they should be compared.  Thus, to merge based on the first (0), third and 
	then second field, the parameter would be -k 0,2,1. If no key field(s) are supplied, 
	the records are merged based on a full record comparison. 
&space
&term	-t sep : Supplies the field separation character.  This is needed only if a key field
	is supplied. If not supplied, and a key field is, the default is the veritcal bar 
	character.
&space
&term	-v : Verbose. 
	
&endterms


&space
&parms	&This expects the list of files to meld together to be supplied on the command line. 
	If a file's name is suffixed with ^.gz, it is assumed to be a gzipped compressed file 
	and will be uncompressed as it is read. 

&space
&exit	An exit code that is non-zero indicates an error and should be accompanied with an 
	error message in standard error and possibly in the master log.

&space
&see
	sort
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Apr 2005 (sd) : Sunrise. (HBD MCD)
&term	28 Apr 2005 (sd) : Corrected comparison bug with null first field.
&term	02 May 2005 (sd) : Corrected problem if a file had no records.
&term	29 Aug 2006 (sd) : Fixed caus of warnings when compiled on sun. 
&endterms

&scd_end
------------------------------------------------------------------ */
