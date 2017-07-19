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
# Mnemonic:	srv_tpchanger.ksh 
# Abstract:	Manage the tpchanger service as needed by srv mgr. Provides the 
#		query/score/start/stop commands for this service.  Also provides an
#		init command to set the necessary service manager variable that 
#		defines the calling interface for start/stop/query/score. 
#
#		Right now the assumption is that we can run only one changer on a node
#		and thus there will be at most one Library entry in the site/config 
#		file.  Further, only one changer of the same type is supported on the 
#		cluster. Small revisions would be necessary to support multiple
#		tape libraries.
#		
#		The narbalek variables (NOT pinkpages) of the form Medialib/2domain
#		are used to translate real device names into domain or family names.
#		The process in the script is to determine the device name of the attached
#		library (via site/config) and use the ng_tp_getid to map that device name
#		into a full hardware name. The full hardware name is then converted to 
#		a Medialib/2domain variable which translates into the family/domain name. 
#
#		These variables are manually set and are of the form
#			MediaLib/2domain/<realname>="<family>" 
#
#		The real name comes from the device itself using the camcontrol or sq_inq
#		command (depending on the system).  If the Medialib variables are not set
#		this script attempts to determine the family (domain) name of the device
#		using the first few characters of the real name. 
#
#		Example variable name and values:
#			MediaLib/2domain/STK_SL500_8222ff="STK" 
#			MediaLib/2domain/QSTAR_412180_995812="QSTAR"
#
# Date:		05 June 2006 (conversion to new flavour or service manager)
# Author:	Andrew Hume/E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  	# get standard functions
PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun} 
. $STDFUN
. $PROCFUN


function chk_running
{
	typeset count=""

	if [[ ! -f $TMP/ps.$$ ]]		# query attempts to get one from the remote place
	then
		ng_ps >$TMP/ps.$$
	fi
	count=$(ng_proc_info -c  "ng_tp_changer" "$domain" <$TMP/ps.$$)
	if (( $count > 0 ))
	then
		return 0
	fi

	return 1	
}

function list_domains
{
	ng_nar_get -P MediaLib/2domain/.* | awk -v verbose=$verbose '
		{ 
			split( $1, a, "/" ); 
			if( verbose )
				print a[3];
			else
			{
				split( a[3], b, "\"" );
				print b[2];
			}
		}' | sort -u
}

port=$NG_NARBALEK_PORT

# -------------------------------------------------------------------
ustring="<domainname> {init|pool_state|state|start|stop|score|query}"
log2file=0			# -l sets to capture stderr into a private log in $NG_ROOT/site
domain=
list=0

while [[ $1 = -* ]]
do
	case $1 in 
		-l)	log2file=1;;
		-L)	list=1;;
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

if (( $list > 0 ))
then
	list_domains
	cleanup 0
fi

if (( $# < 2 ))		# must have domain and command on cmdline
then
	usage
	cleanup 0
fi

domain=$1
shift

if (( $log2file > 0 ))
then
	logf=$NG_ROOT/site/log/${argv0##*/ng_}.$domain; exec 2>>$logf
fi

thisnode=`ng_sysname`
hb_fname=hb_tpchanger.$domain		# basename of heartbeat file; wrtitten to $TMP

# we assume a node can only support one tape changer.  figure out what type is installed here
# none is ok as we can run this on a node to query the service
gre -1 '^library[ 	]' $NG_ROOT/site/config | read x dev

case "$dev" in
	/*)
		id="$(ng_tp_getid $dev 2> /dev/null)"
		fulldomain=$id
		ng_nar_get MediaLib/2domain/$id | read device_dom		# translate real physical ID to domain name STK1, QSTAR1 etc.

		if [[ -z $device_dom ]]			# not mapped in narbalek, make gross translation
		then
			case $id in
				STK*)			device_dom=STK;;
				QUAL*|QSTAR*)		device_dom=QSTAR;;
				*)			device_dom=none;;
			esac
			verbose "device ($dev) not mapped in narbalek (MediaLib/2domain/$id), making assumed translation: $device_dom"
		fi
		;;
	
	*)	device_dom=none;;
esac

case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Tpchanger_$domain="300+30!1!$this -l -v $domain start !$this -l -v $domain stop !$this -l -v $domain score !$this -l -v deprecated!$this -l -v $domain query"
		;;

	start)	
		if [[ $device_dom != $domain ]]
		then
			log_msg "cannot start for domain: $domain (type=$device_dom)"
			cleanup 0 
		fi

		log_msg "starting tpchanger service domain=$domain"

		if chk_running
		then
			nloop=125		# roughly 10 minutes worth of wait
			log_msg "stopping old daemon if running"
			ng_ps |grep tpchanger >&2
			ng_tpreq -d $domain exit		# stop previous one anyway
			sleep 1
		
			now=$(ng_dt -i)
			later="$( ng_dt -p $(( $now + 6000 )) )"
			log_msg "waiting for current one to stop; will give up about $later"

			while chk_running
			do
				nloop=$(( $nloop - 1 ))
				rm $TMP/ps.$$
				if (( $nloop <= 0 ))
				then
					ng_ps|ng_proc_info ng_tp_changer "$domain" | read pid
					log_msg "waited long enough for changer to exit; killing: pid=$pid"
					kill -9 $pid
					sleep 1
				else
					sleep 5
				fi
			done
			log_msg "current daemon has stopped"
		else
			log_msg "did not find one running, no attempt made to stop it"
		fi

		rm -f $TMP/$hb_fname				# trash the file so we get an alarm if tpchanger does not start

		log_msg "starting new daemon"
		tc_log=$NG_ROOT/site/log/tp_changer.$domain 
		ng_roll_log $tc_log

		ng_wrapper --detach -o1 $tc_log -o2 stdout  ng_tp_changer -v $domain $fulldomain
	
		ng_log $PRI_INFO $argv0 "tpchanger service started on $thisnode domain=$domain"
		ng_ppset -p $port srv_Tpchanger_$domain=$thisnode 			

		ng_get_nattr >&2
		sleep 2
		log_msg "enabling tp_changer heartbeat messages"
		ng_tpreq -d $domain heartbeat $TMP/$hb_fname 
		log_msg "============= end of startup ====================="
		


		echo "$domain started"		# silly, but sfpopen seems to leave process defunct if no output so we echo something
		;;
	
	stop)
		if [[ $device_dom != $domain ]]
		then
			log_msg "cannot stop for domain: $domain (type=$device_dom)"
			cleanup 0 
		fi

		echo "stopping changer service `date`" >&2
		ng_tpreq -d $domain exit
		ng_log $PRI_INFO $argv0 "changer service stopped on $thisnode domain=$domain"
		;;

	score)
		if [[ $device_dom != $domain ]]
		then
			log_msg "cannot support service for domain: $domain (dev_dom=$device_dom); score=0"
			echo 0
			cleanup 0 
		fi

		if ng_test_nattr Satellite 
		then
			echo 0
			log_msg "satellite attribute set; cannot accept service domain=$domain score=0"
			cleanup 0
		fi

		if [[ -n $NG_TPCHANGER_MUSTSCORE ]]
		then
			log_msg "mustscore variable found: score=$NG_TPCHANGER_MUSTSCORE"
			echo "${NG_TPCHANGER_MUSTSCORE:-0}"
			cleanup 0
		fi

                if [[ -f $NG_ROOT/site/tpchanger.must.score ]]			# deprecated, but still supported 
                then
                        read x y <$NG_ROOT/site/tpchanger.must.score
			log_msg "mustscore file found: score=$x"
                        echo ${x:-0}
                        cleanup 0
                fi

		if [[ $domain == "none" ]]
		then
			log_msg "no library device; cannot accept service score=0"
			echo 0
			cleanup 0
		fi

		ng_ppget srv_Tpchanger_$domain | read l
		eval export srv_Tpchanger_$domain=$l			# override what could be stale cartulary

		if ng_test_nattr Tpchanger_$domain		# we are already signed up as the tpchanger for this domain
		then
			score=75				# just vote us as the winner
			echo $score
			log_msg "we believe we are the current host ($srv_Tpchanger_$domain)"
			cleanup 0
		else
			if chk_running		# already running, we will stop/start if we win, but this gives our node the edge
			then
				score=30
			else
				score=20
			fi
		fi

		if ng_test_nattr Jobber Filer
		then
			score=2
		fi

		log_msg "returned score=$score"
		echo $score
		;;

	query)			# report to service manager based on what spyglass thinks of the service
				# must ask the spyglass on the node where the test is being executed
		eval node=\${srv_Tpchanger_$domain}
		if [[ -n $node ]]				# srv_Tpchanger_<domain> is defined
		then
			ng_sgreq -s $node explain tpchanger-$domain | gre "state:" | read junk1 state junk2
			verbose "query: spyglass state for srv_Tpchanger_$domain=${state:-MISSING}"
		else
			log_msg "query: srv_Tpchanger_$domain variable was not set; state forced to ALARM"
			state=ALARM
		fi
		case ${state:-ALARM} in
			NORMAL)		echo "green:$node";;
			ALARM)		echo "red:$node";;
			RETRY*)		echo "yellow:$node";;
			BLOCK*)		echo "yellow:$node";;
			*)		echo "red:$node";;
		esac
		;;


	state)		# for something like spyglass -- compute the state and exit 0 for OK, 1 for down, 2 for questionable/retry
		tfile=$TMP/tpreq.query.$$
		eval node=\${srv_Tpchanger_$domain}

		ng_rcmd -t 60 ${node:-srv_Tpchanger_$domain-undefined} ng_ps >$TMP/ps.$$	# ps listing from the remote node
		if chk_running
		then
			ng_rcmd -t 60 ${node:-srv_Tpchanger_$domain-undefined} ng_stat -m '$TMP'"/$hb_fname" |read modtime junk
			now=$( ng_dt -u )
			if (( ${modtime:-0} + 300 < $now ))		# stale heartbeat file?
			then	
				log_msg "state: now=$now  hb_ts=$modtime"
				log_msg "state: seems to be running on $node, but stale heartbeat: set questionable [QUESTION]"
				cleanup 2
			else
				log_msg "state: process in ps list and current heartbeat file [OK]"
				cleanup 0
			fi
		else
			log_msg "cannot find tpchanger in ps list on ${node:-srv_Tpchanger_$domain-undefined-node} [FAIL]"
			cleanup 1
		fi

		log_msg "internal error in state check for tp_changer"
		cleanup 1
		;;

	pool_state)		# for something like spyglass -- compute the state and exit 0 for OK, 1 for down, 2 for questionable/retry
		tfile=$TMP/tpreq.query.$$
		eval node=\${srv_Tpchanger_$domain}
		ng_rcmd -t 300 ${node:-srv_Tpchanger_$domain-undefined} ng_tpreq -d $domain pooldump all >$tfile &
		cpid=$!

		ng_rcmd -t 60 ${node:-srv_Tpchanger_$domain-undefined} ng_ps >$TMP/ps.$$	# ps listing from the remote node
		if chk_running
		then
			wait				# wait for the pool dump request
			
			if [[ ! -s $tfile ]]		# ps shows running, maybe it is just busy. two of these and srvmgr will elect new
			then
				log_msg "state: seems to be running on $node, but no response to pooldump request: set questionable [QUESTION]"
				cleanup 2
				#echo "yellow:$node"		
			else
				log_msg "state: process in ps list and non-zero output from query [OK]"
				cleanup 0
				#echo "green:$node"		# seems ok
			fi
		else
			log_msg "cannot find tpchanger in ps list [FAIL]"
			cleanup 1
		fi

		log_msg "internal error in state check for tp_changer"
		cleanup 1
		;;

esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_tpchanger:Tape changer service start/stop/score/query)

&space
&synop	ng_srv_tpchanger domain-name {init|pool_state|state||start|stop|score|query}

&space
&desc	This provides the service manager with a process that is used
	to determine the state (green, yellow, red) of the service, 
	generate the node's score with regard to the ability to host the tpchanger service, 
	and with entry points to start and stop the service.
&space
&subcat Narbalek Variables
	The narbalek variables (NOT pinkpages) of the form Medialib/2domain
	are used by &this to translate real device names into domain or family names.
	The process in the script is to determine the device name of the attached
	library (via NG_ROOT/site/config) and use ng_tp_getid to map that device name
	into a full hardware name. The full hardware name is then converted to 
	a Medialib/2domain variable which translates into the family/domain name. 
	This translation process is used to determine if the family domain name 
	supplied on the command line is attached to the node. 
&space

	These variables are manually set and are of the form
&break
&litblkb
   MediaLib/2domain/<realname>="<family>" 
&litblke
&space

	The real name comes from the device itself using the camcontrol or sq_inq
	command (depending on the system).  If the Medialib variables are not set
	this script attempts to determine the family (domain) name of the device
	using the first few characters of the real name. 
&space
&litblkb
Example variable name and values:
   MediaLib/2domain/STK_SL500_8222ff="STK" 
   MediaLib/2domain/QSTAR_412180_995812="QSTAR"
&litblke

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-L : List mode.  All command line paramters are ignored and a list of known/recognised
	domain names is presented.  If the verbose option is also given, then the full 
	hardware name is given with the domain name that it translates to as opposed to 
	listing just the domain names. 
&space
&term	-l : Write output to a log file in &lit(NG_ROOT/site/log). 
&space
&term	-p port : The port to send to when communicating with narbalek.  Allows for a 
	separate black board (narbalek) space to test with.
&space
&term	-v : Verbose mode.
&endterms


&space
&parms	&This expects the domain (family) name of the tape changer as well as an action which is one of:
	init, start, stop, query, or score.
	The domain name, something like STK or QSTAR, indicates the tape changer type that is 
	to be operated/reported on.  
	If the list option is used, then no parameters are needed and &this presents a list of 
	valid domain names (as are established by the Medialib variables currently defined in 
	narbalek).

&space
	The action token causes the following to happen:
&begterms
&term	init : Causes the service manager pinkpages variable for this service to be defined. 
	The service manager variable consists of the command line commands to invoke the start, 
	stop, query and score functions of this service as well as election information. See 
	the ng_srvmgr manual page for the specifics about the variable.  The variable name created
	will be of the form &lit(NG_SRVM_Tpchanger_domain) where &ital(domain) is the domain family
	name supplied on the command line (e.g. STK). 
&space
&term	pool_state : Intended to provide a state probe application, like &lit(ng_spyglass,) with the means to 
	query the state of the tape pool.  Exits with a code of 0 (up/ok), 1 (down), 2 (questionable/retry).
&space
&term	query : Determines the state of the service.  A single token is echoed to stdout 
	indicating the state. The token values are defined by the service manager and are 
	assumed to be gree, yellow or red.  
	The state of the changer is determined by looking at the current state as known to &lit(ng_spyglass)
	rather than a direct query. 
	This method is used as the query process can often take longer than service manager can tollerate
	and would cause service managger to assume the service was not functioning.  
	
&space
&term	start : Start the service.  Causes the &lit(ng_tpchanger) daemon to be started with the proper 
	parmaters for the indicated domain family.
	The pinkpages variable srv_Tpchanger_<domain> is set which causes the node to be assigned the 
	Tpchanger_<domain> attribute. 
&space
&term	state : Intended to provide a state probe application, like &lit(ng_spyglass,) with the means to 
	query the state of the changer application.  Exits with a code of 0 (up/ok), 1 (down), 2 (questionable/retry).
&space
&term	stop : Terminates the service by sending an exit command to the changer daemon.  The associated pinkpages variable 
	is NOT changed by the stop command; it is only modified by the node that starts the service so that 
	timing issues do not cause incorrect settings. 
&space
&term	score : Computes the score and writes it to the standard output device. The score 
	is an integer between 0 and 100; the larger the value the more able the node is to 
	accept the service.  A score of zero (0) indicates that the node should not be awarded
	the service. The score is based on wether or not the service is currently assigned to 
	the node, whether the node is the host for the Filer or Jobber processing, and 
	the amount of free disk space in $TMP.  A node with a Satellite attribute is never
	awarded the tpchanger service. 
&space
	If it is necessary to force a node to always score low or high, then a local pinkpages variable
	(NG_TAPECHANGER_MUSTSCORE) should be set to the score that the node will always report. 
	The use of the file 
	$NG_ROOT/site/tpchanger.must.score  to convey this value is now deprecated and a warning 
	will be logged in the master log if it is found and used. 
&space
	Setting a valud of 0 guarentees that the node will never be assigned the service. Other 
	values should be in the range of 1 through 99 with the higher value giving the node 
	the best chance to win the service.  Setting the value of 90 usually ensures that the 
	node will be awarded the service.   The must score variable is intended to keep 
	a node from being elected, or to greatly reduce its chances of being elected; use to 
	ensure election (setting a high value) can produce in undesired results. 
&endterms


&space
&exit	Usually 0, but if the state or pool_state actions are supplied non-zero exit codes indicate 
	various states.  See the earlier discussion on these actions for details.

&space
&see	&seelink(srvm:ng_srvmgr) &seelink(pinkpages:ng_ppget) &seelink(pinkpages:ng_narbalek) &seelink(pinkpages:ng_nar_get)
	&seelink(spyglass:ng_spyglass)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Jun 2006 (sd) : Sunrise.
&term	28 Jun 2006 (sd) : Fixed problem on tpreq call for query.
&term	19 Jul 2006 (sd) : Added info to output of query command.
&term	03 Nov 2006 (sd) : Added pool_state query to query responsiveness based on 
		pool command and to make the state query based on presence in ps list
		and recent mod to heartbeat file.
&term	03 Dec 2007 (sd) : Added self initialisation. 
&term	30 Jul 2008 (sd) : Corrected/enhanced man page to include valid usage and more info about device name. .** HBD AKD
		Added -L option.
&term	17 Dec 2008 (sd) : Changed the default for reporting stale heartbeat to 300s.
&endterms

&scd_end
------------------------------------------------------------------ */

