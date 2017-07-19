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
# Mnemonic:	tp_getid
# Abstract:	return the device id string for the device on the command line
#		
# Date:		8 Aug 2006  -- conversion from Andrew's original script
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="device-name"
while [[ $1 = -* ]]
do
	case $1 in 
		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ $# != 1 || $1 != /dev/* ]]
then
	usage
	cleanup 1
fi

eval id=\"\$TAPE_DEVID_${1##*/}\"

rc=0
if [[ -z $id ]]				# var is not set; decide if it should be
then
	if gre "tape.*$1|library.*$1" $NG_ROOT/site/config >/dev/null			# device is defined	
	then
		verbose "variable not set in pinkpages, attempting to fetch from device"
		id="$( ng_tp_setid $vflag -e $1 )"
	else
		log_msg "device $1 is not configured as a tape or changer on this node"
		rc=1
	fi	
fi

echo ${id:-none}

cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_getid:Get tape/changer id for device)

&space
&synop	ng_tp_getid device-name

&space
&desc	&This maps the device name supplied on the command line (e.g. /dev/nst0) to 
	the hardware ID that is associated with the device. The id is maintained in 
	a pinkpages variable local to the node to allow the id to be fetched when the 
	device is in use.  If the id is not currently in a pinkpages variable, an 
	attempt to fetch it from the device at the time of the call will be made. 
	The device ID will be written to standard output. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : verbose mode. 
&endterms


&space
&parms &This expects the device name to be the only command line parameter. 

&space
&exit	An exit code of 1 indicates that a parameter was missing, or that the device 
	is not configured as a tape or tape changer on the current node. A return code
	of zero indicates success. 

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	xx MMM 2006 (sd) : Its beginning of time. 


&scd_end
------------------------------------------------------------------ */



