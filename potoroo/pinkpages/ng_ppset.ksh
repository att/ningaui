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
# Mnemonic:	ppset.ksh (narbalek flavoured)
# Abstract: 	set a pinkpages variable in narbalek. 
#		pinkpages variables are saved in narbalek using one of three naming 
#		syntaxes:
#			pinkpages/flock:<varname>=<value>
#			pinkpages/<cluster>:<varname>=<value>
#			pinkpages/<node>:<varname>=<value>
# Date:		31 July 2002 
# Author:	E. Scott Daniels
# -----------------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# throw something into the cartulary
function add2cartulary
{
	return		# deprecated 11/11/2008

	if [[ $host != $mynode ]]		# user set -h or -C flag to set for another host/cluster
	then
		return
	fi

	echo "$1"  | awk  -v scope="$scope" '
		{
			x = index( $0, "=" );
			printf( "export %s=\"%s\"\t# revised entry added by ppset scope=%s\n", substr( $0, 1, x-1), substr( $0, x+1 ), scope );
		}
	'>>${CARTULARY}		# append to cartulary for immediate availability on this node
}

# see if we can prevent badly named vars from getting in and causing havoc
function testit
{
	typeset tf=/tmp/test.$$
	echo "$name=\"foobar\"" >$tf
	ksh $tf
	return $?
}

ustring="[-c comment] [-C cluster-name | -f | -l | -h host] [-p port] [-s host] [-v] var_name value-tokens"
# -------------------------------------------------------------------

mynode=`ng_sysname`
host=$mynode		# local var assigned for this node (-h to override)
nhost=$mynode		# assume narbalek running here (-s to override)
cluster=$NG_CLUSTER	# by default scope to our cluster
timeout=6
verbose=0
comment=""
scope=cluster		# -l sets to cause update to be done to the local stuff, -f for flock wide vars; cluster is default
id="(cluster)"		# add to comment to help debugging 
temp=0			# -t sets to cause update just to cartulary, temporary
port=${NG_NARBALEK_PORT:-5555}	# pink pages now stands alone

while [[ $1 = -* ]]
do
	case $1 in
		-c)	comment="$2"		# user added comment too
			lcomment="	# $2"
			shift
			;;

		-C)	host="none" cluster="$2"; shift;;		# allow cluster override (host == junk to prvent cartulary update

		-f)	id="(flock)"; scope="flock";;
		-l)	id="(local)"; scope="local";;		# local to just this node

		-h)	id="(local)"; scope="local"; host=$2; shift;;	# allow a local value to be set for another node
		-L)	;;					# deprecated, but ignored to support clusters still running old version
		-p)	port=$2; shift;;
		-s)	nhost=$2; shift;;	# narbalek host
		-t)	temp=1;;		# temp setting -- only into cartulary
		-T)	timeout=$2; shift;;
		-v)	verbose=1
			v="$v-v "
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

if [[ $# -lt 1 ]]
then
	usage
	cleanup 2
fi

if [[ -n $port  ]]
then
	if [[ $1 = "export" ]]
	then
		shift
	fi

	name=${1%%=*}=		# variable name 	allowing  name=crap crap  OR name crap crap

	if [[ $1 = *=* ]]		# allow name value  or name=value
	then
		stuff="$@"		# all crap entered
		comment="$name$comment   $id"		
	else
		shift				# dont need name; we snarfed its alaready in name with = 
		stuff="$name$@"			# add name= to the rest of stuff

		if [[ -n $comment ]]
		then
			comment="$name$comment"
		fi
	fi

	if ! testit
	then
		log_msg "abort: variable name $name is likely not legal; did not pass shell source test."
		cleanup 1
	else
		verbose "variable name passed shell source test: ${name%%=*}"
	fi

	case $scope in
		local)
			prefix="pinkpages/$host:"
			;;
		cluster)
			prefix="pinkpages/$cluster:"
			;;
		flock)
			prefix="pinkpages/flock:"
			;;

		*)	log_msg "internal error; bad scope: $scope"; 
			cleanup 2
			;;
	esac

	# WARNING: do NOT eval anything with $stuff. Stuff may contain references to variables
	#	that we want to keep as references and not expansions.
	# 
	verbose "narbalek entry created/updated: $prefix$stuff"
	echo "set $prefix$stuff"  | ng_sendtcp -v -t $timeout -e "#end#" $nhost:$port  2>$TMP/PID$$.err
	rc=$?					# we only care about this status (comments can fail w/o bother)

	if (( $rc == 0 ))
	then
		if [[ -n ${stuff#*=} && -n $comment ]]		# update comment only if not deleting the value
		then
				echo "com $prefix$comment"  | ng_sendtcp -e "#end#" $nhost:$port 
		fi

		#add2cartulary "$stuff"
	else
		log_msg "unable to set var/val in narbalek: $stuff"
		cat $TMP/PID$$.err >&2
		#mv  $TMP/PID$$.err $TMP/log/ppset.$$.failed		# this should come out after testing
	fi

	cleanup $rc
else
	error_msg "unable to find cartulary/env variable: NG_NARBALEK_PORT"
fi


cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_ppset:Pink Pages - set a value)

&space
&synop	ng_ppset [-C cluster-override | -f | -l | -h local-host-override]  [-p port] [-s narbalek-host] [-v] var-name [value-tokens]

&space
&desc	&This registers a pinkpages variable with a global information map maintained
	by &lit(ng_narbalek). Variables maintained by &lit(narbalek) are key/value
	pairs with optional comments. Pinkpages variables, registered with &this,
	can be retrieved with &lit(ng_ppget,) and are 
	scoped on one three levels: flock, cluster, and node (local).  Variables
	scoped on the node level may be retrieved only by the node that 
	registered the value.  Cluster scoped name/value pairs may be retrieved 
	on any node in the cluster. Finally, any node with access to &ital(narbalek)
	may retrieve flock scoped values. 
&space
	To scope a variable, one of three command line options is used: -c, -l, and 
	-f.  If more than one of these options is supplied, the last one on the command 
	line (right most) overrides the others and is used.  If none of these options 
	is supplied, then &lit(-c) is assumed and the name/value is scoped to the current
	cluster. 

.if [ 0 ]
&space	
	There are a small number of pinkpages values that must be maintained as local,
	node specific, data items.  These values are kept in the file &lit(cartulary.local)
	within the &lit(site) directory.  &This can be used to update these local values
	by supplying the &lit(-l) (lowercase l) option.  When a local value is supplied, 
	it is added to the local file, and to the cartulary.  When &lit(ng_pp_build)
	recreates the cartulary from the pinkpages database, the local values are appended.
&space

&subcat	Backwards Compatability	
	When the pinkpages information was collected centrally on each cluster, a 
	variable with the name beginning &lit(FLOCK_) was automatically sent to 
	other clusters listed on the &lit(NG_FLOCK_MEMBERS) pinkpages variable. To 
	support scripts that set variables in this manner, variable names that begin 
	with &lit(FLOCK_) will be treated as if the &lit(-f) option were also set on 
	the command line, unless one of &lit(-C, -L,) or &lit(-l) have been set on 
	the command line. 

&subcat Flock Wide Variables
	Variables that are set with &this, and have a variable name which begins with
	&lit(FLOCK_) are automatically set (or reset) on all clusters defined in the 
	&lit(FLOCK_MEMBERS) pinkpages variable.  Propigation to the other clusters'
	pinkpages is immediate, though there will be some latency before the value 
	is updated into each node's cartulary on the remote clusters. If a variable 
	whose name begins with &lit(FLOCK_) is not to be propigated to remote clusters,
	then the &lit(-L) option can be supplied on the command line to prevent 
	propigation.
&space
	If the local to node command line option (-l) is specified, and the variable 
	name begins with &lit(FLOCK_), then no attempt to send the variable to 
	a remote cluster is made. 
.fi
	
	
	
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c string : The string is added as a comment to the data in pinkpages and is 
	reflected in the cartulary when &lit(ng_pp_build) creates the cartulary. &This 
	always adds the scope of the variable to the end of the comment, or as the 
	comment if the user does not provide a comment string.
&space
&term	-C cluster : Allows the cluster name for cluster scoped variables to be overridden.
	By default the culster value defaults to the current cluster; this option allows 
	variables to be set up before a cluster is actually implemented. 
&space
&term	-h host : Supplies the host name used when scoping a variable as a local variable.
	Defaults to the current host, this option allows the ability to preset a series 
	of pinkpages variables for a node before it is started. When set, the -l option 
	is assumed. 
&space
&term	-l : Local mode.  The name value information is formatted and placed only in 
	the local cartulary file in &lit($NG_ROOT/site).
&space
&term	-L : Deprecated and ignored.  Allows older &this scripts running in other clusters
	to send flock wide variables to the narbalek oriented clusters.
&space
&term	-p port : Allows &this to send the message to a narbalek process not listening
	on the standard port. 
&space
&term	-s host : Allows &this to send the message to a narbalek process not running on 
	the local host.
&space
&term	-T sec : Time out that &this will wait for a response from narbalek. If not supplied
	the default is 6 seconds. 
&space
&term	-v : Verbose mode.
&endterms


&space
&parms	A Minimum of one positional parameter, the &ital(variable name) must be 
	supplied on the command line.  All parameters that follow the name are assumed 
	to be the value that should be assigned to the name in the pink pages. 

&space
&exit	&This will exit with a zero (0) return code when it feels that all processing 
	has completed successfully, or as expected. A non-zero return code indicates 
	that there was an issue with either the parameters or data supplied by the user, 
	or &ital(pinkpages) could not be communicated with.

&space
&see	ng_ppget, ng_parrot, ng_pp_build, ng_bo_peep

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
&term	31 Aug 2004 (sd) : Conversion to work with narbalek.
&term	24 Sep 2004 (sd) : -h option automatically sets -l option if -l not supplied.
&term	17 Oct 2004 (sd) : prevent write to cartulary if setting for another host (-h) or cluster (-C).
&term	02 Feb 2006 (sd) : Pulled the support for FLOCK_* variables to automatically be scoped to the flock.
&term	22 Mar 2006 (sd) : Updated doc.
&term	19 Apr 2006 (sd) : Udated the manpage to correct a double -c option indication.
&term	31 May 2007 (sd) : Prevents sending a comment to narbalek if the value is being deleted. 
&term	10 Aug 2007 (sd) : Corrected error message that still referred to PINKPAGES_PORT. 
&term	05 Dec 2007 (sd) : Added better error reporting. 
&term	11 Nov 2008 (sd) : Adding the var=value pair immediately to the local cartuarly has been deprecated. 
&endterms

&scd_end
------------------------------------------------------------------ */
