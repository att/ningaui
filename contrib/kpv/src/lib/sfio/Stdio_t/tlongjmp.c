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
#include	<setjmp.h>
#include	<signal.h>

/* this tests that stdio streams will not be locked even with longjmp */

jmp_buf	Env;
int	Fd[2];
char	Str[] = "Signal handler\n";

#if __STD_C
void sighandler(int sig)
#else
void sighandler(sig)
int	sig;
#endif
{
	if(write(Fd[1], Str, strlen(Str)) != strlen(Str))
		terror("Writing to pipe failed");
	longjmp(Env,0);
}

MAIN()
{
	FILE*	f;
	char*	s;
	char	buf[128];

	if(pipe(Fd) < 0)
		terror("Can't create pipe");
	if(!(f = fdopen(Fd[0],"r")))
		terror("Can't open read stream");

	if(setjmp(Env) == 0)
	{	/* set alarm handler */
		tmesg("\tIf hung, send interrupt or quit signal\n");
		signal(SIGALRM,sighandler);
		signal(SIGHUP,sighandler);
		signal(SIGQUIT,sighandler);
		alarm(2);

		/* we expect this to block on read */
		fgets(buf,sizeof(buf),f);
		terror("fgets did not block!");
	}
	else /* got back from longjmp() */
	{	
#if _SFIO_H
		if((f->mode&SF_RDWR) == f->mode)
			terror("Stream should be locked");
#endif
		clearerr(f); /* make sure that stream is ok for IO */

		/* this fgets should succeed */
		if(!(s = fgets(buf,sizeof(buf),f)) )
			terror("fgets returned NULL");
		if(strcmp(s,Str) != 0)
			terror("fgets returned wrong data\n");
	}

	TSTEXIT(0);
}
