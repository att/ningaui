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
# Mnemonic:	err_stats
# Abstract:	collect error stats for a given cluster
#		
# Date:		23 Aug 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# parse the log fragment snarfed from the remote machine

function parse
{
	if [[ -n $filter && ! -f $filter ]]
	then
		echo "cannot find filter: $filter"
		cleanup 1
	fi

  	awk \
		-v show_zeros=$show_zeros \
		-v table=$filter \
		-v keep_warn=$keep_warn \
		-v msgf=$msgf \
	'
   	BEGIN {
	i = 0                    # required - forces i numeric
	while( table && getline < table )
	{
		if( NF > 1 && (substr( $1, 1, 1 ) != "#")   )
		{
			split( $0, a, "|" );
			pat[i] = a[1];
			lab[i++] = a[2];
		}
	}
  
	tsize = i - keep_warn;

	crit = 2;
	error = 3
	warn = 4;
	info = 6;
	debug = 7;
	type2str[0] = "unk0";
	type2str[1] = "ALRM";
	type2str[crit] = "CRIT";
	type2str[error] = " ERR";
	type2str[warn] = "WARN";
	type2str[5] = "NOTE";
	type2str[info] = "INFO";
	
   }
  
	function hrtime( what )
	{
		split( what, a, "[" );
		return sprintf( "%4s/%2s/%2s %02d:%02d:%02d", substr( a[2], 1, 4 ), substr( a[2], 5, 2 ), substr( a[2], 7, 2 ), substr( a[2], 9, 2 ), substr( a[2], 11, 2 ), substr( a[2], 13, 2 ) );
  
	}

   {
	if( ! first_msg )
		first_msg = hrtime( $1 );
	lastts = lastone;
	lastone = $1;

	node = $3;
	type = $2+0;

	if( type > info )
		next;

	typecount[type]++;

	count[node,type]++;
	seen[node]++;

    	s = tolower( $0 )
 
	for( i = 0; i < tsize && ! (xxx=match( s, pat[i] )); i++ ) ;

	if( type != crit && i < tsize )			# matched, weed it out; always print crits
    		weedcount[i]++
   	else
	{
		ptotal++;
     		printf( "%s\n", $0 ) >>msgf;
	}
    }
  
   END {
		printf( "--------------------------------------------------------------\n\n" );
		printf( "First message timestamp: %s\n", first_msg );
		printf( "Last message timestamp:  %s\n\n", hrtime(lastts) );

		printf( "\n%10s ", "NODE" );
		for( i = 0; i < debug; i++ )
			printf( " %8s", type2str[i] );
		printf( "\n" );
		
		for( n in seen )
		{
			printf( "%10s ", n );
			for( i = 0; i < info+1; i++ )
				printf( " %8d", count[n,i] );
			printf( "\n" );		
		}
		printf( "%10s  ", "TOTALS" );
		for( i = 0; i < debug; i++ )
			printf( "%8d ", typecount[i] );

		printf( "\n" );
		printf( "\nRecognised message summary:\n" );
		for( i = 0; i < tsize; i++ )
		{
			if( lab[i] != "skip" && (weedcount[i] || show_zeros) )
				printf( "\t%40s: %7d\n", lab[i], weedcount[i] )
			total += weedcount[i];
		}

		if( total )
			printf( "\n\t%40s: %7d\n", "Total filtered", total );
		printf( "\t%40s: %7d\n", "Total printed", ptotal );
		printf( "\t%40s: %7d\n", "Grand Total", total + ptotal );
		
  }
  ' <$1
}

# -------------------------------------------------------------------

ustring="[-c cluster] [-e] [-f] [-h host] [-k] [-p] [s] [-w window] [-v]"
window="-d $(( 60 * 60 * 24 ))"
ng_sysname |read myhost
cluster=$NG_CLUSTER
host=""
keep_warn=1;			# keep warning messages; -e turns off
filter=$NG_ROOT/lib/log_stats.tab		# message filter table
keep=0				# do not keep the output 
split=0				# -s sets to split output file
msgf="/dev/fd/1"
parse_stdin=0

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster=$2; shift;;
		-e)	keep_warn=0;;
		-f)	filter=$2; shift;;
		-h)	host=$2; shift;;
		-k)	keep=1;;
		-p)	parse_stdin=1;;
		-r)	;;					# ignored for back compat
		-s)	split=1;;				# split output into two files
		-w)	window=$2; 
			shift
			if [[ $window == *"-"* ]]		# yymmddhhmmss-yymmddhhmmss range entered
			then
				window="${window/-/ }"
			else
				window="-d $window"		# assume number of seconds immediately preceeding the current time
			fi
			;;

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

if (( $parse_stdin > 0 ))
then
	msgf=/tmp/$USER.msg
	parse /dev/fd/0
	cleanup 0
fi

date +%a|read dow
tmp=$TMP
gfn=$tmp/log_stats.$LOGNAME.$cluster		# where we expect the master log extract file to be
if (( $split > 0 ))
then
	sumf=$gfn.$dow.sum
	msgf=$gfn.$dow.msg
else
	sumf=$gfn.$dow
	msgf=$sumf
fi

rm -f $gfn
rm -f $sumf $msgf

verbose "extracting data from master log"
if [[ -n $host ]]		# specific host requested
then
	ng_wreq -s $host job: pri 99 cmd "ng_glog $window >$gfn 2>/dev/null; ng_ccp -d $gfn $myhost $gfn 2>/dev/null"
	verbose "waiting on $gfn to be sent back from $host"
else
	if [[ -n $cluster && $cluster != $NG_CLUSTER ]]		# on another cluster
	then
		ng_wreq -s $cluster job: pri 99 cmd "ng_glog $window >$gfn.dump 2>/dev/null; ng_ccp -d $gfn $myhost $gfn.dump 2>/dev/null"
		verbose "waiting on $gfn to be sent back from $cluster (c)"
	else
		ng_glog $window >$gfn
	fi
fi

timeout=60					# wait just 5 minutes for the data
while [[ ! -r $gfn ]] && (( $timeout > 0 ))	# file unreadable until shunt is finished with it
do
	sleep 5
	timeout=$(( $timeout - 1 ))
done
if [[ ! -r $gfn ]]		# file unreadable until shunt is finished with it
then
	log_msg "gave up waiting on $gfn; not there or not readable"
	cleanup 1
fi

verbose "found file, reading: $gfn" 
(
	echo "Summary generated: `ng_dt -p`" 
	echo ""
	echo "Unfiltered log messages ---------------------------------" >>$msgf
	parse $gfn
) >$sumf 

if [[ $keep -gt 0 ]]
then
	for x in $msgf $sumf
	do
		if [[ -f $x ]]
		then
			verbose "created output: $x"
		fi
	done

else
	cat $gfn.$dow
	rm $gfn.$dow
fi

cleanup 0

exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log_stats:Fetch or process a log segment to generate some stats)

&space
&synop	ng_log_stats [-c cluster] [-h host] [-e] [-f filter] [-s] [-w {seconds|yyyymmddhhmmss-yyyymmddhhmmss}] [-v]

&space
&desc	
	&This causes an extract of the current master log to be created and then 
	sumarises the messages in the log.  The table in $NG_ROOT/lib/log_stats.tab is used
	as a pattern file for summary data.  Messages that match the pattern(s) supplied
	are summarised; messages that do not match the pattern are written to the generated
	output file.
&space
	Informational messages are always supressed and sumarised, even without a matching 
	pattern. Conversely, critical messages are always written to the output file and not 
	sumarised. 

&space
&subcat	Filtering Messages
	In general there are a small number of messages that need to be dealt with, but
	their presence need be made known in the form of a count rather than specific
	messages.  For this reason &this expects to find a filter file in 
	&lit(NG_ROOT)/lib/log_stats.tab) which is used to recognise messages that should
	be filtered. The file may contain zero or more records which have the format
	<pattern>|<text> where <pattern> is used to match log messages (using regular
	expression matching), and text is the message that is written along with the 
	count of messages matching the pattern. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : Log messages from the named cluster will be reported on. If not 
	supplied, messages from the current cluster are parsed.
	This option has no effect if the cluster name given matches the node's cluster
	as defined in NG_CLUSTER.  If the messages on a specific node in the current 
	cluster are to be parsed, the -h option must be used, or &this must be executed
	directly on the desired node. 

&space
	When this option is given, 
	the master log file on the node that answers to the DNS name &ital(cluter) is 
	used, unless the -h option is also supplied. If the -h option is supplied 
	the cluster name is used only in the name of the output file(s) and the host name
	supplied to the -h flag is used as the supplier of the master log. This allows 
	for the case where there is no host that answers to the DNS name that matches 
	&ital(cluster.)  

&space
&term	-e : Errors only.  Warning messages will be surpressed from the analysis.

&space
&term	-f file : Provides the filename of a filter file.  The filter file indicates 
	the message types that should be sumarised and listed at the end of the 
	report.

&term	-h host : Indicates the host to read the master log from.  Allows multiple 
	clusters' logs to be analysed from a single host. If not supplied, the cluster 
	name supplied with the -c option is used to request the extract from the master 
	log. If neither the -h nor the -c options are supplied, the master log on the 
	local node is used. 
&space
&term	-k : Keep output files rather than catting them to the standard output device. 
	Files are placed in the &lit($TMP) directory
	with the filename &lit(logstats.)&ital(user.cluster.dow) where &ital(dow) is the day 
	of the week that the file was produced on.  Using the day of the week as a part of the 
	filename  allows a rolling set of status summaries to be produced automatically for multiple clusters.
&space
&term	-p : Parse standard input.  Allows a master log file to be supplied directly 
	to &this for parsing.  When this option is supplied, &this makes no attempt
	to retriev any messages from any host. 
&space
&term	-s : Split the output.  By default, unfiltered log messages are written to the 
	same output file as the summary. If this option is supplied, the messages are 
	split to a seperate file: $TMP/log_stats.<userid>.<cluster>.<day>.msg

&space
&term	-w window : Supplies a window that is used to snarf log entries.  
	&ital(Window) may be supplied as seconds, or a range of yymmddhhmmss pairs separated
	with a dash (e.g. 20060203000000-20060203235959). If seconds are supplied, then 
	the window is assumed to be the number of seconds immediately preceeding the current
	time.

&space
&term	-v : Verbose mode; indicates what it is doing as it processes things.

&endterms

&space
&see	ng_logd

&bugs	It is likely that this will not work from a satellite node where the NG_ROOT
	value differs from from the cluster.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Aug 2003 (sd) : Thin end of the wedge. (HBD RH)
&term	26 Apr 2005 (sd) : Moved table to lib rather than adm directory.
&term	04 Feb 2006 (sd) : Modified -w option to allow seconds or yymmddhhmmss-yymmddhhmmss range.
		Revampped things to make them work better.
&term	12 Feb 2006 (sd) : Fixed hang waiting for data -- now correctly times out.
&term	05 Jul 2006 (sd) : Changed some of the headers on the generated HTML output.
&term	21 Jul 2006 (sd) : Fixed problem with running on the netif host.
&term	27 Mar 2007 (sd) : Fixed reference to NG_ROOT/adm in man page. 
&term	04 Sep 2008 (sd) : Added -k option and refined how the -c option affects processing. 
&endterms

&scd_end
------------------------------------------------------------------ */
