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
#include	"ningaui.h"
#include	"tapelib.h"

#include	"wrvol.man"

#define		OFF_VOLID	4
#define		MAX_VOLID	6
#define		OFF_POOL	11
#define		MAX_POOL	26

int
main(int argc, char **argv)
{
	int n;
	char buf[81];
	char tbuf[BLOCK_SIZE];
	int checkeot = 1;
	int printit = 0;
	int reset = 0;

	ARGBEGIN{
	case 'f':	checkeot = 0; break;
	case 'p':	printit = 1; break;
	case 'R':	reset = 1; break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage:\t%s [-fv] pool volid dev\n\t%s [-v] -p dev\n", argv0, argv0);
			exit(1);
	}ARGEND
	if(printit){
		if(argc != 1)
			goto usage;
		tape_init(argv[0], O_RDWR);
		tape_seek(0, TAPE_BOT, 0, 0);
		n = read(tape_fd, buf, sizeof buf);
		if(n != 80){
			buf[(n>0)? n : 0] = 0;
			sfprintf(sfstderr, "%s: expected an 80 byte vol hdr, got %d bytes '%s'\n", n, 100, buf);
			exit(1);
		}
		for(n = OFF_VOLID+MAX_VOLID; n > OFF_VOLID;)
			if(buf[--n] != ' ')
				break;
		buf[n+1] = 0;
		for(n = OFF_POOL+MAX_POOL; n > OFF_VOLID;)
			if(buf[--n] != ' ')
				break;
		buf[n+1] = 0;
		printf("volid=%s pool=%s\n", &buf[OFF_VOLID], &buf[OFF_POOL]);
#ifdef	TAPE_SELFDISCOVER
		self_discover(&buf[OFF_VOLID], argv[0]);
#endif
		exit(0);
	}

	if(argc != 3)
		goto usage;

	tape_init(argv[2], O_RDWR);
	if(checkeot || reset){
		tape_seek(0, TAPE_BOT, 0, 0);
		n = read(tape_fd, tbuf, sizeof tbuf);
		if(reset){
			if(n <= 0){
				sfprintf(sfstderr, "can't reset: tape appears unrecorded\n");
				exit(1);
			}
			if(n < (OFF_VOLID+MAX_VOLID)){
				sfprintf(sfstderr, "can't reset: label record too short (%d needs to be >= %d)\n", n, (OFF_VOLID+MAX_VOLID));
				exit(1);
			}
			tbuf[OFF_VOLID+MAX_VOLID] = 0;
			for(n = MAX_VOLID-1; n >= 0; n--){
				if(tbuf[OFF_VOLID+n] == ' ')
					tbuf[n] = 0;
			}
			if(strcmp(&tbuf[OFF_VOLID], argv[1]) == 0){
				/* they match! */
			} else {
				sfprintf(sfstderr, "can't reset: volid mismatch: want '%s', got '%s'\n", argv[1], &tbuf[OFF_VOLID]);
				exit(1);
			}
		} else {
			if(n > 0){
				tbuf[n] = 0;
				sfprintf(sfstderr, "tape appears to be recorded: first block is %d bytes and begins with '%.*s'\n", n, 100, tbuf);
				exit(1);
			}
		}
	}
	tape_seek(0, TAPE_BOT, 0, 0);

	memset(buf, ' ', 80);
	memcpy(&buf[0], "VOL", 3);
	buf[3] = '1';
	n = strlen(argv[1]);
	if(n > MAX_VOLID){
		sfprintf(sfstderr, "%s: vol_id '%s' too long (max=%d)\n", argv0, argv[1], MAX_VOLID);
		exit(1);
	}
	memcpy(&buf[OFF_VOLID], argv[1], n);
	n = strlen(argv[0]);
	if(n > MAX_POOL){
		sfprintf(sfstderr, "%s: pool_id '%s' too long (max=%d)\n", argv0, argv[0], MAX_POOL);
		exit(1);
	}
	memcpy(&buf[11], argv[0], n);
	memcpy(&buf[37], "NINGAUI", 7);
	buf[79] = '1';
	if(write(tape_fd, buf, 80) != 80){
		perror("hdr write");
		exit(1);
	}

	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_wrvol -- write or print a tape volume header label on a tape
&name   &utitle

&synop
	ng_wrvol [-vf] pool volid dev
&break
	ng_wrvol [-v] -p dev

&desc	In the first form, &ital(ng_wrvol) writes a volume header as the first record on the tape mounted on &ital(dev).
	If anything has been recorded on the tape, &ital(ng_wrvol) will halt unless
	forced to proceed (by the &bold(-f) flag).
	The tape will be labelled with volume id &ital(volid) and be marked
	as belonging to the pool named &ital(pool).
&space
	Use of the &bold(-f) flag can be avoided by using the &ital(mt)(1) command
	to rewind the tape and then write a file mark, but that is no more secure nor
	convenient.
&space
	In the second form, &ital(ng_wrvol) will dump out various information from the volume header
	in the form of &ital(name)&cw(=)&ital(value) pairs. At least the fields &cw(pool) and &cw(volid)
	are printed; others may appear.
	If the symbol &cw(TAPE_SELFDISCOVER) was defined during compilation,
	the information will also be reported to
	various tape changer daemons in order to do self-discovery of the drive/changer topology.
&space
	Regrettably, &ital(dev) must be a real tape device; we do tape ioctl's.

&opts   &ital(Ng_wrvol) understands the following options:

&begterms
&term 	-f : record the volume header even if the tape has contents.
&term 	-p : print information from the volume header.
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	An exit code of 1 indicates a format consistency error; details on &cw(stderr),
	or in the &cw(-p) case, that no header had been recorded.

&mods	
&owner(Andrew Hume)
&lgterms
&term	9/13/2001 (ah) : Original code.
&endterms
&scd_end
*/
