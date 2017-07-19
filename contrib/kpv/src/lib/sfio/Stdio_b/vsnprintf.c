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

/*	Internal printf engine to write to a string buffer
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int vsnprintf(char* s, size_t n, const char* form, va_list args)
#else
int vsnprintf(s,n,form,args)
reg char*	s;
reg size_t	n;
reg char*	form;
va_list		args;
#endif
{
	return (s && form) ? (int)sfvsprintf(s,n,form,args) : -1;
}

#if _lib___vsnprintf && !done_lib___vsnprintf && !defined(vsnprintf)
#define done_lib___vsnprintf 1
#define vsnprintf __vsnprintf
#include        "vsnprintf.c"
#endif

