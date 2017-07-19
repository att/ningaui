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

#include <sys/types.h>
#include <stdio.h>
#include	<stdlib.h>
#include <unistd.h>

#include <sfio.h>
#include <ningaui.h>
#include <ng_basic.h>

#include "fdump.man"

main( int argc, char **argv )
{

	Sfio_t *f;
	unsigned char buf[2048];
	int len;
	int reqsize = 1024;
	int flags = D_ROFF;
	long skipto = 0;	/* number of bytes to skip before display */
	long count = 0;		/* number of bytes to display before quitting */
	long stop = 1;		/* number of bytes remaining from count */
	int show;

	ARGBEGIN
	{
		case 'a':	flags |= D_ALL; break;
		case 'c':	IARG( count ); break;
		case 'd':	flags |= D_DECIMAL; break;
		case 'e':	flags |= D_EBCDIC; break;
		case 's':	IARG( skipto ); break;
usage:
		default:	show_man( ); exit( 1 ); break;
	}
	ARGEND


	if( count )
		stop = count;

	if( *argv )
	{
		f = ng_sfopen( NULL, *argv, "r" );
		if( skipto )
			sfseek( f, skipto, SEEK_CUR );
	}
	else
	{
		reqsize = 1024;
		f = sfstdin;
	}

	if( f == NULL )
		exit( 1 );


	while( stop > 0 &&  (len = sfread( f, buf, reqsize )) > 0 )
	{
		show = len;
		if( count )
		{
			if( stop - len <= 0 )
				show = stop;
			stop -= len;
		}

		fdump( sfstdout, buf, show, flags, NULL );
	}

	if( sfclose( f ) )
		fprintf( sfstderr, "write/close error: %s\n", strerror( errno ) );

	return 0;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(fdump:Dump the contents of a file in hex notation)

&space
&synop	fdump [-a] [-d] [-e] [filename]
&space
&desc	&This reads a file and formats it in &ital(hex dump) notation 
	with a cheater block to the right. Depending on options 
	supplied on the command line, the cheater block is either 
	displayed in ASCII or EBCDIC.  Optionally long duplications
	of bytes are &ital(snipped) from the display.

&space
&opts	The following options are recognised by &this.
&smterms
&term	-a : Show all data (don't snip) even if lines would be 
	duplicated.
&space
&term	-c n : Read and display &ital(n) bytes from the file then 
	stop.
&space
&term	-d : Display the offset (left most column) in decimal 
	rather than in hex.
&space
&term	-e : Assume that the file is an EBCDIC file and thus the 
	cheater block is created using the EBCDIC character 
	set rather than the ASCII character set. 
&space
&term	-s n : Skip &ital(n) bytes before beginning to format and 
	print the data. This option is interpreted only when 
	reading from a real file.	The offset displayed after 
	skipping will be relative to the skip point, &stress(not) 
	the beginning of the input file.
&endterms

&space
&parms	Optionally the name of the file to dump can be placed 
	on the command line. If this parameter is omitted, then 
	the standard intput device is read for data.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 05 June 2001 (sd) : Original code.
&term	21 Nov 2006 (sd) : Conversion of sfopen() to ng_sfopen().
&endterms

&scd_end
------------------------------------------------------------------ */
