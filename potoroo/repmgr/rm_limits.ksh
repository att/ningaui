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
# this is now done in site_prep and wstart and based on ncpus
echo "this script is DEPRECATED!" >&2
exit 1








#
# Mnemonic: ng_rm_limits
# Author:   Andrew Hume
# Date:     June 2003
#


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
                                   

#
# ------------------------------------------------------------------------
ustring=''
while [[ $1 == -* ]]
do
	case $1 in 
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

adm=$NG_ROOT/adm
. $adm/cartulary.local			# read in WOOMERA_RM_FS

echo WOOMERA_RM_RCV_FILE=$WOOMERA_RM_FS >> $adm/cartulary.local
echo WOOMERA_RM_SEND=$(($WOOMERA_RM_FS - 1)) >> $adm/cartulary.local
echo WOOMERA_RM_RENAME=4 >> $adm/cartulary.local
echo WOOMERA_RM_DELETE=4 >> $adm/cartulary.local

. $adm/cartulary.local			# read in what we set

ng_log $PRI_INFO $0 "set RM limits to RM_RCV_FILE=$WOOMERA_RM_RCV_FILE RM_SEND=$WOOMERA_RM_SEND RM_RENAME=$WOOMERA_RM_RENAME RM_DELETE=$WOOMERA_RM_DELETE"

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_rm_limits - Setup repmgr limits in the local cartulary
&name &utitle

&space
&synop  ng_rm_limits


&space
&desc   &ital(Ng_rm_limits) assumes &cw($RM_FS) has been set in the local cartulary
	and sets values for &cw(RM_RCV_FILE), &cw(RM_SEND), &cw(RM_RENAME) and &cw(RM_DELETE).

&space

&exit
	&ital(Ng_rm_limts) always returns zero.

&space
&see    ng_scan
&owner(Andrew Hume)
&scd_end

