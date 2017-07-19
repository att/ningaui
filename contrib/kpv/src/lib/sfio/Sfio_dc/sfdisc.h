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
#ifndef _SFDISC_H
#define _SFDISC_H	1

#include	<sfio.h>

#if _PACKAGE_ast
#if _BLD_ast && defined(__EXPORT__)
#define extern          __EXPORT__
#endif
#endif

/* to create a user-defined event specific to some package */
#define SFDCEVENT(a,b,n)	((((a)-'A'+1)<<11)^(((b)-'A'+1)<<6)^(n))

_BEGIN_EXTERNS_
/* functions to create disciplines */
extern int	sfdcdio _ARG_((Sfio_t*, size_t));
extern int	sfdcdos _ARG_((Sfio_t*));
extern int	sfdcfilter _ARG_((Sfio_t*, const char*));
extern int	sfdcseekable _ARG_((Sfio_t*));
extern int	sfdcslow _ARG_((Sfio_t*));
extern int	sfdcsubstream _ARG_((Sfio_t*, Sfio_t*, Sfoff_t, Sfoff_t));
extern int	sfdctee _ARG_((Sfio_t*, Sfio_t*));
extern int	sfdcunion _ARG_((Sfio_t*, Sfio_t**, int));

extern int	sfdclzw _ARG_((Sfio_t*));
extern int	sfdcgzip(Sfio_t*, int);
_END_EXTERNS_

#if _PACKAGE_ast
#undef extern
#endif

#endif
