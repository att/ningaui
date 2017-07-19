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
# Mnemonic:	ng_rm_start
# Abstract:	Start/stop repmgr
#		
# Date:		Long ago.
# Author:	Andrew Hume
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# return the age of the file in seconds
function fage
{
	if [[ -f $1 ]]
	then
		echo $(( $(ng_dt -u) - $(ng_stat -m $1) ))
	else
		echo "0"				# bad file, bad time
	fi
}

# save some stuff (flists, ckpt file, dump file) for debugging and/or disaster recovery
# should be invoked just before we purge and request new flists
function save_stuff
{
	typeset now=$(ng_dt -d)

	if [[ ! -d $TMP/billabong/repmgr ]]
	then
		if ! mkdir -p $TMP/billabong/repmgr 
		then
			log_msg "unable to make $TMP/billabong/repmgr; nothing saved off"
			return
		fi
	fi

	(
		if cd $TMP/billabong/repmgr
		then
			find . -mtime +15 -exec rm -f {} \;
		fi

		if ! cd $NG_ROOT/site/rm
		then
			log_msg "unable to switch to rm directory for backup"
			return 
		fi

		tar -cf - nodes dump rm.cpt | gzip >$TMP/billabong/repmgr/stuff.$now.tgz
		gzip <${ckpt_fname:-nosuchfile} >$TMP/billabong/repmgr/ckpt.$now.gz
	)
}

# look at the set of file lists.  If any are older than 6 hours we delete them
# in all cases we request a new flist from each cluster member and then pause
# for at least 30s to wait for their arrival. If we seem to not have any 
# recent files (maybe this is a node that has not run repmgr before) we wait
# longer for flists.
function validate_flists
{
	typeset recent=0		# number of files that are recent 
	typeset old=0
	typeset f=""
	typeset age=""
	typeset have=""			# number of flist files we have
	typeset nm=""			# number of members in the cluster
	typeset m=""			# cluster member list
	
	verbose "validating flist files"
	for f in nodes/*				# we WILL trash transient files (node.xx) if they are old
	do
		age=$( fage $f ) 
		if (( ${age:-0} > $(( $RM_NODE_LEASE * 3600 )) ))
		then
			old=$(( $rcount + 1 ))				
			rm $f
			verbose "purged old flist: $f $age sec"
		else
			if (( ${age:-0} > 1800 ))			# old, but within usable tollerance so we dont remove
			then
				verbose "flist old but kept: $f $age sec"
			else
				verbose "flist is recent: $f" 	
				recent=$(( $recent + 1 ))		# if NOTHING is recent, we wait longer after we send out flist_send requests
			fi
		fi
	done

	verbose "requesting flists from members; pausing a bit to let them arrive"
	m=$( ng_members -N )			# must use narbalek method of getting members
	for n in $m			# request a list
	do
		ng_rcmd -t 20 $n ng_flist_send  >/dev/null 2>&1
	done

	for n in $m
	do
		nm=$(( $nm +  1)) 
	done
	loop=$(( ($old > 0 || $recent == 0) ? 4 : 1 ))		# how long will we wait -- longer if we trashed one or there were none
	while (( $loop > 0 ))
	do
		sleep 30			# small wait, then check to see how many we got

		have=$( ls nodes/* 2>/dev/null |wc -l)
		if (( $have >= $nm ))
		then
			break
		else
			verbose "waiting for file list files: have $have, need $nm"	
		fi

 		loop=$(( $loop-1 ))
	done
	verbose "wait for file list files ended: have $have, need $nm"	

	if (( ${verbose:-0} > 0 ))
	then
		ls -al nodes/* >&2
	fi
}

if [[ $* != *-i*  && $* != *-e* && $* != *--man* && $* != -\? ]]	# if not exiting, manual, or interactive, drop off off from parent
then
	if ! amIreally $NG_PROD_ID
	then
		log_msg "this command must be executed as user id: $NG_PROD_ID"
		cleanup 2
	fi

	log_msg "detaching; loging to: $NG_ROOT/site/log/rm_start"
	$detach
	exec >>$NG_ROOT/site/log/rm_start 2>&1				# ng node prunes our log before starting
fi

# ---------------------------------------------------------------------------------
ustring="[-e] [-f] [-i] [-u] [-v]"
stop=0
tmp=$TMP/rms
me=$(ng_sysname)
manage_restart=1		# set to 0 by -u when service manager is driving (unmanaged mode)
force=0				# sanity limits on checkpoint age; this overrides
verbose=0
specific_ckpt=""

while [[ $1 == -* ]]
do
	case $1 in 
		-c)	specific_ckpt=$2;;
		-e)	stop=1;;
		-f)	force=1;;
		-i)	;;			# ignore from detach test (interactive)
		-u)	manage_restart=0;;	# service manager is driving; start once

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




if (( $stop > 0 ))		# send stop to repmgr and set goaway to stop rm_start if it is running in manage restart mode
then
	verbose "scheduling repmgr and rm_start to exit"
	ng_log $PRI_INFO $argv0 "sending exit to repmgr"
	ng_goaway ${argv0##*/}
	ng_rmreq -s $srv_Filer exit
	cleanup 0
fi

if ng_test_nattr Satellite Relay
then
	msg="cannot start repmgr on node with Satellite or Relay attribute"
	log_msg "$msg [FAIL]"
	ng_log $PRI_CRIT $argv0 "$msg"
	cleanup 1
fi


if ! cd $NG_ROOT/site/rm
then
	ng_log $PRI_CRIT $argv0 "cannot switch to site/rm directory; repmgr not started"
	cleanup 1
fi

ng_goaway -r $argv0					# if it was left over from our last try -- ditch it

if [[ -n $NG_RM_TUMBLER ]]				# a tumbler indicates that there best be a checkpoint file
then
	NG_RM_REQ_CKPT=${NG_RM_REQ_CKPT:-1}		# they can force it off, but likely will not
else
	ng_ppset NG_RM_TUMBLER="rm,cpt,0,60,0,200"	# no tumbler -- fresh cluster we assume; set it
fi

if [[ -n $specific_ckpt && ! -r $specific_ckpt ]]
then
	log_msg "cannot find user requested checkpoint: $specific_ckpt [FAIL]"
	ng_log $PRI_ERROR $argv0 "cannot find user requested checkpoint: $specific_ckpt; repmgr not started"
	cleanup 1
fi

while true
do
	log_msg "begin repmgr startup; rolling repmgr log files if there"

	ng_roll_log -v -n 4 rm 		# roll the log; keep 4 copies

							# get name of most recent chkpt file on this node, and build a roll forward file
	roll_file=$TMP/PID$$.rm_start.recovery

	if [[ -z $specific_ckpt ]]
	then
		# without the -M option, the old style search is used. this is needed for back compat before any ckpts have md5 values.
		#ng_ckpt_prep -f 8  -v -r $roll_file "REPMGR HOOD|REPMGR FILE" "repmgr.*CHKPT" "rm[.].*cpt"| read ckpt_fname junk
		ng_ckpt_prep -f 8 -M 11 -v -r $roll_file "REPMGR HOOD|REPMGR FILE" "repmgr.*CHKPT" "rm[.].*cpt"| read ckpt_fname junk
	
		replay_marker="${ckpt_fname##*/}"				# repmgrbt marker name should be the ckpt basename
	else
		msg="user supplied checkpoint file is being used: $specific_ckpt"
		log_msg "$msg [OK]"
		ng_log $PRI_INFO $argv0 "$msg"
		ckpt_fname=$specific_ckpt
		specific_ckpt=""				# only good for the first go
		replay_marker=""				# no marker for this
		>$roll_file					# nothing valid in roll file
	fi

	if [[ -r ${ckpt_fname:-nosuchfile} ]]				# file is there and user has set max age; test it
	then
		actual_age="$(fage $ckpt_fname)" 
		if (( ${NG_RM_MAX_CKPT_AGE:-0} > 0  &&  ${actual_age:-0} > ${NG_RM_MAX_CKPT_AGE:-0}  ))
		then
			if(( force > 0 ))
			then
				log_msg "CAUTION: checkpoint file seems old (age=$actual_age sec): $(ls -al $ckpt_fname)" 
			else
				log_msg "abort: checkpoint file seems too old (age=$actual_age sec): $ckpt_fname"
				ng_log $PRI_CRIT $argv0 "repmgr not started: checkpoint file seems too old (age=$actual_age sec): $ckpt_fname"

				ls -al $ckpt_fname >&2
				cleanup 1
			fi
		fi
	else					
		if (( ${NG_RM_REQ_CKPT:-0} > 0 ))
		then
			log_msg "abort: must have a valid checkpoint to start (NG_RM_REQ_CKPT is set)"
			ng_log $PRI_CRIT $argv0 "bad checkpoint file ($ckpt_file) repmgr not started (NG_RM_REQ_CKPT is set)"
			cleanup 3
		fi

		log_msg "WARNING: no checkpoint file found or file was not readable ($ckpt_fname)"
		ckpt_fname=/dev/null				
		replay_marker=""				# there will be no marker we can trust
		ng_log $PRI_CRIT $argv0 "repmgr is being started without a checkpoint file!"
	fi

	save_stuff					# save old flists and the ckpt file for debugging or recovery
	validate_flists					# toss old flists and request new ones (will pause if we request new)
	
	> $tmp.1					# will contain the roll forward list from log; sorted and unique
	if ng_rmbtreq find ${replay_marker:-nomarker}				# does bt have a marker that matches this?
	then
		verbose "marker exists in repmgbt; replay will be requested"
		replay=1			# replay and let tmp.1 be empty
	else
		verbose "marker does not exist in repmgbt; roll forward file will be generated from master log messages"
		replay=0			# no replay so we convert master log messages to recovery (roll forward) commands
		awk ' 
			/REPMGR FILE /	{ sub(".*FILE ", "file ", $0); print $0; next }
			/REPMGR HOOD /	{ sub(".*HOOD ", "hood ", $0); print $0; next }
		' <$roll_file >$tmp.1
		verbose "roll forward file ($tmp.1) created with $(wc -l < $tmp.1) records"
	fi


	(						# generate an initial set of things for repmgr to munch on
		echo "quiet 1"
		cat ${ckpt_fname:-/dev/null}		
		#tac $tmp.1 | uniq			# no longer needed; ckpt-prep sorts unique by datestamp 
		cat $tmp.1				# add in roll forward (empty if we will be replaying
		for i in nodes/*			# copy current file list files extending the lease by 10 minutes
		do
			if [[ $i != *"."* ]]		# nodenames cannot have dots!  avoid node.xx transient files
			then
				j=$TMP/repmgr/rmn_${i##*/}
				(
					sed /instances/q $i | sed '$d'
					echo lease=$(( $(ng_dt -u) + 600))
					sed -n '/instances/,$p' $i
				) > $j
				echo "copylist $j"
			fi
		done
		echo "resync all"
		echo "quiet 0"
		echo "checkpoint"
	) > $tmp.2


	set -x
	ng_repmgr -v -i $tmp.2 > $NG_ROOT/site/log/rm 2>&1 &		# start async; we will wait later if we manage the restart
	pid=$!
	set +x

	sleep 2
	if (( $replay > 0 ))				# send only if rmbt has a replay marker that matched the chkpt file (never if user supplied ckpt)
	then
		verbose "asking repmgrbt to replay from $replay_marker"
		ng_rmbtreq play  $replay_marker
	fi

	verbose "giving repmgrbt a clear to send signal"
	ng_rmbtreq cts					# clear to send to repmgr again (MUST be after replay or we send pending things twice)
	
	if (( $manage_restart < 1 ))			# something else (ng_srvmgr maybe) will notice and restart if repmgr goes away
	then
		verbose "running in unmanaged mode (-u), not waiting for repmgr to finish"
		cleanup 0
	fi

	verbose "waiting for repmgr: $pid"
	wait
	status=$?
	log_msg "repmgr exited with a return value of $status"
	log_msg "last few lines of repmgr log file:"
	tail -15 $NG_ROOT/site/log/rm >&2
	log_msg "========= end of repmgr log extract ============="

	ng_log $PRI_INFO $0 "repmgr terminated status=$status"

	if ng_goaway -c $argv0
	then
		ng_log $PRI_INFO $argv0 "found goaway file; not restarting repmger, $argv0 is exiting"
		log_msg "exit: found goaway file; not restarting repmger"
		ng_goaway -r $argv0			# ditch it before we go.
		cleanup 0
	fi
	
	specific_ckpt=""			# user supplied ckpt name is only good for the first go

	# we used to pause 30 sec before restarting, but now that we wait 30s (min) for flists, we dropped the wait here
done

cleanup 9						# should never get here


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_start:Star/stop replication manager)

&space
&synop	ng_rm_start [-c ckpt-name] [-e] [-f] [-u]

&space
&desc	&This starts the replcation manger on the current node. &This waits for
	replication manger to exit and will restart it unless a specific goaway 
	file exists for &this or &this has been started in &ital(unmanaged restart)
	mode (-u).  Supplying the exit (-e) option on the command line
	cause &this to signal the running replication manger to stop, and sets 
	the goaway file so that the controlling copy of &this will not restart 
	replication manager. 
&space
	Before starting replication manager, &this will attempt to find the most 
	recent checkpoint file on the node, and to generate a list of 'roll forward'
	commands that reflect replication manager activity following the checkpoint
	file and when it stopped.  This allows replication manager to be started 
	on a node that does not have the most recent checkpoint file. 
&space
	The roll forward commands are generated from the &lit(ng_rempgrbt) process
	if it has the necessary information in its &ital(replay cache.)  If the 
	commands cannot be sussed from &lit(ng_repmgrbt,) then the roll forward 
	commands are extracted from the master log. 
&space
	&This will refuse to start the replication manager if the checkpoint file 
	is more than &lit(NG_RM_MAX_CKPT_AGE) seconds old.  If &lit(NG_RM_MAX_CKPT_AGE)
	is not set, then no age resitrictions on the checkpoint file is imposed. 
	&This can be forced to start with an old checkpoint
	file if run with the -f option.  Further, if the pinkpages variable 
	&lit(NG_RM_REQ_CKPT) is set to a positive value, &this will abort if it 
	cannot find a replication manager checkpoint file. The default is to allow
	replication manager to start without a checkpoint file as this is the 
	only way to bring up a new cluster.  It is &stress(strongly recommended)
	that the &lit(NG_RM_REQ_CKPT) varible be set after replication manager
	has been successfuly started on a cluster. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c ckpt-name : The fully qualified pathname of a checkpoint file to use in place of 
	the most recent checkpoint file found on the node.  This file may exist (should exist?) in 
	$TMP and will be used &stress(only) for the first attempt to start the replication manager. 
	When a user supplied checkpoint file is given, there is no attempt to generate any 'roll foward'
	commands based on masterlog data, nor is there an attempt to have &lit(ng_repmgrbt) replay 
	commands from a previous point in time.   Use this option with caution. 
&space
&term	-e : Exit. Cause both the controlling &this process, and replication manger, to 
	exit.
&space
&term	-f : Force mode.  If &lit(NG_RM_CKPT_AGE) is defined and &this cannot find a checkpoint file that is less than 
	&lit(NG_RM_CKPT_AGE) seconds old it will normally abort.  The -f option causes the check to 
	be disabled and replication manager is allowed to start with an old checkpoint
	file. 
&space
&term 	-i : Interactive mode.  When this flag is supplied, &this will remain attached 
	to the tty device. Mostly this is for testing and debugging. 
&space
&term	-u : Sets the restart mode to unmanaged.  When in unmanaged mode, &this does 
	not wait for repmanager to finish and attempt to keep restarting it.  The assumption
	is that when &this is started with the -u option, another application (ng_srvmgr)
	will be managing the restart of replication manager. 
&endterms


&files
	Before replication manger is started, &this will save several things into the 
	&lit($TMP/billabong/repmgr) directory.  Specifically a tar file is created
	(stuff.<time>.tgz) that contains the flist files in the nodes directory, the 
	dump file and the rm.cpt file (all from $NG_ROOT/site/rm).  The checkpoint
	file that is presented to replication mnager is also saved (cpt.<time>.gz)

&space
&exit	An exit code that is not zero indicates an error.  Log messages should be 
	written to both stderr and the master log if an error is noticed.

&space
&see	&lit(ng_repmgr), &lit(ng_repmgrbt), &lit(ng_rmbt_start), &lit(ng_rmreq), &lit(ng_rmbtreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	21 Jun 2003 (sd) : to allow start on a system that has never run repmgr
&term	10 Jan 2005 (sd) : to send repmgrbt an ok2send message after rpmgr shuts down.
			rmbt stops trying to connect to repmgr once it has sent an 
			exit message to prevent loss of commands that are sent after
			the exit it received.  This allows rmbt to start connection attempts again.
&term	11 May 2005 (sd) : Fixed ng_log messages that had bogus severity levels set.
&term	06 Jun 2005 (sd) : Now detaches on its own and directs its log to the right place.
&term	01 Sep 2005 (sd) : Changed FILE and HOOD searches to "REPMGR *" so as not to match
		instances in the checkpoint that have FILE in their name. 
&term	05 Oct 2005 (sd) : Converted tail -r to tac call.
&term	20 Mar 2006 (sd) : Mod to ensure script is run by ningaui.
&term	24 May 2006 (sd) : Now scraps goaway file as it exits (bug 746).
&term	26 May 2006 (sd) : Dumps ls info on the ckpt file it selected. 
&term	04 Oct 2006 (sd) : Modifications to use ng_ckpt_prep to generate roll file and to 
		find the most recent checkpoint file on the node, and to take advantage
		of ng_repmgrbt replay cache.  Added some extra checking with regard
		to age of checkpoint file. 
&term	11 Oct 2006 (sd) : Added resync to the list of initial commands to process.
		Added check to ensure that transient files (node.xx) were not found in
		the search of the flist directory.
&term	21 Nov 2006 (sd) : We now issue a crit message if repmgr is started without a checkpoint file.
&term	09 Feb 2007 (sd) : Added check for tumbler; we set if not there and require a checkpoint if it is
	(will not get bit if rm_dbd fails to start).
&term	19 Sep 2007 (sd) : Modified call to ckpt_prep to step toward the new style of checkpoint 
		messages in the master log (those that have an md5 value).
&term	15 Oct 2007 (sd) : Modified to use NG_PROD_ID rather than assuming it is ningaui.
&term	17 Apr 2007 (sd) : Now spits out the last few lines of the repmgr log file after repmgr terminates.
&term	19 Aug 2008 (sd) : The default values for the tumbler 'a' value was set to 60 (from 16). 
&term	25 Feb 2009 (sd) : Corrected resetting of lease -- repmgr wants unix time not ningaui time.
&term	14 Apr 2009 (sd) : Added check to prevent startup on Satellite/Relay node.
&term	15 Apr 2009 (sd) : Added -c option. 
&term	28 Apr 2009 (sd) : We now capture some things from site/rm before we kick repmgr. Mostly for 
		history and trouble solving.  Files are written to $TMP/billabong/repmgr.
&term	23 Jun 2009 (sd) : Added prune code to keep the stuff saved in billabong trimmed. 
&endterms

&scd_end
------------------------------------------------------------------ */
