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
# Mnemonic:	ng_fixup.ksh
# Abstract:	fixes things after a refurb or node start:
#		+	all bin and lib directories searched for foo.<flock-name> files/dirs. When 
#			found, a symlink (foo) is set to point to it/them.
#		
#		This should be invoked by both site-prep and refurb because there are some 
#		packages that do not cause a node restart after refurb and things wouldn't be 
#		fixed until a node restart at some point in the future. 
#
#		The fix up of hash-bang things is still in refurb. It could be moved here, 
#		but until we need it ouside of refurb there's no need to do the work.
# Date:		14 Oceober 2009
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function point2flock
{
	typeset flock_name=${NG_NARBALEK_LEAGUE:-flock}
	typeset d=""
	typeset f=""
	typeset lname=""

	for d in $NG_ROOT/bin $NG_ROOT/lib $(ls -d $NG_ROOT/apps/*/bin $NG_ROOT/apps/*/lib 2>/dev/null)
	do
		for f in $(ls  -d $d/*.$flock_name 2>/dev/null)
		do
			lname=${f%.*}
			if [[ -L $lname ]]
			then
				rm $lname
			fi

			if ln -s $f $lname
			then
				verbose "created link: $lname -> $f"
			else
				log_msg "unable to create link: $lname -> $f [FAIL]"
				rc=1
			fi
		done
		
	done
}

# -------------------------------------------------------------------
while [[ $1 == -* ]]
do
	case $1 in 

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

rc=0
point2flock

cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_fixup:Fix/adjust things after a refurb/restart)

&space
&synop	ng_fixup [-v]

&space
&desc	&This makes adjustments following a refurb or node start. Currently, the adjustments made are
	to set a symbolic link to files in either the bin or lib directories (including application directories)
	where the files have the name syntax of foo.<flock-name>.  The symbolic link foo will point at the 
	flock specific file. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose mode.
&endterms


&space
&exit	Will exit with a non-zero return code if there was an error. 

&space
&see
	&seelink(tool:ng_node)
	&seelink(tool:ng_site_prep)
	&seelink(tool:ng_refurb)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	14 Oct 2009 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */

