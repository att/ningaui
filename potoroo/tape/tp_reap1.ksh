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
ustring="[-v] [-d domain] bucket"
vtopts=''
domain=$FLOCK_default_tapedomain
tmp=$TMP/reap1_$$
me=`ng_sysname`
filer=${srv_Filer:-no-filer}
logf=$TMP/log/reap1_$$

while [[ $1 = -* ]]
do
	case $1 in
		-d)	tpopts="$tpopts -d $2"; shift;;
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

push(){
	ng_tpreq $tpopts pooladd $3 $1
	ng_tpreq $tpopts pooldel $2 $1
}

resched(){
	# ensure that the deleted files go unstable
	sleep 10	# this is probably unnecessary; don't want following request to go in same munge cycle
	ng_rmreq -a -s $filer 'comment waiting one cycle from reap1'
	ng_wreq "job: TAPE TAPEnst0 cmd ng_tp_reap1 $tpopts $1 $2"
}

exec > $logf 2>&1

mine="pf-tape-$1."
ng_rm_db -p | gre -v -F '+' | gre -F "/$mine" > $tmp.1
sed 's:.*/:z:; s/ .*//; s/z/dump1 /' < $tmp.1 > $tmp.2
# make sure we get only the ones we want
gre -v 'noccur=0' < $tmp.1 | sed 's+^\([^ ]*\) .*/\([^ ]*\) .*+file \2: md5=\1+' > $tmp.3
# and get in alpha order so tp_dumps run in order (more especially, together)
ng_rmreq -a -s $filer < $tmp.2 | gre -F -f $tmp.3 | gre "stable=1.*#$me" |
	sed "s/.*#$me //; s/#.*//; s+.*/\(.*\)+\1 &+" | sort | sed 's/.* //' > $tmp.4
wc $tmp.4
if [[ ! -s $tmp.4 ]]
then
	echo "no $mine files to dump"
	cleanup 0
fi

# we gotta do work!
volid=`ng_tpreq $tpopts pooldump $1 | ng_prcol 2`
if [[ -z "$volid" ]]
then
	echo "no volid available for pool $1"
	ng_log $PRI_ALERT $argv0  "@NG_TP_MAIL no volid available for pool $1"
	cleanup 1
fi
ng_tp_loadvol $tpopts $volid:$2 | read x dev tid
if [[ -z "$tid" ]]
then
	echo "failed to load $volid"
	cleanup 1
fi
> $tmp.5
echo "writing to $volid on $dev"
xargs echo ng_wrseg -f $tmp.5 -s -i $tid $volid $dev < $tmp.4 | sed 1q | read cmd
$cmd
ret=$?
# before we analyse teh error return, process teh files we wrote
sed 's/.*/file & 0/' < $tmp.5 | ng_rmreq -a -s $filer	# synchronously; so its done before we rerun
ng_log $PRI_INFO $argv0 "TAPE_REAP_SET_0BEGIN $volid"
sed "s/.*/ng_log $PRI_INFO 'TAPE_REAP_SET_1WRITE $volid &'/" < $tmp.5 | ksh
ng_log $PRI_INFO "TAPE_REAP_SET_2END $volid $SECONDS"
case $ret in
0)	;;
3)	;;	# unable to open a file -- pretty harmless
4)	echo "unknown tape error"
	push $volid $1 ERRORS
	ng_tpreq $tpopts pooladd ERRORS $volid
	;;
5)	case $TAPE_REAP_ACT in
	VET)
		echo "$volid is complete; sheduling vet"
		ng_tpreq $tpopts pooladd TOVET $volid:$TAPE_REAP_VET_DTE
		ng_tpreq $tpopts pooldel $1 $volid
		ng_log $PRI_ALERT $argv0 "@NG_TP_MAIL reap1($1) finished $volid and scheduled it for vet; logfile is $logf on $me"
		;;
	*)
		echo "$volid is complete; sheduling dup"
		push $volid $1 TODUP
		ng_tpreq $tpopts pooladd TODUP $volid
		ng_log $PRI_ALERT $argv0 "@NG_TP_MAIL reap1($1) finished $volid and scheduled it for dup; logfile is $logf on $me"
		;;
	esac
	;;
*)	echo "unknown errors"
	cleanup 1
	;;
esac

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_reap1:Put files to tape)

&synop  ng_tp_reap1 [-v] [-d domain] pool tapeid

&desc	&This processes cpio files for &ital(pool). It does so by writing them to
	tape (using the designated tape drive), and deleting them through &ital(repmgr).
	It will only write one tape.
&space
	The volid used is the first one in the list for the &ital(pool) as returned by
	&cw(ng_tpreq pooldump) &ital(pool). On error, this volid is deleted from teh pool,
	and added to either &cw(ERRORS) (if &ital(ng_wrseg) returned 4) or &cw(TODUP)
	(if &ital(ng_wrseg) returned 5, which means end of tape).

&subcat Alerts and Mail
	Under certain conditions, &this causes alert messages to be  written  to the 
	master log. These alert messages will cause eMail to be delivered to any user
	listed on the &lit(NG_TAPE_MAIL) pinkpages variable. The message will be a 
	single line of information with a reference to a detail file usually in the 
	&lit($TMP) directory.
	
&opts   &This understands the following options:

&begterms
&term 	-d domain : specify the tape changer domain. By default, it uses &cw($FLOCK_default_tapedomain).

&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	The program will return with a code of zero, unless errors are detected
	(in which case it exits with a code of one).

&see	ng_tp_changer

&mods	
&owner(Andrew Hume)
&lgterms
&term	may 2007 (ah) : Birth.
&term	26 Mar 2007 (sd) : Added support to send alert messages instead of mail.
&endterms

&scd_end
------------------------------------------------------------------ */
