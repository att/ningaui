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

/*	Creating a temporary stream.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
FILE* tmpfile(void)
#else
FILE* tmpfile()
#endif
{
	reg Sfio_t*	sf;
	reg FILE*	f;

	if(!(sf = sftmp(0)))
		f = NIL(FILE*);
	else if(!(f = _stdstream(sf, NIL(FILE*))))
		sfclose(sf);
	else	sf->flags |= SF_MTSAFE;

	return f;
}

#if _lib___tmpfile64 && !_done___tmpfile64 && !defined(tmpfile)
#define _done___tmpfile64	1
#define tmpfile			__tmpfile64
#include			"tmpfile.c"
#undef tmpfile
#endif

#if _lib_tmpfile64 && !_done_tmpfile64 && !defined(tmpfile)
#define _done_tmpfile64	1
#define tmpfile		tmpfile64
#include		"tmpfile.c"
#undef tmpfile
#endif
