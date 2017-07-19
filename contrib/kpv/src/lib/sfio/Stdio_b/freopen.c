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

/*	Reopening a stream
**	Written by Kiem-Phong Vo
*/


#if __STD_C
FILE* freopen(char*name, const char* mode, reg FILE* f)
#else
FILE* freopen(name,mode,f)
reg char*	name;
reg char*	mode;
reg FILE*	f;
#endif
{
	Sfio_t*	sf;

	if(f && (sf = _sfstream(f)) )
		_sfunmap(f);

	if(!(sf = sfopen(sf, name, mode)) )
		return NIL(FILE*);
	else
	{	int	uflag;
		_sftype(mode, NIL(int*), &uflag);	
		if(!uflag) 
			sf->flags |= SF_MTSAFE;

		if(_stdstream(sf,f) != f)
		{	sfclose(sf);
			return NIL(FILE*);
		}
		else	return f;
	}
}

#if _lib___freopen64 && !_done___freopen64 && !defined(freopen)
#define _done___freopen64	1
#define freopen		__freopen64
#include		"freopen.c"
#undef freopen
#endif

#if _lib_freopen64 && !_done_freopen64 && !defined(freopen)
#define _done_freopen64	1
#define freopen		freopen64
#include		"freopen.c"
#undef freopen
#endif
