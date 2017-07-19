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
# run to move filenames to their final resting place (removing attempt number)
# expects a list of filenames (with attempt number) on the command line.
#
#
# Historical note:
#	The original script actually did the renames and registered the newly 
#	named file with repmgr while purging the attempt-numbered file from 
#	repmgr's view of things.  This aparently (sometime while I was 
#	off on a sabbatical of sorts) became a problem from a timing 
#	point of view.  The internal repmgr mv command was added, and this 
#	script was changed to simply generate the mv commands and pass them 
#	along to repmgr.  With this shift two functions were dropped:
#		- the ability to register all files in a directory
#		- the support for setting the replication count
#
#	It is now up to the application that creates the files as programme output
# 	to register the files with the desired replication count and to provide
#	all output file names, via nawab file references, on the command line. 
# -----------------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# echos the number of seconds we should pause based  on whether we could actually 
# extend our lease. We need to pause for the moment to ensure that the files are 
# actually registered before we move them.  If we get the lease, then we will nap 
# for a minute.
function extend_lease
{
	if [[ -n $jobname ]]				# if we were given a job name by seneschal
	then
		if (( $hidden < 1 ))
		then
			verbose "extending lease until 10 minutes from now; sending a bid to seneschal"
			ng_sreq -s $jobber extend $jobname 600		# ensure our lease good for another 10 minutes
			ng_sreq -s $jobber bid $mynode 1		# we really dont count as a job; up the bids
		else
			verbose "we detached, lease not extended"	# we already returned positive so as not to block things up
		fi
		echo 60						# 
	else
		echo 10						# sleep a minimum amount of time - could not extend
	fi
}

# build commands to send to repmgr.  build them here so we can rebuild 
# (dont destroy the original parms to the script)
function build_cmds
{
	while [[ -n $1 ]]
	do
		if [[ $1 == =* ]]
		then				# ignore rep factors; sorry
			shift
			continue
		fi
		
				oldname=$1
		ext=			
		if [[ $oldname == *.* ]]			# preserve the extention if there
		then
       			ext=.${oldname##*.}		# snag from last dot out
		fi
       		newname=${oldname%+$attempt*}$ext
		echo "mv $oldname $newname" 
	
		shift
	done 
}

# --------------------------------------------------------------------------------------------
debugf=$NG_ROOT/site/seneschal/debug 
nodetachf=$NG_ROOT/site/seneschal/s_mvfile.nodetach		# on by default
hidden=0
mynode=`ng_sysname`
verbose=1		# for now we default to chatty 

if [[ $@ != *--man* && $@ != *-\?* ]]		# if not a help request, output to log and possibly detach from parent process
then
	exec 2>>$NG_ROOT/site/log/s_mvfiles

	if [[ -f $nodetachf ]]
	then
		hidden=0				# forces us to extend our lease
		log_msg "started: (NOT detached) $@"
	else
		$detach					# seneschal sees us complete immediately -- we hang out a bit in reality
		log_msg "started: (detached) $@"
		hidden=1
	fi
	
	if [[ -f $debugf ]]
	then
		if [[ -s $debugf ]]
		then
			set -x
		fi
	
		verbose=1
	fi
fi


ustring="attempt-number file [=rep] ..."
now=$(date)
status=0		# return code
delete=0;		# delete the attempt file -- dont move it
jobber=${srv_Jobber}
if [[ -z $jobber ]] 
then
	msg="cannot determine jobber from srv_Jobber ($srv_Jobber)"
	log_msg "ERROR: $msg"
	ng_log $PRI_ERROR $argv0 "$msg"
	cleanup 1
fi
filer=${srv_Filer}
if [[ -z $filer ]] 
then
	msg="could not find filer"
	log_msg "ERROR: $msg"
	ng_log $PRI_ERROR $argv0 "$msg"
	cleanup 1
fi

while [[ $1 == -* ]]
do
	case $1 in 
		-d)	delete=1;;			# just delete the attempt file
		-j)	jobname=$2; shift;;		# we will extend our lease if we know the name

		--man) show_man; cleanup 100;;
		*)	usage; cleanup 100;;
	esac

	shift
done

attempt=$1			# attempt number to strip off
shift

# extend our lease, and see how long we should pause waiting for the file(s) to 
# settle in replication manager before we ask repmgr to do something with them.
# if we detached, then we dont extend the lease as we've already returned good.
extend_lease | read nap

sleep ${nap:-60}			# give repmgr some time to deal with files registered immediately before our call

build_cmds "$@"	>$TMP/s_mvfile.$$ 

count=1
while ! ng_rmreq -s $filer <$TMP/s_mvfile.$$
do
	count=$(( $count + 1 ))

	if (( $hidden < 1  && $count > 10 ))		# if not hidden; we cannot try forever 
	then
		msg="unable to communicate with repmgr on $filer"
		log_msg "ERROR: abort: $msg"
		ng_log $PRI_ERROR $argv0 "$msg"
		cleanup 1
	fi
	log_msg "WARNING: rmreq command failed to $filer; pausing before retry"
	sleep 10

	filer=$(ng_ppget srv_Filer)		# it could have moved
	build_cmds "$@"	>$TMP/s_mvfile.$$ 	# rebuild list in case there were disk problems on TMP
done

if (( $count > 1 ))
then
	verbose "needed $count tries to send data to repmgr"
fi

cleanup 0
exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_s_mvfiles:Move files to names without attempt reference)

&space
&synop	ng_s_mvfiles [-v] [-j jobname] attempt-number filename [filename...]

&space
&desc	&This is invoked by &ital(seneschal) after a job, that listed one or more output 
	files, has completed successfully.  It is the responsiblity of &this to 
	generate the necessary replication manager commands that cause the output files
	(which have embedded attempt numbers) to be renamed to the desired final names. 

&space
	The file(s) supplied on the command line are assumed to be basename(s) of file(s) 
	that have already been registered with replication manager. Further, each filename is
	expected to have the syntax: 
&space
&litblkb
   name[.name]+n[.suffix]
&litblke
&space

&space
	The attempt number is assumed to be all characters following the last plus sign (+)
	character up to the end of the name or the last dot (.) character. (The extension 
	portion of the name may &stress(not) contain a plus sign character.)

&space
&parms	&This expects one or more filenames on the command line.

&space
&exit	&This will exit with a return code of zero (0) if all mv commands were generated and 
	submitted to replication manager successfully.
	A non-zero return code indicates that at least one error was encountered.
	Seneschal support script generally exit with a return code of 100 or greater to distinguish
	their failure as opposed to the failure of a user programme. 

&space
&files
	These files are meaningful to &this:

&subcat	$NG_ROOT/site/seneschal/s_mvfile.nodetach		
	If this file exists, then the script will not detach.  The advantage to running in non-detached
	state is that if the script fails, the failure can be reported to seneschal.  If missing, 
	the script will detach and always report success to seneschal.  The advantage to this 
	is that seneschal is not blocked by this job which may wait up to 10 minutes before giving 
	up if there are problems.  
&space
&see	&ital(ng_seneschal), &ital(ng_nawab), &ital(ng_rm_where,) &ital(ng_repmgr)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Jul 2001 (sd) : First cut.
&term	31 Oct 2001 (sd) : Added ability to accept a replication parameter after the file name.
&term	26 Feb 2001 (sd) : Corrected a timing issue now that ng_where uses a static dump (added
	-r option to ng_rm_where calls).
&term	30 May 2002 (sd) : support new replication factor style.
&term	20 Jun 2002 (sd) : To ensure all files are there before we move any files.
&term	24 Jun 2002 (sd) : Added support to move all files from a direcitory -- needed to support
	report final where unknown number of files generated with each attempt.
&term	13 Jul 2002 (sd) : Added missing status= if register failed.
&term	14 Nov 2003 (sd) : Added the ability to detach for better job thruput.
&term	13 May 2004 (sd) : Converted leader to jobber.
&term	17 May 2004 (sd) : Fixed ref to jobber for rmreq call.
&term	19 Oct 2004 (sd) : Removed refs to ng_leader.
&term	13 Jan 2005 (sd) : Now properly checks for comm issues with repmgr and will retry.
&term	16 Jan 2005 (sd) :  Corrected a arethmetic error in an if statement (-gt used instead of >)
&term	18 Jan 2005 (sd) : Changed to use ng_ppget to detect filer change rather than ng_env as ng_env 
		presents too much info.
&term	23 Feb 2005 (sd) : Moved list build to a function to allow for rebuild if we have problems. 
		If we do not detach, we send a bid to seneschal so that we do not block things.
&term	18 Dec 2006 (sd) : Fixed --man so it actually wrote to stderr rather than allowing the script
		to detach from the tty.
&term	27 Mar 2007 (sd) : Cleaned up the man page to accurately represent the script.
&endterms

&scd_end
