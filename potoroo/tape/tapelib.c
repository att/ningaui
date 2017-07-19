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

#include	<sfio.h>
#include	<stdlib.h>
#include	<memory.h>
#include	"ningaui.h"
#include	<sys/mtio.h>
#include	"tapelib.h"

#ifdef	OS_DARWIN
#include	<sys/ioccom.h>
#endif

int tape_fd;

#define		TAPE_BOT	-3
#define		TAPE_EOT	-2

void
tape_init(char *dev, mode_t mode)
{
	struct mtop mop;

	/* mode will in general be O_RDWR or O_RDONLY */
	if((tape_fd = open(dev, mode)) < 0){
		perror(dev);
		exit(1);
	}
	mop.mt_op = MTSETBLK;
	mop.mt_count = 0;
	if(ioctl(tape_fd, MTIOCTOP, &mop) < 0){
		perror("setblk ioctl");
		exit(1);
	}
}

static
get_volseg(char *buf, int len, char *volid, int *segno)
{
	char *s;

	if((len == 80) && (memcmp(buf, "VOL1", 4) == 0)){
		/* vol header */
		memcpy(volid, buf+4, 6);
		for(s = volid+5; (s >= volid) && (*s == ' '); s--)
			*s = 0;
		*segno = -1;
	} else {
		if(sfsscanf(buf, "%s %d", volid, segno) != 2){
			sfprintf(sfstderr, "bad format for volseg '%12.12s'\n", buf);
			volid[0] = 0;
			*segno = -1;
		}
	}
	if(verbose)
		sfprintf(sfstderr, "volseg returns '%s' seg %d\n", volid, *segno);
}

void
tape_wrfm(void)
{
	struct mtop mop;

	mop.mt_count = 1;
	mop.mt_op = MTWEOF;
	if(ioctl(tape_fd, MTIOCTOP, &mop) < 0){
		perror("wrfm ioctl");
		exit(1);
	}
}

void
tape_seek(char *volid, int segment, char *block, int *len)
{
	struct mtop mop;
	char mybuf[BLOCK_SIZE];
	int k;
	char *buf, tapevolid[NG_BUFFER];
	int segno;
	time_t a, b;

	if(segment < 0){
		mop.mt_count = 0;		/* shouldn't matter but ... */
		switch(segment)
		{
		case TAPE_BOT:	mop.mt_op = MTREW; break;
		case TAPE_EOT:	mop.mt_op = MTEOM; break;
		default:	sfprintf(sfstderr, "bad seek argument %d\n", segment);
				exit(1);
		}
		if(ioctl(tape_fd, MTIOCTOP, &mop) < 0){
			perror("seek ioctl");
			exit(1);
		}
		if(segment == TAPE_EOT){
			buf = mybuf;
			/* go back to the last segment */
#ifdef OS_FREEBSD
			mop.mt_op = MTBSF;
			mop.mt_count = 3;
#else
			mop.mt_op = MTBSFM;
			mop.mt_count = 2;
#endif
			ioctl(tape_fd, MTIOCTOP, &mop);		/* failure not unlikely */
			k = read(tape_fd, buf, BLOCK_SIZE);
			if(verbose > 1)
				sfprintf(sfstderr, "seek to hdr yields %d\n", k);
#ifdef OS_FREEBSD
			if(k == 0){
				k = read(tape_fd, buf, BLOCK_SIZE);
				if(verbose > 1)
					sfprintf(sfstderr, "supplemental read yields %d\n", k);
			}
#endif
			if(k <= 0){
				sfprintf(sfstderr, "%s: illegal EOF in tape_seek\n", argv0);
				exit(1);
			}
			get_volseg(buf, k, tapevolid, &segno);
			if(strcmp(tapevolid, volid)){
				sfprintf(sfstderr, "volid mismatch: got '%s', wanted '%s'\n", tapevolid, volid);
				exit(1);
			}
			if(segno < 0)
				segno = 0;
			if(len)
				*len = segno+1;
			mop.mt_op = MTFSF;
			mop.mt_count = 1;
			if(ioctl(tape_fd, MTIOCTOP, &mop) < 0){
				perror("seek ioctl");
				exit(1);
			}
		}
	} else {
		time(&a);
		buf = block? block : mybuf;
		/* go back to the last segment */
#ifdef OS_FREEBSD
		mop.mt_op = MTBSF;
		mop.mt_count = 3;
#else
		mop.mt_op = MTBSFM;
		mop.mt_count = 2;
#endif
		ioctl(tape_fd, MTIOCTOP, &mop);		/* error is likely BOT */

		for(;;){
			k = read(tape_fd, buf, BLOCK_SIZE);
			if(verbose > 1)
				sfprintf(sfstderr, "header read yields %d\n", k);
			if(k == 0){			/* we read the filemark */
				if(verbose)
					sfprintf(sfstderr, "reading FM ");
				k = read(tape_fd, buf, BLOCK_SIZE);
				if(verbose > 1)
					sfprintf(sfstderr, "supplemental read yields %d\n", k);
			}
			if(k <= 0)
				break;
			if(len)
				*len = k;
			if(verbose){
				time(&b);
				sfprintf(sfstderr, "%ds: ", b-a);
			}
			get_volseg(buf, k, tapevolid, &segno);
			if(volid && strcmp(tapevolid, volid)){
				sfprintf(sfstderr, "volid mismatch: got '%s', wanted '%s'\n", tapevolid, volid);
				exit(1);
			}
			if(verbose > 1)
				sfprintf(sfstderr, "target seg=%d: got %d\n", segment, segno);
			if(segment == segno)
				return;
			k = segment-(segno<0? 0:segno);
			if(k > 0){
				mop.mt_op = MTFSF;
				mop.mt_count = k;
			} else {
				mop.mt_op = MTBSF;
				mop.mt_count = 1-k;		/* i can't explain the 1 */
#ifdef OS_FREEBSD
				ioctl(tape_fd, MTIOCTOP, &mop);		/* ignore error; probably just means we are at BOT */
				mop.mt_op = MTFSR;
				mop.mt_count = 1;
#endif
			}
			if(verbose > 1)
				sfprintf(sfstderr, "\tmt_op=%d mt_count=%d\n", mop.mt_op, mop.mt_count);
			ioctl(tape_fd, MTIOCTOP, &mop);		/* ignore error; probably just means we are at EOT */
		}
		sfprintf(sfstderr, "no match for segment %d\n", segment);
		exit(1);
	}
}

static Md5_t *m5;

void
md5init(void)
{
	m5 = md5_init();
}

void
md5block(char *block, int len)
{
	md5_block(m5, block, len);
}

void
md5term(char *result, int nullterm)
{
	char c;

	c = result[32];
	md5_fin(m5, result);
	if(nullterm == 0)
		result[32] = c;
	m5 = 0;
}

int
grok_seg(char *dirblock, int dirblock_len, Sfio_t *dir, Sfio_t *cpio1, Sfio_t *cpio2, Seg_hdr *sh)
{
	char volid[NG_BUFFER];
	int segno;
	time_t now;
	ng_int64 cpio_len;
	int dir_len, hdr_len;
	char *s;
	int k, ret;
	char csum[NG_BUFFER];
	char buf[BLOCK_SIZE];
	unsigned char null[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x60, 0xef, 0x72, 0x40, 0x00, 0x03, 0x03,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	/* first, reconstruct data out of the dir block */
	if((k = sfsscanf(dirblock, "%s %d %I*d %I*d %d", volid, &segno, sizeof(now), &now,
			sizeof(cpio_len), &cpio_len, &dir_len)) != 5){
		sfprintf(sfstderr, "couldn't read header line for segment: got %d, not 5\n", k);
		return 1;
	}
	if(sh){
		strcpy(sh->volid, volid);
		sh->segno = segno;
		sh->cpio_len = cpio_len;
		sh->now = now;
	}
	if((s = memchr(dirblock, '\n', dirblock_len)) == 0){
		sfprintf(sfstderr, "no newline in dir block\n");
		return 1;
	}
	hdr_len = (s - dirblock) + 1;

	/* now do dirblock consistency check */
	md5init();
	md5block(dirblock, hdr_len+dir_len);
	md5term(csum, 1);
	if(strcmp(csum, dirblock+hdr_len+dir_len)){
		sfprintf(sfstderr, "dirblock csums differ: recorded=%s calc=%s\n", dirblock+hdr_len+dir_len, csum);
		return 1;
	}

	if(1){
		time_t a, b;

		time(&a);
		md5init();
		while(cpio_len > 0){
			k = (cpio_len < BLOCK_SIZE)? cpio_len : BLOCK_SIZE;
			if((k = read(tape_fd, buf, k)) <= 0)
				break;
			md5block(buf, k);
			if(cpio1)
				sfwrite(cpio1, buf, k);
			if(cpio2)
				sfwrite(cpio2, buf, k);
			cpio_len -= k;
		}
		md5term(csum, 1);
		time(&b);
		if(verbose)
			sfprintf(sfstderr, "r/w cpio took %ds\n", b-a);

		if(cpio1){
			sfclose(cpio1);
		}
		if(cpio2){
			sfclose(cpio2);
		}
		if(cpio_len == 0){
			if((k = read(tape_fd, buf, BLOCK_SIZE)) == 80){
				if((memcmp("TLR1", buf, 4) == 0) && (memcmp(buf+11, csum, strlen(csum)) == 0)){
					goto done;
				}
				sfprintf(sfstderr, "trailer error: type '%4.4s' tape md5=%32.32s calc md5=%s\n",
					buf, buf+11, csum);
				return 1;
			}
			sfprintf(sfstderr, "bad length of trailer record\n");
		} else {
			sfprintf(sfstderr, "i/o error: %d bytes to go, read returned %d\n", cpio_len, k);
		}
		/* semantic checks okay, just not enough bytes */
		return 2;
	}
	/* must do dir after cpio, in case there is an error */
done:
	if(dir){
		if(dir_len)
			sfwrite(dir, dirblock+hdr_len, dir_len);
		else
			sfwrite(dir, null, sizeof(null));
	}
	return 0;
}

void
self_discover(char *volid, char *device)
{
	char buf[NG_BUFFER];

	sfsprintf(buf, sizeof(buf), "ng_tp_identify %s %s %s", volid, device);
	ng_bleat(2, "exec %s", buf);
	system(buf);
}
