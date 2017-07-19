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

/*	Opening a stream given a file descriptor.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
FILE* fdopen(int fd, const char* mode)
#else
FILE* fdopen(fd,mode)
int	fd;
char*	mode;
#endif
{
	reg Sfio_t*	sf;
	reg FILE*	f;
	int		flags, uflag;

	if((flags = _sftype(mode,NIL(int*),&uflag)) == 0)
		return NIL(FILE*);
	if(!uflag)
		flags |= SF_MTSAFE;

	if(!(sf = sfnew(NIL(Sfio_t*), NIL(Void_t*), (size_t)SF_UNBOUND, fd, flags)))
		return NIL(FILE*);
	if(!(f = _stdstream(sf, NIL(FILE*))))
	{	sfclose(sf);
		return NIL(FILE*);
	}

	return(f);
}
