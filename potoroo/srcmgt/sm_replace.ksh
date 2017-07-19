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
# Mnemonic:	sm_replace - return a checked out thing to the repository
# Abstract:	
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

# ------------------------------------------------------------------
NG_CVSHOST=${NG_CVSHOST:-cvs-host-not-defined}
export USER=${USER:-$LOGNAME}			 # bloody suns dont have user
cvsroot=${CVSROOT:-:ext:$USER@$NG_CVSHOST:/cvsroot}
export CVS_RSH=${CVS_RSH:-ssh}

date=""
tag_name=""
forreal=""
default_prefix="kgb_ningaui"
verbose=0
cmd=commit
msg="commited on `ng_dt -c` by $LOGNAME  from `ng_sysname`"
ustring="-[n] [-p prefix] [-t tag_number] [-n] [-v] [component]"

while [[ $1 = -* ]]
do
	case $1 in 
		-n)	forreal="echo would execute:";;
		-r)	revision="-r $2"; shift;;
		
		-m)	msg="$2"; shift;;
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

if [[ -n $1 ]] 			# component given
then
	what="$@"
else				# nothing given, so do a 'new module' check
	ng_sm_stat -N | read new_list
	if [[ -n $new_list ]]
	then
		log_msg "WARNING: these might not have been added to CVS: $new_list"
	fi
fi

if [[ ! -d CVS ]]
then
	log_msg "this appears not to be a directory with checked out source"
	exit 1
fi

verbose "replacing modifications into $what; you may need to enter your password"
$forreal cvs  -d $cvsroot $cmd $revision -m "$msg"  ${what:-.}


cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_replace:Source Management replace source in repository -- commit)

&space
&synop	[-m message] [-n] [-v] [source-object]

&space
&desc	&This will replace &ital(source-object) into the source management repository.
	If &ital(source-object) is a directory name the directory is recursively 
	processed and all modified source in the directory, and subdirectories, are 
	replaced. To replace only one source-object, it must be explicitly named. If 
	no source-object is given, then the current directory is assumed. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-m msg : A message that is added by the repository to the 'comments' for the 
	replace command. If this option is not given, a message containing the date, time, 
	user, and system is sent to the repository manager. 
&space
&term	-n : No execute mode. The command(s) that would be executed to update the repository
	are written to the standard output device. 
&space
&term	-r version : Supply a revision number.  Generally of the form x.y. This is passed directly to 
	the source management system and errors, or strange behaviour might result if it is not 
	to its liking.
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms &This allows one or more object names to be supplied on the command line. If supplied, 
	these are passed to the source management system. If omitted, the current working 
	directory is assumed and passed to the underlying source management process.

&space
&exit	The return value from this script is the exit code that is returned by the 
	source management system. 

&space
&see	ng_sm_fetch, ng_sm_tag, ng_sm_stat, ng_sm_new

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Nov 2004 (sd) : Fresh from the oven.
&term	30 Nov 2004 (sd) : Defaults now to . if no parm given.
&term	15 Dec 2004 (sd) : Now passes all commandline parms to cvs.
&term	29 Apr 2004 (sd) : Added -r opttion.
&term	07 Feb 2006 (sd) : Added check for source not added to cvs if no options are given.
&endterms

&scd_end
------------------------------------------------------------------ */
