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

/*	Stdio function setvbuf()
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int _stdsetvbuf(Sfio_t* f, char* buf, int type, size_t size)
#else
int _stdsetvbuf(f,buf,type,size)
Sfio_t*	f;
char*	buf;
int	type;
size_t	size;
#endif
{
	SFMTXSTART(f,-1);

	if(type == _IOLBF)
	{	sfset(f,SF_LINE,1);
	}
	else if((f->flags&SF_STRING))
	{	SFMTXRETURN(f, -1);
	}
	else if(type == _IONBF)
	{	sfsync(f);
		sfsetbuf(f,NIL(Void_t*),0);
		sfset(f,SF_LINE,0);
	}
	else if(type == _IOFBF)
	{	if(size == 0)
			size = SF_BUFSIZE;
		sfsync(f);
		sfsetbuf(f,(Void_t*)buf,size);
		sfset(f,SF_LINE,0);
	}

	SFMTXRETURN(f, 0);
}
