#
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


!/usr/common/ast/bin/ksh
#!/usr/common/ast/bin/ksh
# Mnemonic:	srv_filer.ksh 
# Abstract:	Manage the filer service as needed by srv mgr. Provide the 
#		query/score/start/stop commands for this service.  
#		This script also provides a move command that can be used to manually
# 		move the filer from one node to another in the cluster.  The move 
#		command must be executed on the current filer. 
#		
# Date:		28 March 2008 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  	# get standard functions
PROCFUN=${NG_STDFUN:-$NG_ROOT/lib/procfun} 
. $STDFUN
. $PROCFUN

# wait up to $2 seconds for the process ($1) to not be listed by ps.
# echos to stdout the number running when the function decides to return.
function wait4proc
{
	typeset wait_loops=${2:-10}
	typeset count=1

	while (( count > 0  &&  $wait_loops > 0 ))
	do
		sleep 10 
		wait_loops=$(( $wait_loops - 1 ))
		ng_ps | ng_proc_info -c ${1:-ng_rm_start} | read count
		if (( $count > 0  ))
		then
			verbose "still waiting: $1"
		fi
	done

	echo $count
}

# execute a command and abort if it fails
function try
{
	typeset rc=""

	if (( ${forreal:-0} > 0 ))
	then
		"$@" 
		rc=$?
		if (( $rc != 0 ))
		then
			log_msg "abort: command failed: $@"
			cleanup 1
		fi

	else
		log_msg "command skipped: $@"
	fi
}

# check to see if either nawab or seneschal is listed in the process table
function chk_process
{
	if [[ ! -f $TMP/ps.$$ ]]		# query attempts to get one from the remote place
	then
		ng_ps >$TMP/ps.$$
	fi
	if is_alive "$1" <$TMP/ps.$$
	then
		return 0
	fi

	return 1	
}

# see if the process is responsive
function chk_response
{
	typeset x=""
	typeset tollerate=3000

	echo "dump1 foo" | ng_rmreq -a |read x

	if [[ -n $x ]]			# assume if we got something the process is there and alert
	then
		verbose "$filer: responded"
		return 0
	fi

	verbose "no response from $filer"

	ng_rcmd $filer 'tail -1 $logd/rm' | read ts junk
	now=$( ng_dt -i )
	ts=${ts%\[*}
	if (( ${ts:-0} + $tollerate  > ${now:-0} ))
	then
		verbose "logfile seems current ($(( $now - ${ts:-0} )) old); assuming alive: $filer"
	else
		verbose "from $filer: $ts $junk"
		if [[ -n $ts ]]			# no response or bad stuff
		then
			verbose "no data received from $filer; assuming dead"
		else
			verbose "last update to logfile is old ($(( $now - ${ts:-0} ))ts); assuming dead: $filer"
		fi
		return 1
	fi

	return 0
}

# same code used by both the stop and move functions to stop repmgr and/or rmbt
function stop_daemon
{
	while [[ -n $1 ]]
	do
		case $1 in 
			repmgr)				# stop the main daemon
				verbose "stopping repmgr" 
				ng_rmbtreq dump $NG_ROOT/site/rm/rmbt.dump!		# capture anything pending in case node is going down
				gre -v "^exit " $NG_ROOT/site/rm/rmbt.dump! >$NG_ROOT/site/rm/rmbt.dump
		
				try force ng_rm_start "ng_rm_start -e"		# kills rmstart and sends exit to repmgr
		
				log_msg "waiting up to 300 seconds for repmgr to stop"
				still_running=$( wait4proc ng_rm_start 30 )		# wait up to 300 seconds for rm_start to die on this node
				if (( $still_running > 0 ))
				then
					log_msg "abort: cannot get repmgr to stop on: $thisnode process=$(ng_ps|ng_proc_info -v ng_rm_start)"
					#cleanup 1
				fi
		
				verbose "detected end of rm_start daemon"
				;;

			rmbt)
				verbose "stopping repmgrbt"
				try force ng_rmbt_start "ng_rmbt_start -e"		# halt the threaded interface to repmgr 
				;;
		esac

		shift
	done
}

# -------------------------------------------------------------------
ustring="{start|stop|score|query|move target-node}"
log2file=0				# -l sets to capture stderr into a private log in $NG_ROOT/site
filer="$( ng_ppget -L srv_Filer )"	# avoid stale cartulary
thisnode="$( ng_sysname )"
port=$NG_NARBALEK_PORT			# not needed, but makes testing with a standalone narbalek easier
forreal=1
logd=$NG_ROOT/site/log

while [[ $1 = -* ]]
do
	case $1 in 
		-p)	port=$2; shift;;
		-l)	log2file=1;;
		-n)	forreal=0;;

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

if (( $# < 1 ))		# must have command
then
	usage
	cleanup 0
fi

if (( $log2file > 0 ))
then
	logf=$logd/${argv0##*/ng_}; exec 2>>$logf
fi


case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Filer="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	start)	
		filer=$thisnode
		ng_ps | ng_proc_info -c ng_rm_start |read up 
		if (( $up > 0 ))
		then
			log_msg "filer is already running; stop it first"
			try force ng_rm_start "ng_rm_start -e"		# kills rmstart and sends exit to repmgr
			verbose "repmgr appears to be running here; stopping it first"
			still_running=$( wait4proc ng_rm_start 30 )		# wait up to 300 seconds for rm_start to die on this node
			if (( $still_running > 0 ))
			then
				log_msg "start: abort: cannot stop previous version of rm_start"
				cleanup 1
			fi
		fi
		verbose "repmgr is not running; safe to start"

		log_msg "starting filer service"
		ng_ppset -p $port srv_Filer=$thisnode 			
		ng_ppset -p $port srv_Filerbt=$thisnode 			

		log_msg "running pp_build on all members"
		for x in $(ng_members)
		do
			ng_rcmd $x ng_pp_build					# important so that rm_start sees updated value
		done

		sleep 1		# let that dust settle 

		(
			cd $logd
			if mv rm_start rm_start- 2>/dev/null	# trim the rm_start log
			then
				tail -5000 rm_start- >rm_start
			fi

			if mv rmbt_start rmbt_start- 2>/dev/null
			then
				tail -5000 rmbt_start- >rmbt_start
			fi
			rm -f rm_start- rmbt_start-
		)

		ng_rm_start -v >>$logd/rm_start 2>&1 
		ng_rmbt_start -a 				# threaded repmgr interface, redirects output to its log file

		ng_log $PRI_INFO $argv0 "filer service started on $thisnode"
		log_msg "attrs after start: $(ng_get_nattr)"
		;;
	
	stop)
		verbose "stopping replication manager daemons"
		stop_daemon repmgr rmbt				# stop them both

		cleanup 0
		;;

	score)
		if ng_test_nattr Satellite 
		then
			echo 0
			log_msg "satellite attribute set; cannot accept service; score=0"
			cleanup 0
		fi

                if [[ -f $NG_ROOT/site/filer.must.score ]]
                then
                        read x y <$NG_ROOT/site/filer.must.score
			log_msg "mustscore file found: score=$x"
                        echo ${x:-0}
                        cleanup 0
                fi

		if ng_test_nattr Jobber Catlogue FLOCK_WALDO
		then
			score=2
			verbose "we are either the jobber or catalogue node, score to avoid"
		else
			if ng_test_nattr Filer		# we are already signed up as the filer
			then
				score=75		# just vote us as the winner
				echo $score
				log_msg "we believe we are the current host ($srv_Filer)"
				cleanup 0
			else
				ng_tumbler -g -1 -p NG_RM_TUMBLER | read tf
				clist=$TMP/PID$$.clist				# list of checkpoint files known on this node
				score=3						# default score -- slightly better than node with other service
				boost=20					# score booster for current checkpoint
				ng_rm_where -p -n -5 "rm.[ab][0-9][0-9][0-9].cpt"| gre $thisnode >$clist

				# 8th token is the chkpt path, last token is the md5
				gre "REPMGR CHKPT" $logd/master |tail -20| sort -u -k 1rn,1 | while read j1 j2 j3 j4 j5 j6 j7 j8 junk
				do
					cf=${j8##*/}
					c5="${junk##* }"			# snag the md5 vlaue
					if gre "$cf $c5" $clist >/dev/null 	# if on this node, boost the score 
					then
						verbose "checkpoint is on node: $cf"
						score=$(( $score + $boost ))
					fi

					boost=1		# smaller boost for each ckpt that is not the latest
				done

				verbose "based on local copies of recent checkpoint files score=$score"
			fi
		fi


		log_msg "returned score=$score"
		echo $score
		;;

	query)						# see if the daemons are still alive
		state=0
		if chk_response 
		then
			state=1
		fi

		case $state in 
			1)
				verbose "repmgr responded: report green"
				echo "green:$filer"
				cleanup 0
				;;

			0)	
				if [[ $filer == $thisnode ]]		# local puppy, not a net issue, so assume down hard
				then
					verbose "filer is this host and no response from one/both processes; reporting red"
					echo "red:$thisnode"
					cleanup 0
				fi
				;;
		esac

		# no response, could be network, so report yellow and assume that things might clear before next test; else it becomes red
		verbose "no response could be network issue, report: yellow"
		echo "yellow:$filer"
		;;

	# a special option for this service to manually move the service 
	move)
		verbose "verfication starts"
		if ! ng_test_nattr Filer		# this must originate on the filer node!
		then
			log_msg "the move option can be used only on the current filer"
			cleanup 1
		fi

		target=$2
		m=" $( ng_members ) "
		if [[ $m != *" $target "* ]]
		then
			log_msg "cannot move: target ($target) is not a member of this cluster ($NG_CLUSTER)"
			cleanup 1
		fi

		log_msg "moving filer from $thisnode to $target"
		
		verbose "stopping daemons on this node"
		stop_daemon repmgr					# shut down repmgr, do rmbt later

		verbose "searching masterlog for last checkpoint"
		# 8th token is the chkpt path
		grep "REPMGR CHKPT" $logd/master |tail -20 | sort -u -k 1rn,1 | while read j1 j2 j3 j4 j5 j6 j7 j8 junk
		do
			if [[ -f ${j8:-foo} ]]
			then
				ckpt_path=$j8
				break;
			fi
		done

		if [[ -z $ckpt_path ]]
		then
			log_msg "abort: cannot find  a final checkpoint file to send to the target node"
			cleanup 1
		fi
		verbose "found final checkpoint: $ckpt_path"


		rckpt_path=$( ng_rcmd $target ng_spaceman_nm ${ckpt_path##*/} )
		verbose "sending last checkpoint to: $target:$rckpt_path"
		try ng_ccp $vflag -d $rckpt_path $target $ckpt_path
		try ng_md5 -t $ckpt_path |read lmd5
		try ng_rcmd $target ng_md5 -t $rckpt_path |read rmd5		

		if [[ $rmd5 != $lmd5 ]]
		then
			log_msg "abort: md5 after copy did not match: $target:$rckpt_path=$rmd5 $ckpt_path=$lmd5"
			cleanup 1
		fi
		verbose "copy of chkpt file validated on $target"


		try ng_rcmd $target ng_rm_rcv_file $rckpt_path		# add it to the rm_dbd stash over there for rm_start
				
		ng_rmbtreq rmhost $target				# shift local copy to send queued data to new host before stopping 
		sleep 1
		stop_daemon rmbt					# stop this after rcv file so that it does not gen errors
	
		verbose "sleeping a few seconds to let the dust settle"
		sleep 5
		log_msg "starting repmgr on $target (the next messages are from $target:ng_srv_filer start)"
		try ng_rcmd $target ng_srv_filer start
		log_msg "end of messages from $target"
		log_msg "filer has been moved"
		;;
		
	no_op)				# make for easier testing
		verbose "no-op: $@" 
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_filer:Filer Service Interface)

&space
&synop	ng_srv_filer {start|stop|score|query}
&break	ng_srv_filer move target-node

&space
&desc	This provides the service manager with a process that is used
	to determine the state (green, yellow, red) of the service, 
	generate the node's score with regard to the ability to host the filer service, 
	and with entry points to start and stop the service.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p port : The port to send to when communicating with narbalek.  Allows for a 
	separate black board (narbalek) space to test with.
&endterms


&space
&parms	Expected on the command line is an action token which is one of: start, stop, query, or score.

	The action token causes the following to happen:
&begterms
&term	init : Causes the service manager pinkpages variable for this service to be defined. 
	The service manager variable consists of the command line commands to invoke the start, 
	stop, query and score functions of this service as well as election information. See 
	the ng_srvmgr manual page for the specifics about the variable.  
&space
&term	start : Initialise the service.  
&space
&term	query : Determines the state of the service.  A single token is echoed to stdout 
	indicating the state. The token values are defined by the service manager and are 
	assumed to be gree, yellow or red.  
&space
&term	stop : Terminates the service. 
&space
&term	score : Computes the score and writes it to the standard output device. The score 
	is an integer between 0 and 100; the larger the value the more able the node is to 
	accept the service.  A score of zero (0) indicates that the node should not be awarded
	the service. 
&space
	If it is necessary to force a node to always score low or high, then the file 
	$NG_ROOT/site/filer.must.score  should be set to contain a desired value. A value 
	of 2 should prevent a node from ever winning the election, and a value of 90 should 
	cause the node to always win the election.  Setting the value high is dangerous. 

&space
&term	move : The move command is not a standard ningaui service interface command and is 
	indended to provide a manual method (not via service manager) to shift the filer 
	from one node to another.  The command must be executed as the ningaui production
	user, and must be executed on the node currently running the filer.  The filer 
	processes must be running.  
&space
	The move command validates the current environment, and if all seem right will
	cause the filer processes to stop, and will restart them on the target node. The
	process ensures that the most recent checkpoint file is transferred to the target
	node before restarting. 
&endterms


&space
&exit	Always 0.

&space
&see	ng_srvmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Mar 2008 (sd) : Sunrise. 
&term	11 Apr 2008 (sd) : Added doc on move command to scd. 
&term	16 Apr 2008 (sd) : Added a pp_build force on each node. 
&term	13 Oct 2008 (sd) : Corrected man page.
&term	29 Jan 2009 (sd) : Now sends an rmhost command to rmbt so as to direct the queued messages to 
	the repmgr on the new host once established. 
&term	20 Aug 2009 (sd0 : We now allow stop to run even if not the filer; allows us to ensure that rmbt
		is gone for cases like isolate. 
&endterms

&scd_end
------------------------------------------------------------------ */

