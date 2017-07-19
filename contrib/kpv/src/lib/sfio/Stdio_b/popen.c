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

/*	Opening a process as a stream
**	Written by Kiem-Phong Vo
*/


#if __STD_C
FILE* popen(const char* string,const char* mode)
#else
FILE* popen(string,mode)
char*	string;
char*	mode;
#endif
{
	reg Sfio_t*	sf;
	reg FILE*	f;

	if(!(sf = sfpopen((Sfio_t*)(-1), string, mode)))
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
