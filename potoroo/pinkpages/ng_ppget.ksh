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
#
# Mnemonic:	ppget.ksh (narbalek flavoured)
# Abstract: 	set a pinkpages variable in narbalek. 
#		pinkpages variables are saved in narbalek using one of two naming 
#		syntaxes:
#			<cluster>/pinkpages:<varname>=<value>
#			<node>/pinkpages:<varname>=<value>
#
#		narbalek supports us supplying multiple variable names on a single
#		get command if separated with vert bars.  His behaviour is, working
#		left to right, return the first value that is not empty. This allows
#		us to search node|cluster|flock namespaces for a single variable name
#		with one call to narbalek (read one invocation of sendtcp and one 
#		session setup/taredown). 
#
# Date:		31 July 2002 
# Author:	E. Scott Daniels
# -----------------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# dump all pp stuff from flock, cluster then local name spaces
function dump_stuff
{
	typeset rc=0

	ng_nar_get -p $port -s $host -P pinkpages/flock: $subs
	rc=$(( $rc + $? ))

	ng_nar_get -p $port -s $host -P pinkpages/${target_cluster:-foo}: $subs
	rc=$(( $rc + $? ))

	ng_nar_get -p $port -s $host -P pinkpages/${target_node:-foo}: $subs
	rc=$(( $rc + $? ))

	return $rc
}

ustring="[-a] [-c] [-C cluster] [-f] [-l] [-L] [-p port] [-r] [-s host] [-v] [var_name]"
# -------------------------------------------------------------------

mynode=`ng_sysname`	# node used for execution
target_node="$mynode"	# default target is this node; -h changes it
target_cluster=${NG_CLUSTER:-no-cluster}
host=$mynode		# node where we send narbalek command to -s changes
timeout=4
verbose=0
temp=0			# -t sets to cause update just to cartulary, temporary
port=${NG_NARBALEK_PORT:-5555}	# pink pages now stands alone

cscope="pinkpages/$NG_CLUSTER:\$1"		# things scoped to the cluster
lscope="pinkpages/$mynode:\$1"		# locally scoped things
fscope="pinkpages/flock:\$1"		# things scoped to the flock
try=""					# what order to try things in (prefix wise) default applied in for if not set by -f/c/l
all=0
subs=""
full_list=0
seek_alt=1				# 1 if allowed to find an alternate narbalek if we get an error from our local one

while [[ $1 = -* ]]
do
	case $1 in
		-a)	all=1; full_list=0;;
		-A)	full_list=1; all=0;;

					# these override default order. one or more allowed
		-c)	try="$try$cscope ";;			# try cluster (ours)
		-C)	
			target_cluster=$2
			try="${try}pinkpages/$target_cluster:\$1 "; shift;;	# try cluster (some other as defined in $2)

		-f)	try="$try$fscope ";;			# try flock
		-l)	try="$try$lscope ";;			# try local	
		-L)	seek_alt=0;;				# do not allow a second look up if ours returns error
		-h)	
			target_node=$2					# local data for the named node, not this node
			x=$(ng_ppget -C $target_node -f NG_ROOT)	# we depend on -C doing right with a host name!  dig their ng_root value
			subs="-S NG_ROOT=$x"				# when we fetch we subs in their NG_ROOT
			root_val="$x"
			try="${try}pinkpages/$target_node:\$1 " 	# try host  -- same as -C, but more readable this way
			shift
			;;

		-r)	subs="-S NG_ROOT=$NG_ROOT"			 # subs our root for occurrances of $NG_ROOT 
			root_val="$NG_ROOT"
			;;

		-p)	port=$2; shift;;
		-s)	host=$2; shift;;

		-v)	verbose=1
			v="$v-v "
			;;

		--)	shift; break;;
		--man)	show_man; cleanup 3;;
		-\?)	usage; cleanup 3;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 3
			;;
	esac

	shift
done


if [[ -z $1 ]]					# support old dump default, but we just dump flock,cluster,this node rather than EVERYTHING
then
	set -o pipefail
	dump_stuff | awk -v all=$all  -v full_list=$full_list '		# we assume dump stuff gives in flock, cluster, local order
		{
			if( all )
				print;						# all of the name is printed, so every value is kept
			else
			{
				if( (x = index( $0, ":" )) > 0 )		# trash pinkpages/junk: from name
				{
					sstring = sprint substr( $0, x+1 );		# short string
					if( full_list )
						print sstring;
					else
					{
						split( sstring, a, "=" );
						keep[a[1]] = sstring;			# we keep the last of each name seen for print 
					}
				}
			}
		}
		END {
			for( k in keep )
				print keep[k];		# just the last seen
		}
	'
	cleanup $?
fi

eval search=\"${try:-$lscope $cscope $fscope}\"		# set up search order -- default if user omitted -l -c or -f flags

search="${search% }"			# chop last blank 
if (( $all < 1 ))
then
	while (( $seek_alt >= 0 ))
	do
		# dont use |read x  as it sets bad status if nothing assigned to x; we want good status with empty var if not defined
		x="$(ng_nar_get -v -e -s ${host:-no_host_name} -p $port $subs "${search// /|}")" # add pipes to get first non-empty value from order namespaces
		status=$?

		if (( $status == 0 )) && [[ -n $x ]]
		then
			if [[ $x == "#ERROR#" ]]
			then
				if (( $seek_alt <= 0 ))
				then
					log_msg "error indicator set by narbalek [FAIL]"
					echo ""
					cleanup 2
				fi
	
			else
				if [[ -n $root_val ]]
				then
					echo "${x//\$NG_ROOT/$root_val}"
				else
					echo $x
				fi
				cleanup 0
			fi
		else
			if (( $seek_alt <= 0 ))				# status 0 indicates not defined
			then
				verbose "unable to resolve pinkpages variable using local or alternate narbalek ($host;$search) [FAIL]"
				echo ""
				cleanup 1
			fi
		fi

		#ng_nar_get -b bootstrap_node | read host2 junk			# pick random alternate node for another go
		ng_members | awk -v me=$mynode '
			{
				srand( );
				x = split( $0, a, " " );
				if( x > 0 )
				{
                                	r = int( (rand()*1000) % x) +1;
                                	r2 = int((rand()*1000) % x) +1;
					if( a[r] != me )
						printf( "%s\n", a[r] );
					else
					{
						if( r2 == r )
							r2 = r2 < x ? r2 + 1 : 1;
						printf( "%s\n", a[r2] );
					}
				}
			}
		'| read host2 junk
		verbose "unable to get variable from $host, trying alternate: $host2"
		host=$host2
		seek_alt=$(( $seek_alt - 1 ))
	done

	cleanup 1
fi

# get all things regardless of namespace
for what in $search
do
	set -o pipefail 
	ng_nar_get -s $host -p $port  $subs $what | read x		
	status=$?
	if [[ -n $x ]]
	then
		if [[ $x == "#ERROR#" ]] || (( $status != 0 ))
		then
			log_msg "error indicator set by narbalek trying to lookup: $what [FAIL]"
			cleanup 2
		fi

		echo $what $x
	fi
done


cleanup  0   # always good if all supplied


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_ppget:Pink Pages - get a value)

&space
&synop	ng_ppget [-a] [-c] [-C cluster] [-f] [-l] [-l] [-p port] [-r] [-s host] [-v] [var_name]

&space
&desc	&This sends a request to ng_narbalek (cluster wide information map) 
	to retrieve the pinkpages value associated with &ital(var-name).  
	The value is echoed to the standard
	output device.  If var-name is not set, then a newline is echoed.
	If &ital(var_name) is not supplied on the command line, then all of the 
	applicable pinkages variables for the node are written to the standard output
	device. 
&space
	Variables that are set in narbalek using &lit(ng_ppset) may be retrieved
	with &this.  Pinkpages variables are stored in the &ital(narbalek) information 
	map in one of three name spaces: local, cluster and flock.  Variables placed
	into the local namespace may only be retrived when &this is executed on the 
	node that saved the value.  Similarly values saved in the cluster namespace
	may be retreived by &this only when executed on the same cluster that saved 
	the value.  &This will retrieve flock wide values from any node that has 
	access to &ital(narbalek.) 
&space
	It is common to duplicate variable names in multiple namespaces. For instance, 
	a value of NG_ROOT may be assigned flock wide, but also maintained in the 
	local namespace for nodes that use an alternate directory path for NG_ROOT. 
	The default search order is: local, cluster and finally the flock namespace. 
	The namespace search can be limited, or reordered using the &lit(-c, -f,) and 
	&lit(-l) command line options.  When used alone, they limit the search to just
	the indicated namespace. If more than one is supplied, the search is limited to the 
	indicated namespaces, and in the order that the options appear on the command line. For 
	example, -f -c  would return the value from the flock namespace if it exists
	and then from the cluster namespace if the variable is not defined in the flock
	namespace. If the variable is not in either, the local namespace is not searched.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Get all.  All values of &ital(name) are searched for and written to the 
	standard output device. Names are &stress(not) stripped of namespace information 
	that is used to keep the values separated in &ital(narbalek.)
&space
&term	-A : Get all, strip namespace information.  This will produce duplicate 
	variables if a variable exits in more than one namespace.  
&space
&term	-c : Attempt to locate the variable name in cluster space. Default order is to 
	search local space, then cluster space and finally flock space. May be supplied 
	with &lit(-l) and &lit(-f) options to control what is searched and which order.
&space
&term	-C cluster : Attempt to locate the variable in the cluster space of another 
	cluster.  This may be supplied with &lit(-h,) &lit(-l) and &lit(-f) options to control what 
	is searched and which order.
&space
&term	-f : Attempt to locate the variable name in flock space. Default order is to 
	search local space, then cluster space and finally flock space. May be supplied 
	with &lit(-h,) &lit(-l) and &lit(-c) options to control what is searched and which order.
&space
&term	-h host : Attempt to locate the variable in the name space of another 
	host.  This may be supplied with &lit(-C,) &lit(-l) and &lit(-f) options to control what 
	is searched and which order. If the value of the variable contains the string &lit($NG_ROOT,)
	it is expanded using the value of NG_ROOT on the named host. 
&space
	This option reports the variable's value only if it is specifically defined for that
	node (host), and if no specific value (local value) exists, the result will be nothing.
	The -c and -f options should be supplied (in that order) following the &lit(-h host)
	option on the command line if the value as it would be reported by &this on the 
	given node is desired.  The command &lit(ng_rcmd node10 ng_ppget -r VAR_NAME) would
	result in the same output as &lit(ng_ppget -r -h node10 -c -f VAR_NAME.)

&space
&term	-l : Attempt to locate the variable name in local space. Default order is to 
	search local space, then cluster space and finally flock space. May be supplied 
	with &lit(-f) and &lit(-c) options to control what is searched and which order.
&space
&term	-L : Use local narbalek only.  If we do not get a response from narbalek the normal 
	action is to query another narbalek. If this option is supplied on the command line, 
	the second check is not performed. 
&space
&term	-p port : Allows &this to send the message to a ng_narbalek process not listening
	on its standard port. 
&space
&term	-r : Causes any variable that has &lit($NG_ROOT) as a part of the value to have that
	portion of the value expanded and replaced by the NG_ROOT value for the node. 
	This option should NOT be supplied in conjunction with the -h option. If not supplied, 
	the &lit($NG_ROOT) portions of a value are not expanded. 
&space
&term	-s host : Allows &this to send the message to a ng_narbalek process not running on 
	the local node.
&space
&term	-v : Verbose mode.
&endterms


&space
&parms	The variable name that is to be fetched from the &ital(narbalek) information map
	can be supplied as the only positional parameter on the command line. If it is 
	omitted, then a dump of the &ital(narbalek) information for the flock, current 
	cluster, and the node is written to the standard output device. The order of the 
	dump is fixed and is &stress(NOT) de-duplicated.  


&space
&exit	&This will exit with a zero (0) return code when it feels that all processing 
	has completed successfully, or as expected. A non-zero return code indicates 
	that there was an issue with either the parameters or data supplied by the user, 
	or &ital(pinkpages) could not be communicated with.

&space
&see	ng_ppget, ng_parrot, ng_pp_build, ng_bo_peep, ng_narbalek

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	31 Jul 2002 (sd) : The orignal stuff -- nothing like new code on my last day!
&term	15 Mar 2003 (sd) : Added stuff to support the standalone version of pink pages. 
&term	20 Mar 2003 (sd) : Added support to scribble the new setting to the local cartulary 
	so that it is immediately available on the local node.
&term	03 Sep 2003 (sd) : Added -t option for temporary entries (mostly to support leader change)
&term	05 Jan 2004 (sd) : Started conversion to use $NG_ROOT/site rather than adm.
&term	07 Jan 2004 (sd) : Sets FLOCK_ variables on all clusters defined in FLOCK_MEMBERS unless
	-L is given.
&term	31 Mar 2004 (sd) : Executes a simple test on the variable name to try to ensure that it 
	is a legal shell variable name. Exits 1 if bad
&term	31 Mar 2004 (sd) : Executes a simple test on the variable name to try to ensure that it 
	is a legal shell variable name. Exits 1 if bad
&term	30 Aug 2004 (sd) : Converted for use with narbalek.
&term	20 Sep 2004 (sd) : Modified to take advantage of pipe separated var names on a single 
	narbalek call.  Much more efficent to setup one TCP/IP -- especially when we consider 
	99% of our lookups are flock wide and would have required three TCP/IP sessions!
&term	01 Dec 2004 (sd) : Fixed some problems with doc and ustring. 
&term	26 Apr 2005 (sd) : Added -h option.
&term	21 Aug 2005 (sd) : Added check for #ERROR# back from narbalek which indicates it could not 
	search for the value because its table was not loaded, or being refreshed.
&term	05 Oct 2006 (sd) : Fixed problem when dumping all pinkpages variables.  We were presenting 
		duplicates if a variable was in more than one of the name spaces.  &This now 
		filters them out such that only one of each variable should be presented. 
&term	08 Oct 2007 (sd) : Added a log message written if narbalek sends an ERROR indicator on a 
		full dump command (helps problem determination when pp_build complains).
&term	05 Dec 2007 (sd) : Added second attempt from another node if the fetch of a single variable fails. 
&term	23 Jan 2008 (sd) : Updated man page to reflect the addition of -L option.
		Changed the way an alternate host was located to make a second request. Bootstrap
		method using nar_get was too slow on relay nodes. 
&term	31 Mar 2008 (sd) : Added trailing colon to geneic patterns when doing a dump all so that a 
		node name that matches the first part of a cluster name is not matched. 
&term	22 May 2008 (sd) : Fixed bug in alternate lookup attempt; if no members listed it was causing
		an awk failure. 
&term	21 Apr 2009 (sd) : Modification to allow -r option do the right thing when requesting a single variable. 
&term	28 Apr 2009 (sd) : Changed a log_msg for unable to find variable to a verbose message. 
&endterms

&scd_end
------------------------------------------------------------------ */
