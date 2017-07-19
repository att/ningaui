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
# Mnemonic:	sm_build -  
# Abstract:	will fetch a copy of the ningaui source, set up the directory
#		and then build things.  Assumes a Makefile at the root source level.
#		
# Date:		1 December 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
hold_src=${NG_SRC:-not_defined}			# possible trash in cartulary

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
if [[ -x $CARTULARY ]]
then
	. $CARTULARY
	. $STDFUN
else				# some basic functions if running on a non-potoroo box
	function verbose
	{
		if [[ $verbose -gt 0 ]]
		then
			echo "$@"
		fi
	}

	function log_msg
	{
		echo "$@"
	}

	function cleanup 
	{
		rm -f /tmp/*.$$ $TMP/*.$$
	}
fi

if [[ -d /opt/SUNWspro/bin ]]    	# bloody sun os -- these are all to support sun; erk!
then
	PATH="$PATH:/opt/SUNWspro/bin"
fi

if [[ -d /usr/ucb ]]
then
	PATH="$PATH:/usr/ucb"
fi

if [[ -d /usr/ccs/bin ]]                # bloody sun os
then
	PATH="$PATH:/usr/ccs/bin"
fi					# ----- end sun specific  crap -------

if ! whence gcc >/dev/null		# seems sun-386 does not have gcc installed
then
	verbose "cannot find gcc, will try to build with cc"
	export GLMK_CC=cc
fi


function make_dir
{
	while [[ -n $1 ]]
	do
		if [[ ! -d $1 ]]
		then
			if ! mkdir $1
			then
				log_msg "could not make directory: $1"
				cleanup 1
			fi
		fi

		shift
	done
}

TMP=${TMP:-/tmp}
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
export USER=${USER:-$LOGNAME}				 # bloody suns seem not to set this up
cvsroot=${CVSROOT:-:ext:$USER@$NG_CVSHOST:/cvsroot}
export CVS_RSH=${CVS_RSH:-ssh}

tag_name=""
forreal=""
default_prefix="kgb_ningaui"
verbose=0
setup=0
overlay=""			# optional overlay file to apply to build tree after export
fetch_cmd="-e"
fetch_opts="-s "		# at a minimum do set up after fetch
fetch_it=1			# should fetch source 
src_root=ningaui			# assume root of soruce .../dev/ningaui  or .../build/ningaui
purge_log=0
ignore_errors=0			# -i sets to keep trucking through commmand line things even after one fails

bdir=$TMP/$USER/build		# we will default here if both exist

ustring="{-e|-u|-c} [-n] [-D build-dir] [-p prefix] [-d date | -T tag_number] [-n] [-r src-root] [-v] [[dir:]target]"


while [[ $1 = -* ]]
do
	case $1 in 
		-c)	fetch_cmd="-c";;		# varuous command modifiers
		-e)	fetch_cmd="-e";;
		-u)	fetch_cmd="-u";;		# update with repository
		-U)	fetch_cmd="-U";;		# remove sticky tags all together (humm. danger?)

							# other options
		-b)	fetch_opts="$fetch_opts-b ";;	# use the bleeding edge
		-d)	date="$2"; use_tag=0; shift;;	# extract/checkout as of date
		-D)	bdir="$2"; shift;;		# override build directory
		-i)	ignore_errors=1;;		# keeps going through command line things 
		-n)	mk_opts="$mk_opts-n "; forreal="echo would execute:";;
		-N)	fetch_it=0;;
		-p) 	fetch_opts="$fetch_opts-p $2 "; shift;;
		-P)	purge_log=1;;					# causes purge of log first
		-r)	src_root=$2; shift;;				# src root under build or dev
		-T)	fetch_opts="$fetch_opts-T $2"; shift;;		# something other than the latest kgb tag
		
		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

verbose "verifying things"
if [[ ! -d $bdir ]]
then
	if [[ ${bdir%/*} ]]	# if parent directory exists; then we will create it
	then
		make_dir $bdir
	else
		log_msg "cannot find build directory: $bdir"
		cleanup 1
	fi
fi

export NG_SRC=$bdir/$src_root

if ! cd $bdir 
then
	log_msg "could not switch to build directory: $bdir"
	cleanup 1
fi

if [[ $fetch_it -gt 0 ]]
then
	flog=`pwd`/$USER.fetch_log
	verbose "fetching source into $bdir; date=$date  fetch_opts=($fetch_opts)"
	if [[ -n $date ]]
	then
		$forreal ng_sm_fetch -d "$date" $fetch_opts $fetch_cmd $src_root >$flog 2>&1
	else
		$forreal ng_sm_fetch $fetch_opts $fetch_cmd $src_root >$flog 2>&1
	fi

	if [[ $? -ne 0 ]]
	then
		log_msg "fetch error(s); see log $log [FALIED]"
		cleanup 1
	fi

	verbose "fetch complete [OK]"
else
	verbose "fetch skipped (-N)" 
fi

if ! cd $NG_SRC
then
	if [[ -z $forreal ]]
	then
		log_msg "could not switch to src_root directory: $NG_SRC"
	else
		log_msg "stopping: $NG_SRC does not exist (not fetched because -n option?)"
	fi	
	cleanup 1
fi

if [[ ! -d include ]]
then
	if ! make_dir include stlib .bin include/copybooks
	then
		log_msg "could not create some/all directories: include stlib .bin in $src_root"		# should not get here!
		cleanup 1
	fi
fi 

log=`pwd`/$USER.build_log
if [[ $purge_log -gt 0 ]]
then
	>$log
fi

while [[ -n $1 ]]
do
	verbose "making $1"
	(
		if [[ $1 = *:* ]]
		then
			d=${1%%:*}	# directory
			t=${1##*:}	# target

			if ! cd $d 
			then
				log_msg "could not switch to $d ($1)"
				cleanup 1
			fi
	 	else
			t=$1
		fi	

		echo "===== `date`  starting: $1  pwd=$PWD =====" >>$log
		verbose "starting $1"
		if ! ng_make $mk_opts $t >>$log 2>&1
		then
			echo "ng_build: status: ***** `date`  ERROR: $1    pwd=$PWD ***** [FAILED]" >>$log
			verbose "finished: $1 [FAILED]" 
			if [[ ${ignore_errors:-0} -eq 0 ]]
			then
				cleanup 1
			fi
		else
			echo "ng_build: status: ----- `date`  success: $1    pwd=$PWD ----- [OK]" >>$log
			verbose "finished: $1 [OK]" 
		fi
	)


	shift
done

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_build:Source Management build manager)

&space
&synop	ng_sm_build {-c|-u|-e} [-D build-dir] [-p prefix] [-i] [-n] [-s] [-b | -d date | -T tag-num] [-r src-root-dir]  [-v] [dir:]target

&space
&desc	&This will cause a copy of the source to be exported into the user's build
	directory, and then will run one or more &lit(nmake) commands to build the 
	target(s) supplied on the command line.  By default, source is exported, not 
	checked out, though this can be overridden with a command line option (-c). 
	&space

	The user's build  directory is assumed to be &lit($TMP/$USER/build).
	If the directory does not exist, but the directory &lit($TMP/$USER) does, 
	the build directory is created.
	The -D option is available if this assumption is incorrect.  The default source 
	root is &lit(ningaui) and can be overridden with the &lit(-r) option.  The 
	source root is the directory where the &lit(nmake) commands will be executed. 
	
&space
	The last known good build (kgb) source is fetched unless a specific tag number, 
	or prefix and tag number, are supplied on the command line.  The &lit(-b)
	option can be supplied to cause the bleeding edge source (most recent of each 
	source file in the repository) to be fetched, or the &lit(-d) option can be 
	used to fetch as of a given date.
&space
	The targets supplied on the command line can be prepended with a directory 
	name (e.g. potoroo/pinkpages:) which causes the working directory to be switched
	before the &lit(nmake) command is executed.  If target(s) do not have a colon 
	embedded, then &lit(nmake) is executed at the top source tree level.  Building
	with the target &lit(package) will cause a nuke, precompile, all, package
	to be executed at the top level.


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Fetch out the most recent version (bleeding edge) of the source root (ningaui by 
	default if -r not supplied).
&space
&term 	-c : Fetch will actually check out the source rather than exporting it.
&space
&term	-d date : Fetch source as of &ital(date).
	&ital(Date should be quoted and have the syntax of yyyy-mm-dd hh:mm. 
	This option and the two tag 
	options (-t and -T) are mutially exclusive; odd things may happen if more than 
	one is supplied. 
&space
&term -e : Export the source. Any changes made to the build directory contents cannot 
	be replaced in the repository.  This is the default if neither the -c or -u  options are not supplied.
&space
&term -u : Update the source that is already checked out (brings in changes that were checked	
	in by others).
&space
&term 	-p prefix : Tag prefix to use. If not supplied, then &lit(ningaui) is used. 
	Ignored unless -T is also supplied.
&space
&term	-i : Ignore errors.  If multiple targets are specified on the command line, &this 
	will continue to try to build all of them regardless of the state of the previous. If
	this is not set, then &this will exit with a bad return code after the first target 
	fails.
&space
&term	-n : No execute mode; lists what it can about what it would do.
&space
&term	-N : No fetch mode. No source is fetched.
&space
&term	-r dir : Specifies the subdirectory to the build directory (e.g. ningaui/potoroo) which is 
	provided on the &lit(ng_sm_fetch) command and becomes the working directory before &lit(nmake) 
	is invoked.  Any directory specifications added to commandline targets are assumed relative to 
	this directory.
&space
&term	-T num : Source will be fetched using the specific tag number supplied (rather
	than the current tag).  If the prefix is overridden with the -p option, the 
	tag used for the fetch will be:  &ital(prefix_source_object_num).  Otherwise, the 
	tag used will have the syntax &lit(kgb_)&ital(source-object_num).
	If no source-object name is supplied on the command line, &lit(ningaui) is 
	used (the whold tree)
&endterms

&space
&parms	&This expects targets, with optional directory paths, to be supplied on the command line.
	Each target is passed to &lit(nmake) in the order specified on the command line. Directory 
	pathnames are relative to the repository (e.g. ningaui/potoroo/pinkpages is supplied if 
	we are just building this small bit of potoroo).  Care should be taken when fetching 
	small amounts of the repository as it may not be possible to linkedit the desired source. 

&space
&exit	This script will exit with  a non-zero return code when it notices an error.

&space
&examp
&litblkb
   # export and build everything
   ng_sm_build precompile all install  

   # export just potoroo, prcompile everything then build just repmgr 
   ng_sm_build -r ningaui/potoroo precompile repmgr:all	

   # export everything, precompile, then build just feeds/topo
   ng_sm_build precompile feeds/topology:all
&litblke

&space
&see	ng_sm_autobuild, ng_sm_mkpkg, ng_sm_replace, ng_sm_tag, ng_sm_stat, ng_sm_fetch, ng_sm_new

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Dec 2004 (sd) : Fresh from the oven.
&term	06 Dec 2004 (sd) : Augment path and CC var for bloody sun.
&term	09 Dec 2004 (sd) : Output from fetch now sent to log. Updated man page.
&term	17 Jan 2005 (sd) : Corrected man page. 
&term	22 Jul 2005 (sd) : Converted to use ng_make
&term	21 Jan 2006 (sd) : Added more info to verbose fetch message (date/opts).
&term	18 Mar 2008 (sd) : Fetc info is written to the log after truncating the log rather than allowing
		it to grow without bound. 
&term	08 Apr 2008 (sd) : Added a specific check to find gcc and flips to cc if gcc is not found. (support
		for sun-i386).
&endterms

&scd_end
------------------------------------------------------------------ */
