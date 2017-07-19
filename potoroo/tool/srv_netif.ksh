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
#!/ningaui/bin/ksh
#!/usr/common/ast/bin/ksh
# Mnemonic:	ng_srv_netif
# Abstract:	provide service manager with the necessary support to probe, start and
#		stop the netif service.  When we start we add an alias to the interface
#		that is currently supporting the address that maps to the nodename.
#		when we stop the service, we put that interface down (linux) or remove
#		it (bsd). When we compute our score, we ensure that we can find an 
#		interface that shows the address that maps to our node name. We use
#		dig to do name to IP4 address translation.
#		
# Date:		9 October 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# linux sets the alias by appending :n to the interface name (erk). 
# try to find one that seems not to be in use for the interface we have selected ($1)
function set_cookie
{
	ifconfig | gre "$1" | awk '
		{			
			if( split( $1, a, ":" ) > 1 )
				used[a[2]+0] = 1;
		}

		END {
			if( ! used[2] )		# prefer this 
			{
				print "2";
				exit( 0 );
			}
			else
			{
				for( i = 1; i < 10; i ++ )
					if( ! used[i] )
						break;
			}

			print i;				# overload, just punt
		}
	'	
}

# given a full name, use dig or or simulate dig output with info from etc/hosts
function dig_it
{
	if (( $use_dns > 0 ))
	then
		verbose "using information in DNS; ignoring /etc/hosts"
		dig $1
	else		
		verbose "DNS information not used; using /etc/hosts"
		# simulate dig output with data from /etc/hosts
		gre "[ 	]+$1" /etc/hosts | awk '
			BEGIN {
				printf( ";; ANSWER SECTION:\n" );
			}
			{
				for( i = 2; i <= NF; i++ )
					printf( "%s %s\n", $(i), $1 );
			}
			END {
				printf( ";;\n" );
			}
		'
	fi
}

# given a fully qualified name (node.organisation.biz.com) find the associated 
# interface and netmask. 
# nslookup is much easier, but seems out of favour so we use dig.
# $1 is the dns name to map  (must have domain attached)
# output is iterface name and netmask we should add an alias to
# we use this to find the interface that we want to attach to, and to see if 
# we have already set-up the alias ($NG_CLUSTER.$NG_DOMAIN is passed in rather than 
# the host name)
function get_if
{
	(dig_it $1; ifconfig -a) | awk -v what="$1" -v quiet=${quiet:-0} '		# dig output must be first
		# ifconfig interpretation
		$2 == "Link" {	ifname = $1; next; }		# linux has to be different

		#inet 135.207.42.241 netmask 0xffffffc0 broadcast 135.207.42.255
		function get_info( ipname, 	i)
		{
			for( i = 1; i <= NF; i++ )
			{
				if( substr( $(i), 1, 5 ) == "Mask:" )		# linux
				{
					split( $(i), a, ":" );
					mask[ipname] = a[2];
				}

				if( $(i) == "netmask" )				# bsd/darwin
				{
					mask[ipname] = $(i+1);
					i++;
				}

				if( $(i) == "broadcast" )
				{
					bcast[ipname] = $(i+1);
					i++;
				}
				if( substr( $(i), 1, 6 ) == "Bcast:" )
				{
					split( $(i), a, ":" );
					bcast[ipname] = a[2];
				}
			}

		}

		/<.*UP.*>/ {
			ifname = $1;
			gsub( ":", "", ifname );
			next;
		}

		/inet[ \t]/ { 

			x = split( $2, a, ":" );		# linux is inet addr:192....
			addr2ifname[a[x]] = ifname; 
			get_info( ifname );
			next; 
		}
			

		# dig message interpretation 
		/^;;.*ANSWER SECTION:/ { snarf=1; next; }
		/^;;/ { snarf = 0;  snarf = 0;  next; }

		snarf > 0 {
			if( $1 == what || $1 == what "." )			# dig seems to need to attach a trailing dot
				ip = $NF;
			next;
		}

		END {
			if( addr2ifname[ip] == ""  && quiet == 0 )
				printf( "could not map ip(%s) from dig/hosts to an interface name [FAIL]\n", ip ) >"/dev/fd/2";

			print addr2ifname[ip], mask[addr2ifname[ip]], bcast[addr2ifname[ip]];	# the interface that matches the ip address for the name given
		}
	'
}


# send out a ping that we hope causes the router to associate our mac with the ip
function send_ping
{
	get_if $NG_CLUSTER.$NG_DOMAIN | read iface netmask bcast                # if it is set already, then do nothing
	address=$NG_CLUSTER

	address=${NG_CLUSTER:-foo}

	ping_opt="-S"
	case $(ng_uname) in
		IRIX64)		ping_opt="-I";;		# these seem to be the special cases
		SunOS)		ping_opt="-i";;

		FreeBSD)	ping_opt="-S";;		# we probably could just default to this
		Linux)		ping_opt="-I"; address=${iface:-not.found};;
		Darwin)		ping_opt="-S";;
	esac

	# linux seems to not list default as default when -n is given. if we dont use -n
	# long router names are truncated by some netstat flavours
	router=$(netstat -rn | awk '/^default/ {d=$2; next;} /^0.0.0.0/  { ld=$2; next} END {if( d ) print d; else print ld}')

	log_msg "sending ping ($ping_opt $address)  to router:  $router"
	ping $ping_opt $address -c 1 $router
}

# -------------------------------------------------------------------
ustring="[-f] [-l] [-p port] [-v] {start|stop|score|query}"

interactive=0		# puts log stuff to a known location and not the tty
quiet=0			# set to true when running verify command
thisnode=$(ng_sysname -s)
full_name="$thisnode.$NG_DOMAIN"	
use_dns=${NG_USE_DNS:-1}

while [[ $1 == -* ]]
do
	case $1 in 
		-f)	interactive=1;;
		-l)	use_dns=0;;
		-p)	port="-p $2"; shift;;

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


if (( $interactive < 1 )) && [[ $1 != "test" ]]
then
	exec 2>>$NG_ROOT/site/log/srv_netif
fi


if [[ -d /usr/etc ]]
then
	PATH=$PATH:/usr/etc		# blasted sgis hide netstat here
fi

case $1 in 
	init*)
		# set up the var in pinkpages that defines the service manager interface to this script
		this=${argv0##*/}
		ng_ppset NG_SRVM_Netif="300+30!1!$this -v start !$this -v stop !$this -v score !$this -v deprecated!$this -v query"
		;;

	test)
		
		get_if $NG_CLUSTER.$NG_DOMAIN |read ifname netmask bcast junk
		echo "inteface that $NG_CLUSTER.$NG_DOMAIN maps to: ${ifname:-not assigned to this node} mast/bcast: $netmask $bcast"
		echo "unalias cookie would be: ${ifname#*:}"

		get_if ${2:-${full_name}} |read ifname netmask bcast junk
		echo "inteface that ${2:-${full_name}} maps to: ${ifname:-not assigned to this node}  netmask/bcast: $netmask $bcast"
		cookie="$( set_cookie ${ifname:-not-found} )"

		echo "alias cookie would be: $cookie"
		cleanup 0
		;;

	start|continue)		# assign to an interface the IP address that matches the cluster name
		address=$NG_CLUSTER
		if [[ -z $address ]]
		then
			ng_log $PRI_ERROR $argv0 "unable to start Netif service; NG_CLUSTER seems undefined ($NG_CLUSTER)"
			cleanup 1
		fi

		get_if $NG_CLUSTER.$NG_DOMAIN | read iface netmask bcast		# if it is set already, then do nothing

		if [[ -n $iface ]]
		then
			log_msg "$NG_CLUSTER.$NG_DOMAIN is already mapped to $iface"
			ng_log $PRI_INFO $argv0 "netif service started (already assigned) on $thisnode: addr=$address interface=$iface"
		else
			get_if $thisnode.$NG_DOMAIN | read iface netmask bcast		# get interface we should add the ip address to
			cookie="$( set_cookie ${iface:-not-found} )"

			if [[ -z $iface ]]
			then
				ng_log $PRI_WARN $argv0 "could not map address ($thisnode) to interface; netif service not started"
				cleanup 1
			fi


			log_msg "adding alias for netif service address=$address interface=$iface netmask=$netmask"
			
			NG_DOSU_ALIASID=${cookie:-2} $NG_ROOT/bin/ng_dosu alias $iface $netmask $bcast		# dosu MUST be full path; actually set it

			if ng_test_nattr Linux		 # Linux seems unable to set bcast/netmask on first command; so do it a second time
			then
				sleep 1
				NG_DOSU_ALIASID=${cookie:-2} $NG_ROOT/bin/ng_dosu alias $iface $netmask $bcast		
			fi

			ng_log $PRI_INFO $argv0 "netif service started on $thisnode: addr=$address interface=$iface"
		fi

	
		log_msg "node attrs before start: `ng_get_nattr`"
		ng_ppset $port srv_Netif=$thisnode 				# mark us as the node hosting the service
		log_msg "node attrs after  start: `ng_get_nattr`"
		ifconfig -a >&2
	
		send_ping			# send a ping in hopes that our mac address is now changed at its end
		;;

	ping)					# manual interface to send another
		send_ping
		;;


	stop)		# find the interface that has the $NG_CLUSTER address attached and run ng_dosu unalias to tear it down
			# we do NOT reset srv_Nattr in pinkpages; the node taking the service might have already switched it
		log_msg "stopping netif service; current attributes: $(ng_get_nattr)"
		get_if $NG_CLUSTER.$NG_DOMAIN | read ifname stuff
		if [[ -z $ifname ]]
		then
			
			ifconfig -a >&2
			log_msg "stop: $NG_CLUSTER.$NG_DOMAIN seems not to be associated with an interface; nothing done [WARN]"
			cleanup 0
		fi

		NG_DOSU_ALIASID=${ifname#*:} $NG_ROOT/bin/ng_dosu -v -v unalias ${ifname%:*}

		ng_log $PRI_INFO $argv0 "netif service stopped on $thisnode"
		log_msg "stop: $NG_CLUSTER.$NG_DOMAIN service stopped [OK]"
		;;

	verify)				# like query, but just checks this node; used to ensure that only one iface has ip address assigned
		quiet=1
		get_if $NG_CLUSTER.$NG_DOMAIN | read ifname junk
		if [[ -n $ifname ]]
		then
			echo "bound to $ifname"
		else
			echo "not assigned"
		fi
		;;

	query)	
		if ng_test_nattr Netif							# we are already signed up as the netif
		then
			get_if $NG_CLUSTER.$NG_DOMAIN | read ifname junk
			if [[ -z $ifname ]]				# we cannot find the address on this set of interfaces
			then
				echo "red:$thisnode"
			else
				echo "green:$thisnode"
			fi
			cleanup 0
		fi

		ng_ppget srv_Netif | read netif			# we are not the netif; see where it is, then run query on that node
		if [[ $netif == $thisnode ]]			# ppvar says it is us, but natter disagrees; this is trouble so mark down
		then
			log_msg "disagreement between ng_test_nattr and srv_Netif ppvar; marking down"
			echo "red:unknown-location"
			cleanup 0
		fi

		if [[ -z $netif ]]
		then
			log_msg "cannot find netif node in pinkpages; marking down"
			echo "red:unknown-location"
			cleanup 0
		fi

		ng_rcmd $netif ng_srv_netif query |read answer
		if [[ -n $answer ]]
		then
			log_msg "got answer from $netif: $answer"
			echo "$answer"
			cleanup 0
		fi

		log_msg "no answer from $netif; marking state as yellow"
		echo "yellow: no answer from ${netif:-srv_Netif was not set}"
		cleanup 0
			
		;;

	score)
		if ng_test_nattr Satellite 
		then
			log_msg "node is Satellite -- cannot host this service"
			echo 0
			cleanup 0
		fi

		ng_ppget srv_Netif | read l
		export srv_Netif=$l			# override what could be stale cartulary

                if [[ -f $NG_ROOT/site/netif.must.score ]]		# nothing overrides the value in here
                then
                        read x y <$NG_ROOT/site/netif.must.score			# read the value we must return and spit it out
			log_msg "score comes from $NG_ROOT/site/netif.must.score: ${x:-0}"
                        echo ${x:-0}
                        cleanup 0
                fi

		if ng_test_nattr Netif				# we are already signed up as the netif
		then
			get_if $NG_CLUSTER.$NG_DOMAIN | read ifname	# and it appears we are configured with the ip
			if [[ -n $ifname ]]
			then
				score=75			# just vote us as the winner
				echo $score
				log_msg "we are the current host ($srv_Netif) and address is configured on $ifname; score: 75"
				cleanup 0
			fi

			score=30				# score higher than default to try to keep it here, unless bumpped off below
			log_msg "we are assigned the service, but do not have it configured, checking for other restrictions"
		else
			score=10	# default -- score better than nodes running jobber/filer
		fi

		# lower our score based on some restrictions

		get_if $thisnode.$NG_DOMAIN  | read ifname
		if [[ -z $ifname ]]
		then
			log_msg "score: cannot find suitable interface; score: 0"
			echo 0
			cleanup 0
		fi

		if ng_test_nattr ${NG_NETIF_AVOID} Jobber Filer Macropod
		then
			score=2
			log_msg "node is already hosting another major service ($(ng_get_nattr -s)), score low: $score"
			echo $score
			cleanup 0
		fi

		log_msg "score: $score"
		echo ${score:-0}
		cleanup 0
		;;

	*)	log_msg "unrecognised command: $*"
		cleanup 0
		;;
esac
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_srv_netif:Service Management Interface For Netif Service)

&space
&synop	ng_srv_netif [-f] [-l] [-p port] [-v] {query|score|start|stop}

&space
&desc	&This is the interface that the service manager uses to determine the 
	current state of the network interface node service (query), compute the node's
	ability to host the service (score), start and to stop the service. 
&space
	The pinkpages variable &lit(NG_DOMAIN) must be defined for the Netif service to 
	be successfully managed with &this. The domain is appended to the node name
	in order to properly translate names to IP addressess.  The pinkpages variable
	&lit(NG_CLUSTER) must also be defined and is assumed to be the name that is 
	defined to DNS to the IP address that the Netif node will use. 

&space
&subcat	Forcing The Interface Selection
	If the pinkpages variable &lit(NG_NETIF_IF) is set &this will use the named 
	interface rather than trying to determine the interface to use.  This 
	allows an interface other than the one that supports the address associated
	with the node name to be used. 

&space
&subcat Avoiding Nodes With Services
	By default, a low score will be generated if a node is running any of these
	services: Jobber, Filer, or Macropod.
	If additioinal service nodes are to be avoided, they can be added to the 
	pinkpages variable NG_NETIF_AVOID and a low score will be generated if 
	any of the listed services are currently assigned to the node executing 
	the script.  The list is a space separated set of service names (e.g. Tape Tpchanger).
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f : Interfactive mode. If not supplied, output (stdout and stderr) is written to 
	a private log file in $NG_ROOT/site/log.
&space
&term	-l : Local mode. The information in DNS is not sussed out and the contents of /etc/hosts
	is assumed to have both  a fully quallified entry for the local node and for the cluster
	name. The pinkpags variable (NG_USE_DNS) can be set to 0 to force local mode even if this 
	option is not supplied. 
&space
&term	-p port : The port to pass on &lit(ng_ppset) calls. Mostly for testing using an 
	alternate narbalek.
&space
&term	-v : Verbose mode. 
&endterms

&space
&parms	One of four command tokens is expected:
&space
&begterms
&term	init : Causes the service manager pinkpages variable for this service to be defined. 
	The service manager variable consists of the command line commands to invoke the start, 
	stop, query and score functions of this service as well as election information. See 
	the ng_srvmgr manual page for the specifics about the variable.  
&space
&term	ping : Send a ping to the default router in hopes that the mac address associated with the 
	interface that the netif IP address has been aliased to will be registered by the router. 
	This is done automatically when the service is started and this command is provided only 
	as a manual way to execute the function (service manager will not need to drive this script
	with this subcommand).
&space
&term	query : Causes &this to determine the state of the netif service. A string 
	(green:<node>, yellow:<node>, or red:<node>) is written to the standard output
	device to indicate the current state. 
&space
&term	score : This command initiates the calculation routine(s) which determine the 
	node's ability to host the service.  A numeric value (0 through 99 inclusive) is 
	echoed to the standard output device. The larger the value the better the ability
	of the node to host the service. When a value of 0 is generated, &this expects that
	the service manager will &stress(NOT) assign the service to the node. 
&space
&term	start : Causes the service to be started on the node.  
&space
&term	stop : Causes the service to be stopped on the node. 
&space
&term	verify : Like query, but simply reports whether an interface on the current node or not. 
	Output to stdout is either "bound to <name>" or "not assigned."  
	.** the primary use of this function is for spyglass probes that ensure there is only one node in the cluster that has the ip address 
&endterms
&endterms

&space
&exit	An exit code of zero inidicates that all processing completed normally and without
	errors.  A non-zero return code indicates a failure of somesort. 

&space
&see	ng_srvmgr, ng_srv_logger, ng_srv_jobber

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	09 Oct 2006 (sd) : Its beginning of time. 
&term	07 Feb 2007 (sd) : fixed call to ppset. 
&term	14 Feb 2007 (sd) : Now sets netmask on ng_dosu call (for linux nodes).
&term	19 Mar 2007 (sd) : Fixed issue with query; was getting a blank so the -z test was failing.
&term	30 Mar 2007 (sd) : Removed reference to specific node/domain in a comment.
&term	18 Apr 2007 (sd) : Fixed problem in stop logic -- call to ng_dosu was commented out.
		Also fixed issue with opening private log file.
&term	16 May 2007 (sd) : Now passes in the broadcast address so we can mimic the real interface
		on linux. We also better choose the interface alias number for Linux. Added a second
		call do dosu when adding alias -- seems linux needs this as it gives errors when 
		trying to set the netmask/bcast address all on one call. 
&term	17 May 2007 (sd) : Fixed score process such that if we are assigned the service, but it is not 
		configured to an interface, we still check for Jobber/Filer or other major service. 
&term	19 Jun 2007 (sd) : Now dumps ifconfig output to stderr when starting.
&term	03 Dec 2007 (sd) : Added self initialisation. 
&term	21 May 2008 (sd) : Added -l option and ability to ignore dns and use /etc/host info.
&term	24 Jul 2008 (sd) : Added a ping command to send a ping and corrected issue with sending ping from linux.   .** (HBD EKD/ESD)
&term	31 Jul 2008 (sd) : Corrected problem in ping function.
&term	03 Dec 2008 (sd) : Added verify functionality.
&term	06 Jan 2008 (sd) : When verify command is used, the message 'could not map address to dig info' is surpressed as it is not an error.
&term	02 Oct 2009 (sd) : Added support for NG_NETIF_AVOID list.
&endterms

&scd_end
------------------------------------------------------------------ */
