#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
#
#	This script is installed as fixup_wrapper as site_prep will fix
#	the _ROOT_PATH_ with each node start and install it in $NG_ROOT/bin.
#	this allows ng_root to change between boots if needed; old method
#	did not.
#
#  Mnemonic:	ng_exec.ksh
#  Abstract: 	Small script that does nothing but to establish the ningaui
#		environment and then invokes whatever was placed on the 
#		commandline.
#  Date:	3 August 2001
#  Author: 	E. Scott Daniels
# --------------------------------------------------------------------------

NG_ROOT=${NG_ROOT:-_ROOT_PATH_}			# this value is set by ng_setup, so a default to the real path on this node is LEGIT!

# ------------------- not safe to use stdfunctions like cleanup 

# we must do these two options (-s and -r) before going after the cartulary or *fun
if [[ "$1" == *"-s"* ]]
then
	if ! [[ -d $NG_ROOT/bin ]]	#NG_ROOT value is hosed
	then
		exit 1 
	fi

	export PATH="$NG_ROOT/bin:$PATH"
	case $(ng_ps| grep -c '[n]g_green_light') in
		0)	exit 2;;
		*)	exit 0;;
	esac

	exit 3				# should not need
fi

if [[ "$1" == *"-r"* ]]			# get the hard ng_root value; verify it seems sane
then
	if ! [[ -d $NG_ROOT/bin ]]	#NG_ROOT value is hosed
	then
		echo ""			# ensure we send nothing back
		exit 1 
	fi

	echo "$NG_ROOT"
	exit 0
fi


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}   # get standard configuration file (old default for backward compatability)
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun}  # get process query oriented functions
. $PROCFUN


# ---------------------- now safe to use standard functions like show_man and cleanup

stdout=""
stderr=""

how=normal
while [[  $1 == -*  ]]
do
	case $1 in 
		-e)	eval echo \"\$${2:-x no var name supplied}; exit 0\";;

		-s)	;;	# place holder.  taken care of, and we exited, before.
		-r)	;;	# place holder.  taken care of, and we exited, before.

		-o*)	stdout="$stdout ${1#??}>$2"; shift;;
		-O*)	
			if [[ $2 == "stdout" ]]
			then
				stdout="$stdout ${1#??}>$2"; 		# cannot allow >>&2; ksh barfs
			else
				stdout="$stdout ${1#??}>>$2"; 		# setup to append to file
			fi

			shift;;
	
		--detach) how=detach;;
	
		--exec)	how=exec;;					# should not need this, but keep for back compat
	
		--man|-\?) show_man; exit 1;;
		-*)	echo "unrecognised option"; show_man; exit 1;;
	esac

	shift
done

case $how in
	detach)
			eval "$@" ${stdout//stdout/"&1"} &
			exit 0
			;;
	exec)
			eval exec "$@" ${stdout//stdout/"&1"}
			exit $?
			;;
	normal)
			eval "$@" ${stdout//stdout/"&1"}
			exit $?
			;;
esac

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_wrapper:Small script wrapper to establish the Ningaui environment)

&space
&synop	ng_wrapper [-o{1|2} filename] [--detach] command parms	
&break	ng_wrapper -e variable-name 
&break	ng_wrapper -s
&break	ng_wrapper -r

&space
&desc	&This is a small wrapper script that will setup the Ningaui 
	environment and then invoke the command that was entered on 
	the command line.   It also provides several environmental checks
	depending on command line options.  These checks include:
&space
&beglist
&item	Echoing the value of an environment variable to the standard output device. 
&item	Checking the node to determine if it is safe to execute ningaui processes and 
	exiting with a bad (non-zero) return code if it is not.
&item	Verifying the value of the 'hard defined' NG_ROOT directory is legitimate and
	echoing the value to stdout if it is. 
&endlist

&space
&opts	&This supports the following commandline options. 
&begterms
&term	-e name : Echos the current setting of the environment variable &ital(name)
	to standard output.  The value echoed to standard out will be tha value that 
	a script will have when run in the ningaui environment.  The primary use for 
	this option is to allow values on other nodes to be easily determined.
&space
	NOTE: &break
	Using &lit(-e NG_ROOT) is not safe while
	the node is initialising.  Scripts that need to have a verified value of 
	NG_ROOT should use the -r option of this script (see below). 

&space
&term	-o[n] file : Set the standard output for the command. If &ital(n) is supplied, 
	it is assumed to be a file descriptor number and is added to the front of the 
	redirect symbol.  For instance, &lit(-o2 foo) redirects  stderr to foo using
	2>foo.  The file descriptor can be omiited. If standard error is to be redirected
	to standard output, then the following option pairs can be used:
&space
&litblkb
  -O /tmp/foo.out -o2 stdout 
&litblke

	The keyword &lit(stdout) is translated to &lit(^&1) just before the command is 
	issued eliminating the need for quotes if &2 were attempted to be submitted 
	as the parameter to -o2.  

&space
&term	-O[n] file : Same as -o except that >> is used instead of > to append to the 
	file. 
&space
&term	-r : Return the 'hard coded' value for NG_ROOT.  The ng_wrapper script has a hard 
	coded value installed during node start. When this option is supplied on the 
	command line, the hardcoded value for NG_ROOT is verified as being sane (the 
	referenced directory contains a &lit(bin) directory) and if it seems ok, its value
	is written to the standard output device.  If the value does not seem right, 
	a blank line is written, and the script exits with a non-zero exit code. 

&space
&term	-s : Safe to run. Exits with a good (0) return code if it is safe to run (ng_green_light
	is running). If supplied, this MUST be the first token on the command line and all 
	other tokens are ignored.
	
&space
&term	--detach : &This detaches from the controlling tty device before running the 
	command. When this option is supplied, the calling programme will always
	receive a good exit code. 
&endterms
&space

&parms	All tokens on the command line, beggining with the first token that does not start with a dash, 
	are considered to be a part of the command to execute and are not specicifically
	parsed by &this.

&space
&exit	The exit code returned by &this is the exit code returned to &this
	by the command that it executed. If async execution of the command
	is requested (--detach option) then the exit code is always 0.

&space
	When the -s (safe to run) option is given, the exit code should be one of 
	three values with defined meanings:
&space
&begterm
&term	0 : It is safe to run (true when &this is used as the expression of a shell if).
&space
&term	1 : The NG_ROOT value does not seem right; it is not safe to execute.
&space
&term	2 : The ng_green_light process is not running; it is not safe to execute.
&endterm
&space
	Any other non-zero values are defined only to mean that it is not safe to execute 
	any/certain ningaui commands.

&space
	When the get NG_ROOT option (-r) is used, a non-zero return code indicates that 
	the hard value of root that was placed into the script during node start does not
	seem correct. 

&see	ng_init, ng_node

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Aug 2001 (sd) : Original.
&term	17 Aug 2004 (sd) : Added -e option.
&term	14 Jan 2004 (sd) : Added -o and -O options to make redirection from rota easier/possible.
&term	03 Mar 2004 (sd) : Added -s option to test safe to run. 
&term	01 Apr 2005 (sd) : Corrected search for -s option.
&term	14 Jul 2005 (sd) : Corrected doc.
&term	18 Feb 2006 (sd) : Mod to make -e work under cygwin.
&term	28 Mar 2008 (sd) : Conversion to supporting cartulary defult as NG_ROOT/site/cartulary if NG_CARTULARY 
	is not set and site/cartulary exists.
&term	14 Jul 2008 (sd) : Yankked code to allow cartulary in site (cartulary was never moved from NG_ROOT).
	Corrected issue when '-O2 stdout' was used.  The intent was that -o2 always be used with stdout, but
	as some use of '-O2 stdout' existed, this script now ensures that the correct redirection is generated. 
	.** Corrected a bug with cuscus waldo lnode

&endterms

&scd_end
------------------------------------------------------------------ */
