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

/* -----------------------------------------------------------------------------------
	Mnemonic:	spaceman_nm
	Abstract:	generates a request to spaceman and returns the filename 
			result on stdout. 
	Date:		7 June 2001 
	Author: 	E. Scott Daniels
   -----------------------------------------------------------------------------------
*/
#include	<sys/stat.h>
#include	<sys/types.h>
#include	<stdio.h> 
#include	<unistd.h>
#include	<stdlib.h>
#include	<syslog.h>
#include	<stddef.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>

#include	"spaceman_nm.man"

#define FL_NO_MARKER	0x01		/* do not add a marker (~) to the name */

char	*argv0 = NULL;
int	verbose = 0;
int	flags = 0;

static void usage( )
{
	sfprintf( sfstderr, "usage: %s [-s path] [-v] [-r] file [file2... filen]\n", argv0 );
	exit( 1 );
}

	/* add a marker character to the file name if there is not one 
		must add ~ to avoid issues with ng_tube that uses ! as its seperator 
	*/
static char *add_mark(	char *name )
{
	char	*m;
	char	*nname;			/* new name */
	int	len;

	if( ! name )
		return NULL;

	len = strlen( name );
	m = name + (len -1 );		/* at last ch */

	switch( *m )
	{
		case '/':	break;			/* directory -- no adj */
		case '!':	break;			/* has marker -- no adj */
		case '~':	break;

		default: 				/* add */
			if( flags & FL_NO_MARKER )		/* dont add marker flag */
				break;				/* use name as is */

			nname = ng_malloc( sizeof( char ) * (len + 2), "add_mark: name buffer" );
			sfsprintf( nname, len + 1, "%s%c", name, '~' );
			free( name );
			name = nname;
			*(name + len + 1) = 0;		/* sprintf will not terminate the string */
			break;
	}

	return name; 
}

/* generate a name specificly in a target fileystem
*/
char *gen_specific( char *fs, char *fn )
{
	char *c;
	char *p;
	char buf[NG_BUFFER];

	ng_spaceman_reset( );
	ng_spaceman_fstlist( buf, NG_BUFFER-1, ' ' );

	if( (c = strstr( buf, fs )) != NULL )
	{
		p = strchr( c, ' ' );           /* past fsname */
		p= strchr( p+1, ' ' );         /* past hldcount */
		if( (p = strchr( p+1, ' ' ))  ) /* past third field */
			*p = 0;                 /* terminate the string */

		if( (p = ng_spaceman_rand( c, fn )) == NULL )
			ng_bleat( 0, "ng_spaceman_nm/gen_specific: call to spaceman_rand() failed" );
		else
			return p;
	}
	else
		ng_bleat( 0, "ng_spaceman_nm: bad filesystem name for specific request: %s", fs );

	return NULL;
}


int main( int argc, char **argv )
{
	char	*ep;					/* pointer at env var */
	char	*p;					/* path returned */
	char	*propy = "/no_such_path/";		/* bogus, should not occur */
	int	random = 1;				/* generate a random nname, not the propylaeum (5/27/03 - default)*/
	int	exit_code = 0; 
	char	*specific_fs = NULL;
	char	buf[NG_BUFFER];

#ifdef KEEP_PROPY
	if( (propy = ng_env( "NG_RM_PROPYLAEUM" )) == NULL )
		propy = ng_env( "NG_PROPYLAEUM" );			/* original name */
#endif

	
	if( (ep = ng_env( "NG_SPACEMAN_NO_MARKER" )) )		/* if it exists, we use this to set value */
	{
		if( atoi( ep ) )
		{
			flags |= FL_NO_MARKER;			/* set it if it was set */
			ng_bleat( 2, "filename marker (~) disabled with NG_SPACEMAN_NO_MARKER value" );
		}
	}
	

	ARGBEGIN
	{
		case 'r':	random = 1; break;		/* deprecated (ignored) as we are always in random mode now */
		case 's':	SARG( specific_fs ); break;	/* user wants in a specific filesystem */
		case 't':	flags |= FL_NO_MARKER; break;	/* do not add a marker character */
		case 'v': 	OPT_INC( verbose ); break;
		case '?':	usage( ); exit( 1 );
usage:
		default:
				usage( );
				exit( 1 );
	}
	ARGEND

#ifdef KEEP_PROPY
	if( propy == NULL )
	{
		ng_bleat( 0, "cannot find NG_RM_PROPYLAEUM in environment/cartulary" );
		exit( 1 );
	}
#endif

	if( argc )
	{
		while( argc-- )
		{
			if( (p = add_mark( specific_fs ? gen_specific( specific_fs, *argv ) : ng_spaceman_rand( NULL, *argv ))) )
			{
				sfprintf( sfstdout, "%s\n", p );      		/* put out the file name returned */
				free( p );				/* moot */
				p = 0;
			}
			else
			{
				sfprintf( sfstdout, "/unable/to_get_space/%s\n", *argv );
				exit_code = 1;
			}

			argv++;
		}
	}
	else
	{
		p =  ng_spaceman_rand( NULL, NULL );		/* no name, assume we generate directory and thus no mark */
		sfprintf( sfstdout, "%s\n", p );      		/* put out the file name returned */
		free( p );				/* moot */
	}

	exit( exit_code );
	return exit_code;			/* keep the compilers happy */
}


#ifdef NEVER_DEFINED
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(spaceman_nm:Generate a name in a filesystem managed by Spaceman)

&space
&synop	ng_spaceman_nm [-v] [-s path] [-t] [file1 file2... filen]

&space
&desc	&This generates a fully qualified pathname for each &ital(file) passed in 
	from the command line. The directory portion of the pathname will be 
	the &ital(propylaeum) unless the random (-r) option is supplied. When 
	the random option is supplied (not recommended for most user of &this)
	the directory portion of the pathname is randomly selected from one of 
	the available &ital(replication manager) filesystems on the local node. 
&space
	If no filename is provided on the commad line, then just the directory
	portion of the pathname is returned.

&space
	If all filesystems on the local node are currently reporting less 
	space available than defined by the &lit(NG_RM_FS_FULL) cartulary variable, 
	an invalid filepath will be returned.  Using the bad pathname should 
	result in an error, and thus the caller of this programme does not 
	need to check for an error.
&space
&subcat Filename Policy
	As files are created in replicaiton manager space, it is possible for
	other tools to accidently work on the file before the file is completed
	or registered.  In order to prevent such collisions, filenames are 
	generated with a trailing tilde (~) mark character.  This mark causes
	the file to be ignored by these other tools.  The creator of the file 
	must either register the file (ng_rm_register), or publish it (via the 
	standard funciton publish_file) in order to properly remove the tilde 
	from the filename.  Programmes/scripts that use &this and ng_rm_register
	will not need to make any changes to implement this policy.  

.if [ 0 ]
THIS IS TOO DANGEROUS SO IT IS NO LONGER A PART OF THE MAN PAGE
&subcat	Available file space
&space	If a script knows that the file to be created will be larger 
	larger than the currently defined &lit(NG_RM_FS_FULL) value, 
	then it should set the &lit(NG_RM_FS_FULL) value on the comand line 
	prior to execution.  The value should be of the aprroximate size 
	of  the  file.  which will cause &this to 
	return a filename from a filesystem that has at least the indicated
	amount of space.  
	.** NO this could not have been coded as a paramter as it is the 
	.** library routines that do this, and they do not get the threshold
	.** value anyplace other than the environment/cartulary.
.fi


&space
&opts	&This supports the following options when entered on the command line:
&smterms
&term	-s path : Supplies a specific filesystem path (must be in the replication manager
	arena perview) to use.  The path generated will be within this filesystem.
	The use of this option is considered dangerous and is thus not recommended; it 
	is provided to specificly support the incoming file process in potoroo. You 
	have been warned!
&space

&term	-t : Do not add a marker (tilda) to the end of the file.  This is 
	dangerious, and should only be used in extreme situations.  The marker
	is added to prevent the file from being acted upon by replication manager
	tools while the file is being created/adjusted.
&space
&term	-v : Execute in verbose mode. &This will natter on about various things.
	The more &lit(-v) options entered, the chattier it wants to be.
&endterms


&examples
	Get a file name from a filesystem that has at least 800 Mb free:
.sp
	NG_RM_FS_FULL=800 ng_spaceman_nm myfile|read mf_path
&space
&exit	&This will always exit with a zero (good) after having generated  a string to the 
	standard output device.  This infoms the caller that the string generated is considered
	valid; even if it indicates 'no space.' 
	An exit code that is not zero is likey a result
	of an internal (unexpected) error, or other system problem.

&space
&see	&seelink(lib:ng_spaceman) &seelink(rm:ng_repmgr) &seelink(lib:ng_spaceman_if)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Jun 2001 (sd) : Code birth.
&term	16 Apr 2001 (sd) : Rewrote to work with the new paradigm of spaceman.
&term	02 Jun 2003 (sd) : To make use of the new stuff in ng_spaceman_if lib set, and 
	to remove propylaeum references.
.** left the propy things just in case we decide to revert back someday.
&term	08 Jun 2005 (sd) : Now adds ~ marker to end of filenames.
&term	07 Aug 2005 (sd) : Added -t option.
&term	06 Sep 2005 (sd) : Now turns off ~ if NG_SPACEMAN_NO_MARKER is set to !0.
&term	18 Jan 2006 (sd) : Prevent core dump if arena is not defined.
&term	09 Mar 2006 (sd) : added -s (specific) option. 
&term	23 Aug 1006 (sd) : Updated manpage to reflect spaceman interface change with 
		regard to NG_RM_FS_FULL variable.
&term	28 Aug 2006 (sd) : Pulled references to deprecated spaceman_req. 
&term	03 Jun2 008 (sd) : Cleaned up man page. 
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
