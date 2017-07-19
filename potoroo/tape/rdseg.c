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
#include	<sys/fcntl.h>
#include	<string.h>
#include	"ningaui.h"
#include	"tapelib.h"

#include	"rdseg.man"

int
main(int argc, char **argv)
{
	char cmd_buf[NG_BUFFER];
	int n, k;
	char *s;
	Sfio_t *cp;
	int segno;
	char seg_buf[BLOCK_SIZE];
	int seg_len;

	ARGBEGIN{
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-v] volid segid cpiof dev\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 4)
		goto usage;

	tape_init(argv[3], O_RDONLY);

	/* first, seek to the right segment */
	segno = strtol(argv[1], 0, 10);
	if(segno <= 0){
		segno = 1;
		sfprintf(sfstderr, "%s: warning: segno must be positive; changed to %d\n", argv0, segno);
	}
	tape_seek(argv[0], segno, seg_buf, &seg_len);

	if((cp = sfopen(0, argv[2], "w")) == NULL){
		perror(argv[2]);
		exit(1);
	}
	if(grok_seg(seg_buf, seg_len, 0, cp, 0, 0))
		exit(1);
	
	ng_log(LOG_INFO, "rdseg read segment %d on %s volid %s\n", segno, argv[2], argv[0]);

	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_rdseg -- process a specified segment on a tape
&name   &utitle

&synop  ng_rdseg [-v] volid segid cpiof dev

&desc	&ital(Ng_rdseg) extracts the specified segment from teh tape mounted on &ital(dev)
	and puts the cpio file in file &ital(cpiof).
	A successful return (exit status 0) implies that all checksums and other
	internal consistency checks have been applied successfully.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_rdseg) understands the following options:

&begterms
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	An exit code of 1 indicates a format consistency error; details on &cw(stderr).

&mods	
&owner(Andrew Hume)
&lgterms
&term	9/13/2001 (ah) : Original code.
&endterms
&scd_end
*/
