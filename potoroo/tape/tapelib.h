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

#define		TAPE_BOT	-3
#define		TAPE_EOT	-2

#define		TAPE_SELFDISCOVER	1		/* undefine if you want standalone rdseg etc */

enum { Curversion = 1 };

/* various flavours seem to have differing views of these option constants */
/* tape code is written using linux def, thus adjust if a differing os... */
#ifdef OS_SOLARIS
#define MTSETBLK        MTSRSZ
#define MTBSFM          MTBSF
#endif
#ifdef OS_DARWIN
#define MTSETBLK        MTSETBSIZ
#define MTBSFM          MTBSF
/* MTEOM same on mac as linux */
#else
	/* this must be in the else -- we treate mac as freebsd, ecept here! */
#ifdef OS_FREEBSD

#define MTSETBLK        MTSETBSIZ
#define MTEOM           MTEOD
#endif
#endif

#define		BLOCK_SIZE	(64*1024)

typedef struct
{
	int		segno;
	char		volid[7];
	time_t		now;
	ng_int64	cpio_len;
} Seg_hdr;

extern void tape_init(char *dev, mode_t mode);
extern void tape_seek(char *volid, int segment, char *block, int *len);
extern void tape_wrfm(void);

extern void md5init(void);
extern void md5block(char *block, int len);
extern void md5term(char *result, int nullterm);

extern int grok_seg(char *dirblock, int dirblock_len, Sfio_t *dir, Sfio_t *cpio1, Sfio_t *cpio2, Seg_hdr *sh);

extern void self_discover(char *volid, char *drive);

extern int tape_fd;
