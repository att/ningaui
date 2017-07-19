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
#
# install a package.  for most packages, this just means
# untarring the package and copying it into ng_root.  if an
# INSTALL.ksh script is present at the root of the untarred tree,
# it'll be run instead of the copy.
#. ${NG_CARTULARY:-$NG_ROOT/cartulary}
#. ${NG_STDFUN:-$NG_ROOT/lib/stdfun} 

# called if the command listed by pkg_list is install_it
# davek's original code
function install_it
{
	V=$RANDOM
	mkdir $TMP/.softinst.$$.$V
	SAVEDIR=`pwd`
	cd $TMP/.softinst.$$.$V
	gzip -fdc $1 | tar xf -
	cd $SAVEDIR
	if [[ -f $TMP/.softinst.$$.$V/INSTALL.ksh ]]; then
		. $TMP/.softinst.$$.$V/INSTALL.ksh
	else
		cp -rvp $TMP/.softinst.$$.$V/. $NG_ROOT
	fi
	SAV=$?
	rm -rf $TMP/.softinst.$$.$V
	return $SAV
}


# Mnemonic:	pkginst - install package(s) that are new and desired
# Abstract:	This will obtain a list of package installation commands 
#		from ng_pkg_list and install them.  It can be globally turned
#		off by setting the pinkpages variable NG_PKGINSTALL to off or manual.
#		in manual mode, the script will run only if executed with the -f 
#		option.  If off, it will not run full stop.  If it is set to on or 
#		auto, then it runs automatically.
#		
# Date:		unknown; revised to work with pkg_list 5 Jan 2005 (sd)
# Author:	Dave Kormann
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function is_today
{
	typeset today=""

	date +%a|read today
	if [[ $today = ${1:-junk} ]]
	then
		verbose "today ($today) matches NG_PKGINSTALL: $1"
		return 0
	fi

	verbose "today does not match NG_PKGINSTALL: today=$today  PKGINSTALL=$NG_PKGINSTALL"
	return 1
}

#	if parm is 22 then allow install any time during the hour 22. 
#	if parm is 22-24 then allow install and time between 22:00 and 23:59
function is_hour
{
	typeset hour=""
	typeset parm=${1:-goo}

	date +%H|read hour
	case $parm in 
		*-*)
			if (( $hour >= ${parm%%-*} ))		
			then
				if (( $hour < ${parm##*-} ))
				then
					verbose "hour ($hour) matches NG_PKGINSTALL: $parm"
					return 0
				fi
			fi
			;;

		*)
			if [[ $hour = ${parm%%:*}  ]]		# lop off mm:ss if they supplied it.
			then
				verbose "hour ($hour) matches NG_PKGINSTALL: $parm"
				return 0
			fi
			;;
	esac

	verbose "hour does not match NG_PKGINSTALL: hour=$hour  PKGINSTALL=$NG_PKGINSTALL"
	return 1
}


function send_mail
{
	if [[ -z $NG_PKGINSTALL_MAIL ]]
	then
		return
	fi

	verbose "sending email to: $NG_PKGINSTALL_MAIL"
	cat <<endKat >$TMP/mail.$$

	package install on $myhost
	$1
endKat

	ng_mailer -s "restarting node after package install" -T "$NG_PKGINSTALL_MAIL" -v $TMP/mail.$$
}

# -------------------------------------------------------------------
ustring="[-d sec] [-D] [-f] [-n] [-N] [-P pp-prefix] [-r sec] [-s] [-u] [pkg-name]"

myhost=$( ng_sysname )
force=0;		# set to >1 after check of NG_PKGINSTALL cart variable
forreal=0
rand_delay=0		# random delay before starting node
start_delay=0		# random delay before actually starting work
bounce=1		# by default bounce the daemons; -N turns off
pp_prefix=PKG		# by default we want to install only things listed with PKG_* pp vars; -P can change this
must_solo=0		# set with -s when invoked from rota to prevent runaway multiple scheduling from cron as weve seen on sun

# by default we install only stable things; set to -s if we wish rm_where to return unstable pkgs
case ${NG_PKGINSTALL_STABLE:-stable} in
	false|off|0|not*stable)	rm_state="-s";;
	*)	rm_state="";;
esac

log_msg "starting: pid=$$ $*"

while [[ $1 = -* ]]
do
	case $1 in 
		-d)	rand_delay=300;;	# postpones the restart for a variable amount of time to help prevent a total cluster out
		-D)	start_delay=1;;		# randomly wait before starting any work so as not to slam repmgr with package dump1 reqs
		-f)	force=1;;		# overrides PKGINSTALL=hold (prevent rota execution while allowing manual execution)
		-n)	forreal=-1;;		# MUST be -1, not 0 so that it overrides PKGINSTALL=on
		-N)	bounce=0;;		# dont bounce the node 
		-P)	pp_prefix=$2; shift;;
		-r)	rand_delay=$2;;		# user supplied delay of restart; same as -d except -d is predefined max seconds
		-s)	must_solo=1;;		# we must use solo to prevent multiple kicks from rota/cron

		-u)	rm_state="-s";;	# all (use unstable packages) passed to pkg_list

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

if (( $must_solo > 0 ))
then
	verbose "attempting to fly solo"
	if ! solo
	then
		log_msg "solo attempt failed, aborting"
		cleanup 1
	fi
fi

if (( $start_delay > 0 ))
then
	delay=$(( ($RANDOM % 300) + 10 ))
	verbose "start delay (-D) requested; waiting $delay seconds before starting work"
	sleep $delay
	verbose "continuing after start delay"
fi

# if NG_PKGINSTALL_AVOID has one or more resources listed, then we can only try to install packages 
# when nothing is queued and/or running that references the resources. 
#
restore=""					# command(s) to restore resources if we bail out
if [[ -n $NG_PKGINSTALL_AVOID ]]		# if set, we must avoid running when these woomera resources are not zero
then
	verbose "checking woomera resources to avoid: $NG_PKGINSTALL_AVOID"
	for r in $NG_PKGINSTALL_AVOID 			# set the limits to zero; prep a backout cmd if things running
	do
		ng_wstate -l $r |read limit
		verbose "limit for $r was $limit"
		if (( $forreal >= 0 ))
		then
			 ng_wreq limit $r 0			# set to 0 to prevent any from running for a bit
		fi
		restore="${restore}ng_wreq limit $r $limit; "
	done

	for r in $NG_PKGINSTALL_AVOID 				# now that limits are set to 0, see that nothing is running
	do
		ng_wstate -q -r $r |read queued running
		tot=$(( $queued + $running ))
	done

	if (( $tot > 0 ))
	then
		log_msg "no attempt to install, not all queues are quiet/empty: $NG_PKGINSTALL_AVOID; $queued(queued) $running(running)"
		verbose "running command to reset limits: $restore"
		if (( $forreal >= 0 ))
		then
			eval $restore
		fi

		cleanup 0
	fi

	verbose "ok to install, nothing running/queued for: $NG_PKGINSTALL_AVOID"
fi

case ${NG_PKGINSTALL:-off} in		# controls the state of our ops. if hold, then -f is required to run, if off we will NOT run
	on|auto)	forreal=$(( $forreal + 1 ))		# go unless they have -n on
			msg="would run: "
			;;

	Mon|Tue|Wed|Thu|Fri|Sat|Sun)
			if is_today $NG_PKGINSTALL		# if today matches
			then
				forreal=$(( $forreal + 1 ))		# go unless -n supplied on command line
				msg="would run:"
			else
				forreal=$(( $forreal + $force ))	# go if -f is on and not -n (-f and -n respects -n)
				msg="install day does not match today; use force (-f) to override"
			fi
			;;

	Mon:*|Tue:*|Wed:*|Thu:*|Fri:*|Sat:*|Sun:*)		# today and an hour Fri:22 (fri night 22:00)
			if is_today ${NG_PKGINSTALL%%:*}		# if today matches
			then
				if is_hour ${NG_PKGINSTALL#*:}		# invoked in the right hour
				then
					forreal=$(( $forreal + 1 ))		# go unless -n supplied on command line
					msg="would run:"
				else
					forreal=$(( $forreal + $force ))	# go if -f is on and not -n (-f and -n respects -n)
					msg="install hour does not match current time; use force (-f) to override"
				fi
			else
				forreal=$(( $forreal + $force ))	# go if -f is on and not -n (-f and -n respects -n)
				msg="install day does not match today; use force (-f) to override"
			fi
			;;

	hold)		forreal=$(( $forreal + $force ))	# go if -f is on and not -n (-f and -n respects -n)
			msg="auto install is on hold,  use -f on this command"
			;;

	off)		forreal=-2				# no go even if -f is turned on
			msg="auto install is OFF, set NG_PKGINSTALL to on|hold"

			;;

	*)		log_msg "NG_PKGINSTALL setting is bad: $NG_PKGINSTALL; must be: on|off|hold|day-abbr"
			cleanup 1
			;;
esac


icount=0				# number of packages successfully installed
fcount=0
ng_pkg_list $rm_state -c -P $pp_prefix $1 | while read x		# for each thing listed as needing to be installed
do
	if [[ $forreal -gt 0 ]]			# -n on cmd line OR if hold/off or bad day, forreal is <= 0
	then
		verbose "running: $x"
		if eval $x			# we expect ng_refurb or install_it commands from pkg_list
		then
			log_msg "$x [OK]"
			icount=$(( $icount + 1 ))
		else
			log_msg "$x [FAILED]"
			ng_log $PRI_ALERT $argv0 "@ALERT_NN_pkg_inst $x failed"
			fcount=$(( $fcount + 1 ))
		fi
	else
		log_msg "$msg (cmd=$x)"
	fi
done

if (( $icount > 0 )) && (( $bounce >0 ))
then

	if (( $rand_delay > 0 ))
	then
		if ng_test_nattr srv_Filer		# try to give a bias to having the filer go last
		then
			if( random_delay < 320 )
			then
				r=320			
			else
				r=$random_delay
			fi
		else
			r=$(( ($RANDOM % $rand_delay) + 5 ))
		fi
		verbose "random delay selected, waiting $r sec before initiating stop/start"
		sleep $r
	fi

	msg="$icount packages successfully installed; stopping and starting the node (see $NG_ROOT/site/log/pkg_inst.restart)"
	verbose "$msg"
	send_mail "$msg"
	ng_log $PRI_INFO $argv0 "$icount packages successfully installed; stopping/starting node"
	
	ng_wreq limit Rota_pkg_inst 0		# prevent any queued package installs from kicking off as we must return asap
	ng_wrapper --detach -o1 $NG_ROOT/site/log/pkg_inst.restart -o2 stdout ng_node restart 
else
	verbose "node not restarted: $icount packages installed, $fcount packages failed to install"
	if [[ -n $restore ]]	# must reset limits if there was nothing to restore and we shut them off
	then
		verbose "restoring limits with command: $restore"
		eval $restore
	fi
fi

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_pkg_inst:Install any outstanding packages)

&space
&synop ng_pkg_inst [-d | -r sec] [-f] [-n] [-N] [-P pp-prefix] [-r sec] [-s] [-u] [-v] [pkg-name]

&space
&desc	&This invokes ng_pkg_list for a list of packages that are not currently installed on 
	the node, but should be.  If the list is non-empty, the commands are executed to install
	the package(s).  
&space
	&This modifies its execution with respect to the &lit(NG_PKGINSTALL) variable. The variable 
	may be set to one of &lit(on, off) or &lit(hold,) or may be set to a day of the week abbreviation
	as is returned by the &lit(date) command.  The following explains the action depending on the setting.
&space
&begterms
&term	off : This script is not to install any packages regardless of command line options.
&space
&term	on : This script will install any needed packages unless the no-execution option (-n) is
	set on the command line.
&space
&term	hold : This script will install packages only if the force (-f) option is supplied on the 
	command line.
&space
&term	day : If a day of the week abbreviation (e.g. Mon) is the value of &lit(NG_PKGINSTALL), then 
	the value is assumed to be &lit(on) when the &lit(date) command returns the matching value, and 
	&lit(off) for all other days.  This allows packages to be published multiple times per week, 
	while limiting the installation on a cluster or node to  a single day of the week.  The force (-f) 
	option can be used to override the day seetting if necessary.
&space
&term	day:hr : Same as &ital(day), but supplies an hour on that day as well.  Hours are 0 - 23. 
	Hour may also be supplied as a range in the form of hs-he.  Installs will be allowed starting at
	h1:00 and will not be allowed when h2:00 is reached. Thus a two hour install window from 6:00pm until 
	8:00pm would be coded as: 18-20.  To allow an install between 10pm
	and midnight, code 22-24.  The range cannot cross day boundaries. 
&endterms
&space
	The &lit(hold) setting of the &lit(NG_PKGINSTALL) variable allows for the automatic installation 
	of packages to be disabled from a process such as ng_rota, while allowing manually controlled
	installations. If the value is not set, then the default is &lit(off).

&space
&subcat	The NG_PKGINSTALL_STABLE variable
	The pinkpages variable &lit(NG_PKGINSTALL_STABLE) can control whether or not a node/cluster is allowed
	to install  a package that replication manager considers not to be stable.  If this variable is set to 
	&lit(true) or &lit(1,) then replication manager is querried such that all pacakage names are considered
	regardless of their stability.
&space
&subcat The NG_PKGINSTALL_AVOID variable
	If the pinkpages variable NG_PKGINSTALL_AVOID is set, it is assumed to contain a list of ng_woomera
	resource names.  If any of the jobs currently queued in, or running under ng_woomera, reference any of 
	the resources listed, package installation is not attempted. There is NO override to this other than 
	setting the variable to be an empty string.   This mechanism is used to avoid installing packages (and 
	restarting the node) when there is a significant amount of work to be done such as replication manager sends
	and satellite node compression. 
&space
&subcat The NG_PKGINSTALL_MAIL variable
	This variable can be set to hold one or more email addresses to which mail will be sent after a package
	has been installed and the node is about to be restarted.  Recommended for nodes that are critical (Satellite).
&space
&subcat The PKG_ variables
	Pinkpages variables that begin with &lit(PKG_) are assumed to define the package names (as the remainder of 
	the variable; PKG_potoroo).  The value of each variable is either a date (yyyymmdd) or the word latest. 
	If the -P option is supplied on the command line that defines the prefix to be used in place of &lit(PKG).
	(The manual page for ng_pkg_list should have more details about these variables as the prefix information 
	is passed to that script by &this.)

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-d : Delay the restart of the node.  The restart after successuful install is delayed a random number of seconds
	(between 5 and 300) few seconds.  The filer is always delayed 320 seconds when this option is given. This 
	ensures that the filer is up long enough to support the package install requests when an entire cluster refurbs.
	The attempt is to stagger restarts if an entire cluster is running &this on a scheduled basis. 
	See -r.

&space
&term	-D : Add a delay to the start of execution.  When this option is supplied, the script will wait a random 
	number of seconds before starting work (between 10s and 5 minutes). This helps stagger starting if 
	the cluter schedules package install processes all at the same time. 
&space
&term	-f : Force.  Will force packages to be installed if the value of &lit(NG_PKGINSTALL) is set to &lit(hold),
	or to a specific day of the week.
	This option has no effect if &lit(NG_PKGINSTALL) is set to &lit(off).
&space
&term	-n : No execute mode.  No action will be taken regardless of the setting of &lit(NG_PKGINSTALL).  &This will 
	announce what it would do if this option were not set. 
&space
&term 	-N : No bounce mode. Do not stop and restart the processes on the node after the install.
&space
&term	-P prefix : Set the pinkpages variable prefix which is used to search out package names to use as a default
	list.  If not supplied, and specific package names are not supplied on the command line, then all pink pages
	variables that begin with PKG_ are assumed to have the form PKG_<package-name> and create a list of packages 
	that &this will attempt to install.  
&space
	-r sec : Same as -d excep the user supplies the upper limit.  The filer is always delayed a minimum of 320s.
&space
&term	-s : Force the use of &lit(solo) to ensure that only one package install started from ng_rota (or other periodic
	scheduler) is running at a time.  Will abort if not the first running. 
&space
&term	-u : Use unstable files.  Forces unstable files to be used regardless of the setting of the NG_PKGINSTALL_STABLE
	pinkpages variable.
&endterms


&space
&parms	Optionally, the name of a package may be supplied as the only positional parameter.  When supplied, 
	&lit(PKG_) pinkpages variables are ignored, and &this will install the named package if it should be 
	installed.  When omitted, all packages that are listed with &lit(PKG_) pinkpages variables are considered
	for installation.  The -P option can be used to change the pinkpages variable prefix that is used to 
	determine the 'default' package names used when a name is not supplied on the command line. 


&space
&exit	The exit code is set to the exit code set by the command that was used to install the package. 

&space
&see
	&seelink(tool:ng_refurb), &seelink(tool:ng_pkg_list) &seelink(tool:ng_rota)
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Jan 2005 (sd) : Revised to work with pkg_list, and added &lit(NG_PKGINSTALL) support as well as 
	the self contained documentation.
&term	07 Jan 2005  (sd) : Added node stop/start logic.
&term	24 Jan 2005 (sd) :  Added ability to set PKG_INSTALL to a day of the week.
&term	11 Mar 2005 (sd) : added -N optio to not bounce the node.
&term	14 Jun 2005 (sd) : Added ability to have ddd:hh in PKG_INSTALL.
&term	22 Jul 2005 (sd) : Modified the hour check allowing for ddd:hh-hh range of hours. 
&term	01 Feb 2006 (sd) : Added support for PKG_INSTALL_STABLE variable.
&term	27 Apr 2006 (sd) : Added -P option.
&term	30 May 2006 (sd) : Modified to check NG_PKGINSTALL_AVOID for a list of woomera resources. Package installation
		will be skipped if any of the resources listed have jobs running or queued. 
&term	05 Jul 2006 (sd) : Now restarts the node using ng_node with the restart option.  This is done via a 
		detached wrapper so that the invocation of &this can exit before the shutdown is completed. This script 
		must exit asap so as to allow woomera to exit during the restart.  Using the new ng_node option 
		allows us not to have to run &this detached from woomera; running &this detached from woomera 
		caused issues on relayal with an overlap and restting of INCOMING to 0. 
&term	19 Jul 2006 (sd) : Corrected log file name when restarting.
&term	04 Jan 2007 (sd) : Added -D option.
&term	13 Mar 2008 (sd) : Added an alert message to the master log if installation fails. 
&term	28 Jan 2009 (sd) : Added -r option which allows user to supply the upper limit. 
&term	11 Mar 2009 (sd) : Added -s option to allow calls from rota/cron to force a solo. Fixed usage string. 
&endterms

&scd_end
------------------------------------------------------------------ */
