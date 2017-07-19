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
-------------------------------------------------------------------------
Mnemonic: symtab.c
Abstract: Basic symbol table routines.
Date:     11 Feb 2000
Author:   E. Scott Daniels
-------------------------------------------------------------------------
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#include "ut.h"

/* ----- private functions ---- */
static int sym_hash( unsigned char *n, long size )
{
	unsigned char *p;
	unsigned long t = 0;
	unsigned long tt = 0;
	unsigned long x = 79;

	for( p = n; *p; p++ )      /* a bit of magic */
		t = (t * 79 ) + *p;

	if( t < 0 )
		t = ~t;

	return (int ) (t % size);
}

/* delete element pointed to by eptr at hash loc hv */
static void del_ele( Sym_tab *table, int hv, Sym_ele *eptr )
{
	Sym_ele **sym_tab;

	sym_tab = table->symlist;

	if( eptr )         /* unchain it */
	{
		if( eptr->prev )
			eptr->prev->next = eptr->next;
		else
			sym_tab[hv] = eptr->next;

		if( eptr->next )
			eptr->next->prev = eptr->prev;

		if( eptr->val && eptr->flags & UT_FL_FREE )
			free( eptr->val );
		if( eptr->name )
			free( eptr->name );
		free( eptr );

		table->deaths++;
		table->inhabitants--;
	}
}

static int same( unsigned int c1, unsigned int c2, char *s1, char* s2 )
{
	if( c1 != c2 )
		return 0;		/* different class - not the same */

	if( *s1 == *s2 && strcmp( s1, s2 ) == 0 )
		return 1;
	return 0;
}

/* generic rtn to put something into the table */
/* called by sym_map or sym_put, but not by the user! */
static int putin( Sym_tab *table, char *name, unsigned int class, void *val, int flags )
{
	Sym_ele *eptr;    	/* pointer into hash table */ 
	Sym_ele **sym_tab;    	/* pointer into hash table */ 
	int hv;                  /* hash value */
	int rc = 0;              /* assume it existed */

	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );


	for(eptr=sym_tab[hv]; eptr && ! same( class, eptr->class, eptr->name, name); eptr=eptr->next );

	if( ! eptr )    /* new symbol for the table */
	{
		rc++;
		table->inhabitants++;
	
		eptr = (Sym_ele *) malloc( sizeof( Sym_ele) );
		if( ! eptr )
		{
			fprintf( stderr, "symtab/putin: out of memory\n" );
			exit( 1 );
		} 

		eptr->flags = flags & UT_FL_COPY ? UT_FL_FREE : 0;		/* set free flag if we made a copy of things */
		eptr->prev = NULL;
		eptr->class = class;
		eptr->mcount = eptr->rcount = 0;	/* init counters */
		eptr->val = NULL;                	/* add to head of the list */
		eptr->name = strdup( name );
		eptr->next = sym_tab[hv];
		sym_tab[hv] = eptr;
		if( eptr->next )
			eptr->next->prev = eptr;         /* chain back to new one */
	}

	eptr->mcount++;

	if( flags & UT_FL_COPY )          /* data expected to be a chr string */
	{
		if( eptr->val )		/* assuming we added it last time round */
			free( eptr->val );

		eptr->val = strdup( val );    /* make a copy so user can change */
	}
	else
		eptr->val = val;

	return rc;
}

/* --- public functions ---- */

/* delete all elements in the table */
void sym_clear( Sym_tab *table )
{
	Sym_ele **sym_tab;
	int i; 

	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
		while( sym_tab[i] ) 
			del_ele( table, i, sym_tab[i] );
}

void sym_dump( Sym_tab *table )
{
	int i; 
	Sym_ele *eptr;
	Sym_ele **sym_tab;

	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
	{
		if( sym_tab[i] )
		for( eptr = sym_tab[i]; eptr; eptr = eptr->next )  
		{
			if( eptr->val && eptr->flags & UT_FL_FREE )
				fprintf( stderr, "%s %s\n", eptr->name, eptr->val );
			else
				fprintf( stderr, "%s -> #%x\n", eptr->name, eptr->val );
		}
	}
}

/* allocate a table the size requested - best if size is prime */
/* returns a pointer to the management block */
Sym_tab *sym_alloc( int size )
{
	int i;
	Sym_tab *table;

	if( size < 11 )     /* provide a bit of sanity */
		size = 11;

	if( (table = (Sym_tab *) malloc( sizeof( Sym_tab ))) == NULL )
	{
		fprintf( stderr, "sym_alloc: unable to get memory for symtable (%d elements)", size );
		exit( 1 );
	}

	memset( table, 0, sizeof( *table ) );

	if((table->symlist = (Sym_ele **) malloc( sizeof( Sym_ele *) * size ))) 
	{
		memset( table->symlist, 0, sizeof( Sym_ele *) * size );
		table->size = size;
	}
	else
	{
		fprintf( stderr, "sym_alloc: unable to get memory for %d elements", size );
		exit( 1 );
	}

	return table;    /* user might want to know what the size is */
}

/* delete a named element */
void sym_del( Sym_tab *table, unsigned char *name, unsigned int class )
{
	Sym_ele **sym_tab;
	Sym_ele *eptr;    /* pointer into hash table */ 
	int hv;                  /* hash value */

	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );

	for(eptr=sym_tab[hv]; eptr && eptr->class != class &&  ! same(class, eptr->class, eptr->name, name); eptr=eptr->next );

	del_ele( table, hv, eptr );    /* ignors null ptr, so safe to always call */
}


void *sym_get( Sym_tab *table, unsigned char *name, unsigned int class )
{
	Sym_ele **sym_tab;
	Sym_ele *eptr;    /* pointer into hash table */ 
	int hv;                  /* hash value */

	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );

	for(eptr=sym_tab[hv]; eptr && ! same( eptr->class, class, eptr->name, name); eptr=eptr->next );

	if( eptr )
	{
		eptr->rcount++;
		return eptr->val;
	}

	return NULL;
}

/* 
The only difference between sym_put and sym_map is that put assumes the 
pointer is to a character string and it makes a copy of the data. sym_map 
causes the user's pointer to be put into the table and thus the user is 
responsible for freeing the data. There is some risk to the user as if 
the data location changes, or the user frees the space,  the symbol table 
will have the wrong pointer unless they replace it with another sym_map 
call, or delete the entry! 
*/

/* put an element, replace if there */
/* creates local copy of data (assumes string) */
/* returns 1 if new, 0 if existed */
int sym_put( Sym_tab *table, unsigned char *name, unsigned int class, void *val )
{
	return putin( table, name, class, val, UT_FL_COPY );
}

/* add/replace an element, map directly to user data, dont copy */
/* the data like sym_put does */
/* returns 1 if new, 0 if existed */
int sym_map( Sym_tab *table, unsigned char *name, unsigned int class, void *val )
{
	return putin( table, name, class, val, UT_FL_NOCOPY );
}

/* dump some statistics to stderr dev. Higher level is the more info dumpped */
void sym_stats( Sym_tab *table, int level )
{
	Sym_ele *eptr;    /* pointer into the elements */
	Sym_ele **sym_tab;
	int i;
	int empty = 0;
	long ch_count;
	long max_chain = 0;
	int maxi = 0;
	int twoper = 0;

	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
	{
		ch_count = 0;
		if( sym_tab[i] )
		{
			for( eptr = sym_tab[i]; eptr; eptr = eptr->next )  
			{
				ch_count++;
				if( level > 3 )
				if( eptr->val && eptr->flags & UT_FL_FREE )
					fprintf( stderr, "sym: (%d) %s str=(%s)  ref=%d mod=%d\n", 
						i, eptr->name, eptr->val, eptr->rcount, eptr->mcount );
				else
					fprintf( stderr, "sym: (%d) %s ptr=%x  ref=%d mod=%d\n", 
						i, eptr->name, eptr->val, eptr->rcount, eptr->mcount );
			}
		}
		else
			empty++;

		if( ch_count > max_chain ) 
		{
			max_chain = ch_count;
			maxi = i;
		}
		if( ch_count > 1 )
			twoper++;

		if( level > 2 )
			fprintf( stderr, "sym: (%d) chained=%d\n", i, ch_count );
	}

	if( level > 1 )
	{
		fprintf( stderr, "sym: largest chain: (%d) ", maxi );
		for( eptr = sym_tab[maxi]; eptr; eptr = eptr->next )
	 		fprintf( stderr, "%s ", eptr->name );
		fprintf( stderr, "\n" );
	}

	fprintf( stderr, "sym:%d(size)  %d(inhab) %d(occupied) %d(dead) %ld(maxch) %d(>2per)\n", 
	table->size, table->inhabitants, table->size - empty, table->deaths, max_chain, twoper );
}

void sym_foreach_class( Sym_tab *st, unsigned int class, void (* user_fun)(), void *user_data )
{
	Sym_ele **list;
	Sym_ele *se;
	Sym_ele *next;		/* allows user to delete the node(s) we return */
	int 	i;

	if( st && (list = st->symlist) != NULL && user_fun != NULL )
		for( i = 0; i < st->size; i++ )
			for( se = list[i]; se; se = next )		/* using next allows user to delet via this */
			{
				next = se->next;
				if( class == se->class )
					user_fun( st, se, se->name, se->val, user_data );
			}
}
