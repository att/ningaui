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
#!/usr/common/ast/bin/ksh
# Mnemonic:	join_cluster
# Abstract:	will do what is needed for a node to join the named cluster
#		This script will NOT change default node attributes (if set),
#		prod id, broadcast network or logger port. 
#		
# Date:		29 Nov 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

# this is important as the node might not be started yet, and so we must validate this now 
if [[ ! -d ${NG_ROOT:-foo}/bin ]]
then
	echo "NG_ROOT variable not defined, or does not point to a legit directory: ${NG_ROOT:-not defined}"
	exit 1
fi

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file

# DONT suck in CARTULARY as it might be old and invalid.
if [[ -f $NG_ROOT/lib/cartulary.min ]]
then
	. $NG_ROOT/lib/cartulary.min		# DONT suck in the site/cart.min file as it might be OLD and invalid
fi

if [[ -z $TMP ]]
then
	export TMP=$NG_ROOT/tmp
fi

if [[ ! -d $TMP ]]				# must be legit before stfun can be pulle din
then
	abort "TMP is not legit; set, export and restart this programme"
fi

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# these functions may override some in stdfun
#log a message to our log every time and write it to stdout if verbose mode is on
function verbose
{

	echo "$(ng_dt -d) [$$] $@" >>$log
	if (( $verbose > 0 ))
	then
		echo "$(ng_dt -p) [$$] $@" >&2
	fi
}

# log to both places
function log_msg
{
	echo "$(ng_dt -d) [$$] $@" >>$log
	echo "$(ng_dt -p) [$$] $@" >&2
}

# log only to our log
function log_only
{
	echo "$(ng_dt -d) [$$] $@" >>$log
}

# something wrong; revert cartulary from cartulary.x if there
function abort
{
	log_msg "abort: $1 [FAIL]"
	if [[ -f $CARTULARY.x ]]
	then
		mv $CARTULARY.x $CARTULARY
	fi
	cleanup 2
}

#  remove everything but a select few from the environment
function XXXXwash_env
{
	# we keep only these from being washed away;  
	DRY_CLEAN_ONLY="^CVS_RSH=|^DISPLAY=|^HOME=|^LANG=|^DYLD_LIBRARY_PATH|^LD_LIBRARY_PATH=|^LOGNAME=|^MAIL=|^NG_ROOT=|^TMP=|^PATH=|^PS1=|^PWD=|^SHELL=|^TERM=|^USER="
	env | gre -v $DRY_CLEAN_ONLY | while read x
	do
		unset ${x%%=*}
	done
}

# ensure that any filesystems mounted in system_local_mntd (usually /l) are referenced by symlinks in NG_ROOT
function ensure_m
{
        typeset d="" 
        typeset lname=""
	typeset error=0

	log_msg "ensuring that links in $NG_ROOT to mountpoint directories exist"
        for d in $(ls -d ${system_local_mntd}/ng_* 2>/dev/null)
        do
                lname=${d#*/ng_}
                if [[ -L $NG_ROOT/$lname ]]             # expected and this is ok
                then
                        verbose "ensure_m: $NG_ROOT/$lname already exists: $(ls -dl $NG_ROOT/$lname) [OK]"
                else
                        if [[ -d $NG_ROOT/$lname ]]
                        then
                                rmdir $NG_ROOT/$lname 2>/dev/null        # need to remove if it is empty to allow for simple conversion from old style
                        fi

                        if [[ ! -e $NG_ROOT/$lname ]]                   # if not there, then make a link back to the local mount point
                        then
                                if ln -s $d $NG_ROOT/$lname
                                then
                                        verbose "ensure_m: created link: $NG_ROOT/$lname -> $d  [OK]"
                                else
                                        log_msg "ensure_m: unable to create link: $NG_ROOT/$lname -> $d  [FAIL]"
                                fi
                        else
                                msg="old style mountpoint under NG_ROOT exists: $NG_ROOT/$lname"
				if (( require_new_mount > 0 ))
				then
                                	log_msg "$msg [FAIL]"
					error=1
				else
                                	log_msg "$msg [WARN]"
				fi
                        fi
                fi
        done

	# check to see if anything is mounted in NG_ROOT (we'd catch it earlier only if /l/ng_xxx exists and NG_ROOT/xxx
	# is not a sym link to it.  This catches things on systems that have not been converted to /l/ng_xxxx -- they 
	# should be!
	if (( require_new_mount > 0 ))
	then
		ng_df | grep $NG_ROOT/ >/tmp/PID$$.df
		if [[  -s /tmp/PID$$.df ]]
		then
			log_msg "filesystems are mounted under NG_ROOT ($NG_ROOT); this is not allowed [FAIL]"
			cat /tmp/PID$$.df >>$log
			error=1
		fi
	fi

	return $error
}
# --------------------------------------------------------------------------------------------------------------

# --------------------------------------------------------------------------------------------------------------
# WARNING: it is NOT safe to use TMP until after we execute ensure_m function 
#	we set up vars that reference it now, but must be careful!
#
ustring="[-b] [-m system-mt-path | -M path] [-r NG_ROOT-path] [-s narbalek_host] [-v] cluster-name [narbalek-league-name]"

billabong=$TMP/billabong/join
verbose=0
me=$(ng_sysname -s)
boot=0
keep_rm_files=0
old_cluster=$NG_CLUSTER				# snag this now; before we switch it over
vflag=""
now="$(ng_dt -p)"
time_stamp="$(ng_dt -d)"
system_local_mntd=/l				# where we expect to find ng_* filesystems mounted
require_new_mount=1				# we require that things are mounted in /l (or someplace outside of NG_ROOT)
log=/tmp/PID$$.join_cluster.plog		# must write to pre log until we ensure that TMP points to the right place
attr_list=""					# default node attributes (-a)

rm -f $NG_ROOT/cartulary.x			# in case we abort, we want this gone if it existed before we ran

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	attr_list="$attr_list$2 "; shift;;
		-b)	boot=1;;
		-m)	system_local_mntd="$2"; shift;;
		-M)	require_new_mount=0;  system_local_mntd="$2"; shift;;		# same as -m, except we dont abort if there are old style mounts in NG_ROOT
		-r)	NG_ROOT=$2; shift;;
		-s)	nar_host=$2; shift;;
		-v)	vflag="-v"; verbose=1;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


log_only "join cluter execution starts"			# initially log messages to go tmp and are copied to TMP after ensure_m is executed

cluster="$1"
nar_league="$2"

if ! amIreally ${NG_PROD_ID:-ningaui}
then
	log_msg "must be executed as $NG_PROD_ID"
	cleanup 1
	exit 1			# take no chances here 
fi


if [[ -z $cluster ]]
then
	usage
	cleanup 1
fi

if [[ ! -d $NG_ROOT ]]
then
	log_msg "NG_ROOT directory does not exist; use -r on the command line to supply NG_ROOT path, or create $NG_ROOT"
	cleanup 1
fi

if ! ensure_m				# ensure that references to things like /l/ng_tmp and /l/ng_site are made
then
	mv $log /tmp/join_cluster.$time_stamp
	abort "unable to validate the sanity of mountpoints"
	cleanup 1
fi

# ------------------- do no 'real' work before here (must run ensure_m before anything real is done ----------------

missing_mnt=0
for d in $(ls -d $NG_ROOT)
do
	if [[ -f $d/MOUNT_POINT ]]
	then
		log_msg "filesystem seems not to be mounted for: $d"
		missing_mnt=1
	fi
done
if (( $missing_mnt > 0 ))
then
	abort "correct mount point issues and re-execute"
fi



if [[ ! -d $billabong ]]
then
	if ! mkdir -p $billabong/join 
	then
		abort "mkdir failed: $billabong/join"
	fi
fi

# ============= now safe to reference $TMP ==============================
plog=$log
log=$billabong/join.log			# now we can write to official log
cat $plog >>$log
rm -f $plog


if [[ $cluster == $NG_CLUSTER ]]
then
	ask_user -y -d no -l $log "this node seems to be a member of $cluster, continue?"|read answer
	if [[ $answer == "no" ]]
	then
		cleanup 1
	fi
fi


# search for files in remgr space; if found give option to keep them on node. if user opts not to keep
# they must clean them off and rerun this script. We want to do this EARLY so that we dont blow away
# things before they can clean
#
rfl=$billabong/rm_files.$time_stamp
find -L $(ls -d $NG_ROOT/fs[0-9][0-9] $NG_ROOT/rmfs[0-9][0-9][0-9][0-9] 2>/dev/null) -type f >$rfl 2>/dev/null 
wc -l <$rfl |read repmgr_files
if (( ${repmgr_files:-0} > 0 ))
then
	log_msg "WARNING: regular files still exist in the repmgr filesystems!"
	case $(ask_user -y -d no -l $log "keep files that are currently in repmgr space?" ) in 
		yes)
			log_only "repmgr files will be kept"
			keep_rm_files=1
			;;

		no)
			log_msg "files in repmgr filesystems must be deleted manually; delete them and rerun this script"
			log_msg "a list of files in repmgr space is in: $rfl"
			cleanup 1
			;;

		*)	abort "bad result from ask_user function";;
	esac
else
	verbose "there were no repmgr files detected on this node [OK]"
fi 

# clean tmp of nearly everything. should save billabong stuff
verbose "scrubbing TMP"
ng_tmp_clean -a 1 -A 1 -l join $TMP  >$billabong/tmp_clean.$time_stamp 2>&1



# must stop the node before purging cartulary so things like jobber and filer stuff are stopped if we owned them
if ng_wrapper -s
then
	verbose "stopping ningaui daemons..."
	ng_node stop >$billabong/node_stop.$time_stamp 2>&1
	verbose "node has been stopped"
else
	verbose "no green-light found; assuming ningaui processes are not running, no attempt to stop them"
fi


# compress and move off the old masterlog
(
	slf=$billabong/master.$time_stamp
	verbose "saving and compressing master log into: $slf.gz"
	cd $NG_ROOT/site/log
	mv  master $slf
	gzip $slf
	verbose "creating tgz of site/log directory"
	tar -cf - . |gzip >$billabong/site.log.$time_stamp.tgz
)


# move away old repmgr/rmdbd data
rtf=$billabong/site.rm.$time_stamp.tgz
(
	verbose "backing up repmgr information: $rtf"
	cd $NG_ROOT/site/rm
	tar -cf - . | gzip >$rtf
)

# remove everything in site
verbose "remoing all files and directories in NG_ROOT/site"
# must be careful as mac nodes have directory names with spaces (bloody wrong)
ls -d $NG_ROOT/site/* | while read x
do
	log_only "remove: $x"
	rm -fr $x >>$log 2>&1
done


# bring back rmdbd checkpoint files if user wants to keep repmgr files
if (( $keep_rm_files > 0  ))
then
	verbose "restoring repmgr dbd checkpoint files from $rtf"
	(
		cd $NG_ROOT/site/rm
		gzip -dc $rtf | tar -xf - ./rm_dbd.ckpt.old ./rm_dbd.ckpt		# pull back the two checkpoint files for dbd restart
	)
fi


verbose "setting NG_CLUSTER in pinkpages; using narbalek host: ${nar_host:-$cluster}"
if ! ng_rcmd ${nar_host:-$cluster} ng_ppset -c "'ng_join_cluster@$now'" -h ${me:-join_cluster_failed_host} NG_CLUSTER=$cluster
then
	abort "could not set NG_CLUSTER in pinkpages using host ${nar_host:-$cluster} for host: $me"
	cleanup 1
fi
ng_rcmd ${nar_host:-$cluster} ng_ppget -h ${me:-foo} NG_CLUSTER |  read setting
if [[ ${setting:-no-value} != $cluster ]]
then
	abort "unable to retrieve cluster value from pinkpages for ${me:-no host name} using host: ${nar_host:-$cluster}"
else
	log_msg "NG_CLUSTER in pinkpages/narbalek successfully changed [OK]"
fi

ng_rcmd ${nar_host:-$cluster} ng_ppget -f NG_ROOT | read default_root		# get the cluster default for ng_root
if [[ $NG_ROOT != $default_root ]]						# and if not the same as ours, bang ours into pinkpages
then
	if ! ng_rcmd ${nar_host:-$cluster} ng_ppset -c "'ng_join_cluster@$now'" -h ${me:-join_cluster_failed_host} NG_ROOT=$NG_ROOT
	then
		abort "could ont set NG_ROOT ($NG_ROOT) in pinkpages using host ${nar_host:-$cluster}"
		cleanup 1
	fi
fi

verbose "removing Isolate attribute for node in pinkpages; using narbalek host: ${nar_host:-$cluster}"
if ! ng_rcmd ${nar_host:-$cluster} ng_del_nattr -p -h ${me:-join_cluster_failed_host} Isolated
then
	log_msg "unable to remove Isolate node attribute, do this by hand [WARN]"
fi

if [[ -n $attr_list ]]
then
	if  ng_rcmd ${nar_host:-$cluster} ng_add_nattr -p -h ${me:-join_cluster_failed_host} $attr_list
	then
		log_msg "added attributes for node: $attr_list"
	else
		log_msg "unable to add attributes for node: $attr_list"
	fi
fi

verbose "allowing this node to be assigned masterlog hoods"
ng_rcmd ${nar_host:-$cluster} ng_log_sethoods -b $vflag -a ${me:-badname} >$billabong/sethoods.$time_stamp 2>&1


verbose "setting up the site/cartulary.min file"
mv $CARTULARY $billabong/cartulary.$time_stamp	# save in case we need a post mort
cp $NG_ROOT/lib/cartulary.min $CARTULARY	# must have someting to start with

site_min=$NG_ROOT/site/cartulary.min
if [[ -f $site_min ]]
then
	mcart=$billabong/site.cartulary.min.$time_stamp
	verbose "old site/cartulary.min saved in: $mcart [OK]"
	verbose "removing NG_ROOT, NG_NNSD_NODES, NG_NARBALEK_LEAGUE from $site_min [Ok]"
	mv $site_min $mcart
	gre -v "NG_CLUSTER|NG_NARBALEK_LEAGUE|NG_NNSD_NODES" $mcart >$site_min
fi

# caution: output from this subshell goes to site/cartulary.min
(
	echo "NG_ROOT=$NG_ROOT			# added by join cluster @ $now"
	echo "export NG_CLUSTER=$cluster 	# added by join cluster @ $now" 



	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_NARBALEK_LEAGUE | read nar_league_default
	if [[ -n ${nar_league:-$nar_league_default} ]]					# it is legit for both to be blank
	then
		verbose "narbalek league set to: ${nar_league:-$nar_league_default}"
		echo "export NG_NARBALEK_LEAGUE=${nar_league:-$nar_league_default} 	# added by join cluster @ $now" 
	fi
	
	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_NNSD_NODES | read nnsd_nodes
	if [[ -n $nnsd_nodes ]]
	then
		echo "export NG_NNSD_NODES=\"$nnsd_nodes\" 	# added by join cluster @ $now" 
	fi

	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_DEF_NATTR | read def_attr_list
	if [[ -n $def_attr_list ]]
	then
		echo "export NG_DEF_NATTR=\"$def_attr_list\"	# added by join cluster @$now"
	fi

	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_LOGGER_PORT | read value
	if [[ -n $value ]]
	then
		echo "export NG_LOGGER_PORT=\"$value\"	# added by join cluster @$now"
	fi

	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_BROADCAST_NET | read value
	if [[ -n $value ]]
	then
		echo "export NG_BROADCAST_NET=\"$value\"	# added by join cluster @$now"
	fi

	ng_rcmd ${nar_host:-$cluster} ng_ppget NG_PROD_ID | read value
	if [[ -n $value ]]
	then
		echo "export NG_PROD_ID=\"$value\"	# added by join cluster @$now"
	fi
) >>$site_min

if [[ -f $site_min ]]		# should be there, but why take chances. pick up site specific stuff if there
then
	cat $site_min >>$CARTULARY
fi

wash_env			# scrub all but really needed things and suck a fresh set of minimal values in
. $CARTULARY

verbose "running site_prep to reconstruct the node"
ng_site_prep -v >$billabong/site_prep.$time_stamp 2>&1

log_msg "removing .nox (no execution) files for rota, flist_rcv, ng_init and flist_send [OK]"
ng_goaway -r -X ng_init ng_rota ng_flist_send ng_flist_rcv ng_init

ls $TMP/*.nox 2>/dev/null |wc -l|read n
if (( $n > 0 ))
then
	log_msg "----------------------------------------------------------------------"
	log_msg "WARNING:"
	log_msg "There are currently $n goaway no execute (nox) files in TMP "
	log_msg "these will prevent some potoroo functions from executing. "
	log_msg "Use ng_goaway -l to get a list and ng_goway -r to  remove goaway files."
	log_msg "----------------------------------------------------------------------"
fi

if (( $boot > 0 ))
then
	verbose "starting node daemons"
	ng_init

else
	verbose "boot flag (-b) not set; node not started"
fi

ng_log $PRI_INFO $argv0 "joined cluster: $cluster"


cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_join_cluster:Forces the node to join the named cluster)

&space
&synop	ng_join_cluster [-a attr-list] [-m | -M system-mt-path]  [-r NG_ROOT-path] [-s narbalek-host] [-v] cluster-name [narbalek-league-name]

&space
&desc	&This causes the node to restart joining a new cluster.  The assumption that the 
	node is &stress(not) the first to join the cluster.  If the node is being used
	to seed a new cluster, then ng_setup must be used. 
	
&space
	The following tasks are performed when joining a new cluster:
&space
&beglist
&item	The ningaui daemons on the node are stopped (ng_node stop).
&space
&term	Repmgr and log data is backed up and put into $TMP/billabong/join.	
&space
&item	The TMP and NG_ROOT/site directories are scrubbed of nearly all information.
&space
&item	The NG_CLUSTER environment variable is set in pinkpages. 
&space
&item	The Isolate node attribute is removed (if the node was removed from another cluster).
&space
&item	The cartulary is replaced with the default minimal cartualry from &lit($NG_ROOT/lib/cartulary.min).
	The minimal cartulary in NG_ROOT/site is edited to set the new default values which include:
	&lit(NG_ROOT, NG_NNSD_NODES,) and &lit(NG_NARBALEK_LEAGUE.)
&space
&item	The master log file is moved and compressed.
&space
&term	If the boot flag is set on the command line (-b) the node is restarted (ng_node start). 
&endlist
&space

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a attrs : The attribute(s) that this node will have by default. Examples are Satellite, or Relay.
	If more than one attribute is supplied, they must be supplied with multiple -a options, or as a 
	single list of space separated tokens (quoted0.  
	The list is not validated; service names (e.g. Filer) and O/S types (e.g. FreeBSD) should NOT be given 
	as these attributes are automatically detected at node start. 

&space
&term	-b : Boot the node.  The ningaui daemons will be restarted after the cluster affiliation
	has been changed. Before booting, the goaway files that would have been set by ng_isolate
	are removed. 
&space
&term	-m path : Supplies the system mount directory path.  Assumed to be &lit(/l) if not supplied.
	Ningaui applications expect that all filesystems mounted to this path with the name 
	&lit(ng_*) or &lit(rmfs*) are to have symbolic links created in NG_ROOT which point to 
	the filesystem. The names in NG_ROOT will not have the &lit(ng_) prefix (e.g. /l/ng_site
	will be referenced by the sym link NG_ROOT/site.)
&space
&term	-M path : Same as -m, except &this will not abort if it finds any filesystems or directories
	mounted in NG_ROOT.
&space
&term	-r path : Provides the path of NG_ROOT on the local node. If not supplied, &lit(/ningaui) is 
	assumed. 
&space
&term	-s host : Supplies the name of the host where the narbalek service is running and can 
	be used to set the cluster affiliation in pinkpages. If omitted, the host that answers
	on the cluster name DNS entry is assumed to be up and running and the narbalek on that
	node can be used.  If this is the first node in the clsuter, and there are not any 
	narbalek processes running, ng_setup must be used to set the node and affiliate it
	with a cluster. 
&space
&term	-v : Verbose mode.
&endterms

&parms	These position dependant parameters are expected on the command line:
&begterms
&term	cluster : The name of the cluster that this node will join. This parameter is required.
&space
&term	narbalek-league-name : This is the name of the narbalek league that the node will use 
	when its narbalek attempts to join a communications tree. The parameter is optional and 
	will default to the value of NG_NARBALEK_LEAGUE as defined known by the node that 
	answers to the cluster DNS name, or the node supplied by the &lit(-s) option. This 
	parameter allows the user to override the league name. 
&endterms

&space
&exit	Any non-zero return code inidcates failure.

&see
	&seelink(tool:ng_isolate) &seelink(tool:ng_node) &seelink(tool:ng_init) &seelink(tool:ng_setup)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	29 Nov 2003 (sd) : Thin end of the wedge.
&term	17 May 2004 (sd) : Avoided use of ppget.
&term	19 Oct 2004 (sd) : does not use leader any longer.
&term	19 Nov 2004 (sd) : Now warns if there are no execute goaway files in TMP.
		Corrected man page.
&term	02 Aug 2004 (sd) : Remove reference to */adm.
&term	20 Dec 2005 (sd) : Reworked to make it work with narbalek and to stop/start 
	daemons as necessary.
&term	22 Mar 2006 (sd) : Now removes goaway files if -b option is used. 
&term	31 mar 2006 (sd) : Changed NG_ROOT check to fail if NG_ROOT/bin is not present since 
		we now mandate that /ningaui is always there.
&term	18 Jul 2006 (sd) : Now removes nox file for ng_init -- it could be set if ng_isolate
	is run on a cloned node. 
&term	26 Sep 2006 (sd) : Pulled -n option as it is not needed.
&term	11 May 2007 (sd) : Now removes the Isolate node attribute.
&term	04 Oct 2007 (sd) : Fixed reset of Isolated attribute.
&term	23 Jun 2008 (sd) : Added support to set narbalek league name in site cartulary.min file.
&term	24 Jun 2008 (sd) : Now removes ng_init .nox file too.
&term	10 Jul 2008 (sd) : BIG changes to clean TMP and NG_ROOT/site as well as to prompt user to 
		keep repmgr files if we detect them.
&term	22 Jul 2008 (sd) : Nox files are now removed every time; wash_env used from stdfun. Also requires the user to be the prod id.
&term	25 Aug 2008 (sd) : We now ensure that the symlinks to mount point directories in /l are set.
&term	04 Dec 2008 (sd) : Forced nnsd_nodes setting in the site/cartulary.min to have a quoted value.
&term	16 Feb 2009 (sd) : Added mkdir for billabong/join.
&term	07 Apr 2009 (sd) : Corrected problem where write attempts to the log in billabong/join were happening before the directory was created if missing.
		Corrected problem with searching for repmgr files; in the new world, where NG_ROOT/fs00 is a symlink, find needs -L to chase after it 
		rather than stopping short.
&term	15 Apr 2009 (sd) : Added -a option allowing attributest to be supplied. 
&term	15 Sep 2009 (sd) : Fixed two typos that were buggering execution, and added the capture of three more 
		pinkpages variables into the minimal cartulary.
&endterms

&scd_end
------------------------------------------------------------------ */
