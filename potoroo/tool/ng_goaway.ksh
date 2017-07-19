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
# Mnemonic:	ng_goaway
# Abstract:	checks for a goaway/nox file for the caller in a standard
#		place using a standard naming convention, or sets a 
#		goaway/nox for a tool.
# Date:		20 March 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# touch or remove goaway/nox file or set/clear goaway variable in pinkpages.
#
# WARNING: if $TMP is changed to someother directory, stdfun will need to be 
#	modified as it cannot use this script to test and thus has $TMP hardcoded
# -------------------------------------------------------------------------------
function reset_thing
{
	typeset base="$1$unique.$type"
	typeset val=""

	verbose "removing goaway: $TMP/$base"
	rm -f $TMP/$base

	if [[ -n $use_nar ]]
	then
		base=${base//./_}				# dots are not legit in pinkpages variables
		verbose "removing narbalek goaway: NG_GOAWAY_$base"
		case $use_nar in 
			a)
				ng_ppset -l NG_GOAWAY_$base=""		# clear all possible scope areas
				ng_ppset -f NG_GOAWAY_$base=""
				ng_ppset  NG_GOAWAY_$base=""
				;;

			l|f) 	ng_ppset -$use_nar NG_GOAWAY_$base="" ;;

			c)	ng_ppset NG_GOAWAY_$base="";;		# cluster is default

			*)	log_msg "invalid narbalek scope option entered with -N; must be a, l, f or c"
				cleanup 1
				;;
		esac
	fi
}

function set_thing
{
	typeset base="$1$unique.$type"

	if [[ -n $use_nar ]]
	then
		base=${base//./_}				# dots are not legit in pinkpages variables
		verbose "creating narbalek goaway: NG_GOAWAY_$base"
		case $use_nar in 
			l|f)
				ng_ppset -$use_nar NG_GOAWAY_$base="1"		# set for local or flock
				;;

			c)
				ng_ppset NG_GOAWAY_$base="1"			# set for cluster
				;;

			*)	log_msg "invalid narbalek scope option entered with -N; must be  l, f or c"
				cleanup 1
				;;
		esac
	else
		verbose "creating goaway: $TMP/$base"
		touch $TMP/$base
	fi
}

function list
{
	cd $TMP
	log_msg "goaway files in reverse creation order:"
	ls -alt *goaway 2>/dev/null
	echo "" >&2
	log_msg "no execution files in reverse creation order:"
	ls -alt *nox 2>/dev/null
	echo "" >&2
	log_msg "goaway/nox files in narbalek:"
	ng_ppget -L|gre GOAWAY
}

#------------------------------------------------------------------------------
ustring="[-c|r|s] [-N scope] [-u unique-suffix] [-X] daemon"
action=set
unique=""
type=goaway
use_nar="";			# use narbalek to manage the setting

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	action=check;;
		-l)	action=list;;
		-r)	action=reset;;
		-s)	action=set;;


		-N)	use_nar="$2"; shift;;

		-u)	unique=".$2"; shift;;

		-X)	type=nox;;			# no execute (no start)


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

if [[ $action = "list" ]]
then
	list
	cleanup 0
fi

if [[ $# -lt 1 ]]
then
	cleanup 1
fi

case $action in 
	check)				# check for a goaway/nox, return OK if its there
		target=${1##*/}		# strip any path sent in
		if [[ -f $TMP/$target$unique.$type ]]
		then
			cleanup 0		# return true (need to get out)
		fi

		eval val=\$\(ng_ppget -L NG_GOAWAY_$target${unique}_$type 2>/dev/null \)
		if (( ${val:-0} > 0 ))
		then
			cleanup 0
		fi

		cleanup 1
		;;

	reset)	
		while [[ -n $1 ]]
		do
			target=${1##*/}		# strip any path sent in
			reset_thing "$target" 		# (re)set for all things listed on command line
			shift
		done
		;;

	set)	
		while [[ -n $1 ]]
		do
			target=${1##*/}		# strip any path sent in
			set_thing "$target" 		# (re)set for all things listed on command line
			shift
		done
		;;

	
	*)	log_msg "invalid action: $action"
		cleanup 1
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_goaway:Set, reset, check daemon goaway file, or no execute file)

&space
&synop	ng_goaway -c [-u unique_str] [-N scope] daemon	.br
	ng_goaway {-r|-s} [-N scope] [-u unique_str] daemon [daemon2...] .br
	ng_goaway [-X [-r|-c]] daemon .br

&space
&desc	&This can be used to set, clear, or test for a daemon's go away 
	file or a goaway pinkpages variable.  
	When setting or clearing files, 
	multiple names may be supplied on the command line. When checking 
	for the presence of a goaway file, only one daemon name is expected.
	&This is designed such that a daemon may call this script passing the 
	value of &lit($argv0) and it can be used simply as illustrated in the 
	following code segment:
&litblkb

   if ng_goaway -c $argv0
   then
      ng_goaway -r $argv0   # reset the file
      cleanup 0
   fi
&litblke

&space
	When a daemon checks for a goaway file, it need &stress(not) supply the 
	-N option as both a goaway file and a pinkpages variable will be checked. 
	If either is set, then the result of the script will be true (exit code of 0).

&space
	Should multiple copies of the same daemon be running on the same node, 
	a unique identifier can be supplied (-u) which will allow this script 
	to signal the proper daemon.

&space 
&subcat	No Execution Flag
	The &lit(-X) command line option causes the file(s) created in &lit($TMP) to be
	no execution (.nox) files rather than goaway files.  These files are checked for 
	as scripts start (in stdfun) and if present cause the immdediate exit of the 
	script before execution.  To turn off a no execute file, the &lit(-r) option 
	is used in conjunction with the &lit(-X) option, and the check option (-c) 
	can be used to test for the file.
&space
	No execution pinkpages variables may be set using the -N option, however there 
	are limits to the ability of a script to check for these during its initialisation 
	process. The pinkpages no execution variable may be checked for only by 
	referencing the value as it was sourced from the cartulary; the initialisation 
	process cannot check narbalek/pinkpages directly as it might not be running
	(node start for instance). Thus, there will be a delay of up to 5 minutes after 
	setting a no execution pinkpages varialbe that it will actually take place on 
	the affected node(s).  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Check for the existance of a goaway/nox file for the daemon. If the file 
	exists, the script will return an ok (zero) value.
&space
&term	-r : Reset goaway/nox file(s). The goaway/nox files for all of the daemons listed on 
	the command line are removed. 
&space
&term	-s : Set. Causes the goaway/nox file to be created.  This is the default, but the 
	option is provided to allow easier programming of startup shutdown scripts.
&space
&term	-N scope : Causes a narbalek variable to be set/cleared using the scope supplied. 
	scope is one of l (local node), c (cluster), or f (flock). A scope value of 'a' may 
	be used when removing the goaway; all pinkpages values are cleared. 
&space
&term	-l : List all goaway and no execute files and pinkpages variables.  Files are listed 
	in the reverse order that they are created. 
&space
&term	-u string : Supplies a unique identifier that is added to the filename of 
	the goaway/nox file. This is necessary only if the same daemon is running multiple
	copies on the same host. 
&space
&term	-X : Set a no execute marker for the script(s) named on the command line. The 
	&lit(.nox) file is for each script is created (or removed if -r is supplied)
	in the &lit($TMP) directory.  
&endterms
&space
	If neither of the options is supplied, then the goaway/nox file for each daemon 
	listed on the command line is created. 

&space
&parms	Following any command line options, &this expects one or more daemon names. 
	The names may include a leading path specificaiton which allows the running 
	process to invoke this script passing it &lit($argv0) which may contain the 
	full or relative path of the command.

&space
&exit	When called to check for the goaway/nox files, a zero exit (OK) is returned if 
	the file exists. This allows the logic illustrated earlier to be utilised.
	A non-zero return code indicates that the file does not exist, or that the 
	set/reset operation(s) were not successful.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Mar 2003 (sd) : The beginning.
&term	11 Jul 2003 (sd) : To touch/remote file directly.  Files are created in $TMP
	which does not have the 'must be owned by ningaui' restriction as they did 
	when created in /tmp.
&term	10 Nov 2003 (sd) : Added support for no execute (.nox) files.
&term	06 Mar 2006 (sd) : Added support managing goaway information via pinkpages. 
&term	06 Apr 2006 (sd) : Now does a basename on the parameter passed in. 
&term	25 May 2006 (sd) : Fixed problem resetting multiple things on the command line. 
&term	16 Mar 2009 (sd) : Corrected typo in man page.
&term	23 Apr 2009 (sd) : Change to discard stderr from ppget -- it is ok not to find 
	a pp var set for the goaway information and the 'cannot find' error generated
	by ppget was confusing.
&endterms

&scd_end
------------------------------------------------------------------ */
