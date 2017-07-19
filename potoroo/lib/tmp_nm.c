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


	/* given a buffer with a pattern, create a name, open/close the file and write the name to stdout */
static void make_name( char *buf )
{
	int	fd = -1;

	if( (fd = mkstemp( buf )) < 0 )
	{
		ng_bleat( 0, "ng_tmp_nm: mkstemp failed: %s: %s", buf, strerror( errno ) );
		*buf = 0;
		return;
	}

	ng_bleat( 3, "ng_tmp_nm:  created: %s", buf );
	close( fd );				/* file left created to avoid race cond */
}


char *ng_tmp_nm( char *base )
{
	int	pid = 0;
	int	i;
	int	j;
	int	sname = -1;			/* generate sequentially numbered series of names */
	char	*tmp = NULL;
	char	filename[NG_BUFFER];

	pid = getppid( );			/* assume parent is the one using the file */

	if( (tmp = ng_env( "TMP" )) == NULL )
	{
		ng_bleat( 0, "ng_tmp_nm: could not find $TMP in environment/cartulary" );
		return NULL;
	}


	if( *base == '/' )			/* do not add TMP/ to the beginning */
		sfsprintf( filename, sizeof( filename ), "%s.%d.XXXXXX", base, pid );
	else
		sfsprintf( filename, sizeof( filename ), "%s/%s.%d.XXXXXX", tmp, base, pid );

	make_name( filename );

	if( *filename )
		return strdup( filename );

	return NULL;
}

#ifdef SELF_TEST
main( )
{
	char *c;

	c = ng_tmp_nm( "daniels" );
	printf( "returned: %s\n", c );

	c = ng_tmp_nm( "/tmp/daniels/xxx" );
	printf( "returned: %s\n", c );
}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tmp_nm:Generate a unique name in $TMP or other directory)

&space
&synop	ng_tmp_nm( char *name )

&space
&desc	&This will create a uniquely named temporary file in $TMP. The file 
	is left closed and a pointer to the filename is returned.  If the name 
	given as the only argument dos NOT begin with a slant, then the 
	file is created in the &lit(TMP) directory as TMP is defined in the 
	current environment. The user may supply a path, without the leading 
	slant character, to crate a temporary file in a subdirectory to 
	&lit(TMP.)  
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
&see	mkstemp

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 Mar 2005 (sd) : Sunrise.
&endterms

&scd_end
------------------------------------------------------------------ */
