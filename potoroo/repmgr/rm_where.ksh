#!/ningaui/bin/ksh
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


#!/ningaui/bin/ksh
# Mnemonic:	rm_where - locate a file on a node
# Abstract: 	Attempts to determine where a file or pattern of files really 
#		live. List of information written to stdout.
# Returns: 	Nothing
# Date:		29 Aug 2001
# Author:	E. Scott Daniels
# ==============================================================================


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

MSG_LOG_FILE="/dev/fd/2"		# ensure verbose messages go to stderr

function usage				# overrides stdfun
{
	cat <<endcat >>$MSG_LOG_FILE
	usage:
	$argv0 [-a] [-j joinfile|-d dumpfile] [-v] [-5] [-E] [-s] [-f] node file1 [file2 ... filen]
	$argv0 [-a] [-j joinfile|-d dumpfile] [-v] [-5] [-E] [-s] [-S] -p node pattern1 [pattern2 ... pattern_n]
	$argv0 [-a] [-j joinfile|-d dumpfile] [-v] [-5] [-E] [-s] [-S] -p -n   pattern1 [pattern2 ... pattern_n]
	$argv0 [-a] [-j joinfile|-d dumpfile] [-v] [-5] [-E] [-s] [-f] {-c |-n} file1 [file2 ... filen]
endcat
}

# wait up to $refresh seconds for a dump to appear in $dumpf or for it to be updated
function wait_for_dump
{
	typeset marker="/tmp/marker.$$"
	touch $marker

	verbose "waiting up to $refresh seconds for a new dump file in $dumpf"
	while [[  $marker -nt $dumpf && $refresh -gt 0 ]]	# this works even if $dumpf is missing
	do
		sleep 1
		refresh=$(( $refresh - 1 ))
	done

	rm -f $marker
	verbose "$refresh seconds left on refresh timer"
}

# search the local rmdb for the file/pattern listed on the command line
# we generate a bunch of dump1 requests and parse through them
function local_search
{
	typeset tmp="/$TMP/list.$$"
	typeset gstring=""
	typeset phead=""			# if not pattern then we add lead / and trail blank
	typeset ptail=""

	flist="$@"				# if not pattern, gen info in this order
	if (( $pat <= 0 ))
	then
		phead="/"
		ptail=" "
	fi

	if (( $esc_specials > 0 ))		# escape special characters in the pattern 
	then
		echo "$@" | sed ' s/[+]/[+]/g; s/[.]/[.]/g; s/[*]/[*]/g; s/[\^]/[ytak62030]/g; s/[\$]/[$]/g; ' |read gstring
		set ${gstring:-pattern did not translate}
	fi

	gstring=""

	while [[ -n $1 ]]
	do
		if [[ -z $gstring ]]
		then
			gstring="$phead$1$ptail"
		else
			gstring="$gstring|$phead$1$ptail"
		fi

		shift
	done

	verbose "gre pattern: ${gstring//ytak62030/\\^}" 
	verbose "waiting for dump1 to process..."


	verbose "(local) searching with pattern: (${gstring//ytak62030/\\^})"
	ng_rm_db -p |gre "${gstring//ytak62030/\\^}" | awk '{ x=split( $2, a, "/" ); printf( "dump1 %s\n", a[x] ); }' | sort -u -k 2,2 >$tmp

	filer=${srv_Filer:-junk}
	ng_rmreq -t 600 -a -s $filer <$tmp | awk \
	-v me=$thishost \
	-v verbose=$vcount \
	-v all=$all \
	-v do_size=$size \
	-v do_md5=$md5 \
	-v pat=$pat \
	-v flist="$flist" \
	-v full=$full \
	'

	function vinfo( label, 		aa, x, i, b )
	{
		if( verbose > 1 )
		{
			x = split( $0, aa, "#" );
			printf( "%s: %s ", label, aa[1] ) >"/dev/fd/2";
			for( i = 2; i <= x; i++ )
			{
				split( aa[i], b, " " );
				printf( "%s ", b[1] )>"/dev/fd/2";
			}

			printf( "\n" )>"/dev/fd/2";
		}
	}

	function getval( what, 		a )
	{
		split( what, a, "=" );
		return a[2];
	}

	function basename( what, 	x, a )
	{
		x = split( what, a, "/" );
		return a[x];
	}

	BEGIN {
		not_here = 0;
		stable = 0;
	}

	verbose > 2 { print "raw input: " $0; }
	/stable=0/ && all > 0 && verbose > 0 { printf( "WARNING: unstable file: %s\n", $2 " " $3 ) >"/dev/fd/2" }

	/stable=1/ || all > 0 {
		x = split( $0, a, "#" );
		for( i = 2; i <= x; i++ )
		{
			if( index( a[i], me " " ) == 1 )		# this one is here
			{
				stable++;
				size = md5 = "";
				if( do_size )
					size = " " getval( $4 );
				if( do_md5 )
					md5 = " " getval( $3 );
					
				split( a[i], b, " " );
				if( pat )					# pattern given -- haphazard output right now
					printf( "%s %s%s%s\n", b[1], b[2], md5, size );
				else						# not a pattern  search, output in order given, so must save
					stats[basename(b[2])] = sprintf( "%s%s%s",  b[2], md5, size );   # abs names given, save til end
					
				break;
			}
		}

		if( i > x )
		{
			vinfo( "!here" );
			not_here++;		# listed in rmdb, not a current reg, must be unattached
		}
		
		next;
	}

	/not found:/ {
		vinfo( "!here" );
		not_here++;
		next;
	}

	{ 
		unstable++; 
		vinfo( "unstable" );
		next; 
	}

	END {
		missing = 0;
		found = 0;
		eidx = 0;
		if( ! pat )	# hard pathname(s) given, output in order
		{
			not_here = 0;				# reset -- we dont care how many in the rm_db list were not here 
			x = split( flist, a, " " );		
			for( i = 1; i <= x; i++ )		# for each asked for
			{
				bn = basename( a[i] );
				if( stats[bn] == "" )
				{
					not_here++;
					err[eidx++] = bn;
				}
				else
					found++;

				missing = full ? "MISSING(" bn ")" : "MISSING";
				printf( "%s\n", stats[bn] ? stats[bn] : missing );		# print in order asked for
			}

			if( not_here )
			{
				printf( "ERROR: paths found (%d) != number requested (%d)\n", found, x )>>"/dev/fd/2";
				for( i = 0; i < eidx; i++ )
					printf( "ERROR: %s\n", err[i] )>>"/dev/fd/2";
				exit( 1 );
			}
		}
		else
			if( verbose )
				printf( "totals:  rm_db=%d stable=%d unstable=%d  unattached=%d\n", NR, stable, unstable, not_here ) >"/dev/fd/2";

		exit( 0 );
	}
	'	# end

	return $?
}


#-------------------------------------------------------------------------------

thishost=`ng_sysname`
filer="${srv_Filer:-no-filer}"

cmdline="$@"			# incase we have to send to filer for info

all=0			# print all records in local match even if not stable
refresh=0		# set to sec to wait for refresh with -r seconds
verbose=0
list_nodes=0		# set (-n) to list all nodes that have the file
count=0			# set (-c) to generate count of nodes that have the file
exist=0			# set (-e) to just see if the file is registered (fast?)
full=0
pat=0			# pos parameters contain a pattern of files to match rather than a list
md5=0			# set to 1 (-5) and md5 is printed with filename on pattern listing
size=0			# set to 1 (-s) and file size is printed with filename on pattern listing
no_dump=0		# set to 1 (-N) to use existing dumpfile if there 
short_list=0		# for -p option; generates basenames with node list only. set with -S
dumpf=""		# dump file supplied by user; if not we will default to $RM_DUMP, but must be set late
join=ng_rm_join		# joins are expensive, we accept dump file as a join file and change this to cat
vcount=0
esc_specials=0		# escape special characters in the pattern if set (-e)

while [[ $1 = -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 1;;
		-5)	md5=1;;
		-a)	all=1;;
		-c)	count=1;;
		-d)	dumpf=$2; shift;;
		-e)	exist=1;;
		-E)	esc_specials=1;;			# escape special characters in the pattern if set
		-f)	full=1;;
		-j)	dumpf=$2; join=cat; shift;;		# user made a join file, we just need to cat later
		-n)	list_nodes=1;;
		-N)	no_dump=1;;
		-p)	pat=1;;
		-s)	size=1;;
		-S)	short_list=1;;
		-r)	refresh=$2; shift;;
		-r*)	refresh="${1#-?}";;
		-v) 	verbose=1; vcount=$(( $vcount + 1 ));;
		-\?)	usage; cleanup 1;;

		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1;
			;;
	esac

	shift
done


if (( $(( $count + $pat )) > 1 ))
then
	error_msg "count (-c) and pattern (-p) options cannot be used together"
	usage
	cleanup 1
fi

if (( $(( $exist + $pat )) > 1 ))
then
	error_msg "exist (-e) and pattern (-p) options cannot be used together"
	usage
	cleanup 1
fi

if (( $(( $count + $list_nodes )) > 1 ))
then
	error_msg "count (-c) and list_nodes (-n) options cannot be used together"
	usage
	cleanup 1
fi

if (( $(( $exist + $list_nodes )) > 1 ))
then
	error_msg "exist (-e) and list_nodes (-n) options cannot be used together"
	usage
	cleanup 1
fi

if (( $(( $count +  $list_nodes + $exist )) == 0 ))		# none of these option selected; assume $1 is node name
then
	if [[ -z $1 ]]			# must have node filename
	then			
		error_msg "bad parameters (all missing)"
		usage
		cleanup 1
	fi

	node=$1
	shift

	if [[ -z $1 ]]			# must have a file name/pattern to work with
	then			
		error_msg "bad parameters (some missing)"
		usage
		cleanup 1
	fi 
fi
# --------------------- end parm error checking -------------------

if [[ $node = $thishost ]]		# user asked for info on this node, go to the rmdb here not rmdump for info
then
	local_search "$@"
	cleanup $?
fi

	# the requirement that we are not filer can be lifted when we are sure all nodes run THIS copy of rm_where
if [[ -n $node  && $thishost != $filer ]]	# specific node, but not ours, and we are not filer, send to that node
then
	ng_rcmd $node "ng_rm_where $cmdline"
	cleanup $?
fi

if [[ $thishost != $filer && ! -f ${dumpf:-nosuchfile} ]]		# not a local search, send to filer if we are not
then
        set foo $cmdline                # foo prevents set from taking our parm as its parm
        shift
        opts=""
        while [[ $1 = -* ]]		# pick up options that are passed on each xargs invocation of rcmd
        do
                opts="$opts$1 "
                shift
        done

        cmdline="$*"			# args to go to xargs

        #	ast xargs is broken with -s
        if ! echo $cmdline | xargs  ng_rcmd -t 300 $filer ng_rm_where $opts		# rcmd buffer is ~4k
        then
		cleanup 1
	fi

	cleanup 0			# get out now.
fi

dumpf=${dumpf:-$RM_DUMP}		# if user did not supply, we set default but it MUST be after the previous section

# we must be the filer, or the user supplied a dump/join file to parse

if [[ $refresh -gt 0 ]]			# user asked to wait, do it
then
	wait_for_dump
fi

if [[ ! -f $dumpf ]]				
then

	log_msg "cannot find dump file: $dumpf"
	cleanup 1
fi

gstring=""
rc=0			# assume a happy ending (return code)



# we assume that the ng_rm_join command will put out one rec per file with the syntax:
# file <basename>: md5=<cksum> size=n  noccur=[-]n token=n matched=n#<node> <fullpath>[#<node> <fullpath>...]
if [[ $pat -gt 0 ]]			# gstring contains the patterns, let them match and go!
then					# find all files (basenames) that match the pattern and dump info on them
	if [[ -n $node ]]
	then
		node_pat="#$node "
	else
		node_pat=""
	fi

	if (( $esc_specials > 0 ))		# escape special characters in the pattern 
	then
		echo "$@" | sed ' s/[+]/[+]/g; s/[.]/[.]/g; s/[*]/[*]/g; s/[\^]/[ytak62030]/g; s/[\$]/[$]/g; ' |read gstring
		set ${gstring:-pattern did not translate}
	fi

	unset gstring

	for x in "$@"			# make the grep string using each filename/pattern
	do
		x=${x##*/}			# we think only in basenames for searching
		if [[ -n $gstring ]]
		then
			gstring="$gstring|"
		fi
	
		if [[ $x = "^"* ]]		# set up for beginning of basename
		then
			leadin=""
			x=${x#?}
		else
			leadin='.*'
		fi

		if [[ $x = *'$' ]]		# set up for end of basename
		then
			trail=":"
			x=${x%?}
		else
			trail=""
		fi
		
		#gstring="$gstring^file .*$x.* md5=.*$node_pat"		# build grep string from names/patterns on command line
		gstring="$gstring^file $leadin$x$trail.* md5=.*$node_pat"		# build grep string from names/patterns on command line
	done
	
	verbose "(pat/remote) searching with pattern: (${gstring//ytak62030/\\^})"

	$join <$dumpf | gre "${gstring//ytak62030/\\^}" | awk \
		-v size=$size \
		-v md5=$md5 \
		-v node="$node" \
		-v short_list=$short_list \
		-v all=$all \
		-v verbose=$vcount \
		-v full=$full \
		' 
		function snarf( what,		i )
		{
			split( what, b, " " );
			for( i = 1; i <= NF; i++ )
				if( split( b[i], a, "=" ) == 2 )
				{
					values[a[1]] = a[2];
				}
		}

		BEGIN {
			eidx = 0;
		}

		verbose > 2 { print "raw input: " $0; }

		/stable=0/ && all > 0 && verbose > 0 { printf( "WARNING: unstable file: %s\n", $2 " " $3 ) >"/dev/fd/2"; }

		/stable=1/ || all > 0 {
			x = split( $0, subrec, "#" );
			split( subrec[1], toks, " " );					# file basename:
			toks[2] = substr( toks[2], 1, length( toks[2] )-1 );		# chop : from the end 
			snarf( subrec[1] );						# get things like MD5=
			if( short_list )		# dump basename nodelist md5..... similar to non -p operation
			{
				list = "";
				for( i = 2; i <= x; i++ )
				{
					split( subrec[i], stuff, " " );			# nodename fullpathname
					if( stuff[1] " " stuff[2] != "attempt to" && stuff[1] != "hint:" )
					{
						if( list )
							list = list ",";
						list = list stuff[1];	
					}
				}
				if( list == "" || list == " " )
					list = "none";			# not on any nodes at the moment 

				printf( "%s %s%s%s\n", toks[2], list, md5 ? " " values["md5"] : "", size ? " " values["size"] : "" );
			}
			else						# long list is a list of full pathnames
				for( i = 2; i <= x; i++ )
				{
					split( subrec[i], stuff, " " );			# nodename fullpathname
					if( stuff[1] " " stuff[2] != "attempt to" && (stuff[1] == node || node == "") && stuff[1] != "hint:" )
						printf( "%s %s%s%s\n", stuff[1], stuff[2], md5 ? " " values["md5"] : "", size ? " " values["size"] : "" );
				}
		}
	'
	cleanup $?
fi

order="$*"				# preserve files as listed on command line for ordering

if (( $esc_specials > 0 ))		# escape special characters in the pattern 
then
	echo "$@" | sed ' s/[+]/[+]/g; s/[.]/[.]/g; s/[*]/[*]/g; s/[\^]/[ytak62030]/g; s/[\$]/[$]/g; ' |read gstring
	set ${gstring:-pattern did not translate}
fi

unset gstring
for x in "$@"		# make the grep string differently  when searching for fixed names
do
	if [[ -n $gstring ]]
	then
		gstring="$gstring|file ${x##*/}:"
	else
		gstring="file ${x##*/}:"
	fi
done


verbose "(fixed/remote) searching with string: (${gstring//ytak62030/\\^})"

if (( $exist > 0 ))
then
	gre -F -H "${gstring//ytak62030/\\^}" $dumpf >/dev/null
	cleanup $?
fi

# search the dump for specific filenames (not patterns) that were requestd.
# we guarentee the order of output matches command line list order and place
# MISSING as the filename in the proper place if we did not find the file. 
#
$join < $dumpf | gre "${gstring//ytak62030/\\^}" | awk \
	-v full=$full \
	-v verbose=$vcount \
	-v count=$count \
	-v list_nodes=$list_nodes \
	-v order="$order" \
	-v show_md5="$md5" \
	-v show_size="$size" \
	-v node="$node" \
	-v all=$all \
	'
		function get_val( s, 		a )
		{
			split( s, a, "=" );
			return a[2];
		}

		function snarf( what,		i )
		{
			split( what, b, " " );
			for( i = 1; i <= NF; i++ )
				if( split( b[i], a, "=" ) == 2 )
				{
					values[a[1]] = a[2];
				}
		}

		BEGIN {
			eidx = 0;		#/* error list index */
		}

		verbose > 2 { print "raw input: " $0; }

		/stable=0/ && all > 0 && verbose > 0 { 
			printf( "WARNING: unstable file: %s\n", $2 " " $3 ) >"/dev/fd/2";
			elist[eidx++] = $2 " " $3;
		 }

		/stable=1/ || all > 0 {
			x = split( $0, subrec, "#" );
			split( subrec[1], toks, " " );		# file basename: ...
			snarf( subrec[1] );			# get things like MD5=
			base = substr( toks[2], 1, length( toks[2] ) -1 );
			md5[base] = values["md5"];
			size[base] = values["size"];
			rcount[base] = 0;
		
			found++;
			for( i = 2; i <= x; i++ )			# for each sub record
			{
				split( subrec[i], junk, " " );			# nodename fullpathname
				if( junk[1] " " junk[2] != "attempt to" && (junk[1] == node || node == "") )
				{
					rcount[base]++;
					path[base] = junk[2];
					nodes[base] = nodes[base] ? nodes[base] "," junk[1] : junk[1];
				}
			}
		}

					# print what we got, in the order it was requested
		END {
			requested = split( order, a, " " );
			for( i = 1; i <= requested; i++ )
			{
				missing = full ? "MISSING(" a[i] ")" : "MISSING";
				stuff = sprintf( "%s %s", count ? rcount[a[i]]+0 : "", list_nodes ? nodes[a[i]] : "" );
				x = path[a[i]] ? (stuff != " " ? a[i] : path[a[i]]) " " stuff : missing;

				if( nodes[a[i]] == "" )
				{
					elist[eidx++] = a[i];
				}
				sub(" *$", "", x);			 # trim trailing spaces
				printf( "%s%s%s\n", x, show_md5 ?  " " md5[a[i]] : "", show_size ? " " size[a[i]] : "" );
			}

			if( found < requested && ! count )
			{
				printf( "ERROR: paths found (%d) != number requested (%d)\n", found, requested )>>"/dev/fd/2";
				for( i = 0; i < eidx; i++ )
					printf( "ERROR: %s\n", elist[i] )>>"/dev/fd/2";
				exit( 1 );
			}

			exit( 0 );
		}
	'
rc=$?				

cleanup $rc


exit
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_where:Determine where a replicated file is)

&space
&synop	ng_rm_where [-a] [-d dumpfile | -j joinfile] [-v] [-5] [-s] [-E] [-f] node file1 [file2 ^... filen]
&space	ng_rm_where [-a] [-d dumpfile | -j joinfile] [-v] [-5] [-s] [-E] -p node pattern1 [pattern2 ^... pattern_n]
&break	ng_rm_where [-a] [-d dumpfile | -j joinfile] [-v] [-5] [-s] [-E] -p -n   pattern1 [pattern2 ^... pattern_n]
&space	ng_rm_where [-a] [-d dumpfile | -j joinfile] [-v] [-5] [-s] [-E] [-f] {-c |-n} file1 [file2 ^... filen]

&space
&desc	&This scans a dump of information from the &ital(replication manager)
	for the file(s) or pattern(s) listed on the command line and produces the fully
	qualified path to each file if it exists on &ital(node). 
	If the primary form of the command is used,
	filenames are given on the command line, fully qualified filenames are 
	written to the standard output device.
	These filenames are each separated by a newline character, and are 
	written in the same order as the files
	appear on the command line. 
	If a file does not exist on &ital(node) the text &lit(MISSING) is written 
	instead of the path so as not to lose alignment of data. 
	The filenames that are passed in are expected to be only the &ital(basename)
	portion of the file name, and will be treated as complete, fixed, patterns
	when searching the dump (supplying a file name of &lit(scooter) will match only the file 
	&lit(scooter) and not &lit(scooter.pie, scooter-pie,) or &lit(scooterpie).)

&space
	In the command's first alternate form, using the &lit(-p) (pattern) option, 
	the positional parameter(s) on the command line are assuemd to be 
	patterns that can be passed directly to &lit(gre) to search the dump file. 
	When patterns are provided, the order of the files listed on the standard 
	output device is &stress(not) guarenteed, and the user should be aware 
	that multiple filenames may match the given pattern.
	The output when a pattern search is executed is &lit(<node> <filename>)
	with one pair per output line. 
	The filename output is the fully qualified pathname for the file on the indicated node.
	If the &ital(short listing) option (-S) is given with the pattern option, then 
	the output format becomes &lit(<basename> <nodelist>) where &ital(nodelist) is 
	a comma separated list of the nodes that the file appears on. Because the file 
	might have different paths on each of nodes, only the basename is given. 
&space
	Whether the output is of the short or long forms, 
	any information generated as a result of the use of 
	the size (-s) or checksum (-5) options will be placed following the file
	and node information. 
&space
	The &lit(-n) option can be combined with 
	a pattern search to find all files that match the pattern on all nodes.
	If the &lit(-n) option is used with the pattern option, then all positional 
	parameters are expected to be patterns (the first parameter is &stress(not) 
	treated as a node name).

&space
	In the second alternate form, when either the &lit(-c) or &lit(-n) options
	are used, a count or node list for each file is written for the file.
	At least one of the options must be supplied, and if both are supplied
	then the syntax of each output record is: &lit(filename count nodelist).
	When the node list is generated, it consists of a comma separated list
	of node names on which the file resides. 

&subcat	Leader Execution	
	Because the &ital(replication manager) executes, and maintains its dump file,
	only on the filer node, &this will determine if it is running on the filer
	and if it is not, &this will submit a command on the filer (using &lit(ng_rcmd))
	to execute itself there and return the results. If &this is being executed on 
	the filer, then &this continues normally.

&space
	The exception to this is if the node named on the command line is the node
	where &this is being executed.  In this case, the local &ital(replication manager)
	database is searched for files matching the name or pattern passed on the command line.
	This allows the &this command to execute much more quickly than if it must be run on 
	the filer.  A local search, however, may yield multiple instances for the same basename,
	and when this occurrs &this has no choice but to submit a request to the filer for 
	the information as it cannot tell which of the instances is the one registered with 	
	&ital(replication manager). 

&subcat Patterns
	Patterns supplied to &this are regular expression style patterns supported by the 
	&lit(grep) family of tools.  Because of the number of times a pattern may be 
	expanded and referenced as is is processed by &this, escaped characters in the 
	pattern tend not to yield the desired results.  Instead of escaping a character
	(e.g. \+) it is best to enclose it inside of square braces (e.g. [+]).
	

&space
&opts	The following options are supported by &this:
&begterms
&term	-5 : Causes the md5 checksum to be written for each file that is matched.
	The checksum is written following the filename on each output record that 
	is produced. If both the size and the checksum options are supplied, 
	the checksum is written first. 
&space
&term	-a : List all files even if not stable.
&space
&term	-c : Count. Causes &this to assume that the first positional parameter is 
	a filename, and produces a count of nodes which contain each of the files 
	listed on the command line. 
	If either the size or checksum options (-s, or -5) are supplied, the 
	count value is written before either of the other two pieces of information.
&space
&term	-d dumpfile : Supplies the name of the &ital(replication manager) dump 
	file to parse. If not supplied, the automatically generated dump file 
	(as referenced by the &lit(RM_DUMP) cartulary variable) will be used. 

&space
&term	-E : Escape special symbols in the filename or file pattern. If set, then 
	the special symbols (+, *, $, . and ^^) are automatically escaped in the search string 
	that is given to &lit(gre) when searching the repmgr join file. (The user
	still may need to escape or properly quote these characters on the command
	line.

&space
&term	-f : Full listing. Causes the filename to be generated along with the 
	"missing" indicator when a filename cannot be located in the dump file.
	The file name is enclosed within parenthesis and placed immediately 
	following the "missing" indicator such that no whitespace separates
	the two: &lit(MISSING(scooter).)
&space
&term	-j file : Supplies a file created with the &lit(ng_rm_join) command.  If a 
	process will be invoking &this multiple times, it can be much more efficient
	to create a join file and submit it rather than having &this create one each 
	time it is called. Join files can be created with ng_rm_join <dumpfile
	where dumpfile is typically $NG_ROOT/site/rm/dump.
&space
&term	-N : Causes the dump file name that is supplied via the &lit(-d) option 
	to be used if it exists (no &lit(rmreq) command is issued).
&space
&term	-n : Generate node list. All positional paramters are assumed to be filenames
	and for each file name listed on the command line a list of nodes that 
	contain the file is written with the filename on the standard output device. 
	If used in combination with the pattern option (-p), the list generated is 
	a series of node/filename pairs and includes all nodes on which the file 
	is present. 
&space
&term	-p : Pattern mode. &This expects that any positional parameters on the commandline
	are patterns rather than complete filenames.  A one to one, pattern to file, 
	coorespondance is not guarenteed, neither is the order that the resulting file 
	list is written to the standard output device. 
	For obvious reasons "missing" indicators cannot be written and thus
	the full listing option (-f) has no meaning when this option is used.

&space
&term	-r sec : Wait for refresh. &This will wait up to &ital(sec) 
	for the replication manager to refresh its dump file before reading it. 
&space
&term	-s : Causes the size of the file to be written after the file name. If both the 
	size and MD5 options are supplied, size is written after the checksum.
&space
&term	-S : Short list. Generates a list of basenames followed by a comma separated list 
	of nodes where the file resides. 
&space
&term	-v : Verbose mode.
&endterms	

&space
&parms	If none of the count, node, or pattern options (&lit(-c, -n) or &lit(-p) respectively)
	are supplied, then the first positional parameter is assumed to be the name 
	of the node that is to be searched for. The resulting output will be the 
	fully qualified filename of each of the file names that appear as the 
	remainder of the command line parameters. 
	When the pattern option (&lit(-p)) is supplied, the remiander of the postional 
	parameters are expected to be patterns that can be passed to &lit(egrep()).

&space
&exit	If full pathname could not be found on &ital(node) for all files in the list, 
	then a non-zero return code will be returned to the caller. If all files
	could be found, or the node/count options were specified, then the return 
	code is zero. 

&space
&see	&ital(ng_rep_mgr), &ital(ng_rmreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	30 Aug 2001 (sd) : New code.
&term	25 Oct 2001 (sd) : Added -p option.
&term	06 Dec 2001 (sd) : Added timeout option.
&term	01 Feb 2002 (sd) : Added -s (size) option.
&term	04 Feb 2002 (sd) : Added -N option  to skip the dump if dump file supplied and exists.
&term	19 Feb 2002 (sd) : Mod to use static repmgr dump file and rm_join to make parsing a wee
	bit more simple. Removed timeout support as it is not needed.
&term	26 feb 2002 (sd) : Added a wait for refresh option.
&term	11 Jul 2002 (sd) : To use rmdb to searc the local rmdb for the file - avoid rmwhere storms on the leader.
&term	01 Apr 2002 (sd) : To escape a + in a gsub that bsd awk was barfing on.
&term	16 Apr 2002 (sd) : Corrected a problem that caused rcmd to coredump if a larg number of files was 
	given on the command line.
&term	20 May 2003 (sd) : added ability to accept a pre-created join file to speed things up.
&term	25 Jul 2003 (sd) : corrected pattern matching issue and added # before node in pattern.
&term	21 Dec 2003 (sd) : added support for both ^pattern and pattern$ to match beginning and ending of basename.
&term	30 Dec 2003 (sd) : corrected manpage for -S option (added).
&term	09 May 2004 (sd) : When invoked with the -S option (short list) if the file has no instances, the 
	node list is presented as <none> rather than a blank. (bug tracker issue 144)
&term	18 Jun 2004 (sd) : Fixed issue with hint: passing through.
&term	19 Oct 2004 (sd) : nixed all ng_leader references.
&term	06 Apr 2005 (sd) : Added local support via dump1 command.  Now does not have to look at dump file 
	for local file resolution.  Will find all instances of the local file that are also the current
	active registration.  Prevents unattached instances from showing up in the list as they could before.
&term	16 May 2005 (sd) : Removed a bit of testing code that accidently was left in.
&term	30 Jun 2005 (sd) : Added a 10 minute timeout to the dump1 command.
&term	24 Dec 2005 (sd) : Added -E option which auto escapes special symbols when searching.
&term	21 Feb 2006 (sd) : corrected doc to include -a option.
&term	27 Apr 2006 (sd) : Fixed formatting problems in the man page.
&term 	21 Jun 2006 (sd) : Changed pattern replacement holders. 
&term	06 Apr 2006 (sd) : Fixed such that when -n is used only stable files are returned unless -a is given.
&term	23 Apr 2007 (sd) : Now lists the files that were missing after the error indication (to stderr). Also 
		correctly reports the number found for all cases. 
&term	04 Aug 2008 (sd) : Fixed /* style comment in awk; changed to #. 
&endterms

&scd_end
------------------------------------------------------------------ */
