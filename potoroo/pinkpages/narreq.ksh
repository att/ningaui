#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
# Mnemonic:	ng_narreq
# Abstract:	command line interface to narbalek
#		
# Date:		24 Aug 2007 (productionalised an old script)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function do_diff
{
	ng_rcmd $1 ng_nar_get |gre pinkpages|sort -u >/tmp/PID$$.1nar
	ng_rcmd $2 ng_nar_get |gre pinkpages|sort -u >/tmp/PID$$.2nar
	diff /tmp/PID$$.[12]nar |more
}

function do_backup
{
	if ! cd $NG_ROOT/site/narbalek
	then
		log_msg "cannot switch to $NG_ROOT/site/narbalek"
		cleanup 1
	fi

	d=$(date "+%d")
	ls -t narbalek.$league.h* | while read f
	do
		if [[ -s $f ]]				# find most recent hourly that has size
		then
			verbose "backup created: $f -> narbalek.$league.d$d"
			cp $f narbalek.$league.d$d
			cleanup 0
		fi
	done

	log_msg "could not find a good file to backup!"
	ls -alt narbalek.$league.h* >&2
	cleanup 1
}

#---------------------------------------------------------------------

ustring="[-a | -s host] [-b] [-d] [-D nodea nodeb] [-e] [-p port] [-v] [command]"
node=""
cmd=""
delivery="ng_sendtcp -e #end#" 		# default method of sending command to narbalek, dump uses udp
port=$NG_NARBALEK_PORT
verbose=0
backup=0
league="${NG_NARBALEK_LEAGUE:-flock}"
timeout=10

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	node="$(ng_members)";;
		-b)	backup=1;; 
		-d)	delivery="ng_sendudp -q 1 -r . -t 1"; cmd="0:DUMP";;
		-D)	do_diff $2 $3; exit $?;;
		-e)	cmd="~xit4360";; 
		-p)	port=$2; shift;;
		-s)	node="$node$2 "; shift;;

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

if (( $backup > 0 ))
then
	do_backup 
	cleanup $?
fi

if [[ -z $cmd && -n $1 ]]
then
	if [[ $1 = "exit" ]]
	then
		cmd="~xit4360"
	else
                case $1 in
                        shuffle)        timeout=1; cmd="Tshuffle";;             # convert to tree command
			log)		timeout=1; cmd="log";;			
                        *)              cmd="~$@"
                esac
	fi
fi

if [[ -n $cmd ]]
then
	for n in ${node:-localhost}
	do
		echo $cmd | $delivery -t $timeout $n:$port
	done
fi




cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_narreq:Command line interface to narbalek)

&space

&synop  ng_narreq [-p] [-s host|-a] command [parms]
&break	ng_narreq -b
&break	ng_narreq -e [-p port] [-s host | -a]
&break  ng_narreq -d [-p] [-s host | -a]
&break  ng_narreq -D [-p] node1 node2

&space
&desc	&This provides a command line interface to narbalek.  The general form of the command 
	accepts a command and parameters that are passed directly to narbalek. Other options 	
	may be used to perform tasks such as creating a daily backup of the checkpoint file, 
	and causing narbalek to exit (consistant command form with other ningaui daemons).

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Send/apply command to all narbaleks running on the cluster. 
&space
&term	-b : Create a daily backup of the checkpoint file. Narbalek creates a checkpoint file 
	about every 5 minutes in &lit(NG_ROOT/site/narbalek), and keeps the most recent checkpoint
	for each hour of the day.  When this option is used, &this will find the most recent 
	checkpoint file, that is not empty, and will copy it to a daily file in the same directory.
	The daily files are numbered 0-n such that one file is kept per day provided that &this 
	is executed on a daily basis.
&space
&term	-d : Send a &lit(dump) command to narbalek and wait for a response. The output of a dump 
	command is a one line set of statistics that lists the narbalek's place in the communication 
	tree and other information. If the -a option is used, the dump command will be sent to all 
	narbalek processes running on the cluster. 
&space
&term	-D node node : Extract the pinkpages information from narbalek processes running on the 
	two named nodes and generate a difference. 
&space
&term	-p port : Uses the named port rather than &lit(NG_NARBALEK_PORT).
&space
&term	-s host : Sends the command to the narbalek on &ital(host) rather than to the localhost. 
	This option and -a are mutually exclusive; the last one on the command line will
	be the one used.  Host may be a space separated list of node names if the command is 
	to be executed on more than one node. 
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms	Unless one of the dump, difference, or backup options are used on the command line, &this
	expects that a command, and possibly parameter tokens, will be passed in following the options. 
	These tokens are passed directly to narbalek; &this makes no attempt to validate the command. 

&space
&exit	An exit code that is zero indicates success; all others indicate failure.

&space
&see	ng_narbalek, ng_nar_sut, ng_nar_get, ng_ppget, ng_ppset, ng_nnsd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Aug 2007 (sd) : Its beginning of time. (converted from quick and dirty script)
&term	20 Aug 2007 (sd) : Corrected default setting of backup.
&term	30 Sep 2008 (sd) : Modification to support backing up from the proper league not the 
		assumption that the league will always be flock. League is set based on 
		the pinkpages variable NG_NARBALEK_LEAGUE.
&term	14 Jan 2009 (sd) : Corrected missing quote on var assignment. 
&term	21 Jan 2009 (sd) : Added shuffle command such that it is sent as a 'tree' command.
&endterms


&scd_end
------------------------------------------------------------------ */

