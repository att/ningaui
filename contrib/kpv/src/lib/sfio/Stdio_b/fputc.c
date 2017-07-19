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

/*	Write out a byte
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int fputc(int c, FILE* f)
#else
int fputc(c, f)
reg int		c;
reg FILE*	f;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;

	if((c = sfputc(sf,c)) < 0)
		_stdseterr(f,sf);
	return c;
}

#if _lib__IO_putc && !_done_IO_putc && !defined(fputc)
#define _done_IO_putc	1
#define fputc	_IO_putc
#include	"fputc.c"
#undef fputc
#endif

#if _lib_fputc_unlocked && !_done_fputc_unlocked && !defined(fputc)
#define _done_fputc_unlocked	1
#define fputc	fputc_unlocked
#include	"fputc.c"
#undef fputc
#endif
