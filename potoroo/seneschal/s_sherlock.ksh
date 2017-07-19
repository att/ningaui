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
# Mnemonic:	s_sherlock
# Abstract:	invoked by seneschal just before a job's lease is set to expire, this
#		script investigates the job to determine if it is still running, or 
#		has completed.  If running, a lease extension command is sent back to 
#		seneschal.  If completed, or unknown to woomera, a job completion 
#		message with an appropriate exit code for the job is sent to seneschal.
#		
#		This script can be (in a limited fashion) configured with sherlock.conf
#		in site or lib.  One of the config options is to invoke a user written 
#		programme that determines the action to be taken for the job.  This is
#		handy when it may not be easy to determine whether or not a user programme
#		is still running.   See the man page for more detail.
#		
# Date:		4 January 2006	
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# parse our config file (either $NG_ROOT/site/sherlock.conf  or $NG_ROOT/lib/sherlock.conf
# for options that are specific to the job that is being investigated.  The config file 
# is assumed to be bar (|) separated fields with the first field being a pattern to use
# against the job name.  If the jobname matches the pattern, the associated options are matched
# and the search stops. 
# example:
#     ppcmatch_|invoke=$NG_ROOT/apps/foo/bin/programme |validate_files=0
#     hellocat.*test1|validate_files=0
# See the man page for what things are expected/important in the config
function parse_config
{
	typeset cf=""

	if [[ -f $NG_ROOT/site/sherlock.conf ]]
	then
		cf=$NG_ROOT/site/sherlock.conf
	else
		if [[ -f $NG_ROOT/lib/sherlock.conf ]]
		then
			cf = $NG_ROOT/lib/sherlock.conf
		else
			return
		fi
	fi

	awk -F '|' -v target="$sj_name" '
		{
			if( match( target, $1 ) )
			{
				for( i = 2; i <= NF; i++ )		# print x=y one per line
					printf( "%s\n", $(i) )

				exit( 0 );
			}
		}
	' <$cf >$TMP/sherlock.cmd.$$

	if (( $verbose > 0 ))
	then
		set -x 
	fi
	. $TMP/sherlock.cmd.$$
	set +x
}


# dig info from each running job listed, and send off commands to various nodes
function suss_n_run
{
	#explain: job step_0.hellocat_64d2690d_0 is running; ningd1(node) sje5908ae2(wname) 0(att) 575(lease) 1225(elapsed)
	ng_sreq explain running | awk -v lthreshold=$lthreshold -v live=$live_opt '
		/explain.*job/ {
			jname = $3;

			split( $6, a, "(" );
			node = a[1];

			split( $7, a, "(" );
			wname = a[1];

			split( $8, a, "(" );
			attempt = a[1];

			split( $9, a, "(" );
			lease = a[1];
		
			if( lease < lthreshold )		# only do for short-timers
			{
				printf( "ng_rcmd %s ng_s_sherlock %s -R -a %d -j %s -w %s -v\n", node, live_opt, attempt, jname, wname );
			}
		}
	'|while read x
	do
		if (( ${recursion} > 0 ))
		then
			log_msg "running remote sherlock command: $x"
			eval $x
			echo  "================================================================================="
		else
			log_msg "in too deep!"
		fi
	done
}

# see if we can find an exit cood in the woomera log. if we report end of job, itd be nice to do it with 
# the real exit code than a made up one.
function find_wexit_code
{
	gre $wj_name $wlog |awk  -v sj_name=${sj_name} '
		BEGIN { ec = -1 }
		/exited.*normally.*status=/ {
			split( $NF, a, "=" );
			ec = a[2];
		}
		END { printf( "%d\n", ec ); }
	'
}

# echo only one of these three strings:  extend, fail, ok
# expect pid of the job as $1; 0 if its not known. New woomera puts pid in explain message
function validate_running
{
	if (( ${1:-0} <= 0 ))
	then
		verbose "validate_running: $wj_name: pid not found on explain output; sussing through woomera log"
		gre "job.*$wj_name.*started:.*pid=" $wlog | read x
		pid=${x##*=}
	else
		pid=$1
	fi

	verbose "validate running: woomera reports pid to be: $pid"
	if (( ${pid:-0} <= 0 ))
	then
		log_msg "no pid found for $wj_name; assuming it is running; extending lease"
		echo "extend"
		return
	fi

	ng_ps|awk -v need="$pid"  '
		BEGIN { state = 1; }			# default to error

		{
			if( $2 == need )		# bingo -- its running still
			{
				state = 0;		# good return -- we found it 
				exit( 0 );
			}
		}

		END { exit( state ); }			# 
	'
	if (( $? == 0 ))
	then						# it is running; see if we can determine what process it makes 
		markerf=$TMP/sherlock.control.$$		# control file to see if anything is newer
		touch $markerf

		if [[ -n $output_files ]]			# look for output files and see if they are growing
		then
			ok_msg="validate running: job with pid ($pid) found in ps listing and file(s) being updated" 

			for f in $output_files			# for each file that the job is updating
			do
				find `ls -d $NG_ROOT/fs[0-9][0-9]` -name "$f*"		# assume files will likely have ~ added at end
			done >$TMP/sherlock.flist.$$

			state=0
			verbose "validate running: pausing $delay seconds to wait for file update"
			sleep $delay
		
			while read p
			do
				verbose "checking output file: `ls -al $p`"
				if [[ $p -nt $markerf ]]
				then
					state=1
				fi
			done <$TMP/sherlock.flist.$$
		else
			verbose "validate running: skipped output file validation check"
			ok_msg="validate running: job with pid ($pid) found in ps listing" 
			state=1
		fi

		if (( $state > 0 ))
		then
			verbose "$ok_msg; extending lease"
			echo "extend"
		else
			verbose "validate running: output file(s) are NOT being updated; assuming hung and purging job"
			echo "fail"
		fi
	else
		verbose "validate running: no job with pid ($pid) found in ps listing; purging job in seneschal"
		echo "fail"
	fi
}

# -------------------------------------------------------------------
version="1.2/08126"

wlog=$NG_ROOT/site/log/woomera
sj_name=junk
lease_seconds=1800			# if we decide the job should be extended in seneschal; this is the default amount
delay=30				# how long to wait for a file update
attempt=0;
interactive=1;				# seneschal invokes with -b to cause output to go to log file
recursion=1				# recursion allowed unless -R given
lthreshold=100				# when called with no -j/-w info; check running jobs with leases less than this
validate_files=0			# in some cases it may not make sense to validate files
forreal="log_msg [NOITICE]  not live, would execute: "	# command we run if -L (live) is NOT given 
live_opt=""				# set to -L and passed on rcmd if user runs from command line with -L set
ustring="[-a attempt] [-b] [-e seconds] [-o output-filelist] [-j sene-job] [-w woo-job] [-l seconds] [-L] [-R]"

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	attempt=$2; shift;;
		-b)	interactive=0;;					# invoked by seneschal -- log msg to ROOT/site/log/sherlock
		-e)	lthreshold=$2; shift;;				# check running jobs with leases that expire in less than $2 seconds
		-j)	sj_name=$2; shift;;				# seneschal job name -- needed to report back/take action
		-l)	lease_seconds=$2; shift;;			# user overrides seconds to extend a lease should we decied to
		-L)	live_opt="-L"; forreal="";;			# we are live -- KILL KILL KILL!
		-n)	;;						# we default to this mode, but allow user a comfort -n
		-o)	output_files="$output_files$2 "; shift;;
		-R)	recursion=0;;					# added when we are invoked via rcmd; we dont want loops, so dont allow recursion 
		-w)	wj_name=$2; shift;;				# woomera job name


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

export verbose
if [[ -z $wj_name || -z $sj_name ]]		# if something missing, dig info and report on all jobs
then
	suss_n_run
	cleanup $?
fi


if (( $interactive < 1 ))
then
	exec >>$NG_ROOT/site/log/sherlock 2>&1
	verbose=1
fi

parse_config					# parse our config file for options based on the job name

export validate_files

if [[ -n $invoke ]]			# user has supplied a function to run; expect action and completion code on stdout
then
	verbose "user invoked funcion/programme ($invoke) starts ------------------------"
	eval $invoke -j $sj_name -w $wj_name -f \"$output_files\" | read action ivalue junk
	verbose "user invoked funcion/programme ($invoke) ends --------------------------"
else
	if (( $validate_files <= 0 ))		
	then
		output_files=""				# just scratch the list 
	fi

	log_msg "sussing round for sj=$sj_name wj=$wj_name att=$attempt ($version)"

	ng_wreq explain $wj_name | awk '
 		/currently unknown to/ { print "validate_unknown"; next; }
		/job.*status.*running/ { printf( "validate_running %d", $NF+0); next; }		# NF is the pid in new woomeras
		/job.*status.*finished.*success/ { print "successful"; next; }
		/job.*status.*finished.*fail/ { print "failure"; next; }
		/job.*status not ready to run/ { print "not_ready"; next; }
	
	'|read action parm
fi

# NOTE:  When sending a completion message, and the exit code is not found in woomera, nor supplied by the 
# 	uip, set it to 120-124.  Do NOT use 125 or greater as these are used by seneschal and kshell. 
case ${action:-missing_action} in 
	extend)				# extend, complete are from user invoked programme
		if (( $ivalue < 600 ))
		then
			verbose "user programme extension value too short ($ivalue), using 600 seconds"
			ivalue=600;			# don't allow short extensions as that cycles sherlock over and over
		fi
		verbose "per user programme, extending lease on $sj_name ${ivalue:-$lease_seconds} seconds"
		$forreal ng_sreq extend $sj_name ${ivalue:-lease_seconds}

		;;

	complete)
		verbose "per user programme, completing job $sj_name with a return code of: ${ivalue:-122}"
		$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} ${ivalue:-122} $SECONDS "`times`"
		;;


	validate_unknown*)		# unknown, probably never scheduled or expired; search the woomera log for job name
					# if we find a status, run eoj with that, else with a large exit code to indicate its made up
		find_wexit_code | read ec
		if (( $ec >= 0 ))			# code found in woomera log, send it back
		then
			log_msg "found job woomera in woomera log $sj_name/$wj_name; ending job with ec=$ec"
		else
			log_msg "job unknown to woomera $sj_name/$wj_name; failing job ec=120"
			ec=120
		fi

		$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} 120 $SECONDS "`times`"
		;;

	validate_running)
						# validate running writes stuff to log and passes us a token to take action on
		log_msg "woomera reports job is running; validating"
		case `validate_running ${parm-:0}` in 
			fail)
				$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} 121 $SECONDS "`times`"
				;;

			extend)
				$forreal ng_sreq extend $sj_name $lease_seconds
				;;

			ok)
				$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} 0 $SECONDS "`times`"
				;;

			*)
				;;
		esac
		;;

	successful)					# this should be rare
		log_msg "woomera reports job success $sj_name/$wj_name; sending success"
		$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} 0 $SECONDS "`times`"
		;;

	failure) 
		log_msg "woomera reports job failure $sj_name/$wj_name; sending failure"
		$forreal ng_s_eoj ${sj_name:-foo} ${attempt:-0} 120 $SECONDS "`times`"
		;;

	not_ready)
		log_msg "woomera reports not ready $sj_name/$wj_name; extended lease'"
		$forreal ng_sreq extend $sj_name $lease_seconds
		;;

	*)	;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_s_sherlock:Investigate and report on running seneschal jobs)

&space
&synop	ng_s_sherlock [-e threshold] [-L] [-v] 
&break
	ng_s_sherlock -b -a attempt -j seneschal-job-name -w woomera-job-name [-o output-file] [-L]

&space
&desc	&This is a seneschal helper script that will investigate a long running job 
	and either report the job's status back to seneschal (success or failure) or 
	extend the lease if &this feels the job is still running.  Seneschal will 
	invoke this script for a job that has had a lease expire; it is the script's
	responsibility to determine what senechal should do with the job. 

&space 
&subcat Manual Execution
	While designed to be invoked by &ital(seneschal) with an exact job and woomera job name, &this 
	can be invoked form the command line without the -j and -w options. In this 
	'user mode,' &this will look for jobs on the seneschal running queue that have 
	leases of less than 100 seconds, and will report on their status.  If the user 
	supplies the -L (live) option, &this will take action as though the command
	was invoked by seneschal.
&space
&subcat Actions Taken
	If executed in live mode (-L), &this will take action based on what it finds. 
	The following states are recognised:
&begterms
&term	Active : The job is running and appears to be making progress. The lease will be 
	extended in seneschal.
&space
&term	Hung : The job is in the process table, but appears hung (output files not being 
	updated.  A failure will be reported to seneschal. (At the moment the actual
	job is left running).
&space
&term	Failed : The job is reported by woomera as having executed and exited with a 
	bad (not zero) return code.  A failure status is sent to seneschal.
&space
&term 	Success : The job is reported by woomera as having executed and exited with a 
	good (zero) return code.  A success status is sent to seneschal.
&space
&term	Unknown : Woomera does not know about the job. Likely the schedule command never 
	reached woomera, woomera crashed, or the job ended more than woomera's expiry 
	theshold for keeping job status. When this status is recognised, the a failure
	message is sent back to seneschal. 
	
&endterms

&subcat Config File
	The file &lit(sherlock.conf) in either &lit($NG_ROOT/site) or &lit($NG_ROOT/lib) 
	can be used to set/alter the function of &this;  
	if a file exists in both directories, the one is site is used.
	The file is expected to contain
	newline separated records with each record being vertical bar (|) separated fields.
	The first field is assumed to be a regular expression that if it matches the 
	job name string, will cause the remainder of the record to be used.  Once a pattern
	is matched, configuration file processing stops. 
&space
	The remainder of the fields 
	are &ital(var=value) pairs that are treated as variables inside of &this after the 
	configuration file is parsed. The variables that can be set are:
&space
&begterms
&term	validate_files : If set to 0, output files that are supposed to be created by the job
	are NOT validated.  Thus, the job will be considered to be running based solely on 
	whether or not it was found in the process table. The default for this variable is 1.
&space
&term	invoke : Supplies the name (with path if necessary) of a programme/script that should 
	be invoked.  &This will invoke the programme rather than trying to determine the state
	of the job. See &ital(User Supplied Investigative Programme) below for details. 
&endterms 
&space
	Other variables may be supplied, and prefaced with the keyword export if necessary.
	The purpose of these variables would be to communicate information to a user supplied
	investigative programme, and should be named with a leading &lit(uip_) to avoid 
	conflict with any variables that are defined/used by &this.  &This automatically
	exports the &lit(verbose) and &lit(validate_files) variables for the use of the 
	user supplied programme.

&space
&subcat User Supplied Investigative Programme
	If the configuration file has an entry for the job family of the job that is being
	investigated, and that entry supplies a programme to do the investigation, &this
	will invoke the programme. The programme should expect the following information
	on the command line:
&space
&begterms
&term	-j name : The seneschal job name of the job.
&space
&term	-w name : The woomera job name.
&space
&term	-o list : The list of files that are thought to be created by the programme. 
	These names will be space separated, and will contain attempt number information.
	The job is responsible for determining which replication manager filesystem
	to create each file in, so the programme must search for the files if it will 
	be using them to determine the state of the job.
&endterms
&space
	These parameters will be pased to the programme  after any parameters that were
	listed in the &lit(invoke) field of the configuration file. As an example, 
	consider the configuration file lines:
&space
&litblkb
   ebub_cycle|invoke="$NG_ROOT/apps/ebub/bin/investigate_job -t cycle"
   ebub_report|invoke="$NG_ROOT/apps/ebub/bin/investigate_job -t report"
&litblke
&space

	The user investigation programme invoked for both job families is the same, 
	and the user supplied family type is supplied.  The actual command line as 
	invoked by &this will be:
&space
&litblkb
   $NG_ROOT/apps/ebub/bin/investigate_job -t report -j sene-name -w woo-name -f "$flist"
&litblke

&space
&subcat Output From User Programme
	The output from the user investigative programme is expected to be read from 
	standard output and to contain two tokens: action and value.
	The value token is used differently depending on the action. Recgonised actions
	are:
&space
&begterms
&term	extend : The job's lease should be extended by &ital(value) seconds.  If value is 
	missing, 1800 is used (approximately 30 minutes). If a value less than 600 seconds
	is supplied (10 minutes), it will be ignored and 600 will be used.  This keeps
	&this from being invoked repeatedly. 
&space
&term	complete : A completion message for the job will be sent to seneschal.  The value 
	token is expected to be the return code (completion code) that will be assigned
	to the job.  A code of 0 indicates success, and any other positive integer returned
	as value indicates a failure.  If value is not supplied, &this will complete 
	the job with the exit code of 122. 
&endterms
&space
&subcat User Programme Execution Time
	Seneschal invokes &this when a job's lease is aproximately 5 minutes from expiration.
	If a user defined investigative programme is invoked, it should complete in less
	than a few minutes or the job will be terminated by seneschal with a lease exception.


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-e seconds : The threshold for taking action on leases.  When run without a specific 
	seneschal job name, &this will consider all jobs that have a lease which will expire 
	in &ital(seconds) or less.  If this option is not supplied, the threshold is 100 seconds. 
&space
&term	-L : Live mode.  By default, &this only reports what it thinks should be done and 
	does not actually do anything.  If the live option is supplied, &this will report 
	and send the commands to seneschal. 
&space
&term	-b : Background mode; messages are written to &lit(NG_ROOT/site/log/sherlock). Note: when 
	invoked with this option, &this does NOT make an attempt to detach from the controlling 
	TTY device.  This option simply causes messages to be written to a known log file. 
&space
&term	-a attempt : The attempt number for the job.  This is necessary in order to report 
	a job status to seneschal. Defaults to 0 if not supplied. 
&space
&term	-j name : The seneschal job name of the job to suss out.
&space
&term	-w name : The job name given when the job was scheduled with woomera.
&space
&term	-o file : Output file(s) that are created by the job.  If supplied, &this will attempt 
	to determine if they are being updated.  If files are being updated then the job 
	is considred active.  If no files are supplied, the job is considered active if 
	the process can be found in the process table. 
&space
&term	-v : Verbose mode, though it is pretty chatty without this option. 
&space
&term	-R : An internal flag that is used when &this invokes &this on another node. This 
	prevents recursive death should a command line option be null. 
&endterms


&space
&exit	Always exits good at the moment.
	When a completion message is sent to seneschal, the job's exit code is set with a 
	valid value as best as &this can determine.  If missing data prevents &this from 
	determining the job's actual completion code, a value in the range of 120-124 is
	returned as an indication that the code was set by &this and is not the actual
	exit code of the job. 

&space
&see	ng_seneschal, ng_sreq, ng_woomera, ng_wreq

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Mar 2006 (sd) : Sunrise.
&term	12 Aug 2006 (sd) : Added check for missing pid from woomera log. We will assume it is running
		and extend the lease if we cannot find a pid to check. (woomera bug was writing 
		garbage to its log and occasionally was missing started messages for a job).
&term	05 Oct 2006 (sd) : Validate files now defaults to off.  This was causing grief with 
		rollup jobs that do not generate anything to the output file until the very 
		end of their (long) run. 
&endterms

&scd_end
------------------------------------------------------------------ */
