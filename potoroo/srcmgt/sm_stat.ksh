#!/usr/common/ast/bin/ksh
# ======================================================================== v1.0/0a157 
#                                                         
#       This software is part of the AT&T Ningaui distribution 
#	
#       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
#	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
#	
#       Ningaui software is licensed under the Common Public
#       License, Version 1.0  by AT&T Intellectual Property.
#                                                                      
#       A copy of the License is available at    
#       http://www.opensource.org/licenses/cpl1.0.txt             
#                                                                      
#       Information and Software Systems Research               
#       AT&T Labs 
#       Florham Park, NJ                            
#	
#	Send questions, comments via email to ningaui_support@research.att.com.
#	
#                                                                      
# ======================================================================== 0a229


#!/usr/common/ast/bin/ksh
# Mnemonic:	sm_stat - generate status of things in a nice tabular format.
#		
# Date:		24 Nov 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
hold_src=$NG_SRC				# possible trash in cartulary
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
NG_SRC=$hold_src

#	write $2 if in verbose mode, else write $1
function natter 
{
	if [[ $verbose -gt 0 ]]
	then
		log_msg "$2"
	else
		echo "$1"
	fi
}

# ------------------------------------------------------------------
NG_CVSHOST=${NG_CVSHOST:-cvs-host-not-defined}
export USER=${USER:-$LOGNAME}			 # bloody suns dont have user
cvsroot=${CVSROOT:-:ext:$USER@$NG_CVSHOST:/cvsroot}
export CVS_RSH=${CVS_RSH:-ssh}

verbose=0
ustring=""
cmd=status
flags=""
mod_only=0		# -m -- show only modified things
new_only=0;		# -N -- show only new things
show_new=1		# -n -- turn listing of new files off 
gencmds=0
raw=0			# output in a raw format for programmes to digest
cvs_raw=0		# output cvs formatted crap

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	gencmds=1;;
		-l)	flags="-l$flags ";;
		-m)	mod_only=1;;
		-N)	new_only=1;;
		-n)	show_new=0;;
		-R)	cvs_raw=1;;
		-r)	raw=1;;
		
	
		-v)	
			verbose=1
			;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


if [[ -z $1 ]] 			# component given
then
	if [[ ! -e CVS ]]
	then
		log_msg "the current directory does not appear to be an export directory"
		cleanup 1
	fi

	set "."
fi

pwd=${PWD##*/}

if [[ $gencmds -gt 0 ]]
then
	cmdf=$TMP/diffcmds.$$
fi

errf=/tmp/err.$$
tfile=$TMP/PID$$.raw

cvs  -d $cvsroot $cmd $flags ${@:-.}  >$tfile 2>&1 

if (( $cvs_raw > 0 ))
then
	cat $tfile
	cleanup 0
fi

	awk \
	-v cwd=$pwd \
	-v mod_only=$mod_only \
	-v verbose=$verbose \
	-v cvsroot="-d $cvsroot" \
	-v gencmds=$cmdf \
	-v new_only=$new_only \
	-v show_new=$show_new \
	-v raw=$raw \
	'
	function print_stuff() 
	{
		if( raw )
		{
			printf( "%s %s %s %s\n", dir "/" fname, state, wr, rr );
			return;
		}
	
		fmt = "%-25s %-15s %10s %10s %10s %10s %10s\n";

		if( fname && ! mod_only || !index( state, "Up" ) )
		{
		if( !headers++ )
		{
			if( dir )
			{
				printf( "%c%s\n", headers ? "\n": "", dir );
				wdir = dir "/";
				if( dir == cwd )
					wdir = "";			# not needed if its the only thing 
					
				dir = "";
			}
			printf( fmt, "File", "State", "Wrk Rev", "Rep Rev", "Tag", "Date", "Options" );
		}

			gsub( ".none.", "", st );
			gsub( ".none.", "", sd );
			gsub( ".none.", "", so );

			diff_cmd = "";
			if( gencmds )
				printf( "cvs %s diff -r%s %s | more\n", cvsroot, rr, wdir fname ) >gencmds;

			printf( fmt, fname, state, wr, rr, st, sd, so );

		}

		fname = "";
	}

	BEGIN { dir = "New Files"; }

	/\?/ {
		if( ! show_new )
			next;

		if( match( $2, "[.]h$" ) || match( $2, "[.]c$" ) || match( $2, "[.]ksh$" ) || match( $2, "[.]rb$" )  || match( $2, "[.]py$" ) || match( $2, "[.]py$" ) || match( $2, "[.]l$" ) || match( $2, "[.]y$" ) || match( $2, "[.]csh$" ) || match( $2, "[.]xfm$" ) || match( $2, "[.]im$" ) )
		{
			new = new $2 " ";
			if( ! new_only )
			{
				fname = $2;
				state = "Not Added!"
				print_stuff( );
				fname = "";
			}
		}
		next; 
	}
		
	/^File:/ {
		if( fname && ! new_only )
			print_stuff();
		
		if( $2 == "no" && $3 == "file" )
			fname = $4;
		else
			fname = $2;
		state = $NF;
		next;
	}


	/Working revision:.*New file!/ { wr = "new"; next; }
 	/Working revision:/ { wr = $NF; next; }

   	/Repository revision:.*No revision control file/ { rr = no_file; next; }
   	/Repository revision:/ { rr= $(NF-1); next; }

   	/Sticky Tag:/    { st = $3; next; }
   	/Sticky Date:/ { sd = $3; next; } 
   	/Sticky Options:/ { so = $3; next; }

	/xamining/ { 
		if( fname && ! new_only )
			print_stuff( );

		if( $NF != "." )
			dir = $NF;
		else
			dir = cwd;

		headers = 0;
		next;
	}

	END {
		if( new_only )
			printf( "%s\n", new );
		else
			if( fname )
				print_stuff( );
	}

	' <$tfile

if [[ $gencmds -gt 0  ]]
then
	if [[ -s $cmdf ]]
	then
		gre -v New $cmdf
	else
		verbose "no diff commands generated"
	fi
fi

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_stat:Source Management show status for something)

&space
&synop	[-c] [-m] [source-object]

&space
&desc	&This invokes the source manager's status command to show that status of 
	the source-object(s) listed on the command line. If the &ital(source-object)
	is omitted, then the current directory is assumed and all objects in the directory
	are listed.  The object may be a directory name.  The output is a table that 
	indicates the source file name, current state, working revision number, 
	repository revision number and any sticky information associated with the checked 
	out file. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Generates the cvs difference commands that can be executed to display the 
	differecnes between the latest checked in source and the source in the working 
	tree that is not the same.
&space
&term	 -m : Show only files that are modified (not up to date).
&space
&term	-R : Generate "raw" output as returned directly from the underlying source management system.
&space
&term	-r : Generate "formatted raw" data.  Each line contains the module name, state, repository revision and working revision data.
&endterms


&space
&parms	&This accepts any number of positional parameters. These are passed to the 
	on the command to the source manager as targets of the status command.

&space
&exit	The exit code is set to the return value from the last command executed
	processing the table.

&space
&see	ng_sm_fetch, ng_sm_replace, ng_sm_tag

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Nov 2004 (sd) : Fresh from the oven.
&term	03 Dec 2004 (sd) : Correctly reports files that are removed before the remove is 
	committed.
&term	20 Dec 2004 (sd) : Added -c option to list diff commands.
&term	04 Jan 2005 (sd) : Corrected problem with cat of diff commands.
&term	24 Jan 2005 (sd) : Fixed bug with the diff command output.
&term	07 Feb 2005 (sd) : Added ability to warn about possible new files.
&term	22 Jun 2007 (sd) : Eliminated an error message that was generated when the  -c 
		option is given, and no difference commands are generated, 
	
&term	15 Jan 2008 (sd) : Added support to flag ruby files that are not added.
&term	06 Jan 2009 (sd) : Noticed that sometimes cvs is not returning all data. Redirection
		to a file rather than piping it into awk seems to prevent this. 
&endterms

&scd_end
------------------------------------------------------------------ */
