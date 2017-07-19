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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
ustring="[-v] volid:tid"
vtopts=''
logf=$TMP/log/autovet_$$
dosched=0
me=`ng_sysname`

while [[ $1 = -* ]]
do
	case $1 in
		--sched)	dosched=1;;
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

exec > $logf 2>&1

if [[ $dosched -gt 0 ]]
then
	# get my names via ng_tp_getid and then tpreq mapdev
	awk '$1=="tape" { print $2 }' < $NG_ROOT/site/config | sed 1q | read dev
	tid=`ng_tp_getid $dev`
	ng_tpreq mapdev $tid | read status n1 n2 junk
	mine="$n1|$n2"
	# cycle thru tail(pooldump TOVET) sched my ones
	ng_tpreq pooldump TOVET | read junk x
	while [[ ! -z "$x" ]]
	do
		echo "$x" | read junk x
		tid=${junk##*:}
		case $tid in
		$mine)
			ng_wreq "job: TAPE cmd ng_tp_autovet $junk"
			ng_log $PRI_ALERT $argv0 "@NG_TP_MAIL scheduling autovet for $junk on $me (fyi only)"
			#printf 'scheduling autovet for %s on %s (fyi only)\n' $junk $me | mail andrew
			;;
		esac
	done
	cleanup 0
fi

if [[ $# -ne 1 ]]
then
	usage
	cleanup 1
fi

volid=${1%:*}
# just doublecheck we still need to vet
case `ng_tpreq pooldump TOVET` in
*$volid*)	;;
*)	echo "$volid not in the TOVET pool"
	exit 2
	;;
esac

ng_tp_loadvol $1 | read sys dev
case $sys in
$me)	;;		# continue on
?*)	ng_wreq -s $sys "job: TAPE TAPE_${dev##*/} cmd ng_tp_autovet $1"
	exit 0
	;;
*)	echo 'ng_tp_loadvol errors; exiting' 1>&2
	exit 1
	;;
esac

if ng_tp_vet $dev
then
	ng_tpreq pooldel TOVET $1
	echo "successfully vetted $1 on $me; logfile $logf" | mail andrew bsr
	ng_log $PRI_INFO $0 "vetting $1 on $me:$dev succeeded (VETMSG)"
	cleanup 0
else
	ng_log $PRI_INFO $0 "vetting $1 on $me:$dev failed (VETMSG)"
	echo "Houston: we have a problem vetting $1 on $me; logfile $logf" | mail andrew
fi

cleanup 1

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_autovet:Automate vetting a tape)

&space
&synop	ng_tp_autovet [-v] --sched
&break
	ng_tp_autovet [-v] volid

&space
&desc	Primarily, &this handles the automation part of vetting a tape without human intervention.
	In this form, the specified volid is to be vetted in the specified drive.
	The drive may be specified by either a DTE number (typically 0-3), or the drive id
	(the output of &ital(ng_tp_getid), typically something like &cw(IBM_3580_822312).
	Tapes can be vetted in this way multiple times without ill effect.

&subcat Alerts and Mail
	Under certain conditions, &this causes alert messages to be  written  to the 
	master log. These alert messages will cause eMail to be delivered to any user
	listed on the &lit(NG_TAPE_MAIL) pinkpages variable. The message will be a 
	single line of information with a reference to a detail file usually in the 
	&lit($TMP) directory.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose.
&endterms

&space
&exit	&This should always exit with a good return code.

&space
&see	&ital(ng_tape_vet)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	22 Aug 2005 (ah) : Birth.
&term	17 May 2006 (sd) : Remove assumption that NG_ROOT is /ningaui if not defined. 
&term	26 Mar 2007 (sd) : Added support to send alert messages instead of mail.
&endterms

&scd_end
------------------------------------------------------------------ */
