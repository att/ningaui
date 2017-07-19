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
# Mnemonic:	publish.ksh
# Abstract:	publish a package to the current cluster or send to 
#		another cluster. A package is published by placing it
#		into replication manager space with a repcount of -1 (all).
#		if the cluster (-c) option is supplied, then the package
#		is located in repmgr space on the current cluster, and 
#		flock coppied to the remote cluster.
# Opts:		-c cluster (package must be registered on this cluster)
# Parms:	package name(s) (pkg-potoroo...)
# Date:		06 Feb 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------
hold_src=$NG_SRC				# possible trash in cartulary

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
NG_SRC=$hold_src

function usage
{
	cat <<endKat

	usage:	$argv0  package-filename  		# publish and register on local cluster
		$argv0 [-n] [-l] [-t os-ver] -p -c cluster pattern	# sends all packages matching pattern to cluster
endKat
}


	# local installation of the file into repmgr space
function do_local
{
	typeset file=$1
	typeset rname=`ng_spaceman_nm $file`
	typeset tries=0

	if [[ ! -r ${file:-foojunk} ]]
	then
		log_msg "do_local: cannot find/read file: ${file:-no file given}"
		errors=$(( $errors + 1 ))
		return 1
	fi

	verbose 'unregistering any previous version first; and sleeping a bit'
	$forreal ng_rm_register -u $vflag $file	
	$forreal sleep 30

	verbose "registering file on local cluster: $rname"


	if ! $forreal cp $file $rname
	then
		log_msg "do_local: unable to copy $file to $rname"
		errors=$(( $errors + 1 ))
		return 1
	fi

	rname=`publish_file -e $rname`		# strip trailing ~
	while (( $tries < 10 ))
	do
		tries=$(( $tries + 1 ))

		if  $forreal ng_rm_register $vflag -e $rname
		then
			verbose "registration with repmgr success ($tries tries): $rname"
			return 0
		fi

		
		log_msg "do_local: WARNING: error registering file on local cluster; waiting then trying again"
		sleep $register_delay			# repmgr could be bouncing
	done

	register_delay=1		# if we waited 20 minutes for the first, then probably big problems. set down to avoid blocking longer
	log_msg "do_local: error registering file on local cluster: $rname"
	errors=$(( $errors + 1 ))
	return 1
}

# push the file(s) to which ever remote cluster(s) have a node that looks like ours
function do_push
{
	typeset file=""
	typeset bad=0
	typeset their_cluster=""
	typeset node=""
	typeset junk=""
	typeset md5=""

		# find a readable copy of each file
	for file in $*
	do
		if [[ ! -r $file ]]
		then
			verbose "sussing file ($file) from repmgr space" 
			ng_rm_where -n -p ${file##*/} |grep $mynode| read node file junk
		fi

		if [[ -r ${file:-foofile} ]]
		then
			verbose "found readable copy of  file: $file"
			if md5sum $file |read md5 junk
			then
				echo $file $md5 >>$TMP/publish.flist.$$
			else
				log_msg "ERROR: md5sum failed for: ${file:-not found} [FAILED]"
				bad=1
			fi
		else
			log_msg "ERROR: cannot find/read file: ${file:-not found} [FAILED]"
			bad=1
		fi
	done

	if [[ ! -s $TMP/publish.flist.$$ ]]
	then
		log_msg "could not find readable copies of any files: $*"
		return 1
	fi

	verbose "finding targets that match ${type:-this node type}: searching: $clusters"
	ng_get_likeme $tflag $type -C  $vflag $clusters   >$TMP/publish.nodes.$$

	if [[ ! -s $TMP/publish.nodes.$$ ]]
	then
		log_msg "no nodes found matching type: $type"		# we assume this is ok
		return 0
	fi
			
	while read node			# for each node that we found like ours	
	do
		ng_rcmd $node ng_wrapper -e NG_CLUSTER| read their_cluster

		if [[ -n $their_cluster && $their_cluster != $NG_CLUSTER ]]	# we assume local would have registred it here; skip if us
		then
			while read file md5 junk			# for each good file
			do
				verbose "flock copy to $their_cluster: $file $md5"
				if  $forreal ng_fcp $vflag  -C -1 -c $their_cluster $file $md5
				then
					msg="pushing package to $their_cluster successful for: $file"
					log_msg "$msg [OK]"
					$forreal ng_log $PRI_INFO $argv0 "$msg"
				else
					msg="flock copy to $their_cluster failed for: $file"
					log_msg "$msg  [FAILED]"
					$forreal ng_log $PRI_INFO $argv0 "$msg"
					bad=1
				fi
			done <$TMP/publish.flist.$$
		else
			verbose "no files sent to cluster ${their_cluster:-(could not get cluster for $node)} (this cluster maybe)"
		fi
	done <$TMP/publish.nodes.$$

	return $bad
}


# ---------------------------------------------------------------------
ustring="[-c cluster-name] [-l] [-p] [-n] [-t minor-ver] package-filename"

repmgrv=""
push=0			# publish to all clusters with a like node
local=1;
forreal=""
vflag=""
type=""			# for get like me call
tflag=""		# for get like me call
register_delay=120	# delay between registration attempts

while [[ $1 = -* ]]
do
	case $1 in
		-c)	clusters="$clusters-c ${2// / -c } "; shift;;		# convert to parameter(s) for likeme
		-l)	local=0;;			# dont add to this clusters repmgr space
		-p)	push=1;;			# push to all clusters with like node
		-n)	forreal="echo would:";;
		-t)	tflag="-t"; type="$2"; shift;;		# allow override of type 4.8 maps to 4.9 for instance

		-v)	verbose=1; vflag="-v";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

mynode=`ng_sysname`
if [[ -z $1 ]]
then
	log_msg "usage $0 pkg-file"
	cleanup 1
fi

errors=0

verbose "publish:  starting work for $1  (local=$local push=$push)"

if [[ $local -gt 0 ]]				# add to repmgr on local node
then
	verbose "adding package(s) to local repmgr space: $*"
	for file in $*
	do
		if do_local $file
		then
			msg="local package publication successful: $file"
			log_msg "$msg [OK]"
			$forreal ng_log $PRI_INFO $argv0 "$msg"
		else
			errors=$(( $errors + 1 ))
			msg="local package publication error: $file"
			log_msg "$msg [FAILED]"
			$forreal ng_log $PRI_ERROR $argv0 "$msg"
		fi

	done
fi

if [[ $push -gt 0 ]]				# push to other cluster(s)
then
	do_push $*
	errors=$?
fi

cleanup $errors

exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_publish:Publish a ningaui package)

&space
&synop	ng_sm_publish [-c cluster] [-l] [-p] [-n] [-t]  package-filename [package-filename...]

&space
&desc	&This publishes a 'package file' by copying the file to replication 
	manger file space on the local cluster, and optionally pushing the package
	to any cluster with a node that is of the same operating system type as 
	the node running &this. When a package is registered with replicaiton manager
	the replication count is set to all nodes in order to ensure that the package
	is available across the cluster. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : The name of the cluster(s) to send the package to. If not supplied, 
	the package is sent to all known clusters which have a node that matches the 
	o/s type running &this.
&space
&term	-l : Do &stress(NOT) register the package with the local replication manager. 
&space
&term	-p : Push the package to clusters.  
&space
&term	-n : No execution mode.  &This will attempt to explain what it would do if this 
	flag had not been set. 
&space
&term	-t type : Override the operating system type.  This type will be used to push
	the package to other clusters looking for nodes with the indicated type.
	Types are strings like: &lit(FreeBSD5.2).
&endterms


&space
&parms	&This expects the name of at least one package to be supplied on the command 
	line.  When the push option is set, and multiple package names are supplied on the 
	command line, &this will attempt to push all packages regardless of the 
	success or failure of the others.  

&space
&exit	Any exit code other than zero indicates an error.

&space
&see	ng_rm_register, ng_sm_build, ng_sm_autobuild, ng_get_likeme 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Feb 2004 (sd) : Fresh from the oven.
&term	16 Jan 2005 (sd) : Major revisions to support autobuild and publication of packages. 
&term	26 Jan 2005 (sd) : Changed to speed up publication to different cluster(s) in an effort
		to have all packages arrive before the next pkg_inst is kicked off.
&term	22 Jun 2005 (sd) : Changed to publish the file before registration so we can capture the 
	name change.
&term	03 Jul 2005 (sd) : Fixed call to publish_file that omitted the -e option to get the new 
	file name back.
&term	03 Oct 2005 (sd) : Checks for file when doing a local register before unregistering the 
	existing one. 
&term	20 Jul 2007 (sd) : Fixed usage message.
&term	01 Oct 2007 (sd) : Tweeked man page.
&term	22 May 2008 (sd) : Changed verbose diagnostic to be more clear.
&term	08 Sep 2009 (sd) : Added missing things to usage. 
&endterms

&scd_end
------------------------------------------------------------------ */
