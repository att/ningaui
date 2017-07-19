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
# Mnemonic:	sm_fetch - fetch (export or checkout) from the source management system of choice
#		
# Abstract:	Will get things from the managed source repository.  Optionally it will
#		create the stdlib and include directories.
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

use_tag=1		# will export/checkout using the bleeding edge source unless this is set -- causes last kgb tag to be used
tag_name=""
forreal=""
default_prefix="kgb_ningaui"
verbose=0
cmd=checkout
setup=0
remain=0			# do not remain in cwd; cd to ng_src
force=0

ustring="{-e|-u|-U|-c} [-n] [-p prefix] [-b | -B branch-name | -d date | -T tag_number] [-n] [-r] [-v] [component]"

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cmd=checkout;;			# varuous command modifiers
		-e)	cmd=export;;
		-u)	cmd="update -d";;		# update with repository (all directories and new files too)
		-U)	cmd="update -A";;		# remove sticky tags all together

							# other options
		-b)	use_tag=0;;			# use the bleeding edge
		-B)	cvs_opts="-b"; branch="$2"; shift;;	# fetch from branch
		-d)	date="$2"; use_tag=0; shift;;	# extract/checkout as of date
		-f)	force=1;;
		-n)	forreal="echo would execute:";;
		-p) 	prefix="$2"; shift;;
		-r)	remain=1;;			# remain in the current directory
		-s)	setup=1;;
		-t)	use_tag=1;;			# default behaviour now 
		-T)	tag_no=$2; shift;;		# added to prefix
		
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

if [[ $NG_SRC = /ningaui/src ]]
then
	log_msg "NG_SRC seems to be wrong; it cannot be /ningaui/src!"
	exit 1
fi

wrkd=${NG_SRC%/*}
if [[ $remain -lt 1 ]]
then
	if ! cd $wrkd
	then
		log_msg "could not switch to the directory: $wrkd"
		cleanup 1
	fi
else
	if [[ $PWD = ${NG_SRC}* ]]		# present directory is inside -- maybe not good 
	then
		if [[ $force -lt 1 ]]
		then
			log_msg "present directory ($PWD) is inside of NG_SRC; possible remidies:"
			log_msg "	remove -r option (needed only if NG_SRC does not exist"
			log_msg "	set NG_SRC to proper value (currently: $NG_SRC)"
			log_msg "	use -f (force) option (if this is really desired)" 
			cleanup 1
		fi
	fi
fi

verbose "working directory: $PWD"

if [[ -n $1 ]] 			# component given
then
	what=${1}
	default_prefix="kgb_${what##*/}"
else
	what=ningaui			# default to whole ball of wax
fi

prefix="${prefix:-$default_prefix}"	# if not overridden

if [[ -n $branch ]]
then
	verbose "fetching against branch: $branch"
	tag_name="-r $branch"
else
	if [[ $use_tag -gt 0 ]]
	then
		if [[ -z $tag_no ]]
		then
			verbose "determining the current tag number for $what; you may need to enter your password"
			ng_sm_tag -p $prefix | read tag_name 
			tag_name="-r $tag_name"			# add the cvs option here 
		else
			verbose "using user supplied tag/branch info ($prefix$tag_no) for $what"
			tag_name="-r $prefix${tag_no}"			# must put -r for cvs command here
		fi
	else
		verbose "will fetch/update using the bleeding edge source"
		tag_name=""			# ensure nothing for bleeding edge
	fi
fi


if ! touch foo.$$
then
	log_msg "ERROR: cannot seem to write to this directory: $PWD"
	cleanup 1
fi
rm foo.$$

verbose "fetching for $what using the tag/branch ${tag_name:-none (bleeding edge)}; you may need to enter your password"
if [[ -z $date ]]
then
	$forreal cvs -d $cvsroot $cmd $tag_name ${what:-ningaui}
else
	$forreal cvs -d $cvsroot $cmd -D "$date" ${what:-ningaui}
fi

if [[ $setup -gt 0 ]]
then
	if cd ningaui
	then
		mkdir stlib 2>/dev/null
		mkdir include 2>/dev/null
		mkdir include/copybooks 2>/dev/null
		mkdir .bin 2>/dev/null
		verbose "setup created $what/stlib, $what/include and $what/.bin;"
		verbose "these are not source managed, but needed for building!"
	else
		log_msg "could not switch to $PWD/ningaui; setup directories failed"
	fi
fi

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_fetch:Source Management export or checkout source)

&space
&synop	ng_sm_fetch {-c | -u | -U | -e} [-p prefix] [-n] &break 
	[-s] [-b | -B branch-name | -d date | -T tag-num] [-v] [source-object]

&space
&desc	By default, &this will checkout the named &ital(source-object) from the 
	source management repository.  &This can also be used to export (-e) and 
	update (-u) source.  When source is exported it cannot be replaced 
	(checked back in) and is exported without source management information 
	(ng_sm_stats cannot be used in the directory after the files are written). 
&space
	Source that has previouly been fetched can be updated with &this when the -u
	option is given on the command line. The update process causes the 
	current working directory to be updated with newer information from the 
	repository.  
&space
	By default the last known good build (kgb) tag is used for checkout. 
	The -d (date) and -T (specific kgb tag id) can be used to fetch modules using
	a relative date or tag number.

	In general, tagged source cannot be checked back in using the 
	&lit(ng_sm) scripts.   The generic prefix &lit(ningaui) is used with the 
	tag number unless a different prefix is supplied.  This allows different 
	pieces of the repository (source components in Ningaui terms) to be tagged
	independantly of the tags applied to the whole repository.

&space
	To build all of ningaui, two directories must exist in the upper source
	directory: &lit(stlib) and &lit(include).  Neither of these directories are 
	managed in the source repository and are created if the setup flag (-s) is 
	set and the &ital(source-object) is not supplied (the whole tree is being 
	fetched).
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Check out the most recent version (bleeding edge) of the source-object.
&space
&term 	-c : Check out the source.
&space
&term	-d date : Check out/export source as of &ital(date).
	&ital(Date should be quoted and have the syntax of yyyymmdd hh^:mm. 
	This option and the two tag 
	options (-t and -T) are mutially exclusive; odd things may happen if more than 
	one is supplied. 
&space
&term -e : Export the source (does not allow check in).
&space
&term	-f : Force.  Needed if -r (remain in current directory) option is set &stress(and)
	the current directory is inside of the diredctory identified as &lit(NG_SRC).
&space
&term 	-p prefix : Tag prefix to use. If not supplied, then &lit(ningaui) is used. 
	Ignored unless -T is also supplied.
&space
&term	-n : No execute mode; lists actions without doing.
&space
&term	-r : Remain in the current working directory.  If not supplied, &this will change the 
	current working directory to NG_SRC before doing the fetch operation.
&space
&term	-s : If possible, and needed, setup the include and stable library directories after fetch.
&space
&term	-t : Fetch the source based on the current known good build (kgb) tag for the 
	source-object.  If a kgb tag for the object is not defined, unpredictable 
	results will occur.  (This option is now the default and is not necessary.)
&space
&term	-T num : Source will be fetched using the specific tag number supplied (rather
	than the current tag).  If the prefix is overridden with the -p option, the 
	tag used for the fetch will be:  &ital(prefix_source_object_num).  Otherwise, the 
	tag used will have the syntax &lit(kgb_)&ital(source-object_num).
	If no source-object name is supplied on the command line, &lit(ningaui) is 
	used (the whold tree)
&space
&term	-u : Update the source that is already checked out (brings in changes that were checked	
	in by others). It is not clear that specifying this option acutally works or causes 
	the underlying source manager to fetch all new code. 
&space
&term	-U : Update removing all tags.  Brings all modules in the scope of src-object up to the 
	bleeding edge. Should be used with care. 
&endterms

&space
&parms	&This allows a &lit(source-object) parameter to be supplied. This may be any 
	valid source-object that is known to the source management system. If nothing 
	is supplied, then &lit(ningaui) is assumed and the command is applied to 
	the enire repository.

&space
&exit	The exit value of this script is the return code from the source management 
	system.

&space
&examples
&litblkb
   ng_sm_fetch 	                       # get all of ningaui, bleeding edge, into cwd
   ng_sm_fetch -t ningaui              # get the last known good build src for ningaui
   ng_sm_fetch -t ningaui/potoroo      # get kgb src for potoroo

&litblke
	&space
	Assuming that source was fetched using the last known good tag, it can be 
	updated once a more recent tag is generated with:
&space
&litblkb
   ng_sm_fetch -u -t ningaui
&litlbke

&bugs
	Currently, the repository is accessed via &lit(ssh) and that will require 
	the user to put in her password multiple times during the fetch operation.
	Once when determining the current tag, and once when the fetch is executed.
&space
&see	ng_sm_replace, ng_sm_tag, ng_sm_stat

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Nov 2004 (sd) : Fresh from the oven.
&term	30 Nov 2004 (sd) : Added -b/d options; default behaviour is to check out as of 
	last kgb tag.
&term	01 Dec 2004 (sd) : Put in an error if NG_SRC is set to /ningaui/src.
&term	02 Dec 2004 (sd) : Now CDs to the right spot based on NG_SRC even if the last component 
	in the NG_SRC path is not right. Also fixed the date option to cvs.
&term	07 Dec 2004 (sd) : Added branch support.
&term	20 Dec 2004 (sd) : Prevents unload into NG_SRC with -r if -f is not set. (help avoid
	a tree within a tree. 
&term	18 Apr 2005 (sd) : Corrected documentation.
&term	23 Jan 2006 (sd) : Added -d option to update (-u) such that all directories and files should be 
		forced to be updated. 
&endterms

&scd_end
------------------------------------------------------------------ */
