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
# Mnemonic:	rmbtreq -- repmgrbt request interface
# Abstract:	This script provides a commandline interface to the 
#		replication manager rmbt process. 
#		
# Date:		3 October 2006	(HBDSC)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function usage 		# replaces stdfun usage
{
	cat <<endKat
usage:  $argv0 -e
	$argv0	[-t sec] ackeod
	$argv0	[-t sec] ackwait seconds
	$argv0	[-t sec] comm seconds
	$argv0	[-t sec] cts
	$argv0	[-t sec] dump <filename>
	$argv0  [-t sec] fetch [<marker-name>]
	$argv0  [-t sec] find <marker-name>
	$argv0	[-t sec] hold
	$argv0	[-t sec] stats
	$argv0	[-t sec] Stats
	$argv0	[-t sec] Statsreset
	$argv0	[-t sec] list 
	$argv0	[-t sec] mark <marker-name>
	$argv0	[-t sec] play [<marker-name>]
	$argv0	[-t sec] purge <filename>
	$argv0	[-t sec] rcpurge <marker-name>
	$argv0	[-t sec] rmhost <node-name>
	$argv0	[-t sec] verbose <value>
endKat
}

# -------------------------------------------------------------------
ustring=""

cmd=""						# except for -e, cmd is $1
host=${srv_Filerbt:-$srv_Filer}			# soon we will split rmbt from rm node and srv_Filerbt will be born
port=${NG_RM_PORT}				# default to the repmgr interface port (29002)
timeout=20

while [[ $1 = -* ]]
do
	case $1 in 
		-e)	cmd=exit;; 			# keep same as other *req interfaces
		-p)	port=$2; shift;;
		-s)	host=$2; shift;;
		-t)	timeout=$2; shift;;

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


case ${cmd:-$1} in
	ackeod|ack) 
		cmd="~ackeod~"
		;;
	
	ackwait)
		cmd="~ackwait~$2";;

	exit)	cmd="~exit~"
		;;

	play)	cmd="~rcplay~$2"		# if $2 is missing then play from top
		;;

	rcdump|fetch)	
		cmd="~rcdump~$2"
		;;

	list)	cmd="~rclist~"
		;;

	find)					# we run this here as the output is exit code and then data
		if [[ -z $2 ]]
		then
			log_msg "required paramater missing"
			usage
			cleanup 1
		fi
		cmd="~rcfind~${2:-foo}"
		echo "$cmd" | ng_sendtcp -t 20 -e "#end#" $host:$port|read rc text
		verbose "$text"
		cleanup $(( ! rc ))
		;;

	dump)	
		if [[ -z $2 ]]
		then
			log_msg "required paramater missing"
			usage
			cleanup 1
		fi
		cmd="~dump~$2"				
		;;

	maxconn) cmd="~conn~$2"
		;;

	
	mark)	
		if [[ -z $2 ]]
		then
			log_msg "required paramater missing"
			usage
			cleanup 1
		fi
		cmd="~rcmark~${2:-foo}"
		;;

	stats|Stats|Statsreset)	cmd="~$1~"
		;;

	cts)	cmd="~ok2send~"
		;;

	hold)	cmd="~hold~"
		;;

	rcpurge)	cmd="~rcpurge~$2"
		;;

	purge)
		cmd="~pdump~${2:-/dev/null}"
		;;

	comm)
		cmd="~comm~${2:-20}"
		;;

	host|rmhost)	cmd="~rmhost~${2:-$srv_Filer}"
		;;

	verbose)	cmd="~verbose~${2:-0}"
		;;

	*)	log_msg "unrecognised command: $1"
		usage;
		cleanup 1
		;;

esac

echo "$cmd" | ng_sendtcp -t $timeout -e "#end#" $host:$port

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(name:short description)

&space
&synop
&break	ng_rmbtreq -e
&break	ng_rmbtreq [-t sec] ackeod
&break	ng_rmbtreq [-t sec] ackwait seconds
&break	ng_rmbtreq [-t sec] comm seconds
&break	ng_rmbtreq [-t sec] cts
&break	ng_rmbtreq [-t sec] dump <filename>
&break	ng_rmbtreq [-t sec] fetch [<marker-name>]
&break	ng_rmbtreq [-t sec] find <marker-name>
&break	ng_rmbtreq [-t sec] hold
&break	ng_rmbtreq [-t sec] stats
&break	ng_rmbtreq [-t sec] Stats
&break	ng_rmbtreq [-t sec] Statsreset
&break	ng_rmbtreq [-t sec] list 
&break	ng_rmbtreq [-t sec] mark <marker-name>
&break	ng_rmbtreq [-t sec] play [<marker-name>]
&break	ng_rmbtreq [-t sec] purge <filename>
&break	ng_rmbtreq [-t sec] rcpurge <marker-name>
&break	ng_rmbtreq [-t sec] verbose <value>

&space
&desc	&This is the interface to the replication manager &lit('bt') process. The &lit(bt)
	process is used to cache commands sent to replication manager via &lit(ng_rmreq).
	Commands are cahced in the &lit(bt) process to prevent their loss if replication 
	manager becomes busy, or crashes.  The commands that are suppored by &this are 
	used to control and pull information from the &lit(bt) process. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-e : Causes the &lit(bt) process to exit.  
&space
&term	-p port : The port that &lit(ng_repmgrbt) is listening to.
&space
&term	-s host : The host that &lit(ng_repmgrbt) is running on. 
&endterms


&space
&parms One or more command line parameters are expected by &this. The first is a command token
	which is followed by any information necessary to execute the command.  The following lists
	the recognised command tokens, and describes the additional paramters required and the 
	action that is taken.

&begterms
&spcae
&term	ackeod : Toggles the send ack on end of data flag.  When set, ng_repmgrbt will send an end of data
	mark to replication manager and wait for a handshake indicating that the marker was received. 

&space
&term	ackwait n : Specifies the number of seconds (n) that repmgrbt will wait for an end of data 
	ack message from replication manager. If not changed, the default is 240 seconds. 
&space
&term	comm : Expects &ital(seconds) to be passed as an additional command line parameter. &lit(ng_repmgrbt)
	uses the value as the maximum number of seconds to hold open a connection with replication 
	manager.  
&space
&term	cts : Sends a 'clear to send' message to &lit(ng_repmgrbt.) &lit(Ng_repmgrbt) does not attempt
	to communicate with replication manager until it has received a clear to send.  This message is 
	also needed after &lit(ng_repmgrbt) has been placed on hold. 
&space
&term	dump :  Expects a filename. This command causes &lit(ng_rmepmgrbt) to dump all buffers that have NOT 
		been sent to the replication manager to the named file.  Mostly this is for debugging, and 
		in some cases used as an emergency method of preserving the state if replication manager
		has crashed and cannot be restarted.
&space
&space
&term	fetch : Optionally accepts  a marker name from  the command line.  The command causes all 
	buffers that are in the 'replay queue,'  starting with the named marker, 
	to be written to the standard output device. The output is the same as would be sent to 
	replicaiton manager if the &lit(play) command were to have been used instead of the &lit(fetch) command. 
&space
&term	find : Searches for the marker name provided and returns with a successful exit code if the 
	marker exists. 
&space
&term	hold : This command causes &lit(ng_repmgrbt) to stop sending buffers to the replication manger. No 
	buffers will be sent until the clear to send command is used.  The replication manager start script
	causes the clear to send command to be sent.
&space
&term	stats :	Causes &lit(ng_repmgrbt) to write some statistics information to its private log file.
	This should not be confused with Stats. 
&space
&term	Stats : Causes a few statistics counters to be written back to the tty device. These counts include
	the number of bytes written to replication manager, the number of sessions that have been established
	and the number of selected repmgr message types. This command is not the same as the &ital(stats) command.
&space
&term	Statsreset : Resets the statistics counters. 
&space
&term	list : 	Requests that &lit(ng_repmgrbt) return a list of markers that are currently known. The markkers, 
	and the number of buffers queued with each, are written to the standard output device.  Markers are 
	written in order such that sending a &lit(play) command with a marker name will cause buffers from 
	that set, and all sets appearing after the named marker, to be sent to replication manager. 
&space
&term	mark : Expects that the marker name is provided on the command line.  Using this command causes
	&lit(ng_repmgrbt) to  set a marker using the given name.  Buffers that are sent to replication 
	manager after the mark command is received are queued for replay using the latest marker name. 
&space
&term	play : Optionally takes a marker name and causes &lit(ng_repmgrbt) to send all messages that were queued
	after the marker was established to replicaiton manager. If no marker name is given, then all buffers
	queued in the replay queue are sent. When replaying, buffers are sent in the order that they were
	originally sent to replication manager, with the exception that exit commands are NOT resent.
&space
&term	purge : Causes &lit(ng_repmgrbt) to purge all buffers that have not been sent to the replication 
	manager.  The filename supplied after the command token will be used to write all of the currently
	queued buffers. If no file name is given, /dev/null is used. 
&space
&term	rcpurge : Causes &lit(ng_repmgrbt) to purge the buffers associated with the named replay cache
	marker.  If no marker name is given, the first cache set of buffers is purged. 
&space
&term	 rmhost name : Causes &lit(ng_repmgrbt) to connect to the replication manager running on the named 
	node. This command is intended mostly to support the move feature of the &lit(ng_srv_repmgr) command. 
&space
&term	verbose : Sets the level of chatting that &lit(ng_repmgrbt) will do to stderr. Valid values are
	{1|2}[0-9].  The ones digit controls the standard level of chatter to stderr, while the tens digit
	turns on special verification logging.  The value of 1 in the tens place logs special messages
	sent to replication manager.  A value of 2 causes all input to &lit(ng_repmgrbt) to be logged. 
	Verificaiton logging is done to the file passed in using the -l command line option. 
&space
&endterms

&space
&exit
	In general, an exit code of 0 means success.  In the case of the &lit(find) command, an exit 
	code of 0 indicates that the marker does exist and that a &lit(play) command can be issued
	for the marker. 

&space
&see
	&lit(ng_repmgr) &lit(ng_repmgrbt) &lit(ng_rmreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Oct 2006 (sd) : Its beginning of time.  (HBD SC)
&term	06 Mar 2007 (sd) : Added support for ackeod.
&term	21 Mar 2007 (sd) : Added support for ackwait.
&term	22 May 2007 (sd) : If purge is given, and the filename is omitted, it now defaults to /dev/null.
&term	10 Jan 2008 (sd) : Added Stats commands.
&term	28 Jan 2009 (sd) : Added support for rmhost command. 


&scd_end
------------------------------------------------------------------ */
