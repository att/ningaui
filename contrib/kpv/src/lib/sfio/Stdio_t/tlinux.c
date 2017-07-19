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
#include "stdtest.h"
#include <time.h>

MAIN()
{
	struct tm*	tmbufp;
	time_t		l1, l2;
	FILE*		f;
	char		buf[1024];

	if(!(f = fopen(tstfile(0),"w+")) )
		terror("Open file to write");

	/* these sometimes call rare Stdio functions such as fread_unlocked.
	   Hopefully, iffe did catch these and built them.
	*/
	l2 = time(&l1);
	tmbufp = localtime(&l2);

	fprintf(f, "%ld %ld %p", l1, l2, tmbufp);
	fseek(f, 0L, 0);
	fread(buf, 1, sizeof(buf), f);

	TSTEXIT(0);
}
