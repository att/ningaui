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
# Mnemonic:	ng_endless
# Abstract:	start something, and keep it alive
#
#	WARNING: This script must be able to funciton properaly on a minimal 
#			cartulary as it is invoked from ng_init possibly before 
#			the cartulary is created.
#		
# Date:		31 Jul 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
if [[ -f $CARTULARY ]]
then
	. $CARTULARY
else
	if [[ -f $NG_ROOT/lib/cartulary.min ]]
	then
		. $NG_ROOT/lib/cartulary.min
	else
		echo "WARNING: could not find $NG_ROOT/cartulary nor $NG_ROOT/lib/cartulary.min"
	fi
fi

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
. $NG_ROOT/lib/procfun

PATH=".:$PATH"	# needed for testing


# which seems to lock up on g5 macs for unknown reasons; so we roll our own...
function inpath
{
        for x in ${PATH//:/ }
        do
                if [[ -x $x/$1 ]]
                then
                        return 0                # good
                fi
        done

        return 1                # fail $1 is not in the path (and executible)
}

#  remove everything but a select few from the environment
# deprecated -- we use the one in stdfun now
function local_wash_env
{
	# we keep only these from being washed away;  
	DRY_CLEAN_ONLY="^TZ=|^CVS_RSH=|^DISPLAY=|^HOME=|^LANG=|^DYLD_LIBRARY_PATH|^LD_LIBRARY_PATH=|^LOGNAME=|^MAIL=|^NG_ROOT=|^TMP=|^PATH=|^PS1=|^PWD=|^SHELL=|^TERM=|^USER="
	env | gre -v $DRY_CLEAN_ONLY | while read x
	do
		unset ${x%%=*}
	done
}

# get a new load of cartulary stuff and wash the environment if called for.
function load_cartulary
{
	if [[ -f $CARTULARY ]]
	then
		. $CARTULARY
	else
		if [[ -f $NG_ROOT/lib/cartulary.min ]]
		then
			. $NG_ROOT/lib/cartulary.min
		fi
		if [[ -f $NG_ROOT/site/cartulary.min ]]			# site overrides anything that is defined in lib
		then
			. $NG_ROOT/site/cartulary.min ]]
		fi
	fi

	wash_env

#	if (( ${wash:-0} > 0 ))			# purge things from the environment before starting command
#	then
#		LAUNDRY_LIST="${LAUNDRY_LIST:-srv_ NG_ FLOCK_}"
#		verbose "purging environment of: ${LAUNDRY_LIST}"
#		oroot=$NG_ROOT
#		env | gre "^${LAUNDRY_LIST// /|^}"  | while read x
#		do
#               		unset ${x%%=*}
#	
#		done
#		export NG_ROOT=$oroot		# we never delete this
#	fi
}


# -------------------------------------------------------------------

ok2detach=1
for o in "$@"
do
	if [[ $o != -* ]]
	then
		break;
	fi

	case $o in 
		-e|--man|-f|-\?)	ok2detach=0;;
	esac
		
done

if [[ $ok2detach -gt 0 ]]
then
	log_msg "detaching; log messages written to $NG_ROOT/site/log directory"
	detach_log=$TMP/endless.$$
	$detach
fi

ustring="[-f] [-d work-dir] [-e] [-l] [-w] [-v]   [/path/]command [parms]"
stop=0
vflag=""
interactive=0
factor=1.5		# allows us to increase the delay time between restarts if its unstable
min_delay=45		# minimum delay of 45 seconds
log_prefix=""
wash=0
wdir="$NG_ROOT/site"	# default to running from site; -w can override the work directory

while [[ $1 = -* ]]
do
	case $1 in 
		-d)	wdir="$2"; shift;;
		-f)	interactive=1;;		# foreground -- ignore
		-e)	stop=1;;

		-l)	log_prefix=".";;		# command we invoke has log file management code, use a dummy name to prevent collisoins

		-w)	wash=1;;			# wash all NG_ and FLOCK_ things from environment

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done



id=${1##*/}				# we use the basename of the command as our final log id, and as their log file
id=${id#ng_}
goaway_id=${argv0##*/}.$id		# my id for goaway (same as using -u on goaway command)
mylog=$NG_ROOT/site/log/endless.$id
itslog=$NG_ROOT/site/log/$log_prefix$id

if [[ $stop -gt 0 ]]
then
	ng_goaway  $goaway_id
	log_msg "the ${argv0##*/} wrapping $id has been signaled to stop after $id terminates"
	cleanup 0
fi

if [[ $interactive -lt 1 ]]
then
	exec  >$mylog 2>&1
fi

if ng_goaway -X -c $goaway_id		# we must test for nox because of malformed goaway name -- stdfun cannot do
then
	log_msg "found nox goaway file; not executing"
	cleanup 2
fi

if [[ -n $wdir ]]		# user supplied working directory
then
	if ! cd $wdir
	then
		log_msg "cannot switch to working directory: $wdir"
		cleanup 1
	fi
fi

log_msg "managing: $@"

if [[ $1 = /* ]]
then
	if [[ ! -x $1 ]]
	then
		log_msg "cannot start $1; no executable bit: `ls -al $1 2>&1`"
		cleanup 1
	fi
else
	if ! inpath $1
	then
		log_msg "cannot start $1; not in path: $PATH"
		cleanup 1
	fi
fi

cmd="$@"

last_start=0
ng_goaway  -r $goaway_id		# clear in case left after system crash or something
loop_count=0				# use to expand the delay before start if it keeps crashing 
while true
do

	load_cartulary			# get a fresh set of values before each run

	now=`ng_dt -i`
	if [[ $(( $now - $last_start )) -gt 36000 ]]		# if last start was more than one hour ago
	then
		verbose "rolling its log file: $itslog"	
		ng_roll_log $vflag $itslog			# roll its log
	else
		verbose "logs not rolled, last roll only $(( ($now - $last_start)/10 )) seconds ago"
	fi

	last_start=$now

	verbose "starting $cmd"
	eval $cmd >>$itslog 2>&1
	ec=$?

	verbose "$id ended; exit code=$ec"
	if ng_goaway -c $goaway_id
	then
		log_msg "exiting; found goaway file"
		ng_goaway -r $goaway_id
		cleanup 0
	fi

	now=`ng_dt -i`
	if [[ $(( $now - $last_start )) -gt 3000 ]]		# up for more than 5 minutes -- consider not a reoccuring error
	then
		loop_count=0
	fi

	delay=$(( ($loop_count * $factor * $min_delay) + $min_delay ))
	if [[ $delay -gt 180 ]]
	then
		delay=180
	fi

	verbose "pausing $delay seconds before attempting restart"
	sleep $delay
	
	loop_count=$(( $loop_count +  1 ))
done
	

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_endless:Start, and restart, a daemon)

&space
&synop	ng_endless [-d work-dir] [-e] [-f] [-l] [-w] [-v] [/path/]command [parameters]
&break
	ng_endless -e command

&space
&desc	&This is a generic wrapper script that will start &ital(command) and restart 
	it each time it stops. Both standard out and standard error from the command 
	are redirected to a log file in &lit($NG_ROOT/site/log) using the basename 
	of command as the log file name.   The log file is rolled (using ng_roll_log)
	such that the previous three copies of the log file are kept.  The log file 
	for &ital(command) is not rolled if it is restarted within an hour of the 
	last time the log file was rolled.

&space
	&This tracks its activities in a separate log file in the same log directory.
	The name used is &lit(endless.)&ital(command.)  The log file will have 
	very little information unless the verbose option is supplied on the 
	command line. 

&space
	&ital(Command) is started immedately after verification.  After &ital(command)
	exits, &this will pause a few seconds before attempting to restart the process. 
	If &ital(command) exits  within 5 minutes of the last restart attempt, the 
	delay between restarts is increased a small amount -- up to a maximum of 
	three minutes.

&space
	To prevent &this from executing for a particular &ital(command) a specialised 
	goaway file must be created. The &lit(ng_goaway) command must be given the 
	-u command parameter where &ital(command) is the basename of the command that 
	&this is managing.
	For example, if &this is managing ng_shunt, the following ng_goaway command 
	will prevent &this from executing to start ng_shunt:
&space
&litblkb
   ng_goaway -X -u ng_shunt ng_endless
&litblke
&space
	The same &lit(ng_goaway) command, with the -r option added, is used to 
	remove the goaway no-execute file. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-d wdir : Causes the &ital(wdir) to be used as the current working directory 
	for the applicaiton. If not supplied &lit(NG_ROOT/site) is used.
&space
&term	-f : Foreground mode.  &This will not detach and log messages will be written 
	directly to the tty device. Primarily this is for testing.
&space
&term	-e : Exit. Signals &this to stop when the currently executing &ital(command) 
	completes. If -e is supplied, the command is required, and all other arguments
	are ignored.
&space
&term	 -l : Indicates that &this should use an alternate name for the command's log
	file.  This option  should be used when the command being invoked has its 
	own log file management built in and the log file name would collide with 
	the name generated by &this. 
&space
&term	-w : Wash environment of various ningaui variables before running the command.
	By default the variables that begin with NG_, srv_, and FLOCK_ are removed from 
	the environment.  If the variable LAUNDRY_LIST is set, then it is expected to 
	contains space separated patterns that will be used to match environment 
	variables and to remove them if defined. 

&space
&term	-v : Verbose mode. 
	
&endterms


&space
&parms	All positional parameters are assumed to be the command name, followed by any 
	arguments that must be passed to the command.  &This does &stress(NOT) support
	supplying redirection parameters on the command line.  All output from the 
	command will be redirected to a log file in &lit(NG_ROOT/site/log).

&space
&exit	&This will exit with a non-zero return code if an error is detected.  It is 
	possible for an error to occur during the attempt to detach the process
	from the controlling terminal, and the creation of the log file.  During 
	the small window of time, errors should be logged to &lit($TMP/log/endless.$$).
	This file is removed after the process has successfully detached from the 
	controlling terminal.

&examp
	The following invokes the ng_parrot and cleans the environemnt of several things 
	using the LAUNDRY_LIST variable:
&space
&litblkb
   LAUNDRY_LIST="NG_ FLOCK_ srv_ CJ_ RM_" ng_endless -v ng_parrot -v 
&litblke

&space
&see	ng_wrapper, ng_s_start, ng_n_start, ng_wstart, ng_start_catman, ng_pp_start, ng_rm_start,
	ng_rmbt_start.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	31 Jul 2004 (sd) : Fresh from the oven.
&term	26 Aug 2004 (sd) : Took debugging (-x) out.
&term	28 Aug 2004 (sd) : Added -d option (working directory).
&term	20 Sep 2004 (sd) : Added support for using only the minimal cartulary.
&term	02 May 2004 (sd) : Added -w (wash) option.
&term	13 Dec 2005 (sd) : Fixed the check for the value of $wash.
&term	17 Jan 2006 (sd) : Force work directory to default to NG_ROOT/site.
&term	03 Feb 2006 (sd) : Force a load of the cartulary and possible wash before each invocation
		and not just as this starts. (HBDKARD) 
&term	19 Aug 2007 (sd) : Updated man page to show correct usage.
&term	04 Jun 2008 (sd) : We now use stdfun's wash_env and not the one originally coded here. 
&endterms

&scd_end
------------------------------------------------------------------ */
