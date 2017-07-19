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

/* 
# ---------------------------------------------------------------------------------
# Mnemonic:	ng_stat
# Abstract:	provides a consistant interface to stat info.
#		
# Date:		04 Oct 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdio.h>
#include	<sys/stat.h>
#include	<sys/types.h>
#include	<errno.h>

#include	<ningaui.h>
#include	<ng_lib.h>

#include	"ng_stat.man"

#define OP_NONE		0	/* options requested on commandline */
#define	OP_NAME		1
#define OP_MTIME	2
#define OP_ATIME	3
#define OP_CTIME	4
#define OP_SIZE		5
#define OP_INODE	6
#define OP_MODE		7


void usage( )
{
	sfprintf( sfstderr, "usage: %s [-a] [-c] [-i] [-m] [-M] [-n] [-s] file [file...]\n", argv0 );
	exit( 1 );
}

#define	ADDOP(x) (op_order[op_idx++]=(x))

int main( int argc, char **argv )
{
	struct stat	s;
	int	op_idx = 0;			/* index into order */
	int	op_order[25];			/* order that we present information */
	int	i;
	int	rc = 0;				/* return code */

	memset( op_order, OP_NONE, sizeof( op_order ) );

	ARGBEGIN
	{
		case 'a':	ADDOP( OP_ATIME ); break;
		case 'c':	ADDOP( OP_CTIME ); break;
		case 'i':	ADDOP( OP_INODE ); break;
		case 'M':	ADDOP( OP_MODE ); break;
		case 'm':	ADDOP( OP_MTIME ); break;
		case 'n':	ADDOP( OP_NAME ); break;
		case 's':	ADDOP( OP_SIZE ); break;
		default:	
usage:
				usage( );
				exit( 1 );
	}
	ARGEND

	if( op_order[0] == OP_NONE )		/* default */
	{
		ADDOP( OP_MTIME );
		ADDOP( OP_ATIME );
		ADDOP( OP_CTIME );
		ADDOP( OP_SIZE );
		ADDOP( OP_INODE );
		ADDOP( OP_MODE );
		ADDOP( OP_NAME );
	}

	while( *argv )
	{	
		if( stat( *argv, &s ) >= 0 )
		{
			i = 0; 
			while( op_order[i] != OP_NONE )
			{
				if( i )
				sfprintf( sfstdout, " " );

				switch( op_order[i] )
				{
					case OP_ATIME:
						sfprintf( sfstdout, "%I*d", Ii(s.st_atime) );
						break;

					case OP_CTIME:
						sfprintf( sfstdout, "%I*d", Ii(s.st_ctime) );
						break;

					case OP_MTIME:
						sfprintf( sfstdout, "%I*d", Ii(s.st_mtime) );
						break;

					case OP_MODE:
						sfprintf( sfstdout, "%I*o", Ii(s.st_mode) );
						break;

					case OP_NAME:
						sfprintf( sfstdout, "%s", *argv );
						break;

					case OP_INODE:
						sfprintf( sfstdout, "%I*d", Ii(s.st_ino) );
						break;

					case OP_SIZE:
						sfprintf( sfstdout, "%I*d", Ii(s.st_size) );
						break;

					default:
						sfprintf( sfstderr, "internal error!\n" );
						abort( );
						break;
				}

				i++;
			}

			sfprintf( sfstdout, "\n" );

		}
		else
		{	
			sfprintf( sfstderr, "%s: %s: %s\n", argv0, *argv, strerror( errno ) );
			rc = 1;
		}

		argv++;
	}

	exit( rc );
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_stat:Generat stats about one or more files)

&space
&synop	stat [-a] [-c] [-i] [-M] [-m] [-n] [-s] file [file...]

&space
&desc	&This emits several bits of information about each file 
	named on the command line.  The information, depending on the option 
	flags given, can include:
&space
&beglist
&item	Last modification time (epoch)
&item	Last access time (epoch)
&item	Last change to stats (epoch)
&item	File size
&item	The inode number
&item	The file's mode (octal)
&item	File name
&endlist
&space
	The information is presented in the same order that the option flags
	appear on the command line. If no option flags are given, all of the 
	information is written in the order listed above.  The output 
	from &this is in 'raw' format; no lables or other identifiers are
	printed. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Show last access time.
&space
&term	-c : Show last change to stats information for file.
&space
&term	-i : Show the file's inode number.
&space
&term	-m : Show the last modification time.
&space
&term	-M : Prints the file's mode (all of it, not just the access permissions). The output 
	is in octal. 
&space
&term	-n : Show the file's name.
&space
&term	-s : Show the file's size.
&endterms


&space
&parms	One or more filenames are expected on the command line. 

&space
&exit	An exit code that is not zero indicates that an error occurred 
	while attempting to obtain the stats for one or more files listed
	on the command line. 

&space
&see	stat(1) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Oct 2006 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */
