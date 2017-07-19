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
# Mnemonic:	s_eoj.ksh  - end of job processing
# Abstract:	Executed following a job submitted by seneschal to return status and other
#		data to seneschal
# Parms:	
#		$1 = jobname
#		$2 = attempt number
#		$3 = job status
#		$4 = time (seconds) that this shell has been executing
#		$5 - user system times in the form: user 0m0.00s sys 0m0.00s
# Date: 	17 Jul 2001
# Author: 	E. Scott Daniels
# ------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function sendstats
{
	typeset i=0
	typeset msg="jstatus=$jobname $attempt ${thisnode:-unknown}:$(( $status + $mystatus)):${exec_time}(real) $usr(usr) $sys(sys)$misc"  
	while (( $i < $max_send_tries ))
	do
		#if  echo "$msg" | ng_sendtcp -e "." -t 300 $jobber:${NG_SENESCHAL_PORT:-4444} |read answer
		if  echo "$msg" | ng_sendtcp -e "." -t 300 $target_host:$target_port |read answer
		then
			log_msg "sent status: $msg"	
			log_msg "got answer: $answer"
			return
		fi

		nap=$(( 5 + ($i * 5) ))
		if (( $nap > 30 ))
		then
			nap=30
		fi
		log_msg "send to seneschal failed; retry in $nap seconds ($msg)"
		sleep $nap

		i=$(( $i + 1 ))

		if [[ -z $NG_SENEFUNNEL_PORT ]]		# if no funnel, then we need to check for service loc change
		then
			ng_ppget srv_Jobber | read newjobber			# we could be having issues because service is moving/moved
			if [[ -n $newjobber && $newjobber != $jobber ]]		# get jobber var again from pinkpages and try with it
			then
				target_host=$newjobber		
				log_msg "jobber seems to have shifted nodes, now sending to: $target_host"
			fi
		fi
	done

	ng_log $PRI_WARN $argv0 "could not send status to seneschal: $msg"
}
# ---------------------------------------------------------------------------------------
ustring="jobname attempt status [exec-time from \$SECONDS]"


max_send_tries="${NG_SEOJ_TRIES:-400}"		# 100 is approximately an hour
while [[ $1 = -* ]]
do
	case $1 in
		--man) 	show_man; cleanup 1;;
		*)	usage; cleanup 1;;
	esac

	shift
done


exec 2>>$NG_ROOT/site/log/s_eoj.log

log_msg "starting: ($@)"
ng_dt -i |read now

thisnode=`ng_sysname`
jobber=${srv_Jobber}
if [[ -z $jobber ]] 
then
	log_msg "srv_Jobber was null; getting directly from narbalek with ng_ppget [$(grep -c srv_Jobber $NG_ROOT/cartulary)]"
	ng_ppget srv_Jobber | read jobber
	if [[ -z $jobber ]]
	then 
		log_msg "attempt to get srv_Jobber from pinkpages failed; pausing and then one retry"
		sleep 7
		ng_ppget srv_Jobber | read jobber

		if [[ -z $jobber ]]
		then 
			msg="cannot determine jobber from cartulary srv_Jobber; direct ppget returned null too"
			log_msg "$msg"
			ng_log $PRI_ERROR $argv0 "$msg"
			cleanup 1
		fi
	fi
fi

if [[ -n $NG_SENEFUNNEL_PORT ]]
then
	target_host=localhost
	target_port=$NG_SENEFUNNEL_PORT
	log_msg "sending to local seneschal funnel @$target_port"
else
	target_host=$jobber
	target_port=$NG_SENESCHAL_PORT
fi
mystatus=0			# processing status here

# ng_log $PRI_INFO $argv0 "started: $@"

jobname=$1
attempt=$2
status=$3
exec_time=${4:-0}		# from $SECONDS on command line

echo "$5" | awk '{ x = x $0 " "; } END { printf( "%s", x) } ' | read stuff 	# bsd awk cannot handle newline in times output

awk -v stuff="$stuff" '
	function cvt( x, 	i, m, s )
	{
		m = x+0;
		i = index( x, "m" );
		s = substr( x, i+1 ) * 10;
		return (m*600) + s;		# tenths of seconds
	}

	BEGIN {
		split( 	stuff, a, " " );	# a[2] == usr; a[4] == sys

		u = cvt( a[2] );
		s = cvt( a[4] );

		printf( "%.0f %.0f", u, s )
	}
'| read usr sys		

awk -v t="$exec_time" ' BEGIN { printf( "%d\n", t * 10 ); }' | read exec_time	# tenths of seconds 

if [[ -s ${WOOMERA_JOB_TMP:-nosuchfile} ]]
then
	misc=":"` awk '{ print substr( $0, 1, 1024 ); exit }' <$WOOMERA_JOB_TMP`
	rm -f $WOOMERA_JOB_TMP
else
	misc=""
fi

sendstats
exit $(( $status + $mystatus ))


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_s_eoj:End of job processing for seneschal)

&space
&synop

&space
&desc	&This is invoked after a job has executed in order to 
	return status and potentially other data to &ital(seneschal) about 
	the preceding job. &This will format a UDP message containing 
	the job name, exit status, node name, and up to 1024 bytes 
	of job specific information that must be communicated back 
	to &ital(nawab) (or job manager) via &lit(seneschal).
	The environment variable &lit(WOOMERA_JOB_TMP) is expected to 	
	contain the name of a file that the job has placed any data 
	that &this is to return to &ital(seneschal). If the file 
	referenced by this environment variable is not empty the 
	first 1024 bytes will be read and returned in the status 
	message. 
&space
	&This attempts to send the status message once every few seconds 
	for, but default, as many as 400 tries.  The interval between tries
	extends a bit with each attempt to a maximum of 30 seconds, 
	such that if 400 attempts are 
	made the trial period is between three and four hours.  The number of 
	attempts to send the status back to seneschal can be changed by
	setting the &lit(NG_SEOJ_TRIES) variable in pinkpages. 
&space	
	The format of the status message is dictated by &ital(seneschal)
	and is:
&space
&litblkb
	JOBSTATUS:<jobname> <attempt> <nodename>:<status>:<misc-data>
	jstatus=<jobname> <attempt> <nodename>:<status>:<tseconds>(real) <tseconds>(usr) <tseconds>(sys) <miscdata>"  
&litblke
&space
	&ital(Tseconds) are tenths of seconds, and 
	the &ital(misc-data) is the first bit of data from the file name 
	with the &lit(WOOMERA_JOB_TMP) environment variable.  
&space
&parms	The following positional parameters are required by &this
	and are defined in the order that they are expected to appear
	on the command line.
&begterms
&term	jobname : This is the name that &ital(seneschal) is using to 
	track the job.
&space
&term	attempt : The attempt number assigned when &ital(seneschal) scheduled the 
	job.
&space
&term	status : The exit code of the job.
&space
&term	time : This is time information (real and system) as output from the $SECONDS
	variable. If not supplied, then zero is returned to seneschal. This allows 
	seneschal to compute the amount of time that the job spent in a woomera 
	queue after it was sent to the node.
&endterms

&space
&exit	&This will exit with the status code that is passed in on the 
	command line so that when it is run as a command "stream" via 
	&lit(woomera) the state of the job will be that of the real 
	job, not this script. 

&space
&see	&ital(seneschal), &ital(nawab), &lit(woomera)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 17 Jul 2001 (sd) : Thin end of the wedge.
&term	23 Feb 2003 (sd) : Now expects $4 to be the number of seconds things
	were executing.
&term	04 Mar 2003 (sd) : Added stuff to capture usr/sys time stats in tenths of seconds.
&term	09 Sep 2003 (sd) : Corrected log message bug -- not passing script name 
&term	10 Sep 2003 (sd) : Added ability to show man page.
&term	13 Sep 2003 (sd) : Changed leader references to Jobber references.
&term	10 Sep 2004 (sd) : Extended number of tries from 5 to 400 (about 3.5 hours), and 
	allow it to be changed with NG_SEOJ_TRIES if defined.
&term	19 Oct 2004 (sd) : removed refs to ng_leader.
&term	06 Jun 2006 (sd) : Added a direct ppget attempt if jobber seems null in cartulary.
&term	13 Sep 2007 (sd) : Added code so that if the jobber shifts nodes we start attempting to 
	send to the new node rather than the node that previously had the jobber.
&term	05 Oct 2007 (sd) : Added support to send status to a local funnel rather than directly to seneschal.
&endterms

&scd_end
------------------------------------------------------------------ */
