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
# Mnemonic:	send_rmdump -- send the replication manager dump to another node
# Abstract:	Copies the replication manager dump file to another node and then
#		executes a command on that node to move the file in place.
#		
# Date:		12 May 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

logf=$NG_ROOT/site/log/send_rmdump
if ! [[ "$@" == *-f* || "$@" ==  *-\?* || "$@" == *--man* ]]
then
	log_msg "detaching from controlling tty; messages going to $logf"
	$detach
	exec >$logf 2>&1
fi

# -------------------------------------------------------------------
ustring="[-p seconds] [-d host]"
dest=
delay=120
dest=""
tries=30		# we will retry for about 5min if we cannot find root on the remote node

while [[ $1 == -* ]]
do
	case $1 in 
		-f)	;;			# foreground -- ignore the option
		-d)	dest=$2; shift;;
		-p)	delay=$2; shift;;

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

me=$(ng_sysname)
if [[ $srv_Filer != $me ]]
then
	log_msg "this must execute on the filer; $me seems not to be filer ($srv_Filer)"
	cleanup 1
fi

if [[ -z ${dest:-$srv_Jobber} ]]
then
	log_msg "unable to determine destination; is srv_Jobber defined?"
	usage
	cleanup 1
fi

exec >$NG_ROOT/site/log/send_rmdump 2>&1

while [[ -z $droot ]] && (( $tries > 0 ))
do
	ng_rcmd ${dest:-$srv_Jobber} "ng_wrapper eval echo \$NG_ROOT" |read droot		# root on the dest node
	if [[ -n $droot ]]  
	then
		break;
	fi

	tries=$(( $tries - 1 ))
	log_msg "unable to find root on dest node: ${dest:-$srv_Jobber}; retry in 10s"
	sleep 10
done

if [[ -z $droot ]]
then
	log_msg "giving up: unable to find root on dest node: $dest"
	cleanup 2
fi

verbose "dump file will be sent to: ${dest:-$srv_Jobber}"
ng_goaway -r $argv0
while true
do
	if ng_goaway -c $argv0
	then
		log_msg "found goaway file; exit"
		cleanup 0
	fi

	ng_ppget srv_Jobber|read srv_Jobber
	if [[ ${dest:-$srv_Jobber} != $me ]]			# run only if dest is not this node
	then
		if  ng_ccp -d $droot/site/rm/dump.x ${dest:-$srv_Jobber} $NG_ROOT/site/rm/dump	# send
		then
			ng_rcmd ${dest:-$srv_Jobber} mv $droot/site/rm/dump.x $droot/site/rm/dump	# move into place
			ng_rcmd ${dest:-$srv_Jobber} ls -al $droot/site/rm/dump >$NG_ROOT/site/log/send_rmdump.last
		else
			log_msg "copy failed!" 
		fi
	fi

	sleep $delay
done


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_send_rmdump:Send a repmgr dump file from leader)

&space
&synop	ng_send_rmdump [-p seconds] [-d dest-node]

&space
&desc	&This will copy the replication manager dump file to another node (dest-node)
	every &ital(seconds) seconds.  This exists primarly to support nawab and seneschal
	running on a node different than replication manager, and is necessary only until 
	replication manger supports queries via TCP.

&space
	While seneschal does not need this process any longer, there are other applications that 
	do depend on a local copy of the replication manager dump file, so this script is 
	NOT deprecated.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-d dest host : Defines the host to which the file is coppied.
&space
&term	-p sec : The number of seconds to pause between copies.  If not supplied the default
	is 90 seconds.
&endterms


&space
&see	ng_seneschal ng_repmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 May 2004 (sd) : Fresh from the oven.
&term	01 Nov 2004 (sd) : fixed leader reference.
&term	23 Mar 2005 (sd) : Automated dest so that it will follow srv_Jobber.
&term	13 Oct 2008 (sd) : Changed to prevent sending to the current host if Filer and dest node are the same.
&term	24 Feb 2009 (sd) : Added retry logic to find root on a node that is being bounced. 
&endterms

&scd_end
------------------------------------------------------------------ */
