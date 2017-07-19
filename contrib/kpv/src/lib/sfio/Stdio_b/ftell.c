/*
* 
* ************************************************************************
* *                                                                      *
* *               This software is part of the ast package               *
* *          Copyright (c) 1985-2008 AT&T Intellectual Property          *
* *                      and is licensed under the                       *
* *                  Common Public License, Version 1.0                  *
* *                    by AT&T Intellectual Property                     *
* *                                                                      *
* *                A copy of the License is available at                 *
* *            http://www.opensource.org/licenses/cpl1.0.txt             *
* *         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
* *                                                                      *
* *              Information and Software Systems Research               *
* *                            AT&T Research                             *
* *                           Florham Park NJ                            *
* *                                                                      *
* *                 Glenn Fowler <gsf@research.att.com>                  *
* *                  David Korn <dgk@research.att.com>                   *
* *                   Phong Vo <kpv@research.att.com>                    *
* *                                                                      *
* ************************************************************************
*/
#include	"sfstdio.h"

/*	Tell current IO position pointer
**	Written by Kiem-Phong Vo
*/

#ifndef lcloff_t
#define lcloff_t	long
#endif

#if __STD_C
lcloff_t ftell(reg FILE* f)
#else
lcloff_t ftell(f)
reg FILE*	f;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return (lcloff_t)(-1);

	return (lcloff_t)sfseek(sf, (Sfoff_t)0, SEEK_CUR|SF_SHARE);
}


#if _lib_ftello && !_done_ftello && !defined(ftell)
#define _done_ftello	1
#undef lcloff_t
#define lcloff_t	stdoff_t
#define ftell		ftello
#include		"ftell.c"
#undef ftell
#endif

#if _lib___ftello64 && !_done___ftello64 && !defined(ftell)
#define _done___ftello64	1
#undef lcloff_t
#define lcloff_t	stdoff_t
#define ftell			__ftello64
#include			"ftell.c"
#undef ftell
#endif

#if _lib___ftello64 && !_done_ftello64 && !defined(ftell)
#define _done_ftello64	1
#undef lcloff_t
#define lcloff_t	stdoff_t
#define ftell		ftello64
#include		"ftell.c"
#undef ftell
#endif
