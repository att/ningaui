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

/*	Get current stream position.
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int fgetpos(reg FILE* f, reg stdfpos_t* pos)
#else
int fgetpos(f, pos)
reg FILE*	f;
reg stdfpos_t*	pos;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;
	return (*pos = (stdfpos_t)sfseek(sf,(Sfoff_t)0,SEEK_CUR|SF_SHARE)) >= 0 ? 0 : -1;
}

#if _lib___fgetpos64 && !_done___fgetpos64 && !defined(fgetpos)
#define _done___fgetpos64	1
#define fgetpos			__fgetpos64
#include			"fgetpos.c"
#undef fgetpos
#endif

#if _lib_fgetpos64 && !_done_fgetpos64 && !defined(fgetpos)
#define _done_fgetpos64	1
#define fgetpos			fgetpos64
#include			"fgetpos.c"
#undef fgetpos
#endif
