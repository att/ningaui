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
  ---------------------------------------------------------------------------------
  Mnemonic: 	ng_fish - go fish for a key in a file or buffer
  Abstract: 	These routines provide a binary search capability through either a
		file or a buffer.  The key may be either a string or an ascii
		number that can be converted using atoi() before performing an 
		integer comparison.  An integer search is selected by supplying a 
		key length of zero. The key is assumed to be at the head of each 
		record.	A variable key length search can be accomplised by appending
		the field separator character to the target and submitting the 
		length of the target+sep string as the key length.  As long as the 
		input data has been sorted with this, then it should work.

		It is assumed that the input file contains n sorted records 
		followed by m unsorted records. If the target is not found in 
		the sorted portion of the file, the unsorted portion is looked 
		through (sequentially).

		Public routines:
		ng_fish_buf  - search a buffer for a target. returns a pointer to 
			the record in the buffer if a key match is found.
		ng_fish_file - search an open file for a target record. the file is 
			left 'pointing' to the record such that the next write 
			to the file will overwrite the record, or the next read 
			will fetch the record.
		
  WARNING: 	records returned are NOT null terminated. If this is important
		then the caller needs to null terminate. The buffer is guarenteed
		big enough for the caller to set *(rec+1) == NULL.

  Date: 	13 Nov 2001 (originally for catalogue manager - converted to library
		functions 6 may 2002)
  Author: 	E. Scott Daniels.
  Mod:		06 Dec 2001 (sd) : ensured that if sorted portion < ng_buffer we 
		only read sorted chunk
		31 Jul 2005 (sd) : changes to prvent gcc warnings.
  ---------------------------------------------------------------------------------
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<string.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>

#define NOISE_LEVEL 4	/* bleat only if verbose over this */

/* ----------------- private ----------------------------------------------------- */
static char *fish_unsorted( int fd, void *target, int reclen, long count, long key_off );

static int scompare( char *s1, char *s2, int len )
{
	return strncmp( s1, s2, len );
}

static ng_int64 icompare( ng_int64 *target, char *key )	
{
	return *target - strtoll( key, NULL, 10 );
}

/* wonder how much of a performance savings we would see if we used a pointer to 
   the function of choice rather than this indirect calling business?
*/
static int keylen = 0;
static int fish_compare( void *target, char *buf, long key_off )
{
	char *key;

	key = buf + key_off;

	if( keylen )
		return scompare( target, key, keylen );
	else
		return icompare( target, key );
}


/*	see if its possible for this target to be inside of the buffer 
	returns -1 if target would be 'before' the buffer, 1 after, 
	and 0 if its possible for the target to be in the buffer.
	Assumes that the key field is the first field and is 
	an ascii numeric string that can be given to atoi.
*/
static int in_buf_range( void *target, char *buf, long reclen, long nrec, long key_off )
{
	if( fish_compare( target, buf, key_off ) >= 0 )					/* target >= atoi(buf)|key */
	{
		if( fish_compare( target, (buf + ((nrec * reclen)-reclen)), key_off ) <= 0 )  /* target <= key */
			return 0;

		return 1;	/* target larger than last record */
	}

	return -1;		/* target smaller than last record */
}

/* -------------------------- public ----------------------------------------- */

/*	binary search on a buffer for target
	returns a pointer to the record or null if target not found.
*/
char *ng_fish_buf( char *buf, void *target, int klen, long reclen, long nrec, long key_off )
{
	long hi = nrec;
	long lo = 0;
	long r;			/* current record we are looking at */
	int ncomp = 0;

	keylen = klen;		/* dont know why it was done this way */

	r = (hi - lo)/2;	/* record number we are currently examining - start in middle */

	while( ncomp++ < nrec + 1 )	/* we should never make this many compares - prevent coding error induced loop */
	{
		if( hi <= lo )
			return 0;		/* not there */

		if( fish_compare( target, (buf + (r * reclen)), key_off ) > 0 )		/* n < target */
		{
			lo = r;				/* move forward */
			r += (hi - lo)/2;
			lo ++;
		}
		else
		if( fish_compare( target, (buf + (r * reclen)), key_off ) < 0 )		/* n > target */
		{
			hi = r;					/* move back */
			r -= ((hi-lo)/2)+1;
		}
		else
			return buf + (r * reclen);		/* pointer to the record that we matched on */
	}

	return NULL;
}
/* 
	binary search for target through the open file fd
	reads a chunk of the file into buf, if target could 
	be in the buf, then it searches the buffer for the 
	target. If not we 'back up' or 'move forward' in 
	the file until we go past beginning or end, or find
	the chunk that the record would fit in. The input
	file is assumed to be sorted and the key is an 
	ascii number at the front of each buffer.

	NOTE: count is the number of sorted BYTES when passed in
	not the number of sorted records. We convert it to the number 
	of reads we might need to make to find target. Anything
	after count bytes in the file is considered unsorted records
	and will be rompped through if we dont find target in the 
	sorted portion of the file. If count is <= 0 then we assume
	that the whole file IS sorted.
*/
char *ng_fish_file( int fd, void *target, int klen, long reclen, long count, long key_off )
{

	static	char buf[NG_BUFFER];	/* we leave last buffer read in static to allow user direct access if desired */
	char	*rec;			/* pointer to the matched rec */
	long	req_len;		/* number of bytes per read */
	long	half_chunk;		/* number of records in 1/2 of a request */
	long	chunk;			/* number of records in a request */
	long	len;			/* number of bytes actually read */
	long 	hi;			/* largest record in the current search window */
	long	lo = 0;			/* smallest record in the current search window */
	long	r;			/* record number at the center of the buffer */
	long	bnrec;			/* number of records in the buffer */
	long 	offset;			/* offset of the next read chunk in the file */
	long	rcount;			/* num of possible reads */
	
	keylen = klen;

	if( reclen <= 0 )
		return NULL;

	if( count == 0 )				/* no count supplied, assume whole file is sorted */
		count = lseek( fd, 0, SEEK_END );	/* number of bytes */
	else
		if( count < 0 )				/* nothing sorted */
			return fish_unsorted( fd, target, reclen, 0, key_off );	/* no choice but to take the long way home */

	if( count < NG_BUFFER )		/* dont get more than the sorted stuff in the read */
	{
		req_len = count;
		chunk = (count/reclen);
		half_chunk = chunk / 2;
	}
	else				/* read in ng_buffer sized chunks */
	{
		req_len = (NG_BUFFER/reclen) * reclen;
		chunk = (NG_BUFFER/reclen);
		half_chunk = chunk / 2;
	}

	hi = count/reclen;			/* number of sorted records in the file */
	if( req_len )
		rcount = (count / req_len)+1;		/* number of sorted buffers worth of data in the file */
	else
		rcount = 0;				/* nothing sorted */

	ng_bleat( NOISE_LEVEL, "fish_file: target=(%s) reclen=%d reqlen=%d fd=%d count=%ld rcount=%ld", target, reclen, req_len, fd, count, rcount );
	r = ((hi - lo)/2);			/* start looking at the middle */

	while( rcount-- )			/* we should not but if we reach zero something is really buggered */
	{
		r -= half_chunk;
		offset = r * reclen;		/* first byte in chunk; r is in middle of chunk */
		if( offset < 0 )
			offset = 0;

		lseek( fd, offset, SEEK_SET );		/* go to beginning of the chunk to read */
		len = read( fd, buf, req_len );		/* get the chunk */
		bnrec = len/reclen;			/* number of records in the buffer */
		if( verbose >= NOISE_LEVEL )
		{
			char tbuf[1024];
			tbuf[0] = tbuf[127] = 0;
			if( keylen )
				strncpy( tbuf, buf, 128 );
			ng_bleat( NOISE_LEVEL, "fish_file: read=%d bytes recs=%d first=(%s)", len, bnrec, tbuf );
		}

		if( !bnrec || hi <= lo )
			return 0;		/* not in file */

		if( in_buf_range(  target, buf, reclen, bnrec, key_off ) > 0 )	/* go forward, move ahead */
		{ 
			ng_bleat( NOISE_LEVEL, "fish_file: in range says go forward" );
			lo = r;
			r += ((hi - lo)/2) + chunk;
			lo ++;
		}
		else
		if( in_buf_range( target, buf, reclen, bnrec, key_off ) < 0 )	/* back up some */
		{
			ng_bleat( NOISE_LEVEL, "fish_file: in range says go back " );
			hi = r;
			r -= ((hi-lo)/2)+1;
		}
		else
		{						/* found the right chunk, search it */
			ng_bleat( NOISE_LEVEL, "fish_file: in range says in this buffer" );
			if( (rec = ng_fish_buf( buf, target, klen, reclen, bnrec, key_off )) != NULL )	/* its in there */
			{
				ng_bleat( NOISE_LEVEL, "fish_file: sussbufer found it at: offset=%d + %d", offset, rec-buf );
				lseek( fd, offset + (rec - buf), SEEK_SET );		/* leave file pointer at that spot */
				return rec;
			}
			else
				return fish_unsorted( fd, target, reclen, count, key_off );	/* in unsorted heap at end? */
		}
	}	

	return fish_unsorted( fd, target, reclen, count, key_off );	/* in unsorted heap at end? */
}

/*
	romp through the unsorted portion of a file looking for target
*/
static char *fish_unsorted( int fd, void *target, int reclen, long count, long key_off )
{
	static char	buf[NG_BUFFER];
	int	bcount;

	bcount = lseek( fd, 0, SEEK_END );	/* number of bytes */
	ng_bleat( NOISE_LEVEL, "started unsorted search at: count=%d bcount=%d", count,bcount );

	if( bcount == count )
		return NULL;			/* no unsorted heap at end */

	lseek( fd, count, SEEK_SET );		/* seek to first in unsorted heap */

	while( read( fd, buf, reclen ) == reclen )
	{
		ng_bleat( NOISE_LEVEL+1, "checking againgst: %.20s", buf );
		if( fish_compare( target, buf, key_off )  == 0 )		/* bingo! */
		{
			lseek( fd, -reclen, SEEK_CUR );		/* position for write */
			ng_bleat( NOISE_LEVEL, "found target in unsorted portion" );
			return buf;
		}
	}

	return NULL;
}


/*	a unique concept to a set of library functions as the self test stuff actually makes 
	a tool that can be invoked from a script 
	make fish should generate this.
*/
#ifdef MAKE_TOOL
void usage( char *msg )
{
	sfprintf( sfstderr, "%s\n", msg );
	sfprintf( sfstderr, "usage: %s [-F seperator] [-i] -l recln [-s recs] [-v] file target1 [target2...]\n", argv0 );
	exit( 1 );
}

#include "ng_fish.man"

int main( int argc, char **argv )
{

	int	sorted = 0;			/* nubmer of records that are sorted - 0 == all records sorted */
	int	recln = 0;
	int	fd;
	int	ikey = 0;		/* integer key flag */
	long	key_off = 0;		/* key offset in the buffer */
	long	itarget = 0;		/* integer target if key is integer */
	char	fsep = 0;		/* field seperator */
	char	*buf = NULL;		/* buffer to build things in */
	char	*found;			/* pointer to buffer that was found */
	char	*what;			/* misc pointer for tmp use */


	ARGBEGIN
	{
		case 'F':	SARG( what ); fsep = *what; break;
		case 'i':	ikey = 1; break;
		case 'l':	IARG( recln ); break;
		case 's':	IARG( sorted ); break;
		case 'v':	verbose++; break;
		
		default:
usage:
				usage( "bad option" );
				break;
	}
	ARGEND

	if(argc < 1 )
	{
		usage( "missing key" );
		exit( 1 );
	}

	if( recln <= 0 )
		usage( "missing record length" );

	
	buf = ng_malloc( recln, "buffer" );		

	if( (fd = open( *argv, O_RDONLY )) >= 0 )
	{
		while( --argc )
		{
			argv++;

			if( ikey )			/* integer target */
			{
				itarget = atoi( *argv );
				found = ng_fish_file( fd, &itarget, 0, recln, sorted * recln, key_off );
			}
			else
			{
				if( fsep )
					sfsprintf( buf, recln, "%s%c", *argv, fsep );
				else
					sfsprintf( buf, recln, "%s", *argv );
		
				found = ng_fish_file( fd, buf, strlen( buf ), recln, sorted * recln, key_off );
			}

			if( found )
			{
				write( 1, found, recln );
				if( found[recln-1] != '\n' )
					write( 1, "\n", 1 );
			}
		}

		close( fd );
	}
	else
		sfprintf( sfstderr, "cannot open file: %s %s\n", *argv, strerror( errno ) );
}

#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_fish:Binary search though file)

&space ng_fish [-F seperator] [-i] [-k key_offset] -l recln [-s sorted] [-v] file target [target2...]

&space
&desc	&This provides a binary search capability through a sorted file. The &ital(key) 
	may be fixed or variable length (determined by the searh targets passed in), or 
	may be an integer. &ital(File) is assumed to contain ASCII records with a fixed
	record length. Records do &stress(not), need to be NULL or newline terminated.
	Records that have keys which match the target(s) supplied on the command line 
	are written (with newline seperators) to the sandard output device in the order 
	that the targets were supplied. 
&space
&subcat	Varible Length Keys
	The key is assumed to be variable length if the field specifier is supplied. 
	The field seperator is a single character that is then assumed to seperate 
	the key from the next field of data in the record. If the field seperator 
	is omitted from the command line, then the length of each target string is 
	assumed to be the key length.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-F sep : Supplies the seperator used to determine the end of the key field of 
	each input record. 
&spcae
&term	-i : Integer key comparison.  The targets are converted to integer, and compared 
	as integers, to the key portion of each record searched in the file.
&space
&term	-k offset : Supplies the key offset (bytes from the beginning of the record)
	where &this should expect to find the first byte of the key.  If not supplied
	the beginning of the record (offset of 0) is assumed.
&space
&term	-l recln : Indicates the record length. This command line flag is required. 
&space
&term	-s sorted : Supplies the number of records that are sorted in the file.
	&This assumes that a binary search can be performed on these records, and 
	if the target is not found, a squential search beginning with the record
	&ital(sorted)+1 is undertaken.
	If none of the records are sorted, then &stress(-1) should be passed in
	(not zero), and under these circumstances &lit(grep) is likely to out perform
	&this.
	If omitted, &this assumes that all of the records in the file have been 
	sorted by their key.
&space
&term	-v : Verbose mode; not effective unless at least five of these are present.

&endterms


&space
&parms	The following parameters are expected to be supplied on the command line in 
	the order that they are listed here:
&space
&begterms
&term	file : Supplies the name of the file to search.
&space
&term	target : Supplies one or more target keys to search for.
&endterms

&space
&exit	An exit code that is not zero indicates failure of somesort. A message containing
	the failure is written to the standard error device. 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	07 May 2002 (sd) : First try.
&endterms
.ln

&synop	char *ng_fish_file( int fd, void *target, int klen, long reclen, long count, long key_off )
&break	char *ng_fish_buf( char *buf, void *target, int klen, long reclen, long nrec, long key_off )

&desc	Ng_fish_file executes a binary search of the open file (pointed to by the file descriptor 
	&ital(fd)) for the &ital(target). 
	&ital(Target) points to an ASCII string which is the key to search for.
	The input file is assumed to be sorted based on the 
	key and to contain fixed length records of &ital(recln) bytes.  The key is assumed to begin
	with the first byte pointed to by &ital(record+key_off) and to continue for klen bytes. 
&space
	The key is assumed to be a fixed length (&ital(klen)), however the caller may 'impose' a 
	variable length key search by appending the seperator character to the &ital(target.)
&space
	The following is a list of parameters that are passed to &lit(ng_fish_file.)
&space
&begterms
&term	fd : Is a filedescriptor of the &stress(open) file to search.
&term	target : Pointer to the ASCII key. The key does &stress(not) need to be NULL terminated as 
	the key length is used to determine the number of bytes to compare. 
&term	klen : The number of bytes that are in the key.
&term	reclen : The number of bytes that comprise each record.
&term	count : The number of bytes at the end of the file that are NOT sorted. If this value
	is zero, then the entire file is treated as being sorted. When this value is positive
	a binary search is preformed on records occuring in the file that are &stress(before)
	this file offset, and if the desired key is not located in the file after the binary 
	search, a sequential search beginning at &ital(count) is done. 
&term	key_off : The number of bytes into each record where the key begins. 
&endterms
&space
	&ital(Ng_fish_file) returns a pointer to the buffer containing the record and leaves the 
	file descriptor pointed such that a subsequent write on the file would replace the record.

&space
&subcat	ng_fish_buf
	&ital(Ng_fish_buf) operates much like &lit(ng_fish_file) insomuch as it does a binary search, 
	but the search is limited to the buffer that is passed in. All parameters are the same 
	as were passed to &lit(ng_fish_disk) except for &ital(buf) (a pointer to the buffer to 
	search, and &ital(nrec.) &ital(Nrec) is the number of records that exist in the buffer. 
	The return from &lit(ng_fish_buf) is a pointer to the start of the record inside of the 
	buffer, or NULL if the buffer does not contain the key.
	

&space
&mods
&owner(E. Scott Daniels)
&lgterms
&term: 	13 Nov 2001 (sd) : Originally written for catalogue manager -- converted to generic lib
		function set. 
&term	26 Oct 2008 (sd) : Corrected problem that could cause a floating point exception.
&endterms
&scd_end
------------------------------------------------------------------ */
