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
# Mnemonic: site_prep 
# Abstract: 	Ensure that the basics are there.  This should make the assumption 
#		that the cartulary exists, but might be MINIMAL! 
#
# WARNING:	This script may run as root!
#
# ------------------------------------------------------------------------------------

if [[ ! -d $NG_ROOT/bin ]]
then
	echo "cannot validate NG_ROOT ($NG_ROOT) [FAIL]"
	exit 1 
fi 

if [[ -z $NG_CARTULARY ]]
then
	if [[ -f $NG_ROOT/site/cartulary ]]
	then
		NG_CARTULARY=$NG_ROOT/site/cartulary
	fi
fi

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN

#  keep only n days ($3) worth of files ($2) in the dir ($1) 
function age_out
{
	log_msg "aging off from d=$1 f=$2 a=$3"
	find ${1:-junk} -name "${2:-junk}" -mtime +${3:-2} -ls -exec rm {} \;
}


# ensure that we own the file(s), and set perms 
# must_own file OR must_own perms file1 file2 file3...
# we must jack round moving and copying it because we cannot not take ownership.  erk.
function must_own
{
	if [[ $# == 1 ]]
	then
		typeset perm="660"
	else
		typeset perm=$1
		shift
	fi

	while [[ -n $1 ]]
	do
		if [[ -e $1 ]]			# we sometimes get /foo/bar?? so to avoid garbage messages we now check
		then
			if [[ ! -d $1 ]]
			then
				log_msg "setting ownership of (regular file) $1 perm=$perm"
				rm -f ${1}_orig
				mv $1 ${1}_orig 2>/dev/null		# this is silly, but necessary to snarf ownership
				cp ${1}_orig $1 2>/dev/null
				rm -f ${1}_orig
			else
				log_msg "setting ownership of (!file) $1 perm=$perm"
			fi
		
			chmod $perm $1 2>/dev/null
			chown ${NG_PROD_ID:-ningaui}:${NG_PROD_ID:-ningaui} $1	2>/dev/null	# if we run as root, the previous cmd leaves ownership bad
			log_msg "must-own: $(ls -ald $1)" >&2
		fi

		shift
	done
}

# ensure dir ($1) exists, create if it does not. always set owner/grp to $2/$3
function ensure
{
        typeset perm="775"
        if [[ $1 == "-p" ]]
        then
                typeset perm="$2"
                shift 2
        fi

	eval typeset dir=$1		# eval because if $1 is $NG_ROOT/something we want it to expand
	typeset own=${2:-$NG_PROD_ID}
	typeset grp=${3:-$NG_PROD_ID}
	
	if [[ -z $1 ]]		# likely an undefined variable
	then
		return
	fi

	if [[ -d $dir ]]
	then
		if (( $verbose > 0  ))
		then
			printf "ensure: existed: %-45s " $dir
		fi
	else
		if [[ $dir == *"/"* ]]
		then
			ensure ${dir%/*}			# ensure parent is there too
		fi

		printf "ensure: creating: %-45s " $dir
		if ! mkdir $dir
		then
			ng_log $PRI_CRIT $argv0 "cannot make directory: $dir"
			log_msg "ERROR: ensure: cannot make dir: $dir"
			cleanup 1
		fi
	fi


	chown $own:$grp $dir 2>/dev/null		# always
	if ! chmod $perm $dir
	then
		ng_log $PRI_ERROR $argv0 "unable to set permissions and/or owner to $own:$grp: perm=$perm dir=$dir"
	fi
	ls -ald $dir
}

# we look in system_local_mntd for ng_* things and if they exist we ensure that a symlink from NG_ROOT points to the 
# proper one. we create the symlink if needed and warn if there exists .../ng_something and NG_ROOT/something is not 
# a symlink.  Some of this is done in ng_init in a similar, but not exactly duplicate, manner.  We do it again here
# as equerry might have mounted some of these filesystems, and we now can log warnings about old school mounts 
# in the NG_ROOT directory.
function ensure_m
{
	typeset d=""
	typeset lname=""

	for d in $(ls -d ${system_local_mntd}/ng_* 2>/dev/null)
	do
		lname=${d#*/ng_}
		if [[ -L $NG_ROOT/$lname ]]		# expected and this is ok
		then
			log_msg "ensure_m: $NG_ROOT/$lname already exists: $(ls -dl $NG_ROOT/$lname) [OK]"
		else
			if [[ -d $NG_ROOT/$lname ]]
			then
				rmdir $NG_ROOT/$lname 2>/dev/null	 # need to remove if it is empty to allow for simple conversion from old style
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
				msg="old style mountpoint under NG_ROOT exists: $NG_ROOT/$lname"
				ng_log $PRI_WARN $argv0 "$msg"
				log_msg "$msg [WARN]"
			fi
		fi
	done
}


# ensure file ($1) is there. change owner/grp and run an initilisation command ($4) if supplied
function ensure_f
{
        typeset perm="664"
        if [[ $1 == "-p" ]]
        then
                typeset perm="$2"
                shift 2
        fi

	typeset file=$1
	typeset own=${2:-$NG_PROD_ID}
	typeset grp=${3:-$NG_PROD_ID}
	typeset cmd="$4"
	
	if [[ -z $file ]]
	then
		return
	fi

	if [[ ! -e $file ]]
	then
		if ! touch $file
		then
			log_msg "WARNING: could not create file: $file"
			return 1
		fi
	fi

	chown $own:$grp $file		# always
	chmod $perm $file

	if [[ -n "$cmd" ]]
	then
		log_msg "initialising $file with: $cmd"
		eval $cmd
		if (( $? > 0 ))
		then
			log_msg "WARNING: file initialisation may have failed: $file(file) cmd=$cmd"
		fi
	fi

	return 0
}

#  parms are same order as ln -s wants:  existing-file  symlink-path
function ensure_l
{
	if [[ ! -e $1 ]]
	then
		verbose "creating link $2 -> $1"
		if ! ln -s $1 $2
		then
			log_msg "WARNING: could not link $2 -> $1"
		fi
	else
		verbose "link existed: $2 -> $1"
	fi
}

# ------------------------------------------------------------------------------------

verbose=0
ustring=""
system_local_mntd="${NG_LOGAL_MTDIR:-/l}"		# for now we insist that this is the local mount point for our filesystems

while [[ $1 == -* ]]
do
	case $1 in 

		-v)	verbose=1;;

		-t)	ensure '$NG_ROOT/site/test_dir'
			cleanup $?
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

log_msg "starting: NG_ROOT=$NG_ROOT NG_PROD_ID=${NG_PROD_ID:-default}"

uname -a -n | read user
ng_sysname | read mynode
adm=$NG_ROOT/adm
lib=$NG_ROOT/lib
site=$NG_ROOT/site


ng_fixup -v 			# set any flock related simlinks in bin and lib dirs -- MUST be done very early

# -------------------- root things -- deprecated -----------------------------------------
#chmod a+rwx /tmp



# -------------------- $NG_ROOT $TMP things ----------------------------------------------
# these should be filesystems, but on a satellite they may not be, so ensure they are there.

if [[ ! -d $NG_ROOT ]]
then
	log_msg "ABORT:  NG_ROOT ($NG_ROOT) seems not to be a directory!"
	exit 1
fi

# silly, but weve seen cases where multiple things were mounted /ningaui 
ng_df | awk '{count[$NF]++} END { for( x in count ) if( count[x] > 1 ) bad = 1; print bad+0}' | read bad
if (( $bad > 0 ))
then
	log_msg "ABORT: duplicate mountpoint directories found in df listing"
	ng_df >&2
	exit 1
fi

ensure_m			# ensure that anything ng_* in $system_local_mntd has a link in NG_ROOT

TMP=${TMP:-$NG_ROOT/tmp}	# based on cart variable with NG_ROOT/tmp default -- on satellites it may not be $NG_ROOT/tmp
ensure $TMP
must_own 2777 $TMP 

ensure -p 775 $NG_ROOT/site
ensure_f $NG_ROOT/site/pkg_hist
ensure -p 775 $NG_ROOT/bin
ensure -p 775 $NG_ROOT/lib
ensure -p 775 $NG_ROOT/users
ensure -p 775 $NG_ROOT/users/${NG_PROD_ID:-ningaui}

ensure $TMP/log			# stdout log files for cycle related jobs
ensure $TMP/repmgr		# private idahos for various things
ensure $TMP/seneschal		
ensure $TMP/zoo

age_out $TMP "rmsync*" 2	# keep only two days of rmsync tmp files

# ensure owners/modes for standard filesystems
# site allowed to be 775 as of 9/05/08 in order to support site/www
for i in $NG_ROOT/rmfs???? $NG_ROOT/fs?? $NG_ROOT/sq?? #  $NG_ROOT/site 
do
	must_own 770 $i
done


# ------------------------ site things ---------------------------------------------------
CONFIG=$NG_ROOT/site/config

>$NG_ROOT/site/forward				# commands are placed here for later execution by ng_node once greenlight is up

# make sure we know our hardware config
>$CONFIG						# start empty, all commands should append
if ng_test_nattr !Satellite
then
	ng_scsi_map -i -t changer -s "/dev/sg[0-9] /dev/ch[0-9]" >>$CONFIG		# mtx wants generic device, so find the one that reports changer and record
	ng_scsi_map -b -t tape -s "/dev/nst? /dev/nsa[0-9]" >>$CONFIG			# scsi-tape on linux, sequential-access on others
	ng_set_wwpn >>$CONFIG


	#if [[ -r /proc/scsi/scsi ]]			# linux only
	#then
	#	cat /proc/scsi/scsi | awk '
	#	BEGIN	{ rawdev = tapedev = -1 };
	#	/Channel:/	{ rawdev++; }
	#	/Type: *Medium Changer/	{ printf("library	/dev/sg%d\n", rawdev) }
	#	/Type: *Sequential-Access/	{ tapedev++; printf("tape	/dev/nst%d\n", tapedev) }
	#	' > $CONFIG
	#	chown ${NG_PROD_ID:-ningaui}:${NG_PROD_ID:-ningaui} $CONFIG
	#	chmod 664 $CONFIG
	#else

	#if [[ -e /dev/sa0 ]]
	#then
	#	ls /dev/sa[0-9] 2>/dev/null |awk ' { printf( "tape  %s\n", $0 ); } ' >$CONFIG		# assume sequential access devices are tapes
	#fi
fi

# verify tape scratch dirs exist
verbose "verifying scratch tape directories"
for dev in $(awk '$1 == "tape" { print $2 }' $CONFIG)
do
	rawdev=$(basename $dev)
	x=$(ng_ppget TAPE_SCRATCH_DIR_$rawdev)
	if test -z "$x"
	then
		ng_log $PRI_WARN site_prep "$dev: pp var TAPE_SCRATCH_DIR_$rawdev is undefined"
		log_msg "WARNING: $dev: pp var TAPE_SCRATCH_DIR_$rawdev is undefined"
	else
		eval ensure $x
	fi
done

log_msg "attempting to set permissions for tape drives; running as $(id)"
awk '/tape/ || /library/ { print $2 }' <$CONFIG | while read d junk
do
set -x
	chmod 660 $d >/dev/null 2>&1		# this will fail if not root; we dont care
set +x
	ls -al $d >&2
done

if [[ -f /etc/permissions.local ]]  && whence chkstat >/dev/null
then
	chkstat -set /etc/permissions.local			# will work only when run as root
fi

log_msg "after ckstat and permission changes"
ls -al /dev/n* >&2

					# working directories for various daemons
ensure $NG_ROOT/site/seneschal
ensure $NG_ROOT/site/narbalek
ensure $NG_ROOT/site/catalogue	
ensure $NG_ROOT/site/woomera
ensure $NG_ROOT/site/log			# main daemon private logs 
ensure $NG_ROOT/site/tape
#ensure $NG_ROOT/site/waldo			# these are manual (per Beth) so we dont need to set these
#ensure $NG_ROOT/site/waldo/base
#ensure $NG_ROOT/site/waldo/lib
#ensure $NG_ROOT/site/waldo/lists
#ensure $NG_ROOT/site/waldo/sets
repmgr=$NG_ROOT/site/rm			
ensure $repmgr
ensure $repmgr/nodes				# directory for file list files

stats=$NG_ROOT/site/stats			# statistics directories 
ensure 	$stats
ensure	$stats/nodes				# stats files from other nodes on this cluster
ensure	$stats/cnodes				# stats from all nodes in flock if this is flock collector
for s in $stats/*
do
	must_own 775 $s				# ensure all existing files in these places are open
done


# ---------------------------- cartulary cleanup things ----------------------------------
log_msg "purging cartulary.new files"
rm -f $CARTULARY.new.* 2>/dev/null

# ---------------------------- replication manager file arenas ---------------------------
ng_rm_arena -v init		# regular replication manager space
ng_rm_arena -v pkinit		# parkinglot space
log_msg "active after arena initialisation: $(ng_rm_arena list)"
log_msg "active after arena pk initialisation: $(ng_rm_arena pklist)"

# ------------------------------------ log ------------------------------------------------
log=$NG_ROOT/site/log		
rota_log=$log/rota
ensure $log
ensure_f $log/master
ensure_f $rota_log
ensure_f $XFERLOG			# XFERLOG should be set in cartulary
chown ${NG_PROD_ID:-ningaui}:${NG_PROD_ID:-ningaui} $log/*
chmod 664 $log/*


# ---------------------------------- bin -------------------------------------------------
if [[ ! -x $NG_ROOT/bin/ng_rm_db ]]		# if one did not exist, set a link based on preference: dbd then gdbm then ndbm
then
	if [[ -x $NG_ROOT/bin/ng_rm_dbd  ]]  
	then
		ln -s $NG_ROOT/bin/ng_rm_2dbd $NG_ROOT/bin/ng_rm_db
		log_msg "ng_rm_db set to 2dbd flavour"
	else
		log_msg "ERROR! could not set ng_rm_db; missing $NG_ROOT/bin/ng_rm_dbd; not not started"
		ng_log $argv0 $PRI_CRIT "could not set ng_rm_db link, node not started"
		exit 1
	fi

else
	log_msg "ng_rm_db existed: $(ls -al $NG_ROOT/bin/ng_rm_db)"
fi

ensure_f $NG_ROOT/site/rm/rm_dbd.ckpt


# ------------------------------------ lib ------------------------------------------------
ensure $NG_ROOT/lib
ensure $NG_ROOT/lib/pz				# pzip pin files 

# ----------------------------------- satellite ------------------------------------------
#	caution -- there are als satellite node oriented things in the repmgr arena section

if ng_test_nattr Satellite		
then
	ensure $NG_ROOT/incoming
	chmod 777 $NG_ROOT/incoming
fi

ensure $NG_FTP

# we need to fix NG_ROOT in the wrapper and install in NG_ROOT/bin. We also make a link in /ningaui if it exsits and
# in our home directory.  preferred method is for users to get root with:
#	~ningaui/ng_wrapper -r  
# setting up /ningaiu/ng_wrapper is old school and abandoned because it required sysadmin setup and was politically 
# difficult in some environments.
#
if [[ -f $NG_ROOT/bin/ng_FIXUP_wrapper ]]
then
	twrapper=/tmp/wrapper_$$								# do NOT add .ksh or ng_install will trash the #!
	echo "#!$NG_ROOT/bin/ksh" >$twrapper							# this MUST reference our link
	sed  "s!_ROOT_PATH_!$NG_ROOT!g" <$NG_ROOT/bin/ng_FIXUP_wrapper >>$twrapper		# add the correct root path, then install
	ng_install $twrapper $NG_ROOT/bin/ng_wrapper
	chmod 775 $NG_ROOT/bin/ng_wrapper			

	# (/ningaui/ng_wrapper) this is now officially deprecated and commented out 8/12/08
	#if [[ -d /ningaui ]]				# yes this is hard coded! only if sysadmin created this (backward compat)
	#then
	#	if [[ ! -e /ningaui/ng_wrapper ]]		# yes, this IS hard coded!
	#	then
	#		log_msg "created symlink /ningaui/ng_wrapper"
	#		ln -s $NG_ROOT/bin/ng_wrapper /ningaui/ng_wrapper
	#	else
	#		log_msg "symlink /ningaui/ng_wrapper existed"
	#	fi
	#fi

	rm -f /ningaui/ng_wrapper		# ditch old style; we now support only ~prodid/ng_wrapper
	
	if [[ ! -e ~${NG_PROD_ID:-ningaui}/ng_wrapper ]]
	then
		ln -s $NG_ROOT/bin/ng_wrapper ~${NG_PROD_ID:-ningaui}/ng_wrapper
		chown ${NG_PROD_ID:-ningaui}:${NG_PROD_ID:-ningaui} ~${NG_PROD_ID:-ningaui}/ng_wrapper
	fi

	rm -f $twrapper			# very important that we remove this file now; if we crash it could cause problems if left there
else
	log_msg "WARNING: could not find ng_FIXUP_wrapper to fix and install"
fi

if [[ -f $NG_ROOT/bin/ng_FIXUP_rc.d ]]
then
	trcd=/tmp/ng_rc.d.mod.$$
	sed "s!_ROOT_PATH_!$NG_ROOT!g; s/_PROD_ID_/${NG_PROD_ID:-ningaui}/g" <$NG_ROOT/bin/ng_FIXUP_rc.d >$trcd
	ng_install $trcd $NG_ROOT/bin/ng_rc.d

	rm -f $trcd
else
	log_msg "ERROR: could not find $NG_ROOT/bin/ng_FIXUP_rc.d to fix"
fi

if [[ ! -f $NG_ROOT/bin/ng_dosu ]]
then
	log_msg "ng_dosu is not installed [WARNING]"
	ng_log $PRI_WARN $argv0 "new version of ng_FIXUP_dosu needs to be installed"
else
	if [[ -f $NG_ROOT/bin/ng_FIXUP_dosu ]]
	then
		ng_dosu -? 2>&1|gre version | read dosu_vin
		ng_FIXUP_dosu -? 2>&1|gre version| read dosu_vnew
	
		if [[ $dosu_vin != $dosu_vnew ]]
		then
			log_msg "new version of ng_FIXUP_dosu needs to be installed [WARNING]"
			ng_log $PRI_ERROR $argv0 "new version of ng_FIXUP_dosu needs to be installed"
		else
			log_msg "ng_dosu version matches the latest installed FIXUP version [OK]"
		fi
	fi
fi

ls -al $NG_ROOT/bin/ng_dosu | read junk1 junk2 userid junk3
if [[ $userid != root ]]
then
		log_msg "root does NOT own ng_dosu and we expect it to [WARNING]"
		ng_log $PRI_ERROR $argv0 "root does NOT own ng_dosu and we expect it to"
else
		log_msg "ng_dosu is owned by root as expected [OK]"
fi
	

# -------------------------------- install ~ningaui things --------------------------------


if [[ -f $NG_ROOT/lib/ningaui_usr.tar ]]
then
	if cd ~${NG_PROD_ID:-ningaui}
	then
		tar -xf $NG_ROOT/lib/ningaui_usr.tar
		must_own 644 .profile 
		chmod 700 .ssh
		for x in .ssh/*
		do
			case $x in 
				*orig)	;;
				*) 	must_own 600 $x;;
			esac
		done
		log_msg "installed ningaui user things (profile, ssh, etc.)"
	else
		log_msg "WARNING: could not switch to ~${NG_PROD_ID:-ningaui} user directory; not updated with ningaui_usr info"
	fi
fi

# -------------------------------- setup misc things --------------------------------------
		
#ensure_f $site/refclock ${NG_PROD_ID:-ningaui} ${NG_PROD_ID:-ningaui} "echo ${REFCLOCK_HOST:-no-refclok-host-defined} >$site/refclock"


# -------------------------------- setup device files --------------------------------------
# these will fail during normal node stop/start, but at boot time this script should be 
# executed as root and these will be successful.
rm -f $TAPE/*.reserve
chgrp ${NG_PROD_ID:-ningaui} /dev/st? /dev/nst? /dev/sg? 2>/dev/null
chmod g+w /dev/st? /dev/nst? /dev/sg? 2>/dev/null
cluster=${NG_CLUSTER:-no_cluster_found}


# ------------------------------ local replication manager database --------------------------
if [[ -z $NG_RM_DB ]]           # ng_rm_db coredumps if this is not defined
then
        msg="NG_RM_DB was missing in the cartulary -- guessing at defaut value!"
	ng_log $PRI_WARN site_prep "$msg"
	log_msg "WARNING: $msg"
        export NG_RM_DB=$repmgr/db
fi
chmod 775 $repmgr $repmgr/*


# ----------------------------- final pinkpages environment tweeks ----------------------------
echo ""|RUBYOPT=rubygems ruby 2>/dev/null		# this fails if gems is not installed
if (( $? > 0 ))
then
	log_msg "ruby gems is not installed; clear RUBYOPT from pinkpages local"
	ng_ppset -l RUBYOPT
else
	log_msg "ruby gems is installed; set RUBYOPT as a pinkpages local var"
	ng_ppset -l RUBYOPT=rubygems
fi



# ------  final housekeeping -----------------------------

cleanup 0
exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_site_prep:Ensure that a node is properly prepared)

&space
&synop	ng_site_prep

&space
&desc	&This is driven by &lit(ng_init) as the Ningaui/potoroo environment is 
	started.  It is responsible for ensuring that the needed files and directories
	exist, have the appropriate permissions, 
	and in some cases are properly intitialised. The list of directories and 
	files that it verifies is too extensive to list here, but there are some 
	that should be noted:
&space
&beglist
&item	The list of replication manager filesystems ($NG_ROOT/fsxx) are discovered and the 
	&ital(arena) list in the lib directory is created based on what is found.
&space
&item	All mountpoint directories in &lit(NG_LOCAL_MTDIR) (/l by default) that begin with
	&lit(ng_) cause a symbolic link in &lit(NG_ROOT) to be created such that 
	&lit(NG_ROOT/xxxx) references &lit(/l/ng_xxxx.) This mechanism is used to 
	allow ningaui filesystems that are &stress(NOT) managed by ng_equerry to be 
	automatically established and verified. 
&space
&item 	$NG_ROOT/site/log
&space
&item 	$NG_ROOT/site/log/master
&space
&item 	$NG_ROOT/site/log/rota
&space
&item 	$NG_ROOT/site/narbalek
&space
&item 	$NG_ROOT/site/woomera
&space
&item 	$NG_ROOT/site/seneschal
&space
&item 	$NG_ROOT/lib
&space
&item 	$NG_ROOT/lib/pz
&space
&item 	$NG_ROOT/incoming (satellite only)
&endlist

&space

&space
&opts	Currently, &this accepts neither options nor parameters from the command line.

&space
&exit	An exit code of 0 indicates that the script was successful. Any non-zero exit code
	indicates a failure which should be investigated.

&space
&see	&seelink(tool:ng_init) &seelink(tool:ng_node) &seelink(tool:ng_rc.d) &seelink(tool:ng_setup_common) &seelink(repmgr:ng_rm_arena)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Nov 2001 (sd) : added stuff for ng_root/lib
&term	09 Jul 2002 (sd) : fixed propy stuff
&term	15 Jan 2003 (sd) : Added man page.
&term	01 Apr 2003 (sd) : added init of cartulary.local from cartulary.ningxx changed /ningaui/adm refs to $adm, added creation of den
&term	14 Apr 2003 (sd) : added ensure functions and cleaned a bit
&term	01 Jun 2003 (sd) : ensure that ng_root is set
&term	16 Jul 2003 (sd) : set the Seneschal Limit based on RM_FS
&term	02 Sep 2003 (sd) : Added ensure of $TMP
&term	12 Oct 2003 (sd) : Added ensure of adm/renumber/renumber.drv
&term	15 Jan 2004 (sd) : now ensures $NG_ROOT and /usr/bin/ng_wrapper (latter is installed each time)
&term	03 Feb 2004 (sd) : Added tape check which will work on FreeBSD
&term	25 Feb 2004 (sd) : Turns off ng_flist_send (goaway file) if no repmgr filesystems exist.
&term	11 May 2004 (sd) : ensures that private idahos (TMP/repmgr...) are there
&term	21 May 2004 (sd) : Sets a symlink for ng_rm_db to either _ndbm or _gdbm if ng_rm_db not in bin.
&term	30 Jun 2004 (sd) : Now builds cartulary.local only if it did not exist (assumes ng_init will build, but as setup
		uses site prep this check/build is necessary. Added Satellite section to set up things that are only
		needed on Satellite nodes, and added check to ensure that fsnn/hoard directories are there.
&term	02 Aug 2004 (sd) : Added update of ~ningaui from /usr/lib, did a bit of organisation and commenting.
&term	21 Aug 2004 (sd) : Now ensures that NG_ROOT in ng_wrapper is properly set.
&term	23 Aug 2004 (sd) : Fixed problem with ensuring *_html links.
&term	17 Sep 2004 (sd) : Now ensures narbalek directory exists.
&term	20 Sep 2004 (sd) : Narbalek oriented changes 
&term	15 Nov 2004 (sd) : Ensures that ng_wrapper is not executable if it is installed in /usr/bin
&term	18 Jan 2005 (sd) : Added nagios to site dir.
		Now purges cartulary.new files if they exist.
&term	24 Jan 2005 (sd) : Fixed setting ownership of profile and rhosts -- tar was returning error so we never did it.
&term	26 Jan 2005 (sd) : At boot this can run as root, and that was leaving bad ownership on .ssh files (fixed)
&term	12 Feb 2005 (sd) : Ensures that the tmp files created as rc.d and wrapper are fixed up are deleted. 
&term	25 Feb 2005 (sd) : Fixed typo in case that was causing .rhosts fixup to blow off.
&term	11 Mar 2005 (sd) : Now installs ng_wrapper only in $NG_ROOT/bin for security mandate. 
&term	26 Mar 2005 (sd) : Moves .rhosts filee away if it exists in ~ningaui.
&term	29 Apr 2005 (sd) : Deprecated www stuff.
&term	03 May 2005 (sd) : Slowly moving away from NG_ROOT/lib
&term	17 Jun 2005 (sd) : Delets rmsync files from $TMP if older than 2 days.
&term	30 Jun 2005 (sd) : Created symlink to ng_wrapper if not there.
&term	02 Aug 2005 (sd) : Refclock now lives in site.
&term	03 Aug 2005 (sd) : Corrected use of df to switch to ng_df.
&term	08 Aug 2005 (sd) : Added reference to site/forward.
&term	11 Aug 2005 (sd) : If the adm directory is there, it is quietly moved away.
&term	28 Oct 2005 (sd) : Now ensures TMP/seneschal is created.
&term	20 Dec 2005 (sd) : Fixed check of /ningaui/ng_wrapper.
&term	09 Feb 2005 (sd) : Now install rc.d (after fixup) into $NG_ROOT/bin as well as into /usr/bin. 
&term	07 Apr 2006 (sd) : No longer does anything with .rhosts. 
&term	12 Apr 2006 (sd) : Added support for creating the link from ng_rm_db to ng_rm_2dbd.
&term	17 May 2006 (sd) : Changed rm_db initialisation such that it tries to init a non-existant database only if 
		we are not using the new rm_dbd.
&term	20 Jun 2006 (sd) : ensures permissions are set corretly in ensure function.
&term	06 Sep 2006 (sd) : Fixed two ng_log messages that had buggered PRI_ references. (BT-853) 
&term	29 Sep 2006 (sd) : If REFCLOCK_HOST not defined, we echo a dummy string to the site/refclock file.
&term	11 Oct 2006 (sd) : No longer ensures that site/refclock is created.  The node_stat script uses the 
		$REFCLOCK_HOST from pinkpages directly.
&term	15 Nov 2006 (sd) : Corrected ng_log message to include the script name. 
&term	26 Mar 2007 (sd) : Added alert message if we think ng_dosu is out of date. 
&term	01 Jun 2007 (sd) : Attempts to set perms for tape and library media manager (robot) devices such that all have access.
&term	13 Jun 2007 (sd) : Now uses ng_rm_arena to set up the site/arena directory.
&term	02 Jul 2007 (sd) : Added a chmod to ensure write on for group for tape devices.
&term	25 Jul 2007 (sd) : We now build the site/config info using ng_scsi_map. Also ensures that $TMP has mode of 2777.
&term	02 Aug 2007 (sd) : Added -f option to rm_arena call to make it work when this is run as root.
&term	09 Aug 2007 (sd) : Added call to chkstate for linux nodes, and added a call to ng_scsi_map to search sg devices for tape.
&term	27 Aug 2007 (sd) : Fixed bug in ensure function that caused unexited loop.
&term	15 Oct 2007 (sd) : Modified to allow the use of NG_PROD_ID rather than to assume it is always ningaui.
&term	01 Nov 2007 (sd) : Now assume that gdbm/ndbm versions of ng_rm_dbd are deprecated. Ensures that the ng_rm_dbd checkpoint file is there.
&term	02 Nov 2007 (sd) : Turned off force option to rm_arena; no longer needed to run as root.
&term	01 Mar 2008 (sd) : Changed alert messages for dosu out of whack to error messages as spyglass is probing and sending alarms
		this eliminates duplicate spam. 
&term	28 Mar 2008 (sd) : Conversion for putting cartulary in NG_ROOT/site.
&term	28 Apr 2008 (sd) : Added ensure_m function to create NG_ROOT/ links to mountpoints in /l that are ng_*. 
&term	14 May 2008 (sd) : Changes to squelch some annoyingly harmless messages with regard to permissions. 
&term	05 Jun 2008 (sd) : Changed the system_local_mntd value to try to use NG_LOCAL_MTDIR with a default of /l.
&term	24 Jun 2008 (sd) : No longer create waldo directories -- these are manual.
&term	09 Jul 2008 (sd) : Added calls to find scsi devices (scsi_map) under FreeBSD. 
&term	29 Jul 2008 (sd) : Added setup of NG_ROOT/users and NG_ROOT/users/PROD_ID.
&term	12 Aug 2008 (sd) : Final deprecation for support of /ningaui/ng_wrapper.  We now only support ~$NG_PRODID/ng_wrapper.
&term	29 Sep 2008 (sd) : Changed call to scsi-map to search for nst* and nas* rather than sa*.
&term	05 Sep 2008 (sd) : site now allowed to be mode 775. 
&term	15 Oct 2008 (sd) : We now fix the prod-id setting in rc.d.
&term	28 Oct 2008 (sd) : Added a check to ensure that df does not report more than one filesystem mounted on the same mountpoint 
		directory; we abort if we see this (and yes we have seen this).
&term	25 Nov 2008 (sd) : Added rmfs filesystems to the must_own check list. Corrected ensure function to use NG_PROD_ID as default not ningaui.
&term	03 Mar 2009 (sd) : Added call to ng_set_wwpn. 
&term	13 Apr 2009 (sd) : Added call to ng_rm_arena pkinit to initialise the parking lots. 
&term	21 Aug 2009 (sd) : The pp var RUBYOPT is now set/cleared as a local var based on whether gems is installed. 
&term	14 Oct 2009 (sd) : Added call to ng_fixup to set flock related sim links in bin and lib directories.
&endterm

&scd_end
------------------------------------------------------------------ */
