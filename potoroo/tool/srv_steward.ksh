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
# Mnemonic:	srv_steward.ksh 
# Abstract:	Invoked by service manager to do the actual driving of the 
#		steward service.  The steward service is a convenience service
#		that does little more than to provide an attribute to the elected
#		node. The node elected to host the steward service is then the 
#		target node for scheduled things that need execute only on one
#		node in the cluster. 
#		
# Date:		12 Feb 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
port=$NG_NARBALEK_PORT
ustring="[-f] [-p port] [-v] {start|stop|score|query}"

interactive=0
while [[ $1 = -* ]]
do
	case $1 in 
		-f)	interactive=1;;
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


if (( $interactive < 1 ))
then
	exec 2>>$NG_ROOT/site/log/srv_steward
fi

thisnode=$(ng_sysname)


case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Steward="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	start|continue)	
		ng_log $PRI_INFO $argv0 "steward service started on $thisnode"
		log_msg "starting steward service"
		log_msg "node attrs before start: $(ng_get_nattr)"
		ng_ppset -p $port srv_Steward=$thisnode 			
		log_msg "node attrs after  start: $(ng_get_nattr)"
		;;

	stop)							# do nothing; node that won election will set the var
		log_msg "stopping steward service $(date)" 
		ng_get_nattr >&2
		ng_log $PRI_INFO $argv0 "steward service stopped on $thisnode"
		;;

	query)	
		ng_ppget srv_Steward | read steward

		if ng_test_nattr Steward	# we are already signed up as the steward
		then
			mynode=$( ng_sysname )
			if [[ $steward == $mynode ]]
			then
				echo "green:$mynode"
			else
				echo "red:$steward" 
			fi
			cleanup 0
		fi


		if [[ -z $steward ]]
		then
			echo "red:pinkpages var srv_Steward was not set"
			cleanup 0
		fi

		ng_rcmd $steward ng_wrapper -e TMP | read answer
		if [[ -n $answer ]]
		then
			echo "green:$steward"
			cleanup 0
		fi

		echo "yellow: no answer from ${steward}"
		cleanup 0
			
		;;

	score)
		if ng_test_nattr Satellite 
		then
			log_msg "node is Satellite -- cannot host this service"
			echo 0
			cleanup 0
		fi

		ng_ppget srv_Steward | read l
		export srv_Steward=$l			# override what could be stale cartulary

                if [[ -f $NG_ROOT/site/steward.must.score ]]
                then
                        read x y <$NG_ROOT/site/steward.must.score			# read the value we must return and spit it out
			log_msg "score comes from $NG_ROOT/site/steward.must.score: ${x:-0}"
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

		if ng_test_nattr Steward				# we are already signed up as the steward
		then
			score=75			# just vote us as the winner
			echo $score
			log_msg "we believe we are the current host ($srv_Steward) score: 75"
			cleanup 0
		else
			score=10			# default -- score better than nodes running jobber/filer

			ng_df | grep $TMP |awk '{ printf( "%d\n",  $(NF-1) );  }'|read fullness		
			tmp_bonus=0
			if (( ${fullness:-90} < 20 ))			# some TMP exist as non-filesystems, default to 90% in these cases
			then
				tmp_bonus=10;				# bonus if usage in tmp is < 20%
			fi

			ng_sumfs | awk '				# compute repmgr space used on the node; award points based on avail space
 				/totspace=/ {
                        		split( $2, b, "=" );		# free space in all of repmgr filesystems (k bytes)


					bonus= 10 *  ( b[2]/(1024*200) );	# 10 points for every 200g of free space -- up to 40
                		}

				END { 
					printf( "%.0f\n", bonus > 40 ? 40 :  bonus );
					#printf( "%d\n", 40 * (1-pused) );	# add up to 40 points to score based on how much rm space is avail
				}
			'|read rmscore

			base_score=$score
			score=$(( $score + $tmp_bonus + rmscore ))
		
			log_msg "score: $base_score(base) + $tmp_bonus(tmp-bonus) + $rmscore(rm-score) = $score"

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
&doc_title(ng_srv_steward:Steward service start/stop/score)

&space
&synop	ng_srv_steward [-f] [-p port] [-v] {start|stop|score|query}

&space
&desc	&This provides the service manager with a process that is used
	to generate the node's score with regard to the ability to
	host the steward service, and with entry points to start and stop
	the service.
&space
	The steward service provides nothing more than one node in the cluster
	with the Steward attribute.  The node that has been elected as the 
	steward becomes the target for any activity that must take place on 
	a single node in the cluster.  Examples of this are setting the 
	log_comb hood definitions, scanning for unattached masterlog fragment
	files, and certain spyglass checks.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f : Run in interactive (foreground) mode. If not set verbose and log messages
	are written to a predetermined log file: $NG_ROOT/site/log/srv_steward
&space
&term	-p port : The port to send to when communicating with narbalek.  Allows for a 
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
	Steward node, and sets the srv_Steward pinkpages variable.
&space
&term	stop : Terminates the service. No real action other than logging the event takes place
&space
&term	score : Computes the score and writes it to the standard output device. The score 
	will be between 0 and 75 inclusive.  Sattelite nodes are forced to score 0. Nodes running
	other 'high demand' services (Jobber and Filer) are forced to score 2. If the node 
	has already been assigned the steward service,  a score of 75 is reported.  For all other
	nodes the score is calculated as a value between 10 and 60 and is based on the 
	amount of free space in the replication manager filesystems with a small bonus given
	to nodes that have exceptionally low usage in the $TMP filesystem. The tmp bonus is
	awarded only if $TMP is a real filesystem and not just a directory under another 
	mountpoint.  
&space
	If the file $NG_ROOT/site/steward.must.score exists, it is assumed to contain a hard
	value (as the only token on the first line) that is the score that this node MUST 
	report if the node is not a satellite node.    
	A value of 90 in the file will usually guarentee that a node will win the election, 
	while a value of 0 will ensure that the node is never elected. 
	Care must be taken when setting a value in this file.  
   
&space
&term	query : When this command is entered on the command line, &this will determine the 
	current stat of the steward service and echo one of three keywords back to the 
	standard output device: green, yellow, red.  Green inidcates that the service is 
	running and the node can be reached.  Yellow indicates that the node that is running 
	the service cannot be reached; the state of the service is questionable.  Red
	indicates that the service is stopped/down and that the service manager needs to 
	cause it to be started.  
&endterms


&space
&exit	Always 0.

&space
&see	ng_srvmgr, ng_smreq, ng_srv_tpchanger, ng_srv_alerter 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	11 Feb 2007 (sd) : Sunrise.
&term	03 Dec 2007 (sd) : Added self initialisation. 
&term	04 Dec 2007 (sd) : Added extra check in query to prevent bad node name assigned to srv_ var.
&endterms

&scd_end
------------------------------------------------------------------ */

