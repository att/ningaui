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

#include	<ast.h>
#include	<sum.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>
#include	"ningaui.h"
#include	"tapelib.h"

#define		TAPE_OUT(xb,xn)		if((k = write(tape_fd, xb, xn)) != xn){\
						perror("tape write"); \
						sfprintf(sfstderr, "write of %d returned %d\n", xn, k);\
						exit(1);\
					}

int
main(int argc, char **argv)
{
	char buf[NG_BUFFER], buf1[NG_BUFFER];
	char tbuf[BLOCK_SIZE+1];
	char buf2[BLOCK_SIZE+NG_BUFFER];
	int tbuf_len;
	int buf2_len;
	int n, k;
	char *s;
	Sfio_t *fp;
	Sum_t *st;
	time_t now;
	ng_int64 nbytes;
	struct stat sbuf;
	off_t cpio_len;

	ARGBEGIN{
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-v] volid segid dirf cpiof dev\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 5)
		goto usage;

	tape_init(argv[4], O_RDWR);

	/* first verify the directory fits in a tape block */
	sfsprintf(tbuf, sizeof tbuf, "gzip < %s", argv[2]);
	if((fp = sfpopen(0, tbuf, "r")) == NULL){
		perror(tbuf);
		exit(1);
	}
	for(n = 0; n <= BLOCK_SIZE; n += k){
		k = sizeof(tbuf)-n;
		if((k = sfread(fp, tbuf+n, k)) <= 0)
			break;
	}
	if(k > 0){
		sfprintf(sfstderr, "directory too large; won't compress to %d bytes\n", BLOCK_SIZE);
		exit(1);
	}
	tbuf_len = n;
	sfclose(fp);

	/* format front piece and end (md5sum) piece */
	now = time(0);
	if(stat(argv[3], &sbuf) < 0){
		perror(argv[3]);
		exit(1);
	}
	cpio_len = sbuf.st_size;
	sfsprintf(buf2, sizeof buf2, "%s %s %I*d %I*d %d\n", argv[0], argv[1], sizeof(now), now,
		sizeof(cpio_len), cpio_len, tbuf_len);
	if(verbose)
		sfprintf(sfstderr, "writing hdr: %s", buf2);
	k = strlen(buf2);
	memcpy(buf2+k, tbuf, tbuf_len);
	md5init();
	md5block(buf2, k+tbuf_len);
	md5term(buf2+k+tbuf_len, 1);
	buf2_len = k+tbuf_len + strlen(buf2+k+tbuf_len)+1;

	/* now verify tape is okay and seek to end of tape */
	/* tape_seek(argv[0], TAPE_EOT, 0, 0); */

	/* now write this crap out */
	if((fp = sfopen(0, argv[3], "r")) == NULL){
		perror(argv[3]);
		exit(1);
	}
	if(buf2_len%2)
		buf2_len++;
	TAPE_OUT(buf2, buf2_len);
	nbytes = buf2_len;
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
	memcpy(&buf[4], argv[0], strlen(argv[0]));
	md5term(&buf[11], 0);
	buf[79] = '1';
	TAPE_OUT(buf, 80);
	if(verbose)
		sfprintf(sfstderr, "write trailer '%80.80s'\n", buf);

	ng_log(LOG_INFO, "wrseg wrote %I*d bytes on %s volid %s time %ld seconds\n", sizeof(nbytes), nbytes, argv[4], argv[0], (long)(time(0)-now));

	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_wrseg -- write a segment to tape
&name   &utitle

&synop  ng_wrseg [-v] volid segid dirf cpiof dev

&desc	&ital(Ng_wrseg) records a new segment on the tape mounted on &ital(dev).
	It uses the directory in file &ital(dirf) and the cpio file in file &ital(cpiof).
	There are a significant number of checksums and consistency checks.
	For example, the tape must have &ital(vol_id) as its volume id.
&space
	Each segment is recorded as a tape file; that is, it is followed by
	a filemark. The very first file on a tape is a standard ANSI tape
	volume header (recorded by &ital(ng_wrvol)). Each other file has
	the format of a directory (recorded as a single tape record),
	the contents of &ital(cpiof), followed by an ANSI trailer record
	which includes an md5 checksum of &ital(cpiof). The directory record
	consists of an ASCII header, the &ital(gzip)ped contents of &ital(dirf),
	followed by the md5 checksum of these two pieces.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_wrseg) understands the following options:

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
