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
  Mnemonic:	ng_md5
  Abstract:	Compute the md5 checksum for a file or files. Verify the current 
		md5 checsum of file(s) against a previously computed list.
 		
  Date:		31 Jan 2006
  Author:	Andrew Hume
  ---------------------------------------------------------------------------------
*/
#include	<stdlib.h>
#include	"ningaui.h"
#include	<stdio.h>
#include 	<errno.h>
#include	<string.h>

#include	"ng_md5.man"

#define	FL_TOTAL	0x01		/* total cksum for all input files not per file (-t) */


int	flags = 0;
Md5_t	*m5 = NULL;			/* global to support -t */
ng_int64 sz = 0;
char *version  = "v2.2/09286";

void usage( )
{
	if( verbose )
		sfprintf( sfstderr, "%s %s\n", argv0, version );
	sfprintf( sfstderr, "%s [-c | -l ] [-t] [-s] [file ...]\n", argv0 );
	exit( 1 );
}

void render( char *result, int prsize )
{
	char buf[NG_BUFFER];

	md5_fin(m5, result);		/* finish up and put in result */

	if(prsize){
		sprintf(buf, " %I*d", Ii(sz));
		strcat(result, buf);
	}

	m5 = NULL;			/* it is no longer a valid pointer */
	sz = 0;				/* reset in case of doing multiple files */
}

void
do1(FILE *fp, char *result, int prsize )
{
	char buf[NG_BUFFER];
	int k;

	if( m5 == NULL )		/* one init if in total mode */
		m5 = md5_init();

	while((k = fread(buf, 1, sizeof(buf), fp)) > 0){
		md5_block(m5, buf, k);
		sz += k;
	}

	if( flags & FL_TOTAL )
	{
		*result = 0;		/* in total mode, final 'render' must be called outside of this */
		return;
	}

	render( result, prsize );
}

/* fetch next newline terminated record; return it as a string (chop newline) */
char *getnr( FILE *f )
{
	static char	buf[NG_BUFFER];
	char	*bp;
	int	l;

	if( fgets( buf, sizeof( buf ), f ) != NULL )
	{
		if( (bp = strchr( buf, '\n' )) != NULL )
			*bp = 0;		
	}
	else
		return NULL;

	return buf;
}

/* assume open file fp is a list of newline separated names; 
   read each name, open and compute the md5 [size] fname buffer for it
*/
int dolist( FILE *fp, int prsize )
{
	FILE	*ifile = NULL;		/* file(s) named in the list opened and scanned */
	char	*fname;
	char	sumbuf[100];		/* buffer where the chksum string is put */
	int	rc = 0;

	while( (fname = getnr( fp )) != NULL )
	{
		if(( ifile = fopen( fname, "r" )) != NULL )
		{
			do1( ifile, sumbuf, prsize );

			if( ferror( ifile ) )
			{
				fprintf( stderr, "list read failed: %s: %s\n", fname, strerror( errno ) );
				exit( 1 );
			}
			else
			{
				if( !(flags & FL_TOTAL) )
					printf( "%s %s\n", sumbuf, fname );
			}

			fclose( ifile );
		}
		else
		{
			fprintf( stderr, "open failed: %s: %s\n", fname, strerror( errno ) );
			rc = 1;
		}
	}
#ifdef KEEP

	if( flags & FL_TOTAL )
	{
		render( sumbuf, prsize );		/* finish up and plop cksum and optionally size into buf */
		printf( "%s\n", sumbuf );		/* just the cksum value */
	}
#endif
		
	return rc;
}

/*  assume that open file fp is a list of md5sum filename pairs
    we validate that the md5 value for each is still the same
*/
int docheck( FILE *fp )
{
	FILE	*ifile = NULL;		/* real file to be scanned */
	char	*obuf;			/* original md5 [size] name data */
	char	*fname;
	char	sumbuf[100];		/* buffer where the chksum string is put */
	char	nbuf[1024];		/* new string as we would spit out */
	int	prsize;
	int	rc = 0;

	if( flags & FL_TOTAL )
	{
		fprintf( stderr, "%s: cannot use check and total options together\n", argv0 );
		usage();
		exit( 1 );
	}

	while( (obuf = getnr( fp )) != NULL )
	{
		if( (fname = strrchr( obuf, ' ' )) != NULL )		/* point at the file name */
		{
			prsize = fname == strchr( obuf, ' ' ) ? 0 : 1;		/* if input record has size, set size flag */

			fname++;					/* past the space and at the name */
			if(( ifile = fopen( fname, "r" )) != NULL )
			{
				do1( ifile, sumbuf, prsize );
	
				if( ferror( ifile ) )
				{
					fprintf( stderr, "%s: read failed: %s: %s\n", argv0, fname, strerror( errno ) );
					rc = 1;
				}
				else
				{
					sfsprintf( nbuf, sizeof( nbuf ), "%s %s", sumbuf, fname );
					if( strcmp( obuf, nbuf ) )
					{
						fprintf( stderr, "%s: md5 changed: %s\n", argv0, fname );
						if( verbose )
						{
							fprintf( stderr, "\torig: %s\n", obuf );
							fprintf( stderr, "\tcurr: %s\n", nbuf );
						}
						rc = 1;
					}
				}
	
				fclose( ifile );
			}
			else
			{
				fprintf( stderr, "%s: open failed: %s: %s\n", argv0, fname, strerror( errno ) );
				rc = 1;
			}
		}
		else
		{
			fprintf( stderr, "%s: bad input record for check: %s\n", argv0,  obuf );
			rc = 1;
		}
	}

	return rc;
}


int
main(int argc, char **argv)
{
	char buf[NG_BUFFER];
	FILE *fp;
	int i, k;
	int prsize = 0;			/* print size */
	int	lists = 0;		/* input file(s) are lists  of files to generate md5 [size] name data for */
	int	verify = 0;		/* input file(s) are lists of md5 [size] name strings to verify */
	int	rc = 0;			/* in check mode we save the return code(s) and exit 2 if there was a mismatch */

	prsize = 0;
	ARGBEGIN{
	case 'c':	verify = 1; break;
	case 'l':	lists = 1; break;		/* input file(s) are lists of files */
	case 's':	prsize = 1; break;
	case 't':	flags |= FL_TOTAL; break;	/* calc a total chk sum for all files supplied */
	case 'v':	OPT_INC(verbose); break;
	default:
usage:			
			usage( );
			exit( 1 );
			break;
	}ARGEND

	ng_bleat( 2, "ng_md5 %s", version );

	if(argc > 0){
		for(i = 0; i < argc; i++){
			if((fp = fopen(argv[i], "r")) == NULL){
				perror(argv[i]);
				exit(1);
			}
			
			if( verify )
				rc += docheck( fp );		/* we ignore state of -s on command line; we do the right thing in docheck */
			else
			if( lists )
				rc += dolist( fp, prsize );
			else
			{
				do1(fp, buf, prsize);
				if( !(flags & FL_TOTAL) )
					printf("%s %s\n", buf, argv[i]);
			}
			fclose(fp);
		}

		if( flags & FL_TOTAL )
		{
			render( buf, prsize );			/* finish up the calc and generate the string */
			printf("%s\n", buf);
		}
	} else {
		flags &= ~FL_TOTAL;			/* total flag is moot here, we must have it off */
		if( verify )
		{
			rc += docheck( stdin );
		}
		else
		if( lists )
			rc += dolist( stdin, prsize );
		else
		{
			do1(stdin, buf, prsize);
			printf("%s\n", buf);
		}
	}

	exit( rc > 0 ? 1 : 0 );
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_md5 -- compute MD5 checksums
&name   &utitle

&synop  ng_md5 [-c | -l] [-t] [-s] [file ...]

&desc	&ital(Ng_md5) computes the MD5 checksum for the specified files.
	If no files are specified, it computes teh checksum of stdin.
	The &bold(-s) option causes the size to be printed as well.
&space
&opts	The following options are supported on the command line:
&space
&begterms
&term	-c : Check mode.  The input file name(s) are assumed to contain a list 
	of &lit(md5 [size] filename) strings which are compared to a recalculation
	of the checksum for each named file.  Files whose current state does 
	not generate the same checksum value cause an error message to be written
	to the standard error device and for the exit code to be set to non-zero.
&space
	The size token on each input record is optional. The input file(s) may 
	contain a mixture of records with or without size values. 
&space
&term	-l : List mode.  The filename(s) supplied on the command line, or the 
	standard input, are assumed to be newline separated records, each record
	containing the name of a file for which the checksum is computed.  The output 
	is one line per file with the checksum, and filename. If the size option (-s) 
	is also given, the size is included with the other two values.  If both the 
	-c and -l options are given on the command line, the check option overrides.

&space
&term	-s : Size mode. In addition to the checksum value and the file name, the size 
	of the file is written to the standard output device for each file parsed. 
	The size is placed between the checksum and the filename on the output record.
	If the check option is used, the size option is ignored if it is present on the 
	command line. 
&space
&term	-t : Compute the total checksum for all files (listed or on the command line) 
	as if they were concatinated and piped in via standard input.  An error will 
	be generated if the -c option is given with this option as they are not 
	compatable.  As this is a total checksum value, only a single token (the
	md5 string) is written to the standard output. 
&space
&term	-v : Verbose.  Causes &this to be a bit more chatty while operating.  
&endterms

&exit	An exit code of 0 indicates success.  When the check (-c) option is given, 
	this indicates that all files were successfully verified (the current 
	checksum calculation is the same as in the list provided).  For all other 
	modes, a zero return code indicates that all named files could be successfully
	processed. A non-zero return code indicates a failure of some type.  In check 
	mode it indicates that for one or more files the current checksum did not match
	that in the verification file. 

&examp 	Several examples:
&litblkb
   find . -type f | ng_md5 -l    # compute all md5 values for regular files

   ng_md5 -l flist >/tmp/foo     # compute md5 for all files in flist

   ng_md5 -c /tmp/foo            # verify using list saved in /tmp/foo

   ng_md5 somefile               # compute md5 for a single file
&litblke

&mods	
&owner(Andrew Hume)
&lgterms
&term	31 Jan 2006 (ah) : Original code.
&term	18 Sep 2006 (sd) : Added code to support check and list options. (v2.0)
&term	27 Sep 2006 (sd) : Fixed arg processing so that -? generates a usage message.
&term	28 Sep 2006 (sd) : Added -t processing. (HBD DAWD) (v2.1)
&endterms
&scd_end
*/
