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
 Mnemonic:	tmp_nm 
 Abstract:	Generate a temp name in $TMP based on basename(s) passed 
		into this process. 
		File names will be created with the syntax basename.pid.randombits
		
 Date:		08 March 2005
 Author:	E. Scott Daniels (based on Adamm's original)
# ---------------------------------------------------------------------------------
*/
#include 	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_lib.h>
#include	<tmp_nm.man>


void usage( )
{
	sfprintf( sfstderr, "usage: %s [-s num] [rel-path/]basename [basename...]\n", argv0 );
}

	/* given a buffer with a pattern, create a name, open/close the file and write the name to stdout */
void make_name( char *buf )
{
	int	fd = -1;

	if( (fd = mkstemp( buf )) < 0 )
	{
		ng_bleat( 0, "mkstemp failure: %s: %s", buf, strerror( errno ) );
		exit( 3 );
	}

	sfprintf( stdout, "%s", buf );
	close( fd );				/* file left created to avoid race cond */
}

extern 	int verbose;

int main( int argc, char **argv )
{
	int	pid = 0;
	int	i;
	int	j;
	int	sname = -1;			/* generate sequentially numbered series of names */
	char	*tmp = NULL;
	char	filename[NG_BUFFER];

	verbose = 0;
	pid = getppid( );			/* assume parent is the one using the file */

	ARGBEGIN
	{
		case 'p':	IARG( pid ); break;		/* allow override to support get_tmp_nm function and squirrly shell */
		case 's':	IARG( sname ); break;		/* sequential name */
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	if( argc < 1 )
	{
		usage( );
		exit( 1 );
	}

	if( (tmp = ng_env( "TMP" )) == NULL )
	{
		ng_bleat( 0, "could not find $TMP in environment/cartulary" );
		exit( 2 );
	}


	for( i = 0; i < argc; i++ )
	{
		if( sname > 0 )
		{
			for( j = 0; j < sname; j++ )
			{
				if( j )
					sfprintf( sfstdout, " " );			/* space separate from last */
				sfsprintf( filename, sizeof( filename ), "%s/%s.%d.%d.XXXXXX", tmp, argv[i], j, pid );
				make_name( filename );
			}
	
		}
		else
		{
			sfsprintf( filename, sizeof( filename ), "%s/%s.%d.XXXXXX", tmp, argv[i], pid );
			make_name( filename );
		}

		if( i < argc - 1 )	/* more names */
			sfprintf( sfstdout, " " );
	}

	sfprintf( stdout, "\n" );

	exit( 0 );
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tmp_nm:Generate a unique name in $TMP)

&space
&synop	ng_tmp_nm [-p pid] [-s num] [dir/]basename [[dir/]basename...]

&space
&desc	&This will create one or more files in the &lit($TMP) directory
	using the basename provided.  The basename may optionally be prefixed
	with a directory path that is assumed to already exist in &lit($TMP). 
	For each name supplied on the command line, a temporary name is generated.
	When more than one name is supplied, the generated fully qualified names 
	are written as a space separated list to the standard output device. 
	The filename(s) created will have the form:
&space
	&lit(/tmp-path/)&ital(usr-path/basename)&lit(.ppp.xxxxxx)
&space
	Where:
&begterms
&term	tmp-path : Is the directory path defined as $TMP in the cartulary.
&space
&term	usr-path : Is the optional user supplied directory path prepended to the 
	basename.
&space
&term	basename : Is the basename supplied on the command line.
&space
&term	ppp : Is the process id of the process that invokes &this (as returned 
	by getpid(); see the discussion wth -p later).
&space
&term	xxxxxx : Is a randomly generated string that makes the filename unique.
&endterms

&space
	As the filename is generated, the file is created so as to prevent the 
	duplication of filenames if there is a lag between name generation and 
	file usage.  

&space
&subcat Sequential naming
	It is possible, using the &lit(-s) option, to supply a single basename
	and to generate a sequential list of file names.  When this is done, the 
	number supplied as the value (n) to the &lit(-s) option is used as the upper 
	boundary; n files are created with a sequence number of 0 to n-1. The 
	sequence number is embedded in the filename between the basename and the 
	pid. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p pid : Supplies the process id to use.  Normally the process id that is used
	is the parrent process id of &this.  In most cases this is suitable, however
	if &this is invoked from a shell function, there is a chance that the function 
	is actually exeuting in a subshell and that the pid would not be the expected 
	id.  If it is important to the invoking script, &lit(-p $$) can be passed 
	such that it is ensured that all tmp files ccary the same process id. 
&space

&term	-s n : Generate n files for each basename supplied on the command line.  Sequence 
	numbers are embedded inside of the filename between the basename and the pid.
&endterms


&space
&parms	One or more basenames may be supplied on the command line. Optionally, each basename
	may have directory path specification prepended to it.  It is assumed that this 
	path already exists in the &lit($TMP) directory. 

&examp	The following illustrate how this process can be invoked from within a script.

&space
&litblkb
    wrk_file=`ng_tmp_nm df_mon_wrk`
   
    ng_tmp_nm df_mon_wrk df_mon_bcast df_mon_mail | read wrkf bcastf mailf

    ng_tmp_nm -s 3 df_mon | read wrkf bcastf mailf

    flist=`ng_tmp_nm repmgr/flist`
&litblke

&space
&exit	An exit of 0 indicates that all names were successfully created. Any non-zero 
	exit indicates an error, and a message is likely to have been written to the 
	standard error device. 

&space
&see	mkstemp

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 Mar 2005 (sd) : Sunrise.
&term	19 Jul 2005 (sd) : Added more detail to failure message. 
&term	05 Feb 2005 (sd) : Fixed trailing blank problem.
&term	11 Apr 2007 (sd) : Corrected bug that was preventing spaces between list members when 
		using the -s option.
&endterms

&scd_end
------------------------------------------------------------------ */
