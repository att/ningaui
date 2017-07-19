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

/*	Opening a stream
**	Written by Kiem-Phong Vo
*/

#if __STD_C
FILE* fopen(char* name,const char* mode)
#else
FILE* fopen(name,mode)
char*	name;
char*	mode;
#endif
{
	reg FILE*	f;
	reg Sfio_t*	sf;

	if(!(sf = sfopen(NIL(Sfio_t*), name, mode)) )
		f = NIL(FILE*);
	else if(!(f = _stdstream(sf, NIL(FILE*))))
		sfclose(sf);
	else
	{	int	uflag;
		_sftype(mode, NIL(int*), &uflag);
		if(!uflag)
			sf->flags |= SF_MTSAFE;
	}

	return(f);
}

#if _lib___fopen64 && !_done___fopen64 && !defined(fopen)
#define _done___fopen64	1
#define fopen		__fopen64
#include		"fopen.c"
#undef fopen
#endif

#if _lib_fopen64 && !_done_fopen64 && !defined(fopen)
#define _done_fopen64	1
#define fopen		fopen64
#include		"fopen.c"
#undef fopen
#endif
