#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
#	ningaui startup/shutdown 
#
#	Starting Ningaui -- ng_node start
#	We expect ng_init to be driven by the rc.d script to get the low-level
#	initialisation and site prep work out of the way.  This script does some
#	high-level verification (permissions on things, etc.) and prunes log files,
#	then starts the base set of potoroo daemons.
#
#	Narbalek must be started by ng_init because things like site-prep need
#	to set things in pinkpages (before narbalek they could just update the 
#	local/node cartulary file(s), but those have disappeared with narbalek.
#	So, if we are calld with start, we call ng_init who should drive us 
#	again with Start -- at that point we do real work.
#
#	Stopping Ningaui 
#	Attempts to shut down all of the base potoroo daemons. 
#
#	We treat spyglass differently.  We do NOT stop it when we stop or 
#	recycle.  We pause it.  This way it can scream if the node recycles
#	but does not come up completely.  When we pause it we pause it for 10m
#	which should be plenty of time for things to stop and start. When we start
#	we expect that it is running and if it is, we do cycle it to pick up 
#	a new binary if we have refurb'd or to pick up the new config file. It
#	does mean that we will generate alerts that were squelched waiting on 
#	a change.
#
# ------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun}	# get process oriented functions (is_alive, get_pid, fling...)
. $STDFUN
. $PROCFUN			# for fling and force functions

# verify that all NG_ROOT/xxxx filesystems are listed in the mount table and that
# equerry is happy with what it has to manage. This function will BLOCK until a 
# happy state is reached. We start a few critical daemons before this function is called. 
function verify_mounts
{
	typeset key=0
	typeset okey=0
	typeset tfile=""
	typeset x=""
	typeset alarm=0			# write alarm only once
	typeset counter=15		# write alarm to ng_log after about 7 minutes

	log_msg "mounted filesystem verification begins..."
	get_tmp_nm tfile | read tfile

	while ! ng_eqreq verify mounts   >$tfile 2>&1
	do
		ng_md5 -t <$tfile | read key
		if [[ $okey != $key ]]			# update with state only when it changes
		then
			log_msg "blocked, waiting on filesystems to mount/stabilise. current state:"
			while read x 
			do
				log_msg "$x"		# dump current state with timestamp
			done <$tfile
		fi

		if (( $counter == 0 && $alarm == 0 ))		# write log message just once in a while
		then
			ng_log $PRI_CRIT $argv0 "node start is blocked, waiting on filesystems to mount/stabilise. see $NG_ROOT/site/log/ng_init"
			alarm=1
			counter=120				# alarm again in an hour or so if we are still blocked
		fi

		counter=$(( $counter - 1 ))

		okey="$key"

		sleep 30
	done

	log_msg "filesystems under $NG_ROOT and ng_equerry are stable"
}


# if satellite, we need to log via an agent and to ensure that the logd is NOT listening on the cluster
# log port as that causes complete and total havoc
function set_log_parms
{
	typeset port=""

	log_parms="-f"
	if ng_test_nattr Satellite
	then
		ng_ppget -l NG_LOGGER_PORT|read port	# if a local variable defins the port, use it
		aport="$(( 30000 + ($$ % 1000 ) ))"	# it not, default to some random port so we dont collide
		if [[ -z $port ]]
		then
			ng_ppset -l NG_LOGGER_PORT=$aport	# this must be available for things to send to our logd process
		fi

		log_msg "node type is: Satellite, setting satellite oriented pinkpages vars: LOG_ACK_NEEDED and LOG_BCAST"
		ng_ppset -l LOG_ACK_NEEDED=1
		ng_ppset -l NG_LOG_BCAST=localhost

		log_parms="-f -T $NG_CLUSTER -p ${port:-$aport}"
		log_msg "setting ng_logd parms: $log_parms"
	else
		log_msg "node type is !Satellite, no log parms set."
	fi
}

# force down something that was started with endless. First signal endless not to restart the 
# daemon, then stop the daemon using procfun force command.
function e_force
{
	typeset p=""

	# we now stop endless processes whether we think they are running or not; it is safe 
	# and prevents issues on the sun which has a ps listing that does not generate enough 
	# of the command line to determine what every endless has started. 
	if (( $forreal > 0 ))
	then
		log_msg "stopping endless wrapper for: $1"
		ng_endless -e $1
	else
		log_msg "would stop endless wrapper for: $1"
	fi

	force "$@"
}

# prune the log files listed.  Keeps $1 lines; assumes files are in $NG_ROOT/site/log
# intended to prune log files created by things run here; assumes they are not running
function prune_logs
{
	typeset keep=$1 shift
	typeset l=""

	for l in "$@"
	do
		if [[ $l != /* ]]			# allow a fully quallified path to go as is
		then
			l=$NG_ROOT/site/log/$l			
		fi
		if [[ -f $l ]]
		then
			if tail -$keep $l >$l-
			then
				mv $l- $l
				log_msg "log pruned to $keep lines: $l"
			else
				log_msg "log pruned failed: $l"
			fi
		fi
	done
}

# set dynamic vars in pinkpages that need to be revised with each start
function set_pp
{
	typeset x=""
	typeset y=""
	typeset now="set by ng_node @ `ng_dt -d`"

	x=$(( `ng_ncpus` + 1 ))
	y=$(( $x - 1 ))

	if (( $y < 1 ))
	then
		y=1
	fi

	ng_ppset -c "$now" -l WOOMERA_RM_FS=$x			# these are pulled by wstart and fed to woomera as limits (WOOMERA_ removed)
	ng_ppset -c "$now" -l WOOMERA_Seneschal=$(( $x * 4 )) 
	ng_ppset -c "$now" -l WOOMERA_RM_RCV_FILE=6 
	ng_ppset -c "$now" -l WOOMERA_RM_SEND=$y 
	ng_ppset -c "$now" -l WOOMERA_RM_RENAME=4
	ng_ppset -c "$now" -l WOOMERA_RM_DELETE=4
	ng_ppset -c "$now" -l WOOMERA_PARKSUM=$y
}

# from a list of things, see what is running
function get_running
{
	typeset x=""
	typeset list=""
	
	ps_file=$TMP/node.ps.$$
	ng_ps >$ps_file
	for p in "$@"
	do
		x=`get_pids $p <$ps_file`		# get pids that match the proc name, add name and pid(s) to the list
		if [[ -n $x ]]
		then
			list="$list$p($x) "
		fi
	done

	echo $list
}


# wait for a list of things to stop
function wait4things
{
	typeset things="$@"
	typeset stop=$(( `ng_dt -i` + ${max_wait_tsec:-3000} ))

	if (( $patience > 0 )) && (( $forreal > 0 ))			# wait for things to settle
	then
		last=x
		while [[ -n "$last" ]]
		do
			sleep 2
			cur="`get_running $things`"
			if [[ $cur != "$last" ]]
			then
				log_msg "still running: ${cur:-nothing}"
				last="$cur"
			else
				sleep 3			# sleep a bit longer if nothing had changed
			fi

			if (( `ng_dt -i` > $stop ))
			then
				log_msg "wait timeout: giving up on last running things: $cur"
				return
			fi
		done
	fi
}

# start daemons -- called by start and mend sections of the big switch below, 
# so anything that must be started should be here AND
# fling must be used as that will prevent dup starts when we drive with the restart option
# this does NOT run processes that are started once as the node starts, those are executed in the 
# start portion of the big case. 
#
# NOTE:  if the process being flung does NOT detach itself, then we MUST fling it with "&" as the 
#	last argument to fling or use the -d (detach) fling option!
#	see lib/procfun for fling details; in short it starts the process if not already running
#	and can start it in 'endless' mode using ng_endless (-e).
# ------------------------------------------------------------------------------------------------
function kick_off
{
	typeset build_tries=20

	log_msg "==== starting daemons (kick_off) ========================================================" >&2

	fling -e -w ng_logd "$log_parms"	# these things start first, vital before we do a mount verification
	ng_narreq log				 # allow narbalek to use ng_log after logd is up

	fling -e ng_equerry -v 			# we fling this in init, so it is probably running; we keep it here so that we can start it with node mend too

	verify_mounts				# hold here until all filesystems seem to be stable

	while ! ng_pp_build -v			# must rebuild cartulary to get path correct if any apps directories were not mounted earlier
	do
		build_tries=$(( $build_tries - 1 ))
		if (( build_tries == 0 ))
		then
			log_msg "abort: unable to build a cartulary [FAIL]"
			cleanup 1
		fi

		log_msg "pp_build reported failure; pausing 15s before retry" 
		sleep 15
	done

				# these should not be necessary any longer -- equerry is now started very early (before site-prep)
	ng_rm_arena -v init			# re configure arena to include anything that equerry mounted that siteprep didn't see
	ng_rm_arena -v pkinit
	log_msg "active after arena initialisation: $(ng_rm_arena list)"

					# the remainder of potoroo daemons may now be started
	fling -e -w ng_parrot -f		# parrot -- repeater; command, multicast, log agent
	fling ng_rm_dbd_start -v -v		# the new rm_db daemon -- must start before repmgr, and other things that dig from rm_db
	fling -w ng_argus "-d 60 -l"		# kernel stats to master log once per minute or so
	fling ng_wstart				# woomera -- node based job manager
	fling -e ng_s_funnel -v 		# status message funnel for seneschal

	fling -w -e ng_shunt "-l -v -f"		# node to node file transport manager

	ng_start_valet				# this flings a ng_valet for each /ningaui/pkxx filesystem

	if ((  ${NG_SRVM_PORT:-0} != 0 )) 		# not defined, or specificly set to 0 on this node turns it off
	then
		if ng_test_nattr Satellite
		then
			srvm_agent="-r -a $NG_CLUSTER:$NG_PARROT_PORT"		# set agent and set remote flag that prevents it from being root
		fi
		if [[ -n $NG_NNSD_NODES ]]		# we are running an nnsd somewhere -- srvm uses it to find partners
		then
			fling -e ng_srvmgr $srvm_agent -d ${NG_NNSD_PORT:-4321}  -c ${NG_CLUSTER:-undefined_cluster} -v 		# use nnsd in place of mcast
		else
			if ng_test_nattr Satellite
			then
				fling -e ng_srvmgr -c ${NG_CLUSTER:-undefined_cluster} -v -a $NG_CLUSTER		# if mcast, satellites must use agent
			else
				fling -e ng_srvmgr -c ${NG_CLUSTER:-undefined_cluster} -v 				# plain node with multicast
			fi
		fi
	else
		m="service manager port NG_SRVM_PORT not defined or zero; sevice manger not started"
		log_msg  "$m"
		ng_log $PRI_INFO $argv0 "$m"
	fi

	# ---- service oriented things, start only if srvmgr is not managing them ------
	if ng_test_nattr Filer
	then
		log_msg "i am filer -- starting replication manager stuff"
		ng_srv_filer start

		#ng_ppset srv_Filerbt=$srv_Filer				# prep for split of filer and filerbt
		#fling ng_rm_start "-v" ">>$logd/rm_start 2>&1 &"
		#fling ng_rmbt_start "-a" #">>$logd/rmbt_start 2>&1"		# threaded repmgr interface
	else
		if [[ -f $NG_ROOT/site/rm/dump ]]
		then
			mv -f $NG_ROOT/site/rm/dump $NG_ROOT/site/rm/dump-
		fi
	fi
	
	ng_rmreq resync $mynode						# force a resynch state 

	if ng_test_nattr Jobber 
	then
		smlist="$(ng_smreq list)"				# list of things service manager is driving
		if [[ ${smlist:-none} != *Jobber* ]]			# not driving jobber on this cluster -- safe to start
		then
			log_msg "i am jobber -- starting jobber daemons"
		#	fling ng_s_start "-v -v"
		#	fling ng_n_start "-v"
			ng_srv_jobber -m start			# start jobber daemons, have their start scripts manage restarts (-m)
		else
			log_msg "i am jobber, but service manager is driving; jobber daemons NOT started"
		fi
	fi

	if ng_test_nattr Catalogue
	then
		log_msg "i am catalogue node -- starting catalogue daemons"
		fling ng_start_catman "-v"
	fi

	# this should be treated as an application, not a part of ningaui. start with an app level rota file that has a start/stop target
	#if ! ng_test_nattr Satellite
	#then
	#	fling ng_gmetric "" ">/dev/null 2>&1&"		# this seems to spit a LOT of drivel to stderr, for now ignore it
	#fi

	if ng_test_nattr CDnode
	then
		log_msg "i am a connect direct node -- starting cd related daemons"
		fling -e ng_cdmon -f 
	fi

	log_msg "==== kick_off ends ========================================================" >&2
}


# --------------------------------------------------------------------------------

ustring="{start| [-h] [-n] [-w sec] stop | restart | mend}"
logd=$NG_ROOT/site/log
cluster=`ng_cluster`
verbose=0
forreal=1
forreal_flag=""		# passed to set_leader
max_wait_tsec=3000	# tenths of seconds that we wait at a max
patience=1		# we will wait for things
hurry_flag=""		# flag for terminate call 
wait_flag=""		# flag for termiante call
mynode="$( ng_sysname )"

while [[ $1 == -* ]]
do
        case $1 in
		-h)	hurry_flag="-h"; patience=0;;				# in a hurry; dont wait for anything
		-n)	forreal=0; forreal_flag="-n";;
		-w)	wait_flag="-w $2"; max_wait_tsec=$(( $2 * 10 )); shift;;		# convert user seconds to tenths

                -v)     verbose=1;;
                --man)  show_man; cleanup 1;;
                -\?)    usage; cleanup 1;;
                -*)     error_msg "unrecognised option: $1"
                        usage;
                        cleanup 1
                        ;;
        esac

        shift
done

if ! amIreally $NG_PROD_ID
then
	if (( $forreal > 0  ))
	then
		log_msg "must be executed as $NG_PROD_ID"
		cleanup 1
		exit 1			# take no chances here 
	else
		log_msg "noaction mode set; ignoring the fact you are not $NG_PROD_ID"
	fi
fi

cd $NG_ROOT/site		# get core dumps to land in a known place


if (( $forreal < 1 ))
then
	log_msg "-n option set; no action will be taken"
fi



case "$1" in
start)			# invocation by human?
	log_msg "running ng_init to initialise and start; all startup messages will go to ng_init's log"
	ng_init		# init will call us when time is right -- after starting narbalek and setting up cartulary etc
	cleanup $?
	;;

Start)				# invocation from ng_init
	log_msg "==== ng_setup_common starts ========================================================" >&2
	ng_setup_common		# ensure links to ast things are setup in $NG_ROOT/common
	log_msg "==== ng_setup_common ends   ========================================================" >&2

	set_log_parms		# set up log parameters if a Satellite node

	# prune logs that are NOT 'rolled' as a part of their start process
	#  do NOT prune equerry or narbalek logs here!
	prune_logs 5000 $(ls $NG_ROOT/site/log/rota* 2>/dev/null)		# rota is self-pruning, but this cannot hurt
	prune_logs 5000 $(ls $NG_ROOT/site/log/parksum_* 2>/dev/null)
	prune_logs 5000 rm_start rmbt_start rm_2dbd rm_d1agent rm_dump_backup
	prune_logs 1000 n_start s_start start_catman
	prune_logs 5000 s_mvfiles s_eoj.log
	prune_logs 5000 spaceman refurb				# refurb not started here, but makes sense to trim here
	prune_logs 1000 node_stats rcv_node_stat
	prune_logs 5000 $(ls $NG_ROOT/site/log/srv_* 2>/dev/null)
	prune_logs 5000 ng_incoming hoard_del ng_cdmon ng_cargo ng_incoming_redo
	prune_logs 1000 tmp_clean.root			# should not be needed, but root's crontable buggers the log file
	prune_logs 5000 node_stop

	ng_get_nattr -P				# refresh cached attributes (clear/set services)
	set_pp					# set some dynamic variables in pinkpages

	if whence ng_tp_setid >/dev/null 2>&1		# not built for all environments
	then
		ng_tp_setid -a				# set all tape related pinkpags variables for the node
	fi

	if (( ${_procfun_ver:-0} < 2 ))		# in order to use -e on fling calls
	then
		log_msg "incorrect version of $NG_ROOT/lib/procfun is installed -- version 2 or later required"
		cleanup 1
	fi

	if (( $forreal > 0 ))
	then
		i=0
		max_tries=30

		while ! ng_pp_build		# refresh cartulary  -- must be early; may have to wait for it to find ckpt & load
		do
			i=$(( $i +1 ))
			if (( $i > $max_tries ))
			then
				log_msg "giving up on pp_build after $i tries"
				break;
			fi
			log_msg "no response, or pp-build failed; waiting 5 more seconds (will try $(( $max_tries - $i)) more times)"
			sleep 5			# we MUST wait for the cartulary to build
		done

		log_msg "loading fresh cartulary"
		. $CARTULARY 
	fi

	log_msg "stopping, yes stopping, netif service if it was left attached to an interface"
	ng_srv_netif -v stop
	
	# -------------- start our daemons: parrot, woomera, shunt, etc -------------
 	ng_log "current LOG_ACK_NEEDED=$LOG_ACK_NEEDED  NG_LOG_BCAST=$NG_LOG_BCAST"
	kick_off

	# -------------- inital runs of things, not daemons -------------------------

	fling ng_spaceman "-v" ">>$logd/spaceman 2>&1 &"	# ensure that all hld/slds are there
	fling ng_flist_send ">>$NG_ROOT/site/log/flist_send 2>&1" "&"				# send file list	
	fling ng_rm_db_vet ">$logd/rm_db_vet 2>&1 &"		# verify the rm_db database and run rm-sync if db is valid

	fling ng_node_stat ">$logd/node_stats 2>&1 &"		# different log than in root cron as root may own the other!

	
	if (( ${NG_SPYG_PORT:-0} != 0 )) 			# not defining or specificly setting it to zero for the node, turns it off
	then
		# spyglass is paused when we do a node stop/restart, so we want to cycle it after everything else has been started
		sleep $(( ($RANDOM % 10) + 1 ))			# pause a small amount of time to stagger spyglass starts if we power cycle a cluster
		log_msg "starting spyglass"
		if ! ng_sg_start -v 				# fails if already running; if so we force it to cycle if binary is new
		then			
			ng_stat -m $NG_ROOT/bin/ng_spyglass | read sgts
			ng_dt -u | read now
			if (( $(($now - $sgts))  < $(( 12 * 60 *60 )) ))		# if file is newer than 12 hours
			then
				log_msg "cycling spyglass"
				ng_sgreq -e				# we will cycle it, endless will restart
			else
				log_msg "spyglass already running; binary not new enough to force cycle"
			fi
		fi
	else
		m="spyglass port not defined or 0; not started"
		log_msg "$m"
		ng_log $PRI_INFO $argv0 "$m"
	fi

	fling -e ng_green_light				# ABSOLUTELY MUST AFTER ALL NINGAUI DAEMONS ARE STARTED
	log_msg "================= startup completed ========================="

	log_msg "================= driving rota for node_start ==============="
	ng_rota -v -t node_start			

	if [[ -s $NG_ROOT/site/forward ]]		# if something earlier, or in site prep added commands to the forward file
	then
		log_msg "info: forward file has size; pausing 30s before executing"
		sleep 30
		log_msg "==================== executing commands from forward file ==================="
		. $NG_ROOT/site/forward
		log_msg "==================== end commands from forward file ========================="
	else
		log_msg "info: forward file was empty; no commands executed"
	fi
	log_msg "============================================================="
	;;

mend)	# ----- attempt to restart daemons that aren't running, but do no initialisation ----------
	log_msg "mending by restarting daemons (wrappers mostely) that are not running"
	if ! ng_wrapper -s
	then
		log_msg "system is not stable (bad return from ng_wrapper -s); use ng_node stop and then ng_node start"
		cleanup 1
	fi

 	ng_log "current LOG_ACK_NEEDED=$LOG_ACK_NEEDED  NG_LOG_BCAST=$NG_LOG_BCAST"
	kick_off
	ng_flist_send

	log_msg "============= mend complete =============================="
	;;



stop)	# --- detach from tty/parent process and then stop ------------------------------------
	if (( $forreal <= 0 ))
	then
		log_msg "-n option given; nothing will be stopped"
		ng_node -n -v terminate					# run interactively with the not really option
	else
		slog=$NG_ROOT/site/log/node_stop
		log_msg "stopping all ningaui processes, related messages written to: $slog"
		ng_node -v $hurry_flag $wait_flag terminate >>$slog 2>&1 
	fi

	cleanup 0
	;;


terminate)	# ---- stop things --------------------------------------------------------------------
	log_msg "terminate starts for ningaui daemons"

	if (( $forreal > 0 ))
	then
		ng_log $PRI_INFO $argv0 "gracefully stopping ningaui daemons"		

		verbose "setting a 10 minute pause in spyglass"
		ng_sgreq pause 600		# pause spyglass for 10 minutes

		log_msg "================= driving rota for node_stop ==============="
		ng_rota -v -t node_stop			

	fi

	psfile=/tmp/ps.$$		 			# generate a ps listing that e_force will search
	ng_ps >$psfile

	# NOTE: things started with fling -e must be stopped with e_force

	e_force ng_green_light		# ABSOLUTELY MUST BE THE FIRST THING KILLED

	log_msg "------------ stopping per-node processes --------------"

	if (( $forreal > 0 ))
	then
		log_msg "stopping any running ng_valet processes"
		ng_start_valet -v -e
	fi

	log_msg "-------------- stopping srv_ daemons   do first in case they need primitive daemons  --------------"
	for svc in $(ng_smreq list)				# for each service managed by service manager
	do
		log_msg "stopping service if on this node: $svc"
		eval stopped_$svc=1
		if (( $forreal > 0 ))
		then
			ng_smreq stop $svc
		fi
	done

	#force ng_gmetric "ng_goaway ng_gmetric"
	force ng_ftpmon "ng_ftpmon -e"				# this may take some time to stop (leave here until first ship after 5/12/05)
	e_force ng_shunt
	force ng_argus 
	e_force ng_parrot "echo \"~Q4360\" |ng_sendudp localhost:$NG_PARROT_PORT"		# zookeeper repeater
	e_force ng_logd
	ng_wreq dump >$TMP/woomera.dump		# save state for those who refuse to use nawab
	force ng_wstart "ng_wstart -e"		# will stop woomera too.
	e_force ng_cdmon "ng_goaway ng_cdmon"
	e_force ng_equerry "ng_eqreq -e"
	force "ng_rm_sync"
	e_force ng_s_funnel "echo \"xit4360\" | ng_sendtcp localhost:${NG_SENEFUNNEL_PORT:-4365}"
	e_force ng_srvmgr			




	jobber_stuff="ng_n_start ng_s_start"					# things to wait on need to include jobber wrapers
	if ng_test_nattr Jobber && (( ${stopped_Jobber:-0} == 0 ))		# we allow filer, jobber, d1agent, netif and catalogue to be manually set
	then
		log_msg "i am jobber and not srvmgr managed -- stopping jobber daemons"
		force ng_nawab "ng_n_start -e"
		force ng_seneschal "ng_s_start -e"		# does a ng_sreq -e, and stops the keep alive script
	else
		log_msg "no action: not jobber or already stopped via srvmgr"
	fi

	if ng_test_nattr D1agent && (( ${stopped_D1agent:-0} == 0 ))
	then
		ng_srv_d1agent stop			# stop if running on this node; no harm if not
	fi

	if ng_test_nattr Catalogue && (( ${stopped_Catalogue:-0} == 0 ))
	then
		log_msg "i am catalogue node -- stopping catalogue daemons"
		force ng_catman "ng_start_catman -e"		# stops running catman_start; and catman
		cat_stuff="ng_start_catman"			# things to wait on includes catman wrapper
	else
		log_msg "no action: not catalogue node or already stopped via srvmgr"
	fi

	if (( ${stopped_Netif:-0} == 0 ))			# if not service manager managed
	then
		log_msg "stopping netif service if running"
		ng_srv_netif -v stop		# in case it switchted on us and we didn't stop it
	fi


	log_msg "-------------------------------------------------------"
	log_msg "waiting up to $(( $max_wait_tsec/10 )) seconds for processes to stop"
	wait4things ng_gmetric ng_ftpmon ng_shunt ng_logd ng_wstart $jobber_stuff $cat_stuff

	# these must be stopped LAST and only after all above are not running
	log_msg "--- stopping critical, order dependant,  daemons ------"

	if ng_test_nattr Filer   
	then
		if (( ${stopped_Filer:-0} == 0 ))			# filer is not srvm capabable, but allow for it to be
		then							# skip if it was stopped above
			log_msg "i am filer -- stopping filer daemons"
			ng_srv_filer stop

					# srv_filer should do all of these things
			#ng_rmbtreq dump $NG_ROOT/site/rm/rmbt.dump!		# capture anything pending in case node is going down
			#gre -v "^exit " $NG_ROOT/site/rm/rmbt.dump! >$NG_ROOT/site/rm/rmbt.dump
	
			#e_force ng_send_rmdump				# kill that which sends the dump to the jobber node
			#force ng_rm_start "ng_rm_start -e"		# kills rmstart and sends exit to repmgr
		fi

		# hard wait for repmgr regardless of what stopped it
		max_wait_tsec=5000					# repmgr on some nodes takes longer
		log_msg "waiting up to $(( max_wait_tsec/10 )) seconds for repmgr to stop"
		wait4things ng_repmgr

		force ng_rmbt_start "ng_rmbt_start -e"		# halt the threaded interface to repmgr -- after repmgr is gone!
	else
		log_msg "no action: not filer or already stopped via srvmgr"
	fi

	force ng_rm_dbd_start "ng_rm_dbd_start -e"		# the new rm_db daemon

	log_msg "stopping narbalek"
	e_force ng_narbalek "echo xit4360 | ng_sendtcp -t 1 -e '#end#' localhost:${NG_NARBALEK_PORT:-29055}"
	e_force ng_nnsd "echo 0:exit | ng_sendudp -t 1 localhost:${NG_NNSD_PORT:-29056}"
	max_wait_tsec=600
	wait4things ng_narbalek

	# parrot command sessions can hang and will not stop on their own; we must kill them at this point
	# we must be very sure that narbalek has gone away too.
	if (( $forreal > 0 ))
	then
		for p in ng_narbalek ng_parrot
		do
			x=`ng_ps|get_pids $p`
			if [[ -n $x ]]
			then
				log_msg "killing straggling $p process: $x"
				kill -9 $x
			fi
		done
	fi

	log_msg "shutdown completed"
	;;

restart)		# primarly for pkg_install to avoid it's having to run detached
	verbose "setting a 10 minute pause in spyglass"
	ng_sgreq pause 600		# pause spyglass for 10 minutes
	sleep 2				# let any spyglass probes finish if they can quickly

	ng_node terminate		# now stop the node -- this does leave spyglass running (do NOT use stop)

	verbose "node stop has been executed; pausing for a few seconds before restart"
	sleep ${2:-30}			# let things settle for a few seconds

	ng_init				# restart things (it detaches so we dont get a status)
	cleanup 0
	;;
	

test)	
	wait4things ng_gmetric ng_ftpmon ng_shunt ng_logd ng_wstart ng_s_start ng_n_start
	exit 1
	;;

*)
	log_msg "Usage: $0 {start | [-h] [-n] [-w sec] stop}"
	exit 1
	;;
esac

cleanup 0 
exit 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_node:Start/Stop Ningaui Daemons)

&space
&synop	ng_node start
.** ng_node Start is only invoked by ng_init and thus should NOT be documented!
&break  ng_node [-h] [-n] [-w sec] stop  
&break  ng_node restart [delay-sec] 
&break  ng_node mend

&space
&desc	&This is used to start, stop, restart, or mend the presence of Ningaui 
	daemons on a node. The action taken by &this depends on the command line
	parameter given which is expected to be one of the following:
&space
&begterms
&term start : Causes various logs to be trimmed, the &lit(NG_ROOT/common) directory
	links to be established, starts ningaui daemons and then executes several 
	processes that must be run at startup.  By default, the start action invokes
	the &lit(ng_init) script which then drives this script using the "Start"
	option.   
.** programmer note:
.** 	The dance and separation of function between ng_init and ng_node start was 
.**	due to the original implementation that required ng_init to execute as 
.**	root to install root's crontab and to do other things that only could be 
.** 	done as the super user.  Over time, we have eliminated the desire to do much 
.** 	of anything as root (outside of the few tasks performed by ng_dosu to support
.** 	euqerry and tape) and so the need for separate scripts is not truely necessary
.**	but no effort has been made to combine the init function into the start 
.**	function of this script. 

.if [ 0 ] 
&space
&term	Start : With a capital 'S,' the command does the same startup as described 
	earlier, but does NOT invoke &lit(ng_init) for initialisation.  It is NOT 
	reccommended that this option be used interactively, and is intended to 
	allow &lit(ng_init) to invoke the start function provided by &this.
.fi

&space
&term stop : Causes all ningaui daemons to be stopped.  Some daemons, such as 
	&ital(ng_woomera,) may appear to &this to have stopped, but continue to 
	execute for some time after the shutdown command has been received. 
	The stop option will cause this script to detach from the tty device and 
	execute such that all shutdown related messages are written to the &lit(node_stop)
	file in the &lit(site/log) directory.  

&space
&term	terminate : This command does the same shutdown as the &lit(stop) command,
	except the process is not detached from the parernt process. 
	This allows commands like &lit(ng_isolate) to invoke the shutdown process and capture
	the standard out/error messages in its log rather than a separate log. 

&space
&term mend : Attempts to restart any daemons that appear to not be running. Generally
	this is executed manually when the administrator notices one or more daemons is not
	running.  If a daemon's wrapper script (either a start script like ng_rmbt_start 
	or ng_endless) is down, but the daemon is still running, the daemon must be 
	stopped before executing &this with the mend option or undesired results might 
	occur.  If &this believes that the system is in such a state that a full node 
	stop must be performed, then a message to this effect will be written to the 
	standard error device, and the script will exit with a non-zero return code. 
&space
&term restart : The restart option does both with a pause of &ital(delay-sec) seconds 
	between the end of shutdown and the starting of daemons.  
&endterms 

&space
	Unless the hurry mode option (-h) is supplied on the command line
	when stopping processes, &this will wait up to a maximum amount of 
	time (5 minutes by default) for processes to stop.
&space
	This can safely be run by hand to stop or start as long as it is run under the ningaui
	user id.

&space 
&subcat The $NG_ROOT/site/forward file
	If site_prep, or any command that site_prep executes, writes one or more commands to the 
	file &lit($NG_ROOT/site/forward), &this will execute those commands after ng_green_light
	has been started. 

&space
&subcat User application start/stop
	When invoked to start or stop, &this will also execute &ital(ng_rota) with a section 
	target of either &lit(node_start) or &lit(node_stop.)   Adding an application rota file
	(to either NG_ROOT/app/<name>/lib, NG_ROOT/site or NG_ROOT/lib) that contains these
	sections is the proper way to start and stop application related processes/daemons as 
	ningaui is started and stopped.  Users should not modify &this to cause their 
	processes to be controlled. 
	

&space
&opts	These options are recognised when stopping the node:
&begterms
	-h : Hurry mode.  &This will not wait until it believes all things are 
	stopped before exiting.
&space
&term	-n : No execution mode.  &This will report what it would like to do.
&space
&term	-w n : Causes &This to wait up to n seconds for processes to stop 
	before stopping replicaiton manager and narbalek. This option is 
	ignored if the hurry option (-h) is also supplied. 
	If not supplied, the default is to wait for 5 minutes.
&endterms
&space
&parms	&This expects that &lit(start,) &lit(stop,) &ital(mend) or &lit(restart) will be 
	passed in as the only command line parameter. The parameter causes 
	the obvious thing to be done.

&space
&exit	An exit code of 1 indicates that the command line parameter was missing or was
	invalid; otherwise an exit code of zero is returned.

&space
&see	
	&ital(ng_woomera) &ital(ng_seneschal) &ital(ng_nawab) 
	&ital(ng_parrot) &ital(ng_ftpmon) &ital(ng_spaceman) &ital(ng_flist_send)
	&ital(ng_rm_db_vet) &ital(ng_catman) 
	&ital(ng_dm_stats) &ital(ng_node_stat)
	&ital(ng_repmgr) &ital(ng_repmgrbt)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Jan 2001 (ah) : Initial code
&term	18 Apr 2002 (sd) : Modified spaceman to invoke with new parameters.
&term	01 Apr 2003 (sd) : Added some things to stop.
&term 	03 Apr 2003 (sd) : Genreal housekeeping and added force function.
&term	11 Apr 2003 (sd) : more housekeeping; fling function added
&term	02 Jul 2003 (sd) : Mod to make work on linux as well as the BSD flavours
&term	10 Jul 2003 (sd) : Added mysql start; removed start of propylaum (the entrance to the 
	temple of the replication manager has been sealed).
&term	15 Jul 2003 (sd) : Corrected a few typos
&term	24 Jul 2003 (sd) : added start/stop of shunt.
&term	13 Aug 2003 (sd) : Broke process oriented function (duplicated here and in other start scripts)
		to procfun in the lib.
&term	14 Sep 2003 (sd) : Removed propy refreences, converted ng_ldr_mon call to new format.
&term	16 Feb 2004 (sd) : Added wait round the pp_build call to ensure that we get a refresh.
	It is important to pick up local changes.
&term	17 May 2004 (sd) : Changed rm_sync call to rm_db_vet call.
&term	29 Jun 2004 (sd) : Now stops jobber, catalogue stuff if its the right node.
&term	27 Jul 2004 (sd) : Added specific check for cataman.
&term	09 Aug 2004 (sd) : pulled transmission references; added endless wrapper on fling and force calls.
		Now stops srv_ daemons based on node attributes.
&term	20 Aug 2004 (sd) : Deprecated start of ldr_mon.
&term	26 Aug 2004 (sd) : Corrected prune_logs function name. 
&term	20 Sep 2004 (sd) : Some general cleanup; added stop gmetric now that gmetric spports goaway; reworked
		e_force logic to stop endless processes only if running.
&term	27 Sep 2004 (sd) : Removed attempt to start pinkpages.
&term	02 Oct 2004 (sd) : Added the send_rmdump start up if filer.
&term	04 Oct 2004 (sd) : removed all pinkpages (start) references.  
&term	19 Oct 2004 (sd) : removed reference to ng_leader.
&term	21 Jan 2005 (sd) : checks for straggling parrot processes and kills (long running hangs or agents)
		as these prevent parrot from restarting. 
&term	21 Feb 2005 (sd) : Added logic to wait for things to stop; stopped the send dump script.
&term	03 Mar 2005 (sd) : Now stops/starts ng_green_light.
&term	21 Mar 2005 (sd) : Added service manager start/stop.
&term	23 Mar 2005 (sd) : The repmgr dump sender now follows the jobber; dont need to set -d.
&term	24 Mar 2005 (sd) : Added force of cdmon.
&term	29 Mar 2005 (sd) : Corrected name in the force call for service manager.
&term	08 Apr 2005 (sd) : Added start/stop of valet processes. 
&term	03 May 2005 (sd) : Changed start of parrot to wash the environment.
&term	04 May 2005 (sd) : Now can use ng_rm_start -e to stop both script and repmgr.
&term	09 May 2005 (sd) : Added back compatability when stopping rm_start.  When the new code is 
		installed the old version of rm_start does not recognise the go-away now issued and it 
		stayed up.
&term	10 May 2005 (sd) : Removed ftpmon startup.
&term	25 May 2005 (sd) : Changed e_force to stop things running with endless' -w option.
&term	14 Jun 2005 (sd) : Now forces narbalek down if it does not stop on its own.
&term	13 Jul 2005 (sd) : Force down rm_sync.
&term	02 Aug 2005 (sd) : Removed the assignment of the var adm (not referenced).
&term	08 Aug 2005 (sd) : Added reference to site/forward.
&term	18 Aug 2005 (sd) : Added PARKSUM initialisation to ncpu-1 (RM_FS-1).
&term	16 Nov 2005 (sd) : added nnsd stuff.
&term	28 Dec 2005 (sd) : Moved set_log_parms functionality into Start rather that at start of script
		to eliminate tcp errors when node start run by hand. 
&term	04 Jan 2006 (sd) : fixed the parrot/narbalek killer -- we must be sure they are all down when stopping 
		the node.  The call to get-pids was being given both process names and it can do only one at 
		a time. 
&term	16 Jan 2006 (sd) : Added call to setup_common to ensure NG_ROOT/common links are set.
&term	07 Feb 2006 (sd) : Changed the pattern given to get_pids to allow endless to have a -d option.
&term	26 Feb 2006 (sd) : Fixed problem with the fact that suns do not support -un options on id.
&term	20 Mar 2006 (sd) : Converted to use amIreally stdfun function rather than invoking id directly.
&term	04 Apr 2006 (sd) : Fixed log pruner so that it does the right thing when passed a fully quallified pathname.
&term	16 May 2006 (sd) : Added woomera dump at shutdown to $TMP/woomera.dump. 
&term	06 Jun 2006 (sd) : Now forces attributes to refresh before attempting to start services.
&term	15 Jun 2006 (sd) : Extended wait for repmgr to finish -- increased to 5min.
&term	17 Jun 2006 (sd) : Added -c cluster option to srvmgr startup. 
&term	05 Jul 2006 (sd) : Added restart option so that pkg_install dose not need to be run detached from woomera.
&term	24 Jul 2006 (sd) : Added invocation of spyglass (HBD EKDJR/EASD) 
&term	31 Jul 2006 (sd) : Calls for a pause in spyglass when doing a restart.
&term	03 Aug 2006 (sd) : If spyglass is already running, it loads in the new config file. 
&term	16 Aug 2006 (sd) : Flings spyglass with a -w to wash the environment first. 
&term	17 Aug 2006 (sd) : Checks to see if jobber is managed via service manager and if so does not start jobber daemons.
&term	20 Sep 2006 (sd) : Pauses a few seconds before starting spyglass so that they will be staggered if we powercycle
		the whole cluster.  This could be important in staggering NFS probes. Spyglass is now paused 
		when the stop command is issued and not just when the restart command is issued. 
&term	27 Sep 2006 (sd) : Changes to take advantage of the fact that procfun now uses ng_proc_info to suss 
		information about running processes.
&term	07 Oct 2006 (sd) : Removed start of the repmgr send dump to Jobber node process. Seneschal does not need 
		the dump file anylonger. 
&term	09 Oct 2006 (sd) : We now start spyglass and service manager only if their ports are defined. This allows 
		them to be turned off by default for the release. 
&term	11 Oct 2006 (sd) : Added call to ng_tp_setid to set all pinkpages tape variables for the node.
		Now sends a resync command to repmgr for the node.
&term	03 Nov 2006 (sd) : Added calls to drive rota with a target of node_start and node_stop as the node is
		started/stopped.  This allows for user daemons to be started/stopped without having to modify 
		ng_node. 
&term	22 Nov 2006 (sd) : Fixed problem with sending repmgr a resync command -- node was not defined.
&term	03 Feb 2007 (sd) : Added wash option (-w) to all invocations of things via endless. 
&term	20 Feb 2007 (sd) : We check before invoking ng_tp command(s) as tape is not built for all O/S types.
&term	28 Feb 2007 (sd) : Added prune of s_mvfiles log. 
&term	19 Mar 2007 (sd) : If starting on a satellite node, we now ensure that either a local NG_LOGGER_PORT variable
		is set for the node, or we randomly pick one >30000 to prevent the havoc that occurs if a node is
		on the same network segment (bcast) yet defined as a satellite. See ng_parrot for more info.
&term	07 Jun 2007 (sd) : fixed port setting for satellite logging.
&term	22 Jun 2007 (sd) : The e_force function drives ng_endless to exit whether or not we believe that there is 
		an endless process running for the daemon.  
		.** We were having issues stopping narbalek on sun nodes because their ps listing is too short to suss out the fact that the endless was started to maintain narbalek. 
&term	10 Jul 2007 (sd) : Corrected typo in verbose message.
&term	24 Jul 2007 (sd) : We now force the netif service to stop so that the ip address is released. (HBD EKDJR/EASD)
&term	26 Jul 2007 (sd) : Added mend support, and brought the manpage upto date.  
&term	23 Aug 2007 (sd) : Spyglass is now started with the new ng_sg_start script. If it is already running, and the binary is not new, 
		then spyglass is not cycled in an attempt to keep the refurb related messages to a minimum.
&term	29 Aug 2007 (sd) : Added start for equerry.
&term	20 Aug 2007 (sd) : Added mount verification function which will block the start process if all ningaui 
		oriented filesystems are not stable. 
&term	05 Sep 2007 (sd) : Added logs to trim list.
&term	12 Sep 2007 (sd) : Now removes the repmgr dump file if we are not the filer. 
&term	25 Sep 2007 (sd) : We cause the unsent buffers in rmbt to be written to a file. The bt process will send this 
		data when the node is restarted, but loses the data if the node is taken down.  The info can 
		be manually added back in the case where the node is rebooted before the cache is empty.
&term	04 Oct 2007 (sd) : Added stop of d1agent service if running on the local node.
&term	05 Oct 2007 (sd) : Added start for s_funnel.
&term	15 Oct 2007 (sd) : Modified to use NG_PROD_ID rather than assuming ningaui.
&term	01 Nov 2007 (sd) : Added prune of rcv_node_stat. 
&term	25 Feb 2008 (sd) : We now call rm_arena to reinitialise after we verify mounts. This allows us to see filesystems
		that equerry might have mounted that were not present when site-prep was run.
&term	05 Mar 2008 (sd) : Repmgrbt is now stopped after we notice that repmgr is down. This should prevent issues
		with repmgr trying to record its last checkpoint in dbd. 
&term	06 May 2008 (sd) : We now start srvmgr on a satellite node with an agent so that dump requests make it back.
&term	12 May 2008 (sd) : We now force NG_LOG_BCAST and LOG_ACK_NEEDED when node type is satellite. We also place the 
		random logd port, if it is generated, into pinkpages.
&term	13 Jun 2008 (sd) : Added log message to indicate verification of mounts started.
&term	26 Sep 2008 (sd) : Now sets remote flag when starting service manager on a satellite node to prevent it from becoming root.
&term	25 Nov 2008 (sd) : Squelched error messages generated by ls during log file pruning. 
&term	26 Nov 2006 (sd) : Bumpped up the max wait timeout when running on a filer - repmgr taking a while to stop on some clusters.
&term	01 Dec 2008 (sd) : Added a rebuild of cartulary just after verification that all filesystems are mounted. This ensures that 
		we get any apps/*/bin directories that might not have been mounted during the initial build but were mounted while
		equerry paused things.  It is important to have the right path before starting parrot and woomera. 
&term	02 Jan 2009 (sd) : Added a pp-build retry if first attempt fails; likely cause is slow load from narbalek parent, so it makes
		sense to pause a few seconds and retry.
&term	06 Jan 2008 (sd) : We now alwys drive srv_netif to stop the service.  We don't depend on the attribute to trigger this 
		anymore as the attribute could have been corrupted, or the service assigned to another node without triggering a
		stop on this node. We also stop the service as we start the node to ensure it is not active. 
&term	23 Jan 2009 (sd) : Removed start/stop of gmetric -- should be treated as an external application and started with an app like
		rota file that has a start/stop target.
&term	28 Jan 2009 (sd) : Increased the number of pp_build attempts that we make at the start of kick-off before we abort. When all 
		nodes reboot we've had several that aborted because narbaleks were not getting into the tree in time. 
&term	16 Apr 2009 (sd) : The stop function now runs a list of service manager managed services and stops them if they are 
		running locally. Srv_jobber and srv_filer commands are now used to start/stop these services.
&term	20 Apr 2009 (sd) : Removed the pruning of equerry logs -- not needed and since equerry is now started very early it 
		was causing issues. 
&term	23 Apr 2009 (sd) : Moved the logic to stop services such that it is before service manager on the node is stopped. 
&term	28 Apr 2009 (sd) : Stop now invokes the terminate function of &this in a detached mode so that messages go to 
		a standard log file.  User can tail the log file if needed. 
&term	11 May 2009 (sd) : Restart now invokes 'ng_node terminate' rather than stop as it must be synchronous so that we can 
		run ng_init when it finishes. 
&term	22 May 2009 (sd) : Now sends a log command to narbalek to allow it to start logging -- after ng_logd is started. 
&term	30 Jun 2009 (sd) : The stop function now blocks.  There were too many issues related to it's working in the background.
&endterms

&scd_end
------------------------------------------------------------------ */
