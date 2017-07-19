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
# Mnemonic:	shoveit	
# Abstract:	install a file on various nodes round the cluster
#		
# Date:		Original -- long ago, conversion to ningaui: 27 Jan 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# daughter of son of shove it to work as a prodcuction script
# script to push a file to various hosts and then install the file
# shoveit.ksh [-c cluster] [-t machinetype] filename [installname]
# shoveit.ksh filename installname host...

function chk_nodes
{
	if [[ -z ${nodes// /} ]]		# if missing, or blank, then error 
	then
		log_msg "could not determine a node list from $cluster(cluster) $mtype(type)"
		cleanup 101
	fi
}

# ------------------------------------------------------------------------------
ustring="[-c cluster] [-t os-type|-f|-l|-m|-T] [-n] [-p permissions] [-v] source-file {destile|-} [nodes]"
verbose=0
nodes=""
forreal=1
mynode=`ng_sysname`
do_myself=0
perms=""					# perms to pass with -m (mode) flag on ng_install call
generic_flag=""


# this MUST match logic in ng_get_nattr
#uname -s  | read osname junk			# os name
#uname -r | IFS="." read maj min junk		# version
#mytype=$osname$maj.${min%%-*}-$arch		# freebsd has m.n-type rather than m.n[.p]

#my_uname=`whence uname`				# buggered up built-in in sh or ksh under suse
my_uname=ng_uname
ng_get_nattr | read mytype
$my_uname -p | read arch				# h/w type
mytype="${mytype##* }-$arch"			# last one is guarenteed to be osname.ver.ver-arch

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	do_myself=1;;			# include this node too
		-c)	cluster="$cluster$2 "; shift;;		
		-g)	generic_flag="-g";;

		#-f)	mtype=FreeBSD;;			# force types
		#-m)	mtype=Darwin;;
		#-l)	mtype=Linux;;
		-t)	mtype="$2"; 
			if [[ $mtype != *-* ]]
			then
				generic_flag="-g"		# set generic flag for like me call
			fi
			shift
			;;

		-T)	mtype=$mytype;;				# just to machines of same type

		-n)	forreal=0;;
		-p)	perms="-m $2"; shift;;			# set the filemode option on ng_instlall commands

		-v)	vflag="$vflag -v"; verbose=1;;
		--man)	show_man; cleanup 101;;
		-\?)	usage; cleanup 101;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 101
			;;
	esac

	shift
done

if [[ -n  $generic_flag ]]
then
	mtype=${mtype%-*}
fi

if [[ $# -lt 3 && -z $cluster ]]	# we assume file [des] are only things and default to local cluster
then
	verbose "defaulting to local cluster -- nodes not supplied on command line"
	cluster=$NG_CLUSTER
fi

if [[ $cluster = "flock " ]]
then
	flock_members="$( ng_nar_map -c )"		# get a list of clusters from narbalek

	for x in $flock_members
	do
		members="$members${x%%:*} "		# old FLOCK_MEMBERS had cluster:node; strip off :*
	done

	cluster="$members"
fi

if [[ -n $cluster ]]			# get a list of nodes, based on type
then
	for c in $cluster
	do
		if [[ -z $mtype ]]		# no type specified, get all members
		then
			verbose "determining which nodes are on $c"
			#ng_rcmd $c ng_members -r 2>/dev/null |read n
			ng_rcmd $c ng_members ${vflag%-v} -c $c -N  |read n		# now using narbalek -- only those that respond
			if [[ -z $n ]]	
			then
				log_msg "WARNING: no nodes reported for cluster/type: $c "
			fi
			nodes="$nodes$n "
		else
			verbose "determining which nodes on $c and are type $mtype"
			#ng_rcmd $c ng_species $mtype 2>/dev/null |read n		# just get cluster members of indicated type
			#ng_species -c $c $mtype | read n				# species now uses narbalek 
			ng_get_likeme -c $c -t $mtype ${vflag%-v} -s $generic_flag -c $c |read n
			if [[ -z $n ]]	
			then
				log_msg "WARNING: no nodes reported for cluster/type: $c/$mtype"
			fi
			nodes="$nodes$n "
		fi
	done

	chk_nodes 
else
	if [[ -n $mtype ]]			# machine type w/o cluster -- assume our cluster
	then
		verbose "sussing nodes on this cluster ($NG_CLUSTER) that are $mtype"
		#ng_species $mtype |read nodes
		ng_get_likeme ${vflag%-v} -s $generic_flag -c $NG_CLUSTER |read nodes
		chk_nodes
	fi					# no matches, we will assume node list is on command line (later)
fi

wrk=$HOME/wrk
if [[ ! -d $wrk ]]
then
	verbose "creating $wrk as a working directory"
	if ! mkdir $wrk 2>/dev/null			# ensure it is there
	then
		log_msg "cannot make a working directory: $HOME/wrk"
		cleanup 101
	fi
fi


if [[ -z $1 ]]		# must have at least src filename
then
	log_msg "too few positional parameters:"
	usage
	cleanup 101
fi

fn=$1				# src file
shift

# if they supply as dest file:
#	nothing:	we match the directory path from the source and subs $NG_ROOT if it matches
#	filename:	same as nothing, changing the file name from the source if dest filename differs
#	dir/[file]	we tack dir path on and ng_install resolves fn (uses source)
#	/path/fn	we use their path as is (right or wrong)
#	-		we use their source path and filename, if their path matches $NG_ROOT we substitute

if [[ -z $1 ]]			# assume clustername given allowing - or dest to be omitted
then
	out=${fn%/*}/		# install into the matching directory name
	eval out=\${out#$NG_ROOT/}		# if lead path was NG_ROOT, strip off to add dest's ng_root later

else
	if [[ $1 = "-" ]]		# use matching name from src file
	then
		out=${fn%/*}		# install into the matching directory name
	eval out=\${out#$NG_ROOT/}		# if lead path was NG_ROOT, strip off to add dest's ng_root later
	else
		out=$1			# whatever they gave us; if it starts / then we leave it unchanged
	fi
	shift
fi

if [[ $fn != $NG_ROOT/* ]]
then
	verbose "source is not inside of NG_ROOT; will include this node"
	do_myself=1
fi

if ! cp $fn $wrk
then
	log_msg "cannot copy the input file ($1) to $wrk: $fn"
	cleanup 101
fi
in=$wrk/${fn##*/}		# filename to give to ng_install


if [[ -n $nodes ]]		# if nodes built set to appear as if on cmd line; else use cmd line parms
then
	set $nodes
fi

if [[ $forreal -lt 1 ]]
then
	log_msg "would run: ng_install $in $out"
	while [[ -n $1 ]]
	do
		if [[ $1 != $mynode ]] || (( $do_myself > 0 ))
		then
			log_msg "would send to: $1"
		else
			log_msg "skipping myself"
		fi
		shift
	done

	rm $in
	cleanup 0
fi

nfs_marker=$wrk/nfsmarker.$$
touch $nfs_marker		# lets us check for nfs filesystem on the distant machine
inbn=${in##*/}			# base name of input/install file incase not nfs mounted

errors=0
while [[ -n $1 ]]		# install on each node
do
	if [[ $do_myself -lt 1 && $1 == $mynode ]]
	then
		verbose "skipping myself ($1)"
		shift
		continue
	fi


	verbose "doing $1"
	#ng_rcmd $1 'ng_wrapper eval echo \$NG_ROOT'|read dist_root		#their ng_root
	#dist_in=$dist_root/tmp/$inbn
	ng_rcmd $1 'ng_wrapper -e NG_ROOT' | read dist_root		# the root on the dest node
	ng_rcmd $1 'ng_wrapper -e TMP' | read dist_tmp			# get their TMP 
	dist_in="$dist_tmp/$inbn"

	if [[ $out != /* ]]			# default install directory is ningaui bin
	then
		dist_out="$dist_root/$out"
	else
		dist_out="$out"
	fi

	ng_rcmd $1 "ls $nfs_marker "'2>/dev/null'|read rmarker
	if [[ ${rmarker##*/} != ${nfs_marker##*/} ]]		# no nfs directory on $1
	then
		#verbose "$1 seems not to have an NFS mounted $HOME"

		verbose "ccp source to: $1:$dist_in; install in $dist_out"

		if ng_ccp -d $dist_in $1 $in 
		then
			#log_msg "after install: `ng_rcmd -f $1 ng_install $perms $dist_in $dist_out`"
			if ng_rcmd -f $1 ng_install $perms $dist_in $dist_out >/tmp/x.$$
			then
				log_msg "after install: `cat /tmp/x.$$`"
			else
				log_msg "install of $dist_in -> $dist_out on $1 FAILED."
				errors=$(( $errors + 1 ))
			fi

				
			#echo "ng_rcmd -f $1 ng_install $dist_in $dist_out"
			ng_rcmd $1 "rm $dist_in"
		else
			log_msg "unable to copy $in to $dist_out on $1"
		fi
	
	else									# just install from nfs mounted home directory
		verbose "install in $dist_out"
		#log_msg "after install: `ng_rcmd -f $1 ng_install $in $dist_out`"
		if ng_rcmd -f $1 ng_install $perms $in $dist_out >/tmp/x.$$
		then
			log_msg "after install: `cat /tmp/x.$$`"
		else
			cat /tmp/x.$$
			log_msg "install of $in -> $dist_out  on $1 FAILED."
			errors=$(( $errors + 1 ))
		fi

		#echo "ng_rcmd -f $1 ng_install $in $dist_out"
	fi

	shift
done

rm -f $in $nfs_marker				# trash tmp file created in users work directory
cleanup $errors





exit
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_shoveit:Install a file on multiple cluster nodes)

&space

&synop	
.nf
	ng_shoveit -c cluster [-t machine-type|-f|-m|-l|-T]  [-n] [-p permissions] [-v] sourcefile [destfile|-]
	ng_shoveit {-t machine-type|-f|-m|-l|-T}  [-n] [-v] sourcefile [destfile|-]
	ng_shoveit [-n] [-v] sourcefile {destfile|-} node [node...]
.fo

&space
&desc	&This will install the &ital(sourcefile) on the indicated nodes using &lit(ng_install).
	If &ital(nodes) are not supplied on the command line, then either a cluster name (-c) or a 
	node type (-t) must be supplied. If machine type 
	is supplied, and cluster name is omitted, then the 'current cluster' ($NG_CLUSTER) is assumed.

&space
&subcat Destination File
	The destination file can be specified using several different formats. If specified as 
	a fully quallified path name then that is used, unchanged, as the target file on each node where the 
	installation is attempted.  If the destination file is given as &lit(path/file,) without 
	a leading slant, &this assumes that the path and filename should be appended to the &lit(NG_ROOT) 
	as it is defined on the remote node (e.g. lib/foo will be installed as $NG_ROOT/lib/foo).
	If a dash (-) is given, or omitted (if no nodes are supplied as trailing parameters), then 
	the source pathname is used, however the portion of the source pathname that matches the 
	local value for $(NG_ROOT) is replaced with the value as defined on the remote system.

&space
&subcat	Cluster
	If the cluster name is not supplied on the command line (-c), then it is assumed that either 
	a list of nodes has been supplied as postitonal parameters, or that the node type attributes
	have been supplied (-t).  When supplying just type attributes, and omitting the cluster 
	name option, the current cluter (NG_CLUSTER) is used.  The nodes that belong to a given 
	cluster are determined by sending an &lit(ng_members) request to the network interface node
	of the cluster.  Nodes that are listed are nodes that have replication manager filesystems.
	The cluster name must be the formal name rather than any slang name: numbat rather than bat.

&space
	The cluster name given with the &lit(-c) option can be the special cluster name &lit(flock).
	This will cause all clusters listed by the cartulary varaible &lit(FLOCK_MEMBERS) to be 
	searched for members that match the target node type constraints, or for all members if 
	no member type is indicated.  Multiple &lit(-c cluster) options can be supplied to 
	send to a subset of the flock with one command. 
	
&space
&subcat Machine Type
	If machine type is given (-t),  each node on the cluster is queried to determine its 
	type, and the file is sent only to nodes whose type matches that specified by the  type 
	option.  
	The type supplied may be any attribute that is currently reported by a node.
	The &lit(-T) option indicates that all nodes having the same operating system version 
	and hardware architecture as the node  running &this should be 
	targeted.   
&space
	A not-type can be supplied as machine type in the form of &lit(!string). This makes it 
	possible to install a file on all nodes that do not have a given type (e.g. !Satellite).
	It is also possible to supply multiple types if they are quoted and space separated. If 
	multiple types are supplied, the list is interpreted as an ORed list such that 
	if "Linux FreeBSD" were supplied, all machines of either type would be shoved to.  This 
	interpretation is a function of &lit(ng_species), and can be converted to 'all types' 
	if a -a is used.  Because the type list is passed to &lit(ng_species), the -a must 
	be inside of the quoted type list, and be the first non-blank character: "-a Linux FreeBSD".
	
&space
&subcat Nodes Without NFS Home Directories
	&This first attempts to use the user's NFS mounted home directory ($HOME) to avoid having 
	to copy the source file across the network.  If the user's directory appears not to be 
	mounted and accessable from the remote machine, then &this will send the file to 
	the target node and install from a temporary file on the destination node. In order to 
	avoid collisions in the user's home directory, &this assumes that it may create a 
	working directory ($HOME/wrk), or that one exists that it may use. &This will remove any
	file that it creates in the work directory, but will not delete the directory. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Do all nodes, including this node.  Overrides the skip this node check even if
	the file being shoved is in NG_ROOT.
&space
&term	-c cluster : Install on nodes in this cluster. &ital(Cluster) must be the full name; e.g. &lit(numbat)
	as opposed to the popular reference of &lit(bat). If there is not a node on the cluster that 
	responds to TCP/IP requests sent to it's cluster name, then a node on that cluster can 
	be supplied, though this is not recommended. The special cluster name 'flock' can be supplied
	to indicate that all qualifiing nodes on all clusters should be targeted.  The clusters
	used are thoses listed by the cartulary variable &lit(FLOCK_MEMBERS).
&space
&term	-f : Install only on FreeBSD nodes; limited by the cluster selection option.
	Care must be taken when using this option as the operating sytem major and minor
	revsions numbers are not taken into account and could cause binary files to be 
	shoved to an incompatable node.
&space
&term	-g : Use generic O/S type.  The operating system architecture is not used
	when determining target nodes based on type when the -T option is supplied.
	If the -t option is used to supply a generic type (FreeBSD4.8 or FreeBSD rather than FreeBSD4.8-i386)
	this  option is assumed and need not be supplied. 
&space
&term	-l : Install only on Linux nodes; limited by the cluster selection option.
	Care must be taken when using this option as the operating sytem major and minor
	revsions numbers are not taken into account and could cause binary files to be 
	shoved to an incompatable node.
&space
&term	-m : Install only on Darwin nodes; limited by the cluster selection option.
	Care must be taken when using this option as the operating sytem major and minor
	revsions numbers are not taken into account and could cause binary files to be 
	shoved to an incompatable node.
&space
&term	-n : No execution mode.  &This lists what it would do if this option were not set.
	Care must be taken when using this option as the operating sytem major and minor
	revsions numbers are not taken into account and could cause binary files to be 
	shoved to an incompatable node.
&space
&temr	-p permissions : If supplied, passes the permissions to &lit(ng_install) 
	allowing the user to control the final permissions on the file after it is 
	shoved to each location.
&space
&term	-T : Install only on nodes that are the same type as that running &this.
	The type of node is determined by combining the system name (e.g. FreeBSD) with 
	the major realease number of the system release (e.g. 5 from 5.1). Thus if 
	&this is executed on a node running FreeBSD 5.2, -T will cause the file to be 
	pushed only to nodes that indicate an attribute of &lit(FreeBSD5.)
&space
&term	-t type : Install on nodes that have &ital(type). The type string may contain multiple
	types, or !types if quoted.	The &ital(type) given with a &lit(-t) option is 
	any legitimate node attribute currently defined by one or more nodes on the 
	cluster indicated by the cluster option (-c).  For example, the type could be 
	Satellite, or !Satellite, as well as FreeBSD5.2.
&space
&term	-v : Verbose mode -- chatty about what it is doing. Two -v options cause some 
	subprocesses to be put into verbose mode as well.
&endterms


&space
&parms	Depending on the options, &this expects several positional parameters on the 
	command line.  The following lists the parameters, and when they are expected. 
	The order presented is the order expected.
&begterms
&term	srcfile : This is the name of the file that is to be installed on the remote nodes.
	It is always required.
&space
&term	destfile : This is the name of the destination file -- file where the installation 
	will take place.  It is required if the filename is being changed, or the target 
	diredtory is not &lit($NG_ROOT/bin,) or if node names are given on the command line.
	A dash (-) may be used to indicate the same basename as the source file, and a target
	directory of &lit(NG_ROOT/bin. )
&space
&term	nodes : One or more node names may be supplied as a finite list of nodes on which to install
	&ital(sourcefile).  If a -c and/or -t option(s) are supplied this list is ignored. 
&endterms

&space
&exit	The exit code should reflect the number of files that had errors during the installation 
	procss. &This does &stress(not) stop when an error is encountered.  An exit code of 
	0 indicates that all attempted installations succeeded, and an error code in excess of 
	100 indicates that there was an internal error.

&space
&see	ng_ccp, ng_install, ng_species, ng_members

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Jan 2004 (sd) : Conversion from personal tool.
&term	12 Mar 2004 (sd) : -T now establishes the node type as os name with the major
	number from the osversion (e.g. FreeBSD4) as the node type to ship to.
&term	04 May 2004 (sd) : Verbose message change.
&term	28 Jul 2004 (sd) : Added special cluster name flock -- sends to all members of 
	the flock.
&term	16 Nov 2004 (sd) : Added ability to pass permissions (-p)
&term	14 Jan 2005 (sd) : Converted to use -N on members with cluster name (narbalek) rather 
		than to depend on the remote netif node being up. Changed such that if 
		the source file is from /$NG_ROOT/* it will not send to this node. If the 
		source is not in NG_ROOT, then it will install on the node running the script.
		Old behavour was to avoid the node always.
&term	03 May 2005 (sd) : Fixed -n to report correctly when skipping local node; added doc for
	-a.
&term	16 Sep 2005 (sd) : Now passes the mtype option (-t) to get_likeme to properly determine
	the target nodes based on a supplied type.  
&term	29 Nov 2005 (sd) : Force uname with full path because of differing results between 
	ksh and sh under suse.
&term	16 Dec 2005 (sd) : TO convert to ng_uname.
&term	27 Jul 2006 (sd) : Gets TMP from the dest rather than assuming that TMP is NG_ROOT/tmp which it 
	might not be. 
&term	22 Jul 2008 (sd) : Fixed problem with get like me call -- needs to pass -c rather than executing 
		on the clusters netif. 
&endterms

&scd_end
------------------------------------------------------------------ */
