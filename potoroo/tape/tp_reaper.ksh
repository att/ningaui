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
ustring="[-v] [-d domain] [req ...]"
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

#exec > $logf 2>&1
load=`uptime | sed 's/.*average://' | ng_prcol 2`
if [[ $load -gt 2 ]]
then
	echo "load too high: $load"
	cleanup 0
fi
# now get tapeid
awk '$1=="tape" { print $2 }' < $NG_ROOT/site/config | sed 1q | read dev
if [[ -z "dev" ]]
then
	echo "no tape device in $NG_ROOT/site/config"
	cleanup 0
fi
tid=`ng_tp_getid $dev`
if [[ -z "tid" ]]
then
	echo "ng_tp_getid failed for $dev; weird!"
	cleanup 1
fi
mine="pf-tape-"
ng_rm_db -p | gre -v -F '+' | gre -F "/$mine" > $tmp.1
sed "s/.*$mine//; s/[.].*//" < $tmp.1 | sort | uniq -c | sort -nr > $tmp.2
echo "doing tape $tid, 5min load is $load"
ng_prcol 2 < $tmp.2 | while read feed
do
	ng_wreq "job: TAPE TAPEnst0 cmd ng_tp_reap1 $tpopts $feed $tid"
	echo scheduling $feed
	sleep 5
done
cleanup 0


exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_reaper:Schedule putting files to tape)

&synop  ng_tp_reaper [-v] [-d domain]

&desc	&This examines current &cw(pf-tape-) cpio files and schedules &ital(ng_tp_reap1)
	jobs for each pool of cpio file. Scheduling only occurs if the 5 minutes load average
	(as reported by &ital(uptime)) is less than 2.
	
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
&endterms

&scd_end
------------------------------------------------------------------ */
