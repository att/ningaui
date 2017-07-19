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

/*	Get a line.
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
char* fgets(char* buf, int n, FILE* f)
#else
char* fgets(buf,n,f)
char*	buf;
int	n;
FILE*	f;
#endif
{
	reg Sfio_t*	sf;
	reg char*	rv;

	if(!buf || !(sf = _sfstream(f)) )
		return NIL(char*);
	if(!(rv = _stdgets(sf,buf,n,0)))
		_stdseterr(f,sf);
	return rv;
}

#if _lib_fgets_unlocked && !_done_fgets_unlocked && !defined(fgets)
#define _done_fgets_unlocked	1
#define fgets	fgets_unlocked
#include	"fgets.c"
#undef fgets
#endif
