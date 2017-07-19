#!/usr/common/ast/bin/ksh
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
# ======================================================================== 06049


#!/usr/common/ast/bin/ksh
# Mnemonic:	spy_cknet
# Abstract:	various aspects of network connectivity
#			-- test connections between this node and neighbour nodes
#				can alarm when node cannot be reached via any interface
#				can alarm when node cannot be reached via every interface (conn -a)
#
#			-- test availability of the node that answers to the cluster name that we 
#				expect to be in DNS.
#
#			-- test that remote service is reachable from this node (test is driven 
#				only if green-light seems up on the remote node)
#			
#			-- test to see if green-light is up on a remote node
#
#			-- test to see if spyglass is up on neighbour nodes
#
#			-- test to see if an httpd process is up and running and that a desired 
#				page can be found.
#
#		A neighbour is defined as the node that sits next to this node in the 
#		list returned by ng_members.  Typically we test the node on either side
#		(assume circular list) which allows all nodes to be tested, with some 
#		redundancy, without requiring all nodes to be tested from a single point
#		in the cluster. 
#		
# Date:		
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# ------------------- support functions ---------------------------------------------


# get a list of members and spit out the two that are on either side of us in the
# list.  An easy way for the cluster to check a few without leaving holes and avoiding
# too many repeats
#
function get_neighbours
{
	typeset member_cache=$TMP/spyglass/cache.members
	typeset m="$( ng_members )"

	if [[ -n $m ]]				# good list, cache for times when we miss a list
	then
		echo $m >$member_cache
	else
		if [[ -r $member_cache ]]
		then
			read m <$member_cache		# use from cache 
			log_msg "ng_members returned a null list; used cache: $m"
		fi
	fi

	awk -v mem="$m" -v me=$mynode '		# figure out which nodes we should test
		BEGIN {
			idx = 0;
			x = split( mem, a, " " );
			if( x == 1 )			# if only one node, we assume it is us; blow off
				exit( 0 );

			for( i = 1; i <= x; i++ )	# find our position in the list
			{
				if( a[i] == me )
				{
					idx = i;
					break;
				}
			}

			if( idx )			# print names that appear on either side of us in the list
			{
				printf( "%s\n", a[(idx == 1 ? x : idx-1)] );		# output our two neighbours in the list
				if( x > 2 )
					printf( "%s\n", a[(idx == x ? 1 : idx+1)] );
			}
		}
	'
}

# get all interfaces for a node. emit a single string of space separated inet4 addresses
# if we cannot get the current interfaces (ningaui not running on the remote node), then 
# we look in $TMP/spyglass to see if we saved them earlier.  Likely node is down, but we 
# can still make a good test and not assume so if it is just parrot that is having issues. 
function get_if
{
	if ! ng_rcmd -t 30 $1 ifconfig -a >$TMP/config.$$ 2>/dev/null
	then
		ng_rcmd -t 30 $1 /usr/etc/ifconfig -a >$TMP/config.$$				# bloody sgi nodes
	fi

	if [[ -s $TMP/config.$$ ]]					
	then
		cp $TMP/config.$$ $TMP/spyglass/interfaces.$1		# save to use if we cannot get list in future
	else						 		# host probably down, or at least ningaui, get last known info
		if  gre LOOP  $TMP/spyglass/interfaces.$1 >/dev/null 2>&1 
		then
			verbose "rcmd to $1 failed, using last known interface list in $TMP/spyglass/interfaces.$1"
			cp $TMP/spyglass/interfaces.$1 $TMP/config.$$
		else
			verbose "just digging info from /etc/hosts for $1"
			gre $1 /etc/hosts| awk ' { printf( "%s ", $1 ); } END {printf( "\n" ); }' 
			return 
		fi
	fi

	awk  '
		/LOOPBACK/	{ loop=1; snarf=0; next; }

		/UP[, ]/	{ 				
			if( last_inet )
			{
				printf( "%s ", last_inet );
				last_inet = "";
				snarf = 0;
			}
			else
				snarf=1; 
			next; 
		}

		/inet6/		{ next; }

		/inet/ {
			if( $2+0 == 127 )
				next;
			gsub( ".*:", "", $2 );
			if( snarf )
				 printf( "%s ",  $2 );
			else
				last_inet = $2;				# bloody linux puts before UP
			snarf = 0;
		}

		END {
			printf( "\n" );
		}
	'	<$TMP/config.$$
}

# take two sets of ip addresses (ours and theirs) print all of their 
# ip addresses that we seem to have a matching network address for.
# we use this to figure out which of their interfaces we can test
# assuming that if we do not have an interface on a matching network we 
# cannot reach the other node. Mostly to help with 1000BaseT nets
# that seem to be addressed on private class C networks because of limited
# connectivity to the switch.
function common_nets
{
	awk -v ourlist="${1% }", -v theirlist="${2% }" '
		function getnet( x, 	a, i )			# this does not take into consideration netmask, but should be fine for our needs
		{
			split( x, a, "." );
			if( x+0 < 0x80 )
				return sprintf( "%s", a[1] );
			if( x+0 < 0xc0 )
				return sprintf( "%s.%s", a[1], a[2] );
			if( x+0 < 0xe0 )
				return sprintf( "%s.%s.%s", a[1], a[2], a[3] );
			else
				return sprintf( "%s.%s.%s.%s", a[1], a[2], a[3], a[4] );
			
		}

		BEGIN {
			no = split( ourlist, ours, " " );
			nt = split( theirlist, theirs, " " );

			for( i = 1; i <= no; i++ )
			{
				on = getnet( ours[i] );
				ournet[on] = 1;		# figure out which network each of our interfaces is on
			}

			for( i = 1; i <= nt; i++ )			# if we have an interface on the net for theirs, then we test it
			{
				tn = getnet( theirs[i] );
				if( ournet[tn] )
					printf( "%s ", theirs[i] );
			}

			printf( "\n" );
		}
	'
}

# test a named host ($1) using $2-$n names/ip addresses. 
# the return code indicates the state:
#	0 - node (ping) and a service (discard or parrot) can be reached on all of the interfaces given
#	1 - node and a service can be reached only on some of the interfaces given
#	2 - node cannot be reached on any interface given

function ping_test
{
	verbose "ping-test: $@"

	typeset rc=0
	typeset test_discard=0
	typeset fail_count=0
	typeset pingable=0		# node could be reached via ping on at least one interface
	typeset reached=0		# node could be reached on at least one interface to one service

	while [[ $1 == -* ]]
	do
		case $1 in 
			-d)	test_discard=1;;
		esac

		shift
	done

	typeset host=$1
	shift 


	while [[ -n $1 ]]			# for each interface given on the cmd line
	do
		log_msg "ping_test: from $mynode to $host=$1"
		rc=0
		if ping -t 1 -c 1 $1 >/dev/null 2>&1			# simple ping test
		then
			log_msg "good ping to $host at $1 [OK]"
			pingable=1
		else
			if ping -c 1 $1 >/dev/null 2>&1			# bloody mac version does not support timeout
			then
				log_msg "good ping to $host at $1 [OK]"
			else
				log_msg "could not send ping to $host at $1 [FAIL]"
				fail_count=$(( $fail_count + 1 ))
				rc=1
			fi
		fi

		if (( $rc == 0 ))				# if ping failed above, then forget other tests on the interface
		then
			if (( $test_discard > 0 ))
			then
				verbose "testing connectivity with discard service on $1"
				echo "foo" | ng_sendtcp -h -t 1 $1:9		# hit the discard service, if we connect, we will get a 20
				rc=$?							# as retrun from sentcp -- timeout waiting for response
			else
				rc=1			# force into rcmd test
			fi
	
			if (( $rc == 0 || $rc == 20 ))
			then
				log_msg "good ping and able to connect to discard service on $host at $1 [OK]"
				reached=1
				if (( ${short_circuit:-0} > 0 ))
				then
					return 0
				fi
			else
				verbose "testing connectivity to parrot on $1"
				if ! ng_rcmd -t 30 $1 ls >/dev/null		# if discard is down, or not testing it, test parrot as an alternative
				then
					fail_count=$(( $fail_count + 1 ))
					log_msg "unable to connect to service on $host at $1 [FAIL]"
				else
					log_msg "good ping, connect to parrot on $host at $1 [OK]"
					reached=1
				fi
			fi
		fi

		shift
	done

	rc=3					# should be overridden, this will indicate internal errors
	if (( $fail_count > 0 ))		# some, or maybe all, interfaces not responding 
	then
		if (( $reached > 0 )) 		# something reached, then fail with rc=1
		then
			log_msg "node can be reached only on some interfaces: $host [WARN]"
			rc=1
		else
			log_msg "node cannot be reached [FAIL]"			# nothing reached, fail with rc=2
			rc=2
		fi
	else
		log_msg "node can reached via all interfaces: $host [OK]"		# no failures, rc gets 0
		rc=0
	fi

	return $rc
}


# --------------------- test functions ---------------------------------------------------

# check to see if an http daemon is running on a desired host:port combination
# $1: host[:port] $2: page portion of the url (/~scooter/index.html)
# $3 is a pattern that is expected to be in the returned document. If not supplied <html> is used
#	the pattern is assumed to NOT be case sensitive.
function httpd_check
{
	typeset host=${1%%:*}
	typeset port=${1##*:}
	typeset page="${2:-/index.html}"
	typeset out=$TMP/PID$$.output
	typeset key="${3:-"<html>"}"


	# bug tracker is very picky and needs all of these fields before it responds. 
 	req="GET $page HTTP/1.0\nUser-Agent: ng_sendtcp 2.3\nAccept: */*\nAuthorization: Basic ZGFuaWVsczpJZm9yZ290Lg==\nHost: $host:$port\nConnection: Drop\n\n"

 	#if ! cat /tmp/daniels.h | ng_sendtcp -d -v -t 4 $host:${port:-80} >$out  2>&1
	if ! printf "$req" | ng_sendtcp -d -v -t 4 $host:${port:-80} >$out
	then
		log_msg "unable to connect (or other network failure) to $host:${port:-80} [FAIL]"
		cat $out
		return 1
	fi


	if [[ -s $out ]]		# daemon up and something returned 
	then
		if gre 404 $out >/dev/null
		then
			log_msg "page ($page) not found (404 error) on $host:${port:-80} [FAIL]"
			return 1
		else
			if gre -i "$key" $out >/dev/null 	
			then
				log_msg "returned data contained the key: $key [OK]"
			else
				log_msg "no key in the output: $key [FAIL]"
				head -30 $out
				return 	1
			fi
		fi
	else
		log_msg "http daemon responded, but with empty dataset [FAIL]"
		return 1
	fi

	return 0
}

# check to see that we can reach the node "on either side" of us. each node tests
# two other nodes using the node list returned by ng_members. The selected nodes are 
# those adjacent in the list with the first and last nodes listed being adjacent. 
# 
function conn_check
{
	typeset rc=0

	get_neighbours >$TMP/list.$$

	if [[ ! -s $TMP/list.$$ ]]
	then
		verbose "nothing in the list to test"
		return 0
	fi

	rc=0
	our_ifs="$(get_if $mynode)"
	if [[ -z $our_ifs ]]
	then
		log_msg "something really smells, we cannot get a list of our interfaces [FAIL]"
		return 3
	fi

	conn_rc=0
	while read name
	do
		their_ifs="$( get_if $name )"

		if [[ -z $their_ifs ]]
		then
			log_msg "cannot get interfaces for $name; node down? [FAIL]"
			conn_rc=3
		else
			rattr="$( ng_get_nattr $name )"
			if ng_test_nattr -l "$rattr" Satellite
			then
				is_satellite=1
				this_need=1		# for satellites we force a need of some
				verbose "$name is a satellite, not requiring all interfaces be available to us if -a option was set"
			else
				is_satellite=0
				this_need=$need		# not satellite, so the user selected need applies
			fi

			if (( $test_common > 0 || $is_satellite > 0   ))
			then
				their_ifs="$( common_nets "$our_ifs" "$their_ifs" )"	# test only what we have in common
			fi

			ping_test $dflag $name $their_ifs
			ptrc=$?
			case $this_need$ptrc in 
				00)			# need all, got all up
					log_msg "general status: needed all interfaces for $name to be up; they are [OK]"
					;;

				11|10)			# need some (1x), got some up (x1 | x0)
					log_msg "general status: needed some interfaces for $name to be up; some are [OK]"
					;;

				02|12)			# need some (1x) or all (2x) up, got none up (x2)
					log_msg "general status: needed some interfaces for $name to be up; none are [FAIL]"
					conn_rc=1
					;;

				01)			# need all up (0x) only got some up (x1)
					log_msg "general status: need all interfaces  for $name to be up; only some are [FAIL]"
					conn_rc=1
					;;

				*3)
					log_msg "internal error -- got rc=3 from ping_test [FAIL]"
					conn_rc=3
					;;

				*)			# unknown state, but its not good
					log_msg "general status: unable to communicate with $name on some/all interfaces; none are [FAIL]"
					conn_rc=1
					;;
			esac
		fi

		echo "" >&2
	done <$TMP/list.$$

	return $conn_rc
}

# service test.  check a service on a remote node ($1) using the command ($2). We first test to ensure that 
# greenlight is running on the node.  If it is not, then we return a block rc -- no alarm, but prevents any 
# check that may depend on this   If $3 is supplied then we fail if the result is not exactly that. If $3 is 
# null, then we assume any response on stdout is a positive response and we go from there.
# host ($1) can be a srv_* name; we will translate and substitute it into the command if necessary.
# we will run the command on THIS node which allows for a test of something like catalogue which should know
# how to go off and find the remote node.  If the command needs a push to the right node, the command can be 
# coded with _HOST which will substitute the host into the command after it is looked up. 
# for example:  srv_Jobber "ng_sreq dump 10" "jobs.*flags"
#		srv_Filer  "ng_rmreq -s _HOST sleep 1"
function check_service
{
	typeset force=0
	typeset must_be_srvm=0
	typeset optional_svc=0

	while [[ $1 == -* ]]
	do
		case $1 in 	
			-m)	must_be_srvm=1;;	# must be managed with service manager on this cluster
			-o)	optional_svc=1;;	# service is optional, if srv_Name is not in pp, then we assume its not configured and dont scream
			-f)	force=1;;		# alarm even if node seems down

		esac

		shift
	done

	typeset host=$1
	typeset cmd="$2"
	typeset resp="$3"
	typeset ppdata=""

	if (( $must_be_srvm > 0 ))
	then
		eval ppdata=\"\$NG_SRVM_${host##srv_}\"
		if [[ -z $ppdata ]]
		then
			log_msg "service not defined for this cluster; no check made [OK]"
			rc=0
			return
		fi
	
		log_msg "service is defined for the cluster, continuing [OK]"
	fi

	log_msg "testing service on $host: cmd=$cmd"

	if [[ $host == srv_* ]]
	then
		log_msg "translating host from $host"
		host="$( ng_ppget $host )"
	fi

	if [[ -z $host ]]
	then
		if (( $optional_svc > 0 ))
		then
			log_msg "no hostname for service; service is optional so assume not desired [OK]"
			rc=0
			return 0
		fi

		log_msg "could not translate $1 to a host name; service marked down [FAIL]"
		rc=1
		return 1
	fi

	rc=0
	if (( $force > 0 )) || ng_rcmd -t 30 ${host:-hostname-buggered} ng_wrapper -s		# if green light is up
	then
		if (( $force > 0 ))
		then
			log_msg "probe forced, greenlight might not be running on $host [WARN]"
		else
			log_msg "greenlight running on $host [OK]"
		fi

		${cmd//_HOST/$host} >$TMP/cmd.out.$$	# we run the command substituting $host for _HOST e.g.  ng_sreq -s _HOST dump 10
		if [[ -n $resp ]]
		then
			if gre "$resp" $TMP/cmd.out.$$ >/dev/null
			then
				log_msg "found expected string in output buffer from $host [OK]"
			else
				log_msg "did not find expected string ($resp) in output from $host [FAIL]"
				rc=1
			fi
		else
			if [[ -s $TMP/cmd.out.$$ ]]
			then
				log_msg "received info from $host; assuming legit [OK]"
			else
				log_msg "no informatoin received from $host [FAIL]"
				rc=1
			fi
		fi
		
	else
		log_msg "unable to establish a good connection to, or verify green-light running on $host; test aborts"
		rc=3
	fi

	echo " " >&2
	log_msg "output from $host:"
	head -10 $TMP/cmd.out.$$
	return $rc
}

# check to see if green light running no named node
function check_rmt_gl
{
	if ng_rcmd -t 30 $1 ng_wrapper -s 
	then
		log_msg "green light running on $1 [OK]"
		return 0
	else
		log_msg "green light not running (or no response from) $1 [FAIL]"
		return 1
	fi
}

function check_rmt_spy
{
	if ng_rcmd -t 30 $1 ng_wrapper -s
	then
		ng_sgreq -s $1 explain |wc -l |read count
		if (( ${count:-0} > 0 ))
		then
			log_msg "ng_spyglass on $1 is responsive [OK]"
		else
			log_msg "ng_spyglass on $1 is NOT responsive [FAIL]"
			return 1
		fi
	else
		log_msg "green light not running, or no response from host: $1 [FAIL]"
		return 1
	fi

	return 0
}

# ------------------------------------------------------------------------------------------------------------------

# this overrides the function in stdfun
function usage
{
	cat <<endKat
usage:	$argv0 [-v] conn [-a | -s]
	$argv0 [-v] netif
	$argv0 [-v] remote_green_light
	$argv0 [-v] service [-f] [-m] [-o] name command [response]
	$argv0 [-v] spyglass
endKat
}

# -------------------------------------------------------------------
if [[ -f /usr/etc/ping ]]
then
	PATH=$PATH:/usr/etc		# bloody sgi
fi

test_all=0
force=0

while [[ $1 == -* ]]
do
	case $1 in 

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

mynode=$( ng_sysname )
rc=1
dflag=""
need="1";		# needed return code from ping test (0, 1, or 2)
test_common=1		# test only the interfaces with neighbours that we have a common network. 

case $1 in
	conn*)		# connectivity to neighbours (conn [-s|-a])
		shift
		while [[ $1 == -* ]]
		do
			case $1 in 
				-A)	test_common=0;;		# test every reported interface not just those we have in common (routing may break)
				-a)	need=0;;		# need a perfect return code from ping test (node can be reached on all/every if)
				-d)	dflag="-d";;
				-s)	short_circuit=1;;
				*)	log_msg "unknown option to connect test: $1";;
			esac

			shift
		done

		conn_check 
		rc=$?
		;;

	netif)					# ensure something is listening for the cluster DNS name
		dflag="-d"			# test using discard service if running; avoids alarms if rebooting 
		ping_test $NG_CLUSTER $NG_CLUSTER
		rc=$?
		case $rc in 
			0|1) 	rc=0;;		# we only need response from some of the interfaces, so either rc is good.
			*)	rc=1;;
		esac

		if (( $rc == 0 ))
		then
			for n in $(ng_species -t !Satellite )
			do
				echo "$n: $( ng_rcmd $n ng_srv_netif verify )"
			done >$TMP/PID$$.netif
			gre -c bound $TMP/PID$$.netif | read count
			if (( $count > 1 ))
			then
				log_msg "more than one ($count) node has the netif IP address assigned [FAIL]"
				rc=1
			else
				log_msg "found just one host with the IP address assigned [OK]"
			fi
	
			if (( ${rc:-0} > 0 || ${verbose:-0} > 0 ))
			then
				cat $TMP/PID$$.netif
			fi
		fi

		;;
	
	service)				# test the ability to reach an off node service 
						# $2==nodename  $3==command to run  $4=response expected ($4 can be null)
		shift
		check_service "$@"
		rc=$?
		;;

	spyglass|remote_spyglass)
		rc=0
		nlist="$( get_neighbours)"
		for x in $nlist 
		do
			if ! check_rmt_spy $x
			then
				rc=1
			fi
		done
		;;

	remote_green_light)			# check remote greenlight on two neighbours
		rc=0
		nlist="$( get_neighbours )"		# get the two nodes adjacent in ngmembers list
		shift
		for x in $nlist
		do
			if ! check_rmt_gl $x
			then
				rc=1
			fi
		done
		;;

	httpd*)
		shift
		rc=0
		httpd_check "$@"
		rc=$?
		;;

	debug)					# debugging of this script
		mynode="$( ng_sysname )"
		echo "interfaces for ${2:-must-supply-host} = $(get_if ${2:-must-supply-host})"
		echo "common interfaces between $mynode and $2: " $(common_nets "$(get_if $mynode)" "$(get_if $2)")
		rc=0
		;;

	*)	log_msg "command token is not recognised: $1"
		cleanup 3;
		;;

	
esac

cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_cknet:Spyglass network probes)

&space
&synop	ng_spy_cknet [-v] conn [-s | -a]
&break	ng_spy_cknet [-v] netif
&break	ng_spy_cknet [-v] remote-green-light
&break	ng_spy_cknet [-v] service [-f] [-m] [-o] service-name command [response]
&break	ng_spy_cknet [-v] spyglass
&break	ng_spy_cknet [-v] httpd host[:port] pagename [key]

&space
&desc	This script provides several network oriented tests that can be 
	driven from Spyglass. One test may be selected on each invocation
	using one of the following keywords:

&space
&subcat Connectivity Test (conn)
	Tests connectivity between the current node and two neighbouring 
	nodes. The neighbour nodes are selected using the list of cluster 
	members returned by &lit(ng_members). The nodes that appear immediatly
	next to the current node in the list are selected for testing.  This allows
	the connectivity test to be executed on every node in the cluster without
	over testing and over reporting.  If a node is not reachable it will be 
	reported by only two nodes, rather than by every node in the cluster. 
&space
	These options may be supplied following the &lit(conn) keyword.
&begterms
&term	-a : A successful test is reported only if ALL of the reported interfaces that are 
	tested can be used to reach the neighbour(s).  If not supplied, the return code will
	indicate an OK state if the neighbours can be reached on any interface. 
&space
&term	-A : Test all interfaces that a neighbour reports.  If this issue is not set, then 
	the neighbour is tesed using only the networks that the two nodes have in common. 
	When this option is used, it is assumed that packets can be routed from the node 
	to the neighbour and will potentially cause false alarms if the neighbour cannot 
	be reached because of routing issues. 
&endterms

&space
&subcat Cluster name DNS test (netif)
	Tests to see if a host responds to the DNS name for the cluster (e.g. kultarr rather than
	kult01). This test is a ping only test to ensure that some node is responding to the cluster 
	name.  The test is NOT intended to verify anything else. 

&spae
&subcat	Remote green-light test
	Checks to see if the greenlight process is running on two neighbour nodes. Neighbour nodes
	are selected from the list returned by &lit(ng_members.)

&space
&subcat Service
	The service check attempts to execute the given command on the node that is currently 
	hosting the named service. The service name, and command are required parameters, and 
	the expected response string is optional.  &This will execute the command on the host
	that has been assigned the service. If the response matches the response string, or 
	is not empy when no response string is given, then a good return code is generated. 
	The command is not executed, and a return code of 3 is generated, if greenlight is 
	not running on the remote node.  The force option overrides the greenlight check 
	and thus the probe will fail if the node is not reachable. 
&space
	The -o flag indicates that the service is optional and if the service name cannot 
	be translated into a host name, it is to be assumed that the service is not desired and 
	that this case is OK.  This flag should not be used if the -m flag is supplied.
&space
	The -m flag indicates that the service is expected to be managed by service manager
	and if it is not in service manger's list, then it is not checked. The -o flag should 
	not be used in conjunction with this flag as it could prevent failurs from being reported. 

&space
	The following are examples:
&space
&litblkb
   ng_spy_cknet service srv_Jobber 'ng_sreq dump 30'
   ng_spy_cknet service srv_Jobber 'ng_nreq explain foo' 'programme not found:'
   ng_spy_cknet service -f srv_TAPE_DUP_NODE
   ng_spy_cknet service -o srv_Catalogue
&litblke

&space
&subcat Spyglass
	Checks to see if the spyglass on two neighbour nodes is active.

&space
&subcat Httpd check
	Causes the script to try to fetch the indicated page (e.g. /~scooter/index.html)
	from the host and port supplied on the command line.  If port is omitted, then 80 is
	assumed.  If the key string is supplied, the resulting document is searched for the 
	string and if not found the document is assumed to be in error and an alarm will be 
	raised.  The key string is case insensitive. If no key is supplied, the string "<html>"
	is used.  An alarm is raised if:
&space
&beglist
&item	A connection to host:port cannot be established.
&item	The request generates an empty data set, or a data set with the string "404" in it (not found).
&item	The key is not foun in the data. 
&endlist

&space
	The resulting output to standard error attempts to explain the reason for the alarm.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose mode.
&endterms


&space
&parms	The first token following any recognised options is taken to be the test name.
	Following the test name options specific to the test may be supplied. 
&space

&space
&exit
	This script exits with one of the following return codes:
&space
&begterms
&term	0 : All connections tested seem to be working. 
&term	1 : One or more connections tested failed, alarm should be sent.
&term	2 : One or more connections tested failed, state is questionable and a retry should be 
	made before an alarm is sent.
&term	3 : The state is not good, but no alarm should be sent. All probes that depend on a 
	good return from this test should be blocked. 
&endterms

&space
&see
	ng_spyglass, ng_spy_ckfsfull, ng_spy_ckdaemons

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	18 Jul 2006 (sd) : Its beginning of time. 
&term	19 Jul 2006 (sd) : Added netif check.
&term	08 Aug 2006 (sd) : Added /usr/etc to support sgi nodes (erk).
&term	09 Aug 2006 (sd) : added remote greenlight check. 
&term	14 Aug 2006 (sd) : Added remote spy check.
&term	26 Sep 2006 (sd) : Change to prevent error if var not set in check remote spyglass function.
&term	04 Oct 2006 (sd) : Set sane limits on all ng_rcmd invocations.
&term	10 Jan 2007 (sd) : Added caching of member list so if filer goes away we have a small hope
	of checking at least the nodes that were our neighbours last time. 
&term	01 Dec 2008 (sd) : Added httpd check.
&term	03 Dec 2008 (sd) : Extended the netif check to ensure that not more than one node has assigned
	the network interface IP address. 
&term	23 Jan 2009 (sd) : Fixed typo in error messages. 
&term	21 Apr 2009 (sd) : Added force option to service, and documented this subtask in the man page.
&term	30 Apr 2009 (sd) : Added -o option to service check to allow optional services that are not managed by service manager to be ignored. 
&term	15 May 2009 (sd) : Corrected problem where a misleading message was issued if all interfaces
		are required to be up, but only some are. 
&term	30 Jun 2009 (sd) : Enhanced the httpd request so that even picky servers like bug tracker will respond. 
&scd_end
------------------------------------------------------------------ */
