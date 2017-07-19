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

/*	Read formatted data from a stream
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int _stdscanf(const char* form, ...)
#else
int _stdscanf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;

#if __STD_C
	va_start(args,form);
#else
	reg char	*form;	/* scanning format */
	va_start(args);
	form = va_arg(args,char*);
#endif

	rv = form ? (int)sfvscanf(sfstdin,form,args) : -1;
	va_end(args);
	return rv;
}
