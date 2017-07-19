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
#include	<time.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<sys/mtio.h>
#include	<string.h>
#include	"ningaui.h"
#include	"tapelib.h"

#include	"tape_vet.man"

void grok(int nfile, char *buf, int len, int prdir, int prcpio, int new);
static Seg_hdr sh;

int
main(int argc, char **argv)
{
	int k;
	int nfile;
	int print = 0;
	int cpio = 0;
	int offset = -1;
	int new = 1;
	char buf[BLOCK_SIZE*2];
	char *mustvolid = 0;

	ARGBEGIN{
	case 'c':	cpio = 1; break;
	case 'i':	SARG(mustvolid); break;
	case 'n':	new = 1; break;
	case 'o':	IARG(offset); break;
	case 'p':	print = 1; break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-cnpv] [-i volid] [-o seg] dev\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 1)
		goto usage;
	if(new)
		print = 0;

	tape_init(argv[0], O_RDONLY);
	tape_seek(0, TAPE_BOT, 0, 0);

	sfprintf(sfstderr, "%s:\n", argv[0]);
	for(nfile = 0; ; nfile++){
		if((offset > 0) && nfile){
			tape_seek(sh.volid, offset, buf, &k);
			nfile = offset;
			offset = -1;
		} else {
			k = read(tape_fd, buf, sizeof buf);
		}
		if(k <= 0){
			/* we used to do different things on error, but the drive(r)s no longer reliable
				in returning EOF on a partial write */
			sfprintf(sfstderr, "EOT\n");
			break;
		}
		grok(nfile, buf, k, print, cpio, new);
		sfprintf(sfstderr, "EOF\n");
		sfsync(sfstderr);
		if(mustvolid && (strcmp(sh.volid, mustvolid) == 0)){
			sfprintf(sfstderr, "%s: volid mismatch: expected '%s', got '%s'\n", argv0, mustvolid, sh.volid);
			exit(2);
		}
	}

	exit(0);
}

char *
detrail(char *src, int n, char *res)
{
	char *r;

	memcpy(res, src, n);
	for(r = res+n-1; r >= res; r--){
		if(*r == ' ')
			*r = 0;
		else
			break;
	}
	return res;
}
	
void
grok(int nfile, char *buf, int len, int print, int printcpio, int new)
{
	char b1[NG_BUFFER], b2[NG_BUFFER], *s, cpioname[NG_BUFFER];
	static char volid[NG_BUFFER];
	int k, j;
	Sfio_t *p, *null, *cpio1, *cpio2;

	if((nfile == 0) && (len == 80) && (memcmp(buf, "VOL1", 4) == 0)){
		k = read(tape_fd, b1, sizeof b1);
		sfprintf(sfstderr, "\ttape hdr: pool='%s' volid='%s'\n", detrail(buf+11, 26, b1), detrail(buf+4, 6, volid));
		if(k != 0)
			sfprintf(sfstderr, "\twarning: trailing goo after header\n");
		return;
	}
	cpio1 = cpio2 = 0;
	if(new){
		if((s = ng_tmp_nm("tape_vet")) == NULL)
			exit(1);
		strcpy(b2, s);
		sfsprintf(b1, sizeof b1, "ng_cpio_vfy -e -t '%s %d' > %s", volid, nfile, b2);
		cpio1 = sfpopen(0, b1, "w");
	}
	if(printcpio){
		sfsprintf(cpioname, sizeof cpioname, "%d.cpio", nfile);
		cpio2 = sfopen(0, cpioname, "w");
	}
	null = sfopen(0, "/dev/null", "w");
	if(print){
/*
		sfsprintf(b1, sizeof b1, "gzip -d | sed 's/$/ %s %d/' > %d.dir", volid, nfile, nfile);
		p = sfpopen(0, b1, "w");
*/
		sfsprintf(b1, sizeof b1, "%d.dir", nfile);
		p = sfopen(0, b1, "w");
	} else
		p = null;
	if((j = grok_seg(buf, len, p, cpio1, cpio2, &sh)) == 0){
		sfprintf(sfstderr, "\tvolid='%s' segment=%d cpio_len=%.1fMB date=%s", sh.volid,
			sh.segno, sh.cpio_len*1e-6, ctime(&sh.now));
		if(nfile != sh.segno){
			sfprintf(sfstderr, "\terror: bad segment id %d; expected %d\n", sh.segno, nfile);
			exit(1);
		}
		k = read(tape_fd, b1, sizeof b1);
		if(k != 0)
			sfprintf(sfstderr, "\twarning: trailing goo within segment\n");
		if(new){
			Sfio_t *fp;

			if((fp = sfopen(0, b2, "r")) == NULL){
				perror(b2);
				exit(1);
			}
			while((k = sfread(fp, b1, sizeof(b1))) > 0)
				sfwrite(sfstdout, b1, k);
			sfclose(fp);
		}
	} else {
		sfprintf(sfstderr, "\terror: bad format segment\n");
		if(1){
			sfprintf(sfstderr, "\tunlinking %s\n", cpioname);
			unlink(cpioname);
		}
		if(j == 2){
			sfprintf(sfstderr, "\tassuming short read from EOT; proceeding\n");
			/* no exit! */
		} else
			exit(1);
	}
	if(p != null)
		sfclose(p);
	sfclose(null);
	unlink(s);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_tape_vet -- verify conformance of a tape
&name   &utitle

&synop  ng_tape_vet [-cnvp] dev

&desc	&This analyses the contents of the tape mounted on &ital(dev).
	It gives some summary information about each tape file, or segment,
	as well as verifying the consistency of each segment (it checks the
	checksums!). Normally, despite doing checksums, it rans at tape I/O speed.
&space
	&This will also print out the catalog entries for teh files on tape
	via &bold(-n) (works for both old and new format tapes) or &bold(-p)
	(works for old format tapes only). Note that the latter just copies
	the stored directories and is deprecated. The &bold(-c) will also copy all
	the cpio files to disk. You can specify both &bold(-c) and &bold(-n)
	at the same time.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_tape_vet) understands the following options:

&begterms
&term	-c : copy the cpios being vetted to disk (with name &ital(volid)&cw(/)&ital(segno)&cw(.cpio)).
&term	-n : new dir format.
&term 	-p : print the (old format) gzipped dirs to stdout. Each entry is augmented by volume id and segment id.
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
