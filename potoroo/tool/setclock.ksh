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
# Mnemonic:	setclock
# Abstract:	Sets the clock on the local node from a refrence host
#		
# Date:		15 April 2003 (revised)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
ustring=""

while [[ $1 = -* ]]
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

ng_leader | read leader
clock=`cat $NG_ROOT/adm/refclock`
uname -a | read systype junk

if [[ -z $clock ]]
then
	log_msg "cannot find hostname to sync to"
	cleanup 1
fi

err=0
ntpdate -q $clock | sed 1q | tr -d ',' | read x x x x x err x

case $err in				# abs
	-*)	err=$(( 0 - $err ));;
esac

if [[ $err -gt 1 ]]
then
	# if we're more than 1sec off, reset
	ntpdate -s -b -u $clock > /dev/null

	now=`date "+%D %T"`

	case $systype in
		Linux)		clock --s --date="$now" --utc;;		# force a hardware set
		FreeBsd)	;;
		Darwin)		;;
		*)		;;
	esac
fi

if [[ `hostname -s` = $leader ]]
then
	true
else
	ng_ccp -t $leader /dev/null > /dev/null 2>&1
fi

cleanup 0

exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_setclock:Check and adjust the time)

&space
&synop	ng_setclock

&space
&desc	&This will check and ajust the system time if necessary. The local time 
	is compared to the time on a reference host named in the file 
	$lit($NG_ROOT/adm/refclock).  If the time difference is more than a second, 
	the time is adjusted.
&space
	Under some flavours of UNIX (um, Linux particurlary) the hardware clock 
	must be forced in addition to adjusting the soft clock.  If it is necessary,
	this hardware adjustment is also done by this script.

&space
&see	ntpdate(8)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 Apr 2003 (sd) : Revision for BSD; added doc.
&endterms

&scd_end
------------------------------------------------------------------ */
