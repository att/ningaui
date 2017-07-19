/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

#define	_POSIX_SOURCE
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<time.h>
#include	<math.h>

/*
 * argument processing
 */
#define	ARGBEGIN	for((argv0? 0: (argv0=*argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt, _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				while(*_args) switch(_argc=*_args++)
#define	ARGEND		}
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc

char *argv0;
typedef enum { Write, Read, Readwrite } Action;
int verbose = 0;
#define	MAXVAL	0x1fffffff		/* 32 bit max pos */
#define	scaleK	1024
#define	scaleM	(1024*1024)
#define	scaleG	(1024*1024*1024)


void test1(char *dev, Action action, int seq, int bsize, time_t tlimit, int nblks, char *op, char *order, int mode, int offset, int arena, char *com);		/* arena, offset in units of bsize */

int autosizes[] =
{
	4*scaleK,
	64*scaleK,
	256*scaleK,
	1*scaleM,
	4*scaleM,
	-1
};

int
factor(char *s, int bsize)
{
	switch(*s)
	{
	case 'b': case 'B':	return bsize;
	case 's': case 'S':	return 512;
	case 'k': case 'K':	return scaleK;
	case 'm': case 'M':	return scaleM;
	case 'g': case 'G':	return scaleG;
	}
	return 1;
}

int
scale(int q, int unit, int bsize)		/* express q*unit in blocks */
{
	double r;				/* we believe this will work */

	r = q;
	r *= unit;
	r /= bsize;
	return (int)r;				/* round down */
}

char *
pretty(int n, int bsize, char *res)
{
	double d;
#define	DIV(kk)	(fmod(d, (double)kk) < .001)

	d = n;
	d *= bsize;
	if(DIV(scaleG))
		sprintf(res, "%.0lfGB", d/scaleG + 0.1);
	else if(DIV(scaleM))
		sprintf(res, "%.0lfMB", d/scaleM + 0.1);
	else if(DIV(scaleK))
		sprintf(res, "%.0lfKB", d/scaleK + 0.1);
	else
		sprintf(res, "%.0lfB", d + 0.1);
	return res;
}

int main(int argc, char **argv)
{
	Action action = Read;
	int seq = 0;
	int bsize = -1;
	time_t tlimit = 15;
	int ntime;
	int mode = 0;
	time_t t0, tlast, t;
	int ntest;
	long nlast;
	long n;
	int nblock = -1;
	char *op = NULL;
	char *order = "rand";
	long b;
	off_t off;
	int autosize;
	int offset = 0;
	int k;
	int arena = 0;
	char *buf;
	char *com;
	char *amult = "", *omult = "", *bmult;

	com = 0;
	ARGBEGIN{
	case 'a':	arena = strtoul(ARGF(), &amult, 0); break;
	case 'b':	bsize = strtoul(ARGF(), &bmult, 0); bsize *= factor(bmult, 1); break;
	case 'c':	com = ARGF(); break;
	case 'm':	action = Readwrite; mode = O_RDWR; op = "R/M/W"; break;
	case 'n':	nblock = strtoul(ARGF(), &bmult, 0); nblock *= factor(bmult, 1); break;
	case 'o':	offset = strtoul(ARGF(), &omult, 0); break;
	case 'r':	action = Read; mode = O_RDONLY; op = "R"; break;
	case 's':	seq = 1; order = "seq"; break;
	case 't':	tlimit = strtoul(ARGF(), 0, 0); break;
	case 'v':	verbose++; break;
	case 'w':	action = Write; mode = O_WRONLY; op = "W"; break;
	default:	goto usage;
	}ARGEND

	if(argc != 1){
usage:
		fprintf(stderr, "Usage: %s [-r|-w|-m] [-s] [-b bs] [-o offset] [-t tlimit|-n nblks] [-a arena] disk\n", argv0);
		fprintf(stderr, "	-r|-w|-m	read(dflt), write, or read-write\n");
		fprintf(stderr, "	-s		sequential (dflt=random)\n");
		fprintf(stderr, "	-b bs		blocksize (dflt=");
		for(k = 0; autosizes[k] > 0; k++)
			fprintf(stderr, " %d", autosizes[k]);
		fprintf(stderr, ")\n");
		fprintf(stderr, "	-t tlimit	target time for run (dflt=15s)\n");
		fprintf(stderr, "	-n nblks	number of blocks to i/o (dflt is use -t)\n");
		fprintf(stderr, "	-o offset	offset of arena start (dflt=0)\n");
		fprintf(stderr, "	-a arena	disk size (dflt=measure by stat)\n");
		fprintf(stderr, "	-c comment	comment added to output lines\n");
		fprintf(stderr, "	all sizes are bytes with suffix(sS=512,kK=1024,mM=1048576,gG=1024M,bB=block)\n");
		exit(1);
	}
	if(bsize == -1)
		autosize = 1;
	else {
		if((bsize%512) || (bsize < 512)){
			fprintf(stderr, "%s: bad blocksize %d (must be multiple of 512)\n", argv0, bsize);
			exit(1);
		}
		autosize = 0;
	}
	if(tlimit < 1){
		fprintf(stderr, "%s: bad tlimit %d\n", argv0, tlimit);
		exit(1);
	}
	if(arena == 0){
		arena = 128;
		amult = "G";
	}
	if(arena < 1){
		fprintf(stderr, "%s: bad disksize %d\n", argv0, arena);
		exit(1);
	}
	if(nblock < 0)
		nblock = MAXVAL;
	else
		tlimit = MAXVAL;
	if(autosize){
		for(k = 0; autosizes[k] > 0; k++)
			test1(argv[0], action, seq, autosizes[k], tlimit, nblock, op, order, mode, scale(offset, factor(omult, autosizes[k]), autosizes[k]), scale(arena, factor(amult, autosizes[k]), autosizes[k]), com);
	} else
		test1(argv[0], action, seq, bsize, tlimit, nblock, op, order, mode, scale(offset, factor(omult, bsize), bsize), scale(arena, factor(amult, bsize), bsize), com);
	exit(0);
}

void
myseek(int fd, int b, int bsize)
{
	int slim;

	slim = MAXVAL/bsize;
	if(lseek(fd, (b%slim)*bsize, SEEK_SET) < 0){
		fprintf(stderr, "%s: seekerr on blk %d", argv0, b);
		perror("");
	}
	for(b -= b%slim; b > 0; b -= slim){
		if(lseek(fd, slim*bsize, SEEK_SET) < 0){
			fprintf(stderr, "%s: seekerr on blk %d", argv0, b);
			perror("");
		}
	}
}

void
test1(char *dev, Action action, int seq, int bsize, time_t tlimit, int nblks, char *op, char *order, int mode, int offset, int arena, char *com)
{
	int ntime;
	int fd;
	time_t t0, tlast, t, t1, t2;
	int ntest;
	long nlast;
	long n;
	long b;
	off_t off;
	int k;
	char *buf;
	char pb[512];

	srand(getpid()+(unsigned)time(0));
	buf = malloc(bsize);
	if(buf == NULL){
		fprintf(stderr, "%s: can't malloc(%d)\n", argv0, bsize);
		exit(1);
	}
	for(k = 0; k < bsize; k++)
		buf[k] = rand()>>4;
	if((fd = open(dev, mode)) < 0){
		perror(dev);
		exit(1);
	}

	t0 = time(0);
	ntime = 1;
	tlimit += t0;
	tlast = t0;
	n = 0;
	nlast = 0;
	ntest = 1;
	b = offset-1;
	arena += offset;
	for(;;){
		if(seq){
			if(++b > arena){
				b = offset;
				myseek(fd, b, bsize);
			}
		} else {
			b = rand()>>4;
			b *= rand()>>4;
			b *= rand()>>4;
			if(b < 0)
				b = ~b;
			b = offset + b%(arena-offset);
			myseek(fd, b, bsize);
		}
		switch(action)
		{
		case Read:
			if((k = read(fd, buf, bsize)) != bsize){
				fprintf(stderr, "%s: read(%d)@%d returned %d: ", argv0, bsize, b, k);
				perror("");
				goto done;
			}
			break;
		case Write:
			if((k = write(fd, buf, bsize)) != bsize){
				fprintf(stderr, "%s: write(%d)@%d returned %d: ", argv0, bsize, b, k);
				perror("");
				goto done;
			}
			break;
		case Readwrite:
			if((k = read(fd, buf, bsize)) != bsize){
				fprintf(stderr, "%s: read(%d) returned %d: ", argv0, bsize, k);
				perror("");
				goto done;
			}
			lseek(fd, -bsize, SEEK_CUR);
			if((k = write(fd, buf, bsize)) != bsize){
				fprintf(stderr, "%s: write(%d) returned %d: ", argv0, bsize, k);
				perror("");
				goto done;
			}
		}
		n++;
		if(n >= nblks)
			break;
		if((n%ntest) == 0){
			t = time(0);
			ntime++;
			if(t >= tlimit){
				break;
			}
			if(t > tlast){
				ntest = (n-nlast)/(t-tlast);
				if(ntest < 2)
					ntest = 2;
				else if(ntest > 10)
					ntest = 10;
				tlast = t;
				nlast = n;
			}
		}
	}
done:
	t1 = time(0);
	fsync(fd);
	t2 = time(0);
	t = t2-t0;
	if(t == 0)
		t = 1;
	printf("%s\t%s\t%s\t%.2fMB/s\t%s",
		order, op, pretty(1, bsize, pb), n/((double)scaleM)*bsize/t, dev);
	if(com)
		printf("\t%s", com);
	if(verbose){
		char b[512], e[512];

		printf("\t(%s..%s %ds", pretty(offset, bsize, b), pretty(arena, bsize, e), t);
		if(verbose > 1)
			printf("; %d/%ds=%.1f/s, fsync=%ds", n, t, n/(float)t, t2-t1);
		printf(")");
	}
	printf("\n");
	free(buf);
	close(fd);
}
