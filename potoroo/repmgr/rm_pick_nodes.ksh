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
# Mnemonic:	rm_pick_nodes
# Abstract:	generate a list of nodes on a cluster in much the same way as repmgr
#		might select them for file copies.
#		
# Date:		25 April 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-c cluster] [count]"

cluster=$NG_CLUSTER
pickn_flags=""					# opts pased to pickn
dfname=""

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	pickn_flags="$pickn_flags-b ";;			# use repmgr loadbalancing algorithm rather than uniform dist
		-c)	cluster=$2; shift;;
		-f)	dfname=$2; shift;;				# dispersion file name

		-v)	vflags="$vflags-v "; verbose=1;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

ng_ppget -C $cluster srv_Filer |read filer
if [[ -z $filer ]]
then
	log_msg "cannot get filer for $cluster"
	cleanup 1
fi

if [[ -n $dfname ]] && (( ${NG_RM_PICKN_DISPERSE:-1} >= 1 ))
then
	ng_rm_disperse $dfname | read pre suf
	hood="$pre$suf"
	if [[ -n $hood ]]
	then
		verbose "selecting node based on dispersion hood: $hood"
		ng_rcmd $filer 'gre := $NG_ROOT/site/rm/dump|gre '$hood'|head -1' | read junk1 junk2 node
		if [[ -n $node ]]
		then
			if [[ " $NG_RM_PICKN_AVOID " != *" $node "* ]]
			then
				verbose "select node: $node"
				echo $node
				cleanup 0
			else
				verbose "dispersion node ($node) is in avoid list, selecting random node"
			fi
		else
			log_msg "no dispersion hood mapping found in rmdump for file, selecting random node"
		fi
	else
		log_msg "no dispersion mapping known to ng_rm_disperse, selecting random node"
	fi
fi
				# base selection on the hood that the file maps to 
#/ningaui/site/rm/nodes/ningd7:totspace=581361 freespace=424833
# spaces round the avoid list are key
ng_rcmd $filer 'grep -H freespace $NG_ROOT/site/rm/nodes/*' | awk -v avoid=" $NG_RM_PICKN_AVOID " -v verbose=$verbose '
	{
		split( $1, a, ":" );
		x = split( a[1], b, "/" );
		node = b[x];

		if( index( avoid, " " node " " ) )		# skip anything in the avoid list
		{
			if( verbose )
				printf( "pick_nodes: skipped: %s\n", node ) >"/dev/fd/2";
			next;
		}

		if( index( node, "." ) )			# skip coot00.28829
			next;

		split( a[2], c, "=" );
		split( $2, d, "=" );

		tot = c[2];
		avail = d[2];

		printf( "%s %.0f %.0f\n", node, tot, avail );
	}
' | ng_rm_pickn $vflags $pickn_flags $1

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_pick_nodes:Pick nodes as repmgr might)

&space
&synop	ng_rm_pick_nodes [-c cluster] [count]

&space
&desc	&This generates a list of node names from the indicated cluster. Nodes
	are selected using the current list of nodes sending filelists to 
	replication manger. By default, the selection method should generate 
	a failry uniform distribution across all of the available nodes. When the 
	balance option (-b) is supplied on the command line, preference is given to 
	nodes with the most available disk space, in an  effort to balance the 
	data across the cluster. As a result, with the balance option, some nodes 
	may appear very seldomly in the list, or be omitted completely.
&space
	Regardless of the method used (uniform or balanced)
	nodes that are reporting a 100% discount (Sattelites) are automatically excluded because 
	of the perception that there is no free disk space.  

	
&space
&subcat Avoiding Nodes
	The pinkpages variable &lit(NG_RM_PICKN_AVOID) may contain a list of space separated
	nodes names which should never be picked.  This permits a manual way of avoiding 
	a damaged node whose file list is still present in the replication manager 
	nodes directory.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Balanced orientation rather than uniform distribution.  When this option is
	given, the preference is given to nodes that have the most available disk space
	rather than an attempt to generate a list of nodes with each node selected uniformly.
&space
&term	-c cluster : Specifies the cluster from which a node is found. If not supplied 
	the current cluster is assumed.
&space
&term	-f filename : Selects the target node baed on the hood returned by a call to &lit(ng_rm_disperse)
	with the given filename and the node that the hood name "points to" inside of 
	replicaiton manager.  If the resulting node name can be determined, and the node
	is not on the avoid list (see above), then that node name is written to stdout. 
	If the pinkpags variable &lit(NG_RM_PICKN_DISPERSE) is set to 0, this feature is 
	disabled. If the variable is not set, the default is 1. 
&space
	In the case where a filename does not map to a hood name, or the hood is not known to 
	replication manager, the normal selection logic is used to generate the node name.
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms	One optional command line parameter may be supplied to &this. This is the number 
	of nodenames to return.  If the count is more than one, then one nodename per line 
	is written to standard output.  

&space
&exit	An exit code of 0 indicates success; all others failure.

&space
&see
	ng_rm_pickn, ng_fcp, ng_repmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Apr 2005 (sd) : Sunrise.
&term	29 Apr 2005 (sd) : Converted to proper way of determining filer (using ppget).
&term	09 May 2005 (sd) : Modified to allows -b (balance) to be sent to ng_rm_pickn.
&term	22 Apr 2007 (sd) : when supplying -v it is passed through to ng_rm_pickn.
&term	28 Aug 2007 (sd) : Added ability to list nodes to avoid in NG_RM_PICKN_AVOID. 
&term	24 Jul 2008 (sd) : Corrected typo in man page. .** HBD EKD/ESD/76/71
&term	06 Nov 2008 (sd) : Added -f option and ability to select based on the assigned hood of a file.
&term	25 Aug 2009 (sd) : Changed gre to grep -H when sussing the freespace. If just one node, then the data
		coming back did not have the host name and that was buggering things. 
&endterms

&scd_end
------------------------------------------------------------------ */
