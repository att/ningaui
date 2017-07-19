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
# Mnemonic:	ng_sm_tag -- tag the source in the managed repository, or 
#		report the current tag (default)
#		
# Date:		24 Nov 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
hold_src=$NG_SRC				# possible trash in cartulary
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

#	write $2 if in verbose mode, else write $1
NG_SRC=$hold_src

function natter 
{
	if [[ $verbose -gt 0 ]]
	then
		log_msg "$2"
	else
		echo "$1"
	fi
}

function usage		# overrides stdfun
{
	echo "usage:"
	echo "	$argv0 [-n] [-s date] [-p prefix] [-v] [source-object]"
	echo "	$argv0 -B branch-name [-n] [-v] source-object"
}

# ------------------------------------------------------------------
NG_CVSHOST=${NG_CVSHOST:-cvs-host-not-defined}
export USER=${USER:-$LOGNAME}			 # bloody suns dont have user
cvsroot=${CVSROOT:-:ext:$USER@$NG_CVSHOST:/cvsroot}
export CVS_RSH=${CVS_RSH:-ssh}

date=""
tag_name=""
forreal=""
set=0
inquire=1
default_prefix="kgb_ningaui"		# if no component given: assume all of tree
verbose=0
branch=""
list=0

while [[ $1 = -* ]]
do
	case $1 in 
		-B)	branch="${2:-junk}"; shift;;		# set a branch rather than tag; requires component on cmd line
		-l)	set=0; list=1;;
		-n)	forreal="echo would execute:";;
		-p) 	prefix="$2"; shift;;
		-s)	set=1; date="$2"; shift;;
		
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


if [[ -n $branch ]]
then
	if [[ -z $1 ]]
	then
		log_msg "ERROR: must enter component (e.g. ningaui/potoroo/tool) on command line when specifying a branch"
		cleanup 1
	fi
	what=$1
	
	bname="bch_${branch}_${what##*/}"
	verbose "establishing branch ($bname) for $what"
	$forreal cvs -d $cvsroot rtag -b -f ${bname:-barf} ${what:-nosuchthing}
	cleanup $?
fi


		# assume tag to be set if we get here
if [[ -n $1 ]] 			# component given
then
	what=${1}
	default_prefix="kgb_${what##*/}"		# use last dir name in tag
fi

prefix="${prefix:-$default_prefix}_"	# if not overridden	 -- will have date appended
now=`ng_dt -d`

if [[ $set -gt 0 ]]
then
	tag_id="${date// /}"		# make the id based on the date used in the cvs command (nice to match)
	tag_id="${tag_id//:/}"		# assume user entered date in yyyymmdd hh:mm  format
	if [[ ${#tag_id} -ne 12 ]]
	then
		log_msg "date supplied on -s option might be wrong. must be: yyyymmdd hh:mm"
		cleanup 1
	fi
else
	tag_id="<date>"
fi

if [[ -z $tag_name ]]			# get current tag name
then
	#T 2004-11-23 20:18 +0000 daniels ningaui [kgb_ningaui_00:2004.11.23.05.00.00]
	verbose "fetching tag for prefix; $prefix"
	cvs -d $cvsroot history -a -T |  awk -v list=$list -v prefix="$prefix" '
		BEGIN { max = 0; }

		index( $0, prefix ) {
			if( list )
				printf( "%s\n", $0 ) >"/dev/fd/2";
			n = substr( $NF, length( prefix )+2 )+0;		yyyymmddhhmm
			if( n > max )
				max = n;
		}
		END { 
			if( ! list )
				printf( "%.0f\n", max ); 
		}
	' | read cur_n

	next_tag_name="$prefix$tag_id"			# next tag name
	cur_tag_name="$prefix${cur_n:-0}"		# current tag name
fi


if [[ $set -gt 0 ]]			# update the repository based on the date
then
	if [[ -z $date  ]]
	then
		echo "must enter -d date "
		exit 1
	fi

	verbose "next tag for ${what:-ningaui} will be: $next_tag_name and is tagging all modules prior to $date"
	$forreal cvs -d $cvsroot rtag -f -D "$date" ${next_tag_name:-barf} ${what:-ningaui}
else
	if [[ $list -lt 1 ]]
	then
		natter "$cur_tag_name" "tag info for $what: current=$cur_tag_name  next=$next_tag_name"
	fi
fi


cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_tag:Tag source in the repository, or report current tag)

&space
&synop	ng_sm_tag [-p prefix] [-l | -s date] [-n] [-v] [source-object]
&break
	ng_sm_tag -B branchname [-n] [-v] source-object

&space
&desc	&This will report the current revision mark, or tag, as it has 
	been defined in the repository. The current tag name is written 
	to the standard output device. 
	If the proper option is given, the &ital(source-object) is 
	tagged with the next value.
&space
	If &ital(source-object) is supplied on the command line, the 
	tag retrieved from, or applied to, the repository is based on 
	the object rather than the whole of the Ningaui source tree. 
&space
	For the most part, tags are applied to a state of the source tree
	based on the fact that extracting the source with a tag is known
	to generate a good build.  Tag names are created with the syntax:
	&lit(kgb_)&ital(object-name_n)
&space
Where:
&begterms
&term	kgb_ : Is a standard prefix (known good build).
&term	object-name : is the object name (e.g. potorooo); by default the 
	name &lit(ningaui) is applied to the whole source structure.
&term	n : Is the tag number.  The tag number is the current date in 
	the form of yyyymmddhhmm. 
&endterms
	
&space
	A portion of the tree can be branched with &this if the -B option is used.
	Once a branch is created, the &lit(ng_sm_fetch) script can be used to 
	fetch modules on the branch using the branch (-B) option of that script.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-B name : Creates the branch (name) for the source-object supplied on the command
	line.  When this option is supplied, the source-object &stress(MUST) be supplied 
	on the command line. 
&space
&term	-l : List. Causes all tags for the source-object to be listed.
&space
&term	-n : No execution mode; just indicates what it might actually do.
&space
&term	-p prefix : By default the tag applies to the whole of the ningaui soruce and 
	thus the prefix kgb_ningaui is used.  This option supplies an alternate tag. If
	a source-object is supplied on the command line, the prefix will be altered automatically
	based on the last component of the object path; this option should rarely be 
	needed. (See the examples.)
&space
&term	-s date : Set the tag. Source objects will be tagged based on the date 
	where the most recent version of the object, not newer than date, is tagged.
	&ital(Date) should be quoted on the command line and will consist of two tokens:
	the date (yyyymmdd), and the time (hh:mm).  
&endterms


&space
&parms	&This allows one positional parameter to be supplied on the command line.
	It is assumed to be a legitimate source-object in the repository that can 
	be tagged. If the object is a directory, the tag will be applied to all 
	members of the directory, and recursively to all subdirectories.


&space
&exit	The exit code from this script is the return value from the source management 
	environmnt.

&space
&examp
	The following are examples of how to use &this:
&space
&litblkb
   ng_sm_tag -s "20041123 00:00"	  # tag all of ningaui as of 11/23/04 (kgb_ningaui_n)
   ng_sm_tag -s "20041120 04:00" potoroo  # tag potoroo; tag will be kgb_potoroo_n 

   ng_sm_tag | read cur_t         # get the current tag int shell var
   ng_sm_tag potoroo | read pt    $ get the current potoroo tag
&litblke

&space
&see	ng_sm_fetch, ng_sm_replce, ng_sm_stat, ng_sm_new.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Nov 2004 (sd) : Fresh from the oven.
&term	30 Nov 2004 (sd) : Converted tag number to datestamp (yyyymmddhhmm).
&term	07 Dec 2004 (sd) : Added -B (branch) support.
&term	16 Dec 2004 (sd) : Added -l option.
&term	04 Jan 2005 (sd) : Tag id is now based on the date used to mark the code (supplied with -s)
	rather than the date/time that teh tag was generated. 
&endterms

&scd_end
------------------------------------------------------------------ */
