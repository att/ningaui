#!/ningaui/bin/ksh
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


#!/ningaui/bin/ksh
# Mnemonic:	rm_arena_list
# Abstract:	list all of the mount-points that are defined in NG_ROOT/site/arena
#		
# Date:		17 May 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-b] [-c] [-v]"
col_list=0
full=1

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	full=0;;
		-c)	col_list=1;;

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

if cd $NG_ROOT/site/arena
then
	ls | while read f
	do
		case $f in
		 	\.*)	;;				# skip unimportant files
			*)	if [[ ! -d $f ]]
				then
					read r <$f
					if (( $full > 0 ))
					then
						list="$list$r ";
					else
						list="$list${r##*/} ";
					fi
				fi
				;;
		esac
	done

	if (( $col_list > 0 ))
	then
		for x in $list
		do
			echo $x
		done
	else
		echo "$list"
	fi
else
	log_msg "unable to find $NG_ROOT/site/arena directory"
	cleanup 1
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_arena_list:List Repmgr Arena Filesystems)

&space
&synop	ng_rm_arena_list [-b] [-c] [-v]

&space
&desc	&This will list all of the filesystems on the node that are currently in use 
	by replication manager. 
	These are the fileystems that are listed by the &lit(NG_ROOT/site/arena) 
	directory.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Basename; lists only the basename of each filesystem (e.g. fs00).
&space
&term	-c : Column mode; lists filesystems one per line, rather than all on a single line. 
&endterms


&space
&exit	This script will exit with a zero return code if all is well. A non-zero return 
	indicates a failure of sorts.

&space
&see	ng_repmgr, ng_repmgrbt, ng_rmreq, ng_rm_btstats, ng_rm_btreq, ng_rm_db, ng_rm_dbd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	17 May 2007 (sd) : The start of the race. 
&endterms


&scd_end
------------------------------------------------------------------ */

