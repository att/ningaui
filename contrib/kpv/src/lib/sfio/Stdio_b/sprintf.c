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

/*	Format a string
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int sprintf(char* s,  const char* form, ...)
#else
int sprintf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
#if __STD_C
	va_start(args,form);
#else
	reg char*	s;
	reg char*	form;
	va_start(args);
	s = va_arg(args,char*);
	form = va_arg(args,char*);
#endif

	rv = (s && form) ? (int)sfvsprintf(s,SF_MAXINT,form,args) : -1;
	va_end(args);

	return rv;
}

#if __STD_C
int asprintf(char** as,  const char* form, ...)
#else
int asprintf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
#if __STD_C
	va_start(args,form);
#else
	reg char**	as;
	reg char*	form;
	va_start(args);
	as = va_arg(args,char**);
	form = va_arg(args,char*);
#endif

	rv = (as && form) ? (int)sfvaprints(as,form,args) : -1;
	va_end(args);

	return rv;
}
