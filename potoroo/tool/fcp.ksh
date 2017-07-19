#
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


# Mnemonic:	rm_fcp - flock copy
# Abstract:	shoves a file to another node (via ccp) on another cluster.
#		the file is preregistered with repmgr onthe other cluster.
#
# Date:		05 Jan 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


plog=$TMP/log/fcp.debug

# 'tee' msgs to our log as well as to user's log
function my_log
{
	if (( ${NG_FCP_CAPTURE:-0} > 0 ))
	then
		echo "$(date) [$$] $1" >>$plog
	fi
	log_msg "$1"
}

function my_verbose
{
	if (( ${NG_FCP_CAPTURE:-0} > 0 ))
	then
		echo "$(date) [$$] $1" >>$plog
	fi
	verbose "$1"
}

function my_cleanup
{
	case $1 in 
		0)	ng_log $PRI_INFO ${argv0##*/} "fcp ok $mynode:$lfile --> ${cluster:-cluster_unknown}:${node:-node_unknown}:${rname:-no-spaceman-name}"
			;;
		*)	ng_log $PRI_ERROR ${argv0##*/} "fcp failed ($1) $mynode:$lfile --> ${cluster:-cluster_unknown}:${node:-node_unknown}:${rname:-no-spaceman-name}"
			;;
	esac

	my_verbose "exiting: rc=$1 dest=$cluster:$node rname=${rname:-unset} lfile=$lfile"
	cleanup $1
}


# send $1 (file) with optional $2 (md5)  to remote cluster
function pitch
{
	typeset file=$1
	typeset md5=$2
	typeset size=""
	typeset f=""
	typeset n=""

	if [[ -z $file ]]
	then
		log_msg "pitch: missing filename"
		usage
		my_cleanup 10
	fi

	my_verbose "pitching $1 ($2)"

	if [[ ! -r $file ]]
	then
		log_msg "missing or unreadable file: $file"
		my_cleanup 11
	fi
	
	if (( $register_file > 0 ))			# if registering file on remote cluster, must have md5, then do it
	then
		if [[ -z $md5 ]]
		then
			my_verbose "md5 not supplied on command line; looking up md5 and size for $file in repmgr, on this host"
			ng_rm_where -s -5 ${mynode:-junk} ${file##*/} | read f md5 size		# assume file lives on this node

			if [[ -z $md5 ]]				# resort to lookup on filer, tho this seems bad
			then
				ng_rm_where -s -5 -n ${file##*/} | read f n md5 size
			fi

			if [[ -z $f || $f == MISSING ]]
			then
				my_log "cannot find md5 for file ($file) in replication manager space"
				my_cleanup 12
			fi
		
			if [[ -z $md5 ]]
			then
				my_log "md5 returned by rm_where for $file is bad or missing: ($md5)"
				my_cleanup 13
			fi
		else
			echo "$md5 $file"|ng_rm_size|read junk junk size
		fi
	
		my_verbose "file ${md5:-badmd5} ${file##*/} ${size:-0}"

		echo "file $md5 ${file##*/} $size $copies" >/tmp/rmcmds.$$
		if [[ -n $hood ]]
		then
			echo "hood $md5 ${file##*/} $hood" >>/tmp/rmcmds.$$
		fi
		if ! ng_rmreq -s $filer	</tmp/rmcmds.$$		# preregister the file
		then
			my_log "unable to communicate with  repmgr on $cluster:$filer"
			my_cleanup 14
		fi

		my_verbose "file registered with remote replication manager"
	else
		my_verbose "sending file as an unattached instance"
	fi

	ng_rcmd ${node:-no-hostname} ng_spaceman_nm $file|read rname

	if [[ -z $rname ]]
	then
		my_log "unable to get a spaceman name from $node"
		my_cleanup 15
	fi

	rname=${rname%\~}			# trash tilda if spaceman put it there; shunt will add !

	my_verbose "send $file --> $node:$rname"

	if (( $show_dest > 0 ))
	then
		echo "$node $rname"			# both node and path with this option on
	else
		echo "$rname"				# just put out fname to mimic ccp
	fi

	if ! ng_ccp $vflag -d $rname $node $file >/dev/null	# ccp echos out the file path; we already did so supress
	then
		my_log "ccp failed: $file --> $node:$rname"
		my_cleanup 16
	else
		my_verbose "ccp ok: $file --> $node:$rname"
	fi

	if (( $move > 0 ))			# verify receipt before registration
	then
		ng_rcmd ${node:-no-hostname} md5sum $rname|read sum junk
		if [[ $sum != $md5 ]]
		then
			my_log "after copy mismatch of md5 values: $md5(local) $sum(remote)"
			my_cleanup 17
		fi
		my_verbose "md5 values match after copy: $md5 $sum"
	fi

	if (( $sync > 0 ))			# do instance stuff synchronously?
	then
		if ! ng_rcmd ${node:-no-hostname} ng_rm_rcv_file $rname
		then
			my_log "received file failed: $node:$rname"
			my_cleanup 18
		fi
	else
		if ! ng_wreq -s ${node:-no-hostname} "job: RM_RCV_FILE cmd ng_rm_rcv_file $rname"
		then
			my_log "ng_wreq received file failed: $node:$rname"
			my_cleanup 19
		fi
	fi

	ng_log $PRI_INFO $argv0 "send ok $mynode:$file --> $node:$rname   reg=$register_file hood=$hood"

}



# -------------------------------------------------------------------

ustring="[-C count] [-H hood] [-m] [-n node|!node[,node...]] [-t [!]nattr] [-u] [-a] -c cluster file [md5sum size]"
node=""
cluster=""
hood=""
copies="-2"
move=0
sync=1
register_file=1		# causes file to be registered on remote side; turn off with -u (send as unattached instance)
ntype="!Satellite"	# 7/15/04 -- changed default behaviour to this.
mynode=$(ng_sysname -s)
ctimeout=30		# connection timeout
rtimeout=220		# response timeout
verbose=0
show_dest=0		# -d causes the destination (node/path) to be announced onto stdout

while [[ $1 == -* ]]
do
	case $1 in 
				# -1 and -2 are deprecated (not documented) but recognised for backwards compat
		-1)	rmreq_version="V1 ";;			# allow old repmgr cluster to send to new repmgr cluster
		-2)	rmreq_version="V2 -p 29002";;			# allow old repmgr cluster to send to new repmgr cluster

		-a)	sync=0;;
		-c)	cluster=$2; shift;;
		-C)	copies=$2; shift;;
		-d)	show_dest=1;;
		-H)	hood="$2"; shift;;
		-m)	move=1;;
		-n)	node=$2; shift;;
		-t) 	ntype="$2"; shift;;
		-u)	register_file=0;;

		-v)	vflag="$vflag-v "; verbose=$(( $verbose + 1 ));;
		--man)	show_man; my_cleanup 91;;
		-\?)	usage; my_cleanup 91;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			my_cleanup 91
			;;
	esac

	shift
done

if [[ -n $node && $node == "!"* ]]		# -n !nodea,[!]nodeb  prevents us from sending to these nodes
then
	not_list="$node"
	node=""
else
	node=${node%%,*}			# we only take the first one
fi

if [[ -z $cluster ]]
then
	my_log "cluster name (-c) must be supplied on command line"
	usage
	my_cleanup 91
fi

ng_ppget -C $cluster srv_Filer|read filer
if [[ -z $filer ]]
then
	ng_rcmd $cluster ng_wrapper -e srv_Filer  | read filer
	if [[ -z $filer ]]
	then
		my_verbose "unable to determine filer for $cluster; pausing then trying again"
		sleep 10
		ng_ppget -C $cluster srv_Filer|read filer
	fi
fi

if [[ -z $filer ]]
then
	my_log "abort: still cannot determine srv_Filer for $cluster cluster"
	my_cleanup 1
fi

# ---- we now use ng_rm_pick_nodes to select a node as repmgr would so we dont need members
#ng_rcmd -c $ctimeout -t $rtimeout $filer ng_members -r 2>/dev/null| read nlist		# fast as only one question need be asked

#if [[ -z $nlist ]]			# should not happen, but pause, then try one more time before panicking
#then
#	my_verbose "members list from $filer was empty; pausing before a retry"
#	sleep 5
#	ng_rcmd -c $ctimeout -t $rtimeout $filer ng_members -r 2>/dev/null| read nlist	
#	if [[ -z $nlist ]]						# something wrong; panic now
#	then
#		my_log "ng_members returned a null list (twice) from $filer; aborting"
#		my_cleanup 21
#	fi
#fi

delay=0
tries=2						# number of tries before we will do a species
while [[ -z $node ]]
do
	#ng_rm_pick_nodes -c $cluster | read node		# pick in the manner that repmgr does

	ng_rm_pick_nodes -c $cluster 50 | awk -v verbose=${verbose:-0} -v not_list="$not_list" '
	BEGIN {
		gsub( "!", "", not_list );		# lead token must have !, other nodes might or might not -- trash all
		gsub( " ", ",", not_list );		# allow blank separated too
		x = split( not_list, a, ","  );
		for( i = 1; i <= x; i++ )
			dont_sel[a[i]] = 1;		# collect in hash by name
	} 
	{	
		if( dont_sel[$1] == 1 )
		{
			if( verbose > 1 )
				print "select node: rejecting: ", $1 >"/dev/fd/2";
			next;
		}
		printf( "%s\n", $1 );		# print first encountered that is not listed on the dont-sel list
		exit( 0 );
	}
	' | read node
	
	if [[ -n $ntype ]]			# specific type of node requested or !requested -- ensure it does/does not match
	then
		if ! ng_test_nattr -l "$(ng_get_nattr -t 180 $node)" $ntype
		then
			node=""
			tries=$(( $tries - 1 ))
			if (( $tries < 0 ))
			then
				my_verbose "abort: could not find a node that mached the requested type: $ntype"
				my_cleanup 22
			fi

			if (( $tries < 1 ))			# last ditch effort to find a node of the right type
			then
				my_verbose "needing a species list to honor node type of $ntype; pregnant pause..."

						# species is slow; but beats looping forever
 				ng_rcmd -c $ctimeout -t $rtimeout $filer ng_species ${ntype:-!Satellite} 2>/dev/null| read nlist		
			 	echo "$nlist" | awk '			
			                {
						srand();
			                       s = int( ((rand()*1071) % NF)+1);
						print $(s);
			                }
			        ' |read node
			fi
		fi
	else
		if [[ -z $node ]]
		then
			delay=$(( $delay + 10 ))
			if (( $delay > 45 ))
			then
				delay=45
			fi
			my_verbose "did not get a node -- pausing $delay seconds"
			sleep delay
		fi
	fi
done

my_verbose "selected node to receive file: $node"

if [[ -z $1 ]]				# nothing, assume file list from stdin
then
	while read a b junk		# filename [md5]
	do
		lfile=$a		# for debugging
		pitch $a $b 
	done
	my_cleanup $?
else
	lfile=$1		# for debugging

	pitch $1 $2			# if caller used $(md5sum foo$( on cmd line $3 will have filename too; be careful
	my_cleanup $?
fi


my_verbose "exiting normally"
my_cleanup 0




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_fcp:Flock Copy)

&space
&synop	ng_fcp [-C count] [-H hood] [-m] [-n node|!node[,node...]] [-t [!]nattr] [-u] -c cluster file [md5sum]

&space
&desc	Copies a file on the local node to another cluster in the flock and registers it with repmgr
	on the destination cluster.
	If the md5 value and size are not supplied on the command line then they 
	are sussed out of replication manager. This allows a file to be copied even 
	if it is not registered on the local cluster. 
&space
	By default, &this will not send the file to a node which has the attribute of
	&lit(Satellite.)  This can be overridden using the node type option (-t) 
	which allows the user to specify the node attribute that the destination 
	node must have.  If &this cannot find any nodes on the target cluster that 
	matches the indicated type, or do not have the indicated type if !attribute
	is supplied, then the failure will be indicated and &this will exit with a 
	non-zero return code.
&space
&subcat	Debugging
	If the pinkpages variable &lit(NG_FCP_CAPTURE) is set to 1, then all log and verbose
	messages are captured in &lit(NG_ROOT/site/log/fcp). In this mode, verbose messages 
	are always captured regardless of the user's setting of the verbose option on the 
	command line.  The user will see verbose messages on stderr only if the command line 
	option is set. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : The local registration of the instance on the destination node is done asynchronously,
	rather than the default.
	That is, the &ital(ng_rm_rcv_file) is done via &ital(ng_wreq) rather than &ital(ng_rcmd).
&space
&term	-c cluster : The name of the cluster (e.g. numbat) that is to receive the file.
	This flag is required.
&space
&term	-C copies : Supplied the number of copies that should be maintained by the 
	destination replication manager.  If not supplied the default number is assumed.
&space
&term	-d : Show destination.  For each file that is coppied, the node and destination pathname 
	is written to the standard output device. 
&space
&term	-H hood : Hood to supply when registering with the other cluster.
&space
&term	-m : Prep for move. Verifies the MD5 checksum on the copied file and will invoke 
	the replication manager receive process on the remote cluster only if the checksum
	values match. The coppied file is left as an unattached instance if there is 
	a mismatch.
&space
&term	-n node : Specific node in the other cluster to which the file is coppied.
	If not supplied the first, then a node is selected at random from the cluster's 
	members list -- taking into consideration the node type supplied by the user. 
&space
	Alternately, &ital(node) can be one or more nodes not to select rather than a node 
	to select.  If the list is started with a bang (!) character, then the list is treated
	as node(s) that should not be sent the file.  (e.g. -n "!bat03,bat02")
&space
&term	-t attribute : Indicates the attribute, or !attribute, type of the node that should 
	be targeted if a specific node is not supplied with the -n option. By default,
	an attribute type of &lit(!Satellite) is used if this option is omitted.
&space
&term	-u : Send as unattached instance.  The file is sent, and added to the destination
	node's local database, but is &stress(NOT) registered with replication manager. 
	After the node's next file list is sent to the leader, the file will appear as 
	an unattached instance. 
&space
&term	-v : Verbose mode. More than one -v might cause extra messages to be presented. 

&endterms


&space
&parms	The filename of the file to copy, and optionally its md5 and size.  If the md5
	and size are not given, then replication manager is querried for those values
	and the copy will fail if the file is unknown to replication manager.

&space
&exit	A zero return code indicates a positive result: the file was coppied and 
	added to the file list on the remote node.  Any non-zero return code indicates
	an error.

&space
&see
	ng_rmreq, ng_rm_file_rcv

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Jan 2004 (sd) : Thin end of the wedge.
&term	20 Jan 2004 (sd) : Added randimiser on the node selection.
&term	25 Feb 2004 (sd) : Added send as unattached instance (-u) and node type options.
&term	17 Mar 2004 (sd) : Added -2 option to support sending to a  repmgr version 2 cluster
&term	08 Apr 2004 (sd) : Corrected a bug checking for empty node list.
&term	17 May 2004 (sd) : Avoided use of ppget.
&term	09 May 2004 (sd) : fixed problem with scotts fat fingers (missing $)
&term	15 Jul 2004 (sd) : Default behaviour changed to avoid using Satellite nodes as a destination.
&term	12 Oct 2004 (sd) : Added early check for filename availability, and better my_verbose messages.
&term	01 Apr 2005 (sd) : Now writes destination host:path even when not in verbose mode.
		Added debugging.
&term	20 Apr 2005 (sd) : Added timeouts to all rcmds (30s connection and 220s response).
&term	29 Apr 2005 (sd) : Fixed typo that was causing satellite nodes to be selected.
&term	07 Aug 2005 (sd) : Must remove ~ that remote spaceman_nm adds; shunt will protect the file and we
		dont want ~ on the end when it gets there.
&term	04 Apr 2006 (sd) : Fixed message that should have been logged only when in verbose mode. 
&term	08 Jun 2006 (sd) : Added timeout to get_nattr call.  If it does result in an rcmd to the node
		we need to be patient.  This was causing get_nattr storms on busy nodes. 
&term	16 Jun 2008 (sd) : Added code to prevent errors if missing node var. 
&term	17 Apr 2009 (sd) : Added abilty to supply "not" list on -n. 
&term	14 May 2009 (sd) : Aded -d option. 
&endterms

&scd_end
------------------------------------------------------------------ */
