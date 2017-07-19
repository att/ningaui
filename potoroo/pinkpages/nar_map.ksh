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
# Mnemonic:	nar_map.ksh
# Abstract:	present some kind of illustration of the state of the 
#		narbalek processes.
#		
# Date:		12 Jan 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# get a list of nodes from one ($1) or all of the nnsd processes; we sort and elim dupes
function get_node_list
{
	for x in ${1:-$nnsd_nodes}
	do
		# ! must be added to league var, not !$league because nar does not handle !<null>
		echo "0:list:$league"| ng_sendudp -q 1 -t 1 -r "." $x:${nnsd_port:-29056}
	done | awk '
		{
			x = split( $0, a, " " );
			for( i = 1; i <= x; i++ )
				print a[i];
		}
	' |sort -u|gre "${npat:-.*}"
}

# build a list of nodes that are currently active and known to nnsd
# and then sends a dump request to each. output is the raw dump data on stdout
function get_dump_info
{
	if [[ -n $input_file ]]		# user supplied static file 
	then
		cat $input_file
		return
	fi

	if (( ${force_multicast:-0} > 0 ))
	then
		echo "0:DUMP" | ng_sendudp  -m -r "." -t $timeout ${narbalek_group}:$narport 
	else
		get_node_list | while read x
		do
			if (( ${vcount:-0} > 3 ))
			then
				log_msg "sending dump request to: $x"
			fi
			echo "0:DUMP" | ng_sendudp  -q 1 -r "." -t 1 $x:$narport
		done
	fi
}

# dump the lists from each nnsd
function dump_nnsd_list
{
	for x in $NG_NNSD_NODES
	do
		get_node_list $x   >/tmp/nnsd_list.$$
		log_msg "reported by $x: "`wc -w </tmp/nnsd_list.$$`" nodes"
		awk ' { printf( "%s ", $1 ); } END { printf( "\n\n" ); }' </tmp/nnsd_list.$$
	done
}

# parse output from get_dump_info generating the desired data for user
function dump_info
{
	get_dump_info | awk \
	-v do_tree=$do_tree  \
	-v verbose=$vcount \
	-v do_state=$do_state \
	-v list_clusters="$list_clusters" \
	-v list_members="$list_members" \
	-v clusters="$clusters" \
	-v long_list=$long_list \
	'
	function get_val( field, 	what, a )
	{
		what = $(field); 
		split( what, a, "(" );

		if( field == parentf  || field == mef ) 		# must trash port if there (older narbaleks)
		{
			if( split( a[1], b, "." ) == 5 )
			 	return sprintf( "%d.%d.%d.%d", b[1], b[2], b[3], b[4] );
		}

		return a[1];
	}


	function print_chain( sid, tab, rsibs, isroot, 		blank, a, i, x )
	{
		if( depth++ > 25 )
			return;

		if( printed[sid] && sid != "ORPHANED" )
		{
			printf( "graph is circular through %s %s\n", sid, ip2name[sid] );
			#exit( 2 );
			return;
		}

		printed[sid]++;

		blank = 0;
		if( rsibs )			# siblings after this?
			mytab = tab;		# yep, just use tab string for our info
		else
			mytab = substr( tab, 1, length( tab ) -1 ) "|";	# nope, replace last blank with bar

		vstring = verbose > 0 || isroot ? (verbose > 1 ? sid " " version[sid] : version[sid]) : "";
		if( isroot )
			printf( "%s %s\n", ip2name[sid], vstring );
		else
			printf( "%s-%s %s %s\n", mytab, ip2name[sid], flags_text[sid],  vstring );

		printed[sid] = 1;

		if( children[sid] )
		{
			x = split( children[sid], a, " " );	
			for( i = 1; i <= x; i++ )
			{
				if( blank )
				{
					printf( "%s\n", tab (x >= 1 ? "    |" : "     ") );
				}

				mytab = i < x ? "    |" : "     ";
				blank = print_chain( a[i], tab mytab, x - i, 0 );

			}
			blank++;		# if we printed children then caller needs to blank
		}

		depth--;
		return blank;
	}
	
	#STATE: ce226d6a00fe072 bil13 135.207.249.83.29055(me) 135.207.249.73.29055(mom) ningdev(cluster) flock(league) 1(kids) 6(caste)
	function set_fields( 		i, a )
	{
		idf = 2;
		hostf = 3;
		for( i = 4; i <= NF; i++ )
		{
			if( index( $(i), "(me)" ) )
				mef = i;
			else
			if( index( $(i), "(mom)" ) )
				parentf = i;
			else
			if( index( $(i), "(cluster)" ) )
				clusterf = i;
			else
			if( index( $(i), "(league)" ) )
				leaguef = i;
			else
			if( index( $(i), "(kids)" ) )
				kidsf = i;
			else
			if( index( $(i), "(caste)" ) )
				castef = i;
			else
			if( index( $(i), "(flags)" ) )
				flags = i;
			else
			if( index( $(i), "(ver)" ) )
				verf = i;
		}
		#printf( "mef=%d momf=%d cluf=%d leagf=%d castef=%d verf=%d(%s)\n", mef, parentf, clusterf, leaguef, castef, verf, $(verf) ) >"/dev/fd/2";
	}

	/isolated.league/ { next; }
	/no.cluster.cluster/ { if( !do_tree) next; }		# old isolate left node able to join tree; ditch only if not printing tree

	/STATE:/ {
		if( do_state )
			print;

		#if( !hostf )
		if( $(NF) != last_nf )
			set_fields( );
		last_nf = $(NF);

		count++;
		host = $(hostf);
		host_ip = get_val( mef );
		if( ip2name[host_ip] )
		{
			ext++;
			host_ip = host_ip ext;
		}

		parent = get_val( parentf );
		cluster = get_val( clusterf );
		caste[host_ip] = get_val( castef );
		mama[host_ip] = parent;
		if( verf )
			version[host_ip] = get_val( verf );
		else
			version[host_ip] = "";

		if( flags )
		{
			f = (substr( $(flags), 3, 1 )+0) % 2;		# if odd, agent flag is on
			if( f )
				flags_text[host_ip] = "VIA_AGENT";
		}
		else
			flags_text[host_ip] = "";

		ip2name[host_ip] = host;
		name2ip[host] = host_ip;
		members[cluster] = members[cluster] host " ";

		if( cluster != "unknown" && !seen[cluster]++ )
			all_clusters = all_clusters cluster " ";

		if( parent == "I_AM_ROOT!" )
			root = host_ip;
		else
			children[parent] = children[parent] host_ip " ";

	}

	END {

		for( x in children )				# if the (mom) field was a name rather than an ip 
			if( x != "I_AM_ROOT!"  &&  x+0 == 0 )
			{
				n = split( children[x], a, " " );
				for( i = 1; i <= n; i++ )
					children[name2ip[x]] = children[name2ip[x]] " " a[i];
				children[x] = ""
			}

		if( verbose )
			printf( "state msgs: %d\n", count );

		ip2name["ORPHANED"] = "ORPHANED";
		printed["ORPHANED"] = 1;



		if( do_tree )
		{
			print_chain( root, "  ", 0, 1 );
			
			do
			{
				n = "";
				for( x in ip2name )
				{
					if( ! printed[x] && (n == "" || caste[x] < caste[n] ) )
						n = x;
				}

				if( n  )
				{
					if( ! ip2name[n] )		# likely the entry is here because its listed as a parent, but we nver saw it
						printed[n]++;		# fake it out so we move along
					else
					{
						if( !ip2name[mama[n]] && mama[n] )
						{
							printf( "parent alias might be interfering with accurate tree rendering %s = ", mama[n] );
							system( "ng_rcmd " mama[n] " ng_sysname" );
						}
						printf( "\ndisconnected: %s %s  parent = %s\n", n, ip2name[n], ip2name[mama[n]]  ? ip2name[mama[n]] : mama[n] )
						print_chain( n, " ", 0, 1 );
					}
				}
			}
			while( n );
		}

		if( list_clusters )
			printf( "%s\n", all_clusters );

		if( list_members )
		{
			if( clusters == "all " )
				x = split( all_clusters, a, " " )
			else
				x = split( clusters, a, " " )
			
			for( i = 1; i <= x; i++ )
			{
				if( long_list )
				{
					y = split( members[a[i]], b, " " );
					for( j = 1; j <= y; j++ )
						printf( "%s\n", b[j] );
				}
				else
					printf( "%s%s\n", x > 1 ? a[i] " = " : "", members[a[i]] );
				
			}
		}
	}
'
}


# -------------------------------------------------------------------
ustring="{-c | -m cluster | -N | -s | -t } [-L league] [-l] [-p port] [-T sec] [-v] [--nnsd_port port] [--nnsd_nodes node(s)]"

narbalek_group=${NG_NARBALEK_GROUP:-235.0.0.7}
narport=${NG_NARBALEK_PORT:-5555}
nnsd_port=$NG_NNSD_PORT
npat=
what=state
clusters=""
timeout=4
vcount=0
long_list=0;
leauge=""				# default to query of all leagues

if [[ -z $NG_NNSD_NODES  ]] || (( ${NG_NARMAP_FORCEMC:-0} > 0 ))
then
	force_multicast=1		# causes messages via multicast assuming no nnds running
else
	force_multicast=0		# causes messages to be sent via multicast rather than to list from nnsd
fi

do_tree=0		# the awk spits out things based on these (-t)
list_clusters=0		#(-c)
do_state=0		# (-s)	(default)
list_members=0		# (-m)
intput_file=""
league="!${NG_NARBALEK_LEAGUE:-flock}"		# bang is required as a nnsd separator

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	what=clusters;;
		-g)	narbalek_group=$2; shift;;
		-i)	input_file=$2; shift;;
		-l)	long_list=1;;
		-L)	league="!$2"; shift;;			# limit to nodes registered with this league
		-m)	what=members; clusters="$clusters$2 ";;
		-M)	force_multicast=1;;
		-N)	what=nnsd_list;;		# get list from each nnsd that is active
		-p) 	narport=$2; shift;;
		-P)	npat=$2; shift;;
		-t)	what=tree;;
		-s)	what=state;;
		-T)	timeout=$2; shift;;
		-v)	vcount=$(( $vcount + 1 ))
			verbose=1
			;;

		--nnsd_port)	nnsd_port=$2; shift;;
		--nnsd_nodes)	nnsd_nodes="$nnsd_nodes$2 "; shift;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

nnsd_nodes="${nnsd_nodes:-$NG_NNSD_NODES}"

rc=0
case $what in 
	tree)		echo "league=${league/!/}"; do_tree=1; dump_info; rc=$?;;
	clusters)	list_clusters=1; dump_info;;
	members)	list_members=1; dump_info;;
	state)		do_state=1; dump_info;;
	nnsd_list)	dump_nnsd_list; cleanup $?;;

	*)	log_msg "unrecognised action: $what";;
esac


cleanup $rc
exit



STATE: ce226d6a00fe072 bil13 135.207.249.83.29055(me) 135.207.249.73.29055(mom) ningdev(cluster) flock(league) 1(kids) 6(caste)
STATE: cb6ac59006e0908 bil09 135.207.249.79.29055(me) 135.207.249.211.29055(mom) bilby(cluster) flock(league) 1(kids) 6(caste)
STATE: cb6ab758043c44e bil06 135.207.249.76.29055(me) 135.207.249.26.29055(mom) bilby(cluster) flock(league) 1(kids) 6(caste)
STATE: cb6ac00e038b4e5 bil08 135.207.249.78.29055(me) 135.207.249.209.29055(mom) bilby(cluster) flock(league) 2(kids) 3(caste)
STATE: cbfef8aa04569fc bil10 135.207.249.80.29055(me) 135.207.249.210.29055(mom) unknown(cluster) flock(league) 1(kids) 6(caste)

STATE: 0c896a75813cc7b2 ningd0 135.207.42.240.5555(me) 135.207.42.241.5555(mom) ningdev(cluster) flock(league) 2(kids) 1(caste)
STATE: 0c896a75813e7d9b ningd4 135.207.42.244.5555(me) 135.207.42.242.5555(mom) ningdev(cluster) flock(league) 0(kids) 2(caste)
STATE: 0c896a75803d8d26 ningd3 135.207.42.243.5555(me) 135.207.42.242.5555(mom) ningdev(cluster) flock(league) 0(kids) 2(caste)
STATE: 0c896a7580a0e120 ningd1 135.207.42.241.5555(me) I_AM_ROOT!(mom) ningdev(cluster) flock(league) 2(kids) 0(caste)
STATE: 0c896b61c0d29999 fox00 135.207.249.40.5555(me) 135.207.42.240.5555(mom) flyingfox(cluster) flock(league) 0(kids) 2(caste)
STATE: 0c896a7580619c8c ningd2 135.207.42.242.5555(me) 135.207.42.241.5555(mom) ningdev(cluster) flock(league) 2(kids) 1(caste)
STATE: 0c896ae1006329fb ningd6 135.207.42.246.5555(me) 135.207.42.240.5555(mom) ningdev(cluster) flock(league) 0(kids) 2(caste)



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_map:Display useful information about Narbalek)

&space
&synop	ng_nar_map {-c | -m cluster | -N | -s | -t } [-L league] 
&break	[-l] [-p port] [-T sec] [-v] [--nnsd_nodes nodes] [--nnsd_port port]

&space
&desc	&This sends an status request to all narbaleks currently participating on the multicast group
	(NG_NARBALEK_GROUP) and waits for a short time collecting responses.  Based on comand line options, 
	the responses are formatted into one of:
&space
&beglist
&item	A representation of the narbalek communication tree.
&item	The raw status messages received.
&item	The list of members for the cluster(s) indicated.	
&item	The list of clusters.
&endlist

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Causes the list of clusters represented by narbalek processes to be displayed.  The list is 
	written as a single, space separated line of output.
&space
&term	 -g group : Override the &lit($NG_NARBALEK_GROUP) variable and supply a mcast group.
&space
&term	-L league : Quearies only nodes that have running processes that have registered with an ng_nnsd
	process using the named &ital(league). This is not necessary when quering the flock wide narbalek
	processes, but allows &this to be used to query ng_dptree based processes that are running on fewer
	than all nodes in the flock (makes things incredibly faster). 
&space
&term	 -l : Long listing.  For members, each node name is written to a separate line.
&space
&term	-m cluster : Lists the members belonging to the named cluster. If a host is down, or does not report 
	quickly enough (see timeout), then it will not be listed. 
	By default, the node names are written as a single output line onto standard output.  If more than one 
	cluster name is given (e.g. -m "bilby kowari"), then a single line for each cluster is written using the 
	syntax: &lit(cluster = node node node).  If the long listing option is also set, then one node per line is 
	written and the cluster name is omitted.
&space
&term	-N : Show nnsd list information.  Each narbalek name service daemon (nnsd) process that is defined in 
	the &lit(NG_NNSD_NODES) variable is querried.  The list of nodes known to each nnsd process is written
	to the standard output device. 
&space
&term	-p port : The port to use when communicating with narbalek.  NG_NARBALEK_PORT is used it not supplied.
&space
&term	-s : Display the raw status messages received (default).  These messages include the hostname, IP address of the 
	host, the IP address of the node's parent in the narbalek tree, the cluster it believes it belongs to, the league
	that the narbalek is using to determine tree affiliation, the number of children that are attached to the narbalek
	in the tree, and its caste in the tree.  The root of the tree, will list the string &lit(I_AM_ROOT!) instead of a 
	parent IP address (the mom address).
&space
&term	-t : A representation of the narbalek communication tree is written to standard output. In cases where nodes are 
	communicating via a multicast agent, their children will not be properly placed into the tree and will be shown as 
	'detached' tree segments.  
&space
&term	-T sec : Sets a timeout value in seconds.  This is the number of seconds that &this will wait for responses from 
	the narbalek processes. Only narbalek processes that respond duing this timeout period are examined, and this may 
	cause odd representations when listing in tree format. The default is 4 seconds if this option is not supplied.
&space
&term	-v : Verbose mode.

&space
&term	--nnsd_nodes node : One or more, space separated nodes that are running &lit(ng_nnsd.) If omitted, the 
	&lit(NG_NNSD_NODES) variable is used.
&space
&term	--nnsd_port port : The port that &lit(ng_nnsd) is listening on. If omitted, the value of &lit(NG_NNSD_PORT) 
	is used. 
&endterms


&space
&exit	Currently, the exit code is always zero.

&space
&see	ng_narbalek, ng_nar_set, ng_nar_get, ng_ppset, ng_ppget, ng_nnsd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Jan 2005 (sd) : Added doc and cleaned things up to make a production script.
&term	02 Feb 2005 (sd) : Added -g parm.
&term	20 Jun 2006 (sd) : Added -L option to supply the league of processes to query.
&term	24 Jul 2006 (sd) : Added better circular tree checking.
&term	04 Jun 2007 (sd) : Added --nnsd* options to make testing easier, and to support multiple nnsd groups.
&term	09 Jun 2007 (sd) : Fixed issue reading fields in stats message such that it adjusts based on the version number.
&term	15 Jul 2008 (sd) : Ignores isolated nodes (league==isolated).
&term	24 Jul 2008 (sd) : Added default to NG_NARBALEK_LEAGUE with a default of flock. .** HBD EKD/ESD
&term	15 Apr 2009 (sd) : Corrected ustring.
&endterms

&scd_end
------------------------------------------------------------------ */
