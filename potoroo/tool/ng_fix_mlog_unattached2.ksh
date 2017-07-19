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

# Mnemonic:     ng_fix_mlog_unattached2
# Abstract:     Register unattached mlog and masterlog instances (part 2)
#		(fix_mlog_unattached schedules one of these for each unattached
#		file that it finds)
#
# Date:         26 June 2006
# Author:       Bethany Robinson

# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# --------------------------------------------------------------------

set -o pipefail

ustring="file md5sum size"
verbose=0
mlprefix=masterlog
frag_prefix=mlog
force=0
list=""

while [[ $1 = -* ]]
do
        case $1 in
		-f)	force=1;;
		-l)	list=$2; shift;;
		-P)	frag_prefix=$2; shift;;
		-p)	mlprefix=$2; shift;;
                --man)  show_man; cleanup 1;;
		-v)     verbose=1; vflag="-v";;
                -\?)    usage; cleanup 1;;
                -*)     error_msg "unrecognised option: $1"
                        usage;
                        cleanup 1
                        ;;
        esac

        shift
done

file=$1
checksum=$2
size=$3
mynode=$(ng_sysname)

# only register files older than a day or thereabouts
file2=`find $file -mtime +1 -print`
if [[ "$file" == "$file2" ]] || (( $force > 0 ))
then
	verbose recycling $file $checksum $size
	ng_log $PRI_INFO $0 "recycling $file $checksum"

	typeset -u hood="${mlprefix}_"${file##*.}		# convert to upper case (repmgr requirement)

	if [[ ${file##*/} == "$frag_prefix"* ]]
	then
		verbose "fragment registered: $hood(hood) $checksum(md5) $file"
        	ng_rm_register $vflag -H $hood -C $checksum $file
	else
		if [[ ${file##*/} == "$mlprefix"* ]]
		then
			dir="${file%/*}"			# get directory path
			base="${file##*/}"			# basename
			id="$(ng_dt -i)$$"			# a unique id; must assume concurrent tasks on same node
			#new=$dir/mlog.$base.$checksum  
			new=$dir/mlog.${mynode}_orphan.$id.${base##*.}	# convert to fragment name syntax: prefix.node.id.date
			
			verbose  "masterlog orphan registered as fragment: $file(old) $new(new)"
			if mv $file $new 
			then
				if ng_rm_register $vflag -H $hood $new
				then
					ng_rm_del_file $file $checksum
					ng_log $PRI_INFO $argv0 "masterlog orphan registered as fragment: $file(old) $new(new)"
				fi
			fi
				
		fi
	fi
else
	verbose "not recycling $file $checksum $size; not old enough"
fi

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_fix_mlog_unattached2 - register unattached mlog and masterlog instances. 
&name &utitle

&space
&synop  ng_fix_mlog_unattached2 file md5sum size

&space
&desc   &This is given an unattached mlog or masterlog instance
	to register.  It checks to make sure the file is older than a day.  If so, it registers 
	the mlog instances.  For masterlogs, it renames them such that they appear to be fragment
	files (they are already split), and thus are gathered up by the next &lit(ng_log_comb)
	execution.

&space
&see   ng_fix_mlog_unattached, ng_log_frag, ng_log_comb, ng_logd 
&space
&owner(Bethany Robinson)
&lgterms
&term	01 Feb 2007 (sd) : Made changes to support the distributed log_comb process.  Fragment files
	have different naming syntax, and things are hooded based on the masterlog date and not 
	blindly to the Logger node.
&endterms
&scd_end
