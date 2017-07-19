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
	int	i;
	char	wbuf[1023];
	char	rbuf[1023];
	FILE	*fp;

	for(i = 0; i < sizeof(wbuf); ++i)
		wbuf[i] = (i%26)+'a';
	wbuf[sizeof(wbuf)-1] = '\0';

	if(!(fp = fopen(tstfile(0), "w+")) )
		terror("Opening temp file\n");

	for(i = 0; i < 256; ++i)
		if(fwrite(wbuf,sizeof(wbuf),1,fp) != 1)
			terror("Writing\n");

	fseek(fp,(long)0,0);

	for(i = 0; i < 256; ++i)
	{	if(fread(rbuf,sizeof(rbuf),1,fp) != 1)
			terror("Reading\n");

		if(strcmp(rbuf,wbuf) != 0)
			terror("Unmatched record\n");
	}

	TSTEXIT(0);
}
