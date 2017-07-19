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

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sfio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include	<ningaui.h>

#define	MAGIC	"070707"

#include	"cpio_vfy.man"

int
main(int argc, char **argv)
{
	Sfio_t *in, *out;
	unsigned char buf[56*1024];
	char filename[56*1024];
	char tmp[56*1024], dest[56*1024];
	ng_uint64 filesize, toread;
	int i, n;
	int namelen;
	int eflag = 0;
	char *p;
	int get_extra;
	char *extra = 0;
	int create = 0;
	char *targ = 0;

	ARGBEGIN{
	case 'c':	create = 1; break;
	case 'e':	eflag = 1; break;
	case 't':	SARG(targ); break;
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf(sfstderr, "Usage: %s [-v] [-e] [-c] [-t tspec]\n", argv0);
			exit(1);
	}ARGEND
	if(argc != 0)
		goto usage;

	in = sfstdin;		/* read from stdin */
	sfsprintf(tmp, sizeof tmp, "poot_%d", getpid());

	for(;;){
		/* magic number */
		n = sfread(in, buf, 6);
		if(n != 6){
			sfprintf(sfstderr, "%s: read(magic) returns %d\n", argv0, n);
			exit(1);
		}
		buf[n] = 0;
		if(strcmp(buf, MAGIC) != 0){
			sfprintf(sfstderr, "%s: bad magic: expected '%s', got '%s'\n", argv0, MAGIC, buf);
	    		exit(1);
		}

		/* skip the next 8 fields (7 x 6 bytes + 11 bytes) */
		if((n = sfread(in, buf, 53)) != 53){
			sfprintf(sfstderr, "%s: fieldskip1 failed (%d)\n", argv0, n);
			exit(1);
		}

#define	get_null_term(k, str)							\
	if((n = sfread(in, buf, k)) != k){					\
		sfprintf(sfstderr, "%s: %s failed (%d)\n", argv0, str, n);	\
		exit(1);							\
	}									\
	buf[k] = 0

		/* file name length */
		get_null_term(6, "namelen");
		namelen = strtol(buf, NULL, 8);

		/* file size */
		get_null_term(11, "filesize");
		filesize = strtoll(buf, NULL, 8);

		/* file name (and extended options?) */
		get_null_term(namelen, "namelen2");
		n = strlen(buf);
		if((n + 1) != filesize){
			/* get the real length of the file name */
			namelen = n;

			p = buf + n + 1;
			while(*p){
				if(*p == 's'){
					/* use the size in the extension */
					filesize = strtoull(p+1, NULL, 16);
					if(verbose > 1)
						sfprintf(sfstderr, "---- new filesize %I*u from '%s'\n", Ii(filesize), p+1);
				}
				while(*p++)
					;
			}
		}
		if(verbose > 1)
			sfprintf(sfstderr, "--reading '%s' and %I*d bytes\n", buf, Ii(filesize));

		/* check for cpio trailer record */
		if(strcmp(buf, "TRAILER!!!") == 0){
			if(verbose)
				sfprintf(sfstderr, "found cpio trailer; exiting\n");
			break;
		} else {
			strcpy(filename, buf);
		}
		if(eflag){
			p = strrchr(filename, '/');
			if(p)
				p++;
			else
				p = filename;
			get_extra = strcmp(p, "__EXTRA__") == 0;
		}

		/* now process file contents */
		toread = filesize;
		if(create){
			if((out = sfopen(0, tmp, "w")) == NULL){
				perror(tmp);
				exit(1);
			}
		}
		md5init();
		while(toread > 0){
			if(toread > sizeof(buf))
				n = sizeof(buf);
			else
				n = toread;
			if((i = sfread(in, buf, n)) != n){
				sfprintf(sfstderr, "%s: read(%d) returned %d; toread=%I*d\n", argv0, n, i, Ii(toread));
		    		exit(1);
			}
			md5block(buf, i);
			if(create)
				sfwrite(out, buf, i);
			toread -= i;
			if(get_extra && (extra == 0)){
				if(p = memchr(buf, '\n', i))
					*p = 0;
				else
					buf[(i > 0)? i-1:i] = 0;
				extra = ng_strdup(buf);
			}
		}
		md5term(buf, 1);
		sfprintf(sfstdout, "%s %s %I*d", buf, filename, sizeof(filesize), filesize);
		if(targ)
			sfprintf(sfstdout, " %s", targ);
		if(extra)
			sfprintf(sfstdout, " %s", extra);
		sfprintf(sfstdout, "\n");
		if(create){
			sfclose(out);
			mkdir(buf, 0777);
			if((p = strrchr(filename, '/')) == 0)
				p = filename;
			sfsprintf(dest, sizeof dest, "%s/%s", buf, p);
			if(rename(tmp, dest) < 0)
				perror(dest);
		}
	}
	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	cpio_vfy -- verify a cpio file
&name   &utitle

&synop  cpio_vfy [-v] [-e] [-c] [-t tspec]

&desc	&This reads and verifies the cpio file on standard input.
	On standard output, it puts a list of member files, together
	with their md5 checksum and length.
	A successful return (exit status 0) implies all cpio headers and file lengths are
	consistent and have been navigated successfully.

&desc	The &cw(-t) option specifies tape specification stuff (typically volid and segment)
	that will be inserted in the output after the file's length.

	The &cw(-e) option specifies that extra information be output for each member
	after the length (and &ital(tspec) if specified). The information is taken from a member whose basename is &cw(__EXTRA__),
	and is only available after that member is processed. (Thus, it should be first.)

	The &cw(-c) option extracts files from the cpio and puts them in a directory whose name
	is the MD5 checksum of the file's contents.

&opts   &This understands the following options:

&begterms
&term 	-e : add extra fields, as described above.
&term	-t tspec : add tspec as described above.
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms


&exit	An exit code of 1 indicates a format consistency error; details on &cw(stderr).

&mods	
&owner(Andrew Hume)
&lgterms
&term	5/27/2005 (ah) : Original code (tweaked version of moskowitz code).
&endterms
&scd_end
*/
