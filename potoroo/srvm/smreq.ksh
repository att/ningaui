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
# Mnemonic:	ng_smreq
# Abstract:	service manager request interface 
#		
# Date:		09 June 2006 (HBD SEZ)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function get_root
{
	if [[ -z $root ]]			# not on command line
	then
		verbose "sussing out root of tree..."
		ng_nar_map -t 5 -L srvm_$cluster $multicast_opt -p $port|awk '
		/I_AM_ROOT!/ { print $3; exit( 0 ) }
		' |read root

		if [[ -z $root ]]
		then
			log_msg "cannot determine root of tree for port $port"
			cleanup 2
		fi

		verbose "$root is the root node of the service manager process tree"
	fi
}

function add_service
{
	NG_SRVM_LIST="$(ng_ppget NG_SRVM_LIST)"		# must have fresh copy
	if [[ "!"$NG_SRVM_LIST"!" != *"!$1!"* ]]
	then
		l="$NG_SRVM_LIST!$1"
		l=${l/!!/!}
		ng_ppset NG_SRVM_LIST="$l"
		verbose "new service list: $l"
	else
		log_msg "service ($1) already in list: $NG_SRVM_LIST"
	fi
}

function del_service
{
	NG_SRVM_LIST="$(ng_ppget NG_SRVM_LIST)"		# must have current value, not cartulary cache
	if [[ "!"$NG_SRVM_LIST"!" == *"!$1!"* ]]
	then
		eval l=\"\${NG_SRVM_LIST/$1/}\"		# ditch the name from the list
		l=${l/!!/!}				# clear double bang
		l=${l/%!/}				# clear trailing !
		ng_ppset NG_SRVM_LIST="$l"
		verbose "new list set: $l"
	else
		log_msg "service ($1) not in list; no action taken ($NG_SRVM_LIST)"
	fi
}

# -------------------------------------------------------------------
ustring="[-e] [-M] [-p port] [-r] [-t] [-s node] command"
port=${NG_SRVM_PORT:-4444}
cluster=$NG_CLUSTER
root=
cmd=
multicast_opt=

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster=$2; 
			shift
			port="$( ng_ppget -h $cluster NG_SRVM_PORT )"
			;;
		-d)	cmd=dump_raw;; 
		-e)	cmd=exit; root=localhost;;
		-M)	multicast_opt="-M";;
		-p)	port=$2; shift;;
		-r)	cmd=root;;
		-R)	cmd=raw;;
		-s)	root=$2; shift;;		# if not supplied, we figure it out which can be slower. 
		-t)	cmd=tree;;

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


# some commands are implemented here, others sent to srvm for execution
case ${cmd:-$1} in
	exit)	
			get_root
			cmd="xit2361"
			;;

	add)		add_service $2
			cleanup $?
			;;

	del)		del_service $2
			cleanup $?
			;;

	raw)		ng_nar_map -L srvm_$cluster $multicast_opt -p $port
			cleanup $?
			;;

	tree)		ng_nar_map -L srvm_$cluster -t $multicast_opt -p $port
			cleanup $?
			;;

	dump_raw)	ng_nar_map -L srvm_$cluster $multicast_opt -p $port
			cleanup $?
			;;

	dump)	
			get_root
			cmd="Dump:"
			;;

	explain)	
			get_root
			cmd="explain:${2:-missing-service-name}"
			;;

	list)		echo ${NG_SRVM_LIST//!/ }
			cleanup 0
			;;

	recap)		get_root
			cmd="recap:${2:-missing-service-name}"
			;;

	force)
			get_root
			cmd="force:${2:-missing-service-name}"
			;;

	verbose)	
			root=${root:-localhost}		# default this to local copy
			cmd="Verbose:${2:-1}"
			;;

	root)		get_root
			echo $root
			cleanup 0
			;;

	stop)		
			cmd="stop:$2"
			root=localhost;
			;;

	*)		log_msg "unrecognised command token: ${cmd:-$1}"
			usage
			cleanup 1
			;;
esac

echo "$cmd"  |ng_sendtcp -e "#end#"  ${root:-bad-root-node}:${port:-bad-port}
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_smreq:Service Manager Request Interface)

&space
&synop	ng_smreq [-M] [-p port] [-s host] [-v] request-name [request-data]
&break	ng_smreq [-M] [-p port] [-s host] [-v]  {-r | -t}

&space
&desc	&This is provides a simple interface to the service manager listening on the 
	supplied port (-p) or &lit(NG_SRVM_PORT) if the &lit(-p) option is not supplied.
	The request supplied on the command line is formatted and passed to the 
	service management process for action. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-M : Use multicast mode to communicate with the process(es). 
&space
&term	-p port : Supplies the port that the service management process is listening on.
&space
&term	-s host : Causes the request to be sent directly to the named host.  If this is 
	not supplied, then the request is sent to the controlling service management process
	(the process tree root).  Determining the controlling process can take several seconds
	so it is advantagious to supply this information. 
&space
&term	-r : Susses out the process tree root and writes it to standard output. 
&space
&term	-t : Presents a caracter based drawing depicting the process tree that the service management
	processes have organised themselves into. 
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms
	Tokens following any  command line options are assumed to be the request token and any 
	data needed for the request.  The following describes the request tokens that are 
	recognised, and the action that is caused when they are executed.
&space
&begterms
&term	add service-name : Adds the service to the list of services that is currently being managed
	by service manager (updates the NG_SRVM_LIST pinkpages variable).  
	This command does not add the NG_SRVM_<service-name> variable which is usually created by 
	executing the &lit(init) command of the service interface (e.g. ng_srv_netif init). 
&space
&term	del service-name : Remove the service from the list. 
&space
&term	dump : Causes information about all managed services to be written to standard output. 
&space
&term	explain : Accepts a service name as data and causes the service manager to list information that 
	it maintains about the service. Information is written to the standard output device. 
&space
&term	force : Accepts a service name and causes the controlling process to force an election for the 
	service.  Elections are normally only held when the control process believes that the service 
	is not running. 
&space
&term	 list : Gets a list of service names that are currently being managed by service manager. 
&space
&term	reset : Accepts the name of a service and a number of seconts.  The command causes service manager to 
	reset the lease to the amount of seconds provided. 
&space
&term	recap : Accepts the name of a service and generates a recap of the decision process based on the 
	last set of results. This is of little use than for debugging service manager. 
&space
&term	stop service : Forces the service to be stopped on the local node. The service is NOT stopped 
	if it is not running on another node. This command is used to stop services on the local node 
	when the node is shutting down.  Service manager will eventually recognise that the service is 
	insactive and will hold another election and restart the service.  If the service is to be stopped
	across the cluster, the &lit(del) subcommand should be used (e.g. &this del Jobber).
&space
&term	verbose : Accepts a numeric value as its data and requests that the service manager process set its
	verbose level to that value. 
&space
&term	
&endterms

&space
&exit	Returns 0 unless there was an error.

&space
&see	ng_srvmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	09 Jun 2006 (sd) : Its beginning of time.  (HBD SEZ)
&term	21 Jun 2006 (sd) : Sets port when -c is given based on NG_SRVM_PORT for that cluster.
&term	19 Jun 2008 (sd) : Added support for recap command.
&term 	15 Apr 2009 (sd) : Implemented stop, add and del commands.
&term	20 Oct 2009 (sd) : Corrected verbose option so that it sends it to localhost. 


&scd_end
------------------------------------------------------------------ */
