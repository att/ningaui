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
# Mnemonic:	sm_del - delete a module and remove from repository
#		nor do the files need to exist.
#		
# Date:		20 DEc 2004
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

forreal=""
verbose=0
cmd=remove
local="-l"			# we force local on for this
ustring="[-m message] [-n] src-object"
msg="deleted on `ng_dt -c` by $LOGNAME  from `ng_sysname`"

while [[ $1 = -* ]]
do
	case $1 in 
		-m)	msg="$msg: $2"; shift;;

		-n)	forreal="echo would execute:";;

		-r)	local="";;				# recurse
		
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

if [[ -z $1 ]]
then
	usage
	cleanup 1
fi

verbose "deleting: $@"
for f in "$@"
do
	if [[ -f $f ]]
	then
		$forreal mv $f $f.deleted		# lets them get it back if a booboo
	fi
done

$forreal cvs -d $cvsroot $cmd $local "$@"

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_del:Source Management delete source module) 

&space
&synop	ng_sm_del file [file...]

&space
&desc	&This will delete the file(s) named on the command line from the repository.  
	If the files exist in the current directory, they will be removed from the 
	current directory first.

&space
&opts	The following options are recognised when placed on the command line:
&begterms
&term	-m msg : A message to be associated with the file(s). If not supplied, 
	a dummy string is passed to the repository manager. 
&space
&term	-n : No execute mode.
&space
&term	-r : Recursive mode. By default, the local (-l) option is on preventing 
	recursion.
&space
&term	-v : Verbose mode. 
&endterms

&space
&parms	This script allows one or more files to be passed on the command line. The 
	files do not need to exist until an ng_sm_replace (checkin) command is executed
	for the current, or parent, directory.

&space
&exit	The return code from this script is the return code generated by the 
	interface to the repository.

&space
&see	ng_sm_fetch, ng_sm_replace, ng_sm_stat, ng_sm_tag, ng_sm_new

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Dec 2004 (sd) : Fresh from the oven.
&term	27 Feb 2005 (sd) : No longer tries to send -m on the command (not supported in CVS).
&term	13 Jan 2009 (sd) : Corrected the man page.
&endterms

&scd_end
------------------------------------------------------------------ */
