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

#if __STD_C
int _stdfflush(FILE* f)
#else
int _stdfflush(f)
FILE*	f;
#endif
{
	if(!f)
		return -1;
	if(f->extent >= 0 && !(f->mode&SF_INIT))
		(void)sfseek(f, (Sfoff_t)0, SEEK_CUR|SF_PUBLIC);
	if((f->mode&SF_WRITE) && sfsync(f) < 0)
		return -1;
	if((f->mode&SF_READ) && sfpurge(f) < 0)
		return -1;
	return 0;
}
