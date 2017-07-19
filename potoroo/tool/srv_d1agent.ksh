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
# Mnemonic:	srv_d1agent.ksh 
# Abstract:	Invoked by service manager to do the actual driving of the 
#		d1agent service.  
#		
# Date:		01 May 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
nar_port=$NG_NARBALEK_PORT
d1_port=$NG_D1AGENT_PORT

ustring="[-f] [-p d1port] [-P narbalek-port] [-v] {start|stop|score|query}"

interactive=0
while [[ $1 = -* ]]
do
	case $1 in 
		-f)	interactive=1;;
		-p)	d1_port=$2; shift;;
		-P)	nar_port=$2; shift;;

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


if (( $interactive < 1 ))
then
	exec 2>>$NG_ROOT/site/log/srv_d1agent
fi

thisnode=$(ng_sysname)


case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_D1agent="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	start|continue)	
		ng_log $PRI_INFO $argv0 "d1agent service started on $thisnode"
		log_msg "starting d1agent service"

		
		ng_ppset -p $nar_port srv_D1agent=$thisnode 			
		echo "verify " | ng_sendtcp -e . -t 2 localhost:$d1_port| read answer
		if [[ -n $answer ]]
		then
			log_msg "d1agent already running  and responding on this node"
			cleanup 0
		fi

		ng_wrapper --detach -O1 $NG_ROOT/site/log/rm_d1agent -o2 stdout ng_rm_d1agent -v
		cleanup $1
		;;

	stop)							# do nothing; node that won election will set the var
		log_msg "stopping d1agent service $(date)" 
		echo "exit 4360" | ng_sendtcp -t 2 -e . localhost:$d1_port
		ng_log $PRI_INFO $argv0 "d1agent service stopped on $thisnode"
		;;

	query)	
		ng_ppget srv_D1agent | read d1agent

		if [[ -z $d1agent ]]
		then
			echo "red:pinkpages var srv_D1agent was not set"
			cleanup 0
		fi

		ng_rcmd $d1agent ng_wrapper -e TMP | read answer
		if [[ -n $answer ]]
		then
			echo "verify " | ng_sendtcp -e . -t 2 $d1agent:$d1_port| read answer
			if [[ -z $answer ]]
			then
				log_msg "first verification attempt failed; pausing before a retry"
				sleep 3
				echo "verify " | ng_sendtcp -e . -t 2 $d1agent:$d1_port| read answer
			fi

			if [[ -z $answer ]]
			then
				log_msg "empty verification; marking down"
				echo "red:$d1agent"
			else
				log_msg "verification: $answer"
				echo "green:$d1agent"
			fi
			cleanup 0
		fi

		log_msg "questionable status: unable to connect to node: $d1agent"
		echo "yellow: no answer from node ${d1agent}"
		cleanup 0
			
		;;

	score)
		if ng_test_nattr Satellite 
		then
			log_msg "node is Satellite -- cannot host this service"
			echo 0
			cleanup 0
		fi

		if ! whence ng_rm_d1agent >/dev/null 2>&1
		then
			log_msg "ng_rm_d1agent seems not to be installed; score=0"
			echo 0 
			cleanup 0
		fi

		ng_ppget srv_D1agent | read l
		export srv_D1agent=$l			# override what could be stale cartulary

                if [[ -f $NG_ROOT/site/d1agent.must.score ]]
                then
                        read x y <$NG_ROOT/site/d1agent.must.score			# read the value we must return and spit it out
			log_msg "score comes from $NG_ROOT/site/d1agent.must.score: ${x:-0}"
                        echo ${x:-0}
                        cleanup 0
                fi



		if ng_test_nattr Jobber Filer Macropod
		then
			log_msg "node is already hosting another major service, score low: 2"
			score=2
			echo $score
			cleanup 0
		fi

		if ng_test_nattr D1agent				# we are already signed up as the d1agent
		then
			score=75					# just vote us as the winner
			echo $score
			log_msg "we believe we are the current host ($srv_D1agent) score: 75"
			cleanup 0
		else
			score=$(( 10 * $(ng_ncpus) ))			# default -- score better than nodes running jobber/filer
			if (( $score > 60 ))
			then
				score=60
			fi

			log_msg "score: $score"
		fi


		echo ${score:-0}
		;;

	*)	log_msg "unrecognised command: $*"
		cleanup 0
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_d1agent:D1agent service start/stop/score)

&space
&synop	ng_srv_d1agent [-f] [-p port] [-v] {start|stop|score|query}

&space
&desc	&This provides the service manager with a process that is used
	to generate the node's score with regard to the ability to
	host the d1agent service, and with entry points to start and stop
	the service.
&space
	The d1agent service is an interface to the replication manager dump1 command 
	interface.  The service accepts dump1 requests from a client and 
	initiates a 'watch' on each file indicated in the requests. As long as the 
	client process keeps the TCP session to the agent open, the agent will 
	forward any dump1 responses from replication manager to the client, but only 
	when there is a change. Please see the man page for &lit(ng_rm_d1agent) for 
	more information.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f : Run in interactive (foreground) mode. If not set verbose and log messages
	are written to a predetermined log file: $NG_ROOT/site/log/srv_d1agent
&space
&term	-p port : The port number that should be used when attempting to communicate with 
	the d1agent.  If not supplied, NG_D1AGENT_PORT will be used. When the agent is 
	started, it will always use the NG_D1AGENT_PORT pinkpages variable to determine
	its wellknown listen port. 
&space
&term	-P port : The port to send to when communicating with narbalek.  Allows for a 
	separate black board space to test with.
&space
&term	-v : Verbose mode. 
&endterms

&space
&parms	Expected on the command line is an action token which is one of: start, stop, or score.

	The action token causes the following to happen:
&begterms
&term	init : Causes the service manager pinkpages variable for this service to be defined. 
	The service manager variable consists of the command line commands to invoke the start, 
	stop, query and score functions of this service as well as election information. See 
	the ng_srvmgr manual page for the specifics about the variable.  
&space
&term	start : Initialise the service.  Registers the node with replication manager as the 
	D1agent node, and sets the srv_D1agent pinkpages variable.
&space
&term	stop : Terminates the service. No real action other than logging the event takes place
&space
&term	score : Computes the score and writes it to the standard output device. The score 
	will be between 0 and 75 inclusive.  Sattelite nodes are forced to score 0. Nodes running
	other 'high demand' services (Jobber and Filer) are forced to score 2. If the node 
	has already been assigned the d1agent service,  a score of 75 is reported.  For all other
	nodes the score is calculated as a value between 10 and 60 and is based on the 
	amount of free space in the replication manager filesystems with a small bonus given
	to nodes that have exceptionally low usage in the $TMP filesystem. The tmp bonus is
	awarded only if $TMP is a real filesystem and not just a directory under another 
	mountpoint.  
&space
	If the file $NG_ROOT/site/d1agent.must.score exists, it is assumed to contain a hard
	value (as the only token on the first line) that is the score that this node MUST 
	report if the node is not a satellite node.    
	A value of 90 in the file will usually guarentee that a node will win the election, 
	while a value of 0 will ensure that the node is never elected. 
	Care must be taken when setting a value in this file.  
   
&space
&term	query : When this command is entered on the command line, &this will determine the 
	current stat of the d1agent service and echo one of three keywords back to the 
	standard output device: green, yellow, red.  Green inidcates that the service is 
	running and the node can be reached.  Yellow indicates that the node that is running 
	the service cannot be reached; the state of the service is questionable.  Red
	indicates that the service is stopped/down and that the service manager needs to 
	cause it to be started.  
&endterms


&space
&exit	Always 0.

&space
&see	ng_srvmgr, ng_smreq, ng_srv_tpchanger, ng_srv_alerter, ng_srv_steward, ng_srv_jobber

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 May 2007 (sd) : Sunrise.
&term	01 Nov 2007 (sd) : Added more diagnostics to query mode. 
&term	03 Dec 2007 (sd) : Added self initialisation. 
&endterms

&scd_end
------------------------------------------------------------------ */

