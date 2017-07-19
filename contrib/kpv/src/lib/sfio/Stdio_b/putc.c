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
int putc(int c, FILE* f)
#else
int putc(c, f)
reg int		c;
reg FILE*	f;
#endif
{
	return fputc(c,f);
}

#if _lib_putc_unlocked && !_done_putc_unlocked && !defined(putc)
#define _done_putc_unlocked	1
#define putc	putc_unlocked
#include	"putc.c"
#undef putc
#endif
