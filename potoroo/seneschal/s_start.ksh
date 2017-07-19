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
#
# Mnemonic:	ng_s_start
# Abstract:	Starts seneschal and initialises things a bit 
# Date:		18 Oct 2001
# Author: 	E. Scott Daniels
# -------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# must do this before first trace call so that it puts things in our desired log
logf="$NG_ROOT/site/log/s_start"
altlog="$NG_ROOT/site/log/s_start.$$"
if [[ -f $logf && ! -w $logf ]]
then
	ng_log $PRI_WARN $argv0 "cannot write to log ($logf) switching to $altlog"
	logf="$altlog"
fi

if ! [[ "$@" = *-e* || "$@" = *-f* || "$@" =  *-\?* || "$@" = *--man* ]]
then
	if ! amIreally $NG_PROD_ID
	then
		log_msg "this programme must be executed using the user id: $NG_PROD_ID"
		cleanup 2
	fi

	log_msg "detaching from controlling tty; messages going to $logf"
	$detach
	exec >$logf 2>&1
fi

trace_log=$logf
$trace_on

# ----------------------------------------------------------------------------
function abort
{
	$trace_on
	
	typeset err=$1
	shift
	
	if [[ $err -gt 0 ]]
	then
		ng_log ${err:-$PRI_ERROR} $argv0 "abort: seneschal not started: $1"
	fi

	echo "$1" >/dev/fd/2
	cleanup 1
}

function start_it
{
	$trace_on 

	. $CARTULARY			# pick up changes before starting anew 

	if [[ $trace -gt 0 ]]
	then
		set -x
	fi

	log_msg "starting seneschal..."
	ng_log $PRI_INFO $argv0 "starting ng_seneschal"

	if [[ ! -d $NG_SENESCHAL_LAIR  ]]
	then
		if ! mkdir -p $NG_SENESCHAL_LAIR
		then
			ng_log PRI_CRITICAL $arv0 "could not make seneschal lair directory: $NG_SENESCHAL_LAIR"
			cleanup 1
		fi
	fi

	if cd $NG_SENESCHAL_LAIR  
	then
		ng_seneschal -f $cmd_v >>$slog 2>&1 &
		pid=$!			# for wait later
		sleep 5			# allow it to open network things and initialise

		c=`ng_ps | egrep -v "vi.*seneschal|grep|ng_s_seneschal" |  grep -c "seneschal"`
		if [[ $c -gt 0 ]]
		then
			if [[ -f pause ]] 
			then
				log_msg "pause file found, pausing seneschal"
				ng_sreq pause
			fi

			if [[ -f suspend ]]		# suspend any nodes before setting loads
			then
				while read x
				do
					if [[ $x != "#"* ]]
					then
						log_msg "$x has been suspended; found entry in $NG_SENECHAL_LAIR/suspend"
						ng_sreq load ${x:-ning99} 0	# ensure seneschal knows the node
						ng_sreq suspend ${x:-ning99} 1
					fi
				done <suspend

				ng_sreq dump 30
			fi

			ng_sreq limit RSmaster -1			# master resource - unbound (-1)

			if [[ -f limit ]]
			then
				while read x
				do
					if [[ $x != "#"* ]]
					then
						log_msg "setting limit: $x"
						ng_sreq limit ${x:-RSmaster -1}
					fi
				done <limit
			fi

			if (( $wait4exit  > 0 ))
			then
				ng_ppset srv_Jobber=$thishost 		# if we are waiting, then we assume we must set this
				wait $pid
				rc=$?
				ng_log $PRI_INFO $argv0 "seneschal stopped: exit code was: $rc"
				log_msg "exit code from seneschal was: $rc"
			else
				rc=0
				log_msg "unmanaged mode: not waiting for seneschal; s_start will terminate"
				return 0 
			fi
			
		else
			log_msg "start failed. See $slog for info"
			ng_log $PRI_WARN $argv0 "seneschal did not start. see $slog for info"
			return $wait4exit			# if 1, then we loop and try again; if 0 then we let service manager deal with it
		fi

	
	else
		ng_log $PRI_CRITICAL $argv0 "could not change to NG_SENESCHAL_LAIR: $NG_SENESCHAL_LAIR"
		cleanup 2
	fi

	return 1
}

# -----------------------------------------------------

# MUST do all of this before we parse opts/parms as we need the command line ($*) intact
#
id -un | read myuser
if [[ $myuser = "root" ]]
then
	echo "su-ing to $NG_PROD_ID to run $argv0 $@"
	args="$@"				# must do it this way or su gets confused
	su -l $NG_PROD_ID -c "$argv0 $args"  	# if root, start as ningaui
	cleanup $?
fi

# ------------- initialise and parse the command line -----------------
ustring="[--man] [-f] [-m masterlogname] [-p] [-r] [-s] [-u] [-v]"
logd=$NG_ROOT/site/log
slog=$logd/seneschal		# seneschals work area (for purge)

thishost=`ng_sysname -s`
verbose=0
cmd_v=""		# command verbose (-v) for passing on seneschal command
stop=0			# set to stop if running
recycle=1		# start seneschal even if stopping
purge=0			# purge the log file if -p
wait4exit=1		# old school - we wait for sene to die and restart (-u [unmanaged] turns this off)

if [[ -z $NG_SENESCHAL_LAIR ]]
then
	export NG_SENESCHAL_LAIR=${NG_SENESCHAL_LAIR:-$NG_ROOT/site/seneschal}
	ng_ppset NG_SENESCHAL_LAIR="$NG_SENESCHAL_LAIR"
fi

parms="$*"

while [[ $1 = -* ]]
do

	case $1 in 
		-e)	stop=1
			recycle=0
			ng_goaway  $argv0	# cause running s_start to quit too
			;;
		-f)	;;			# causes skip of detach earlier; must ignore here
		-p)	purge=1;;
		-r)	stop=1;;
		-s)	suspend="$2$suspend "; shift;;
		-u)	wait4exit=0;;
		-v)	verbose=1; cmd_v="-v $cmd_v" ;;
		--man) show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	usage; cleanup 1;;
	esac

	shift
done





c=`ng_ps | gre -v "gre|ng_s_seneschal" |  grep -c "ng_seneschal"`
if [[ $stop -gt 0  ]]
then
	if [[ $c -gt 0 ]]
	then
		verbose "stoping..."
		ng_log $PRI_INFO $argv0 "stopping seneschal: `id`"
		ng_goaway $argv0		# signal the s_start wrapper to leave on exit of seneschal
		ng_sreq -e
	else
		verbose "was not running"
	fi

	cleanup 0
fi

if [[ $recycle -le 0 ]]		# time to exit if not starting it back up
then
	cleanup 0
fi

if [[ $c -gt 0 ]]
then
	abort $PRI_WARN "seneschal is already running"
fi

if [[ $purge -gt 0 ]]		# purge log if necessary
then
	>$logf			# our log file (s_start)
	mv $slog ${slogf}-	# seneschal log file
fi

for s in $suspend
do
	if ! grep ${s:-junk} $NG_SENESCHAL_LAIR/suspend >/dev/null 2>&1
	then
		echo $s >>$NG_SENESCHAL_LAIR/suspend
	fi
done

delay=45
ng_dt -i |read last_start
ng_goaway -r $argv0			# remove any left over goaway file that was there
last_roll=$last_start			# so we prevent roll if started frequently

ng_roll_log $cmd_v seneschal

while ! start_it		
do
	if ng_goaway -c $argv0
	then
		ng_log "exiting, found goaway file (seneschal not restarted)"
		ng_goaway -r $argv0			# leave things tidy
		cleanup 0
	fi

	log_msg "beginning the restart process..."

	ng_dt -i | read now
	if [[ $(( $now - $last_roll )) -gt 36000 ]]		# roll the log only when last start more than 1hr ago
	then
		log_msg "rolling logs"
		ng_roll_log $cmd_v seneschal
		last_roll=$now
	else
		log_msg "logs not rolled, were rolled a mere $(( ($now - $last_roll)/10 )) seconds ago"
	fi

	if [[ $(( now - $last_start )) -lt 3000 ]]	# not up 5 minutes even, set delay start
	then
		log_msg "increasing delay, last start only $(( $now - $last_start )) seconds ago"
		delay=$(( ($delay * 15)/10 ))		# increase delay 1.5 times
	else
		delay=45				# it was up a while, reset delay to default
	fi

	if [[ $delay -gt 900 ]]
	then
		delay=900			# sanity
	fi

	log_msg "waiting $delay seconds before restarting"
	sleep $delay
	ng_dt -i |read last_start
done

cleanup 0

exit 0
&scd_start
&doc_title(ng_s_start:Start/stop script for Seneschal)

&space
&synop	ng_s_start [--man] [-v]

&space
&desc	&This is a small wrapper script used to start and stop 
	the &ital(seneschal). This script continues to run and will 
	restart &ital(seneschal) if it stops or dies.  This script 
	automatically detaches from the controlling tty device so there
	is no need to run the script asynchronously.
&space
	Once &ital(seneschal) has been started, &this will attempt to 
	automatically suspend, and set execution limits, for nodes in 
	the current cluster.  Per node job limites (want values) are 
	sent to seneschal for each node in the cluster.  The value sent is
	the current value of the cartulary varaible &lit(NG_SENE_DEF_LOAD) or
	2 if the cartulary variable is not set. Nodes are suspended if they
	are listed in  
&space
&subcat The Lair
	The directory defined by &lit(NG_SENESCHAL_LAIR) is used as a current
	working directory by &this, and is expected to contain several administrative
	files if needed. These files are:
&begterms
&term	pause : If this file is present, seneschal is paused immediately upon start.
&space
&term	suspend : This file is expected to contain a list of nodes that are suspended
	as seneschal starts up. 
&space
&term	limit : This file may contain nodename limit pairs. They will be sent using 
	&lit(ng_sreq limit) commands to seneschal once seneschal has started. 
&endterms

&space
&opts	The following options are recognised when entered on the 
	command line:
&space
&begterms
&term	-e : Exit. Causes both &ital(ng_seneschal) and &this to exit. Under 
	normal conditions, when &ital(ng_seneschal) stops, &this will attempt 
	to restart it.
&space
&term	-f : Keep &this from detaching (foreground mode).
&space
&term	-r : Stop, then restart &lit(ng_seneschal) (recycle).
&space
&term	-s node : Suspend node.  Causes &itlal(ng_seneschal) to suspend sending jobs 
	to the node immediately after seneschal is started.
&space
&term	-u : Unmanaged mode.  When this is set, &this will NOT attempt to keep seneschal 
	running after it exits.  The assumption is that this flag is set when seneschal 
	is being managed by service supervision. 
&space
&term	-v : Verbose mode.
&space
&term	--man : Generate the  man page info on standard out.
&endterms

&space
&exit	&This will put itself into the background and wait for &lit(ng_seneschal)
	to complete. If the completion code returned by &lit(ng_seneschal) is 
	not zero, then it will restart &lit(ng_seneschal) believing that the
	programme termination was not desired.  

&files
	The following files are important to &this:
&lgterms
&term	$NG_ROOT/site/log/seneschal.log : The standard output and standard error
	from &lit(ng_seneschal) are directed to this file. A timstamp is written 
	to the file each time &lit(ng_seneschal) is started. 
&space
&term	$NG_ROOT/site/log/s_seneschal : This is the log file where &this 
	will write its messages. It is maintained separately from the &ital(ng_seneschal)
	log file.
&space
&term	$NG_SENESCHAL_LAIR/suspend : Contains a list of node names that should be suspended
	when &ital(ng_seneschal) is started.  Any node supplied with the suspend option 
	on the command line (-s) is added here.  Comment lines are ignored.
&endterms
&space
&see	ng_sreq ng_seneschal

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	21 Sep 2001 (sd) : First acorns off of the tree.
&term	16 Apr 2003 (sd) : Adjusted the suspension logic, and timeout between restarts.
		Added -s option.
&term	10 Jul 2003 (sd) : Added message to indicate the exit code from seneschal,
	and to use ng_ps. Made log file more consistant with other start wrappers.
&term	25 Aug 2003 (sd) : Mod to set the jobber var in pinkpages. Corrected two log messages.
&term	03 Oct 2003 (sd) : Changed members to use -r rather than -n
&term	14 Mar 2003 (sd) : Corected log message after seneschal exited.
&term	17 Aug 2004 (sd) : Now uses NG_SENE_DEF_LOAD to set loads on nodes. If not in cartulary,
	then the old default of 2 is used. Also to touch goaway when -e given.
&term	28 Mar 2004 (sd) : Retries several times if a members list comes back empty.
&term	29 Mar 2004 (sd) : Extended amount of time it waits for member list.
&term	14 May 2005 (sd) : Changed log msg error level from critcal to warning.
&term	16 May 2005 (sd) : Removed error message on restart -- now just generates an info msg to the master log.
&term	28 Jun 2005 (sd) : Upped the number of tries we make to find a members list.
&term	14 Nov 2005 (sd) : Now uses narbalek to find members of the cluster (ng_members -N)
&term	26 Dec 2005 (sd) : Looks for ng_seneschal rather than seneschal for running jobs.  Thus it will not 
	pick up any running job that has $TMP/seneschal/... on the ocmmand line.
&term	16 Mar 2006 (sd) : No longer neecessary to set loads for nodes; seneschal ckpts the info now. 
&term	20 Mar 2006 (sd) : Now ensures that the script is executed by ningaui.
&term	12 Jun 2006 (sd) : Added -u (unmanaged) flag (for service manager starts).
&term	01 Sep 2006 (sd) : This script no longer use NG_SENE_DEF_LOAD; Seneschal saves the current 
		values at runtime and uses those. When a new node is encountered, the variable is 
		still referenced. 
&term	27 Mar 2007 (sd) : Fixed man page. 
&term	01 Oct 2007 (sd) : Tweeked man page. 
&term	15 Oct 2007 (sd) : Mod to use NG_PROD_ID rather than ningaui.
&term	26 Oct 2007 (sd) : This no-longer sets srv_Jobber if we beileve service manager is running the Jobber.
&term	28 Mar 2008 (sd) : Eliminated direct refernce to cartulary.
&endterms

&scd_end
------------------------------------------------------------------ */

