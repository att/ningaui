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
# Mnemonic:	ng_wstart
# Abstract:	Start woomera.
#		
# Date:		Revision: 10 Jul 2003
# Author:	Andrew Hume
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

#  remove everything but a select few from the environment
# we now use the wash_env in stdfun
function old_wash_env
{
	# we keep only these from being washed away;  
	DRY_CLEAN_ONLY="^TZ=|^CVS_RSH=|^DISPLAY=|^HOME=|^LANG=|^DYLD_LIBRARY_PATH|^LD_LIBRARY_PATH=|^LOGNAME=|^MAIL=|^NG_ROOT=|^TMP=|^PATH=|^PS1=|^PWD=|^SHELL=|^TERM=|^USER="
	env | gre -v $DRY_CLEAN_ONLY | while read x
	do
		unset ${x%%=*}
	done
}

# -----------------------------------------------------------------------------------

logf=$NG_ROOT/site/log/wstart		# start wrapper log file
wlogf=$NG_ROOT/site/log/woomera		# woomera log file

if ! [[ "$@" = *-e* || "$@" = *-f* || "$@" =  *-\?* || "$@" = *--man* ]]
then
	if ! amIreally ${NG_PROD_ID:-ningaui}
	then
		log_msg "this command must be executed as user id: $NG_PROD_ID"
		#cleanup 2
	fi

	log_msg "detaching from controlling tty; script messages going to $logf; woomera messages to $wlogf"
	$detach
	exec >>$logf 2>&1
	log_msg "wstart begins execution"
fi

# should be converted to use: ng_roll_log -v -n 3 woomera
function roll_log
{
	keep=$(( ${1:-3} - 1 ))
	if cd $NG_ROOT/site/log
	then
		while [[ $keep -gt 0 ]]
		do
			old=woomera.$(($keep - 1 )).gz
			new=woomera.$keep.gz
			cp -p $old $new  2>/dev/null

			keep=$(( $keep-1 ))
		done
			
		mv woomera woomera.0
		rm -f woomera.0.gz
		gzip woomera.0 &		# move off and do in parallel for faster start
	fi
}

# -------------------------------------------------------------------
ustring="-v"

logd=$NG_ROOT/site/log
vflag=""
stop=0

while [[ $1 = -* ]]
do
	case $1 in 
		-f)	;;			# ignore -- keeps in foreground
		-e)	stop=1;;

		-v)	verbose=1; 
			vflag="$vflag-v "
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

if [[ $stop -gt 0  ]]
then
	verbose "stoping ng_woomera and ng_wstart"
	ng_log $PRI_INFO $argv0 "stopping woomera: `id`"
	ng_goaway  $argv0	# cause running wstart to quit too; BEFORE sending -e
	ng_wreq -e		# force woomera termination
	cleanup 0
fi


#ng_ps | awk -v pid="$$" '
#	/awk/ { next; }
#	/wstart.*DETACHED/ {			# must include DEATCHED because weve had cases where our parent still existed
#		if( $2 != pid )			# at this point!  If the pid is not ours, assume already running
#			print;
#	}
#'| read stuff

ng_ps | ng_proc_info ${argv0##*/} DETACHED | read stuff		# all pids for wstart processes that are marked detached
if [[ $stuff != $$ ]]						# more than just our pid
then
	log_msg "found other wstart processes (${stuff//$$/ }) in ps list; pausing a few seconds and retrying"
	sleep 60							# we have seen 'shadows' on the sun nodes that go away
	ng_ps|ng_proc_info ${argv0##*/} DETACHED | read stuff		# all pids for wstart processes that are marked detached
	if [[ $stuff != $$ ]]
	then
		log_msg "seems that ${argv0##*/} is already running; not starting"
		log_msg "ps info: $stuff"
		cleanup 2
	fi
fi

last_start=0			# time of last start so we dont roll the log file off if we keep failing 
delay=30			# time to pause before attempting restart

wash_env			# ditch most things from the environment except NG_ROOT
if [[ -z $LOGNAME ]]
then
	export LOGNAME=${NG_PROD_ID:-ningaui}		# on some nodes (sun) we dont get this when started by a root su ningaui
fi

ng_goaway -r $argv0		# if we just got started, purge any left overs!

while true
do
	if ng_goaway -c $argv0
	then
		log_msg "exiting, found goaway file"
		ng_goaway -r $argv0			# must leave things tidy
		cleanup 0
	fi

	ng_dt -i | read now
	if [[ $now -gt $(( $last_start + 36000 )) ]]		# roll the log only when last start more than 1hr ago
	then
		roll_log 3					# keep three copies, gzipped
		delay=30					# bump delay back to minimum
		log_msg "logs rolled"
	else
		log_msg "restarting woomera in $delay seconds"
		sleep $delay
		delay=$(( ($delay * 25)/10 ))			# if we crap out really soon, then wait longer
	fi

	if [[ -f $NG_ROOT/site/woomera/restart_delay ]]
	then
		read max_delay <$NG_ROOT/site/woomera/restart_delay
	else
		max_delay=300;		# some sanity -- not more than a 5min delay
	fi

	if [[ $delay -gt $max_delay ]]
	then
		delay=$max_delay			# enforce sanity 
	fi

	last_start=$now
	log_msg "starting woomera"
	ng_log $PRI_INFO $argv "starting woomera"

	if ! cd $NG_ROOT/site/woomera
	then
		log_msg "unable to switch to site/woomera directory; attempting to create"
		if ! mkdir $NG_ROOT/site/woomera
		then
			log_msg "unable to create site/woomera directory; giving up"
			cleanup 1
		fi
	fi
	
	# fish out limits from cartulary and put into req --- woo picks them up from there
	#		linux sed does not support -E; second sed not as clever, but works 
	# we fish from cartulary to allow them to change between stop and start
	# --------------------------------------------------------------------------------------
	#sed -E -n '/^#/d
	#s/#.*$//
	#/(^|[ 	])WOOMERA_/s/.*WOOMERA_([^ =	]*).*=[ 	]*"*([-0-9]*).*/limit \1 \2/p' < $CARTULARY > req

	sed   -n '/^#/d
	s/#.*$//
	s/"//g
	/[ 	]WOOMERA_/b snarf
	d; b
	: snarf
	s/.*WOOMERA_/limit /
	s/=/ /p' <$CARTULARY >req

	cp req reqx
	env >env
	ng_woomera ${vflag:--v} -s req > $wlogf 2>&1  

	rc=$?
	log_msg "woomera terminated with exit code of: $?"

done

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_wstart:Start Woomera)

&space
&synop	ng_wstart [-e] [-v]

&space
&desc	This script starts and optionally stops ng_woomera.  After starting woomera, 
	&this remains active (detached from the tty device used to start it) and 	
	will restart woomera should it stop. 
&space
&subcat Limit Initialisation
	&This will extract all cartulary variables of the form &lit(WOOMERA_)&ital(name)
	and pass these to woomera as limit commands.  The resoruce used on the limit 
	command is the string that follows the &lit(WOOMERA_) portion of the cartulary 
	name.  Thus the cartulary variable &lit(WOOMERA_RM_SEND) will set the resource limit 
	for &lit(RM_SEND).
&space
	Cartulary variables that are set/changed after woomera has been started will not 
	be used until woomera is stopped and restarted.  The cluster initialisation script
	(ng_site_prep) is generally responsible for setting local, node specific, limits
	based on the CPU resources and other hardware specific aspects of the node. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-e : Exit. Causes a running invocation of both woomera and &this to exit.
&space
&term	-v : Verbose. By default woomera is started with a single -v flag.  Adding one 
	or more -v flags to the &this command line increases the number of flags passed
	to woomera. 
&endterms

&space
&exit	&This should always exit with a good return code.

&examp
	To set a limit for a resource (RM_FS) at initialisation time the following variable should  be 
	set in the cartulary (pinkpages):

&space
&litblkb
    ng_ppset WOOMERA_RM_FS 4
&litblke
&space
	If the limit is to be applied only to a single node, then the &lit(ng_ppset) 
	command must be executed on that node, and the -l (local) flag must be supplied on 
	the commandline. 

&space
&see	&ital(ng_woomera), &ital(ng_seneschal), &ital(ng_nawab), &ital(ng_ppset),
	&ital(ng_site_prep)

&space
&mods
&owner(Scott Daniels)
&lgterms
&temr	28 Mar 2003 (sd) : added spaces in front of WOOMERA so as not to get NG_WOOMERA things
&term	12 Jul 2003 (sd) : Conversion into the ningaui start up methodology.
&term	17 Sep 2004 (sd) : No longer truncates the wstart log file.
&term	07 Feb 2005 (sd) : Added ability to set down the max delay between restart attempts
		(mostly for easier testing).
&term	25 Feb 2005 (sd) : Fixed check to prevent duplicates from running.
&term	26 Feb 2005 (sd) : Unset CDPATH before starting woomera.
&term	03 Mar 2005 (sd) : Fixed sed to deal with trailing comments
&term	02 Jun 2005 (sd) : Adjusted sed to work with linux (-E not supported)
&term	06 Jun 2005 (sd) : Now washes environment.
&term	04 Aug 2005 (sd) : Environment wash cleans everything, not just NG or FLOCK vars. 
&term	14 Aug 2005 (sd) : Added DYLD_LIBRARY_PATH to list of env vars not washed; stupid macs.
&term	21 Jan 2006 (sd) : Need to leave TZ in the laundry list of env vars not to scrub at start.
&term	20 Mar 2006 (sd) : Now ensures that ningaui is starting this.
&term	31 Mar 2006 (sd) : eliminated setting NG_ROOT from default.
&term	27 May 2006 (sd) : Changed already running check to handle case where our parent has not 
		exited after starting this as a detached process.
&term	26 Jul 2006 (sd) : Fixed setting of vflag variable.  We still ensure woomera started with 
		one -v option, but no longer add one to what the user enters. 
&term	27 Sep 2006 (sd) : Converted check for already running wstart to use new ng_proc_info
		and to retry if we think we see another; we have seen 'shadows' on the suns
		that have prevented us from starting. 
&term	28 Apr 2008 (sd) : We set LOGNAME as on some nodes (sun) we dont have this value set.
&term	04 Jun 2008 (sd) : We now use the stdfun version of wash_env as it handles readonly vars correctly.
&term	02 Jan 2009 (sd) : Corrected prod-id check.
&term	14 Oct 2009 (sd) : Increased the time we wait after noticing a 'shadow' wstart process. 5 sec wasn't
		enough in all cases; new delay is 60.  Seems only to be needed on the sun nodes.
&endterms

&scd_end
------------------------------------------------------------------ */
