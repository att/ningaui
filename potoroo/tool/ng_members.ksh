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
# Mnemonic:	ng_members
# Abstract:	Looks at the participants of the last election and figures out 
#		who belongs in the cluster (nodes that did not vote because they 
#		are down, or otherwise unavailable are not reported on and this 
#		may not be a desired side effect of the script).
# Date: 	4 March 2002
# Author:	E. Scott Daniels
# ------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function get_members 
{
	case $1 in
		flock)
			ng_nar_map -c
			;;

		sclusters)			# generate a list of clusters that can house flock wide services
			echo "${NG_SCLUSTERS:-no-legit-clusters}"		# avoid kowari, ningdev, coot
			;;

		registered)			# members as registered with ng_cluster vars in narbalek
			ng_nar_get -P "pinkpages/.*:NG_CLUSTER" | grep ${cluster:-$NG_CLUSTER} |awk '
				{
					gsub( ":.*$", "", $1 );
					split( $1, a, "/" );
					print a[2]; 
				}
			'
			;;

		narbalek)	
			ng_nar_map -L ${NG_NARBALEK_LEAGUE:-flock} -T $timeout -l -m ${cluster:-$NG_CLUSTER} 
			;;

		rm)	
			if [[ $filer == "no_var_filer" ]]	# repmgr down or not being run on the cluster
			then
				verbose "no filer defined, getting a registered list from narbalek"
				get_members registered
			else
				if [[ $myhost == $filer ]]
				then
					(cd $NG_ROOT/site/rm/nodes; find . -mtime -$time -type f | grep -v xx) | awk '
						{
							if( split( $1, a, "/" ) == 2 )		# just want ./xxxx  not . or ./xxxx/yyyy
								if( ! index( a[2], "." ) )
										printf( "%s\n", a[2] );
					}
					'
				else
					verbose "getting list from filer: $filer"
					ng_rcmd $filer ng_members -r
				fi
			fi
			;;

		nodes)	
			if [[ $stats == "no_var_stats" ]]	# repmgr down or not being run on the cluster
			then
				verbose "no stats node defined, getting a registered list from narbalek"
				get_members registered
			else
				if [[ $myhost == $stats ]]
				then
					(cd $NG_ROOT/site/stats/nodes && find . -mtime -$time -type d) | awk '
						{
								if( split( $1, a, "/" ) == 2 )		# just want ./xxxx  not . or ./xxxx/yyyy
								printf( "%s\n", a[2] );
						}
					'
				else
					verbose "getting list from stats node: $stats"
					ng_rcmd $stats ng_members -n
				fi
			fi
			;;
	esac
}

function usage 
{
	cat <<endKat
	usage: $argv0 [-a|n|N|R|r] [-c cluster] [-l] [-s chr] [-T sec] [-t days] [-v]"
	       $argv0 -S [-l] [-s chr]
	       $argv0 -f [-l] [-s chr]
endKat
}
# -------------------------------------------------------------------
filer=${srv_Filer:-no_var_filer}		# must set to known string if not defined
stats=${srv_Stats:-no_var_stats}
myhost=`ng_sysname`

how=""			# how to determine members
prefix=""		# prefix to strip from node name (none stripped if empty)
time=1			# if we have seen an flist/stats file in the last time days, its a member
long=0			# set with -l -- long listing one node per line
timeout=4
cluster=""
union=0
hcount=0
sep=" "			# list seperator

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	hcount=$(( $hcount + 1 )); how="rm nodes narbalek registered";;
		-n) 	hcount=$(( $hcount + 1 )); how="${how}nodes ";;		# look in site/nodes for answer	(not always correct)
		-N)	hcount=$(( $hcount + 1 )); how="${how}narbalek ";;
		-r)	hcount=$(( $hcount + 1 )); how="${how}rm ";;
		-R)	hcount=$(( $hcount + 1 )); how="${how}registered ";;
		-S)	howcount=1; how=sclusters;;		# generate list of clusters that we can run flock services on
		-f)	howcount=1; how=flock;;

		-c)	cluster=$2; shift;;
		-l)	long=1;;			# list one per line
		-s)	sep="$2"; shift;;		# a list seperator if desired
		-t)	time=$2; shift;;		# allow user defined time
		-T)	timeout=$2; shift;;
		-u)	union=1;;
		-v) 	verbose=1;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ $# -gt 0 ]]
then
	log_msg "positional parameter(s) unrecognised: $*"
	cleanup 1
fi

for h in ${how:-registered}
do
	get_members $h >>$TMP/list.$$				# get members by each method, then sort unique 
done

if (( $union > 0 ))
then
	awk -v need=$hcount '
		{
			seen[$1]++;
		}
		END {
			for( x in seen )
				if( seen[x] >= need )
					print x;
		}
	' <$TMP/list.$$ |sort -u >$TMP/slist.$$
else
	sort -u $TMP/list.$$ >$TMP/slist.$$
fi

awk -v long=$long -v prefix="$prefix" -v sep="$sep" '
	{
		if( long )
			for( i = 1; i <= NF; i++ )
				printf( "%s\n", $(i) );
		else
		{
			gsub( " ", sep, $0 );				# in case multiple per line
			printf( "%s%s", NR > 1 ? sep : "", $0 );	# in case one per line, add sep after 1st
		}
	}
	END { 
		if( !long )
			printf( "\n" ); 
	}
	' <$TMP/slist.$$


cleanup 0
exit 0				# needed to support scd 
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_members:List Members Of The Cluster)

&space
&synop	ng_members [-a|n|N|R|r] [-c cluster] [-l] [-s chr] [-T sec] [-t days] [-v]
&break	ng_members -S [-l] [-s chr]
&break	ng_members -f [-l] [-s chr]

&space
&desc	&This will determine the members of a cluster using one of several different 
	mechanisms based on command line options.  The resulting output is a space
	separated list of node names.  The &lit(-l) (long list) and &lit(-s) (alternate character
	separated list) options can be used to change the presentation of the list

&space
	If no determination method is supplied on the command line a default mechanism
	is used that is currently the most accurate method of determining the members 
	of the cluseter.  If more than one mechanism is used, the members list will be
	computed using each method, with the resulting list being all nodes reported 
	by any mechanism.  Each node is guarenteed to appear in the list only once. 

&space
&subcat	Mechanism
	Several command line options (see below) control which mechanism is used to 
	determine the members list for the cluster.  Currently the supported mechanisms
	are:
&space
&beglist
&item	search the narbalek/pinkpages name space for cluster members
&item	determine members based on current file lists in the replication manager's node directory
&item	use the current status files in the stats directories
&endlist

&space
	A 'current file' in either the replication manager node directory, or the status
	directory, is considered to be a file that has been updated within the last day.
	The &lit(-t) option can be used to change that such that the threshold is more than 
	one day.
	
&space
	Depending on the mechanism used to generate the members list, &this may cause 
	a job to be executed on another node, the &lit(srv_Filer) for instance, and 
	thus execution time might be increased.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : All mode. The same as supplying the options -N, -n, and -r and -R.
&space
&term	-c cluster : Lists members for the cluster. This option cannot be used with the -r
	option. 
&space
&term	-f : Flock listing.  Generates a list of clusters.  The list is created by sending 
	a request on the Narbalek multicast group, and listing the various cluster
	names that are returned by the narbaleks that respond.  If a cluster is not 
	reacable, or has no running narbalek processes, it will not be listed.
	
&space
&term	-l : Long listing -- members are listed one per line rather than on a single line.
&space
&term	-n	: Node mode. Nodes are determined by looking at the &lit(nodes) directory
	within the statistics directory: &lit($NG_ROOT/site/stats.)
&space
&term	-N : Narbalek mode. The members of the indicated cluster are listed based on 
	the cluster that each responding narbalek process believes it belongs to.
	The list will reflect only the nodes that respond within the timeout period, 
	and thus may not be a complete list.  The execution time of this command is 
	slightly longer than the others as the script waits for a minimumn number of 
	seconds (see -T) for narbalek responses. 
&space
&term	-r : Repmgr mode. Nodes in the cluster are determined by looking at the nodes 
	reporting to replication manager.
&space
&term	-R : Registered mode; lists all nodes that are registered as belonging to the 
	named cluster.  A node is considered registered to a cluster if it has an 
	explicit NG_CLUSTER variable set in pinkpages.
	The node list generated with this option does not guarentee that the
	node is running, or that it exists as nodes may be preregistered. 
&space
&term	-S : Flockwide service cluster list.  When this option is placed on the command line
	a list of clusters that can be used to potentially host flock services is 
	written to the standard output device.  
&space
&term	-s char : Separates each member node name with &ital(char). If omitted, a blank 
	is used.
&space
&term	-T sec : Timeout. Number of seconds to wait for responses from narbalek.
&space
&term	-t days : Defines the number of days that a stats file or file list file is considered
	to be valid.  If a file has been modified in the last &ital(days) days, then it 
	represents a node that is active on the cluster. The default is 1 day.
&space
&term	-u : Union.  Returns the union of members if more than one method is used to 
	compute the members list (e.g. ng_members -r -R -u).
&space
&term	-v : Verbose mode.
&endterms

&space
&exit	This command returns a non-zero exit code when an error was detected. 

&space
&see	&ital(ng_rcmd) &ital(ng_narbalek) &ital(ng_nar_get) &ital(ng_ppget)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Mar 2002 (sd) : Fresh cement.
&term	15 Jul 2002 (sd) : converted to use dig as nslookup claims to be depreciated.
&term	09 Aug 2002 (sd) : Added repmgr method of determining members, and prefix stripping.
&term	26 Aug 2002 (sd) : Corrected index() parms in rm case (excludes filenames containing dot character(s)).
&term	31 Aug 2004 (sd) : Added -l option, and narbalek option.
&term	08 Oct 2004 (sd) : Added -N (narbalek) option and ability to supply a seperator character.
	Updated man page.
&term	18 Oct 2004 (sd) : Now errors if command line positional parms are given.
&term	12 Jan 2005 (sd) : Added support for new nar_map interface to members list (-N and -R changed/added).
&term	18 Feb 2005 (sd) : Added unon option.
&term	29 Mar 2005 (sd) : Added -S and -f options.
&term	17 Mar 2008 (sd) : Changed default to be registered members so that the dependance on filer being up and 
		running can be eliminated. 
&term	24 Jul 2008 (sd) : Fixed problem when running with a narbalek league that is not the default. .** HBD EKD/ESD 76/71
&endterms

&scd_end
------------------------------------------------------------------ */
