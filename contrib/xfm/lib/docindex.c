/*
* ---------------------------------------------------------------------------
* This source code is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* If this code is modified and/or redistributed, please retain this
* header, as well as any other 'flower-box' headers, that are
* contained in the code in order to give credit where credit is due.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Please use this URL to review the GNU General Public License:
* http://www.gnu.org/licenses/gpl.txt
* ---------------------------------------------------------------------------
*/
/*
* --------------------------------------------------------------------
*
*	Mnemonic: 	docindex 
*	Abstract: 	create an index of words
*			application programmes register words to track with the 
*			di_register() and di_synonym() functions.  The di_add() function is 
*			called each time a word is  noticed on a page which updates the 
*			pertinant references.  The di_list()
*			function writes the list using several user supplied format strings.
*			The user supplies the open file handle.  If a space format string is supplied,
*			di_list() breaks on a change in first letter of the terms and 
*			writes the space formatting string. The group and end group format strings
*			are used when a group entry is encountered.
*
*			We assume that references are added in page sequential order
*			and do not do anything fancy to prune duplicate page references other than 
*			looking to see if the current page was the last page noticed.
*			We also depend on this to allow for page ranges (e.g. 12-19)
*
*	Date:		17 Oct 2007
*	Author: 	E. Scott Daniels
* --------------------------------------------------------------------
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>

#include "symtab.h"

#define ENTRY_CLASS	1	/* entry space for preventing duplicates */
#define MAP_CLASS	2	/* word map references in hash */
#define SYN_CLASS	3	/* synonym class */

#define RF_SYN		0x01	/* reference is a syn, next points at the real entry */

#define MAX_REFS	256

/* a reference in the index */
typedef struct ref_info {
	struct ref_info *next;		/* if in word list the next one, if a synonym then this points at the one in the word list */
	char	*entry;			/* string used as the entry in the index */
	char	*iword;			/* string used for sorting (upper case first ch) */
	char	*wlist[100];		/* list of words in the entry */
	int	flags;
	int	nwords;			/* number of words in the entry */
	int	pgref[MAX_REFS];
	char 	**glist;
	int	ridx;			/* page or group reference index (insertion point) */

	long	last_token;		/* token number of last matched */
	int	tidx; 			/* next word in the wlist to try */
} Ref_info;


/* a map of a word to all ref_info blocks whose entry contains the word */
typedef struct word_map {
	char	*word;			/* the word */
	int	nalloc;			/* number of references allocated */
	int	idx;			/* index into list of next free one */
	Ref_info **rlist;		/* list of ref info blocks that use this word */
} Word_map;


/* info needed for a group reference */
typedef struct group_info {
	struct group_info *next;
	char	*gname;
	char	**words;
} Group_info;


static Sym_tab *table  = NULL;
static Ref_info *entry_list = NULL;
static Group_info *group_list = NULL;
static unsigned long token_id = 1;		/* token counter to track sequential hits */

/* -------------- private functions ----------------------------- */
static void di_init( )
{
	table = sym_alloc( 24999 );
}

static void dump_rp( Ref_info *rp )
{
	printf( "ref-info: %s (%x) next=%x flags=%x words=%d ridx=%d\n", rp->entry, rp, rp->next, rp->flags, rp->nwords, rp->ridx );
}

/* find the insertion point -- returns the block that comes before the new block */
static Ref_info *get_ip( char *name )
{
	Ref_info *rp = NULL;
	Ref_info *pp = NULL;		/* previous block */

	if( ! entry_list  )
		return NULL;

	if( strcmp( name, entry_list->iword ) <= 0 )
		return NULL;

	pp = entry_list;
	for( rp = entry_list->next; rp && strcmp( name, rp->iword ) > 0; rp = rp->next )
		pp = rp;

	return pp;
}

/* create a new word map block (if needed) and point at a ref_info block 
	returns a pointer to the word buffer that can be referenced
*/
static char *wmap2ref( char *word, Ref_info *rp )
{
	Word_map	*mp;

	if( (mp = sym_get( table, word, MAP_CLASS)) == NULL )
	{
		mp = (Word_map *) malloc( sizeof( Word_map ) );
		if( ! mp )
		{
			fprintf( stderr, "docindex: unable to alloc memory for word map" );
			exit( 1 );
		}

		memset( mp, 0, sizeof( Word_map ) );
		mp->word = strdup( word );
		mp->nalloc = 10;
		mp->rlist = (Ref_info **) malloc( sizeof( Ref_info *) * mp->nalloc );
		if( ! mp->rlist )
		{
			fprintf( stderr, "docindex: unable to alloc memory for ref map" );
			exit( 1 );
		}
		memset( mp->rlist, 0, sizeof( Ref_info *) * mp->nalloc );
		sym_map(  table, mp->word, MAP_CLASS, mp );				/* add to hash */
	}

	if( mp->idx >= mp->nalloc )			/* if we need more pointers */
	{
		mp->nalloc += 100;
		mp->rlist = (Ref_info **) realloc( mp->rlist, sizeof( Ref_info *) * mp->nalloc );
		if( ! mp->rlist )
		{
			fprintf( stderr, "docindex: unable to realloc memory for ref map" );
			exit( 1 );
		}
	}

	mp->rlist[mp->idx++] = rp;
	return mp->word;
}


/* insert the word info block into the list */
static void insert_rp( Ref_info *rp )
{
	Ref_info	*ip;

	if( (ip = get_ip( rp->iword )) == NULL )		/* insert before the head block */
	{
		rp->next = entry_list;
		entry_list = rp;
	}
	else
	{
		rp->next =  ip->next;			/* insert after the block returned */
		ip->next = rp;
	}
}

/* print the passed in term. ufmt is the printf format string that the user 
   passed in; something like: .di %s : %d from xfm
*/
static void print_term( FILE *f, Ref_info *rp, char *ufmt )
{
	int 	i;
	
	fprintf( f, ufmt ? ufmt : "%s %d", rp->entry, rp->pgref[0] );
	for( i = 1; i < rp->ridx; i++ )
	{
		if( rp->pgref[i-1] + 1 == rp->pgref[i] )	/* contigious pages are listed as n-m */
		{
			if( i == rp->ridx - 1  || rp->pgref[i] + 1 != rp->pgref[i+1] )
				fprintf( f, "-%d", rp->pgref[i] );
		}
		else
			fprintf( f, ", %d", rp->pgref[i] );
	}

	fprintf( f, "\n" );
}


/* list all of the words associated with a group. the gfmtb is the format 
   for a printf done at the beginning of the group and passed the group name. 
   gfmte is the format string for a printf done at the end.  ufmt is the format
   string for each term in the group (passed to print_term)
   gfmtb is probably something like &indent .di %s : 
   gfmte is probalby something like .ed &uindent
*/
static void print_group( FILE *f, Ref_info *gp, char *ufmt, char *gfmtb, char *gfmte )
{
	int	i;
	int	printed = 0;	/* printed the group heading flag */
	Ref_info *rp;		/* info block of each group term */
	

	for( i = 0; i < gp->ridx; i++ )
	{
		if( ((rp = sym_get( table, gp->glist[i], ENTRY_CLASS )) != NULL) && rp->ridx )		/* pont at next term in the group */
		{
			if( !printed && gfmtb )			/* print group only when one sub member has something */
			{
				fprintf( f, gfmtb, gp->entry );
				printed++;			
			}

			print_term( f, rp, ufmt );
		}
	}

	if( gfmte && printed )
		fprintf( f, gfmte );
}


/* do the real work behind registration.  we use a static routine to allow internal 
   functions a bit more flexibility (to insert into the list or not, and to define hash class)
   returns a pointer to the new ref struct
*/
static Ref_info *enroll( char *entry, int insert, int class )
{
	Ref_info *rp;
	Ref_info *ip;			/* insertion point */
	int	i;
	char	*tok;
	char	*strtok_p;


	if( ! table )
		di_init( );
	else
		if( (rp = sym_get( table, entry, ENTRY_CLASS )) != NULL )		/* already registered; ignore */
			return rp;

	rp = (Ref_info *) malloc( sizeof( Ref_info ) );
	if( !rp )
	{
		fprintf( stderr, "di_register(enroll): cannot allocate memory\n" );
		exit( 1 );
	}

	memset( rp, 0, sizeof( Ref_info ) );
	rp->entry = strdup( entry );
	rp->iword = strdup( entry );
	*rp->iword = toupper( *rp->iword );			/* make word to sort on */

	if( insert )
	{
		if( (ip = get_ip( rp->iword )) == NULL )		/* insert before the head block */
		{
			rp->next = entry_list;
			entry_list = rp;
		}
		else
		{
			rp->next =  ip->next;			/* insert after the block returned */
			ip->next = rp;
		}
	}

	sym_map(  table, entry, class, rp );		/* add to hash -- simply to enable easy check for second attempt to add */

	/* add a reference for each word */
	tok = strtok_r( rp->entry, " \t", &strtok_p );
	i = 0;
	while( tok )
	{
		rp->wlist[i++] = wmap2ref( tok, rp );		/* map the word back to here */
		tok = strtok_r( NULL, " \t", &strtok_p );
	}

	rp->nwords = i;				/* number of words we are looking for */

	free( rp->entry );			/* we trashed it with strtok */
	rp->entry = strdup( entry );		/* we would like a new one please */

	return rp;
}

/* -------------- public routines ----------------------------- */
/* register a reference (name) to track 
	the reference is split into words (blank separated) allowing multiple word 
	references to be tracked.  each word is referenced by a word-map which points 
	back to the registration.
*/
void di_register( char *name, int insert )
{
	enroll( name, 1, ENTRY_CLASS );
}


/* add a syn (new) that is tracked and reported on as though it were existing */
void di_synonym( char *exist, char *new )	
{
	Ref_info	*orp;		/* old entry */
	Ref_info	*srp;		/* synonym entry */

	if( ! table )
		di_init();

	if( sym_get( table, new, SYN_CLASS ) || sym_get( table, new, ENTRY_CLASS) ) 		/* already there */
		return;								/* no dups */

	if( (orp = sym_get( table, exist, ENTRY_CLASS )) == NULL )		/* if not there, get it there now */
		orp = enroll( exist, 1, ENTRY_CLASS );

	srp = enroll( new, 0, SYN_CLASS );	/* add the syn, but DONT put it into the list */

	srp->next = orp;			/* next of a syn points at the real thing */
	srp->flags |= RF_SYN;			/* set syn flag */
}

void di_group( char *name, char *word )
{
	Ref_info 	*rp;
	Ref_info	*ip;		/* point where group block is inserted into the list */

	if( ! table )
		di_init( );

	if( (rp = sym_get( table, name, ENTRY_CLASS )) == NULL )		/* already registered; ignore */
	{
		rp = (Ref_info *) malloc( sizeof( Ref_info ) );
		if( !rp )
		{
			fprintf( stderr, "di_group: cannot allocate memory\n" );
			return;
		}

		memset( rp, 0, sizeof( rp ) );
		rp->entry = strdup( name );
		rp->iword = strdup( name );
		*rp->iword = toupper( *rp->iword );			/* make word to sort on */
		rp->glist = (char **) malloc( sizeof( char * ) * 256 );
		memset( rp->glist, 0, sizeof( char * ) * 256 );

		sym_map( table, name, ENTRY_CLASS, rp );
		insert_rp( rp );
	}

	rp->glist[rp->ridx++] = strdup( word );
}

/* add an observation of the word, if it has been registered */
void di_add( char *name, int pg )
{
	Ref_info *rp;
	Ref_info *ip;
	Word_map *mp;
	char	*aname = NULL;			/* altered name if we strip trailing punct */
	int	i;

	i = strlen( name ) - 1;
	if( ispunct( name[i] ) )
	{
		aname = strdup( name );
		aname[i] = 0;
	}

	if( (mp = (Word_map *) sym_get( table, aname ? aname : name, MAP_CLASS )) != NULL )	/* get ref to all things that use this word */
	{
		for( i = 0; i < mp->idx; i++ )
		{
			rp = mp->rlist[i];

			if( strcmp( name, rp->wlist[rp->tidx] ) == 0 )		/* word match */
			{
				if( ++rp->tidx == rp->nwords )			/* this matched the last word in the string */
				{
									/* must adjust rp before checking for syn */
					rp->tidx = 0;				/* force to start over now */

					if( rp->flags & RF_SYN )		/* is this a synonym? */
						rp = rp->next;			/* get the real thing to  add the ref to */

					if( rp->ridx < MAX_REFS )
					{
						if( rp->ridx )			/* not the first */
						{
							if( rp->pgref[rp->ridx-1] != pg )
								rp->pgref[rp->ridx++] = pg;
						}
						else
							rp->pgref[rp->ridx++] = pg;
					}
				}
			}
			else
				rp->tidx = 0;				/* start over the next time through */
		}
	}

	if( aname )
		free( aname );
}


/* ufmt supplies the format for the word and first page number allowing something 
   like ".di %s : %d" to be supplied for pfm
*/
void di_list( FILE *f, char *ufmt, char *uspace, char *gfmtb, char *gfmte )
{
	Ref_info *rp;
	char	fmt[1024];
	int	i;
	char	lastc;				/* last c for extra spacing */

	lastc = 'A' - 1;

	for( rp = entry_list; rp; rp = rp->next )
	{
			if( rp->glist )	
				print_group( f, rp, ufmt, gfmtb, gfmte );
			else
			{
				if( rp->ridx )
				{
					if( (toupper( *(rp->entry)) > lastc) && uspace )
					{
						fprintf( f, uspace, (char) toupper( *(rp->entry) ) );
						lastc = toupper( *(rp->entry) );
					}
	
					print_term( f, rp, ufmt );
				}
			}
	}
}


/* ------------------- self test -------------- */
#ifdef SELF_TEST
main()
{
	Ref_info *rp;

	di_register( "scooter", 1);
	di_register( "pies are", 1 );
	di_register( "good", 0 );
	di_register( "zebra", 1 );
	di_register( "9", 1 );
	di_register( "koalah", 1 );
	di_register( "are", 1 );
	di_register( "+", 1 );

	di_register( "Daniels, Scott", 1 );
	di_synonym( "Daniels, Scott", "Daniels" );
	di_synonym( "Daniels, Scott", "Scott Daniels" );

	di_register( "replication manager", 1 );
	di_synonym( "replication manager", "Replication manager" );
	di_synonym( "replication manager", "Replication Manager" );

	di_group( "group1", "are" );
	di_group( "group1", "scooter" );

	di_add( "are", 98 );
	di_add( "scooter", 1 );
	di_add( "Scooter", 3 );
	di_add( "scooter", 5 );
	di_add( "+", 55 );
	di_add( "scooter", 7 );
	di_add( "scooter", 9 );
	di_add( "scooter", 100 );

	di_add( "Scott", 6 );
	di_add( "Daniels", 6 );

	di_add( "pies", 2 );
	di_add( "9", 89 );
	di_add( "pies", 4 );
	di_add( "pies", 5 );
	di_add( "pies", 7 );
	di_add( "pies", 8 );
	di_add( "Replication", 8 );
	di_add( "Manager", 8 );
	di_add( "pies", 10 );
	di_add( "Scott", 16 );
	di_add( "Daniels", 16 );
	di_add( "pies", 6 );
	di_add( "Daniels", 17 );
	di_add( "Pies", 12 );
	di_add( "zebra", 80 );
	di_add( "Manager", 80 );
	di_add( "pies", 13 );		/* better match pies are */
	di_add( "are", 13 );		/* also should match are */
	di_add( "pies", 15 );
	di_add( "Zebra", 84 );

	di_add( "replication", 18 );
	di_add( "manager", 18 );
	
	di_add( "good", 10 );
	di_add( "good", 11 );
	di_add( "good", 12 );

	di_add( "are", 99 );

	di_list( stdout, "\t%s  %d", "\n==%c==\n", "\t%s\n", "\n\n" );
}
#endif
