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

/*	Write out a string to stdout.
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int puts(const char* str)
#else
int puts(str)
reg char*	str;
#endif
{
	reg int		rv;
	reg Sfio_t*	sf;

	if(!str || !(sf = _sfstream(stdout)))
		return -1;

	if((rv = sfputr(sfstdout,str,'\n')) < 0)
		_stdseterr(stdout,sf);
	return rv;	
}
