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
# Mnemonic:	rm_dbd_start
# Abstract:	starts the replication manager local database daemon (ng_rm_dbd) 
#		and keeps it running until we see a goaway file.  Running this 
#		script with -e will send an exit to dbd, and terminate the copy 
#		of this script that is watching dbd. 
#		
# Date:		10 April 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function start_it
{
	verbose "starting rm_db_tcp"
	if [[ ! -s $ckpt ]]
	then
		touch $ckpt			# must exist as empty if its not there
	fi

	>$ckpt.rollup.init				# start empty

	if [[ -s $ckpt.rollup ]]
	then 
		verbose "found non-empty rollup file; moving to *.init for processing"
		cat $ckpt.rollup >>$ckpt.rollup.init
		mv $ckpt.rollup $ckpt.rollup.used
		roll_opt="-r $ckpt.rollup.init"
	fi

	# this is not good as we do not bounce enough to make this work.  Original intent
	# was to catch faiures while we were down, but the reality was that we were catching 
	# stuff while we were up (tcp timeouts etc) and putting those in DAYS later is not good.
	# we still look for and move the failed file away for tracking purposes. 
	if [[ -s $ckpt.failed ]]			# xactions that had comm issues
	then
		verbose "found failed xaction file ($ckpt.failed) adding those to init"
		#cat $ckpt.failed >>$ckpt.rollup.init
		mv $ckpt.failed $ckpt.failed.used
		#roll_opt="-r $ckpt.rollup.init"
	fi

	now=$( ng_dt -i )
        if (( $now > $next_roll  ))             # roll the log only when last start more than 1hr ago
        then
                verbose "rolling logs"
                ng_roll_log rm_dbd
                next_roll=$(( $now + 36000 ))		# an hour in tenths 
        fi


	set -x
	ng_rm_dbd $roll_opt -c $ckpt -l $ckpt -p $port $v_opt >>$NG_ROOT/site/log/rm_dbd 2>&1 
	set +x

	log_msg "ng_rm_dbd finished rc=$?"
}


# -------------------------------------------------------------------

if ! [[ "$@" = *-e* || "$@" = *-f* || "$@" =  *-\?* || "$@" = *--man* ]]
then
	log_msg "detaching from tty; message directed to $NG_ROOT/site/log/rm_dbd_start"
	$detach
	exec >>$NG_ROOT/site/log/rm_dbd_start 2>&1
fi

next_roll=0;					# time afterwhich we will roll the log on restart
ckpt=$NG_ROOT/site/rm/rm_dbd.ckpt
port="${NG_RMDB_PORT:-3456}"
stop=0						# stop things if user enters -e
v_opt=""					# verbose options filtered through to dbd
force=0

while [[ $1 = -* ]]
do
	case $1 in 
		-F)	force=1;;		# forces start if chkpoint is not there
		-f)	;;			# foreground option -- ignore here
		-e)	stop=1;;
		-p)	port="$2"; shift;;

		-v)	verbose=1; v_opt="$v_opt-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


if (( $stop > 0 ))
then
	ng_goaway ${argv0##*/}
	ng_rm_2dbd -e -P $port
	verbose "ng_rm_dbd has been signaled to stop"
	cleanup 0
fi

if [[ ! -f $ckpt ]]			# prevent problems while we roll this out
then
	if (( $force > 0 ))
	then
		log_msg "WARNING: ckpt file did NOT exist; force is on so we are starting anyway"
		touch $ckpt
	else
		log_msg "ckpt file does not exist, to prevent problems ng_rm_dbd NOT started"
		ng_log $PRI_CRIT $argv0 "rm_dbd checkpoint file is missing, expected it in: $ckpt"
		cleanup 1
	fi
else
	verbose "found checkpoint: $ckpt"
fi

if [[ ! -s $ckpt ]]		# we allow a zero length ckpt ONLY if there is not an old one; no old one we assume this is a virgin system
then
	if (( $force > 0 )) || [[ ! -f $ckpt.old ]]
	then
		verbose "ckpt file was zero length, allowing dbd to start because force option was on, or assuming its a new node (ckpt.old missing)"
	else
		log_msg "ckpt file ($ckpt) is zero length, ng_rm_dbd NOT started"
		ng_log $PRI_CRIT $argvo "rm_dbd not started, checkpoint file is zero length: $ckpt"
		cleanup 1
	fi
fi

ng_goaway -r ${argv0##*/}
while true
do
	start_it

	if ng_goaway -c ${argv0##*/}
	then
		verbose "found goaway; exiting"
		cleanup 0
	fi

	log_msg "sleeping 30 before restart attempt"
	sleep 30 
done

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_dbd_start:Start the repmgr database daemon)

&space
&synop	ng_rm_dbd_start [-e ] [-f] [-p port] [-v]

&space
&desc	Start the replication manager database daemon. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	 -e : Cause the daemon to exit, and do not restart it.
&space
&term	-f : Run in foreground. 
&space
&term	-p port : Port that the daemon should use.  NG_RMDB_PORT is used if not supplied.
&space
&term	-v : Verbose modeterm	
&endterms

&space
&exit	A zero return code indicates success; all others failure.

&space
&see	ng_rm_dbd, ng_rm_2dbd, ng_repmgr 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 Apr 2006 (sd) : Its beginning of time. 
&term	10 May 2006 (sd) : Fixed problem with name of log file (/ningaui coded, not NG_ROOT).
&term	31 Jul 2006 (sd) : Added master log message if checkpoint file not found, and better 
		check point file sanity checking.


&scd_end
------------------------------------------------------------------ */
