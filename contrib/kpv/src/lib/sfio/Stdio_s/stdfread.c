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
#include	"stdio_s.h"

/*	Read in a block of data
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
ssize_t _stdfread(Void_t* buf, size_t esize, size_t nelts, Sfio_t* f)
#else
ssize_t _stdfread(buf, esize, nelts, f)
Void_t*	buf;
size_t	esize;
size_t	nelts;
Sfio_t*	f;
#endif
{
	ssize_t	rv;

	if(!f || !buf)
		return -1;

	if((rv = sfread(f, buf, esize*nelts)) >= 0)
		return esize == 0 ? 0 : rv/esize;
	else	return 0;
}
