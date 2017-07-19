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
# Mnemonic:	fmutation - detect differences between two files
# Abstract:	This bit compares files and reports on differences providing 
#		some kind of analysis about offset and other interesting facts. 
#		
# Date:		14 July 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	<stdio.h>
#include	<stdlib.h>
#include 	<sys/types.h>
#include 	<sys/uio.h>


#include	<ningaui.h>
#include	<ng_lib.h>

#include	"fmutation.man"

#define SMALLER_OF(a,b)		(a < b ? a : b)

int verbose = 0;
long dcount = 0;		/* differences discovered */
long long tot_diff = 0;		/* total bytes different */
int ok_window = 10;		/* number of bytes that must match in a row for us to consider the difference to be over */
int page_size = NG_BUFFER;	/* number of bytes we read in bulk */

int open_file( char *name )
{
	int fd;

	if( (fd = open( name, O_RDONLY, 0 )) < 0 )
	{
		ng_bleat( 0, "unable to open file: %s: %s", name, strerror( errno ) );
		exit( 1 );
	}

	return fd;
}

void usage( )
{
	fprintf( stderr, "usage: %s reference-file suspect-file\n", argv0 );
	exit( 1 );
}

/* suck in the remainder of the file in fd and report how many bytes it had left */
long long finish_file( int fd )
{
	long long count = 0;
	int len;
	char buf[NG_BUFFER];

	while( (len = read( fd, buf, page_size ))  > 0 )
		count += len;

	return count;
}

int pctg( long long part, long long whole )
{
	double r;

	r = ((double) part)/( (double) whole);

	return (int) (r*100);
}

/* test the next n bytes in the buffer. return 1 if they are the same
   this allows us to vary the 'window' following the first byte that 
   matches after a difference.
	n - number of bytes to look at 
	p1,p2 pointers to buffers
	len - bytes remaining in the buffers
*/
int n_match( int n, char *p1, char *p2, int len )
{
	int i;

	if( len < n )
		return 0;

	for( i = 0; i < n; i ++ )
	{
		if( *p1++ != *p2++ )
			return 0;
	}

	return 1;
}

void show_diff( int num, long long page, long long sidx, long long doffset, long long len, long long zcount )
{
	static int header = 0;
	char *buf = "";

	if( ! header++ )
		ng_bleat( 1, "%4s %5s %9s %9s %9s %7s %5s", "DIFF", "PAGE", "INDEX", "OFFSET", "LENGTH", "BOUNDRY", "ZEROS" );

	if( doffset % 4096 == 0 )
		buf = "4096";
	else
	if( doffset % 2048 == 0 )
		buf = "2048";
	else
	if( doffset % 1024 == 0 )
		buf = "1024";
	else
	if( doffset % 512 == 0 )
		buf = "512";
		
	ng_bleat( 1, "%4d %5I*d %9I*d %9I*d %9I*d %7s %4d%%", num, Ii(page) , Ii(sidx), Ii(doffset), Ii(len), buf, pctg( zcount, len ) );
}

void test_buf( long long page, char *rbuf, char *sbuf, int len )
{
	static long long dsize=0;
	static long long doffset = 0;	/* offset into file where we saw difference start */
	static long long zcount = 0;	/* number of bytes in difference that are zeros */
	static long long dpage = 0;	/* page where the difference started */
	static int	idx = 0;		/* index into buffer of difference start */
	char	wbuf[1024];
	char	*rp;				/* pointers as we step through buffer */
	char 	*sp;
	int	i = 0;

	rp = rbuf;
	sp = sbuf;

	if( len == 0 )				/* assume we are at end and need to report on last difference */
	{
		if( dsize )
		{
			show_diff( dcount, dpage, idx, doffset, dsize, zcount );
		}
		return;
	}

	for( i = 0; i < len; i++ )
	{
		if( *sp != *rp )			/* different */
		{
			if( !dsize )
			{
				if( verbose > 2 )
				{
					fdump( sfstderr, sp, 16, 0, "suspect" );
					fdump( sfstderr, rp, 16, 0, "reference" );
				}
				idx = rp - rbuf;			/* mark start of difference in the current page */
				doffset = idx + (page * page_size);	/* offset of diff start in the file */
				dpage = page;				/* record page where difference started */
				dcount++;				/* number of different segments */
				zcount = 0;
			}
			dsize++;			/* must count in case difference spans pages - size of this difference */
			tot_diff++;			/* total bytes different */
			if( *sp == 0 )
				zcount++;
		}
		else					/* they are the same */
		{
			if( n_match( ok_window, sp+1, rp+1, len - i - 1) )	/* if next n bytes (ok_window) are the same, assume we are back on track */
			{
				if( dsize )			/* we now have the first good byte after a difference */
				{
					show_diff( dcount, dpage, idx, doffset, dsize, zcount );
					dsize = 0;
					zcount = 0;
					idx = 0;
					doffset = 0;
				}
			}

		}

		sp++;
		rp++;
	}
}

int main( int argc, char **argv )
{
	char	refbuf[NG_BUFFER];		/* data from reference file */
	char	susbuf[NG_BUFFER];		/* data from suspect file */
	int	ref = -1;
	int	sus = -1;			/* file descriptors */
	long long reflen;				/* length read from buffer */
	long long suslen;
	long long page = 0;			/* we read in NG_BUFFER pages -- count them */
	long long refcount = 0;			/* length of each file */
	long long suscount = 0;
	long long diff = 0;			/* difference in read buffer lengths if they dont match */
	
	ARGBEGIN
	{
		case 'p':	IARG( page_size ); break;
		case 'w':	IARG( ok_window ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	if( page_size > NG_BUFFER )
		page_size = NG_BUFFER;

	if( argc < 2 )
	{
		usage( );
		exit( 1 ); 
	}

	ref = open_file( argv[0] );
	sus = open_file( argv[1] );

	ng_bleat( 1, "page_size=%I*d window=%d", Ii(page_size), ok_window );

	for( ;; )
	{
		reflen = read( ref, refbuf, page_size );
		refcount += reflen;

		suslen = read( sus, susbuf, page_size );
		suscount += suslen;

		test_buf( page, refbuf, susbuf, SMALLER_OF( reflen, suslen ) );
		if( reflen == suslen )
		{
			if( reflen <= 0 )			/* assume end of file and both ended together */
			{
				ng_bleat( 4, "end of file reached on both files" );
				break;
			}
		}
		else
		{
			if( reflen < suslen )
			{
				test_buf( page, NULL, NULL, 0 );		/* one last bit if tot_diff is set in function */
				diff = suslen - reflen;
				tot_diff += diff; 					/* add to the number of bytes different */
				dcount++;
				suslen = finish_file( sus );
				ng_bleat( 0, "diff %d suspect file is %I*d bytes longer than reference file", dcount, Ii( suslen + diff ) );
				suscount += suslen;
				break;
			}
	
			if( suslen < reflen )
			{
				test_buf( page, NULL, NULL, 0 );		/* one last bit if tot_diff is set in function */
				diff = reflen - suslen;
				tot_diff += diff;					/* add to the number of bytes different */
				dcount++;
				reflen = finish_file( ref );
				ng_bleat( 0, "diff %d reference file is %I*d bytes longer than suspect file", dcount, Ii( reflen +diff ) );
				refcount += reflen;
				break;
			}
		}

		
		page++;			
	}	

	close( ref );
	close( sus );

	
	ng_bleat( 0, "suspect file:   %9I*d bytes %s", Ii(suscount), argv[1] );
	ng_bleat( 0, "reference file: %9I*d bytes %s", Ii(refcount), argv[0] );
	ng_bleat( 0, "%I*d differences for a total of %I*d bytes", Ii(dcount), Ii(tot_diff) );
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(fmutate:Detect mutations between files)

&space
&synop	ng_fmutation [-p pagesize] [-w window] reference-file suspect-file

&space
&desc	Accepts two file names, presumably a reference (good) copy and a suspect (bad) copy, and reports 
	on the differences between the two.  It attempts to point out things like 
	when all differences happen on a 512, 1k or 4k boundry, and the percentage
	of each difference that are null values (zeros).
&space
	&This is not a &lit(diff) programme.  It is designed to identify the differences between 
	two files that are likely the result of file corruption during transfer, 
	or related to failing disk.  The differences listed include the page number, offset in the page, 
	offset in the file, length of the difference, if it occurred on a recognised boundry 
	and the percentage of the difference that were null bytes. 
&space
	If the files are of different lengths, the excess of the longer file is reported as a 
	single difference. 
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p bytes : The block read size. Default is 4096 bytes. 
&space
&term	-w bytes : When a difference is noticed, &this attempts to compute the length of 
	the differing segment of the file.  The window size defines the number of consecutive
	bytes following a difference that must match before &this considers the difference to 
	have ended.  If not supplied, the default is 10 bytes. 
&space
&term	-v : Increase the verbosity level. Multiple -v options make &this even more chatty 
	about what it is doing.  With one -v option, details about each difference are printed
	to the stderr device.  Otherwise just a summary is given. 
&endterms


&space
&parms
	&This expects that the filenames of both files are placed on the command line following 
	any options. 

&space
&exit	An exit of 0 indicates that there were no differences. A non-zero exit code indicates 
	that differences were found and information about them written to the standard error 
	device. 

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	14 Jul 2006 (sd) : Its beginning of time. 
&term	15 Aug 2007 (sd) : Added doc and made it an official tool.


&scd_end
------------------------------------------------------------------ */
