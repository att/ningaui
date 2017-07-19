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

/*	Clear error status from a stream
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
void clearerr(FILE* f)
#else
void clearerr(f)
FILE*	f;
#endif
{
	reg Sfio_t*	sf;
	
	if(f && (sf = _sfstream(f)) )
	{	sfclrlock(sf);
		_stdclrerr(f);
	}
}

#if _lib_clearerr_unlocked && !_done_clearerr_unlocked && !defined(clearerr)
#define _done_clearerr_unlocked	1
#define clearerr	clearerr_unlocked
#include		"clearerr.c"
#undef clearerr
#endif
