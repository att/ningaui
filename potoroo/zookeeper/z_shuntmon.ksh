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
# Mnemonic:	shunt_mon - shunt stats monitor
# Abstract:	Will peek at the master log and generate interesting stats about 
#		shunt file movement. if -V, then generate zookeeper view update info
#		and send to parrot on -h host.  -p prefix is needed to get the view 
#		name right if running in -V mode
#		
# Date:		2 Aug 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


export PATH="$PATH:$HOME/bin"

if [[ $@ = *-D* ]]
then
	log_msg "detaching and writing log to: $NG_ROOT/site/shuntmon"
	detach_log=$TMP/$LOGNAME.shuntmon
	$detach
	exec >$NG_ROOT/site/z_shuntmon 2>&1
	echo "started: `date`"
fi

# -------------------------------------------------------------------
export NG_WREQ_QUIET=1		#prevent wreq from issuing log messages

me=`ng_sysname`

ustring="[-D] [-F] [-f logfile] [-h host] [-l] [-L sec ] [-n] [-p prefix] [-r yyyymmddhhmmss-yyyymmddhhmmss | -s seconds] [-c tail-count] [-V] [-v]"
count=22000
sec=3600		# default to the most recent hour
limit_measure=0
genv=0
prefix=${me%%[0-9]*}
delay=0
phost=${srv_Leader:-no_leader_found}
bcast=1
log=$NG_ROOT/site/log/master
sort_opt="-k 3,3"			# sort on to
node=".*"

while [[ $1 = -* ]]
do
	case $1 in 
		-D)	;;			# ignore for detach 
		-a)	showall=1;;		# for views 
		-c) 	count=$2; shift;;
		-f)	log=$2; shift;;
		-F)	sort_opt="-k 1,1";;
		-h)	phost=$2; shift;;
		-n)	bcast=0;;
		-N)	node="$2"; shift;;
		-p)	prefix=$2; shift;;
		-l)	limit_measure=1;;
		-L)	delay=$2; shift;;
		-s)	sec=$2; shift;;
		-r)	count=0; range="$2"; shift;;
		-v)	verbose=1;;
		-V)	genv=1; sec=300;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ $count -eq 0 ]]
then
	dump_log="cat"
else
	dump_log="tail -$count "
fi

mbrs="$(ng_members -R)"
while true
do
	headerf=/tmp/header.$$
	>$headerf 

	ng_dt -i |read now

	$dump_log $log | awk \
	-v mbrs="$mbrs" \
	-v showall=${showall:-0} \
	-v prefix=$prefix \
	-v genv=$genv \
	-v verbose=$verbose \
	-v sec=$sec \
	-v now="$now" \
	-v limit_measure=$limit_measure \
	-v range="$range" \
	-v headerf="$headerf" \
	'
	function cmd( c, 	buf )		# execute a command and return resuts
	{
		while( (c|getline) > 0 )
		{
			buf = sprintf( "%s%s ", buf, $0 );
		}
		close( c );

		gsub( " $", "", buf );
		return buf;
	}

	BEGIN {
		if( genv )
			verbose = 0;

		stop = now+10;

		if( range )
		{
			x = split( range, a, "-" );
			start = cmd( "ng_dt -i " a[1] ) + 0;
			if( x > 1 )
				stop = cmd( "ng_dt -i " a[2] ) + 0;
		}
		else
			start = now - (sec*10);		# only look at the last n seconds (converted to tenths) 

		#printf( "start=%.0f stop=%.0f\n", start, stop )>"/dev/fd/2";

		meg = 1024 * 1024;

		if( verbose )
			node_header = "From -> To";
		else
			node_header = "Destination";

		nnodes = split( mbrs, node_list, " " );
		for( i =1; i <= nnodes; i++ )
		{
			norder[node_list[i]] = i;		# ability to compare node[a] < node[b]
			suf[node_list[i]] = substr( node_list[i], length( node_list[i] ) -1 );  	# last 2 chr
		}
	}

	function s2dhm( tsec, 		d, h, m )		# tenths of seconds to dayhourminute
	{
		if( tsec < 0 )
			return " ";		# can be - if we never see a start msg in the log 

		tsec /= 10;			# convert to seconds
		d = int(tsec / 86400);
		if( d > 180 || d < 0 )
			return " ";		# likely a missing lead date

		tsec -= d * 86400;
		h = int(tsec / 3600);
		tsec -= h * 3600;
		m = tsec / 60;

		if( d )
			return sprintf( "%dd%02dh%02dm", d, h, m );

		return sprintf( "%02dh%02dm", h, m );
	}
	
	function mkvname( f, t,		a, b )
	{
		gsub( "-g", "", f );
		gsub( "-g", "", t );

		if( norder[f] < norder[t] )
			return sprintf( "shunt_%s_ln%s%s", prefix, suf[f], suf[t] );
		else
			return sprintf( "shunt_%s_ln%s%s", prefix, suf[t], suf[f] );
	}

	/ng_shunt.*stats:/ {
		if( $1+0 < start || $1+0 > stop )
			next;

		if( !first_msg )
			first_msg = sprintf( "%s %s", $1, start);
		last_msg = $1;

		if( ! first )
			first = $1+0;

		from = $6;
		to = $8;
		split( to, a, ":" );
		tonode = a[1];
		fromnode = $3;
		gsub( "-g", "", fromnode );		# we dont care about -g interface
		gsub( "-g", "", tonode );

		if( !genv && verbose )
			node = fromnode " -> " tonode;
		else
		{
			if( genv )
			{
				if( norder[fromnode] && norder[tonode] )
				#if( substr( fromnode, 1, length( prefix ) ) != prefix ||
				#	substr( tonode, 1, length( prefix ) ) != prefix )
				#	next;
					node = mkvname( fromnode, tonode );
				else
					next;
			}
			else
				node = tonode;
		}

		bytes = $(NF-2)+0;
		if( ! (time = $(NF-1)/10 ) )
			time = .1;

		if( index( $0, "0(rc)" ) )
		{
			if( bytes > (meg * 2 ) || limit_measure < 1 )
			{
				allmeg += bytes/meg;
				allsec += time;
				allgood++;

				totmeg[node] += bytes/meg;
				totsec[node] += time;

			}
			successto[node]++;
		}
		else
		{
			failedto[node]++;
			allbad++;
		}
	}

	function ts2date( ts,	aa, b, a )	# ningaui timestamp to a pretty date string
	{
		split( ts, aa, "[" );
		a = aa[2];
		b = sprintf( "%s/%s/%s %s:%s:%s", substr( a, 1, 4 ), substr( a, 5, 2 ), substr( a, 7, 2 ), substr( a, 9, 2 ), substr( a, 11, 2 ), substr( a, 13, 2 ) );
		return b;
	}

	END {
		if( genv )
		{
			for( i = 1; i < nnodes; i++ )
			{
				for( j = i+1; j <= nnodes; j++ )
				{
					n = sprintf( "shunt_%s_ln%s%s", prefix, suf[node_list[i]], suf[node_list[j]] );
					successto[n] = 1;			# forces a print of all lines into the view
				}
			}

			moved = 0;
			for( x in successto )
			{
				m = totmeg[x];
				moved += m;
			
				if( m > 200 )
					colour = 8;	# yellow
				else
				if( m > 100 )
					colour = 0;	# white
				else
				if( m > 50 )
					colour = 6;	# green
				else
				if( m > 25 )
					colour = 9;	# purple 
				else
				if( m > 10 )
					colour = 5;	# blue
				#else
				#if( m && allmeg < 1000  )	# flag all links that had any traffic when traffic is light
					#colour = 18;
				else
					colour = showall ? 18 : -1;

				printf( "RECOLOUR %s %d\n", x, colour );
			}
			printf( "RECOLOUR shunt_%s_moved %d\n", prefix, moved );
			exit( 0 );
		}
		
		if( range )
		{
			printf( "first: %s   last: %s   elapsed: %s\n", ts2date( first_msg ), ts2date( last_msg ), s2dhm( last_msg - first_msg ) ) >>headerf;
		}
		else
			printf( "Transfers during the last %.1f minutes\n", (now-first)/600 ) >>headerf;

		printf( "%-20s %4s %4s %9s %9s\n", node_header, "OK", "Bad", "MB", "MB/s" ) >>headerf;
		for( x in successto )
		{
			s = totsec[x] ? totsec[x] : 1;
			printf( "%-20s %4d %4d %9.2f %9.2f\n", x, successto[x], failedto[x], totmeg[x], totmeg[x]/s );
			seen[x]++;
		}

		for( x in failedto )
			if( ! seen[x] )
				printf( "%-20s %4d %4d %9.2f %9.2f\n", x, successto[x], failedto[x], 0, 0 );

		printf( "%-20s %4d %4d %9.2f %9.2f\n", "Totals:", allgood, allbad, allmeg, allmeg/allsec+1 ) >>headerf;
			
	}
	' |sort $sort_opt >/tmp/shunt_mon.$$


	if [[ $genv -gt 0 && $bcast -gt 0 ]]
	then
		i=0
		while read x
		do
			if [[ $i -gt 25 ]]		# pause so as not to overrun udp
			then
				sleep 1;
				i=0
			else
				i=$(( $i + 1 ))
			fi

			echo $x
		done </tmp/shunt_mon.$$ | ng_sendudp $phost:$NG_PARROT_PORT
			
		#ng_sendudp $phost:$NG_PARROT_PORT </tmp/shunt_mon.$$
	else
		cat $headerf
		gre "$node" /tmp/shunt_mon.$$
	fi

	if [[ $delay -lt 1 ]]
	then
		cleanup 0
	else
		sleep $delay
	fi
done

cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_z_shuntmon:Zookeeper shunt datamovement monitor)

&space
&synop	ng_z_shuntmon.ksh [-D] [-F] [-f logfile] [-l] [-L sec ] [-r yyyymmddhhmmss-yyyymmddhhmmss | -s seconds] [-c tail-count] [-V] [-v]

&space
&desc	&This reads through the last bit of the master log and sumarises the 
	shunt transfer messages producing a by node list of transfers including
	a megabyte per second rate value.  If The verbose mode is provided on 
	the command line, then the output shows from/to node pairs and the 
	statistics for transfers between the nodes.   The various command line 
	options control the number of master log lines that are examined, and/or
	a time range for which the stats are generated.

&space
	If the view option (-V) is given, then zookeeper view update messages 
	are generated and sent to the &lit(ng_parrot) process on the indicated 
	host for distribution to all registered zookeeper processes. To support
	a constant zookeeper feed, &this can be run as a daemon. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-D :	Run in detached mode. The script will put itself into the background.
&space
&term	-a :	Show all lines regardless of activity. Ignored unless view generation flag is set.
&space
&term	-c n : 	Supplies the most recent &ital(n) number of master log lines to read.  The default 
	is 22,000. On clusters where the master log is updated at a rate greater than 22,000 messsages
	per hour, this option may need to be used in order to generate statistics for the last 
	hour.  If &lit(n) is set to 0, then the entire master log is examined.
&space
&term	-f filename : Supplies the name of the log file to read. If omitted the master log is assumed.
&space
&term	-F :	Sort output based on the from node. If not supplied, the 'to' node is used as the 
	sort key (when in verbose mode).
&space
&term	-h host :	Send zookeeper view update data to the &lit(ng_parrot) process running on &ital(host).
&space
&term	-n :	Do not broadcast zookeeper info (mostly for testing).
&space
&term	-N node : Limit output to stats just for the node (can be a pattern)
&space
&term	-p :	The node name prefix (e.g. ning, bat). Needed only when generating zookeeper view information.
&space
&term	-l :	Limits the stats to transferrs of files >= 2M.
&space
&term	-L sec :	Sets the loop delay when run as a daemon.
&space
&term	-s sec :	Examines only the last &ital(sec) seconds from the master log. If not supplied, 
	messages from the most recent 3600 seconds (1 hour) are processes.
&space
&term	-r start-end : Indicates the range of log messages over which the stats are collected. &ital(Start)
	and &ital(end) are both timestamps in the form &lit(yyyymmddhhmmss).
&space
&term	-v :	Verbose mode. 
&space
&term	-V :	Zookeeper view interface mode.  Generates and brodadcasts information to an ng_parrot 
	process for forwarding to zookeeper monitors.
&endterms


&space
&parms	No command line positional parameters are expected.

&space
&exit	Zero to inidicate all is well; non-zero to indicate error.

&space
&see	ng_parrot, ng_zoo

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Aug 2003 (sd) : Thin end of the wedge. (revised from original shunt_mon)
&term	28 Jul 1004 (sd) : Added SCD, and made a production tool.
&term	28 Aug 2006 (sd) : changed name for release.
&term	17 Mar 2008 (sd) : Now gets a list of all registered nodes, not just active ones.
&endterms

&scd_end
------------------------------------------------------------------ */
