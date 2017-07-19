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
# Mnemonic:	log_comb (v2)
# Abstract:	For each masterlog file (masterlog.yyyymmdd000000) that is hooded 
#		to this node, we gather all matching fragment files (mlog.node.id.yyyymmdd000000)
#		that are on this node, and add them to the masterlog. The new masterlog
#		is registered. The old copy and fragment files are deleted.  Fragment
#		files are generated each hour by ng_log_frag and hooded based on the 
#		cooresponding masterlog file date.  Ng_log_sethoods is responsible for
#		tracking and making the hood assignments for the master logs.  This 
#		script should be scheduled three to six times a day. 
#	
#		
#		analysis files: they are kept one per masterlog and are a running bit 
#		of information. The will eventually be cleaned up by the tmp cleaner
#		after we stop updating the masterlog, so we make no effort to purge
#		or otherwise keep them tidy.
#
# Date:		26 Jan 2007 
# Author:	E. Scott Daniels
#
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function debug
{
	if (( ${debug:-0} > 0 ))
	then
		log_msg "$@"
	fi
}

# remove the named file unless the user has added the keep (-k) option on the command 
# line. If -k, then the named file is moved to $TMP/log_comb/keep
# $2 is an optional tag for the verbose line
function ditch_file
{
	if (( $keep_ifiles > 0 ))
	then
		if [[ ! -d $TMP/log_comb/keep ]]
		then
			mkdir -p $TMP/log_comb/keep
		fi

		mv $1 $TMP/log_comb/keep/
		verbose "ifile kept: $2 $1"
	else
		verbose "ifile removed: $2 $1"
		rm -f $1
	fi
}

# add something to the analysis file:
#	add2anal "comment" type [type-data]
function add2anal
{
	typeset val=""

	echo "$(ng_dt -p) [$$] $1"  >>$anal_file

	case $2 in 
		cat|file) cat $3 >>$anal_file;;  	# add comment then file contents

		cmd)	$3 >>$anal_file 2>&1;;

		comment) ;;			# add nothing but comment

		vars)
			while [[ -n $1 ]]
			do	
				eval val=\$$1
				echo "$1=($val)" >>$anal_file
			done
			;;
	esac

	echo "" >>$anal_file
}

# the only real way to absolutely delete a file from repmgr (knockes the chance of
# the file becoming an unattached instance as low as possible.
function trash_rm_file
{
	typeset fn=$1
	typeset md5=""
	typeset size=""
	typeset i=0
	typeset node=""

	ng_rm_where -s -p -n -5 $fn | while read node junk md5 size	# find locations
	do
		if (( $i < 1 ))
		then
			echo "file $md5 $fn $size 0" |ng_rmreq		# remove the registration 
			i=1
		fi
	
		verbose "delete file: $node $fn $md5"
		ng_rcmd $node ng_rm_del_file $fn $md5		# delete the phyiscal copy, remove from rmdb and send an uninstance to rm
	done

}

# take $1 and find the path to that file on this node. If it is not here, but is
# on another node, then return the string ERROR. If it is not here, and not 
# known to repmgr either, then we assume we are creating a new file and we return 
# a path to an empty file.
function get_path
{
	typeset fname=$1
	typeset path=""
	typeset fn=""
	typeset unreg_data=""

	ng_rm_where -5 -s $mynode $fname 2>/dev/null | read path unreg_data
	if [[ $path != "MISSING" && ! -z $path ]]
	then
		verbose "found path for $fname: $path $unreg_data"
		echo $path $unreg_data
		return
	fi

	# its missing from this node, lets be sure that it is missing all round
	ng_rm_where -n $fname | read fn nodes
	if [[ -n $nodes ]]
	then
		log_msg "$fname is registered, but  unstable or a copy is not on $mynode: nodes=$nodes path=$path [FAIL]"
		echo "ERROR"
		return
	fi
		
	path=$TMP/log_comb.dummy.$fname.$$		# dummy empy file for sort
	touch $path
	log_msg "created empty file for path=$path [INFO]"
	echo $path $unreg_data
}


# take a hood name (MASTERLOG_yyyymmdd000000) and suss out any fragment files
# (mfrag.<node-name>.<idstring>.yyyymmddd000000) and add them to the master log file 
# (masterlog.yyyymmdd000000) generating a new masterlog file (masterlog.yyyymmdd000000.NEW). 
# Then verify that all of the records from the fragment files are in the new masterlog 
# file.  If verification passes, the new masterlog is registered, and the old log and
# fragment files are deleted.  If we fail, we leave an analysis file and do not delete
# the fragments; all other outputs are scrubbed.
#
function combine
{
	typeset -u hname=$1					# hood name (masterlog_xxxxxxxxx) must be uppercase!
	typeset mlname="$mlprefix.${hname##*_}"			# convert hood name to the master log file name masterlog.xxxxxxxx)
	typeset new_mlpath=""					# spot for new file in repmgr space
	typeset count=0
	typeset fpat=""
	typeset buggered=$TMP/log_comb/log_comb.buggered.${mlname##*.}	# spot where we save off buggered records
	typeset sorted_new=$TMP/log_comb.sortedin.$$		# sorted records from all fragment files (input to update masterlog)
	typeset frag_list=$TMP/log_comb.frag.$$			# fragment files that match current pattern
	typeset unreg_cmds=$TMP/log_comb.unreg.$$		# commands issued to unregister things
	typeset stats_msg=""
	typeset nrec_newml=0					# record count in new masterlog

	anal_file=$TMP/log_comb/log_comb.anal.${mlname##*.}	# this is a global var and contains analysis info in case of failure

	#verbose "hmame=$hname mlname=$mlname"
	fpat="${fpat_pre}.*[.]${mlname##*.}"				# full fragment search pattern based on masterlog name
	
	# suss out mlog files for this masterlog at most only include 2 orphan masterlog files .. too many and we blow up the sort.
	gre $fpat $master_frag_list | awk ' 
		/orphan/ { if( ++ocount < 3 ) print; next;} 
		         { if( ++rcount < 500 ) { print; pcount++} } 	# limit regular files too; else we blow up sort (cmd line args)
		END { 
			if( pcount )
				printf( "\tadding %d of %d total files\n", pcount, rcount ) >"/dev/fd/2" }
	' >$frag_list	

	wc -l <$frag_list | read count
	if (( $count <= 0 ))
	then
		debug " processing fragments for ml=$mlname hood=$hname fragpat=$fpat"
		debug "0 fragment files on this node for $mlname [OK]"
		return 0
	fi

	log_msg " processing fragments for ml=$mlname hood=$hname fragpat=$fpat"
	add2anal "---------------------------- hood=$hname mlname=$mlname frags=$count" comment
	verbose "$count fragment files to be added to $mlname"
	add2anal "fragment file list: $count files" file $frag_list

	
	verbose "searching for master log file: $mlname"
	get_path $mlname | read mlpath mlmd5 mlsize		# path of the old file; creates an empty if not on any node

	if [[ $mlpath == "ERROR" ]]				# an error if not here, but elsewhere, so scram fast
	then
		log_msg "cannot find old master log on this node: $mlname  [FAIL]"
		add2anal "old masterlog exists, but not on this node: $mlname" comment
		return	1
	fi

	add2anal "old masterlog ($mlmd5) path" cmd "ls -al $mlpath"

	# generate file of unique records to add (we truncate any lines that might be longer than we want)
	set -o pipefail
	sort -u -T $TMP  $(awk '{printf( "%s ", $2 );}' <$frag_list ) |awk -v anal=$anal_file '
		{
			if( length( $0 ) > 1440 )
			{
				printf( "truncated: %s\n", $0 ) >>anal;
				printf( "%s<trunc>\n", substr( $0, 1, 1420 ) );		# ng_log should truncate to this; if it did not
			}
			else
				print;
		}
	' >$sorted_new	
	if (( $? != 0 ))
	then
		ditch_file $sorted_new	"sorted input from fragments:"
		log_msg "fragment sort failed: unable to create a sorted new file ($sorted_new) for $mlname"
		return	1
	fi

	add2anal "new sorted path: rec=$(wc -l <$sorted_new)" cmd "ls -al $sorted_new"

	ng_spaceman_nm ${mlname}.NEW | read new_mlpath
	log_msg "creating new masterlog file: $new_mlpath"

	set -o pipefail
	ng_tube sort -T $TMP -u $mlpath $sorted_new !tee $new_mlpath ! ng_md5 |read new_md5 junk		# build new file
	rc=$?
	if (( $rc != 0 ))
	then
		log_msg "merge failed (rc=$rc): unable to create new (merged) file: $mlpath + $sorted_new -> $new_mlpath"
		ditch_file $sorted_new	"sorted input from fragments:"
		ditch_file $new_mlpath "new masterlog:"
		return	1
	fi

	verbose "verifying md5 of new file"
	ng_md5 $new_mlpath | read verify_md5 junk
	if [[ "$verify_md5" != "$new_md5" ]]
	then
		log_msg "new file might have been truncated. md5 verification failed: expected $new_md5, got $verify_md5 [FAIL]"
		log_msg "error analysis file: $anal_file"
		ditch_file $sorted_new	"sorted input from fragments:"
		ditch_file $new_mlpath "new masterlog:"
		return 1
	fi

	verbose "md5 of new file ($new_md5) is good  [OK]"
	
	verbose "verifying new file contains all input records"
	comm -23 $sorted_new $new_mlpath >$buggered	# print lines that appeared only in file 1 (sorted_new -- new input recs)
	rc=$?
	if (( $rc != 0 )) || [[ -s $buggered ]]
	then
		wc -l <$buggered | read count
		log_msg "verification failed (rc=$rc): bad return and/or bugger file not empty: $count record(s)"
		log_msg "buggered file: $buggered; analysis file: $anal_file"
		add2anal "buggered filename: $buggered" comment
		ditch_file $sorted_new	"sorted input from fragments:"
		ditch_file $new_mlpath "new masterlog:"
		return 1
	fi

	verbose "all new fragment records found in new masterlog file; new file is vetted [OK]"


	# all id good, so it is now safe to clean house:
	# 	delete fragments, delete old masterlog, register new, success ng_log message
	#
	ditch_file $buggered "buggered list:"		# not named with $$ so we must force off

	while read node fn md5 size					# fragments can be removed easily as they are not overlaid
	do
		echo "file $md5 ${fn##*/} $size 0" 
	done <$frag_list >$unreg_cmds

	add2anal "files listed for deletion" file $unreg_cmds

	wc -l <$new_mlpath | read nrec_newml
	stats_msg="${new_mlpath/.NEW/} $nrec_newml(nrec) $new_md5(md5) $hname(hood)"
	log_msg "new masterlog stats: $stats_msg"		# to stderr here, stats_msg written to master later if in forreal mode

	if (( $forreal > 0 ))
	then
		register=1				# ok to register unless new md5 matches the old one
		if [[ -n $mlmd5 ]] 			# release old masterlog only if it existed before this run
		then
			if [[ $mlmd5 != $new_md5 ]]	# repmgr has issues if we remove an identical copy, so check and leave if same
			then
				#echo "file $mlmd5 ${mlpath##*/} $mlsize 0" >$unreg_cmds	# this usually leaves an unattached instance
				trash_rm_file ${mlpath##*/}					# remove and prevent unattached instances 
			else
				verbose "old md5 matches new md5; old masterlog left, new file will be deleted [OK]"
				register=0
			fi
		fi

		ng_rmreq <$unreg_cmds			# trash the the fragments

		if (( $register > 0 ))
		then
			mv $new_mlpath ${new_mlpath/.NEW/}
			verbose "registering new masterlog: ${new_mlpath/.NEW/} hood=$hname"
			ng_rm_register -H $hname -v ${new_mlpath/.NEW/}
			ng_log $PRI_INFO $argv0 "new masterlog created: $stats_msg"
		else
			verbose "new masterlog will be scratched because it is identical to old: $mlmd5(old) == $new_md5(new)"
			ditch_file $new_mlpath "new masterlog:"
		fi
	else
		if [[ $mlmd5 == $new_md5 ]]	# repmgr has issues if we remove an identical copy, so check and leave if same
		then
			log_msg "new masterlog file has same md5 value as old [INFO]"
		fi
		add2anal "no execute mode was on, nothing really done" comment
		log_msg "no execute set, did NOT unregister old, nor register the new [OK]"
		ditch_file $sorted_new	"sorted input from fragments:"
		ditch_file $new_mlpath "new masterlog:"
	fi

	verbose "combination complete for $mlname"
}

function delay_some
{
	typeset rval=$(( $RANDOM % 600 ))
	verbose "delay set for $rval seconds"
	sleep $rval
}

# -------------------------------------------------------------------
ustring="[-a] [-k] [-l date] [-p mlprefix] [-n] [-p mlprefix] [-P fragprefix] [-v]"
errors=0
mlprefix=masterlog.${NG_CLUSTER:-bad_cluster}	# prefix of master log (-p to change for testing or parallel operation)
fpat_pre="/mlog[.]"	# lead portion of the fragment file matching pattern allows for testing override (-P to change)
limited=""		# one or more dates to do rather than doing all
show_assigned=0
forreal=1
keep_ifiles=0		# keep intermediate files on error
delay=0			# -d sets and causes us to randomly delay to prevent slamming repmgr on a cluster

if [[ ! -d $TMP/log_comb ]]
then
	if ! mkdir $TMP/log_comb 
	then
		log_msg "unable to create our TMP directory: $TMP/log_comb"
		cleanup 3
	fi
fi

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	show_assigned=1;;
		-d)	delay=1;;
		-k)	keep_ifiles=1;;
		-l)	limited="$limited$2|"; shift;;
		-n)	forreal=0;;
		-p)	mlprefix=$2; shift;;
		-P)	fpat_pre=$2; shift;;


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

if (( $delay > 0 ))
then
	delay_some
fi

mynode=$( ng_sysname )
todo=$TMP/log_comb.todo.$$
limited="${limited:-.*}"
master_frag_list=$TMP/log_comb.allfrag.$$			# list of all fragment files node filename md5 size
typeset -u hood_prefix=${mlprefix%.*}		# hood must be in upper case (repmgr requirement) -- must lose the cluster name

ng_rcmd  $srv_Filer "gre \"$hood_prefix.*:=\" "'$NG_ROOT/site/rm/dump' | awk -v mynode=$mynode ' $NF == mynode { print $1; next } ' >$todo

verbose "creating a list of all fragment files in repmgr space"
ng_rm_where -s -5 -p $mynode "$fpat_pre" >$master_frag_list		# include all orphans here, we limit them later


if (( $show_assigned > 0 ))
then
	log_msg "masterlog files that currently are hooded to $mynode:"
	gre "${limited%\|}" $todo 
	cleanup 0
fi

verbose "log_comb starts"
verbose $(wc -l <$todo)" hoods assigned to this node"

gre "${limited%\|}" $todo | while read f
do
	if ! combine $f
	then
		log_msg "unable to add file(s) for master log file $f [FAIL]"
		if (( $forreal > 0 ))
		then
			ng_log $PRI_ALERT $argv0 "attempt to add files to master log file $f failed. see: $anal_file"
		fi
		errors=$(( $errors + 1 ))
	fi
done 
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log_comb:Combine masterlog fragments and add to masterlog)

&space
&synop	ng_log_comb [-k] [-l yyyymmdd] [-n] [-p masterlog-prefix] [-P frag-prefix] [-v]
&break	ng_log_comb -a [-v]

&space
&desc	&This searches out the fragment files associated with each masterlog file
	that is &ital(assigned) (via a replication manager hood) to the current
	node. The fragment files are combined and then merged with the masterlog
	file. The new masterlog file is then placed in replication manager space
	and registered. 
&space
	There are maintenance scripts that take care of master log files that 
	become unattached in replication manager space.  However, it may at times
	be necessary to manually put a masterlog file back into the mix such that 
	&this will pick it up and merge it with the current masterlog file. To do
	this, the file just needs to be named such that it looks like it is a 
	fragement file, and registered in replication manger space.  To prevent 
	collisions, the following syntax for the name should be used:

&space
&litblkb
   masterlog.<node-name>_orphan.<id-string>.<yyyymmdd000000>
&litblke
&space
	The &ital(id-string)  can be anything that will avoid a collision with 
	orphan files generated by the ningaui maintenance tools.  The current
	system time (ng_dt -U) with some random number appended to it should 
	be fine. The &lit(yyyymmdd) is the date that matches the current 
	logfile being generated. 

&space
&subcat Problem Solving
	Corrupted fragment files cause the most problems with creating a masterlog
	file. If &this detects a problem the masterlog file will not be updated and 
	the fragment files will not be purged.  In this case, running &this manually
	(with the -k and -l options to keep intermediate files and limit processing 
	only to one masterlog file).  The intermediate files will be moved to 
	&lit(TMP/log_comb/keep) for analysis.  The analysis file in &lit(TMP/log_comb)
	will also contain useful information.  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Show the list of masterlog files that are assigned to the node, then quit.
&space
&term	-d : Force a delay of 1 to 600 seconds.  Used when invoking from a scheduled mechanism
	to prevent slamming replicaiton manager from all nodes at once. 
&space
&term	-k : Keep intermediate files when a failure is detected. These files are moved to 
	&lit(TMP/log_comb/keep/) and might overlay existing files. Best used when running 
	manually to solve a problem and with the -l option to prevent overlay.
&space
&term	-l date : Limits the search for fragment files to the date supplied.
	Date is the date associated with the masterlog file (a Monday) in the form of &lit(yyyymmdd.)
	Multiple
	-l options can be supplied. If no -l option is given, all fragments are searched for.
&space
&term	-p prefix : The prefix for masterlog file names. If not given, &lit(masterlog) is used.
&space
&term	-P frag-prefix : Supplies the prefix used for fragment file names. If not supplied, 
	mlog is used. 
space
&term	-v : Verbose mode. 
&endterms


&space
&exit	An exit code indicates error.  When an error is dectected, the fragement files are 
	left in replication manager space, and the old masterlog file is not updated. 

&space
&files
	&This expects a &lit(log_comb) directory to exist in &lit($TMP) and attempts to 
	create one if it does not exist. Into this directory it writes analysis files 
	which are named using the masterlog date that they apply to. The analysis files
	are useful when manual intevention is needed to deal with the in ability to 
	create a merged/new masterlog file.  Analysis files will continue to be 'fresh'
	as long as there are log messages that are found for their associated masterlog 
	file. Once a masterlog file stops being updated, the analysis files will go stale 
	and eventually be deleted by &lit(ng_tmp_cleaner.)

&space
&see	ng_log, ng_logd, ng_log_frag, ng_log_sethoods

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Jan 2007 (sd) : Its beginning of time. 
&term	15 Feb 2007 (sd) : Fixed issue when a new masterlog is identical to old. Repmgr was
		having issues if we cleaned up the old in this case. 
&term	19 Feb 2007 (sd) : Fixed pickup of orphaned masterlog files. It was limiting the 
		files too early in the search.
&term	22 Feb 2007 (sd) : Fixed registration of new masterlog if it matches the old; it does not
		register it as this causes repmgr issues. 
&term	21 Mar 2007 (sd) : Limits the number of mlog fragment files it processes; in the event of 
		a build up of files we were blowing sort out from a command line perspective. 
&term	06 Aug 2007 (sd) : No longer generates the ng_log alert message when running with -n.
&term	11 Mar 2009 (sd) : Added delay so as not to swamp repmgr when all nodes kick a log-comb off.
&term	20 Mar 2009 (sd) : Added cluster name to the filename in the repmgr area. 
		Also updated the man page to add a discussion on naming an orphan file. 
&endterms


&scd_end
------------------------------------------------------------------ */

