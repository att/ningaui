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

#include	<sys/stat.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>

#include <sfio.h>
#include "ningaui.h"

#include "rm_size.man"

off_t get_size( char *fpath )
{
	struct stat sb;

	if(stat(fpath, &sb) < 0){
		ng_bleat( 0, "cannot stat: %s: %s", fpath, strerror( errno ) );
		return -2;
	} 

	return sb.st_size;
}

int
main(int argc, char **argv)
{
	char md5[NG_BUFFER], buf[NG_BUFFER], fpath[NG_BUFFER];
	off_t size;
	int	limit = 0;	/* dont write md5 and name on output */

	ARGBEGIN{
	case 'l':	limit=1;;
	case 'v':	verbose++; break;
	default:
usage: 
			sfprintf(sfstderr, "usage: %s [-v]\n", argv0);
			exit(1);
	}ARGEND

	switch( argc )
	{
		case 0:				/* assume stdin to be read for md5 path records */
			while(sfscanf(sfstdin, "%s %s", md5, fpath) == 2){
				size = get_size( fpath );
		
/*
				if(stat(fpath, &sb) < 0){
					perror(fpath);
					size = -2;
				} else
					size = sb.st_size;
*/
		
				if( limit )
					sfprintf(sfstdout, "%I*d\n", sizeof(size), size);
				else
					sfprintf(sfstdout, "%s %s %I*d\n", md5, fpath, sizeof(size), size);
			}
			break;

		case 1:				/* assume just filename */
				size = get_size( argv[0] );
				if( limit )
					sfprintf(sfstdout, "%I*d\n", sizeof(size), size);
				else
					sfprintf(sfstdout, "%s %I*d\n", argv[0], sizeof(size), size);
				break;
		default:				/* assume md5 filename on command line */
				size = get_size( argv[1] );
				if( limit )
					sfprintf(sfstdout, "%I*d\n", sizeof(size), size);
				else
					sfprintf(sfstdout, "%s %s %I*d\n", argv[0], argv[1], sizeof(size), size);
				break;
	}
	
	
	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_repmgr:Perform replication manager functions.)

&space
&synop	ng_rm_size [[md5] filename]

&space
&desc	&This reads records from the standard input and generates 
	those records back to standard output with the size of the file listed on each 
	record added as the last token of the record.  The input records are 
	expected to be two tokens where the second token is the filename.  If the limit
	option is used, then only the size is written on each output record. 
&space
	If command line parameters are passed, input from the standard input device 
	is ignored and the command line is assumed to contain either the md5 and file
	name, or just the filename.  Output in this mode is: the tokens that were 
	on the command line followed by the file size in bytes. If the limit option
	(-l) is given, then only the size is written to the standard output device. 
&space
	If the size of a file cannot be determined, a value of -2 is written and an 
	error is generated to the stderr device.
	
&space
&opts	The following options are recognised by &this; the default
	action is adding entries.
&begterms
&term	-l : Limit the output to only the size. 
&endterms

&space
&exit	Always 0;

&space
&see	ng_repmgr, ng_rm_register, ng_rm_unreg

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	22 May 2001 (ah) : Original code 
&term	06 Jul 2005 (sd) : Now reports a size of -2 if it cannot stat the file.
&term	10 Sep 2007 (sd) : Added -l option and wrote man page. 
&endterms
&scd_end
*/
