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
# Mnemonic:	nar_set -- set a name/var pair in narbalek
# Abstract:	will insert the name/value pair into a narbalek map
#		
# Date:		
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# -------------------------------------------------------------------
host=localhost
port=${NG_NARBALEK_PORT:-9999}
ustring="[-l seconds] [-s host] [-p port] {-a affiliated-cluster | name=\"value\"}"

set_aff=""
ncmd="set "			# set command (trailing space is important)
cond=""				# condtion for a conditional set
cond_sep_ch=";"			# chr that sets off the condition in name;cond;=value
nar_verbose=""
send_exit=0
expiry=""			# -l sets as a leased variable
fast=0

now=$(ng_dt -i)
while [[ $1 = -* ]]
do
	case $1 in 
		-a)	set_aff="$2"; shift;;		# set the affiliation of narbalek with an 'organisation'  (cluster)

		-c)	cond="$cond_sep_ch${2}$cond_sep_ch"; 			# condition that must be true in order to set value
			ncmd="i"			#nar command is an if set
			shift
			;;

		-C)	ncmd="com ";;			# value is comment (blank is important)

		-e)	send_exit=1;;

		-f)	fast=1;;			# do not pause before checking a leased variable

		-l)	leased=1			# leased variable
			expiry=$2; shift
			cond=";n<$now;"		# set the var only if the lease has expired or var is unset
			ncmd="i"
			;;
		-p)	port=$2; shift;;

		-s)	host=$2; shift;;


		-V)	nar_verbose=$2; shift;;
		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -n $set_aff  ]]
then
	verbose "setting the affiliation to: $set_aff"
	printf "affiliate $set_aff\n" | ng_sendtcp -e "#end#" $host:$port 
	cleanup $?
fi


if [[ -n $nar_verbose ]]
then
	printf "verbose $nar_verbose\n" | ng_sendtcp -e "#end#" $host:$port 
	cleanup $?
fi

if [[ $send_exit -gt 0 ]]
then
	verbose "sending exit"
	printf "xit4360\n" | ng_sendtcp -e "#end#" $host:$port 
	cleanup $?
fi

if [[ -z $1 ]]
then
	log_msg "missing command line parms"
	usage
	cleanup 1
fi

if [[ $1 = *=* ]]
then
	name="${1%%=*}"		# assume name="value" or "name=value"
	value="${1#*=}"
else
	name="$1";
	shift;
	value="$@";		# assume name value value ...
fi

if [[ -n $expiry ]]
then
	expiry="$(( $now + ($expiry * 10) ))/$$:"		# convert to tenths of seconds and add to current time
fi

verbose "sending: $ncmd$name$cond=$expiry$value"  
echo "$ncmd$name$cond=$expiry$value"  | ng_sendtcp -e "#end#" $host:$port 

if [[ -n $expiry ]]		# if setting a leased variable, then we must see if we got it
then
	if (( $fast < 1 ))
	then
		sleep 1
	fi

	ng_nar_get -s $host -p $port  $name | read cvalue
	if [[ $cvalue != $expiry$value ]]
	then
		verbose "did not override lease; variable ($name) not changed (got=$cvalue expected=$expiry$value)"
		cleanup 1
	else
		verbose "leased variable set successfully"
		cleanup 0
	fi
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_set:set a name/value pair in  a narbelek information map)

&space
&synop	ng_nar_set [[-f] -l seconds] [-s host] [-p port] name="value"
&break	ng_nar_set [-s host] [-p port] -a cluster

&space
&desc	&This is the interface to &ital(narbalek) used to set a name value 
	pair.   The cluster affiliation that &ital(narbalek) uses can also 
	be set with &this. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a name : Sets the cluster affiliation in &ital(narbalek).  Narbalek can be started
	before the node's cluster name is known, and it is wise to affiliate a running 
	&ital(narbalek) with a cluster.  This option is used to communicate the affiliation
	to &ital(narbalek).  
&space
&term	-f : Fast check mode.  When the leased variable option is used, the check to determine
	if the variable was successfully assigned is made after a small pause. If this option is
	given, the pause is skipped.
&space
&term	-l seconds : The variable is treated as a leased variable and can only be updated/removed
	if the lease has expired.  The &ital(seconds) value supplied is the length of the new lease
	given to the variable if the value can be changed.  
&space
&term	-s host : Supplies the host to communicate with.  If not supplied, the local host
	is assumed. 
&space
&term	-p port : Overrides the well known port number that &ital(narbalek) typically uses. 
	If not supplied, the value is assumed to be in the cartulary variable &lit(NG_NARBALEK_PORT).
&space
&term	-V level : Sent a verbose level change to narbalek on this host. 
&endterms


&space
&parms	After options are processed, all remaining command line tokens are assumed to be 
	parameters. The first is assumed to be the name followed by value tokens. The 
	first token may also be of the form &lit(name=token) (see examples). The name
	may contain any ASCII character &stress(except) the equal sign (=).
&space
	Variable naming conventions are not enforced by &ital(narbalek), but should be 
	agreed upon and adhered to by all &ital(narbalek) users to prevent 
	collisions. The suggested format is:
&space
&litblkb
   <programme>[/qualifier]:variablename
&litblke
	Where
&begterms
&term	programme : Is the name of the script or programme that is creating the 
	variable (e.g. pinkpages).
&space
&term	qualifier : Is a qualifier that the programme establihsed to further divide the 
	namespace. 
&space
&term	variablename : Is the name of the variable as it might be known in the cartulary
	or other namespace outside of &ital(narbalek).
&endterms
 
&space
&examp
	The following illustrate the use of this interface to &ital(narbalek):
&space
&litblkb
  ng_nar_set  pinkpages/flock:NG_SENESCHAL_PORT=29004
  ng_nar_set  pinkpages/flock:NG_SENESCHAL_PORT 29004
&litblke

&space
&exit	When the lease (-l) option is given, the exit value is 0 (good) if narbalek accepted the 
	value and chenged the contents associated with the variable.  If the current lease on the 
	variable had not expired, and thus the value was not changed, the return code is set to 1.

&space
	If the lease option is not given, a zero return code indicates that the process was successful.
	Any other exit code indicates error.

&space
&see	ng_narbalek, ng_nar_get, ng_ppset, ng_ppget

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2004 (sd) : Fresh from the oven.
&term	 22 Jan 2005 (sd) : Corrected error if no parms given, now writes usage.
&term	08 Jun 2009 (sd) : Added support for leased variables. 
&endterms

&scd_end
------------------------------------------------------------------ */
