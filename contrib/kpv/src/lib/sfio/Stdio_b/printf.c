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

/*	Write out data to stdout using a given format.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int printf(const char* form, ...)
#else
int printf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
	reg Sfio_t*	sf;
#if __STD_C
	va_start(args,form);
#else
	reg char*	form;	/* print format */
	va_start(args);
	form = va_arg(args,char*);
#endif

	if(!form || !(sf = _sfstream(stdout)))
		return -1;

	if((rv = (int)sfvprintf(sf,form,args)) < 0)
		_stdseterr(stdout,sf);

	va_end(args);

	return rv;
}
