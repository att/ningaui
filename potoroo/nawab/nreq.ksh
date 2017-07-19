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
#
# Mnemonic:	nreq.ksh
# Abstract: 	Interface to nawab; provides consistency with seneschal, woomera, repmgr etc.
# Date:		28 January 2002
# Author:	E. Scott Daniels
# -----------------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function usage 		# overrides stdfun version
{
	cat <<endcat >&2

	usage: $argv0 [-p port] [-s host] [-t secs] -l [<]programme-file
	       $argv0 [-p port] [-s host] [-t secs] -e
	       $argv0 [-p port] [-s host] [-t secs] nawab-command args

	Programme order:
		programme [name[_]];
			%variable = value
			step_name: [after <stepname>] [<iteration>] "comment"
				[consumes = resource[,resource...]
				[priority = n]
				[attempts = n] 
				[nodes = {attributes}|nodename|!nodename] 
				[woomera = "woomera-resources"] 
				[required_inputs = n]
				[<- %input-var [=file]] 
				[-> %output-var = file] 
				cmd command string
endcat
}

function submit_programme
{
	typeset pf=$1
	typeset last_errs=""
	load_status=0				# this is global

	if [[ $timeout -lt 10000 ]]
	then
		timeout=10000					# big programmes could need a while to get in, or to help with old nawab 
	fi

	ng_sendtcp $v -t $timeout -d $jobhost:$port <$pf |  while read errs msg
	do	
		printf "$errs %s\n" "$msg" >&2
		if [[ -n $errs ]]			# seems we are getting a blank line from nawab, don't loose the real last info
		then
			last_errs="$errs"
		fi
	done	

	if [[ -z "$last_errs" ]]
	then
		load_status=1
		return 1				# cause to retry if desired
	fi

	if [[ ${last_errs:-1} -ne 0 ]]			# if missing assume it failed 
	then
		error_msg "programme load failed."
		load_status=1
		return 0				# we dont retry this kind
	fi		

	return 0
}
	
# -------------------------------------------------------------------

stop=0			# set to 1 to exit by -e flag
load=""			# load programme and send to nawab
timeout=30
verbose=0
delay=5			# amount of time (sec) to pause between programme load attempts
port=$NG_NAWAB_PORT	# default port number
asis=0
attempt=1		# number of times to attempt programme submission (-A changes)
load_status=0		# 
ng_sysname -s |read myhost

if [[ "$*" != *-e* ]]		# if shutting down they MUST supply the host name or we assume this host
then
	#ng_ppget srv_Jobber| read jobhost	# default host to send request to
	jobhost=${srv_Jobber:-no_jobhost}
else
	jobhost="$myhost"
fi

while [[ $1 = -* ]]
do
	case $1 in
		-a)	asis=1;;		# do not attempt to convert name to prog troupe job
		-A)	attempt=$2; shift;;

		-c)	cluster=$2; shift;;
		-c*)	cluster="${1#-?}";;

		-e)	stop=1;;

		-l)	load=1;;

		-p)	port=$2; shift;;

		-s)	jobhost="$2"; shift;;
		-s*)	jobhost="${1#-?}";;

		-t)	timeout="$2"; shift;;
		-t*)	timeout="${1#-?}";;

		-v)	verbose=1
			v="$v-v "
			;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -n $cluster ]]
then
	#ng_rcmd $cluster ng_ppget srv_Jobber|read jobhost
	#ng_rcmd $cluster ng_wrapper 'eval echo \$srv_Jobber'|read jobhost
	ng_ppget -C $cluster srv_Jobber|read jobhost
	if [[ -z $jobhost ]]
	then
		log_msg "unable to determine jobber for cluster: $cluster"
		cleanup 1
	fi
	verbose "communication will be with: $jobhost:nawab"
fi


if [[ -z $port ]]
then
	error_msg "unable to find cartulary/env variable: NG_NAWAB_PORT"
	cleanup 1
else
	verbose "using port: $port"
fi

if [[ stop -gt 0 ]]
then
	if [[ $load -gt 0 ]]
	then
		error_msg "conflicting options. specify either -e or -l, but not both!"
		usage
		cleanup 1
	fi

	if ! echo "exit" | ng_sendudp -t $timeout $jobhost:$port
	then
		error_msg "unable to communciate with nawab at: $jobhost:$port"
		cleanup 1
	fi
	cleanup 0
fi

if [[ $load -gt 0 ]]
then
	pgmf=/tmp/nreq_pgm.$$
	node=$(ng_sysname)

				# capture programme from file or stdin; add end line as nawab expects (=end<username>:<nodename>)
	awk -v node="$node" -v uname="${USER:-$LOGNAME}" '{print} END{ printf( "=end%s:%s\n", uname, node ); }' <${1:-/dev/fd/0} >$pgmf		

	if [[ -s $pgmf ]]
	then
		while ! submit_programme $pgmf
		do
			attempt=$(( $attempt - 1 ))
			if (( $attempt < 1 ))
			then
				error_msg  "unable to communciate with nawab at: $jobhost:$port; giving up"
				cleanup 1
			fi
			verbose  "unable to communciate with nawab at: $jobhost:$port; will retry after short delay (${delay}s)"
			sleep ${delay%%.*}
			delay=$(( $delay * 2.5 ))
		done

		cleanup $load_status
	else
		error_msg "programme contained no records; not submitted"
		cleanup 1
	fi

else
	if [[ $asis -gt 0 ]]
	then
		msg="$*"			# send it along unchanged
	else
		for x in $*
		do
			case $x in 
				ex)	msg="${msg}explain ";;		# bug in nawab assumed ex meant exit
				*.*_*)					# assume <troupe>.<programme>_<jobnumber>  prog/troup may have _
					troupe="${x%%.*}"
					prog="${x#*.}"
					jobnum="${prog##*_}"		# job number is everything after last _
					prog="${prog%_*}"			# last _ and what is after is job number
					msg="$msg$prog $troupe $jobnum "		# add as three components ordered nicely
					;;

				*)	msg="$msg$x ";;
			esac
		done
	fi

	verbose "$msg"

	echo "$msg" | ng_sendudp $v -t $timeout -r "." $jobhost:$port
	rc=$?
	if [[ $rc -ne 0 ]]
	then
		error_msg "unable to communciate with nawab at: $jobhost:$port ($rc)"
		cleanup 1
	fi
fi

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nreq:Nawab Request - External Interface)

&space
&synop	ng_nreq [-A attempts] [-s host] [-t secs] -l [<]programme-file
&break  ng_nreq [-s host] [-t secs] -e
&break	ng_nreq [-s host] [-t secs] nawab-command args

&space
&desc	&This is the external itnterface to &ital(nawab.) It can be used to load a 
	programme into, send a command to, or to stop &ital(nawab.)

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-A n : Make &ital(n) attempts when trying to load a programme if communicaiton with nawab
	cannot be established, or is unexpectedly broken.  Between each attempt the script will 
	sleep a few seconds; the period of delay increases with each attempt. 
&space
&term	-c cluster : Causes the job host on &ital(cluster) to be located and all communications
	with nawab will assume that nawab is executing on that host.  
&space
&term	-e : Cause &ital(nawab) to exit gracefully.
&space
&term	-l : Load programme. The programme is read from standard input unless a postitional 
	parameter is given.  If a positional parameter appears on the command line, then
	&this assumes it to be the name of the programme file and will send that file to 
	&ital(nawab.) (Please refer to the &ital(nawab) manual page for a completed 
	description of its programming language.)
&space
&term	-p port : Allows user to override the port number that &ital(nawab) may be listening 
	on. If not supplied the value &lit(NG_NAWAB_PORT) is expected to be defined 
	in the cartulary.
&space
&term	-s host : Specifies the host to which &this attempts to communicate with. If 
	this option is not supplied, the cluster job host (srv_Jobber) is assumed to be the host on which
	&ital(nawab) is executing, unless the exit (-e) option is given. If the exit option 
	is supplied, the local host is assumed to be the target unless this option is 
	provided.
&space
&term	-t secs : Passes &ital(secs) to the UDP/TCP interface as the timeout value causing 
	a maximum delay of &ital(secs) seconds before assuming that no response from 
	&ital(nawab) will be received. 
&space
&term	-v : Verbose mode.
&endterms


&space
&parms	&This recognises two sets of positional parameters depending on the command line 
	options that were set. If the user included the load option (-l) on the command
	line, then &this expects that the first parameter is the name of the file to 
	send to &ital(nawab) as a programme. 
&space
	If the load option is omitted from the command line, then &this assumes that all 
	positional parameters that appear on the command line are &ital(nawab) commands
	and will be sent to &ital(nawab) for processing. &This makes no attempt to 
	validate that the command and arguments are correct. 

&space
&exit	&This will exit with a zero (0) return code when it feels that all processing 
	has completed successfully, or as expected. A non-zero return code indicates 
	that there was an issue with either the parameters or data supplied by the user, 
	or &ital(nawab) could not be communicated with.

&space
&see	ng_nawab, ng_seneschal, ng_sreq, ng_sendudp, ng_sendtcp

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Jan 2002 (sd) - The beginning.
&term	03 Jul 2002 (sd) - To adjust for <$1 KSH difference.
&term	25 Aug 2003 (sd) - To force the entry of -s if -e given.
&term	05 Jan 2003 (sd) - Added -c option
&term	02 Apr 2004 (sd) - added trap to expand ex to explain; works round nawab bug that 
	assumed ex was exit.
&term	17 May 2004 (sd) - Avoids use of ppget
&term	16 Aug 1005 (sd) - Converted jobber lookup to use ng_ppget -C.
&term	24 Mar 2006 (sd) - Added -A (attempt) option.
&term	27 Mar 2006 (sd) - Fixed problem in printf that was causing it not to print %stuff strings right.
&term	08 Dec 2006 (sd) - Fixed problem with error message if nawab crashes duing a load. It now should
		give an error rather than garbage. 
&term	18 Jan 2006 (sd) - Added user name and node name to the programme data sent to nawab (HBDGMM)
&endterms

&scd_end
------------------------------------------------------------------ */
