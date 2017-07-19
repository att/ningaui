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
# Mnemonic:	nar_get - get a value from narbalek given a name
# Abstract:	Makes a query for a narbalek value given a name. If no 
#		var is given, the table space is dumpped using the pattern
#		and substitutions given on the command line.
#
#		NOTE: This script MUST be able to run only on the info in 
#		lib/cartulary.min --- its needed before the cartulary is built
#		
# Date:		29 August 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

if [[ -f $NG_ROOT/lib/cartulary.min ]]
then
	. $NG_ROOT/lib/cartulary.min
else
	echo "panic:  cannot find minimal cartulary -- trying real one, but this is not good!" >&2
	. ${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
fi

if [[ -f $NG_ROOT/site/cartulary.min ]]
then
	. $NG_ROOT/site/cartulary.min
fi

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# -------------------------------------------------------------------
host=localhost
port=${NG_NARBALEK_PORT:-29055}
ustring="[-b] [-l] [-P name-pattern] [-p port] [-s host] name=\"value\""

me="$(ng_sysname)"
pat=""
subs=""
boot_req=0
info_req=0
timeout=5
retry=1
get_cmd="Get"		# by default the value will NOT have $name references expanded
leased=0

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	boot_req=1;;			# a boot strap request -- narbalek likely not running on our node yet
		-e)	get_cmd="get";;			# return values expanding $name things
		-i)	info_req=1;;			# get info from the host
		-o)	retry=0;;

		-l)	leased=1;;			# variable is a leased value -- we strip the lease data for the caller

		-s)	host=$2; shift;;
		-p)	port=$2; shift;;

		-P)	pat="$2"; shift;;
		-S)	get_cmd="get"; subs=":$2"; shift;;		#  assume its a name=value pair (get causes expansion)

		-t)	timeout=$2; shift;;
		-v)	verbose=1;;

		-X)	get_cmd="X";;				# a testing command
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ $boot_req -gt 0 ]] 		# if in bootstrap mode, send a multicast and use a host that responds 
then
	# dump stuff returned from narbaleks looks like:
	#STATE: 0c8a146ea0245026 bat00 135.207.249.10.5555(me) 135.207.249.73.5555(mom) numbat(cluster) flock(league) 2(kids) 8(caste)
	ng_nar_map | awk -v me=$me -v cluster=$NG_CLUSTER '		# get a dump from all listening narbaleks
		BEGIN {
			aidx = 0;		
			fidx = 0;
		}
		{
			if( $3 == me )			# we cannot be a bootstrap node.
				next;

			split( $6, a, "(" );
			if(  a[1] == cluster )
				avail[aidx++] = $3;
			else
				fallback[fidx++] = $3;
		}

		END {
			srand();
			if( aidx )
			{
				r = int((1000 * rand( )) % aidx);
				printf( "%s\n", avail[r] );
			}
			else
			{
				if( fidx )
				{
					r = int((1000 * rand( )) % fidx);
					printf( "%s\n", fallback[r] );
				}
			}

			#printf( "cluster=%s n=%s aidx=%d fidx=%d r=%d\n", cluster, n, aidx, fidx, r ) >"/dev/fd/2"
		}
	'| read host

	if [[ $1 = "bootstrap_node" ]]		# user just wanted to get the name of a boot-strap host
	then
		echo $host
		cleanup 0
	fi
	verbose "bootstrap mode: using $host to get information"
fi

if [[ $info_req -gt 0 ]]		# user wants info from the host
then
	printf "D\n" | ng_sendtcp -e "#end#" $host:$port 
	cleanup $?
fi


pat="${subs:-:}:$pat"		# must be :sub:pattern or ::pattern

status=0
if [[ -z $1 ]]		# no var name to get, then dump all that match the pattern (.* by default)
then
	printf "dump$pat\n"  | ng_sendtcp -e "#end#" $host:$port | awk '
		BEGIN { found = 0; }
		/^#.*end of table/ { found = 1; exit(0) }
		/^\#ERROR\#/ { exit( 1 ); }
		{ print; }
		END { exit( found ? 0 : 1 ); }
	'
	status=$?
else
	#printf "$get_cmd $1\n"  | ng_sendtcp -e "#end#" $host:$port |awk '/^\#ERROR\#/ { exit( 1 ); } { print; }'
	printf "$get_cmd $1\n"  | ng_sendtcp -e "#end#" $host:$port |awk -v leased=$leased '/^\#ERROR\#/ { exit( 1 ); } 
			{ 
				if( leased  && (x=index( $0, ":" )) > 0 ) 
					print substr( $0, x+1 );  
				else 
					print $0; 
			}'
	status=$?
	if (( $status != 0 && $retry > 0 ))
	then
		verbose "unstable table error from narbalek; pausing 20s then retrying"
		sleep 20
		printf "$get_cmd $1\n"  | ng_sendtcp -e "#end#" $host:$port |awk -v leased=$leased '/^\#ERROR\#/ { exit( 1 ); } 
			{ 
				if( leased  && (x=index( $0, ":" )) > 0 ) 
					print substr( $0, x+1 );  
				else 
					print $0; 
			}'
		status=$?
	fi
fi

cleanup $status



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_set:set a name/value pair in  a narbelek information map)

&space
&synop	ng_nar_get [-b] [-e] [-i] [-l] [-o] [-s host] [-p port] name[|name...]
&break
	ng_nar_get [-b] [-P pattern] [-S subinfo] 
&break
	ng_nar_get -b bootstrap_node

&space
&desc	
	&This acts as an interface to &lit(ng_narbalek) allowing the user to
&space
&beglist
&item	Get the value associated with a name in the &ital(narbalek) namespace
&item	Dump some or all of the &ital(narbalek) information map
&item	Get the name of another host with a running &ital(narbalek)
&item	Request status information from &ital(narbalek)
&endlist

&space
	In the command's first form, the variable &ital(name) is looked up 
	and its value written to standard output.  If more than one names 
	are given, they must be separated with vertical bar characters (|).
	The behaviour of &this when multiple names are provided is to write 
	the first non-empty value located to standard output. Names are 
	tried from left to right as they appear on the command line. 

&space
	When no names are supplied on the command line, some or all of the 
	&ital(narbalek) information map is dumpped to the standard output 
	device. Each record generated has the format:
&space
&litblkb
   name="value" [#comment]
&litblke
&space
	If the pattern option (-P) is supplied, then only variable names 
	which match &ital(pattern) (a regular expression) are dumpped, rather
	than dumping the whole information map.
&space
	The -b option causes &this to search for a running &ital(narbalek) on 
	another node. It may be set for any form of the command, but if the variable 
	name supplied is &lit(bootstrap_node) then the node name of the node 
	with the running &ital(narbalek) is written to the standard output device
	rather than sending that node's &ital(narbalek) a command. (This is 
	useful when multiple commands need to be sent to a remote narbalek -- it 
	is more efficent to discover a remote host once, than for each command).
&space

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Bootstrap mode. A query of all running narbalek processes is made and a
	random node is selected which will be used for the request rather than the 
	defaulting to the narbalek on this node. The node returned will be from the 
	same cluster unless no narbalek processes on the cluster respond. 
	This is slower, but allows ng_init to boostrap (to get the 
	cluster name for the node) before narbalek is started on the local node. 
	If the variable name supplied on the command line 
	is &lit(bootstrap_host), then the hostname selected is written 
	to the standard output and no value is fetched.  
	This allows the caller to get an alternate node; one with an active narbalek 
	process in the event that narbalek on the current node has not been started. 
&space
&term	-e : Expand variables.  As values are returned by &ital(narbalek), any $name 
	references are expanded.  The assumption is made that &lit($name) is defined in 
	the &ital(narbalek) information map and if not the expansion will be a null string.
&space
&term	-l : The variable is to be considred a leased variable (see ng_nar_get) and will 
	have the lease information stripped.  Lease information is considered to be all data
	from the start of the value up to and including the first colon.  Generally it is 
	of the form: <timestamp>/<pid>:. 
&space
&term	-o : One attempt.  Normally, if an unstable table error is deteced when searching 
	for a single variable, a second attempt to fetch the value is made after a short
	pause.  If this option is set, then an error is returned immediately.  An unstble
	table error is generated if narbalek is in the process if reloading its table 
	and thus cannot determine the value of the variable requested. 
&space
&term	-p port : Defines the port to communicate with &lit(ng_narbalek) if it is not 
	listening on &lit($NG_NARBALEK_PORT).
&space
&term	-s host : Allows a narbalek process running on a different host to be querried.
&space
&term	-P pattern : Allows the user to supply a regular expression pattern which is 
	used when dumping the table.  Only variables with names that match the pattern 
	will be written to standard output.
&space
&term	-S subs : When dumping the table, a name/value pair can be substituted for
	and will be expanded in all statements that are dumpped. Format of &ital(subs)
	is name=value. This allows a variable, such as NG_ROOT, to be redefined 
	and replaced in all occurances found in the information map values as the 
	information is written.  If this option is supplied, expansion is automatically
	turned on (as though the -e option were used).
&endterms


&space
&parms	Optionally, one positional parameter is expected on the command line. If supplied, 
	the parameter is taken to be the name of a variable to search for in &ital(narbalek)
	and whose value will be written to standard output. If the parameter is a 
	vertical bar (|) separated series of variable names, each variable in turn (working 
	left to right as they appear on the command line) is searched for. The resulting 
	output is the first non-empty value that is found.  
&space
	The process of a multiple name search is supported by &ital(narbalek) and thus 
	it is much more efficent to pass multiple names than to invoke &this until a 
	value is returned. 

&space
&exit	An exit code of zero inidcates that processing completed successfully. When 
	a dump is requested, a zero return code inidcates that the end of information 
	marker was received and the record(s) written to standard output are complete.
	Unless a return code of 0 is indicated, any data written to the standard output 
	device should be discarded and treated as incomplete. 


&space
&see	ng_nar_put, ng_narbalek, ng_ppput, ng_ppget, ng_pp_build

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	29 Aug 2004 (sd) : Hot off the skillet.
&term	07 Sep 2004 (sd) : Added get-info (-i) option.
&term	20 Sep 2004 (sd) : Improved doc.
&term	02 Mar 2005 (sd) : Now exits with a legit return code after a get command.
&term	31 May 2005 (sd) : Updated man page to correct typos.
&term	21 Aug 2005 (sd) : Added support to trap ERROR from narbalek which indicates table not stable.
&term	15 Feb 2006 (sd) : Added initialisation of var found in awk, and explicit 0 on exit in same awk.
&term	13 Apr 2007 (sd) : Fixed bootstrap node generation. 
&term	15 Jan 2008 (sd) : Removed misplaced diagnostic to stderr.
&term	30 Jul 2008 (sd) : Corrected pulling in of just minimal cartulary files so that both lib and site versions
		are sourced in the proper order.
&term	08 Jun 2009 (sd) : Added -l (leased var) option.
&endterms

&scd_end
------------------------------------------------------------------ */
