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
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
ustring='[-v] flistfile'
NG_RM_NODES=${NG_RM_NODES:-$NG_ROOT/site/rm/nodes}

while [[ $1 == -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 1;;
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

case $# in
1)	# move it there first
	set -ex
	sed 1q $1 | read who junk		# in case they added garbage
	if [[ ! -z "$who" ]]
	then
		ng_install $1 $NG_RM_NODES/.$who && rm $1
		chmod 664 $NG_RM_NODES/.$who
		ng_rename $NG_RM_NODES/.$who $NG_RM_NODES/$who
		ng_rmreq "copylist $NG_RM_NODES/$who"
	fi
	;;
*)	usage;;
esac

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_rcv_flist:Move a repmgr filelist into the fold)

&space
&synop  ng_rm_rcv_filelist flistfile


&space
&desc   Filelists are sent to the node running replication manager and placed into 
	the &lit($TMP) directory.  Once the list has been transferred, &this is 
	scheduled to move the list into the replication manager work directory:
	&lit($NG_ROOT/site/rm/nodes).
	As the list is moved into the work directory it is given the name that matches
	the node sending the file (assumed to be the only token on the first line of the
	list). 

&space
&opts   &This accepts one argument: name of the file containing the 
	file list. For eth exact format, look at the code for &ital(ng_flist_send).

&exit
	This script will set the system exit code to one
	of the following values in order to indicate to the caller the final
        status of processing when the script exits.
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&endterms

&space
&examp  ng_rm_rcv /tmp/junk.26354

&space
&see    ng_flist_send, ng_repmgr, ng_repmgrbt
&owner(Andrew Hume)
&lgterms
&term	01 Aug 2006 (sd) : Corrected manual page and added --man option.
&endterms
&scd_end

