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
#include	"stdtest.h"


MAIN()
{	FILE	*f;
	int	i, c;

	if(!(f = fopen(tstfile(0), "w+")) )
		terror("Can't open temp file\n");

	for(i = 0; i < 10000; ++i)
		if(fputc((i%26)+'a', f) < 0)
			terror("Writing %c\n",(i%26)+'a');

	fseek(f, 0L, 0);

	for(i = 0; i < 10000; ++i)
		if((c = fgetc(f)) != ((i%26)+'a'))
			terror("Input=%#o, Expect=%c\n",c,(i%26)+'a');

	TSTEXIT(0);
}
