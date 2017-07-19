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
# Mnemonic:	get_likeme
# Abstract:	gets a list of nodes that are teh same o/s flavour as this node, or
#		a list of cluster(s) that have at least one node that is of the same 
#		flavour as this node.  Yes, this is similar to ng_species, but species
#		will not short circuit if all we need to know is that somewhere on 
#		the cluster exists a node of our type.
#		
# Date:		12 Jan 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# return success if node ($1) has same node type as defined in $ntype
function test_node
{
	typeset rattrs=""
	typeset x=""
	typeset matched_rc=0		# return code sent if we match (set to 1 if !attr is specified 
	typeset pattern="$ntype"	# what to search for -- we may strip
	typeset p=""

	if [[ $1 == $me ]]
	then
		return 1
	fi

	if [[ $ntype = "!"* ]]		# if we match Satellte and !Satellite supplied, then send back fail
	then
		matched_rc=1
		pattern=${ntype#?}
	fi

	ng_rcmd ${1:-badhostname} 'ng_get_nattr' | read rattrs
	if (( $use_arch > 0 ))
	then
		ng_rcmd ${1:-badhostname} "ng_uname -p" | read rarch	  # batter than the old way
		rattrs=${rattrs##* }-$rarch
	fi

	if [[ " $rattrs " == *" $pattern "* ]]		# ntype is in their attr list; spaces round things are a MUST!
	then
		return $matched_rc			# good or bad depending on if there was !attribute
	fi

	if (( $use_arch > 0 ))				# no initial match, see if an arch equivlancy exists for the remote node
	then
		for p in $NG_EQUIV_ARCH			# pairs of  this:that  such that if the remote arch is this, we try that against our pattern
		do
			if [[ $rarch == ${p%%:*} &&  ${rattrs%-*}-${p#*:} == $pattern ]]
			then
				verbose "equivalent arch matched remote=$rattrs pat=$pattern equiv=$p"
				return $matched_rc
			fi
		done
	fi

	verbose "$1 no match: $rattrs != *$pattern*"
	return $(( ! $matched_rc ))
}

# get a list of nodes from cluster ($1) that have same type. of need_one is 
# set then we list only the first and return
function get_nodes
{
	typeset members=""
	typeset m=""
	typeset o=""
	typeset nlist=""

	ng_members -N -c $1 | read members
	verbose "testing cluster $1 = $members"

	for m in $members
	do
		if test_node $m
		then
			if (( $single_line > 0 ))		# save and echo at end
			then
				nlist="$nlist$m "
			else
				echo $m
			fi

			rc=0						# found one, so good return code
			verbose "$1:$m matches requested type"
			if [[ $need_one -gt 0  ]]
			then
				return 0
			fi
		fi
	done

	if (( $single_line > 0 ))
	then
		echo $nlist
	fi
}

# -------------------------------------------------------------------
ustring="[-C ] [-c cluster] [-t type] [-v]"
need_one=0			# we need only one like node per cluster
cluster=""
verbose=0
single_line=0			# single line output

me=`ng_sysname`
ntype=`ng_get_nattr`		# get_nattr declares that the last token is of the order of FreeBSD5.3
ntype="${ntype##* }"		# so keep just the last token
use_arch=1			# factor in the arch too
has_arch=0			# user supplied type has arch

while [[ $1 = -* ]]
do
	case $1 in 
		-C)	need_one=1;;
		-c)	cluster="$cluster$2 "; shift;;
		-g)	use_arch=0;;				# generic -- dont use arch
		-s)	single_line=1;;
		-t)	ntype="$2"; 
			if [[ $ntype == *"-"* ]]
			then
				has_arch=1
			else
				use_arch=0
			fi
			shift
			;;

		-v)	verbose=1;;
		--man)	show_man; cleanup 2;;
		-\?)	usage; cleanup 2;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 2
			;;
	esac

	shift
done

if (( $need_one > 0 ))		# force single_line off it just need the first hit
then
	single_line=0
fi


if (( $use_arch > 0  && $has_arch < 1 ))	# add arch to the local type
then
	#ksh -c 'uname -p' |read arch
	#ng_rcmd localhost "`ng_rcmd localhost whence uname` -p" | read arch	  # NOT pretty, but it MUST be this way to work with uname in function.
	#ng_rcmd localhost "ng_uname -p" | read arch	  
	ng_uname -p | read arch
	ntype=$ntype-$arch
fi


verbose "searching for nodes with attribute: ${ntype:-any}"

rc=1
if [[ -z $cluster ]]
then
	ng_nar_map -c |read cluster		# get all clusters known to narbalek
fi

for c in $cluster
do
	get_nodes $c
done

cleanup ${rc:-1}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_get_likeme:Finds nodes with the same O/S type)

&space
&synop	ng_get_likme [-c cluster] [-C] [-t type] [-v]

&space
&desc	&This will search for nodes that have the same operating system type as the 
	node that is running the command.  
	The name(s) of matching nodes are written to the standard output device, one
	node name per line. 
	The search can be limited to specific
	clusters using -c; -C causes just the first node found to match types
	to be listed for each cluster searched (quicker).

&space
	Primarily this is used to send package files only to clusters that contain
	specific types of nodes; no sense in sending a Mac O/S package to a cluster
	that contains only FreeBSD nodes. 
&space
	The node that is running this script will never be listed as a match.

&space
&subcat Hardware Architecture Equivalency
	Some hardware architectures may be equivalent and will be matched if the 
	remote architecture is defined in NG_EQUIV_ARCH pinkpages variable.  The 
	variable should contain space separated pairs of &ital(this:that) where 
	a remote architecture of &ital(this) will be translated to &ital(that)
	if the remote node's archecture is not a direct match with the node executing the 
	script. 
	The contents of this variable are ignored if the &lit(-g) option is used. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : Test nodes on this cluster only.  If not supplied then all clusters
	known to narbalek are tested.  Mutiple cluster names may be given if space 
	separated and quoted. If not supplied, then all clusters known to Narbalek are searched.
&space
&term	-C : Cluster test.  Searches the cluster and returns the first node found that
	matches the desired type.  If multiple clusters were supplied, or all clusters are 
	being searched, this option causes the first matching node discovered in each 
	cluster to be written to the stdandard output device. 
	This is quicker if the user needs only to know that 
	a cluster contains at least one node of the given type
&space
&term	-g : Generic mode.  Normally the machine architecture is considered when 
	determining whether or not a remote node is 'like me.'  If this option is 
	supplied, then only the O/S name and version number (e.g. FreeBSD5.3) is 
	considered rather than FreeBSD5.3-i386. When a type is supplied with  the -t 
	option, generic is assumed; allowing for types that are node attributes (e.g. Satellite)
	to be used.
&space
&term	-s : Single line output mode.

&space
&term	-t type :  Overrides the type searched for.  This type will be used as the 
	matching criteria and can be anything that would be reported by &lit(ng_get_nattr)
	for a node. The type can be supplied as !name to list all of the nodes that are not
	of that type. 
	
&space
&term	-v : Verbose mode.
&endterms


&space
&exit	An exit code of 0 indicates that at least one matching node was found. If the 
	exit code is non-zero then no nodes were found to match the type.

&space
&see	ng_get_nattr, ng_add_nattr, ng_nar_map, ng_members

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Jan 2005 (sd) : Fresh from the oven.
&term	06 Apr 2005 (sd) : Added support for arch type. (-g turns it off).
&term	16 Sep 2005 (sd) : Fixed a bug in the type matching so that !attribute works.
&term	13 Oct 2005 (sd) : Fixed a typo in -t) that was causing two arch strings to be added.
&term	28 Nov 2005 (sd) : must go after uname -p info in an odd way as ksh seems to have a built-in for it which 
&term	16 Dec 2005 (sd) : Converted to ng_uname. 
		is not consistant.  Icky.
&term	20 May 2008 (sd) : Changed verbose message when remote node does not match this node's type. 
&term	29 Jul 2009 (sd) : Added support for NG_EQUIV_ARCH. 
&endterms

&scd_end
------------------------------------------------------------------ */
