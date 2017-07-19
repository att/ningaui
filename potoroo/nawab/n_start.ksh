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
#    #! will be inserted automatically by ng_install
#
# Mnemonic:	ng_n_start - start nawab
# Abstract:	Starts nawab with a checkpoint and recovery file if one 
#		exists.
# Date:		28 Feb 2002
# Author: 	E. Scott Daniels
# -------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun}  # get process oriented functions
. $STDFUN
. $PROCFUN

# must do this before first trace call so that it puts things in our desired log
logf="$NG_ROOT/site/log/nawab"
altlog="$NG_ROOT/site/log/nawab.$$"

if [[ -f $logf && ! -w $logf ]]
then
	ng_log $PRI_WARN $argv0 "cannot write to log ($logf) switching to $altlog"
	logf="$altlog"
fi
trace_log=$logf
$trace_on

# ----------------------------------------------------------------------------
function abort
{
	$trace_on
	
	typeset err=${1:-$PRI_ERROR}
	shift
	
	if [[ $err -gt 0 ]]
	then
		ng_log $err $argv0 "abort: nawab not started: $1"
	fi

	log_msg "$1" >/dev/fd/2
	cleanup 1
}

function start_it
{
	$trace_on 

	typeset cf=""
	typeset rfile=$TMP/nawab_rfile.$$ 
	typeset rsfile=$TMP/nawab_rfile_sort		# sorted recovery file; do NOT make these PID$$.* files as they must remain after this script exits

	>$rsfile

	if [[ $trace -gt 0 ]]
	then
		set -x
	fi

	verbose "starting..."
	log_msg "starting: `date`" >>$logf
	ng_log $PRI_INFO $argv0 "starting ng_nawab"

	if (( $initial_ckpt > 0 ))		# must find a checkpoint and gather roll forward (recovery) data
	then
		# suss chkpt info from log; build recovery file with all stuff after last ckpt made message
		# we look even if initial-ckpt is 0 because we need to know what tumbler info is to be set to
		# rsfile contains log messages that allow nawab to roll forward from the chkpt info
		# nawab_status: are messages put into the roll foward file and nawab_ckpt: are messages that 
		# list the checkpoint names created.
	
		if [[ -n $target_ckptfn ]]
		then
			verbose "searching for user supplied, specific, checkpoint data: ${target_ckptfn##*/}"
			ng_ckpt_prep -v -M 11 -f 10 $targ_opt -r $rsfile "nawab_status:" "nawab_ckpt:" "${target_ckptfn##*/}" | read ckptfname junk
			#ng_ckpt_prep -f 10 $targ_opt -r $rsfile "nawab_status:" "nawab_ckpt:" "${target_ckptfn##*/}" | read ckptfname junk
		else
			verbose "searching for checkpoint with pattern: $ck_pre.*$ck_suf"
			ng_ckpt_prep -M 11 -f 10 $targ_opt -r $rsfile "nawab_status:" "nawab_ckpt:" "$ck_pre.*$ck_suf" | read ckptfname junk
			#ng_ckpt_prep -f 10 $targ_opt -r $rsfile "nawab_status:" "nawab_ckpt:" "$ck_pre.*$ck_suf" | read ckptfname junk
		fi
	
		verbose "log search done. ckpt=$ckptfname "
		if [[ -n $target_ckptfn  && $target_ckptfn != $ckptfn ]]	# user gave specific name, and it was not returned by prep
		then								# we will honour it, but recovery might not be correct
			if [[ $target_ckptfn = /* ]]			# fully qualified -- use as is
			then
				ckptfname=$target_ckptfn
			else						# need to find location in repmgr space
				verbose "sussing user supplied checkpoint file chkpt file from repmgr: $target_ckptfn" 
				ng_rm_where $thishost $target_ckptfn | read ckptfname
			fi
			verbose "overriding logfile checkpoint name with user supplied name: $ckptfname"
		fi
	
		if  [[ ! -r ${ckptfname:-nosuchfile} ]]		# must use a chkpt, and what we got is no good
		then
			if [[ -n $ckptfname ]]
			then
				log_msg "abort: checkpoint file could not be read: $ckptfname"
				ng_log $PRI_ERROR $argv0 "checkpoint file was not readable, nawab not started: $ckptfname"
				cleanup 3
			fi
	
			ng_log $PRI_INFO $argv0 "checkpoint file was needed, but not found; pausing before reattempt"
			log_msg "no checkpoint file found; repmgr may not be up"
			log_msg "pausing 2 min before a retry to allow filer node to come up"
			sleep 120
			return  0
		fi


		verbose "passing in ckpt fiile name: $ckptfname"

		ckptfname="-c $ckptfname"
		if [[ -s $rsfile ]]
		then
			recovery="-r $rsfile"				# parm for nawab
			verbose "checkpoint roll forward info being passed in has $(wc -l <$rsfile) records"
		else
			verbose "no checkpoint roll forward information was needed"
		fi
	else
		verbose "checkpoint and recovery information ignored (-s option supplied)"
		initial_ckpt=1				# turn on for any other calls
		ckptfname=""
		recovery=""
	fi

	# we run nawab in foreground as this script stays running and restarts if exit is bad
	if (( $wait4exit > 0 ))
	then
		set -x
		ng_nawab $vflag -f  $ckptflag  $port $recovery $ckptfname -t $tumbler_vname   >>$logf 2>&1
		rc=$?
		
	else
		set -x
		ng_nawab $vflag $ckptflag  $port $recovery $ckptfname -t $tumbler_vname   >>$logf 2>&1
		# if we fail, then we should mark something such that this node loses the election next time round
		return 0
	fi
			

	set +x
	ng_log $PRI_INFO $argv0 "nawab exit code was: $rc"
	verbose "exit code was: $rc"

	target_ckptfn=""			# target applies only to first start
	target_opt=""
	return $rc		
}

# -----------------------------------------------------
logd=$NG_ROOT/site/log
TMP=${TMP:-/tmp}

# MUST do all of this before we parse opts/parms as we need the command line ($*) intact
#

if ! [[ "$@" = *-e* || "$@" = *-f* || "$@" =  *-\?* || "$@" = *--man* ]]		# dont detach if stopping, help,  or forced "foreground"
then
	if ! amIreally $NG_PROD_ID
	then
		log_msg "this command must be executed as user id: $NG_PROD_ID"
		cleanup 2
	fi

	detach_log=$logd/n_start
	log_msg "$argv0: detaching. stdout/err redirected to: $logd/n_start"
	$detach
	exec >>$logd/n_start 2>&1
	log_msg "n_start script started..."
fi

# ------------- initialise and parse the command line -----------------
ustring="[--man] [-c ckptfile] [-f] [-m masterlogname] [-N] [-P port] [-p] [-r] [-s] [-u] [-v]"
wdir=$NG_ROOT/site
logf=$logd/nawab
mlog="$NG_ROOT/site/log/master"

thishost=`ng_sysname -s`
verbose=0
vflag=""		# command verbose 
stop=0			# set to stop if running
testing=0		# -t sets on to start nawab from local directory
purge=0			# purge the log file if -p
target_ckptfn=""	# -c overrides this and forces recovery from this file -- even if its not the last
initial_ckpt=1		# read initial checkpoint
ckptflag="";		# -N sets to the dont do any checkpointing when nawab invoked
tumbler_vname=NG_NAWAB_TUMBLER		# variable with tumbler info
ck_pre=nawab		# default checkpoint filename pre/suffix strings (if tumbler_vname not defined)
ck_suf=cpt	
wait4exit=1		# old way -- script waits and restarts (-u [unattended] turns this off)

while [[ $1 = -* ]]
do

	case $1 in 
		-c)	target_ckptfn="$2"; shift;;
		-c*)	target_ckptfn="${1#??}";;
		-e)	stop=1;;
		-f)	;;			# causes skip of detach earlier; must ignore here
		-m)	mlog=$2; shift;;
		-N)	ckptflag="-n";;			# disables checkpointing completely
		-P)	port="-p $2"; shift;;
		-p)	purge=1;;
		-s)	initial_ckpt=0;;		# skips initial checkpoint
		-t)	testing=1;;
		-T)	tumbler_vname=$2; shift ;;
		-u)	wait4exit=0;;
		-v)	verbose=1; vflag="-v $vflag" ;;

		--man) show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	echo "dont get: $1"; usage; cleanup 1;;
	esac

	shift
done

if [[ -z NG_NAWAB_TUMBLER ]]		# no tumbler defined -- set default
then
	export NG_NAWAB_TUMBLER="nawab,cpt,0,10,0,20"
	ng_ppset NG_NAWAB_TUMBLER="NG_NAWAB_TUMBLER"
	initial_ckpt=0						# assume first invocation on cluster -- no ckpt expected
fi

eval echo \$$tumbler_vname | IFS="," read ck_pre ck_suf junk1 
if [[ -z $ck_pre || -z $ck_suf ]]
then
	log_msg "abort: checkpoint info invalid or missing from tumbler var ($tumbler_vname); var might be null"
	cleanup 1
fi

if [[ -n $target_ckptfn ]]
then
	targ_opt="-t $target_ckptfn"		# target filename for the checkpoint search
fi

ng_goaway -r ${argv0##*/}

if [[ $testing -lt 1 ]] && ! cd $wdir
then
	abort $PRI_ERROR "unable to switch to: $wdir" 
fi

if [[ $stop -gt 0  ]]
then
	verbose "stoping..."
	ng_goaway ${argv0##*/}					# assume there is a running n_start that needs to exit too
	ng_log $PRI_INFO $argv0 "stopping nawab: `id`"
	ng_nreq $port -e
	cleanup 0
fi

ng_ppset srv_Jobber $thishost

if ng_ps | gre ${NG_PROD_ID:-ningaui} | gre -v $$ | is_alive "ng_n_start"			# any other n_start but us running?
then
	abort $PRI_WARN "ng_n_start is already running"
else
	verbose "we seem to be the only copy running"
fi


if [[ $purge -gt 0 ]]		# purge log if necessary
then
	>$logf
fi

target_ckptfn=${target_ckptfn##*/}		# just the basename if they gave us everything
delay=45


last_roll=0				# time we last rolled the log (not more frequently than 1 hour)

# start nawab and wait for it to stop, once down we restart it unless our goaway file has 
# been touched
while true  
do
	ng_ppget srv_Jobber |read jobber
	if [[ $jobber != "$thishost" ]]
	then
		log_msg "this host no longer the jobber host, not starting nawab and exiting"
		cleanup 0
	fi

	ng_dt -i |read now
	if [[ $(( $last_roll + 36000 )) -lt $now ]]		# last roll more than 1 hour ago (tenths of seconds)
	then
		ng_roll_log $vflag nawab
		last_roll=$now
	fi

	last_start=$now
	start_it		

	if ng_goaway -c ${argv0##*/}
	then
		log_msg "found goaway file, exiting."
		ng_goaway -r $argv0			# clean up the go away file
		cleanup 0
	fi

	if (( wait4exit == 0 ))
	then
		log_msg "nawab started, n_start exits now. we are not managing restarts -- assume supervision does that now"
		cleanup 0
	fi


	# old school -- we manage nawab and try to keep it alive 

	ng_dt -i |read now
	if [[ $(( $last_start + 3000 )) -gt $now ]]		# did not stay up very long, wait a while
	then
		delay=$(( ($delay * 25)/10 ))
	else
		delay=45
	fi

	if [[ $delay -gt 900 ]]
	then
		delay=900				# more than 15 min is stupid
	fi

		
	log_msg "starting nawab after a delay of $delay seconds"
	sleep $delay
	target_ckptfn=""				# only use the target on the initial startup
	ng_log $PRI_INFO $argv0 "restarting nawab"
done

cleanup 0

exit 0
&scd_start
&doc_title(ng_n_start:Start/stop script for Nawab)

&space
&synop	ng_n_start [--man] [-c ckptfile] [-f] [-m masterlogname] [-P port] [-p] [-r] [-s] [-v]

&space
&desc	&This is a small wrapper script used to start and stop 
	&ital(nawab.)
&space
	By default, &this will attempt to start &lit(ng_nawab), and will 
	abort if it believes that &lit(ng_nawab) is already running.
	Stopping and recycling &lit(ng_nawab) is also controlled using 
	&this by supplying the appropriate command line option.

&space
&opts	The following options are recognised when entered on the 
	command line:
&space
&begterms
&term	-c name : Supplies the name of a checkpoint file that should be used
	to recover the state of &ital(nawab.) If this option is not supplied, 
	the last checkpoint file logged will be used. Care should be taken when 
	using this option as all status messages found after the checkpoint file
	was created will be placed into the recovery file. The checkpoint file 
	name "tumblers" are set based on the last checkpoint file in the log 
	and not the file supplied.  The checkpoint file name supplied, must 
	be a file that was created and registered by &ital(nawab) such that 
	an appropriate log entry for the file can be found.
&space
&term	-e : Stop nawab and do not restart it.  Causes &this to terminate as well.
&space
&term	-f : Stay in foreground. Unless this option is supplied, &this will 
	detach itself from the user environment before beginning its work. Supplying
	this option causes &this to remain attached to the current &lit(tty) 
	device and allow standard error messages to be directed to the &lit(tty).
&space
&term	-m log : Defines the log that will be searched when &lit(ng_nawab) is started.
	From this log file the name of the last checkpoint file is determined
	and any stream information records that appear in the log after the 
	last successful checkpoint message are used to create the recovery file 
	that &lit(ng_nawab) will use to restore its last known state. 

&space
&term	-P port : Supplies the port number to give to &lit(ng_nawab). If not supplied
	&lit(ng_nawab) will use the &lit(NG_NAWAB_PORT) cartulary variable.
&space
&term	-p : Causes the log file created by &this to be purged.
&space
&term	-r : Stop, then restart &lit(ng_nawab) (recycle).
&space
&term	-s : Skip checkpoint recovery. When this option is supplied, &ital(nawab) is 
	started without supplying a checkpoint or recovery file. The checkpoint 
	name "tumblers" are set based on the last checkpoint file in the log. 
&space
&term	-u : Unmanaged. This script does not try to restart nawab when it exits. 
	It is assumed that nawab will be started this way when being managed with 
	service supervision.
&space
&term	-v : Verbose mode.
&space
&term	--man : Generate the  man page info on the standard out device.
&endterms

&space
&exit	&This will put itself into the background and wait for &lit(ng_nawab)
	to complete. If the completion code returned by &lit(ng_nawab) is 
	not zero, then it will restart &lit(ng_nawab) believing that the
	programme termination was not desired.  

&files
	The following files are important to &this:
&lgterms
&term	$NG_ROOT/site/log/nawab : The standard output and standard error
	from &lit(ng_nawab) are directed to this file. A timstamp is written 
	to the file each time &lit(ng_nawab) is started. 
&endterms
&space
&see	&ital(ng_seneschal)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	28 Feb 2002 (sd) : First acorns off of the tree.
&term	04 Jun 2002 (sd) : Made options consistant with seneschal start options.
&term	24 Jun 2002 (sd) : Implemented the Andrew ckpt -r search technique.
&term	10 Jul 2003 (sd) : Made start/stop more consistant with other scripts.
		Converted ps calls to ng_ps
&term	27 Mar 2004 (sd) : Ensured that Jobber is set after test for -e.
&term	21 Aug 2004 (sd) : Converted chekpoint prep to use ng_ckpt_prep script as 
	the original logic was not always finding the correct file and was slow.
&term	01 Nov 2004 (sd) : Corrected typo in verbose message, corrected check for readable 
	checkpoint file if -s given."
&term	09 May 2005 (sd) : Added retry if no checkpoint name was found -- assumes that 
	nawab	it is likely caused by repmgr, or filer node, being down.
&term 	02 Aug 2005 (sd) :  Better testing for process already running.
&term	05 Aug 2005 (sd) : Fixed typo in check to see if we were already running.
&term	20 Mar 2006 (sd) : Now ensures that the script is started by ningaui.
&term	31 Mar 2006 (sd) : Pulled out an old id == root check -- we used to su to ningaui
		but this should never be run from root.
&term	12 Jun 2006 (sd) : Conversion to using ng_tumbler and pinkpages variables to 
		drive the checkpoint file tumbler info. Added -u (unmanaged) option for 
		service manager starts.
&term	14 Jun 2006 (sd) : Added verification of information from tumbler var.
&term	13 Sep 2006 (sd) : Fixed calls to grep (converted to gre)
&term	03 Oct 2006 (sd) : Adjusted the checkpoint search to use just the chkpt filename
		returned by ng_ckpt_prep (it used to return tumbler info which we now 
		get from pinkpages).
&term	10 Oct 2006 (sd) : Fixed typo in referencing target checkpoint variable name.
&term	19 Sep 2007 (sd) : Took first step toward moving toward using the new md5 feature
		in ng_ckpt_prep. 
&term	15 Oct 2007 (sd) : Modified to use NG_PROD_ID rather than assuming ningaui.
&term	19 Oct 2007 (sd) : Fixed bug with passing the roll-forward file to nawab on start.
&term	29 Oct 2007 (sd) : Turned on sussing out checkpoint with the md5 value.  
&term	03 Dec 2007 (sd) : Added self initialisation of tumbler pp var if not set. 
&term	15 Sep 2009 (sd) : Corrected a bug with roll forward file which was being delted 
		as the script exited; ok if the script is managing and restarting nawab, 
		but not ok if service manager is doing the work. 
&endterms

&scd_end
------------------------------------------------------------------ */

