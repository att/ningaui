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
#ifndef _symtab_h
#define _symtab_h

#define FL_NOCOPY 0x00          /* use user pointer */
#define FL_COPY 0x01            /* make a copy of the string data */
#define FL_FREE 0x02            /* free val when deleting */

typedef struct Sym_ele
{
	struct Sym_ele *next;          /* pointer at next element in list */
	struct Sym_ele *prev;          /* larger table, easier deletes */
	unsigned char *name;           /* symbol name */
	void *val;                     /* user data associated with name */
	unsigned long mcount;          /* modificaitons to value */
	unsigned long rcount;          /* references to symbol */
	unsigned int flags; 
	unsigned int class;		/* helps divide things up and allows for duplicate names */
} Sym_ele;

typedef struct Sym_tab {
	Sym_ele **symlist;			/* pointer to list of element pointerss */
	long	inhabitants;             	/* number of active residents */
	long	deaths;                 	/* number of deletes */
	long	size;	
} Sym_tab;

extern void sym_clear( Sym_tab *s );
extern void sym_dump( Sym_tab *s );
extern Sym_tab *sym_alloc( int size );
extern void sym_del( Sym_tab *s, unsigned char *name, unsigned int class );
extern void *sym_get( Sym_tab *s, unsigned char *name, unsigned int class );
extern int sym_put( Sym_tab *s, unsigned char *name, unsigned int class, void *val );
extern int sym_map( Sym_tab *s, unsigned char *name, unsigned int class, void *val );
extern void sym_stats( Sym_tab *s, int level );
void sym_foreach_class( Sym_tab *st, unsigned int class, void (* user_fun)(), void *user_data );
#endif
