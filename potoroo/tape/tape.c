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

#include	<fcntl.h>
#include	<sfio.h>
#include	<stdlib.h>

#ifdef	OS_DARWIN
#include	<sys/ioccom.h>
#endif

#include	<sys/mtio.h>
#include	"ningaui.h"
#include	"tapelib.h"

#include	"tape.man"

void tanal(char *dev, int seek);

int
main(int argc, char **argv)
{
	int n;
	char buf[81];
	int seek = 1;

	ARGBEGIN{
	case 'i':	seek = 0; break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-v] [-i] dev\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 1)
		goto usage;

	tanal(argv[0], seek);

	exit(0);
}

void
tanal(char *dev, int seek)
{
	int fd;
	char *buf;
	int maxbuflen;
	int eot;
	int k;
	int ln, nn;
	int nperfile;
	struct mtop mop;

	if((fd = open(dev, O_RDONLY)) < 0){
		perror(dev);
		exit(1);
	}
	mop.mt_op = MTSETBLK;
	mop.mt_count = 0;
	if(ioctl(fd, MTIOCTOP, &mop) < 0){
		perror("setblk ioctl");
		exit(1);
	}
	if(seek){
		mop.mt_op = MTREW;
		mop.mt_count = 0;
		if(ioctl(fd, MTIOCTOP, &mop) < 0){
			perror("rewind ioctl");
			exit(1);
		}
	}
	maxbuflen = 4*256*1024;
	buf = ng_malloc(maxbuflen, "tape buffer");

	sfprintf(sfstdout, "%s:\n", dev);
	for(eot = 0; eot == 0;){
		nperfile = 0;
		ln = -1;
		nn = 0;
		while((k = read(fd, buf, maxbuflen)) > 0){
			if(ln != k){
				if(ln != -1)
					sfprintf(sfstdout, "\t%dx%d\n", nn, ln);
				ln = k;
				nn = 0;
			}
			nn++;
			nperfile++;
		}
		if(ln != -1)
			sfprintf(sfstdout, "\t%dx%d\n", nn, ln);
		if(nperfile == 0){
			sfprintf(sfstdout, "EOT\n");
			eot = 1;
		} else
			sfprintf(sfstdout, "EOF\n");
		sfsync(sfstdout);
	}
	ng_free(buf);
	close(fd);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_tape -- dump structutre of a tape
&name   &utitle

&synop  ng_tape [-v] dev

&desc	&ital(Ng_tape) analyses the contents of the tape mounted on &ital(dev),
	and prints out the record and file structure of the tape.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_tape_vet) understands the following options:

&begterms
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	An exit code of 1 indicates a problem; details on &cw(stderr).

&mods	
&owner(Andrew Hume)
&lgterms
&term	9/13/2001 (ah) : Original code.
&endterms
&scd_end
*/
