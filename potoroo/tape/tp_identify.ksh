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

# -------------------------------------------------------------------
ustring="[-v] volid device"
vtopts=''

while [[ $1 = -* ]]
do
	case $1 in
		-v)	verbose=1; 
			vflag="$vflag -v"
			;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

x=`ng_ppget CHANGER_ALL`
drive=`ng_tp_getid $2`
if [[ -n "$x" && -n $drive ]]
then
	for i in $x
	do
		ng_tpreq -d $i identify $1 $drive
	done
fi

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_identify:Identify volume on a drive)

&synop  ng_tp_identify volid device

&desc	&This sends &cw(identify) commands to all known tape changer daemons.
	This is part of teh self-discovery of teh tape changer DTE to device topology.
	
&opts   &This understands the following options:

&begterms
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	The program will return with a code of zero, unless errors are detected
	(in which case it exits with a code of one).

&see	ng_tp_changer

&mods	
&owner(Andrew Hume)
&lgterms
&term	nov 2006 (ah) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
