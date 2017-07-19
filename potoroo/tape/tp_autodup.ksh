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
ustring="[-v] medium"
vtopts=''
logf=$TMP/log/autodup_$$
dosched=0

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
	# cycle thru tail(pooldump TODUMP) sched dups
	ng_tpreq pooldump TODUP | read junk x
	while [[ ! -z "$x" ]]
	do
		echo "$x" | read junk x
		ng_wreq "job: TAPE cmd ng_tp_autodup $junk"
		#printf 'scheduling autodup for %s on %s (information only)\n' $junk $me | mail andrew
		ng_log $PRI_ALERT $argv0 "@NG_TP_MAIL scheduling autodup for $junk on $me (information only)"
	done
	cleanup 0
fi

if [[ $# -ne 1 ]]
then
	usage
	cleanup 1
fi

me=`ng_sysname`

push(){
	ng_tpreq $tpopts pooladd $4 $3
	ng_tpreq $tpopts pooldel $2 $1
}

volid=${1%:*}
# just doublecheck we still need to vet
case `ng_tpreq pooldump TODUP` in
*$volid*)	;;
*)	echo "$volid not in the TODUP pool"
	exit 2
	;;
esac

ng_tp_loadvol $1:$TAPE_DUP_DTE | read sys dev tid
case $sys in
$me)	;;		# continue on
?*)	ng_wreq -s $sys "job: TAPE TAPE_${dev##*/} cmd ng_auto_vet $1"
	exit 0
	;;
*)	echo 'ng_tp_loadvol errors; exiting' 1>&2
	exit 1
	;;
esac

set -e
set -x
x=`ng_ppget TAPE_SCRATCH_DIR_${dev##*/}`
if [[ -z "$x" ]]
then
	echo "no scratch dir for $dev"
	echo "no scratch dir for $dev" | mail andrew
	cleanup 1
fi
cd $x
ng_tp_vet -c $dev
while [[ "`echo *.cpio`" != "*.cpio" ]]
do
	date
	volid=`ng_tpreq $tpopts pooldump ARCHIVE | ng_prcol 2`
	if [[ -z "$volid" ]]
	then
		echo "no volid available for pool ARCHIVE"
		echo "no volid available for pool ARCHIVE" | mail andrew
		cleanup 1
	fi
	ng_tp_loadvol $tpopts $volid:$tid | read x dev tid
	if [[ -z "$tid" ]]
	then
		echo "failed to load $volid"
		cleanup 1
	fi
	set +e
	ng_wrseg -f junk -s -i $tid $volid $dev *.cpio
	ret=$?
	date; wc junk
	set -e
	ng_prcol 2 < junk | xargs rm -f; rm junk
	case $ret in
	0)	continue;;
	4)	echo "unknown tape error"
		push $volid TODUP $volid ERRORS
		;;
	5)	echo "$volid is complete"
		;;
	*)	echo "unknown errors"
		cleanup 1
		;;
	esac
	ng_tpreq pooldel ARCHIVE $volid
	ng_tpreq pooladd TOVET $volid:`ng_ppget TAPE_VET_DTE`
done
ng_tpreq pooldel TODUP $1
echo "successfully duped ${1%:*}" | mail andrew bsr
cleanup 0

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
&term	22 aug 2005 (ah) : Birth.
&term	17 May 2006 (sd) : Remove assumption that NG_ROOT is /ningaui if not defined. 
&term	26 Mar 2007 (sd) : Added support to send alert messages instead of mail.
&endterms

&scd_end
------------------------------------------------------------------ */
