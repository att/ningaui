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
# Abstract:	Start tp_changer.
#		
# Author:	Andrew Hume
# ---------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

logf=$NG_ROOT/site/log/tp_cstart		# start wrapper log file
wlogf=$NG_ROOT/site/log/tp_changer		# actual log file

if ! [[ "$@" = *-e* || "$@" = *-f* || "$@" =  *-\?* || "$@" = *--man* ]]
then
	log_msg "detaching from controlling tty; script messages going to $logf; tp_changer messages to $wlogf"
	$detach
	exec >$logf 2>&1
	log_msg "start wrapper script begins"
fi

# -------------------------------------------------------------------
ustring="-v"

logd=$NG_ROOT/site/log
wdir=$NG_ROOT/site/tape
vflag="-v"			# ensure that we have at least one level on
stop=0

while [[ $1 = -* ]]
do
	case $1 in 
		-e)	stop=1;;

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

stuff=`ng_ps |grep -v grep | grep  " ng_tp_changer "`
if [[ $stop -gt 0  ]]
then
	verbose "stoping ng_tp_changer and ng_wstart"
	ng_log $PRI_INFO $argv0 "stopping tp_changer: `id`"
	ng_goaway  $argv0		# cause running wstart to quit too; BEFORE sending -e
	ng_tp_tcreq exit		# force tp_changer termination
	cleanup 0
fi

if [[ -n $stuff ]]
then
	log_msg "seems that tp_changer is already running; not started"
	log_msg "$stuff"
	cleanup 2
fi


last_start=0		# time of last start so we dont roll the log file off if we keep failing 
delay=1

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
		ng_roll_log -v -n 3 tp_changer			# keep three copies, gzipped
		delay=30					# bump delay back to minimum
		log_msg "logs rolled"
	else
		log_msg "restarting tp_changer in $delay seconds"
		sleep $delay
		delay=$(( ($delay * 25)/10 ))			# if we crap out really soon, then wait longer
	fi

	if [[ $delay -gt 900 ]]
	then
		delay=900			# enforce sanity 
	fi

	last_start=$now
	log_msg "starting tp_changer"
	ng_log $PRI_INFO $argv "starting tp_changer"

	if ! cd $wdir
	then
		log_msg "unable to switch to "$wdir" directory; attempting to create"
		if ! mkdir $wdir
		then
			log_msg "unable to create "$wdir" directory; giving up"
			cleanup 1
		fi
	fi
	
	ng_tp_changer $vflag TLS-412180 > $wlogf 2>&1  

	rc=$?
	log_msg "tp_changer terminated with exit code of: $?"

done

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_wstart:Start Woomera)

&space
&synop	ng_tp_cstart [-e] [-v]

&space
&desc	This script starts and optionally stops ng_tp_changer.  After starting tp_changer, 
	&this remains active (detached from the tty device used to start it) and 	
	will restart tp_changer should it stop. 
&space
&subcat Limit Initialisation
	&This will extract all cartulary variables of the syntax &lit(WOOMERA_)&ital(name)
	and pass these to tp_changer as limit commands.  The resoruce used on the limit 
	command is the string that follows the &lit(WOOMERA_) portion of the cartulary 
	name.  Thus the cartulary variable &lit(WOOMERA_RM_SEND) will set the resource limit 
	for &lit(RM_SEND).
&space
	Cartulary variables that are established after woomer has been started will not 
	be used until tp_changer is stopped and restarted.  The cluster initialisation script
	(ng_site_prep) is generally responsible for setting local, node specific, limits
	based on the CPU resources and other hardware specific aspects of the node. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-e : Exit. Causes a running invocation of both tp_changer and &this to exit.
&space
&term	-v : Verbose. By default tp_changer is started with a single -v flag.  Adding one 
	or more -v flags to the &this command line increases the number of flags passed
	to tp_changer. 
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
&see	&ital(ng_tp_changer), &ital(ng_seneschal), &ital(ng_nawab), &ital(ng_ppset),
	&ital(ng_site_prep)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Jul 2003 (sd) : Conversion into the ningaui start up methodology.
&term	17 May 2006 (sd) : Remove assumption that NG_ROOT is /ningaui if not defined. 
&endterms

&scd_end
------------------------------------------------------------------ */
