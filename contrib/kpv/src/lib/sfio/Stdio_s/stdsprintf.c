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


/*	sprintf functions
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int _stdvsnprintf(char* s, size_t n, const char* form, va_list args)
#else
int _stdvsnprintf(s, n, form, args)
char*	s;
size_t	n;
char*	form;
va_list	args;
#endif
{
	return (int)sfvsprintf(s,n,form,args);
}


#if __STD_C
int _stdsnprintf(char* s, size_t n, const char* form, ...)
#else
int _stdsnprintf(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;

#if __STD_C
	va_start(args,form);
#else
	reg char*	s;
	reg size_t	n;
	reg char*	form;
	va_start(args);
	s = va_arg(args,char*);
	n = va_arg(args,size_t);
	form = va_arg(args,char*);
#endif

	rv = (int)sfvsprintf(s,n,form,args);

	va_end(args);

	return rv;
}

#if __STD_C
int _stdvasprintf(char** as, const char* form, va_list args)
#else
int _stdvasprintf(as, form, args)
char**	as;
char*	form;
va_list	args;
#endif
{
	return (int)sfvaprints(as, form, args);
}


#if __STD_C
int _stdasprintf(char** as, const char* form, ...)
#else
int _stdasprintf(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;

#if __STD_C
	va_start(args,form);
#else
	reg char**	as;
	reg char*	form;
	va_start(args);
	as = va_arg(args,char**);
	form = va_arg(args,char*);
#endif

	rv = (int)sfvaprints(as, form, args);

	va_end(args);

	return rv;
}


#if __STD_C
int _stdsprintf(char* s, const char* form, ...)
#else
int _stdsprintf(va_alist)
va_dcl
#endif
{
	va_list	args;
	reg int	rv;

#if __STD_C
	va_start(args,form);
#else
	reg char*	s;
	reg char*	form;
	va_start(args);
	s = va_arg(args,char*);
	form = va_arg(args,char*);
#endif

	rv = (s && form) ? (int)sfvsprintf(s,SF_BUFSIZE,form,args) : -1;

	va_end(args);

	return rv;
}
