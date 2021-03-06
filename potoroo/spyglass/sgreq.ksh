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
# Mnemonic:	sgreq - spyglass request interface
# Abstract:	provides a commandline interface to spyglass
#		
# Date:		21 July 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function usage
{
	cat <<endKat
usage:  $argv0 [-s host] [-p port] -e
	$argv0 [-s host] [-p port] [-t timeout] command parms

Commands and parameters:
	delete {test|digest} name
	explain [{testname | digest [digest-name]}]
	parse config-filename
	verbose n
	verbose testname {on|off}
	squelch testname {n-seconds|on|off|change|reset}
	State n
endKat
}

# -------------------------------------------------------------------
port=$NG_SPYG_PORT
host=localhost
stop=0
timeout=30
show_alarms=0

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	show_alarms=1;;
		-e)	stop=1;;
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

if (( $stop > 0 ))
then
	echo "xit050705" |ng_sendtcp -t 2 -e "#end#" $vopt $host:$port
	verbose "spyglass on $host has been signaled to stop"
	cleanup 0
fi

if (( ${show_alarms:-0} > 0 ))
then
	verbose "these probes are currently in an alarm or questionable state:"
	echo "list:" | ng_sendtcp -t $timeout -e "#end#" $vopt $host:$port | gre "QUES|ALARM"
	cleanup $(( ! $? ))
fi

case $1 in 
	exit)
		stop=1
		;;

	*)					# default assumes that parms on the command line just need to be : separated
		buf="$*"
		buf="${buf// /:}"
		;;
esac 

echo $buf | ng_sendtcp -t $timeout -e "#end#" $vopt $host:$port
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sgreq:Spyglass request iterface)

&space
&synop	ng_sgreq [-e]
&break	ng_sgreq [-a] [-s host] [p port] [-t timeout] [-v] command

&space
&desc	&This sends a command to &lit(ng_spyglass).  Supported commands are determined
	by &lit(ng_spyglass). Currently the supported commands include:
&space
&begterms
&term	list : List the state of all currently active tests.
&space
&item	explain testname : Explain the current state of the named test.
	If &ital(testname) is not given, all tests are explained. 
&space
&term	squelch testname time : Causes alarms that would be generated by the &ital(test)
	to be slienced until the time supplied.  &ital(time) may be in the form of
	&lit(hh:mm,) or may be either of the words &lit(change) or &lit(reset).  If 
	&lit(change) is supplied, alarms are squelched until a change in the alarm state
	reported by the probe is noticed.  If &lit(reset) is given, then alarms are 
	squelched until the probe returns a normal (ok) state. 
&space
&term	delete test testname : Causes the named test to be deleted from the list of 
	active probes.
&space
&term 	delete digest digestname : Causes the named digest to be deleted. 
&space
&term	parse filename : Causes &lit(ng_spyglass) to read the named config file. 
	When reading a config file, probes that have the same names are overlaid. Existing 
	probes that are not named in the configfile remain &stress(unchanged.) This allows
	for easy real-time modification of existing probes or addition of new probes. 

&endterms

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Show alarms and exit.  This is the same as execuing a list command and 
	grepping for 'ALAMR|QUES'. 
&space
&term	-e : Causes an exit command to be sent to &lit(ng_spyglass).
&space 
&term	-p port : Uses the indicated TCP/IP port when trying to communicate with &lit(nt_spyglass).
	When not supplied, the value assigned to the pinkpages variable &lit(NG_SPYG_PORT) is used.
&space
&term	-s host : Sends the command to the &lit(ng_spyglass) process running on &ital(host). If 
	not supplied on the command line, the command is sent to the &lit(ng_spyglass) process 
	running on the local host. 
&space
&term	-t sec : Timeout.  Will fail if spyglass does not respond in &ital(sec) seconds.
&space
&term	-v : verbose mode. 
&endterms


&space
&parms	The command that is to be sent to &lit(ng_spyglass), and any command parameters, 
	are expected to follow any options supplied on the command line. 

&examp
&litblkb
   ng_sgreq delete test flist-files              # delete a test
   ng_sgreq parse /ningaui/lib/alt_spyglass.cfg  # load an alternate config
   ng_sgreq explain seneschal
   ng_sgreq squelch wado-canon 7200              # slience waldo alarms for 7200 seconds
   ng_sgreq squelch flist-files reset            # silence until state returns to normal
&litblke
&space
&exit
	An exit code of zero indicates that processing completed successfully. Any other return 
	value indicates an error.  Where possible, and error message will be written to 
	the standard error device. 

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	21 Jul 2006 (sd) : Its beginning of time. 
&term	04 Nov 2006 (sd) : Corrected man page.
&term	02 Jan 2007 (sd) : Fixed man page.
&term	19 Aug 2007 (sd) : added timeout option.
&scd_end
------------------------------------------------------------------ */
