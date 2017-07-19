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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
#
_cleanup_dirs=$TMP/repmgr
ustring='old-file md5sum new-file'

while [[ $1 = -* ]]			# while "first" parm has an option flag (-)
do
	case $1 in 
		--man)	show_man; cleanup 99;;
		-v)	verbose=1; verb=-v;;
		-\?)	usage; cleanup 1;;
		*)
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac				# end switch on next option character

	shift 
done					# end for each option character

echo $2 $1 | ng_rm_db -f | read junk path junk

filer=${srv_Filer:-no-filer}

if [[ -n $path ]]
then
	new=`dirname $path`/$3
	mv $path $new && echo $2 $path $new | ng_rm_db -r 2>$TMP/repmgr/rm_rename.$$ 1>&2
	# NOTE: newline in next command IS important
	ng_rmreq -s $filer "uninstance `ng_sysname` $2 $path
instance `ng_sysname` $2 $new"
else
	ng_log $WARN "unable to find file to move: $1 $2" 
	cleanup 1
fi

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_rename_file:Rename a file managed by replication manager)

&space
&synop  ng_rm_rename_file oldname md5 newname


&space
&desc   &This will rename the file from the &ital(oldname) to the &ital(newname) 
	on the current node, adjust the local replication manager (rm_db) database,
	and will send the necessary commands to replication manager to indicate
	the change in name. 

&exit
	An exit code of 0 indicates that &this was able to accomplish the task
	without error. A non-zero return code indicates a failure of somesort. 
	Failures are generally accompanied with an explaination written to 
	stderr.

&space
&examp  ng_rm_rename_file pf-abs-list+0 9876876876fea7a0897a0987 pf-abs-list

&space
&see    ng_repmgr, ng_rm_db, ng_rm_dbd, ng_rm_register, ng_rm_unreg

&mods
&owner(Andrew Hume)
&lgterms
&term	02 Mar 2003 (sd) : added check to prevent move if not found in local db.
&term	15 Nov 2004 (sd) :  trashed xx.$$.xx filenames.
&term	06 Apr 2006 (sd) : Corrected man page.
&term	01 Aug 1005 (sd) : Fixed options loop to include --man.
&endterms
&scd_end

