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
# new version -- takes into account narbalek and narbalek managed cartulary
# Mnemonic:	init - initialises ningaui/potoroo as the system is started. 
# Abstract:	In low/all mode: ensures basic things (crontabs etc) are set up and calls
#		ng_site_prep to ensure that the node is up to snuff. If invoked in high
#		or all mode: starts some core processes and then calls start_node to finish
#		the startup task. We expect this to be run from an rc.local or somesuch 
#		boot time script as root, but it can be run as ningaui/potoroo. The root 
#		crontable is installed ONLY if the user running the script is root, AND
#		the system name begins with ning. Thus this should be safe to run on 
#		smaug/relay where we dont control root's crontab.
#
#		WARNING: this script may run as root!  Dont do anything dangerous!!
#
#		We expect NG_PROD_ID to be set by the ningaui rc script. It should 
#		also be in pinkpages if different than ningaui. 
#
#		NOTE:
#			with the implementation of narbalek, and the deprecation of 
#			cartulary.node cartulary.cluster, there are a few pieces of 
#			info that we must pull from narbalek before it is safe to run
#			any real ng_ scripts. We send a multicast request on the narbalek
#			net and ask for stats -- we use the first to repond as a 'boot host'
#			and make a few queries for important things from it:
#				pinkpages/thishost:NG_CLUSTER
#				pinkpages/thishost:NG_DEF_NATTR
#
#			once these are known, it is safe to go on and start things like
#			our own copy of narbalek.
#	
# Date:		1 April 2003 (no fooling)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------


# ------------------------ functions -------------------------------
# ensure mount references are pointed to correctly, must be run here and not in site prep as we need NG_ROOT/site
# very early and it typically is a mounted filesystem.
#
# we look in system_local_mntd for ng_* things and if they exist we ensure that a symlink from NG_ROOT points to the 
# proper one. we create the symlink if needed and warn if there exists .../ng_something and NG_ROOT/something is not 
# a symlink. this is simalar, but not identical to, the check in site-prep. 
function ensure_m
{
	typeset d=""
	typeset lname=""
	typeset rc=0

	for d in $(ls -d ${NG_LOCAL_MTDIR:-/l}/ng_* 2>/dev/null)		# for any ng_xxxx we expect system to mount 
	do
		lname=${d#*/ng_}
		if [[ -L $NG_ROOT/$lname ]]				# expected and this is ok
		then
			log_msg "init/ensure_m: $NG_ROOT/$lname already exists: $(ls -dl $NG_ROOT/$lname) [OK]"
		else
			if [[ -d $NG_ROOT/$lname ]]
			then
				rm -f $NG_ROOT/$lname/MOUNT_POINT	# must ditch this so it doesn't bugger the rmdir
				rmdir $NG_ROOT/$lname 2>/dev/null	# need to remove if it is empty to allow for simple conversion from old style
			fi

			if [[ ! -e $NG_ROOT/$lname ]]			# if not there, then make a link back to the local mount point 
			then
				if ln -s $d $NG_ROOT/$lname
				then
					log_msg "ensure_m: created link: $NG_ROOT/$lname -> $d  [OK]"
				else
					log_msg "ensure_m: unable to create link: $NG_ROOT/$lname -> $d  [FAIL]"
				fi
			else
				# too early for us to log this in master log; site-prep should do it
				log_msg "old style mountpoint under NG_ROOT exists: $NG_ROOT/$lname [WARN]"  
			fi
		fi
	done

	while [[ -n $1 ]]				# things passed in (e.g. site, tmp) must be mounted now, others equerry can take care of later
	do
		if [[ -f $NG_ROOT/$1/MOUNT_POINT ]]
		then
			log_msg "filesystem not mounted: $d"
			rc=1
		fi

		shift
	done

	return rc
}

# verify that all NG_ROOT/xxxx filesystems are listed in the mount table and that
# equerry is happy with what it has to manage. This function will BLOCK until a 
# happy state is reached. We start a few critical daemons before this function is called. 
function verify_mounts
{
	typeset key=0
	typeset okey=0
	typeset tfile=""
	typeset x=""
	typeset alarm=0			# write alarm only once
	typeset counter=15		# write alarm to ng_log after about 7 minutes

	log_msg "mounted filesystem verification begins..."
	get_tmp_nm tfile | read tfile

	while ! ng_eqreq verify mounts   >$tfile 2>&1
	do
		ng_md5 -t <$tfile | read key
		if [[ $okey != $key ]]			# update with state only when it changes
		then
			log_msg "blocked, waiting on filesystems to mount/stabilise. current state:"
			while read x 
			do
				log_msg "$x"		# dump current state with timestamp
			done <$tfile
		fi

		if (( $counter == 0 && $alarm == 0 ))		# write log message just once in a while
		then
			ng_log $PRI_CRIT $argv0 "node start is blocked, waiting on filesystems to mount/stabilise. see $NG_ROOT/site/log/ng_init"
			alarm=1
			counter=120				# alarm again in an hour or so if we are still blocked
		fi

		counter=$(( $counter - 1 ))

		okey="$key"

		sleep 30
	done

	log_msg "filesystems under $NG_ROOT and ng_equerry are stable"
}

# set up the nattr things that can change while down -- add to static values for the node from $NG_DEF_NATTR
function establish_nattrs
{
	typeset systype=""
	typeset memsize=""
	typeset msize=""
	typeset cpu_count=""
	typeset cpurich=""			
	typeset sqfs=""					# presence of squatter file systems
	typeset tape=""
	typeset mlib=""

	uname -s|read systype 

	ng_ncpus -m |read msize cpu_count
	if [[ $msize -gt 2200000000 ]]		# >2g is large
	then
		memsize="Lgmem"
	fi
	
	if [[ $cpu_count -gt 3 ]]
	then
		cpurich="Cpu_rich"
	fi
	
	if [[ `df |grep -c "/sq0"` -gt 0 ]]
	then
		sqfs="Sqfs"
	fi
	
	if grep "^library" $NG_ROOT/site/config >/dev/null
	then
		mlib="Medialib"
	fi
	
	if grep "^tape" $NG_ROOT/site/config >/dev/null
	then
		tape="Tape"
	fi
	
	export NG_NATTR="$NG_NATTR $cpurich $memsize $sqfs $tape $mlib"		# add to what was hard set 
	log_msg "setting node attributes: $NG_NATTR"
	ng_ppset -l NG_NATTR="$NG_NATTR"						# export the running var
}

function lookup_cluster
{
	ng_nar_get pinkpages/$myhost:NG_CLUSTER|read cluster	# too early to trust pp scripts; nar_xxx are minimal scripts too
	if [[ -n $cluster ]]
	then
		export NG_CLUSTER=cluster
		log_msg "cluster determined by response from narbalek: $NG_CLUSTER"
		return 0
	fi
	
	# not in our narbalek, or our narbalek dead, try another
	log_msg "searching for an alternate narbalek host"
	ng_nar_get -t 5 -b bootstrap_node | read boot_host
	if [[ -n $boot_host ]]
	then
		ng_log "searching for NG_CLUSTER in $boot_host:narbalek"
		ng_nar_get -s ${boot_host:-no-boot-host-found} pinkpages/$myhost:NG_CLUSTER|read cluster	# try another narbalek
	fi
	if [[ -n $cluster ]]
	then
		export NG_CLUSTER=$cluster
		log_msg "cluster determined by response from narbalek on $boot_host: $NG_CLUSTER"
		return 0
	fi
	
	if [[ -f $CARTULARY ]]  
	then
		unset NG_CLUSTER
		grep NG_CLUSTER= $CARTULARY |tail -1| read cluster
		eval $cluster
		export NG_CLUSTER		# set in the eval
		log_msg "cluster determined by entry in existing cartulary ($CARTULARY): $CLUSTER"
		return 0
	fi


	if [[ -f $CARTULARY.prev ]]  
	then
		unset NG_CLUSTER
		grep NG_CLUSTER= $CARTULARY.prev |tail -1| read cluster
		eval $cluster
		export NG_CLUSTER		# set in the eval
		log_msg "cluster determined by entry in backup cartulary ($CARTULARY.prev): $NG_CLUSTER"
		return 0
	fi

	log_msg "WARNING: cannot determine cluster from narbalek, remote narbalek or cartulary"
	export NG_CLUSTER="no-cluster"
}


# start the narbalek name service daemon if we are listed in NG_NNSD_NODES
function start_nnsd
{

	if [[ ${NG_NNSD_NODES:-no_hosts_defined} == *$myhost* ]]		# if we are listed in the hosts that should be running nnsd
	then
		if [[ $myid == "$NG_PROD_ID" ]]
		then
			( 
				cd $NG_ROOT/site/narbalek; 
				log_msg "starting nnsd; pwd=$PWD"
				fling -p -w -e ng_nnsd -v -p ${NG_NNSD_PORT:-29056} 
			)
		else
		( 
			cd $NG_ROOT/site/narbalek 				# should not be needed, but it cannot hurt
			log_msg "switching euid to $NG_PROD_ID to start nnsd; pwd=$PWD"
			su - $NG_PROD_ID -c "$NG_ROOT/bin/ng_wrapper --detach ng_endless -d $NG_ROOT/site/narbalek -w -v  ng_nnsd -v -p ${NG_NNSD_PORT:-29056} "
		)
		fi
	else
		log_msg "nnsd not started, $myhost is not listed in: $NG_NNSD_NODES"
	fi
}

function start_narbalek
{
	typeset nar_nnds_flag=""

	if [[ -z $NG_NNSD_NODES  ]]
	then
		log_msg "WARNING: NG_NNSD_NODES not defined in cartulary.min or environment. narbalek will be started in multicast mode."
	else
		nar_nnds_flag="-d ${NG_NNSD_PORT:-29056}"
		echo "export NG_NNSD_NODES=\"$NG_NNSD_NODES\"" >>$CARTULARY		# endless scrubs values so this MUST be in the cartulary
	fi

	start_nnsd 				# if we are an nnsd node, then we must start it first
	if [[ $myid = "$NG_PROD_ID" ]]			# we need to ensure it is owned by ningaui
	then
		( 
			cd $NG_ROOT/site/narbalek; 
			log_msg "starting narbalek; pwd=$PWD"
			fling -p -w -e ng_narbalek $nar_nnds_flag $nar_league $no_root_flag $nar_agent -v -p ${NG_NARBALEK_PORT:-29055}
		)	
	else
		( 
			cd $NG_ROOT/site/narbalek; 
			log_msg "switching euid to $NG_PROD_ID to start narbalek; pwd=$PWD"
			su - $NG_PROD_ID -c "$NG_ROOT/bin/ng_wrapper --detach ng_endless -d $NG_ROOT/site/narbalek -w -v  ng_narbalek $nar_nnds_flag $nar_league $no_root_flag -v -p ${NG_NARBALEK_PORT:-29055}"
		)
	fi
}

# ------------------------- end functions -- begin some needed setup --------------------------------------
	
# ========================================================================================================
# DANGER:
# 	This script might be run by root at boot time.  Therefore caution must be taken when 
#	executing commands that can/do create files -- they will be left as root/root and possibly 
#	unusable by other parts of potoroo.
#=========================================================================================================

log_warn=/tmp/PID$$.init.warn

export NG_ROOT						# we expect ng_rc.d to set this before we are invoked

if [[ ! -d ${NG_ROOT:-foo}/bin ]]			# must test ng_root/bin -- /ningaui will exist as an empty directory on some systems
then
	echo  "panic: cannot find NG_ROOT, or it seems not to be a valid directory for ningaui: ${NG_ROOT:-root-not-defined}" >&2
	exit 100					# dont use cleanup.
fi


if [[ -r $NG_ROOT/lib/cartulary.min ]]			# pick up an early base of env stuff
then
	. $NG_ROOT/lib/cartulary.min 			# must have something very early on and we cannot trust /cartulary yet
	if [[ -r $NG_ROOT/site/cartulary.min ]]
	then
		. $NG_ROOT/site/cartulary.min		# pickup local overrides if there are any
	fi
else
	echo  "panic: cannot find $NG_ROOT/lib/cartulary.min" >&2
	exit 101
fi

# normally from cartulary, but thats sucked in later so set a few things up now
export PATH="$NG_ROOT/bin:.:$NG_ROOT/common/bin:$NG_ROOT/common/ast/bin:/usr/local/bin:/bin:/usr/bin:/usr/X11R6/bin:/usr/sbin:/sbin"
export LD_LIBRARY_PATH=".:$NG_ROOT/common/ast/lib:/usr/local/lib"
export DYLD_LIBRARY_PATH=".:$NG_ROOT/common/ast/lib:/usr/local/lib"		# darwin

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# set name early like all other scripts, but we get it later

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  	# once path set, it is safe to pull in stdfun
PROCFUN=${NG_PROCFUN-$NG_ROOT/lib/procfun}  	# process oriented std-functions (fling etc)
. $STDFUN
. $PROCFUN

ls /tmp/solo.* $TMP/solo.* >/tmp/solo_list.$$ 2>/dev/null	# list before we would add ours, to delete if we pass solo test
log_msg "attempting to fly solo"

if ! solo						# we must be able to go solo at this point
then
	log_msg "abort initialisation: solo failed"
	cleanup 0
fi

log_msg "solo attempt was successful"		# flying solo, we now clean up all old solo files we found earlier
while read x
do
	rm -f $x
done </tmp/solo_list.$$
rm  /tmp/solo_list.$$
chown $NG_PROD_ID:$NG_PROD_ID /tmp/solo.* $TMP/solo.* 2>/dev/null	# we need to ensure that ours is NOT owned by root should we die
if [[ -z $LOGNAME ]]			# some systems seem not to set these up; lets force them in if need be
then
	export LOGNAME=$NG_PROD_ID;
fi
if [[ -z $USER ]]
then
	export USER=$NG_PROD_ID;
fi

if [[ -z $TMP ]]
then
	echo "abort: TMP environment variable not defined"
	exit 102
fi

# ------------ before we can go any further we need to be sure that things like tmp, and site have symlinks and are mounted
# ------------ unfortunately too early to rely on eqreq verify mounts to do this. 
if ! ensure_m site tmp >/tmp/PID$$.init.tlog 2>&1
then
	cat /tmp/PID$$.init.tlog 
	log_msg "abort! unable to ensure that site and tmp are mounted"
	cleanup 1
	exit 3
fi

if [[ ! -d ${TMP:-foodir} ]]
then
	echo "TMP did not exist -- trying to make: $TMP"
	if ! mkdir -p ${TMP}
	then
		echo "could not make TMP: ${TMP:-TMP not defined}"
		exit 101
	fi

	chown $NG_PROD_ID:$NG_PROD_ID $TMP 
	chmod 777 $TMP

	pd=${TMP%/*}			# fix parent directories too
	while [[ -n $pd ]]
	do
		if [[ $pd != /tmp ]]
		then
			log_msg "fixing ownership/perms on $pd"
			chown $NG_PROD_ID:$NG_PROD_ID $pd
			chmod 777 $pd
		fi

		pd=${pd%/*}
	done
fi

ng_sysname| read myhost			# must have these things defined very early
lib=$NG_ROOT/lib			# spot for things like cart.min and crontab base files


if ! id -u -n 2>/dev/null |read myid		# bloody suns have to be different
then
	id | read myid
	myid=${myid%%\)*}
	myid=${myid#*\(}
fi

if [[ "$*" != *-f* && "$*" != *--man* && "$*" != *-\?* ]]	# immed redirect of log unless man or -? asked for
then
	if [[ $myid != "root" && $myid != "$NG_PROD_ID"  && $myid != "potoroo" ]]
	then
		echo "$argv0b must be run as root, $NG_PROD_ID, or potoroo: you seem to be $myid"
		exit 100
	fi

	logd=$NG_ROOT/site/log
	if [[ -d $logd ]]
	then
		log=$logd/${0##*/}	# capture trace stuff immediately
	else
		echo "using tmp log directory; desired log directory not found: $logd"		# before site prep this may happen
		log=/tmp/${0##*/}	
	fi

	echo "detaching -- messages now directed to: $log"
	$detach			# must detach to prevent rest of boot process from hanging if we take a while to do our thing

	exec 1>>$log 2>&1
	chown $NG_PROD_ID:$NG_PROD_ID $log
	version="2.0/05157"
	log_msg "$argv0b $version: started: $@"			# after switch to log file

fi

cat /tmp/PID$$.init.tlog  >&2 2>/dev/null			# messages from ensure_m saved earlier to get into log file
rm -f /tmp/PID$$.init.tlog 

# make sure cartulary parses cleanly
(
	set -e
	. $CARTULARY
)
if (( $? != 0 ))				# something smells
then
	log_msg "current cartulary does not parse cleanly, using minimal information from lib and site/cartulary.min"

	if [[  -f $NG_ROOT/lib/cartulary.min ]]
	then
		cp $NG_ROOT/lib/cartulary.min $NG_ROOT/cartulary		# establish a min for first scripts that run
		if [[ -r $NG_ROOT/site/cartulary.min ]]
		then
			cat $NG_ROOT/site/cartulary.min >>$NG_ROOT/cartulary
		fi
	
		chown $NG_PROD_ID:$NG_PROD_ID $NG_ROOT/cartulary
		chmod 664 $NG_ROOT/cartulary
	
		if ! gre NG_NNSD_NODES $NG_ROOT/cartulary.prev >>$NG_ROOT/cartulary	# must have this and we DONT want it hardcoded in cart.min
		then
			log_msg "could not find NG_NNSD_NODES in previous cartulary, narbalek may start in multicast mode [WARN]"
		fi
	else
		log_msg "$NG_ROOT/lib/cartulary.min not found, aborting [FAIL]"
		cleanup 10
	fi
else								# clean cartulary, save for historical reference
	. $CARTULARY						# it parsed so suck it in
	cp $NG_ROOT/cartulary $NG_ROOT/cartulary.prev
	chown $NG_PROD_ID:$NG_PROD_ID $NG_ROOT/cartulary.prev
	chmod 664 $NG_ROOT/cartulary.prev
fi



# --------  its now safe to process command line options and do real work ---------------------------
# --------  but do NOT run any ng_ commands that need a full cartulary until it is rebuilt ----------

ustring="[-f] [-v]"
uname -a | read systype junk		# in case it matters
what=all				# causes emulation of both the old rc05 and rc95 scripts
argv0b="${argv0##*/}"
ng_sysname -s|read thishost	


while [[ $1 = -* ]]
do
	case $1 in 
		-f)	;;				# ignore -- allow -f foreground to keep from detaching above
		-h)	what=high;;			# deprecated, but we dont barf if we get them
		-l)	what=low;;			# deprecated, but we dont barf if we get themmulate rc05 script

		-v)	verbose=1;;			# unnecessary as we log everything with this
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

					# must start narbalek before everything so we can generate a new cartulary
if [[ ! -d $NG_ROOT/site/narbalek ]]			# must have these before we can start narbalek 
then
	if ! mkdir -p $NG_ROOT/site/narbalek
	then
		log_msg "abort: could not make $NG_ROOT/site/narbalek"
		cleanup 1
	fi

	log_msg  "created: $NG_ROOT/site/narbalek"
	chmod 775 $NG_ROOT/site/narbalek
	chown $NG_PROD_ID:$NG_PROD_ID $NG_ROOT/site/narbalek
fi

if [[ ! -d $NG_ROOT/site/log ]]
then
	if ! mkdir -p $NG_ROOT/site/log
	then
		log_msg "abort: could not make $NG_ROOT/site/log"
		cleanup 1
	fi

	log_msg  "created: $NG_ROOT/site/log"
	chmod 775 $NG_ROOT/site/log
	chown $NG_PROD_ID:$NG_PROD_ID $NG_ROOT/site/log
fi

if [[ -f $NG_ROOT/site/narbalek/agent ]]		# this host needs a multicast agent for narbalek
then
	read nar_agent <$NG_ROOT/site/narbalek/agent
	log_msg "narbalek will be directed to multicast agent at: $nar_agent" 
	eval nar_agent=\"-a $nar_agent\"			# command line option for narbalek (allow $PARROT_PORT in file )
fi

if (( ${NG_NARBALEK_NOROOT:-0} > 0 ))		# narbalek on this node is specifically asked not to be root
then
	no_root_flag="-P"
else
	no_root_flag=""
fi

if [[ -n $NG_NARBALEK_LEAGUE ]]			# this MUST be defined in site/cart.min 
then
	nar_league="-l $NG_NARBALEK_LEAGUE"
else
	nar_league=""
fi

start_narbalek				# narbalek and nnsd must be started REALLY early

	
x=""
while [[ -z "$x" || $x = *ORPHANED* ]]
do
	log_msg "waiting for narbalek to stabilise: $x"
	sleep 1
	ng_nar_get -t 1 -i |read x		# get a status dump; when ORPHANED goes away, nar is up and parented
done
if [[ $x = *I_AM_ROOT* ]]			# if our narbalek thinks we are root, we must pause -- the macs seem not to hear the 
then						# messages from the real root for a bit and it leads to bad things if we build cartulary now
	tries=90
	log_msg "narbalek has declared itself root; we must give time to resolve any root collision"
	while [[ ($tries -gt 0  &&  $x = *I_AM_ROOT* ) || $x = *ORPHANED* ]]
	do
		tries=$(( $tries - 1 ))
		log_msg "waiting for narbalek root stabilisation: $x ($tries)"
		sleep 1
		ng_nar_get -t 1 -i |read x		# continue to peek at status until timeout or our narbalek has ceeded 
	done
fi
sleep 1						# ensure narbalek gets a load before we start digging info from it
log_msg "narbalek seems alive and well: $x"

if [[ -z $NG_CLUSTER ]]			# in general we expect it not to exist, but this allows it to be preset for testing
then
	lookup_cluster			# get it from narbalek, or last existing cartulary if narbalek not there.
fi

ng_pp_build -v				# reconstruct the cartulary from narbalek
chown $NG_PROD_ID:$NG_PROD_ID $CARTULARY
. $CARTULARY				# and lets start to use it ------ now safe to run commands that need full cartulary ------

cluster=$NG_CLUSTER			# must test the value that was set in cartulary to ensure it is sane -- else we crash
if [[ -z $cluster || $cluster == "#ERROR#" ]]			# it should at least have been set to no-cluster in lookup, but ensure its not ""
then
	log_msg "abort: cannot find cluster name!"
	ng_log PRI_CRIT $argv0 "abort: cannot find cluster name!"
	cleanup 1
	exit 1			# take no chances here
fi
log_msg "will join cluster: $cluster"
ng_nar_set -a $NG_CLUSTER		# affiliate it with our cluster



chmod 775 $CARTULARY
chown $NG_PROD_ID:$NG_PROD_ID $CARTULARY


# ---- build cron tables for both ningaui and root ----
# The real path for ng_root must be hard coded in the crontab, hence the sed to do it 
# based on the value in the local cartulary
(
	echo "# do not edit. generated at boot by $argv0b"
	cat $lib/crontab_base.ningaui 
	for x in $NG_ROOT/lib/crontab.$cluster.ningaui $NG_ROOT/lib/crontab.$myhost.ningaui 
	do
		if [[ -r $x ]]			# dont cause error if no file
		then
			echo "# ---- entries from $x -----"
			cat $x
		fi
	done
) | sed  "s!_ROOT_PATH_!$NG_ROOT!g" >$lib/crontab.ningaui

if [[ $myid != "root" ]]		# on bsd cannot use -u unless root
then
	crontab $lib/crontab.ningaui
else
	crontab -u ningaui $lib/crontab.ningaui
fi

# -- 4 Apr 2008 -- decision not to create crontab.root was made so code is commented
#	out along with the note that we are not installing it.  
# we build the root crontab regardless of situation, though we do NOT install it
# it exists as a suggestion to systems admins to use as or add to existing root crontab
#(
#	echo "# do not edit. generated at boot by $argv0b"
#	cat $lib/crontab_base.root 
#	for x in $NG_ROOT/lib/crontab.$cluster.root $NG_ROOT/lib/crontab.$myhost.root 
#	do
#		if [[ -r $x ]]		#dont cause error if no file
#		then
#			echo "# ---- entries from $x -----"
#			cat $x
#		fi
#	done
#) | sed  "s!_ROOT_PATH_!$NG_ROOT!g" >$lib/crontab.root
#
#log_msg "root crontab $lib/crontab.root not installed" 		# emperical decision not to install the root crontab

#if [[ $myid = "root" ]] && ng_test_nattr !Satellite		# safe to install 
#then
#	crontab -u root $lib/crontab.root
#else
#	log_msg "root crontab ($lib/crontab.root) not installed: not root or this is a satellite node"
#fi


# must start equerry before we run site-prep; must start after narbalek is kicked off
verbose "starting equerry"
fling -e ng_equerry -v 			# filesystem mounter -- can mount everything except NG_ROOT, NG_ROOT/tmp, and NG_ROOT/site
sleep 2
verbose "verifing mounts"
verify_mounts				# block until equerry is happy

log_msg "==== site prep starts ========================================================" >&2
if ! ng_site_prep -v
then
	log_msg "==== site prep failed ========================================================"
	ng_node stop
	log_msg "system stopped because of site prep failure"
	cleanup 1
fi

log_msg "==== site prep ends   ========================================================" >&2

log_msg "primary initialisation ends"

					# setting up attributes must be done after site-prep as we depend on */site/config stuff updated by site-prep
NG_NATTR="$NG_DEF_NATTR"		# set our attributes to the default, and then go add boot-time generated attrs
establish_nattrs			# set the node attributes in the cartulary.local file

log_msg "starting node...."

if [[ -n $NG_BROADCAST_NET ]]		# if a broadcast network is defined, select the interface and set vars
then
	log_msg "setting local cartulary broadcast interface names based on: $NG_BROADCAST_NET"
	ng_setbcast -N $NG_BROADCAST_NET -V LOG_IF_NAME
	ng_setbcast -N $NG_BROADCAST_NET -V CLUSTER_IF_NAME
fi

# now the ningaui stuff
log_msg "starting ningaui node daemons (ng_node)"
cd $NG_ROOT/site
if [[ $myid = "$NG_PROD_ID" ]]		# CAUTION:  the option to ng_node must be with a capital S
then
	ng_node Start			# as it allows user to start with start which invokes this script
else
	su - $NG_PROD_ID  -c "$NG_ROOT/bin/ng_wrapper ng_node Start" 
fi
log_msg "-------------------- end node start messages -------------------"

log_msg "initialisation complete"

cleanup 0



exit
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(start:Start various potoroo components)

&space
&synop	ng_init [-f] [-v]

&space
&desc	&This does very low level initialisation of potoroo things as the 
	system is started.  It is expected that &this will be invoked by the 
	&lit(rc.local) or somesuch boot time launching mechanism. It
	is assumed that this script might be invoked with the effective user id
	of &ital(root)   and will take the necessary steps to switch to the 
	ningaui production id when necessary. (It is strongly recommended that 
	the systems administration staff use the ningaui rc.d script to invoke
	this script; the ningaui rc.d script switches to the ningaui id before
	this script is invoked.)
&space
	The following tasks are preformed by this script:
&space
&beglist
&item	Removes solo files from /tmp and $TMP.
&item	Establish at least a minimal cartulary if none is present.
&item	Create the potoroo crontab and install it
&item	Executes ng_site_prep to prepare the node and ensure the necessary directory 
	structure exists.
&item	Executes ng_node to start ningaui processes.
&item	Sets LOG_IF_NAME and CLUSTER_IF_NAME based on NG_BROADCAST_NET
&endlist
&space
&subcat The Environment and Cartulary
	The only assumption made by &this is that NG_ROOT is set in the environment
	before it is invoked.  If it is not set, it will default to &lit(/ningaui)
	and will abort if that does not seem to be an appropriate directory for 
	NG_ROOT.

&space
	As &this is executed before ng_narbalek has started, the contents of 
	&lit(NG_ROOT/lib/cartulary.min) and &lit(NG_ROOT/site/cartulary.min) 
	are used as an initial environment base. Values in the site cartulary.min
	file override values in the lib copy (a static copy distributed with potoroo)
	of the file.  Other than the minimal environment (PATH, well known port 
	assignments and ng_log information), &this specifically uses the following 
	variables from these files:
&space
&lgterms
&term	NG_NARBALEK_LEAGUE : Defines the league that narbalek is started with. The
	combination of the narbalek port value, and the league name, are used by 
	narbalek to determine which other nodes it should be communicating with
	when inserting itself into the communications tree. 
&space
&term	NG_NARBALEK_NOROOT : If set to 1, &this will start narbalek with the option that
	prevents this node from becoming the root of the narbalek tree.  This 
	option is generally desired for Satellite nodes. 

&term	NG_NNSD_NODES : This is a list of nodes that the narbalek name service daemon
	should be started on.  For a node that must start the service, this 
	variable must be defined in the site/cartulary.min file to ensure that the 
	service is started if the cartulary file cnanot be used.

&space
&term	NG_PROD_ID : This defaults to &lit(ningaui) if not defined. If a production 
	user name that is to be used to execute core ningaui daemons and processes
	is to be used, this variable is expected to be defined in the &lit(cartulary.min)
	file in the &lit(NG_ROOT/site) directory.
&endterms



&space
&opts	The following options are recognised by &this when placed on the command line
&smterms
&term	-f : Run in an interactive (foreground) mode. If omitted, this script detaches
	and logs messages to NG_ROOT/site/log/ng_init.
&space
&term	-v : Verbose mode. Might cause more information to be written to the standard
	error device.
&endterms

&space

&files	 These files are important in some way to &this:
&space
&lgterms
&term	$NG_ROOT/lib/cartulary.min : Expects to find the minimal cartulary data here. This 
	is used (with the cartulary.min file in site) to establish an initial environment 
	before narbalek is started. Further, if the cartulary cannot be created, the contents
	of this file, and the one in site, are used to create the cartulary.
&space
&term	$NG_ROOT/site/cartulary.min : This file, if it exists, provides node specific overrides
	to the values in the copy contained in the &lit(NG_ROOT/lib) directory. The file 
	may also contain node specific values that are not in the lib copy, but are needed
	before a cartulary can be built using narbalek.
&space
&endterms

&space
&see	&seelink(tool:ng_node) &seelink(tool:ng_site_prep) &seelink(ng_rc.d)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Apr 2003 (sd) : Initial try.
&term	05 May 2003 (sd) : Corrected a path issue affecting pp_build.
&term	20 Jun 2003 (sd) : to do better when starting on a fresh system -- cartulary
	build issues corrected.
&term	07 Aug 2003 (sd) : Modified to remove -l from su commands as this causes FreeBSD
	5.1 to hang.
&term	09 Dec 2003 (sd) : Added more attributes that we collect at start time
&term	16 Feb 2004 (sd) : added call to bo_peep to set up flock variables.
&term	16 Mar 2004 (sd) : Corrected duplication of nattr problem if NG_NATTR not defined in local cart.
	Also ensures that $NG_ROOT exists before dotting it in. 
&term	09 Jul 2004 (sd) : Now starts ng_logd with correct parms if run on a satellite
&term	28 Jul 2004 (sd) : Now drops from the starting process so as not to block rest of boot process.
&term	03 Sep 2004 (sd) : Changes to support existance of narbalek and cartulary generated from narbalek.
&term	26 Oct 2004 (sd) : Added support for narbalek multicast agent. 
&term	17 Feb 2005 (sd) : Added export to PATH -- seemes needed for SuSe linux.
&term	03 Mar 2005 (sd) : Added check and delay if narbalek initialises as the root node. The macs tend to 
	do this and then end up resolving the root collision after a few seconds. We must wait for this 
	resolution to pass before we can safely go on.
&term	17 Mar 2005 (sd) : Rearranged, and set some better exit messages.
&term	03 May 2005 (sd) : Changes to shift to lib from adm.
&term	08 Jul 2005 (sd) : Added test for TMP as on the sun nodes it gets trashed.
&term	18 Jul 2005 (sd) : Fixed issue with where TMP is checked -- caused problems with reboot.
&term	21 Jul 2005 (sd) : Now pulls in a minimal cartulary as soon as it can.
&term	02 Aug 2005 (sd) : Removed reference to adm/cartulary.min as a backup to lib/cartulary.min
&term	08 Sep 2005 (sd) : Now invokes narbalek via wrapper if we are run as root; suse issues.
&:term	18 Sep 2005 (sd) : Fixed invocation of ng_init from root (added - to su).
&term	14 Sep 2005 (sd) : Now does a node stop if site_prep fails to leave everything down.
&term	13 Dec 2005 (sd) : Set wash flag on nnsd and narbalek invocations so that environment is cleaned 
		before the process is started. Added call to solo to prevent duplicate starts.
&term	30 Jan 2006 (sd) : Corrected a typo in the su call to start narbalek.  -p was given instead of -d. 
&term	26 Feb 2006 (sd) : Bloody suns do not support -un option on id; forced to change.
&term	27 Mar 2006 (sd) : Added a bit more armour to prevent problems when run as root (esp. on suns).
&term	31 Mar 2006 (sd) : Changed setting of NG_ROOT.
&term	01 Apr 2006 (sd) : Fixed bug introduced yesterday -- missing } on ROOT check.
&term	28 Apr 2006 (sd) : Aborts if $TMP is not set. 
&term	21 Jun 2006 (sd) : Cleanup of default string to something sane. 
&term	07 Aug 2006 (sd) : Moved establishment of attributes to be after siteprep as some attributes depends on info 
		in site/config which is setup in site-prep.
&term	19 Feb 2007 (sd) : Removed the install of the root crontab. Emperical decision made not to demand that one exist.
&term	27 Feb 2007 (sd) : Fixed reference to adm in man page. 
&term	03 Apr 2007 (sd) : Removed dependancy on /ningaui/ng_wrapper.
&term	15 May 2007 (sd) : Some cleanup, and we no longer attempt to use the old cartulary in order to avoid one that
		was corrupted during a system crash (mostly to fix issues with this on SGI).
&term	29 Jun 2007 (sd) : Changed quotes from single to double on su for the ng_node start call.  Seems that some
		flavours of Linux do not drive the profile before invoking the command. 
&term	15 Oct 2007 (sd) : Modified to use NG_PROD_ID rather than ningaui as the user id for production start/stop.
&term	28 Mar 2008 (sd) : Converted references to NG_ROOT/cartulary to $CARTULARY and set CARTULARY based on existance 
		of $NG_ROOT/site/cartulary.
&term	29 Apr 2008 (sd) : We now must ensure that USER and LOGNAME are set; some flavours dont set it when root uses su to invoke this.
&term	30 Apr 2008 (sd) : Handle case where cluster look up comes back as #ERROR#.
&term	06 Jun 2008 (sd) : Added code to ensure that symlinks and/or directories for site and tmp exist. We abort if not as we cannot
		exist without them.
&term	17 Jun 2008 (sd) : To use NG_NARBALEK_LEAGUE from the cartulary.min (assumed to be site) to set the league for 
		narbalek. Also cleaned up/enhanced the man page. 
&term	15 Apr 2009 (sd) : We now start equerry before siteprep as siteprep expects all things to be mounted, including parking lots.
&endterms

&scd_end
------------------------------------------------------------------ */
