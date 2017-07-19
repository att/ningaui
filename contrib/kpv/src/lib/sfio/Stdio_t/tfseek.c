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
{
	FILE	*fp;
	int	i, c, e;

	if(!(fp = fopen(tstfile(0), "w+")) )
		terror("Can't open temp file\n");

	for(i = 0; i < 256; ++i)
		if(putc(i, fp) < 0)
			terror("Bad putc\n");

	for(i = 1; i < 255; ++i)
	{	if(fseek(fp, (long)(-i), SEEK_END) < 0)
			terror("fseek seek_end failed\n");
		if((c = getc(fp)) != (e = 256-i) )
			terror("Bad getc: expect %d, get %d\n", e, c);

		if(fseek(fp, (long)(i), SEEK_SET) < 0)
			terror("fseek seek_set failed\n");
		if((c = getc(fp)) != i)
			terror("Bad getc: expect %d, get %d\n", i, c);

		if(fseek(fp, (long)(-1), SEEK_CUR) < 0)
			terror("fseek seek_cur failed\n");

		if((c = getc(fp)) != i )
			terror("Bad getc: expect %d, get %d\n", i, c);
	}

	TSTEXIT(0);
}
