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
# Mnemonic:	ng_get_nattr
# Abstract:	get the node attributes for a named machine
# parms:	node -- if omitted, then assumed to be the local node
#		
# Date:		
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# write em -- trash lead/trail/multi blanks, comma sep if needed 
function put_out
{

	echo $1 | sed 's/^ * //; s/ * $//;' | read data
	if (( $publish > 0 ))
	then
		echo $1 | sed 's/^ * //; s/ * $//;' | read data
		ts=$(( $(ng_dt -i) + 3000 ))
		method="ng_get_nattr -P $me"
		ng_ppset -l NG_CUR_NATTR="$ts~$method~$data"
	fi

	if (( $show_data > 0 ))		# if publish only not set
	then
		if (( $comma_sep > 0 ))
		then
			data="${data// /,}"
	#		echo $1 | sed 's/^ * //; s/ * $//; s/ * /,/g;'	| read data
		fi
		echo $data
	fi
}
	

# we must now get srv_ things directly from pinkpages/narbalek to avoid stale information 
# when service election changes things.
# see which srv_* things are set to our node
function get_service_list
{
	ng_ppget | awk -v me="$(ng_sysname)" -v tname=${TMP:-/tmp}/x.$$ '
        /^srv_/ {
                gsub( "\"", "", $0 );		# things come out var="val" and we dont want quotes
                gsub( "#.*", "", $0 ); 		# probably not needed, but it cant hurt
                split( $1, a, "=" );

		if( substr( a[2], 1, 1 ) == "@" )		# srv_Junk=@foo  -- get value of $foo and see if that is me
		{
			system( "ng_wrapper -e "substr( a[2], 2 )" >" tname );
			getline a[2] < 	tname; 
			close( tname );
		}

		if( (i = index( a[2], ":" )) > 0 )
			a[2] = substr( a[2], 1, i-1 );		# allow srv_Foo=ningd3:stuff where we ignore :stuff

		if( a[2] == me )
			slist = slist substr( a[1], 5 ) " ";	# save just the service name (not srv_)
        }

	END {
		print slist
	}
	'
}

# -------------------------------------------------------------------
ustring="[-c] [-p] [-P] [-s] [-v] [nodename]"
me=`ng_sysname -s`
node=$me
srv_only=0
opts=""
comma_sep=0
timeout=10
publish=0
show_data=1
cat_list=0;

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	opts="$opts-c "; comma_sep=1;;
		-l)	cat_list=1;;
		-p)	opts="$opts-p "; publish=1;;
		-P)	opts="$opts-P "; publish=1; show_data=0;;		# publish only 
		-s)	opts="$opts-s "; srv_only=1;;
		-t)	timeout=$2; shift;;
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

node=${1:-$node}		# if user entered a node name



if (( $publish > 0  && $srv_only < 1 ))			# as long as we are not publishing the answer, check cached value
then
	ng_ppget -h $node NG_CUR_NATTR | IFS="~" read ts method data		# current published node attributes
	if [[ -n ts ]]
	then
		now=$( ng_dt -i )
		if (( $now < ${ts:-0} ))			# data is stil valid
		then
			put_out "$data"
			cleanup 0
		else
			verbose "cached data expired; building new"
			#opts="$opts-p "					# in case it is off node (set this only after we are sure this change is EVERYWHERE
			publish=1
		fi
	fi
fi

if [[ $node != $me ]]
then
	ng_rcmd -t $timeout $node ng_get_nattr $opts 2>/dev/null
	cleanup 0
fi



# we assume that all srv_ indicators are in the environment from cartulary
# if one matches our host, then we add the name sans the srv_
# we allow the value to be node:stuff and return true if node is ours.


get_service_list | read list

# old way -- expensive from a process count perspective 
#ng_ppget|grep "^srv_" | sed 's/"//g; s/#.*$//' | while read s		# avoid things like FLOCK_srv_xxx
#do
#	#eval what=\$$s
#	echo $s | IFS='=' read var val
#	if [[ $val = "@"* ]]			# srv_xxxx=@foo -- we get value from $foo
#	then
#		eval val=\"\$${val#?}\"
#	fi
#
#	if [[ ${val%%:*} = $me ]]
#	then
#		list="$list ${var##srv_}"
#	fi
#done

if (( $srv_only > 0 ))			# this will do if they just want server stuff
then
	put_out "$list"
	cleanup 0
fi
	
ng_uname -s |read system junk
ng_uname -r |IFS="." read maj min junk
ng_uname -p | read arch

dlist="$NG_NATTR"		# default list assigned in cartulary.nodename

if (( $cat_list > 0 ))		# list attributes with type (service, perm, temp)
then
	printf "%10s %s\n" "current:" "$dlist"
	printf "%10s %s\n" "permanent:" "$NG_DEF_NATTR"
	printf "%10s %s\n" "system:" "$system $system$maj.${min%%-*}"
	printf "%10s %s\n" "service:" "$list"
	cleanup 0
fi

# REQUIREMENT: the system.x.y  attribute MUST be last in the list.  Some scripts depend on this.
put_out "$dlist $list $system ${system}$maj.${min%%-*}"  	 # freebsd has form n.m-type vs n.m.p


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_get_nattr:Get node attributes)

&space
&synop	ng_get_nattr [-c] [-p] [-P] [-s] [nodename]

&space
&desc	&This will echo to the standard output all of the attributes that 
	are assocaited with a node.  This list is constructed based on 
	the &lit(NG_NATTR) cartulary variable that should be set in the 
	lit(cartulary.)&ital(nodename) file, and the service name (e.g. Jobber)
	of any &lit(srv_) cartulary variable whose value is the current node
	(or matches the node name passed on the command line).  
	The value of &lit(NG_NATTR) set in the node's cartulary file is augmented
	during system initialisation to include attributes related to hardware 
	components that migh change between system resets. 
	The operating system type is also added to the attribute list during the 
	initialisation.
&space
	The attributes are given for the node on which the command is executed
	unless a node name is provided on the command line. The following is a list
	of attributes that are determined either when &this is invoked, or during 
	system initialisation and should not be set in the node's cartulary file:
&space
&begterms
&term	Cpu_rich : The node has more than three CPUs installed.
&space
&term	Darwin : The node is running a version of the Mac OS X operating system.
&space
&term	FreeBSD : The node is running a version of the FreeBSD operating system.
&space
&term	Leader : The node is acting as the cluster leader.  This attribute is set 
	for a node if the &lit(srv_Leader) cartulary (pinkpages) variable as the 
	nodename as its value.
&space
&term	Jobber : The node is running cluster job management daemons (Seneschal and 
	Nawab). The attribute is based on the &lit(srv_Jobber) cartulary variable.
&space
&term	Lgmem : The node has more than 2G of memory installed.
&space
&term	Linux : The node is running a version of the Linux operating system.
&space
.** logger is deprecated
.** &term	Logger : Log combination services are executed on the node that has 
.**u	set the &lit(srv_Logger) cartulary variable and is thus given this attribute.
&space
&term	Netif : The node is acting as the external network interface host. The 
	attribute is determined by the setting of &lit(srv_Netif) in the cartulary.
&space
&term	Sqfs : One or more squatter filesystems exist on the node. 
&space
&term	SunOS : The node is running a version of the Solaris operating system.
&space
&term	Tape : The node has physcial tape access.
&term	
	
&endterms
&space
	Attributes that are expected to be assigned in the node's cartulary variable
	include:
&space
&begterms
&term	Rmro : Node is a read-only replication manager node. 
&space
&term	Satellite : The node is designated as a remote node and does not participate 
	in normal cluster elections, and might indicate an added expense associated 
	with transferring files to or from the node. 
&endterms


&opts	These options are recognised when placed on the command line:
&begterms
&term	-c : Comma separate the list of attributes.
&space
&term	-l : List the attributes by catigory.  The current node attributes are listed 
	in the catigories: current, permanent, system, service.  The current attributes are 
	those that are manually set using the &lit(ng_add_nattr) command and any attributes that 
	were set by &lit(ng_init) and/or &lit(ng_site_prep) based on hardware attributes (tape, media/library changer
	squatter filesystem(s) etc). 
	The permenant list are those manually set attributes that survive a node stop.  
	System attributes are the attributes based on the operating system, and service attributes are the attribues 
	given to the node based on the current services that the node hosts. 
&space
&term	-p : The data echoed to the standard output device is also published in pinkpages. 
		The published data is of the form <timestamp>~<method>~<data>. Timestamp
		is the time after which the pubished data is considered stale. Method 
		is the command that is needed to republish the data.
&space
&term	-P : Same as -p, except no data is written to the standard output device. 
&space
&term	-s : Servers only.  The list of attributes will be based soley on the &lit(srv_) 
	pinkpages variables that have a value for the current node. 
&space
&term	-t sec : Supply a timeout in seconds. If the request for attributes to the node is 
	not received during the timeout period, a response is assumed not to be coming. If 
	not supplied, a default ot 10 seconds is used.
&endterms

&space
&parms	A single, optional, command line parameter is recognised. If present, this is assumed to 
	be the name of the node where a request for its attributes is sent. If omitted,
	the attributes for the current node are emitted.

&space
&exit	Always zero.

&space
&see	ng_add_nattr, ng_del_nattr, ng_species, ng_seneschal, ng_nawab

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Oct 2003 (sd) : The first.
&term	04 Dec 2003 (sd) : Updated doc to add new nattr funcitons.
&term	20 Dec 2003 (sd) : Added a timeout of 10 seconds.
&term	12 Mar 2004 (sd) : Added a system with version attribute
&term	25 Mar 2004 (sd) : Added -s option.
&term	10 May 2004 (sd) : Added -c option; and it strips lead/trail/multiple blanks.
&term	30 Jul 2004 (sd) : Added note inicating that the ostype.x.y attribute is now required to be 
	the last in the list as some scripts depend on that. 
&term   18 Aug 2004 (sd) : Converted srv_Leader references to corect srv_ things.
&term	14 Jan 2005 (sd) : Added timeout -t parm.
&term	25 Mar 2005 (sd) : Mod to get srv_* directly from narbalek/pinkpages rather than depending 
	on cartulary which might be stale. 
&term	06 Apr 2005 (sd) : O/S attribute (last) now carries arch info (e.g. -i386).
&term	19 Jul 2005 (sd) : Turned off -arch as get_likeme and shovit deal with the arch type themselves.
&term	22 Jul 2005 (sd) : Corrected typo in the call to put_out.
&term	29 Nov 1005 (sd) : Force usage of uname via full path to combat suse linux and a buggered builtin 
	in either ksh or sh. 
&term	16 Dec 2005 (sd) : Convert to ng_uname.
&term	26 May 2006 (sd) : now caches info in pinkpages, added -p/P options
&term	25 Jul 2006 (sd) : Changed the assureance of TMP being set.
&term	08 Aug 2006 (sd) : Corrected issue that allowed attributes to be saved in ppages with commas.
&term	20 Mar 2007 (sd) : Pulled reference to Logger from the man page. 
&term	20 Sep 2008 (sd) : Added -l option. 
&endterms

&scd_end
------------------------------------------------------------------ */
