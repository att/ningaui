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
# Mnemonic:	end_proc
# Abstract:	this script kills a daemon by command name.
#		by default the daemon is killed outright; a command to nicely 
#		stop the daemon can be supplied
#		
# Date:		06 Nov 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-c stop-command] [-n] [-v] daemon_name [parm-string]"

cmd="kill -9 _PID_"	# default to a hard kill"
verbose=0
vflag=""
forreal=1		# -n turns off and we just say what we might do

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cmd="$2"; shift;;

		-n)	forreal=0;; 
		-v)	verbose=1; vflag="-v$vflag ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

plist=$TMP/pslist.$$
dname="$1"		# daemon name
dparms="$2"

if [[ -z $dname ]]
then
	log_msg "missing daemon name"
	usage
	cleanup 1
fi

ng_ps >$plist		# capture so we can run it twice if in verbose mode

ng_proc_info $dname "$dparms" <$plist | read pids

if [[ -z $pids ]]
then
	log_msg "no processes found that match command=$dname with parms=$dparms"
	cleanup 0
fi

if (( $verbose > 0 )) || (( $forreal < 1 ))
then
	log_msg "matching processes:"
	awk -v pid_list="$pids" '
		BEGIN {
			n = split( pid_list, pid, " " );
		}

		{
			for( i = 1; i <= n; i++ )
				if( $2 == pid[i] )
					printf( "\t%s\n", $0 );
		}
	'<$plist
fi

if (( $forreal < 1 ))
then
	cmd="echo would kill/stop pid=_PID_ with: '$cmd'";
fi

for p in $pids
do
	eval ${cmd//_PID_/$p}		
done


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_end_proc:Cause a daemon to stop)

&space
&synop	ng_end_proc [-c stop-command] [-n] [-v] command-name [unique-parm]

&space
&desc	&This will suss out all processes currently executing on the node that match 
	the command-name given on the command line. If &ital(unique-parm) is 
	also given, then only those processes that are executing that have the 
	&ital(unique-parm) as a part of the parameter list displayed by the &lit(ps) 
	command will be selected. 
&space
	For each selescted process, the &ital(stop-command) is executed. If no 
	&ital(stop-command) is provided on the command line, then a hard kill 
	is executed for the process. 
&space
	If the process that is being ended with &this was started with &lit(ng_endless,)
	Then the controlling endless command should be stopped first; unless the 
	desired result is to have the process restarted after &this stops it.

&space
&subcat	Limitations
	This script will function as documented only under operating systems that provide
	a suficent amount of command parameters via the &lit(ps) command.  There are 
	some flavours of O/S that truncate the command parameters in a ps listing so 
	severely as to make it impossible to find a command/unique-parm pair in the 
	list. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c stop-cmd : The command that should be executed to stop the process. The 
	string _PID_ in the command will be replaced with the actual process id of the 
	process as the command is invoked. 
&space
&term 	-n : No action mode. The script will list the processes that it would have taken 
	actaion on, and the command(s) that would have been invoked, but will not actually
	do anything real.
&space
&term	-v : Verbose mode. Script is chatty on stderr when this is set. 
&endterms


&space
&parms
	&This expects that the command name of the process that is to be kill is supplied. 
	An optional parameter, a unique string, can be supplied which is used to select
	the proper process if multiple instances of the same command are executing.  
	

&space
&exit	An exit code that is not zero indicates an error. 

&examp
	The following command causes all ng_valet processes to be killed:
&litblkb
  ng_end_proc -v ng_valet
&litblke
&space
	This command causes only the valet process started against the pk08h filesystem 
	to be stopped:
&space
&litblkb
  ng_end_proc -v ng_valet pk08h
&litblke
&space

	This command sends an exit command to the process rather than having it killed:
&space
&litblkb
  ng_end_proc -c 'echo "~Quit" |ng_sendudp localhost:$NG_PARROT_PORT' ng_parrot 
&litblke
&space
	It should be noted that the port variable is expanded as the command is executed, and not 
	as &lit(ng_end_proc) is invoked. 

&space
&see	ng_proc_info, ng_ps, ng_endless

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Nov 2006 (sd) : The start. 


&scd_end
------------------------------------------------------------------ */
