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
*/
#include	<sfio.h>
#include	<string.h>
#include	<stdlib.h>
#include	"ningaui.h"
#include	"ng_ext.h"

/*#define	REORDER */		/* define if you want pull-to-front reordering on hits */
/* some nice primes are 4099, 49999, 249989, 999983 */
#define	NHASH	49999
#define	HASHMUL	79L	/* this is a good value */

int aghdebug	= 0;

Symtab *
syminit(int nhash)
{
	Syment **s;
	Symtab *st;

	if(nhash == 0)
		nhash = NHASH;
	st = ng_malloc(sizeof *st, "symtab");
	st->nhash = nhash;
	st->syms = ng_malloc(nhash * sizeof(Syment), "syments");
	for(s = st->syms; s < &st->syms[st->nhash]; s++)
		*s = 0;
	return st;
}

Syment *
symlook(Symtab *st, char *sym, int space, void *install, int flags )
{
	long h;
	char *p;
	Syment *s;

	if (st == (Symtab *)NULL) {
                ng_log(LOG_ERR, "symtab:  symlook has null symtab");
                abort();
        }
	for(p = sym, h = space; *p; h += *p++) {
		h *= HASHMUL;
	}
	if(h < 0) {
		h = ~h;
	}
	h %= st->nhash;
#ifdef	REORDER
	q = 0;
#endif

	for(s = st->syms[h]; s; s = s->next){
		if((s->space == space) && (strcmp(s->name, sym) == 0)){
#ifdef	REORDER
			if(q){
				q->next = s->next;
				s->next = st->syms[h];
				st->syms[h] = s;
			}
#endif
			if(install)
				s->value = install;
			return s;
		}
#ifdef	REORDER
		q = s;
#endif
	}
	if(install == 0)
		return 0;

	s = ng_malloc(sizeof(*s), "symtab entry");

	s->name = (flags & SYM_COPYNM) ? ng_strdup( sym ) : sym;
	s->flags = flags;
	s->space = space;
	s->value = install;
	s->next = st->syms[h];
	st->syms[h] = s;
	return(s);
}

void
symdel(Symtab *st, char *sym, int space)
{
	long h;
	char *p;
	Syment *s, *ls;

	if (st == (Symtab *)NULL) {
                ng_log(LOG_ERR, "symdel: given null symtab");
                abort();
        }

	for(p = sym, h = space; *p; h += *p++) {
		h *= HASHMUL;
	}
	if(h < 0) {
		h = ~h;
	}
	h %= st->nhash;

	for(s = st->syms[h], ls = 0; s; ls = s, s = s->next)
		if((s->space == space) && (strcmp(s->name, sym) == 0)){
			if(ls)
				ls->next = s->next;
			else
				st->syms[h] = s->next;
			if( s->flags & SYM_COPYNM )
				ng_free( s->name );
			ng_free(s);
			return;				/* must not continue */
		}
}

void
symtraverse(Symtab *st, int space, void (*fn)(Syment *, void *), void *arg)
{
	Syment **s, *ss, *sn;

	if (st == (Symtab *)NULL) {
		ng_log(LOG_ERR, "symtab: symtraverse has null symtab");
		abort();
	}
	for(s = st->syms; s < &st->syms[st->nhash]; s++){
		for(ss = *s; ss; ss = sn){
			sn = ss->next;
			if((ss->space == space) || (space < 0)){
				(*fn)(ss, arg);
			}
		}
	}
}

void symclear( Symtab *st )		/* clear all entries -- free our stuff, but does not clean user data */
{
	Syment **s, *ss, *sn;

	if( ! st )
		return;			/* foolish user */

	for(s = st->syms; s < &st->syms[st->nhash]; s++)
	{
		for(ss = *s; ss; ss = sn)
		{
			sn = ss->next;
			if( ss->flags & SYM_COPYNM )
				ng_free( ss->name );
			ng_free( ss );
		}
		*s = 0; 
	}
}

void
symstat(Symtab *st, Sfio_t *fp)
{
#define	SYM_STAT_COUNT	1000
	Syment **s, *ss;
	int n;
	int l[SYM_STAT_COUNT];

        if (st == (Symtab *)NULL) {
                ng_log(LOG_ERR, "symtab:  symstat has null symtab");
                abort();
        }

	for(n = 0; n < SYM_STAT_COUNT; n++) {
		l[n] = 0;
	}
	for(s = st->syms; s < &st->syms[st->nhash]; s++){
		for(ss = *s, n = 0; ss; ss = ss->next)
			n++;
		if(n >= SYM_STAT_COUNT)
			n = SYM_STAT_COUNT-1;
		l[n]++;
	}
	sfprintf(fp, "symbol table stats:\n");
	for(n = 0; n < SYM_STAT_COUNT; n++) {
		if(l[n]) sfprintf(fp, "\t%ld of length %d\n", l[n], n);
	}
}

#ifdef SCDSTUFF
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(sumtab:Ningaui Symbol Table Functions)

&space
&synop	#include <ningaui.h>
&break	#include <ng_lib.h>
&space
&break	Symtab * syminit(int nhash)
&break	Syment * symlook(Symtab *st, char *sym, int space, void *install, int flags )
&break	void symtraverse(Symtab *st, int space, void (*fn)(Syment *, void *), void *arg)
&break	void symdel(Symtab *st, char *sym, int space)
&break	void symclear( Symtab *st )		
&break	void symstat(Symtab *st, Sfio_t *fp)
	

&space
&desc
	This collection of functions allocates and manages a hashed symbol table. 
	The table maps character names to void pointers which are assumed to reference 
	some meaningful user data. Each table may be divided into multiple name spaces
	using the space parameter on the &lit(symlook) function. 

&space
	The symbol table uses a datastructure &lit(Syment) to manage the data. Pointers to 
	this structure are returned by &lit(symlook) and are passed to functions called by 
	&lit(symtraverse.)  The structure has the following layout:
&space
&litblkb
  typedef struct Syment
  {
	  int		space;	/* namespace value */
	  int		flags;	
	  char		*name;	/* symbol name in the namespace */
	  void		*value; /* pointer to user data */
	  struct Syment	*next;
  } Syment;
&litblke

&space
	&lit(Syminit) is used to initialise a new table. The &lit(nhash) parameter 
	defines the number of hash pointers that will be allocated.  This number should 
	be a prime, and large enough to keep the number of hashing collisions to a 
	minimum.  The &lit(syminit) function returns a pointer to the symbol table 
	datastructure which must be passed to all other symbol table management functions. 

&space
	&lit(Symlook) is used to both put a value into the symbol table (st) and to 
	look-up and fetch a value.  When the &lit(install) parameter is NULL, the &lit(symlook)
	function searches for the string &lit(sym) in the table, and returns a pointer to 
	the corresponding &lit(Syment) datastructure if found.  When the &litinstall) parameter 
	is not NULL, it is assumed to contain a pointer that is saved as the &lit(value) of 
	a &lit(Syment) structure and placed into the hash table.  If there was an existing 
	entry with the same name, the value pointer in the existing structure is overlaid; the 
	pointer to the name is not changed. 
&space
	The &lit(flags) parameter is used when installing a value in the table and mayb be one
	of two values:
&space
&begterms
&term	SYM_COPY_NM : Causes the name (sym) to be duplicated allowing the caller to free or alter
	the &lit(sym) buffer that is passed in.  When this flag is set, the 
&space
&term	SYM_NOFLAGS : This value is the safe way to indicate no flags are to be set. 
&endterms
&space

&space
	The &lit(space) parameter allows the calling programme to specify a namespace. The 
	combination of symbol name (sym) and space value are used to search the hash table. 
	
&space
	
	The &lit(symtraverse) function invokes the user supplied function once for each 
	symbol table entry in the designated namespace (space). The invoked user function 
	is passed a pointer to the syment datastructure, and a void pointer (arg) that is 
	passed when &lit(symtraverse) is invoked.  It is safe for the user function to delete
	the symbol table entry. 
&space
	The &lit(symdel) function causes the entry that matches the name string (sym) to be 
	deleted.  The entry name is deleted by this function  only if the entry was created 
	using the &lit(SYM_COPY_NM) flag.   The data pointed to by the &lit(syment) value 
	pointer is &bold(never) altered. 

&space
	The &lit(symclear) function clears all of the entries, for all of the namespaces, in the 
	table.  The name strings are deleted only for the entries that were created with the 
	&lit(SYM_COPY_NM) flag. 

&space
	The &lit(symstats) function can be used to generate statistics about the current symbol
	table to an output file (generally sfstdio).  The  statistics generated can be used 
	to determine whether the number of hash entries provided to the initialisation function 
	is sufficent. 

&space

&space
&see

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Feb 1997 (sd) : Its beginning of time. 
&term	31 Jul 2005 (sd) : Changed to eliminate gcc warnings.
&term	13 Jan 2006 (sd) : Added symclear to wash the entire table.
&term	29 Mar 2006 (sd) : changed strdup() to ng_strdup() in look.
&term	03 Oct 2007 (sd) : Added man page.	
&endterms


&scd_end
------------------------------------------------------------------ */

#endif
