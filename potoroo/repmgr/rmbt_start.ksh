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
# Mnemonic:	rmbt_start -- start the repmgrbt process and keep it going
# Abstract:	This script starts the repmgr batch threaded listener process and 
#		restarts it should it fail.
#		
# Date:		12 April 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun}  # get process control functions
. $STDFUN
. $PROCFUN

mylog=$NG_ROOT/site/log/rmbt_start

function chk_running
{
	cat $TMP/ps.$$ |ng_proc_info ${argv0##*/}| IFS=" " read a b		# we expect that if more than one running b will not be empty
	if [[ -n $b ]]
	then
		log_msg "it appears that $argv0 is already running; pid list=$a $b"
		return 1
	fi

	return 0
}


function start_it
{
	typeset async_flag=""

	cd $NG_ROOT/site				# go someplace known for easy core dump finding

	ng_dt -i |read now
	if [[ $(( $last_roll + 36000 )) -lt $now ]]		# last roll more than 1 hour ago (tenths of seconds)
	then
		ng_roll_log $vflag $logfile			# roll the rmbt error log
		ng_roll_log $vflag $msg_log	# roll the rmbt interesting message log
		last_roll=$now
	else
		verbose "log not rolled -- last roll was pretty recent"
	fi

	last_start=$now

	if (( $manage_restart > 0 ))
	then
		async_flag=""			# keep synchronus if we are managing restarts
	else
		async_flag="&"
	fi

	set -x
	ng_repmgrbt $aflag $vflag -s ${srv_Filer:-localhost} -R $readers -l $msg_log -p $port -r $rport >>$logfile 2>&1 $async_flag	
	set +x
	
	return $?
}

# ---------------------------------------------------------------------------------------------------------------------------------------------

					# prevent multiple executions if at all possible -- do this early (before command line processing)
ng_ps >$TMP/ps.$$			# must get a snapshot because our call to chk_running shows as a second process -- erk
case "$*" in
	*-\?*|*-c*|*-e*|*--man*)	;;			# allow even if already running, stay in fg (order in case is important)

	*-f*)		if ! chk_running			# abort now if running, stay in fg if not
			then
				cleanup 1
			fi
			;;
	*)
		if ! chk_running 
		then
			log_msg "${argv0##*/}: pausing 90s before making second check for running process"
			sleep 90
			ng_ps >$TMP/ps.$$			
			if ! chk_running
			then
				log_msg "${argv0##*/}: another ${argv0##*/} processes still running, giving up"
				cleanup 1
			fi
		fi
		$detach
		exec >>$mylog 2>&1
		log_msg "version 1.2/01297 starting"
		;;
esac

rm $TMP/ps.$$			# trash now as we are long running 

# ---------------------------------------------------------------------
ustring="[-a] [-{c|e} [-s host]] [-f] [-o] [-p port]  [-r repmgr-port] [-R num-readers] [-u] [-v]"
readers=${NG_RMBT_READERS:-3}
rport=${NG_REPMGRS_PORT:-5555}				# port that repmgr is listening on
port=${NG_RMBT_PORT}					# port that rmbt will listen on 
stop=0
vflag=""
logfile=$NG_ROOT/site/log/repmgrbt			# name of log for bt stuff
msg_log=$NG_ROOT/site/rm/rmbt				# where selected messages will be written (hood,file...)
aflag=""						# ack/handshake flag 
manage_restart=1					# we are managing the restart; -o (once) turns this off
dynamic_reader_set=1		# if user sets -R we will NOT check reader count from ppages with each start

if [[ -z $port ]]
then
	port=${NG_RMBT_PORT:-29002}
fi

filer=${srv_Filerbt:-$srv_Filer}		# for exit; use srv_Filerbt first as we are slowly moving in that direction

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	aflag="-a";;
		-f)	;;			# ignored -- foreground

		-c)	stop=1;;		# just cycle it 
		-e)	stop=2;;		# full stop; take down rmbt_start too
		-p)	port=$2; shift;;
		-r)	rport=$2; shift;;
		-R)	readers=$2; dynamic_reader_set=0; shift;;
		-s)	filer=$2; shift;;	# override srv_Filerbt default
		-u)	manage_restart=0;;	# service manager (or something else) is managing the restart (unmanaged mode)

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	log_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if (( $stop > 0 ))
then
	if [[ -z $filer ]]
	then
		log_msg "stop: could not resolve srv_Filerbt or srv_Filer"
		ng_log $PRI_ERROR $argv0 "stop: unable to determine the filer node name from srv_Filerbt or srv_Filer"
		cleanup 1
	fi

	if [[ $stop -gt 1 ]]				# halt and leave it down, so we must
	then
		if [[ $filer != $(ng_sysname) ]]	 # turn off the running rmbt-start process first
		then
			ng_rcmd $filer ng_goaway $argv0			# on another node
		else
			ng_goaway $argv0
		fi
			
		verbose "ng_repmgrbt has been signaled to stop, and will not be restarted"
	else
		verbose "ng_repmgrbt has been signaled to stop, it will be restarted"
	fi

	ng_rmbtreq $vflag -e -s $filer
	cleanup 0
fi


last_roll=0			# time of last rollup -- never as far as we can tell
delay=30			# seconds to delay between starts if it keeps dying
ng_goaway -r $argv0		# get it out of the way if left over 

while true
do
	verbose "starting ng_repmgrbt"
	ng_dt -i |read last_start

	nreaders="$( ng_ppget NG_RMBT_READERS )"
	if (( $dynamic_reader_set > 0  &&  ${nreaders:-0} > 0 ))
	then
		verbose "picked up reader count from pinkpages: $nreaders"
		readers=$nreaders		# yes, this will override -R!
	fi

	start_it

	if (( $manage_restart < 1 ))
	then
		verbose "not managing restarts of ng_repmgrbt; exit now"
		cleanup 0
	fi

	log_msg "terminated with rc=$?"

	if ng_goaway -c $argv0
	then
		log_msg "found goaway file -- exiting and not restarting"
		ng_goaway -r $argv0				# trash the file
		cleanup 0
	fi

	ng_dt -i |read now
	if [[ $(( $last_start  + 3000 )) -gt $now ]]		# tenths -- less than 5 minutes ago?
	then
		delay=$(( ($delay * 1.5) ))
	else
		delay=2						# tiny delay for the first retry if its been a while
	fi

	if [[ $delay -gt 180 ]]
	then
		delay=180
	fi

	netstat -an|grep $NG_RM_PORT
	verbose "delaying $delay seconds before attempting restart"
	sleep $delay
done

log_msg "reached odd exit point outside of while true loop"
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rmbt_start:Start/manage the repmgr threaded listener)

&space
&synop	ng_rmbt_start [-a] [-f] [-o] [-p port]  [-r repmgr-port] [-R num-readers] [-u] [-v]
&break	ng_rmbt_start [-c | e] [-s host] [-v]
&space
&desc	&This will start the Replication Manager threaded listener process ("bt")
	and keep restarting it if it dies.  The &lit(ng_repmgrbt) daemon is the 
	batch interface to replication manager.  The "bt" process accepts replication 
	manager commands and send them in batches to replication manager for 
	processing.  Please refer to the &lit(ng_repmgrbt) manual page for more details.
&space
	&This now supports the mode where the replication manager "bt" process
	is managed by the service manager (ng_srvmgr) and thus &this should 
	not attempt to restart the process. When this is the case (the -o flag
	is given on the command line), &this will start the replication manager 	
	"bt" process and then exit.


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Set the acknowlegement handshake flag when starting repmgrbt.
&space
&term	-f : Foreground mode. If not set, &this detaches from the controlling tty device
	and writes standard error messages to $NG_ROOT/site/log/rmbt_start.
&space
&term	-c : Cycle. Causes the current repmgrbt process to cycle, but does not stop
	the running &this process. 
&space
&term	-e : Exit. An exit will be sent to the &lit(ng_repmgrbt) process, and 
	the executing instance of &this will also be signalled to stop without
	restarting the daemon.
&space
&term	-p port : The port number that is passed into the &lit(ng_repmgrbt) process. 
	If not supplied it will pass in &lit(NG_RMBT_PORT). Note, once upon a time 
	this was &lit(NG_REPMGRS_PORT) but that is too confusing.
&space
&term	-r port : The port number that &lit(ng_repmgrbt) will be given to use to 
	communciate with replication manager. If not supplied, &lit(NG_REPMGRS_PORT) 
	will be used. 
&space
&term	-R n : Defines the number of reader processes that the daemon will create.
	If not supplied, 3 is used as a default value. The default can be overridden 
	by the &lit(NG_RMBT_READERS) variable. If -R is used, the NG_RMBT_READERS
	variable is &stress(ALWAYS) ignored. 
&space
&term	-s host : Indicates the name of the node that the exit command is sent to. This 
	defaults to the pinkpages variable &lit(srv_Filerbt) if not supplied. This option 
	is meaningless unless used with either the exit or cycle options.
&space
&term	-u : Unmanaged mode.  Normally, &this will manage the restart of &lit(ng_repmgrbt).
	This flag causes &this just to start &lit(ng_repmgrbt); &this will not wait for 
	&lit(ng_repmgrbt) to exit and will not attempt to restart it.  Once &lit(ng_repmgrbt)
	has been invoked, &this will exit with a good return code if this option is given.

&space
&term	-v : verbose mode.
&endterms


&space
&exit	This script exits with a zero return code to indicate that processing
	completed normally. Any non-zero exit code indicates an error and will likely 
	be accompanied by a message to the standard error device. 

&space
&see	&lit(ng_repmgr), &lit(ng_repmgrbt), &lit(ng_rmreq), &lit(ng_rm_start),
	&lit(ng_rmbtreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Apr 2004 (sd) : The first cut from the cloth.
&term	19 Oct 2004 (sd) : removed ng_leader ref.
&term	18 Sep 2005 (sd) : Fixed usage string and man page to be accurate.
&term	29 Sep 2006 (sd) : Converted to use ng_proc_info to look for other rmbt-start processes.
&term	03 Oct 2006 (sd) : Added -u option to support service manager control.  Added the 
		passing of srv_Filer to bt for the same reason. Prep for use of srv_Filerbt
		in the near future. (HBD SC)
&term	12 Jan 2006 (sd) : Changed so that we pick up NG_RMBT_READERS from pinkpages to set the 
		number of readers; so it can be easily changed with a cycle of repmgrbt. 
&term	29 Jan 2007 (sd) : No invokes ng_rmbtreq to send the exit command rather than 
		trying on its own. Added -s option. Fixed usage message and man page.
&term	20 Jan 2007 (sd) : Fixed use of ng_rcmd to set the goaway file; cannot be done at 
		node stop on the local node because parrot is gone. 
&term	03 Jul 2007 (sd) : Made a few tweeks to figure out why this script sometimes is 
		executed but generates no trace of having run.
&term	19 Oct 2007 (sd) : Tweeked the check for already running logic; now waits and retries the check
		after 90s. This is to combat quick restarts that seem to bring a new copy up before 
		the old copy has exited.  
&endterms

&scd_end
------------------------------------------------------------------ */
