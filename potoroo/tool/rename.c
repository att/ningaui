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
# Mnemonic:	ng_rename 
# Abstract:	bloody mv on some systems does a copy regardless. This is killing 
#		us for things like the cartulary and filelists. 
#		
# Date:		09 Aug 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include 	<unistd.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<string.h>


#include	<ningaui.h>

#include	<rename.man>

void usage( )
{
	fprintf( stderr, "usage: %s oldfile newfile\n", argv0 );
	exit( 1 );
}

main( int argc, char **argv )
{
	extern int errno;
	int status;

	ARGBEGIN 
	{
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND
	if( argc < 2 )
		usage( );

	status = rename( argv[0], argv[1] );
	if( status < 0 )
	{
		fprintf( stderr, "%s: %s %s: %s\n", argv0, argv[0], argv[1], strerror( errno ) );
		exit( 1 );
	}
	exit( 0 );
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rename:Renames file atomicly)

&space
&synop	ng_rename oldname newname

&space
&desc	&This changes the name of a file from &ital(oldname) to &ital(newname). It 
	uses the rename() system call to guarentee that the operation is atomic. Both
	files &stress(MUST) be on the same filesystem.  
&space
	This command is &stress(NOT) a &lit(mv) command; it will not copy the old file
	if the new file is on a different filesystem, nor will it handle moving multiple 
	files into a directory.  It is the responsibilty of the script/user that invokes
	this command to ensure that the file is being truely renamed.

&space
&parms	&This expects two filenames on the command line. The filenames are taken literally
	and thus if fully qualified filenames are given, it is assumed that they are 
	both in reference to the same filesystem.

&space
&exit	An exit code of 0 indicates success. Any non-zero exit code inidcates failure.

&space
&see	rename(3)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	09 Aug 2006 (sd) : Its beginning of time. 


&scd_end
------------------------------------------------------------------ */
