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
#!/ningaui/bin/ksh
# Mnemonic:	ng_rollup
# Abstract:	Will corrdinate the rollup of files from across the cluster. 
#		caller may supply a list or pattern.  Replication manager will 
#		be searched if a pattern is supplied. Supports two modes:
#			1) local -- files are rolled up locally; result is registered
#			2) mgt -- the script invokes ng_collate to manage the rollup
#				across the cluster.
# local test command:
#	ng_rollup -l  -v -f /ningaui/fs00/14/08/pf-coll-ningd4-3a8c464-5.list  -m 'ls -al' /ningaui/tmp/daniels.out
#
#		
# Date:		24 April 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# weed files associated with nodes that are ignored, or suspended. invoked only when NOT in local mode
function discard_nodes
{
	if [[ -n $srv_Jobber ]]
	then
		ng_sreq -s $srv_Jobber dump 30|grep SUSPENDED|awk '{ split( $2, a, "(" ); printf( "%s ", a[1] ); }'|read suspended
		verbose "suspended nodes will be ignored: $suspended"
	else
		log_msg "unable to find jobber host to determine which nodes are suspended."
	fi
	
	if [[ -n $exattr || -n $exnode ||  -n $suspended ]]				# an exclude node/nattr list exists
	then
		unique_count <$mylist | read countb 			# number of unique files in the list before
		if [[ -n $exattr ]]
		then
			ng_species $exattr |read exclude		# convert attributes into node names
			if [[ -n $exclude ]]
			then
				exclude="$exclude "			# needs trailing blank
			fi
		fi
		verbose "ignoring nodes because of attributes: $exclude"
		verbose "ignoring nodes because of suspension: $suspended"
		verbose "ignoring nodes listed on cmd line:    $exnode"
	
		exclude="$exclude$exnode$suspended"
		exclude="${exclude% }"				# prevent trailing blank from converting to " |"
		if [[ -n $exclude ]]				# if null, probably no nodes matched the indicated attribute
		then
			verbose "exclude node pattern = (${exclude// / |})"
			if egrep -v "${exclude// / |}" $mylist >$mylist.x
			then
				unique_count <$mylist.x |read counta			# number of files listed after the prune
				if [[ $counta -ne $countb ]]
				then
					log_msg "pruning files based on ignored nodes removed all replications of some files from the list [FAIL]"
					log_msg "$countb unique files in list before prune; $counta files after [FAIL]"
					cleanup 4
				fi
				verbose "$countb unique files in list before prune; $counta files after [OK]"
		
				mv $mylist.x $mylist		# pruned list back
			else
				log_msg "error trying to exclude with pattern: ${exclude// / |} [FAIL]"
				log_msg "likely issue is that the pattern caused no filenames to be listed."
				cleanup 3
			fi
		fi
	fi
}

# add $@ to exnode or exattr list; we assume attributes start with uppercase letter
function set_exclude
{
	typeset p=""

	for p in $*
	do
		case $p in 
			!*|[A-Z]*)	exattr="$exattr$p ";;
			*)		exnode="$exnode$p ";;
		esac
	done
}

# look on this node for $1 echo node path pair  if we find it and it can be read
# deprecated in version 2
function look_here
{
	typeset lpath=""

	if lpath=`ng_rm_where $mynode ${1##*/}`			# look for specific file here
	then
		if [[ -r $lpath ]]
		then
			verbose "look_here: found $1 in $lpath on $mynode"
			echo $mynode $lpath			
			return 0
		fi
	fi

	log_msg "look_here: cannot find file on $mynode: $1"
#	ng_log $PRI_ERROR $argv0 "cannot find file on $mynode:: $1"
	return 1
}

# count number of unique filenames in the stdin; assuming  that the record format is:
#	nodename path/filename junk
function unique_count
{
	awk ' { x=split( $$2, a, "/" ); print a[x]; }'|sort -u|wc -l
}
				

# take a list of files and send dump1 commands for them to repmgr; save the output 
# for each file if stable.
# parms: list-file output-file
function fetch_dump1
{
	typeset d1f=${2:-$TMP/rmdump.$$}		# spot to put the output
	typeset pattern="stable=1"

	verbose "fetching dump1 info"
	if  (( $list_unstable > 0 ))
	then
		pattern="stable="		# match all
	fi

	awk '
		{ printf( "dump1 %s\n", $1 ); }
	' <${1:-no-input-file} | ng_rmreq -t 300 -a | gre "$pattern" >$d1f
}

# requests dump1 information from repmgr for each file in the list ($1) and then parses out
# the results from repmgr. Ouput (stdout) is a list of node path size tripples
function rm_list
{
	typeset listf=${1:-/dev/null}
	typeset dumpf=$TMP/rm_dump.$$
	typeset expect=""
	typeset found=""

	fetch_dump1 $listf $dumpf 	# pull a dump of each file in the list

	wc -l $listf | read expect junk
	wc -l $dumpf | read found junk

	if (( $expect != $found ))
	then
		if (( ignore_count_err < 1 ))
		then
			log_msg "did not find expected numbers of files in repmgr: expect=$expect found=$found  list=$listf [FAIL]"
			cat $listf >&2
			log_msg "======================================================="
			cat $dumpf >&2
			cleanup 1
		else
			log_msg "did not find expected numbers of files in repmgr: expect=$expect found=$found  list=$listf [WARN]"
		fi
	fi

	verbose "expect($expect) from repmgr, found($found) [OK]"

	if [[ ! -e $dumpf ]]
	then
		log_msg "after fetch dump copy seems missing: $dumpf"
		cleanup 1
	fi

	verbose "parsing dump1 information"
	awk -F "#" '
	{
		split( $1, b, " " );
		split( b[4], c, "=" );
		size = c[2]+0;
		for( i = 2; i <= NF; i++ )			# skip file, matched, size crap
			if( split( $(i), a, " " ) == 2 )	# split filename size
			{
				if( a[1] != "attempt"  )	# repmgr notes attempt to delete/copy etc here too :(
					printf( "%s %s %s\n", a[1], a[2], size );
			}
			else
				printf( "bad stuff i=%d> (%s)\n", i, $(i) ) >"/dev/fd/2";
	}
	' <$dumpf  

	verbose "finished with list from repmgr"

}

# check repmgr space on all nodes in the cluster
# adds a list of bad nodes to the exclude list
function check_space
{
	typeset n=""
	typeset list=""
	typeset node=""

	for n in $(ng_members)
	do
		echo "$n $(ng_rcmd $n foo)"
	done | while read node stats
	do
		if [[ $stats == *"no_rmmgr_space"* ]]
		then
			list="$list$node "
		fi
	done

	exnode="$exnode $list"
}

# convert stdin into node /path/fname size tripples. Input may be one of thes formats:
#	node [/path/]fname [size]
#	[/path/]fname
#
# output order will NOT match the input order so dont depend on it
# if this is a local rollup, then we will also verify each of the /path/fnames generated
# acutally are on disk and readable.
function adjust_list
{
	typeset suss_list=$TMP/suss_list.$$		# list of files that we will have to search repmgr for
	typeset tuple_list=$TMP/tuple_list.$$		# list of node /path/fname size tripples 
	typeset hold_list=$TMP/hold_list.$$		# list of node /path/fname size tripples
	
	awk -v tuple_list=$tuple_list '			# start by splitting off files we need to look up to suss file; others to stdout
							# suss list consists just of the basename, one per record

	function basename( f, 	a, x )			# obvious
	{
		x = split( f, a, "/" );
		return a[x];
	}
	function isfullq( f )				# return true if fname is fully qualified 
	{
		if( substr( f, 1, 1 ) == "/" )
			return 1;
		return 0;
	}

	{
		if( NF > 1 )			# if multiple tokens, assume node [path/]fname size
		{
			if( isfullq( $2 ) )
				printf( "%s %s %s\n", $1, $2, $3) >tuple_list;	# this goes as is and we assume supplier has vetted it
			else
				printf( "%s\n", basename( $2 ) );		# if user forgot lead slant, we will dig from repmgr
		}
		else
			printf( "%s\n", basename( $1 ) );			# assume just the basename and must dig from repmgr
	}' |sort -u >$suss_list

	if [[ -s $suss_list ]]
	then
		verbose "sussing node paths from repmgr"
		rm_list $suss_list >>$tuple_list			# find files from suss list in repmgr
	fi

	if [[ $mode == "local" ]]			# prune files not on this node, and verify all paths are legit files
	then
		mv $tuple_list $hold_list
		error=0
		gre "^$mynode " $hold_list | while read a b c junk	# pull off just files on our node and validate
		do
			if [[ ! -r $b ]]
			then
					log_msg "local: could not verify file: $b"
					error=1
			else
					verbose "local: found file: $b"
					echo $a $b $c
			fi
		done  >>$tuple_list	#  new pair list

		if (( $error > 0 ))
		then
			log_msg "abort: error(s) found while verifying local data files"
			cleanup 1
		fi
	fi

	sed 's/ $//' <$tuple_list		# strip the trailing blank used to ensure full name match, and write to stdout
}


# take the list of files (either filenames, or node filename size tripples) and run them through 
# the supplied method.  If in passthru mode, the method is given a list of filenames
# otherwise, the list is placed on the method's command line via xargs
#
function combine_files
{
	verbose "rolling files with: $method" 
	>$outfile

	if [[ $passthru -gt 0 && $method != "cat" ]]	# cat cannot support passthrough, so it makes sense to do xargs for it
	then
		typeset method_list="/tmp/methodlist.$$"

		verbose "passing filelist ($method_list) directly to method"
		awk '{ print $2; }' <$mylist >$method_list		# strip node and size from the records
		if [[ $method == *"==rollup_flist=="* ]]
		then
			eval ${method/==rollup_flist==/$method_list} >$outfile
			rc=$?
		else
			eval $method $method_list >$outfile
			rc=$?
		fi
	else
		verbose "invoking method via xargs"
											# use eval so method can be something like cat|sort
		awk '{ print $2; }' <$mylist | xargs -n 1000 $method >>$outfile		# allow for node file size tripples, or just filenames
		rc=$?
	fi

	return $rc
}

function usage 
{
	argv0b=${argv0##*/}
	cat <<endKat
	$argv0b	--man
	$argv0b	[-c count] [-f filelist-file] [-F final-method] [-i node] [-k] [-l] [-L logname]  [-m method] [-P] [-p file-pattern] [-r] [-t target] output-file [file1...filen]
	Files to be rolled up may be supplied using any combination of:
	-f, -p, or file names on the command line.
endKat
}

# -------------------------------------------------------------------

ng_log $PRI_INFO $argv0 "strted: $@"

ustring="[-c count] [-d] [-f filelist-file] [-F final-method] [-i node|attribute] [-l] [-L logname]  [-n] [-p file-pattern] [-r] [-s] [-t target]"

log=""
flist=""		# list of files	(filename node, may contain duplicate basenames, order unimportant)
pat=""			# pattern to search with
mode="mgt"		# -L sets to local; default to mgt mode (invokes collate)
dellist=0		# -d will set; delete file list (unregister) if true
ng_sysname -s |read mynode	
target=$mynode		# given as hint when output file is registered; defult to pulling to this node
register=0		# -r sets if user wants output file automatically registered
method=cat		# default method is to cat files
expect=""		# if supplied (-c count)  then we expect this number in the final list file (passed to collate)
forreal=1		# -n sets to -0 -- we just say what we might do
passthru=0		# -P - causes the file list to be passed to method rather than to xargs (local only)
passthru_opt=""		# option for collate
final_method=""		# secondary method run on intermediate files
attempts="3"
keep=""			# -k sets to pass into collate -- keep intermediate files
exattr=""		# list of node attributes to ignore
exnode=""		# list of hard nodes to ignore
list_unstable=0		# list files even if they show unstable
ignore_count_err=0	# set to ignore count mismatch between list and what we got from repmgr
size_flag="-s"		# default to running collate in size mode
verbose=1

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	attempts="$2"; shift;;		# seneschal jobs are reattempted this many times after faiure
		-a*)	attempts="${1#-?}";;

		-c)	expect="-e $2"; shift;;		# expects this count of unique files, we should verify
		-c*)	expect="-e ${1#-?}";;

		-d)	dellist=1;;			# delete the list on exit

		-f)	flist="$2"; shift;;		# file containing the list to roll together
		-f*)	flist=${1#-?};;
	
		-F)	final_method="-F$2"; shift;;	# the -F MUST be abutted to the first character of the string!
		-F*)	final_method="-F${1#-?}";;

		-i)	set_exclude $2; shift;;		# ignore these nodes

		-I)	ignore_count_err=1;;

		-k)	keep="${keep}-k ";;		# passes through to collate to keep intermediate files (-k keep lists; -k -k lists & intermed)

		-j)	job_name="$2"; shift;;		# we will extend our lease with seneschal if given this

		-l)	mode="local";;

		-L)	log="$2"; shift;;
		-L*)	log=${1#-?};;

		-m)	method="$2"; shift;;			# method used to roll the files together
		-m*)	method="${1#-?}";;
	
		-n)	forreal=0;;

		-P)	passthru=1			# pass the list to the method, rather than xarging it
			passthru_opt="-P"		# option for collate
			;;

		-pri)	priority="-p $2"; shift;;
		--pri)	priority="-p $2"; shift;;

		-p)	pat="$2"; shift;;		# file pattern to build list
		-p*)	pat=${1#-?};;

		-r)	register=1;;			# register the output file (local mode only)

		-s)	list_unstable=1;;
		-S)	size_flag="";;			# turn off

		-T)	timeout="-T $2"; shift;;	# timeout parm passed directly to collate
		-T*)	timeout="-T ${1#-?}";;
		
		-t)	target="$2"; shift;;
		-t*)	target=${1#-?};;

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup -k 1;;
		-\?)	usage; cleanup -k 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -n $log ]]
then
	if [[ -d $log ]]		# if user gave us a directory, create $log/rollup.$$
	then
		exec >$log/rollup_$$ 2>&1
	else
		exec >$log.$$ 2>&1
	fi
fi

verbose "${argv0##*/} version 2.0/02047"
verbose "NG_RM_FS_FULL= $NG_RM_FS_FULL"

if [[ -n $job_name ]]
then
	if [[ -n $srv_Jobber ]]
	then
		verbose "extending lease to ${NG_ROLLUP_MINTO:-5000} seconds for job: $job_name"
		ng_sreq -s $srv_Jobber extend $job_name ${NG_ROLLUP_MINTO:-5000}
	else
		log_msg "could not find srv_Jobber in environment; lease NOT extended"
	fi
fi

if [[ $# -lt 1 ]]
then
	log_msg "missing output file name on command line"
	usage
	cleanup -k 1
fi

mylist=$TMP/list.$$		# list we create

if [[ $register -gt 0 ]]		# if we will be registering the output file get a name in repmgr land
then
	ng_spaceman_nm -v -v ${1##*/} |read outfile		
	if [[ ! -d ${outfile%/*} ]] 
	then
		log_msg "bad directory name in repmgr space: $outfile"
		cleanup 5
	fi
else
	outfile=$1		# not registering, use exactly what they gave us
fi
shift				# remaing parms, if any, are files to roll

verbose "combined output will be written to: $outfile"

# get user list and ensure names are fully qualified with node.  we cannot check input numbers
# against output numbers for errors because it is possible to prune the input list if we are in 
# local mode.  adjust files does its best to try to detect errors when in local mode and files 
# cannot be found on this node. 
#
verbose "list adjustment/generation begins; flist=$flist"
if [[ -n $flist ]]
then
	if [[ ! -r $flist ]]
	then
		log_msg "cannot find/read file list: $flist"
		cleanup 1 
	fi

	adjust_list <$flist >$mylist 		#ensure fully qualified names 
	wc -l <$flist|read fsize junk
	wc -l <$mylist|read mysize junk
	verbose "$fsize files listed in $flist read; resulting method list had $mysize files"

	if [[ $mode == "local" ]]		# in local mode, each and every file in the list must be accounted for
	then
		#cat $flist $mylist | awk '
		(
			sed 's/^/f1 /' <$flist			# must have one of these in my list
			sed 's/^/f2 /' <$mylist
		) | awk '

			$1 == "f1" { 
			 	x = split( $2, a, "/" ); 	# flist is a single column of filenames (fname is now 2 after we added f1) 
				got_f1[a[x]] = 1; next; 
			}

			$1 == "f2" { 
			 	x = split( $3, a, "/" ); 	# the output from adjust is two or three columns
				got_f2[a[x]]++; next; 
			}

			END {
				state = 0;

				for( f in got_f1 )
					if( got_f2[f]+0 < 1 )
					{
						state = 1;
						printf( "exact_list check: missing file: %s\n", f )>"/dev/fd/2";
					}
				exit( state );
			}
		'
		if (( $? > 0 ))
		then
			log_msg "abort: not all files in $flist(flist) were found"
			cleanup 1 
		else
			verbose "exact_list check: all files found [OK]"
		fi
	fi
fi

if [[ -n $pat ]]
then
	if [[ $mode = "local" ]]
	then
		where_opt="$mynode"		# if pat, only those on this node
		pat="$pat.*"			# space at end allows for quirk in rm_where when finding local files
	else
		where_opt="-n"			# find file on any node
	fi

	verbose "files matching pattern $pat added to method list"
	ng_rm_where -s $vflag -p $where_opt "$pat" >>$mylist
fi

if [[ $# -gt 0 ]]
then
	while [[ -n $1 ]]
	do
		echo $1
		shift
	done | adjust_list >>$mylist		# add command line provided names to the list

	verbose "command line listed files added to method list"
fi
verbose "list adjustment/generation ends"


if [[ $mode != "local" ]]
then
	discard_nodes				# prune list of files associated with ignored/suspended nodes
fi

exit 1
wc -l <$mylist|read mysize
verbose "method list contained $mysize files"

if [[ $forreal -lt 1 ]]
then
	log_msg "no action (-n) flag selected, would combine these files:"
	verbose "------------------------- method list ---------------------------"
	cat $mylist >&2
	verbose "-----------------------------------------------------------------"
	log_msg "no action (-n) flag selected, would combine the files listed above"
	cleanup 0
fi

if (( $verbose > 0 ))  &&  [[ $mode = "local" ]]		# mostly to capture in log files as run by collate
then
	verbose "------------------------- method list ---------------------------"
	cat $mylist >&2
	verbose "-----------------------------------------------------------------"
fi

if [[ -s $mylist ]]
then
	if [[ $mode = "local" ]]
	then
		if [[ -n $target ]]
		then
			target="-H $target"	# its a hood flag  for rm_register in local mode 
		fi
		combine_files			# local rollup 
	else
		if [[ -n $target ]]
		then
			target="-t $target"	# pass right into collate
		fi
		# because $final_method MUST be quoted, it MUST be the last option; immediately before $method
set -x
		ng_collate $size_flag $keep -a $attempts $priority $passthru_opt  $timeout $vflag  $target $expect -o $outfile "$final_method" $method <$mylist
		rc=$?
set +x
	fi
else
	log_msg "resulting file list was empty!"
	cleanup -k 1
fi

if [[ $rc = 0 ]]
then
	if [[ $register -gt 0 ]]	# the initial invocation of this should generally not have regsiter flag
	then				# as the invocations from collate will be registering the files
		if [[ $rc -lt 1 ]]
		then
			if [[ -n $job_name ]]
			then
				if [[ -n $srv_Jobber ]]
				then
					verbose "re-extending lease to ${NG_ROLLUP_MINTO:-5000} seconds before register: for job: $job_name"	# md5s might take long
					ng_sreq -s $srv_Jobber extend $job_name ${NG_ROLLUP_MINTO:-5000}
				else
					log_msg "could not find srv_Jobber in environment; lease NOT extended"
				fi
			fi

			verbose "registering output file: $outfile"
			ls -al $outfile >&2
			ng_rm_register -v $target $outfile
		else
			verbose "something failed, trashing output: $outfile"
			rm -f $outfile
		fi
	fi
fi

if [[ $dellist -gt 0 && -n $flist ]]
then
	verbose "unregistering the input file list"
	ng_rm_register -u $flist
fi

verbose "done, exit code from method is: $rc"
if [[ $rc -gt 0 ]]
then
	keeptmp="-k"
fi
cleanup $keeptmp $rc

exit 

SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rollup:Roll together related files from round the cluster)

&space
&synop
	ng_rollup [-c count] [-f filelist-file] [-F final-method] [-i node|attribute] [-l] [-L logname]  [-m method] [-n] [-p file-pattern] [-r] .br 
[-t target] [-T sec] output-file [file1...filen]

&space
&desc
	&This causes related files to be combined into a single file using the &ital(method) 
	supplied on the command line, or the &lit(cat) command if no method is indicated. 
	&This may be started in either &ital(local) mode (-l) or cluster wide mode. 

&subcat The File List
	The list of files that are combined is created based on the list of files passed as 
	command line parameters, listed in the file list (-f option), or found in the replication 
	manager space using the indicated pattern (-p option).  Any combination of these three 
	methods can be used to supply  a list of files to &this.
&space
	When a file list is supplied it may contain records with only the filename, or with 
	nodename, file pathname, filesize tripples. 

&subcat Cluster Wide Mode
	When invoked in cluster wide mode (which is the default), &lit(ng_rollup) determines the location 
	of all of the instances for the files supplied on the command line, or files that match the 
	indicated pattern.  This list is given to &lit(ng_collate) to manage the rollup across the cluster.
	In breif, &lit(ng_collate) will divide the files by node, and invoke the method for each set 
	to produce an intermediate file on each node. The intermediate files are collected onto one node
	in the cluster at which point &lit(ng_collate) causes them to be combined using the final method, if
	supplied, or the same method that was used to create the intemediate files. 

&subcat Local Mode
	Local mode is indicated with the &lit(-l) option, and  combines only files that reside on the 
	node where &this was started. The resulting output file is registered only if the &lit(-r)
	(register) option is set on the command line.  
	Local mode is primarily intended for use by &lit(ng_collate) as it creates the intermediate files, 
	but can be invoked manually if needed. 

&subcat The Method
	The method is the programme that will be invoked to combine the files that match the 
	file pattern, and/or were supplied via the command line or file list. By default, the 
	method is invoked via &lit(xargs) with the filenames supplied as command line parameters.
	If the &ital(passthru) option is supplied (-P), then the name of the file containing the 
	list of filenames to combine is passed to the method as the only command line parameter. 
	In either case, the filenames are fully qualified pathnames. 
&space
	Regardless of the invocation technique used, the method is expected to generate the resulting
	file onto standard output.  &This is responsible for directing standard output to the 
	correct file, and for registering the file with the replication manager if that was 
	requested by the user. 
&space
	The method is expected to exit with a zero (0) return code if it was successful, and a non-zero 
	return code if the output could not be created. 
&space
&subcat The Final Method
	The intermediate files produced on each node are collected on a central node and combined into 
	the final output file.  This method used to accomplish the final pass is the same supplied with 
	the &lit(-m) option, but can be overridden if needed using the &lit(-f) option to supply a
	&ital(final method.)  Arguments and expectaions for the final method are the same as for the 
	first pass method. 

&space
&subcat	The List Of Files
	By default, the list of files that the method is supposed to process is supplied as the final 
	positional parameters to the command.  If the passthrough option (-P) is supplied, then the 
	command line is given a filename containing a list of newline separated filenames that are to be
	processed. This file list file name is placed at the end of the method command. If the method 
	string contains the characters "==rollup_flist==," then that is replaced by the filelist filename
	rather than placing it at the end.  For example, the following command causes the list to be placed
	after the &lit(-L) token in the method:
&space
&litblkb
   ng_rollup -v -m "combiner -L ==rollup_flist== $date $user" -P $output_file
&litblke
&space
	This technique allows the method to accept two positional parameters, and to treat the list value
	as an option.  This may be necessary when the method programme is used by more than just
	&this. Another example might be:
&space
&litblkb
   ng_rollup -v -m "combiner" -F "cat ==rollup_flist== | xargs sort -u" -P $output_file
&litblke
&space
	This allows the final mthod to also be passed a list of filenames via a file where the programme (sort)
	does not provide for a filelist file.


&space
&opts	The following options are recognised by &this when placed on the command line:

&begterms
&term	-c count : Indicates the number of unqique filenames that are expected. The count is passed to 
	&lit(ng_collate) which will verify that it has exatly this number of files, and abort if it
	does not. This option has not meaning when running in local mode. 

&space
&term	-d : Deletes the list file after it is done.
&space

&space
&term	-f file : Names the file containing a list of files to be rolled up. The list is newline separated
	records each with the syntax of:
&break
	&lit([node] [path/]filename [filesize]
&break
	If the node and/or the path for each file is omitted, then &lit(ng_rm_where) will be used to search
	for the file(s).  Duplicates are allowed, and recommended, such that all copies of replicated files
	are available to help even the rollup activities across the cluster. 
	
&space
&term	-F final-method : This is the command that is executed to process the set of intermediate files
	generated by the &ital(method) supplied using the &lit(-m) option. If this option is not supplied, 
	the method supplied with th e&lit(-m) option (or the default) is used to process the intermediate
	files.  
&space
&term	-i token : Ignore files on node. &ital(Token) is either a node name or a node attribute. 
	Attributes will be converted to node names, and the resulting set of nodes will be used as an
	ignore list.  
	Attributes can be listed as space separated tokens, provided that the list is enclosed in quotes,
	and the not modifier may be applied (e.g !FreeBSD).  Multiple &lit(-i) options may be supplied 
	in place of submitting a list. 
	&space
	Files which reside on node(s) in the ignore list will be pruned before the 
	rollup method is invoked.  &This will check to ensure that pruning the list does not completely 
	eliminate any of the files from the rollup.  If &this detects such a case, the process will 
	exit with an error.  It is also wise to supply the count option (-c) if the number of files 
	is known before the rollup.
	&space
	When running with the ignore option, it is suggested that a trial execution be submitted with the 
	&lit(-n) (no execution) flag set.  This will list the nodes that have been ignored, and the files
	that will be included in the rollup. 
&space
&term	-k : Keep mode.  Causes &this to indicate to &lit(ng_collate) a keep state. If one &lit(-k) is 
	supplied, then the lists generated for each intermediate and final rollup are kept (left in the 
	replication manager filesystems).  If a second &lit(-k) is supplied (e.g. -k -k), then both 
	the intermediate files, and the list files are retained after completion of &lit(ng_collate.)

&space
&term	-l : Local mode. Files are limited to those residing on the node where &this is executing. If this option 
	is not present, &this invokes &lit(ng_collate) to manage the combination of files across the cluster.

&space
&term	-L file : Specifies a log file to write log messages to. If not supplied messages are displayed on the 
	stndard error device. If &ital(file) is a directory, a log file &lit(rollup_$$) is created in that 
	directory.
&space
&term	-m method : This command line option supplies the command that is to be used to 
	combine the listed files. The command must be capable of accepting one or more filenames on the 
	command line, and writing an output file to the standard output file. &ital(Method) will be invoked
	using &lit(xargs), and if it requires parameters, they may be supplied as a quoted token.  
	This method is also used to combine the intermediate files after they have been collected on a
	central node, unless the command line also includes a &lit(-f) option.
	If this option is not supplied, &lit(cat) is default method.

&space
&term	-n : No execution mode. The list of files that would be combined is created, written to the standard
	output device, and then &this exits with a good return code. 

&space
&term	-P : Passthru mode.  Causes the file list to be passed to the method via a filelist file, rather 
	than as command line parameters.  The name of the file containing all of the files to be 
	rolled-up is passed to the method as the only command line parameter. 
&space
&term	-p pattern : Specifies a pattern that is passed to &lit(ng_rm_where) in order to build a list of 
	files that are to be combined. If &ital(pattern) does not generate the desired list of files, 
	expiermentation with &lit(ng_rm_where) might be necessary to determine the exact pattern to supply, 
	or a list may need to be created manually and supplied using the &lit(-f) option.
&space
&term	-r : Register the output file. This option will cause the output file to be registered, and is 
	generally necessary only when &this is invoked by &lit(ng_collate). 

&space
&term	-S : Turn off the size option to collate. When size is turned off, collate will divide the files for 
	each rollup job based on count per node rather than on file size. 

&space
&term	-t node : Causes a hint to be given to the replication manager which should cause the output file 
	to be placed on &ital(node). If running in local mode, this option is effective only if the register 
	option is also supplied. If no target is supplied, the current host is assumed.
&space
&term	-T sec : Supplies a timeout in seconds. When running in cluster mode, jobs that to not return a status 
	are considered if &ital(sec) seconds have passed following the submission of the job. If no timeout
	value is supplied, &this will wait until the status of all jobs has been returned.
&endterms

&parms
	Following any necessary command line options, &this expects a minimum of one command line parameter, 
	the name of the output file, and zero or more input file names. The output file name may be 
	a relative or fully qualified pathname, however any pathname is ignored unless executing in local mode
	with the register option turned off. 
	Any filenames placed following the output name will be added to the list of files (either supplied by 
	the -f option, or created via pattern matching).

&space
&exit	An exit code of zero indicates that all of the requested files were combined into a single output file.
	Any non-zero exit code indicates an error; an accompaning error message should be written to either the 
	standard error device, or the log file. 

&space
&examp	The following illustrate several methods for invoking &this:
&space
&litblkb
  # collect all pinkpages files into a single file
  ng_rollup -v -m cat -p pinkpages pp_ckpt.rollup

  # collect all pinkpages files existing on the current node
  ng_rollup -v -m cat -p pinkpages -l pp_ckpt.rollup

  # collect all cycle report files for edition $ed, final output on ning21.
  # there should be exactly 5000 matches to the pattern; fail if not.
  ng_rollup -v -m report -c 5000 -p "ds-$apl-....-$ed.rpt" -t ning21 pf-$apl-$ed.rpt

  # collect all files listed in /tmp/daniels.f
  ng_rollup -l -v -m cat -f /tmp/daniels.f /tmp/daniels.rollup

  # roll up files, but invoke the method with a file list rather than 
  # supplying the files on the command line.
  ng_rollup -P -m squish_files -p "pf-search-....-rslt" ps-search-result

  # allow the flist list to be passed in the middle of the 
  # method command rather than as the last parameter
  ng_rollup -P -m cat ==rollup_flist== |xargs grep foo /tmp/rollup.out

&litblke

&space
&see	&seelink(tool:ng_collate), &seelink(repmgr:ng_rm_where)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Apr 2003 (sd) : Complete revamp to work with collate.
&term	15 May 2003 (sd) : Added passthru mode. 
&term	19 May 2003 (sd) : added usage string.
&term	27 May 2003 (sd) : changed comment regarding propy.
&term	23 Aug 2003 (sd) : Added final method capability
&term	24 Feb 2004 (sd) : added ability to ignore suspended nodes, and nodes with 
	attributes passed in on ignore option.
&term	05 Oct 2005 (sd) : Minimum timeout is extended to NG_ROLLUP_MINTO if set; 5000
	seconds if not set.
&term	24 Aug 2006 (sd) : Cleaned up a bit, added a verbose message to confirm setting of NG_RM_FS_FULL value.
&term	06 Dec 2006 (sd) : Added code to better suss out node and full path info from repmgr.  Individual rm_where calls
		were too slow for more than 10,000 files (especially after rm_where started using dump1).
&term	08 Jan 2007 (sd) : Corrected man page (-F description was missing).
&term	04 Feb 2007 (sd) : Added ability to put the string "==rollup_flist==" into the method command to allow for 
		more flexible method command lines. 
&term	26 Feb 2007 (sd) : Added check that prevented problems when excluding nodes based on attributes when the 
		cluster does not have any nodes of that type.
&term	09 Apr 2007 (sd) : Added extra check to ensure when a local rollup is being done, that all files
		listed in a flist are found on the local node. 
&term	26 Mar 2008 (sd) : Made changes to support gathering and sending size as a part of the file list to collate. 
&term	16 Apr 2008 (sd) : Corrected typo in local file match function.
&term	16 Oct 2009 (sd) : Added check for no space on device and it will add that node to the list to exclude. 
&endterms

&scd_end
------------------------------------------------------------------ */

