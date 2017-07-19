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
# Mnemonic:	rm_mon
# Abstract:	monitor repmgr activity and send to parrot
#		
# Date:		08 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

PATH=$PATH:$HOME/bin

if [[ "$*" != *-f* ]]
then
	detach_log=/tmp/$LOGNAME.dlog
	$detach
fi

# -------------------------------------------------------------------
export NG_WREQ_QUIET=1		# prevent wreq from logging cannot connect messages 

ustring="[-c cluser] [-h parrot-host] [-i viewvariable] [-l resource-name-list] [-n nodes] [-r repeat-seconds] "
phost=$srv_Netif
list=" RM_FS RM_SEND RM_RCV_FILE RM_DELETE RM_RENAME RM_DEDUP"
nodes=""
id="rmjobs"
repeat=0
dfmon=0;
dftimer=1
cluster=""

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster="$cluster$2 "; shift;;
		-f)	;;			# foreground ignore
		-d)	dfmon=1;;
		-h)	phost=$2; shift;;
		-i)	id=$2; shift;;
		-l)	list="$2"; shfit;;
		-n)	nodes="$2"; shift;;
		-r)	repeat=$2; shift;;

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


if [[ -z $nodes ]]		# user did not supply hard nodes
then
	cluster=${cluster:-$NG_CLUSTER} 		# ensure it is set for the loop
	for c in $cluster
	do
		ng_rcmd $c ng_members  -R |read x
		nodes="$nodes$x "			# combine nodes from all named clusters
	done
fi


if [[ -z $nodes ]]
then
	log_msg "cannot find any nodes to monitor"
	cleanup 1
fi

echo $nodes
while true
do
	>/tmp/bcast.$$
	for x in $nodes
	do
		ng_wreq -t 14 -s $x explain $list 2>/dev/null | awk -v bcast=/tmp/bcast.$$ -v id="$id" -v node=$x '
			/^resource / {
				split( $5, a, "=" );
				total += a[2];
				saw++;
			}
	
			END { 
				if( saw )
					printf( "RECOLOUR %s_%s 12 \"%d\"\n", node, id, total ); 
				else
					printf( "RECOLOUR %s_%s 12 \"XXX\"\n", node, id, total ); 
			}
		' >>/tmp/bcast.$$
	done

	if [[ -s /tmp/bcast.$$ ]]
	then
		ng_sendudp $phost:$NG_PARROT_PORT </tmp/bcast.$$
	fi

	if [[ $dfmon -gt 0 ]]
	then
		dftimer=$(( $dftimer - 1 ))
		if [[ $dftimer -eq 0 ]]
		then
			dftimer=5;
			df_mon.ksh -n "$nodes"
		fi
	fi
	if [[ $repeat -gt 0 ]]
	then
		sleep $repeat
	else
		cleanup 0
	fi

	if [[ -n $cluster ]]			# if user did not supply a hard list, refresh each pass
	then
		nodes=""
		for c in ${cluster:-$NG_CLUSTER}
		do
			ng_rcmd $c ng_members  -R |read x
			nodes="$nodes$x "			# add all nodes from all clusters named
			
		done
	fi
done


cleanup 0

exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_z_rmmon:Zookeeper repmgr monitor)

&space
&synop
	[-c cluster] [-h parrot-host] [-i viewvariable] [-l resource-name-list] [-n node(s)] [-r repeat-seconds] 

&space
&desc	&This peeks at woomera queues and reports the number of outstanding file 
	manipulation jobs queued on each node in a cluster.  The output of this 
	script is formatted for delivery to a W1KW process in order to cause it to 
	update its view(s).  Messages are forwarded to an &lit(ng_parrot) process
	which in turn sends the message(s) to all registered W1KW processes. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : The cluster to query. If not defined, the nodes on the current cluster
	are examined.
&space
&term	-h host : The host that the parrot is running on.  If not defined, the cluster's
	network interface host, as defined by the &lit(srv_Netif) pinkpages variable, 
	is used. 
&space
&term	 -i name : The base name of the variables in the W1KW view that are to be addressed. 
	If not supplied, the variables are expected to start with &lit(rmjobs).
&space
&term	-l list : The list of &lit(ng_woomera) resources that are looked at and reported on.
	If not defined the list is: RM_FS, RM_SEND, RM_RCV_FILE, RM_DELETE, RM_RENAME, and RM_DEDUP.
&space
&term	-n nodes : The list of nodes to query. If not defined the all of the nodes for the 
	indicated or default cluster are used. 
&space
&term	-v : Verbose mode.
&endterms

&space
&exit	This script should always exit with a zero return code. 

&space
&see	ng_parrot, ng_z_dfmon, ng_z_psmon

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2002 (sd) : Thin end of the wedge.
&term	28 Aug 2006 (sd) : changed name for release, cleaned up cluster and default parrot values.
&term	17 Mar 2008 (sd) : Now gets all registered members, not just active ones. 
&endterms

&scd_end
------------------------------------------------------------------ */
