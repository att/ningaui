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
#!/usr/common/ast/bin/ksh
# Mnemonic:	srv_jobber.ksh 
# Abstract:	Manage the jobber service as needed by srv mgr. Provide the 
#		query/score/start/stop commands for this service.  
#		
# Date:		12 June 2006 (HBD MEJK)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  	# get standard functions
PROCFUN=${NG_STDFUN:-$NG_ROOT/lib/procfun} 
. $STDFUN
. $PROCFUN


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

	case $1 in 
		sene*)
			ng_sreq -s $jobber ping |read x		# ping is responded to as soon as the tcp buffer is read; it is not queued
			;;

		nawab)
			tollerate=6000			# nawab, when inactive, might only write every 10 minutes
			ng_nreq -t 15 -s $jobber explain foo|read x
			;;
	esac

	if [[ -n $x ]]			# assume if we got something the process is there and alert
	then
		verbose "$jobber:$1 responded"
		return 0
	fi

	verbose "no response from $jobber:$1"

	ng_rcmd $jobber 'tail -1 $NG_ROOT/site/log/'$1 | read ts junk
	now=$( ng_dt -i )
	ts=${ts%\[*}
	if (( ${ts:-0} + $tollerate  > ${now:-0} ))
	then
		verbose "logfile seems current ($(( $now - ${ts:-0} )) old); assuming alive: $jobber:$1"
	else
		verbose "from $jobber: $ts $junk"
		if [[ -n $ts ]]			# no response or bad stuff
		then
			verbose "no data received from $jobber; assuming dead"
		else
			verbose "last update to logfile is old ($(( $now - ${ts:-0} ))ts); assuming dead: $jobber:$1"
		fi
		return 1
	fi

	return 0
}


# -------------------------------------------------------------------
ustring="{start|stop|score|query}"
log2file=0				# -l sets to capture stderr into a private log in $NG_ROOT/site
jobber="$( ng_ppget -L srv_Jobber )"	# avoid stale cartulary
thisnode="$( ng_sysname )"
port=$NG_NARBALEK_PORT			# not needed, but makes testing with a standalone narbalek easier
manage_type="-u"			# by default, start scripts management type is unmanaged (srvm manager is doing it)

while [[ $1 = -* ]]
do
	case $1 in 
		-m)	manage_type="";;		# start scripts should manage restarts, turn off -u flag
		-p)	port=$2; shift;;
		-l)	log2file=1;;

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
	logf=$NG_ROOT/site/log/${argv0##*/ng_}; exec 2>>$logf
fi


case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Jobber="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	start)	
		jobber=$thisnode
		ng_sreq -e -s $jobber >/dev/null 2>&1		# it may not be up; ignore stupid errors
		ng_nreq -e -s $jobber >/dev/null 2>&1		
		tum_vname=NG_NAWAB_TUMBLER

		skip=""
		tumv=$( ng_ppget $tum_vname )
		if [[ -z $tumv ]]				# assume not set, we need to and flag nawab to start w/out ckpt
		then
			skip="-s"
			ng_ppset $tum_vname="nawab,cpt,0,10,0,24"
		fi

		log_msg "starting jobber service (nawab/seneschal)"
		ng_ppset -p $port srv_Jobber=$thisnode 			
		ng_pp_build					# important so that [sn]_start see updated value

		(
			cd $NG_ROOT/site/log
			if mv s_start s_start- 2>/dev/null
			then
				tail -5000 s_start- >s_start
			fi
			if mv n_start n_start- 2>/dev/null
			then
				tail -5000 n_start- >n_start
			fi
			rm -f n_start- s_start-
		)
		ng_s_start -v $manage_type			# -u option is unmanaged mode; s_start/n_start will not attempt restarts assuming we drive
		sleep 1
		ng_n_start $skip -v  $manage_type

		ng_log $PRI_INFO $argv0 "jobber service started on $thisnode"
		log_msg "attrs after start: $(ng_get_nattr)"
		;;
	
	stop)
		jobber=$thisnode
		echo "stopping jobber service `date`" >&2

		ng_sreq -e -s $jobber
		ng_nreq -e -s $jobber
		ng_log $PRI_INFO $argv0 "jobber service stopped on $thisnode"
		;;

	score)
		if ng_test_nattr Satellite 
		then
			echo 0
			log_msg "satellite attribute set; cannot accept service; score=0"
			cleanup 0
		fi

                if [[ -f $NG_ROOT/site/jobber.must.score ]]
                then
                        read x y <$NG_ROOT/site/jobber.must.score
			log_msg "mustscore file found: score=$x"
                        echo ${x:-0}
                        cleanup 0
                fi

		if ng_test_nattr Filer Catlogue FLOCK_WALDO
		then
			score=2
			verbose "we are either the filer or catalogue node, score to avoid"
		else
			if ng_test_nattr Jobber		# we are already signed up as the jobber
			then
				score=75		# just vote us as the winner
				echo $score
				log_msg "we believe we are the current host ($srv_Jobber)"
				cleanup 0
			else
				ng_tumbler -g -1 -p NG_NAWAB_TUMBLER | read tf
				#ng_rm_db -p | awk "/$tf/ { print \$2; exit( 0 ) }"| xargs ls | read x
				ng_rm_where -n -p $tf|gre $thisnode | read x	# this rm-where is faster than -p $this node
				if [[ -n $x ]]
				then
					score=35
					verbose "we have a current copy of the nawab log file ($x) so we score better"
				else
					score=20
				fi
			fi
		fi


		log_msg "returned score=$score"
		echo $score
		;;

	query)						# see if the daemons are still alive
		state=0
		if chk_response seneschal
		then
			state=1
		
			if chk_response nawab
			then
				state=2
			fi
		fi

		case $state in 
			2)
				verbose "both nawab and seneschal responded: report green"
				echo "green:$jobber"
				cleanup 0
				;;

			1)	verbose "one did not respond, report red"
				echo "red:$jobber"
				cleanup 0
				;;

			0)	
				if [[ $jobber == $thisnode ]]		# local puppy, not a net issue, so assume down hard
				then
					verbose "jobber is this host and no response from one/both processes; reporting red"
					echo "red:$thisnode"
					cleanup 0
				fi
				;;
		esac

		# no response, could be network, so report yellow and assume that things might clear before next test; else it becomes red
		verbose "no response could be network issue, report: yellow"
		echo "yellow:$jobber"
		;;

	no_op)				# make for easier testing
		verbose "no-op: $@" 
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_jobber:Jobber Service Interface)

&space
&synop	ng_srv_jobber {start|stop|score|query}

&space
&desc	This provides the service manager with a process that is used
	to determine the state (green, yellow, red) of the service, 
	generate the node's score with regard to the ability to host the jobber service, 
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
	$NG_ROOT/site/jobber.must.score  should be set to contain a desired value. A value 
	of 2 should prevent a node from ever winning the election, and a value of 90 should 
	cause the node to always win the election.  Setting the value high is dangerous. 
&endterms


&space
&exit	Always 0.

&space
&see	ng_srvmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Jun 2006 (sd) : Sunrise. (HBD MEJK)
&term	14 Jun 2006 (sd) : Added a bit more info to verbose messages.
&term	08 Feb 2007 (sd) : Added additional check when no response from seneschal/nawab; checks
		timestamp on log file. 
&term	21 Feb 2007 (sd) : We prune the _start logs now. 
&term	27 Feb 2007 (sd) : Better handling no abiliby to communicate with jobber during query.
&term	20 Mar 2007 (sd) : Added node info to state report.
&term	18 Apr 2007 (sd) : Seneschal is now pinged as a query rather than asked to explain something. This
		should result in quicker responses when seneschal is busy as the request is not put onto 
		a work queue.
&term	27 Apr 2007 (sd) : Fixed problem if variable undefined in expression. 
&term	08 Aug 2007 (sd) : Changed FLOCK_waldo to FLOCK_WALDO to prevent being assigned to the node that 
		is running that service. 
&term	01 Nov 2007 (sd) : Fixed typo in check response function.
&term	03 Dec 2007 (sd) : Added self initialisation. 
&term	21 Feb 2007 (sd) : Ppget call must now have -L option so this can be used by ng_setup.
&term	30 Sep 2009 (sd) : Corrected typo in var set by -m option. 
&endterms

&scd_end
------------------------------------------------------------------ */

