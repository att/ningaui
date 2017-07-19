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

#define NIL(type)	((type)0)

/* magic header for delta output */
#define VD_MAGIC	"vd01"

#include	<stddef.h>

#ifdef DEBUG
_BEGIN_EXTERNS_
extern int		abort();
_END_EXTERNS_
#define ASSERT(p)	((p) ? 0 : abort())
#define DBTOTAL(t,v)	((t) += (v))
#define DBMAX(m,v)	((m) = (m) > (v) ? (m) : (v) )
#else
#define ASSERT(p)
#define DBTOTAL(t,v)
#define DBMAX(m,v)
#endif

/* short-hand notations */
#define reg		register
#define uchar		unsigned char
#define uint		unsigned int
#define ulong		unsigned long

/* default window size - Chosen to suit malloc() even on 16-bit machines. */
#define MAXINT		((int)(((uint)~0) >> 1))
#define MAXWINDOW	((int)(((uint)~0) >> 2))
#define DFLTWINDOW	(MAXWINDOW <= (1<<14) ? (1<<14) : (1<<16) )
#define HEADER(w)	((w)/4)

#define M_MIN		4	/* min number of gk_bytes to match	*/

/* The hash function is s[0]*alpha^3 + s[1]*alpha^2 + s[2]*alpha + s[3] */
#define	ALPHA		33
#if 0
#define A1(x,t)		(ALPHA*(x))
#define A2(x,t)		(ALPHA*ALPHA*(x))
#define A3(x,t)		(ALPHA*ALPHA*ALPHA*(x))
#else	/* fast multiplication using shifts&adds */
#define A1(x,t)		((t = (x)), (t + (t<<5)) )
#define A2(x,t)		((t = (x)), (t + (t<<6) + (t<<10)) )
#define A3(x,t)		((t = (x)), (t + (t<<5) + ((t+(t<<4))<<6) + ((t+(t<<4))<<11)) )
#endif
#define HINIT(h,s,t)	((h = A3(s[0],t)), (h += A2(s[1],t)), (h += A1(s[2],t)+s[3]) )
#define HNEXT(h,s,t)	((h -= A3(s[-1],t)), (h = A1(h,t) + s[3]) )

#define EQUAL(s,t)	((s)[0] == (t)[0] && (s)[1] == (t)[1] && \
			 (s)[2] == (t)[2] && (s)[3] == (t)[3] )

/* Every instruction will start with a control gk_byte.
** For portability, only 8 bits of the gk_byte are used.
** The bits are used as follows:
**	iiii ssss
** ssss: size of data involved.
** iiii: this defines 16 instruction types:
**	0: an ADD instruction.
**	1,2,3: COPY with K_QUICK addressing scheme.
**	4,5: COPY with K_SELF,K_HERE addressing schemes.
**	6,7,8,9: COPY with K_RECENT addressing scheme.
**		For the above types, ssss if not zero codes the size;
**		otherwise, the size is coded in subsequent gk_bytes.
**	10,11: merged ADD/COPY with K_SELF,K_HERE addressing
**	12,13,14,15: merged ADD/COPY with K_RECENT addressing.
**		For merged ADD/COPY instructions, ssss is divided into "cc aa"
**		where cc codes the size of COPY and aa codes the size of ADD.
*/

#define VD_BITS		8	/* # bits usable in a gk_byte		*/

#define S_BITS		4	/* bits for the size field		*/
#define I_BITS		4	/* bits for the instruction type	*/

/* The below macros compute the coding for a COPY address.
** There are two caches, a "quick" cache of (K_QTYPE*256) addresses
** and a revolving cache of K_RTYPE "recent" addresses.
** First, we look in the quick cache to see if the address is there.
** If so, we use the cache index as the code.
** Otherwise, we compute from 0, the current location and
** the "recent" cache an address that is closest to the being coded address,
** then code the difference. The type is set accordingly.
**
** An invariance is 2*K_MERGE + K_QTYPE + 1 == 16
*/
#define K_RTYPE		4		/* # of K_RECENT types		*/
#define K_QTYPE		3		/* # of K_QUICK types		*/
#define K_MERGE		(K_RTYPE+2)	/* # of types allowing add+copy	*/
#define K_QSIZE		(K_QTYPE<<VD_BITS) /* size of K_QUICK cache	*/

#define K_QUICK		1		/* start of K_QUICK types	*/
#define K_SELF		(K_QUICK+K_QTYPE)
#define K_HERE		(K_SELF+1)
#define K_RECENT	(K_HERE+1)	/* start of K_RECENT types	*/

#define K_DDECL(quick,recent,rhere) 	/* cache decls in vdelta	*/ \
	int quick[K_QSIZE]; int recent[K_RTYPE]; int rhere/*;*/
#define K_UDECL(quick,recent,rhere) 	/* cache decls in vdupdate	*/ \
	long quick[K_QSIZE]; long recent[K_RTYPE]; int rhere/*;*/
#define K_INIT(quick,recent,rhere) \
	{ quick[rhere=0] = (1<<7); \
	  while((rhere += 1) < K_QSIZE) quick[rhere] = rhere + (1<<7); \
	  recent[rhere=0] = (1<<8); \
	  while((rhere += 1) < K_RTYPE) recent[rhere] = (rhere+1)*(1<<8); \
	}
#define K_UPDATE(quick,recent,rhere,copy) \
	{ quick[copy%K_QSIZE] = copy; \
	  if((rhere += 1) >= K_RTYPE) rhere = 0; recent[rhere] = copy; \
	}

#define VD_ISCOPY(k)	((k) > 0 && (k) < (K_RECENT+K_RTYPE) )
#define K_ISMERGE(k)	((k) >= (K_RECENT+K_RTYPE))

#define A_SIZE		((1<<S_BITS)-1)		/* max local ADD size	*/
#define A_ISLOCAL(s)	((s) <= A_SIZE )	/* can be coded locally	*/
#define A_LPUT(s)	(s)			/* coded local value	*/
#define A_PUT(s)	((s) - (A_SIZE+1) )	/* coded normal value	*/

#define A_ISHERE(i)	((i) & A_SIZE)		/* locally coded size	*/
#define A_LGET(i)	((i) & A_SIZE)
#define A_GET(s)	((s) + (A_SIZE+1) )

#define C_SIZE		((1<<S_BITS)+M_MIN-2)	/* max local COPY size	*/
#define C_ISLOCAL(s)	((s) <= C_SIZE )	/* can be coded locally	*/
#define C_LPUT(s)	((s) - (M_MIN-1) )	/* coded local value	*/
#define C_PUT(s)	((s) - (C_SIZE+1) )	/* coded normal value	*/

#define C_ISHERE(i)	((i) & ((1<<S_BITS)-1)) /* size was coded local */
#define C_LGET(i)	(((i) & ((1<<S_BITS)-1)) + (M_MIN-1) )
#define C_GET(s)	((s) + (C_SIZE+1) )

#define K_PUT(k)	((k) << S_BITS)
#define K_GET(i)	((i) >> S_BITS)

/* coding merged ADD/COPY instructions */
#define A_TINY		2		/* bits for tiny ADD		*/
#define A_TINYSIZE	(1<<A_TINY) 		/* max tiny ADD size	*/
#define A_ISTINY(s)	((s) <= A_TINYSIZE )
#define A_TPUT(s)	((s) - 1)
#define A_TGET(i)	(((i) & (A_TINYSIZE-1)) + 1)

#define C_TINY		2		/* bits for tiny COPY		*/
#define C_TINYSIZE	((1<<C_TINY) + M_MIN-1)	/* max tiny COPY size	*/
#define C_ISTINY(s)	((s) <= C_TINYSIZE)
#define C_TPUT(s)	(((s) - M_MIN) << A_TINY)
#define C_TGET(i)	((((i) >> A_TINY) & ((1<<C_TINY)-1)) + M_MIN )

#define K_TPUT(k)	(((k)+K_MERGE) << S_BITS)

#define MEMCPY(to,from,n) \
	switch(n) \
	{ default: memcpy((void*)to,(void*)from,(size_t)n); \
		   to += n; from += n; break; \
	  case 7 : *to++ = *from++; \
	  case 6 : *to++ = *from++; \
	  case 5 : *to++ = *from++; \
	  case 4 : *to++ = *from++; \
	  case 3 : *to++ = *from++; \
	  case 2 : *to++ = *from++; \
	  case 1 : *to++ = *from++; \
	  case 0 : break; \
	}

/* Below here is code for a buffered I/O subsystem to speed up I/O */
#define I_SHIFT		7
#define I_MORE		(1<<I_SHIFT)			/* continuation bit	*/
#define I_CODE(n)	((uchar)((n)&(I_MORE-1)) )	/* get lower bits	*/

/* fast string I/O */
#define STRPUTC(tab,c)		(*(tab)->delta++ = (uchar)(c))
#define STRWRITE(tab,s,n)	{memcpy((tab)->delta,(s),(n)); (tab)->delta += (n);}
#define STRPUTU(tab,u,buf) \
	{	reg uchar *s, *endb; reg ulong v = (u); \
		s = (endb = &buf[sizeof(buf)])-1; *s = I_CODE(v); \
		while((v >>= I_SHIFT)) *--s = I_CODE(v)|I_MORE; v = endb-s;\
		memcpy((tab)->delta,s,v); (tab)->delta += v; \
	}
#define STRGETC(tab)		(*(tab)->delta++)
#define STRREAD(tab,s,n)	{memcpy((s),(tab)->delta,(n)); (tab)->delta += (n);}
#define STRGETU(tab,u) \
	{	reg int c; \
		for(u = 0;; ) \
		{	c = *(tab)->delta++; u = (u<<I_SHIFT) | (c & (I_MORE-1)); \
			if(!(c&I_MORE)) break; \
		} \
	}
