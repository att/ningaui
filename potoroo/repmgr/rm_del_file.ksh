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
ustring='file md5'
while [[ $1 = -* ]]
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
	esac	

	shift 
done	

echo ${2:-bogus} ${1:-bogus} | ng_rm_db -f | read junk path junk

if [[ -z $path ]]		# no path get out now else rm fails
then
	cleanup 0
fi

rm -f $path && ng_flist_del $1 $2 "$3"

cleanup 0
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_del_file:Remove a file from the local repmgr database)

&space
&synop  ng_rm_del_file file md5 


&space
&desc   &This causes the named file and md5 pair to be removed from the local 
	replication manager database (rm_db).  This script does &stress(NOT) 
	make any attempt to alter the file registration information kept
	 by replication manager itself as it is assumed that the script 
	has been invoked by replication manager, or for the express purpose
	of managing the local database. 

&exit
	This script always returns 0.

&space
&see    ng_repmgr, ng_repmgrbt, ng_rm_db, ng_rm_dbd, ng_rm_register, ng_rm_unreg
&owner(Andrew Hume)
&lgterms
&term	29 Nov 2005 (sd) : Check for empty path from rm_db before allowing a 
	call to rm. 
&term	01 Aug 2006 (sd) : Added --man and cleaned up option parsing.
&endterms
&scd_end

