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

/*	Scan input data using a given format
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int fscanf(FILE* f, const char* form, ...)
#else
int fscanf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
	reg Sfio_t*	sf;

#if __STD_C
	va_start(args,form);
#else
	reg FILE*	f;	/* file to be scanned */
	reg char*	form;	/* scanning format */
	va_start(args);
	f = va_arg(args,FILE*);
	form = va_arg(args,char*);
#endif

	if(!form || !(sf = _sfstream(f)))
		return -1;

	if((rv = (int)sfvscanf(sf,form,args)) <= 0)
		_stdseterr(f,sf);

	va_end(args);

	return rv;
}
