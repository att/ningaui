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

/*	Output a string.
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int fputs(const char* s, FILE* f)
#else
int fputs(s, f)
reg char*	s;
reg FILE*	f;
#endif
{
	reg int		rv;
	reg Sfio_t*	sf;

	if(!s || !(sf = _sfstream(f)))
		return -1;

	if((rv = sfputr(sf,s,-1)) < 0)
		_stdseterr(f,sf);
	return rv;
}

#if _lib_fputs_unlocked && !_done_fputs_unlocked && !defined(fputs)
#define _done_fputs_unlocked	1
#define fputs	fputs_unlocked
#include	"fputs.c"
#undef fputs
#endif
