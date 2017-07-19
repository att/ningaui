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
# Mnemonic:	srv_alerter.ksh 
# Abstract:	Manage the alerter service as needed by srv mgr. Provided as the 
#		score/start/query/stop commands for this simple service.  The service 
#		when started, ensures that the svr_Alerter variable is set in 
#		pinkpages.  
#		
# Date:		20 Jan 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# -------------------------------------------------------------------
exec 2>>$NG_ROOT/site/log/srv_alerter
port=$NG_NARBALEK_PORT
ustring="[-p port] [-v] {start|stop|score|query}"

while [[ $1 = -* ]]
do
	case $1 in 
		-p)	port=$2; shift;;

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


thisnode=`ng_sysname`


case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Alerter="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	start|continue)	
		ng_log $PRI_INFO $argv0 "alerter service started on $thisnode"
		log_msg "starting alerter service"
	
		log_msg "node attrs before start: `ng_get_nattr`"
		ng_ppset -p $port srv_Alerter=$thisnode 			
		log_msg "node attrs after  start: `ng_get_nattr`"
		;;

	stop)
		log_msg "stopping alerter service `date`" 
		ng_get_nattr >&2
		ng_log $PRI_INFO $argv0 "alerter service stopped on $thisnode"
		;;

	query)	
		ng_ppget srv_Alerter | read alerter

		if ng_test_nattr Alerter							# we are already signed up as the alerter
		then
			mynode=$( ng_sysname )
			if [[ $mynode == $alerter ]]
			then
				echo "green:$mynode"
			else
				echo "red:$alerter"
			fi
			cleanup 0
		fi

		ng_rcmd $alerter ng_wrapper -e TMP | read answer
		if [[ -n $answer ]]
		then
			echo "green:$alerter"
			cleanup 0
		fi

		echo "yellow: no answer from ${alerter:-srv_Alerter was not set}"
		cleanup 0
			
		;;

	score)
		if ng_test_nattr Satellite 
		then
			echo 0
			cleanup 0
		fi

                if [[ -f $NG_ROOT/site/alerter.must.score ]]
                then
                        read x y <$NG_ROOT/site/alerter.must.score			# read the value we must return and spit it out
                        echo ${x:-0}
                        cleanup 0
                fi


		ng_ppget srv_Alerter | read l
		export srv_Alerter=$l			# override what could be stale cartulary

		if ng_test_nattr Jobber Filer
		then
			score=2
			echo $score
			cleanup 0
		fi

		if ng_test_nattr Alerter				# we are already signed up as the alerter
		then
			score=75			# just vote us as the winner
			echo $score
			log_msg "we believe we are the current host ($srv_Alerter)"
			cleanup 0
		else
			score=10
		fi


		echo $(( $score ))
		;;

	*)	log_msg "unrecognised command: $*"
		cleanup 0
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_alerter:Alerter service start/stop/score)

&space
&synop	ng_srv_alerter {start|stop|score|query}

&space
&desc	This provides the service manager with a process that is used
	to generate the node's score with regard to the ability to
	host the alerter service, and with entry points to start and stop
	the service.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p port : The port to send to when communicating with narbalek.  Allows for a 
	separate black board space to test with.
&endterms


&space
&parms	Expected on the command line is an action token which is one of: start, stop, query or score.

	The action token causes the following to happen:
&begterms
&term	init : Causes the service manager pinkpages variable for this service to be defined. 
	The service manager variable consists of the command line commands to invoke the start, 
	stop, query and score functions of this service as well as election information. See 
	the ng_srvmgr manual page for the specifics about the variable.  
&space
&term	start : Initialise the service.  Registers the node with replication manager as the 
	Alerter node, and sets the srv_Alerter pinkpages variable.
&space
&term	stop : Terminates the service. No real action other than logging the event takes place
&space
&term	score : Computes the score and writes it to the standard output device. The score 
	is an integer between 0 and 100; the larger the value the more able the node is to 
	accept the service.  A score of zero (0) indicates that the node should not be awarded
	the service. The score is based on wether or not the service is currently assigned to 
	the node, whether the node is the host for the Filer or Jobber processing, and 
	the amount of free disk space in $TMP.  A node with a Satellite attribute is never
	awarded the Alerter service. 
&space
	If it is necessary to force a node to always score low or high, then the file 
	$NG_ROOT/site/alerter.must.score  should be set to contain a desired value. A value 
	of 2 should prevent a node from ever winning the election, and a value of 90 should 
	cause the node to always win the election.  Setting the value high is dangerous. 
&space
&term	query : When this command is entered on the command line, &this will determine the 
	current stat of the alerter service and echo one of three keywords back to the 
	standard output device: green, yellow, red.  Green inidcates that the service is 
	running and the node can be reached.  Yellow indicates that the node that is running 
	the service cannot be reached; the state of the service is questionable.  Red
	indicates that the service is stopped/down and that the service manager needs to 
	cause it to be started.  
&endterms


&space
&exit	Always 0.

&space
&see	ng_srv_mgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Jul 2005 (sd) : Sunrise.
&term	03 Dec 2007 (sd) : Added self initialisation. 
&term	04 Dec 2007 (sd) : Added better check against bad node name in srv_ var.
&endterms

&scd_end
------------------------------------------------------------------ */

