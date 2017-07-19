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

/*	Open a stream given a file descriptor.
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
Sfio_t* _stdpopen(const char* file, reg const char* mode)
#else
Sfio_t* _stdpopen(file,mode)
reg char*	file;
reg char*	mode;
#endif
{
	Sfio_t*	f;

	if((f = sfpopen((Sfio_t*)(-1), file, mode)) )
	{	int	uflag;
		_sftype(mode, NIL(int*), &uflag);
		if(!uflag)
			f->flags |= SF_MTSAFE;
	}

	return f;
}
