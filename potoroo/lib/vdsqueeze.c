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

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sfio.h>
#include	"ningaui.h"
#include	"vdelhdr.h"

/*	Squeeze a string.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 2/15/95
*/

#ifdef DEBUG
long	S_copy, S_add;	/* amount of input covered by COPY and ADD	*/
long	N_copy, N_add;	/* # of COPY and ADD instructions		*/
long	M_copy, M_add;	/* max size of a COPY or ADD instruction	*/
long	N_merge;	/* # of merged instructions			*/
#endif

#define MERGABLE(a,c,k) ((a) > 0 && A_ISTINY(a) && \
			 (c) > 0 && C_ISTINY(c) && \
			 (k) >= K_SELF )

typedef struct _match_s	Match_t;
typedef struct _table_s	Table_t;
struct _match_s
{	Match_t*	next;		/* linked list ptr	*/
};
struct _table_s
{	ng_byte*		delta;		/* output area		*/
	ng_byte*		tar;		/* target string	*/
	int		n_tar;
	K_DDECL(quick,recent,rhere);	/* address caches	*/
	Match_t*	base;		/* base of elements	*/
	int		size;		/* size of hash table	*/
	Match_t**	table;		/* hash table		*/
};

/* encode and output delta instructions */
static int
vdputinst(Table_t* tab, ng_byte* begs, ng_byte* here, Match_t* match, int n_copy)
{
	reg int		n_add, i_add, i_copy, k_type;
	reg int		n, c_addr, copy, best, d;
	ng_byte		buf[sizeof(long)+1];

	n_add = begs ? here-begs : 0;	/* add size		*/
	c_addr = here-tab->tar;		/* current address	*/
	k_type = 0;

	if(match)	/* process the COPY instruction */
	{	/**/DBTOTAL(N_copy,1); DBTOTAL(S_copy,n_copy); DBMAX(M_copy,n_copy);

		best = copy = match - tab->base;
		k_type = K_SELF;
		if((d = c_addr - copy) < best)
		{	best = d;
			k_type = K_HERE;
		}
		for(n = 0; n < K_RTYPE; ++n)
		{	if((d = copy - tab->recent[n]) < 0 || d >= best)
				continue;
			best = d;
			k_type = K_RECENT+n;
		}
		if(best >= I_MORE && tab->quick[n = copy%K_QSIZE] == copy)
		{	for(d = K_QTYPE-1; d > 0; --d)
				if(n >= (d<<VD_BITS) )
					break;
			best = n - (d<<VD_BITS); /**/ASSERT(best < (1<<VD_BITS));
			k_type = K_QUICK+d;
		}

		/**/ASSERT(best >= 0);
		/**/ASSERT((k_type+K_MERGE) < (1<<I_BITS) );

		/* update address caches */
		K_UPDATE(tab->quick,tab->recent,tab->rhere,copy);

		/* see if mergable to last ADD instruction */
		if(MERGABLE(n_add,n_copy,k_type) )
		{	/**/DBTOTAL(N_merge,1);
			i_add = K_TPUT(k_type)|A_TPUT(n_add)|C_TPUT(n_copy);
		}
		else
		{	i_copy = K_PUT(k_type);
			if(C_ISLOCAL(n_copy) )
				i_copy |= C_LPUT(n_copy);
		}
	}

	if(n_add > 0)
	{	/**/DBTOTAL(N_add,1); DBTOTAL(S_add,n_add); DBMAX(M_add,n_add);

		if(!MERGABLE(n_add,n_copy,k_type) )
			i_add = A_ISLOCAL(n_add) ? A_LPUT(n_add) : 0;

		STRPUTC(tab, i_add);
		if(!A_ISLOCAL(n_add) )
			STRPUTU(tab, (ulong)A_PUT(n_add), buf);
		STRWRITE(tab, begs, n_add);
	}

	if(n_copy > 0)
	{	if(!MERGABLE(n_add,n_copy,k_type))
			STRPUTC(tab, i_copy);

		if(!C_ISLOCAL(n_copy) )
			STRPUTU(tab, (ulong)C_PUT(n_copy), buf);

		if(k_type >= K_QUICK && k_type < (K_QUICK+K_QTYPE) )
			STRPUTC(tab, (ng_byte)best);
		else	STRPUTU(tab, (ulong)best, buf);
	}

	return 0;
}


/* Fold a string */
static int
vdfold(Table_t* tab)
{
	reg ulong	key, n;
	reg ng_byte	*s, *sm, *ends, *ss, *heade;
	reg Match_t	*m, *list, *curm, *bestm;
	reg ng_byte	*add, *endfold;
	reg int		head, len;
	reg int		size = tab->size;
	reg ng_byte	*tar = tab->tar;
	reg Match_t	*base = tab->base, **table = tab->table;

	endfold = (s = tar) + tab->n_tar;
	curm = base;
	if(tab->n_tar < M_MIN)
		return vdputinst(tab,s,endfold,NIL(Match_t*),0);

	add = NIL(ng_byte*);
	bestm = NIL(Match_t*);
	len = M_MIN-1;
	HINIT(key,s,n);
	for(;;)
	{	for(;;)	/* search for the longest match */
		{	if(!(m = table[key&size]) )
				goto endsearch;
			list = m = m->next;	/* head of list */

			if(bestm) /* skip over past elements */
			{	for(;;)
				{	if(m >= bestm+len)
						break;
					if((m = m->next) == list)
						goto endsearch;
				}
			}

			head = len - (M_MIN-1); /* header before the match */
			heade = s+head;
			for(;;)
			{	
				if((n = m-base) < head)
					goto next;
				sm = tar + n;

				/* make sure that the M_MIN ng_bytes match */
				if(!EQUAL(heade,sm))
					goto next;

				/* make sure this is a real match */
				for(sm -= head, ss = s; ss < heade; )
					if(*sm++ != *ss++)
						goto next;
				ss += M_MIN;
				sm += M_MIN;
				ends = endfold;
				for(; ss < ends; ++ss, ++sm)
					if(*sm != *ss)
						goto extend;
				goto extend;

			next:	if((m = m->next) == list )
					goto endsearch;
			}

		extend: bestm = m-head;
			n = len;
			len = ss-s;
			if(ss >= endfold)	/* already match everything */
				goto endsearch;

			/* check for a longer match */
			ss -= M_MIN-1;
			if(len == n+1)
				HNEXT(key,ss,n);
			else	HINIT(key,ss,n);
		}

	endsearch:
		if(bestm)
		{	if(vdputinst(tab,add,s,bestm,len) < 0)
				return -1;

			/* add a sufficient number of suffices */
			ends = (s += len);
			ss = ends - (M_MIN-1);
			curm = base + (ss-tar);

			len = M_MIN-1;
			add = NIL(ng_byte*);
			bestm = NIL(Match_t*);
		}
		else
		{	if(!add)
				add = s;
			ss = s;
			ends = (s += 1);	/* add one prefix */
		}

		if(ends > (endfold - (M_MIN-1)) )
			ends = endfold - (M_MIN-1);

		if(ss < ends) for(;;)	/* add prefices/suffices */
		{	n = key&size;
			if(!(m = table[n]) )
				curm->next = curm;
			else
			{	curm->next = m->next;
				m->next = curm;
			}
			table[n] = curm++;

			if((ss += 1) >= ends)
				break;
			HNEXT(key,ss,n);
		}

		if(s > endfold-M_MIN)	/* too short to match */
		{	if(!add)
				add = s;
			break;
		}

		HNEXT(key,s,n);
	}

	return vdputinst(tab,add,endfold,NIL(Match_t*),0);
}

int
vdsqueeze(void* target, reg int size, void* delta)
{
	reg int		n;
	Table_t		tab;
	ng_byte		buf[sizeof(long)+1];

	if(size <= 0)
		return 0;
	else if(!target || !delta)
		return -1;

	tab.n_tar = size;
	tab.tar = (ng_byte*)target;
	tab.delta = (ng_byte*)delta;
	tab.base = NIL(Match_t*);
	tab.table = NIL(Match_t**);

	if(!(tab.base = (Match_t*)malloc(size*sizeof(Match_t))) )
		return -1;

	/* space for the hash table */
	n = size/2;
	do (size = n); while((n &= n-1) != 0);
	if(size < 64)
		size = 64;
	if(!(tab.table = (Match_t**)malloc(size*sizeof(Match_t*))) )
	{	free((void*)tab.base);
		return -1;
	}
	else	tab.size = size-1;

	/* initialize table before processing */
	for(n = tab.size; n >= 0; --n)
		tab.table[n] = NIL(Match_t*);
	K_INIT(tab.quick,tab.recent,tab.rhere);

	STRPUTU(&tab,tab.n_tar,buf);
	n = vdfold(&tab);

	free((void*)tab.base);
	free((void*)tab.table);

	return n < 0 ? -1 : (tab.delta - (ng_byte*)delta);
}
