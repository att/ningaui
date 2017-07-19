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
# Mneminic:	ng_sendbid
# Abstract: 	Uses ng_udpsend to send a bit to seneschal. 
# Parms:	bid [node]
# Date: 	2 July 2001
# Author: 	E. Scott Daniels
#----------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

port=${NG_SENESCHAL_PORT}	# port that seneschal is ears to
node=`ng_sysname -s`		# default to the node that we are on
load=0
jobber=${srv_Jobber:-nojobber}
ustring="[-l] [-n nodename] [-p port] [-s jobber-node] [--man] value"

while [[ $1 = -* ]]
do
	case $1 in 
		-l)	load=1;;
		-n)	node="$2"; shift;;
		-n*)	node="${1#-?}";;
		-p)	port="$2"; shift;;
		-p*)	port="${1#-?}";;
		-s)	jobber="$2"; shift;;
		-s*)	jobber="${1#-?}";;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	echo "unrecognised option $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -z $port ]]
then
	echo "$argv0: cannot find port (NG_SENESCHAL_PORT)"
	cleanup 1
fi

if [[ $load -gt 0 ]]
then
	ng_sreq -s $jobber -p $port load ${2:-$node} ${1:0}
	if [[ -z $1 || $1 -eq 0 ]]
	then
		ng_sreq bid ${2:-$node} 0		# reset any auto bids that were outstanding
	fi
else
	ng_sreq bid ${2:-$node} ${1:-1}
fi

cleanup 0

exit 0
&scd_start
&doc_title(ng_sendbid:Send a bid for work to ng_seneschal)

&space
&synop	ng_sendbid [-p port] [-n node] [-s server] bid
&break	ng_sendbid [-p port] [-n node] [-s server] -l load

&space
&desc	&This sends a bid for either a specific number of jobs or 
	to set the load level for a particular node.
	When the &lit(-l) (load) option is used &ital(load) is sent 
	to &lit(ng_seneschal) on the jobber causing &lit(ng_seneschal)
	to attempt to keep &ital(load) jobs scheduled on the node.
	To disable &lit(ng_seneschal) from attempting to keep a specific
	number of jobs scheduled on a node, a load of zero (0) should 
	be entered.
	When &ital(bid) is sent, it is treated by &lit(ng_seneschal) as 
	a one time request. &lit(Ng_seneschal) will attempt to schedule
	&ital(bid) jobs until the requested number have been scheduled 
	on the node, or another bid request is received. To cancel any
	outstanding bids, a value of zero (0) should be entered.
&space
	By default, the bid/load request is sent to the &lit(ng_seneschal)
	running on the jobber of the cluster, and the bid is sent 
	for the node on which &this command was entered. These can 
	be overridden if necessary using command line options.
	

&space
	Bid and load requests are submitted via &lit(ng_sreq).

&space
&opts	The following options are recognised by &this when placed 
	on the command line ahead of the positional parameters:
&space
&begterms
&term	--man : Display this manual page, and exit.
&space
&term 	-l : Indicates that the first positional parameter is to be 
	treated as a load value rather than a bid. A &lit(load) 
	request is sent to &lit(ng_seneschal) rather than a &lit(bid)
	 request.
&space
&term	-n node : Overrides the default node and causes the bid/load
	request to be treated as if it was sent from &lit(node).
&space
&term	-p port : Supply a port number to use when communicating with 
	&lit(ng_seneschal). If not supplied, the value of the 
	&lit(NG_SENESCHAL_PORT) cartulary variable will be used. 
&space
&term	-s server : Causes the request to be sent to an &lit(ng_seneschal) 
	running on a host other than the jobber. 
&endterms

&space
&parms	&This expects that a value will be entered as the only positional
	command line parameter. The value is placed in either the load
	or bid request that is sent to &lit(ng_seneschal).

	
&space
	It must be noted that &lit(ng_seneschal) 
	uses the bid value without regard to the previous value, 
	or the number of jobs that were assigned using the previous
	value.  Thus if a previous bid of 10 resulted in 8 jobs 
	being scheduled, with &lit(ng_seneschal) holding the last two bids
	for future jobs, when a new bid of ten is received, &lit(ng_seneschal)
	changes the bid to ten; the bid is not accumulated. This allows 
	a node to send a bid of zero to prevent &lit(ng_seneschal) from 
	scheduling any additional jobs, without requiring the node to 
	track the current bid request in combination with the number 
	of jobs that have been scheduled (a feat that might not be 
	possible).

&space
&exit	An exit code of zero (0) indicates that the bid request 
	was constructed and sent.  It does not guarentee that the 
	request was received, recgonised or processed by &lit(ng_seneschal).

&space
&see	ng_seneschal	ng_sreq

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Jul 2001 (sd) : Initial go at it.
&term	29 Oct 2001 (sd) : added load stuff.
&term	19 Oct 2004 (sd) : Nixed ng_leader refs.
&endterms

&scd_end
------------------------------------------------------------------ */
