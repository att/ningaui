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
#if __STD_C
void t_error(char* form, ...)
#else
void t_error(va_alist)
va_dcl
#endif
{
	char	*s, buf[1024];
	int	n;
	va_list	args;
#if __STD_C
	va_start(args,form);
#else
	char*	form;
	va_start(args);
	form = va_arg(args,char*);
#endif

	s = buf; memset(buf, 0, sizeof(buf));
#ifdef __LINE__
	sprintf(s,"Line=%d: ", Sftline);
	s += strlen(s);
#endif
	vsprintf(s,form,args);
	va_end(args);

	if((n = strlen(buf)) > 0)
	{
#if _PACKAGE_ast
		if(buf[n-1] == '\n')
			buf[n-1] = ' ';
#else
		if(buf[n-1] != '\n')
		{	buf[n] = '\n';
			n += 1;
		}
#endif
		write(2,buf,n);
	}

	sftcleanup();
	exit(1);
}
