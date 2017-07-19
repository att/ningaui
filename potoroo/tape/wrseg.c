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

#include	<time.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/fcntl.h>
#include	<sys/stat.h>
#include	<errno.h>
#include	"ningaui.h"
#include	"tapelib.h"
#include	"wrseg.man"

static void writeseg(int segid, char *file, char *volid, char *devid, Sfio_t *fp);

#define		TAPE_OUT(xb,xn)		if((k = write(tape_fd, xb, xn)) != xn){\
						sfprintf(sfstderr, "write of %d returned %d (eot=%d)\n", xn, k, (errno == ENOSPC));\
						perror("tape write"); \
						exit(errno == ENOSPC? 5:4);\
					}

ng_int64 nbytes;
int nsegs;
int dostats = 0;

int
main(int argc, char **argv)
{
	int k;
	time_t ta, tb, tc;
	int segid;
	char *tid = "unknown";
	char *farg = 0;
	Sfio_t *fp;

	ARGBEGIN{
	case 'f':	SARG(farg); break;
	case 'i':	SARG(tid); break;
	case 's':	dostats = 1; break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-vs] [-i drive_id] [-f flist] volid dev cpio ...\n", argv0);
			exit(1);
	}ARGEND
	if(argc < 2)
		goto usage;

	ta = time(0);
	tape_init(argv[1], O_RDWR);
	if(farg){
		if((fp = sfopen(0, farg, "w")) == NULL){
			perror(farg);
			exit(1);
		}
	} else
		fp = 0;

	/* now verify tape is okay and seek to end of tape */
	tape_seek(argv[0], TAPE_EOT, 0, &segid);
	tb = time(0);
	if(verbose)
		sfprintf(sfstderr, "tape_seek returns segid %d\n", segid);

	for(k = 2; k < argc; ){
		writeseg(segid++, argv[k], argv[0], tid, fp);
		if(++k < argc)
			tape_wrfm();
		else
			break;
	}
	tc = time(0);

	if(dostats)
		sfprintf(sfstderr, "%s: %s: %d segs, %.3fGB, %I*ds (%I*ds seek)\n", argv0, argv[0],
			nsegs, nbytes*1e-9, Ii(tc-ta), Ii(tb-ta));

	exit(0);
}

static void
writeseg(int segid, char *file, char *volid, char *devid, Sfio_t *filefp)
{
	char buf[NG_BUFFER];
	char tbuf[BLOCK_SIZE+1];
	char buf2[BLOCK_SIZE+NG_BUFFER];
	int tbuf_len;
	int buf2_len;
	int k;
	Sfio_t *fp;
	time_t now;
	time_t ta, tb;
	struct stat sbuf;
	off_t cpio_len, slen;

	ta = time(0);
	/* format front piece and end (md5sum) piece */
	now = time(0);
	if(stat(file, &sbuf) < 0){
		perror(file);
		exit(3);
	}
	cpio_len = sbuf.st_size;
	slen = cpio_len;
	/* set up additional payload */
		/* this version, none */
	tbuf[0] = 0;
	tbuf_len = 0;
	/* now write it out */
	md5init();
	sfsprintf(buf2, sizeof buf2, "%s %d %I*d %I*d %I*d %d %s\n", volid, segid,
		Ii(now), Ii(cpio_len), Ii(tbuf_len), Curversion, devid);
	if(verbose)
		sfprintf(sfstderr, "writing hdr: %s", buf2);
	k = strlen(buf2);
	memcpy(buf2+k, tbuf, tbuf_len);
	md5block(buf2, k+tbuf_len);
	md5term(buf2+k+tbuf_len, 1);
	buf2_len = k+tbuf_len + strlen(buf2+k+tbuf_len)+1;

	/* now write this crap out */
	if((fp = sfopen(0, file, "r")) == NULL){
		perror(file);
		exit(3);
	}
	if(buf2_len%2)
		buf2_len++;
	TAPE_OUT(buf2, buf2_len);
	nbytes += buf2_len;
	md5init();
	while(cpio_len > 0){
		k = BLOCK_SIZE;
		if(k > cpio_len)
			k = cpio_len;
		k = sfread(fp, tbuf, k);
		if(k <= 0)
			break;
		md5block(tbuf, k);
		TAPE_OUT(tbuf, k);
		nbytes += k;
		cpio_len -= k;
	}
	memset(buf, ' ', 80);
	memcpy(&buf[0], "TLR", 3);
	buf[3] = '1';
	memcpy(&buf[4], volid, strlen(volid));
	md5term(&buf[11], 0);
	memcpy(buf2, &buf[11], 32); buf2[32] = 0;
	buf[79] = '1';
	TAPE_OUT(buf, 80);
	tb = time(0);
	if(verbose)
		sfprintf(sfstderr, "write trailer '%80.80s'\n", buf);
	if(dostats)
		sfprintf(sfstderr, "\tsegment %d %s took %I*ds\n", segid, file, Ii(tb-ta));
	if(filefp)
		sfprintf(filefp, "%s %s %I*d\n", buf2, file, Ii(slen));
	nsegs++;
	sfclose(fp);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_wrseg -- write a segment to tape
&name   &utitle

&synop  ng_wrseg [-sv] [-i id] [-f flist] volid dev cpiof ...

&desc	&ital(Ng_wrseg) records new segments on the tape mounted on &ital(dev).
	It uses the cpio file in file &ital(cpiof).
	There are a significant number of checksums and consistency checks.
	For example, the tape must have &ital(vol_id) as its volume id.
	For smallish segments, there is often a significant speed advantage
	in doing multiple segments per invocation of &this.
&space
	Each segment is recorded as a tape file; that is, it is followed by
	a filemark. The very first tape file is a standard ANSI tape
	volume header &cw(VOL1) (recorded by &ital(ng_wrvol)). Each other tape file consists of
	a plaintext (not binary) header with checksum (recorded as a single tape record),
	the contents of &ital(cpiof), followed by an ANSI trailer record &cw(TLR1)
	which includes an md5 checksum of &ital(cpiof).
&space
	If the &bold(-f) option is used, segments successfully recorded are written to the given file.
	Each file is written as a triple: MD5 checksum, path, size.
	As always with modern tape, there are no guarantees;
	rarely, some errors may be detected only after this program has exited.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_wrseg) understands the following options:

&begterms
&term 	-f : record the files successfully written in the given file.
&term 	-i : record the segment with teh given id (typically the drive's magic identifier).
&term 	-s : print statistics on how much got written and how fast.
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	An exit code of 1 indicates a problem; details on &cw(stderr).
	An exit code of 3 indicates inability to stat or open a segment file.
	An exit code of 5 indicates an ENOSPC write error, which should mean the tape is full.
	An exit code of 4 indicates any other tape I/O error.

&mods	
&owner(Andrew Hume)
&lgterms
&term	9/13/2001 (ah) : Original code.
&endterms
&scd_end
*/
