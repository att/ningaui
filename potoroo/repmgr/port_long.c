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

/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/

/*	Write out a long value in a portable format
**
**	Written by Kiem-Phong Vo.
*/

/* man this is a hack....
	trying to avoid using the ast, ive snarfed the sfio portable 
	long routine, and butchered it into something that shoudl 
	create a buffer with the long rather than writing it to a 
	stream.  Ive had to hard include things etc, etc. 
*/
#include <sfio.h>
#define SFUVALUE(v)     (((unsigned long)(v))&(SF_MORE-1))
#define SFSVALUE(v)     ((( long)(v))&(SF_SIGN-1))
#define SFBVALUE(v)     (((unsigned long)(v))&(SF_BYTE-1))


int port_long( long v, unsigned char *b )
{
#define N_ARRAY		(2*sizeof(Sflong_t))
	unsigned char	*s, *ps;
	ssize_t	n, p;
	unsigned char		c[N_ARRAY];

	s = ps = &(c[N_ARRAY-1]);
	if(v < 0)
	{	/* add 1 to avoid 2-complement problems with -SF_MAXINT */
		v = -(v+1);
		*s = (unsigned char )(SFSVALUE(v) | SF_SIGN);
	}
	else	*s = (unsigned char)(SFSVALUE(v));
	v = (Sfulong_t)v >> SF_SBITS;

	while(v > 0)
	{	
		*--s = (unsigned char)(SFUVALUE(v) | SF_MORE);
		v = (Sfulong_t)v >> SF_UBITS;
	}
	n = (ps-s)+1;

	memcpy( b, s, n );
	return n;
}
