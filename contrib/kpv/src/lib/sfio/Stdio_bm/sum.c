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
#include	<sfio.h>

#ifdef __STDC__
main(int argc, char** argv)
#else
main(argc,argv)
int	argc;
char	**argv;
#endif
{
	Sfio_t	*f[10];
	int	i, n;
	char	*str, title[1024];
	double	real, user, sys, u_s;
	int	size, kbyte;
	double	u, s, us;
	int	sz, kb;
	char	c, func[128];

	for(n = 0; n < argc-1; ++n)
	{	if(!(f[n] = sfopen((Sfio_t*)0,argv[n+1],"r")))
			return -1;
		str = sfgetr(f[n],'\n',1);
	}

	sfprintf(sfstdout,"func\tuser\tsys\tu+s\tsize\tkbytes/s\n");
	while((str = sfgetr(f[0],'\n',1)) != (char*)0)
	{	sfsscanf(str,"%s%lf%lf%lf%lf%d%c%d",
			func,&real,&user,&sys,&u_s,&size,&c,&kbyte);
		for(i = 1; i < n; ++i)
		{	if(!(str = sfgetr(f[i],'\n',1)))
				return;
			sfsscanf(str,"%s%lf%lf%lf%lf%d%c%d",
				func,&real,&u,&s,&us,&sz,&c,&kb);
			user += u;
			sys  += s;
			if(kb >= 0)
				kbyte += kb;
		}
		user /= n;
		sys  /= n;
		u_s   = user+sys;
		if(c != 'K')
			kbyte /= n;
		else if(u_s > 0.)
			kbyte = size/u_s;
		else	kbyte = -1;
		
		sfprintf(sfstdout,"%s\t%.2f\t%.2f\t%.2f\t%d%c\t%d\n",
			func, user, sys, u_s, size, c, kbyte);
	}
}
