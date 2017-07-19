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

#-#-#-#
#-#-#-# we need a more graceful exit if no files are specified
#-#-#-#

# $Id: tp_dump.ksh,v 1.44 2006/05/23 00:30:31 andrew Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_dump
# Abstract:	take a list of checksum/basename pairs, split them into
#		groups, create one .cpio file per group, write the .cpio
#		file to tape, and clean up
#		
# Date:		18-Aug-2005
# Author:	Adam Moskowitz
# -----------------------------------------------------------------------------


#
# Load the standard configuration file
#
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}
. $CARTULARY

#
# Load the standard functions
#
STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}
. $STDFUN
export tpMSGFILE

export nCksums=0
export nDumped=0
export nDup=0
export nFailed=0
export nFiles=0
export nJobs=0
export nMissing=0
export nSkipped=0
export segSize=2G
export destCluster=$NG_CLUSTER

export P=$PRI_INFO
export Prog=$(basename $0)
export ustring="[-p] [-f file] [-c cluster] [-s size] -t pool"

export J
export N
export T

ng_log $P $Prog "\$@ = $*"


main () {
    progMsg=0
    xval=0

    #
    # Parse the command line
    #
    while [[ $1 == -* ]]
    do
	case $1 in
	--man)
	    show_man
	    myCleanup 0
	    ;;

	-c)
	    destCluster=$2
	    shift 2
	    ;;

	-f)
	    listFile=$2
	    shift 2
	    ;;

	-p)
	    progMsg=1
	    shift
	    ;;

	-s)
	    segSize=$2
	    shift 2
	    if ! expr "$segSize" : '.*G' > /dev/null ; then
		segSize="${segSize}G"
	    fi
	    ;;

	-t)
	    tapePool=$2
	    shift 2
	    ;;

	-v)
	    shift
	    ;;

	-\?)
	    usage
	    myCleanup 0
	    ;;

	-*)
	    error_msg "unrecognized option: $1"
	    usage
	    ng_log $P $Prog "finished with errors; exiting with 1"
	    myCleanup 1
	    ;;
	esac
    done

    # -t is required
    if [[ ! $tapePool ]] ; then
	echo "${Prog}: tape pool not specified" 1>&2
	usage
	ng_log $P $Prog "tape pool not specified; exiting with 1"
	myCleanup 1
    fi

    # Verify $listFile (if we got it)
    if [[ $listFile ]] ; then
	if [[ ! -f $listFile ]] ; then
	    echo "${Prog}: file not found: $listFile" 1>&2
	    ng_log $P $Prog "file not found: $listFile; exiting with 1"
	    myCleanup 1
	fi

	if [[ ! -s $listFile ]] ; then
	    echo "${Prog}: $listFile is empty" 1>&2
	    ng_log $P $Prog "$listFile is empty; exiting with 1"
	    myCleanup 1
	fi
    fi

    J=$(ng_tp_jobid)
    N="$Prog($J)"
    T=$(ng_dt -u)

    tpMSGFILE=$TMP/log/$Prog.$tapePool.$J.$T.log

    v="$Revision: 1.44 $"
    v="$(echo "$v" | awk '{ print $2 }')"
    tpMsg $P "$Prog(v$v)" "start: $tapePool $J $tpMSGFILE"
    tpMsg -d $P "$N" "pid $$ running v$v on $(ng_sysname)"
    tpMsg -d $P "$N" "jobId = $J"
    tpMsg -d $P "$N" "tapePool = $tapePool"
    if [[ $listFile ]] ; then
	tpMsg -d $P "$N" "listFile = $listFile"
    fi

    #
    # Copy (or write) the list of pairs to a known place; while we're at
    # it, make sure each file is listed exactly once
    #
    files=$(get_tmp_nm tpd-files.$J)
    if [[ $listFile ]] ; then
	sort +0 -2 $listFile |
	myUniq > $files
    else
	listFile=$(get_tmp_nm tpd-list.$J)
	sort +0 -2 - |
	tee $listFile |
	myUniq > $files

	if [[ ! -s $listFile ]] ; then
	    echo "${Prog}: no files on stdin" 1>&2
	    tpMsg $P $Prog "no files on stdin exiting with 1"
	    myCleanup 1
	fi
    fi

    # Count how many files and how many dups
    nFiles=$(gre -c . $listFile)
    nDup=$(( $nFiles - $(gre -c . $files) ))
    tpMsg $P "$N" "nFiles = $nFiles, nDup = $nDup"

    # Make a bunch of temp files
    tmpDump=$(get_tmp_nm tpd-dump1.$J)
    tmpJoin1=$(get_tmp_nm tpd-join-1.$J)
    tmpJoin2=$(get_tmp_nm tpd-join-2.$J)
    tmpStrs=$(get_tmp_nm tpd-strs.J)

    # Mung the list into a form suitable for join(1)
    awk '{ printf("%s.%s %s\n", $1, $2, $0) }' $files |
    sort +0 -1 > $tmpJoin1

    # Submit the files to ng_rmreq using "dump1"
    rm -f $files.x[a-z][a-z][a-z]
    split -a 3 -l 25000 $files $files.x
    > $tmpDump
    for f in $files.x[a-z][a-z][a-z] ; do
	awk '{ print "dump1", $2 }' $f |
	ng_rmreq -s $srv_Filer -a >> $tmpDump
    done
    rm -f $files.x[a-z][a-z][a-z]

    # Count how many files we didn't find
    nMissing=$(gre -c '^not found:' $tmpDump)
    tpMsg $P "$N" "nMissing = $nMissing"

    # Another munging for join
    gre '^file ' $tmpDump |
    awk '{ printf("%s.%s %s\n", $3, $2, $0) }' |
    sed -e 's/^md5=//' -e 's/: / /' |
    sort +0 -1 > $tmpJoin2

    # Use join to find the unmatched checksums
    join -1 1 -2 1 -v 2 -o 2.1,2.2,2.3 $tmpJoin1 $tmpJoin2 |
    sed 's/^\([^.]*\)..* file \(.*\)/file \2 md5=\1 /' > $tmpStrs
    nCksums=$(gre -c . $tmpStrs)
    tpMsg $P "$N" "nCksums = $nCksums"
    echo '^not found: ' >> $tmpStrs

    # Remove missing and unmatched files from the list
    gre -v -F -f $tmpStrs $tmpDump > $files
    nDumped=$(gre -c . $files)
    tpMsg $P "$N" "(preliminary) nDumped = $nDumped"

    #
    # Massage the list of files into a form acceptable to ng_admeasure
    # (node basename size)
    #
    > $tmpDump
    gre '^file .*#' $files |
    awk '
    {
	n = index($0, "size=")
	if (n == 0) {
	    next
	}

	n += 5
	sz = substr($0, n) + 0
	n = split($0, xx, "#")
	for (i = 2; i <= n; i++) {
	    if (xx[i] ~ /^[^ ]+ \/[^ ]+$/) {
		b = xx[i]
		sub(".*/", "", b)
		sub(" .*", "", b)
		printf("%s %s %.0f\n", b, xx[i], sz)
	    }
	}
    }' |
    sort |
    sed 's/[^ ]* //' > $tmpDump

    #
    # Make sure we got something
    #
    if [[ ! -s $tmpDump ]] ; then
	nDumped=0
	endMessage $progMsg
	tpMsg $P "$N" "finished; no files to dump; exiting with 2"
	myCleanup 2
    fi

    #
    # Make a directory to hold the file lists
    #
    tmpdir=$TMP/tpd-$tapePool.$J
    tpMsg -d $P "$N" "tmpdir = $tmpdir"

    tpMsg -d $P "$N" "mkdir -p $tmpdir {"
    mkdir -p $tmpdir >> $tpMSGFILE 2>&1
    ret=$?
    tpMsg -d $P "$N" "} ret = $ret"

    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "mkdir exited with $ret; aborting"
	echo "${Prog}: mkdir exited with $ret; aborting" 1>&2
	tpMsg $P "$N" "finished with errors; exiting with 3"
	myCleanup 3
    fi

    #
    # Use size and node to split the files into groups
    #
    groups=$(get_tmp_nm tpd-groups.$J)
    cmd="ng_admeasure -f -v -o $groups -g $segSize $tmpdir/$tapePool.${J}--"
    tpMsg -d $P "$N" "$cmd < $tmpDump {"
    ng_admeasure -f -v -o $groups -g $segSize $tmpdir/$tapePool.${J}-- \
	< $tmpDump >> $tpMSGFILE 2>&1
    ret=$?
    tpMsg -d $P "$N" "} ret = $ret"

    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "ng_admeasure exited with $ret; aborting"
	echo "${Prog}: ng_admeasure exited with $ret; aborting" 1>&2

	file=$TMP/$Prog.$tapePool.$J.T.files
	mv $tmpDump $file
	echo "${Prog}: ng_admeasure input saved in $file" 1>&2

	tpMsg $P "$N" "finished with errors; exiting with 4"
	myCleanup 4
    fi

    nJobs=$(grep -c . $groups)
    tpMsg $P "$N" "nJobs = $nJobs"

    #
    # Get srv_Jobber's $TMP
    #
    jobtmp=$(ng_rcmd $srv_Jobber echo '$TMP')
    tpMsg -d $P "$N" "jobtmp = $jobtmp"
    if [[ ! $jobtmp ]] ; then
	tpMsg $P "$N" "unable to get \$TMP for srv_Jobber ($srv_Jobber)"
	echo "${Prog}: unable to get \$TMP for srv_Jobber ($srv_Jobber)" 1>&2
	tpMsg $P "$N" "finished with errors; exiting with 5"
	myCleanup 5
    fi

    #
    # Process each group
    #
    nFailed=0
    nSkipped=0
    while read f ; do
	f2=$(basename $f)
	seqno=$(ng_tp_n32a $(echo $f2 | sed 's/.*--//'))
	bf=$(echo $f2 | sed "s/--.*/_$seqno/")

	# Save the node
	node=$(sed -n 1p $f)

	# Strip the node line and the paths
	sed -n 's,.*/,,p' $f > $tmpdir/tmp$$
	mv $tmpdir/tmp$$ $f

	# If we're running on srv_Jobber just use the local file
	if [[ $(ng_sysname) == $srv_Jobber ]] ; then
	    ftmp=$f
	else
	    # Copy the file list to srv_Jobber
	    tpMsg -d $P "$N" "ng_ccp -d $jobtmp/$bf.flist $srv_Jobber $f {"
	    ng_ccp -d $jobtmp/$bf.flist $srv_Jobber $f >> $tpMSGFILE 2>&1
	    ret=$?
	    tpMsg -d $P "$N" "} ret = $ret"

	    if [[ $ret -ne 0 ]] ; then
		printf "%s: unable to copy %s to %s; continuing\n" \
		    $Prog $f $srv_Jobber 1>&2
		nSkipped=$(( $nSkipped + $(gre -c . $f) ))
		tpMsg $P "$N" "nSkipped = $nSkipped"
		rm -f $f
		xval=7
		continue
	    fi

	    ftmp=$jobtmp/$bf.flist
	fi

	# Create a job file
	t=$tmpdir/tmp$$
	mkJob $tapePool ${J}_$seqno $ftmp > $t
	tpFileMsg $Prog $t

	# Submit the job
	tpMsg -d $P "$N" "ng_nreq -v -l < $t {"
	ng_nreq -v -l < $t >> $tpMSGFILE 2>&1
	ret=$?
	tpMsg -d $P "$N" "} ret = $ret"

	if [[ $ret -ne 0 ]] ; then
	    fmt1="%s: error submitting job %s;"
	    fmt2="ng_nreq returned %d; continuing"
	    printf "$fmt1 $fmt2\n" $Prog tpdmkcpio_${J}_$seqno $ret
	    xval=8
	    nFailed=$(( $nFailed + 1 ))
	    nSkipped=$(( $nSkipped + $(gre -c . $f) ))
	    tpMsg $P "$N" "nFailed = $nFailed, nSkipped = $nSkipped"
	    tpMsg $P "$N" "job=${J}_$seqno target=$node FAILED"
	fi

	# Log where this job is targeted
	tpMsg $P "$N" "job=${J}_$seqno target=$node OK"

	# Be good citizens and clean up our own mess
	rm -f $t $f
    done < $groups

    #
    # Clean up and go home
    #
    endMessage $progMsg
    tpMsg $P "$N" "finished; exiting with $xval"
    myCleanup $xval
} # main


endMessage () {
    mtype=$1

    vars="$nFiles $nDup $nMissing $nCksums $nSkipped $nDumped $nJobs"
    vars="$vars $nFailed"

    nAccepted=$(( $nJobs - $nFailed))

    str="id=$J"
    str="$str requested=$nFiles"
    str="$str duplicate=$nDup"
    str="$str missing=$nMissing"
    str="$str cksum=$nCksums"
    str="$str skipped=$nSkipped"
    str="$str dumped=$nDumped"
    str="$str jobs=$nJobs"
    str="$str failed=$nFailed"
    str="$str accepted=$nAccepted"

    tpMsg $P "$N" "FINAL: $str"

    if [[ $mtype -eq 1 ]] ; then
	echo "$str"
    else
	width=$(
	    echo $vars |
	    tr ' ' '\012' |
	    awk '{ print length($0) + 2 }' |
	    sort -n |
	    tail -1
	)

	printf "Job ID $J\n"
	printf -- "----------------------\n"
	printf "%${width}s files requested\n" $nFiles

	if [[ $x -gt 0 ]] ; then
	    printf "%${width}s duplicate files\n" $nDup
	fi

	if [[ $x -gt 0 ]] ; then
	    printf "%${width}s names not found\n" $nMissing
	fi

	if [[ $x -gt 0 ]] ; then
	    printf "%${width}s checksums not found\n" $nCksums
	fi

	if [[ $x -gt 0 ]] ; then
	    printf "%${width}s files skipped\n" $nSkipped
	fi

	printf "%${width}s files dumped\n" $nDumped

	if [[ $nDumped -gt 0 ]] ; then
	    printf "%${width}s jobs submitted\n" $nJobs

	    if [[ $x -gt 0 ]] ; then
		printf "%${width}s jobs failed\n" $nFailed
	    fi

	    printf "%${width}s jobs accepted\n" $nAccepted
	fi
    fi
} # endMessage


#
# $1 -> pool
# $2 -> job
# $3 -> flist
#
mkJob () {
    echo "programme tpdmkcpio_$2"

    log="$TMP/log/ng_rcmd.$1.$2.\$(ng_dt -u).err"
    #
    # It's safe to delete the flist file now, because nawab uses it
    # when it *accepts* the job; by the time the job actually *runs*
    # we're done with the file. On the other hand, we need to do this
    # only if we're not running on srv_Jobber.
    #
    if [[ $(ng_sysname) != $srv_Jobber ]] ; then
	echo ""
	echo "cleanup: \"delete the flist file on srv_Jobber\""
	echo "  consumes TPflist=2n"
	echo "  priority = 91"
	echo "  cmd ng_rcmd $srv_Jobber rm -f $3 > $log 2>&1"
    fi

    log="$TMP/log/ng_tp_makecpio.$1.$2.\$(ng_dt -u).err"

    echo ""
    echo "create: \"create the .cpio file\""
    echo "  consumes TPmkcpio=1n"
    echo "  priority = 91"
    echo "  attempts = 1"
    echo "  nodes = { !Satellite }"
    echo "  <- %files = file ( \`\"cat $3\"\` )"
    echo "  -> %cpio = pf-tape-$1.$2.cpio"
    echo "  cmd ng_tp_makecpio -t $1 -j $2 -o %cpio %files > $log 2>&1"

    log="$TMP/log/ng_tp_sched_1.$1.$2.\$(ng_dt -u).err"

    echo ""
    echo "schedule: after create \"schedule the copy and delete\""
    echo "  consumes TPsched"
    echo "  priority = 91"
    echo "  nodes = { !Satellite }"
    echo "  <- %cpio"
    echo "  cmd ng_tp_sched_1 -t $1 -j $2 -c $destCluster %cpio > $log 2>&1"
} # mkJob


myCleanup () {
    if [[ $tmpdir ]] && [[ -d $tmpdir ]] ; then
	rm -rf $tmpdir
    fi

    cleanup $1
} # myCleanup


myUniq () {
    awk '{
	pair = $1 $2
	if (pair != prev) {
		print $0
		prev = pair
	}

    }'
} # myUniq


main "$@"


/* ---------- SCD: Self Contained Documentation -------------------
&scd_start
&doc_title(tp_dump:write a list of files to tape)

&space
&synop
	tp_dump [-p] [-f file] [-s size] -t pool

&space
&desc
	&This takes a list of checksum/basename pairs (in that order,
	separated by spaces), breaks them into groups of appropriate size
	and locality, creates cpio archives for each group, write the
	archives to tape, and cleans up. All files are considered to
	belong to the tape pool specified with the "-t" switch. &This
	accomplishes all this by submitting nawab jobs to run the
	programs that do the actual work (including tp_makecpio,
	tp_sendcpio, and tp_writecpio).

	If "-f" is not given, the list of files is read from stdin.

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-f : The file containing the list of files to be dumped.
&space
&term	-p : When exiting, show the status message in a form that's
	easily parsed; the default is to show a human-readable message.
	In all cases the -p form of the message is logged.
&space
&term	-s : Segment size (in GB); default is 2GB.
&space
&term	-t : Tape pool.
&endterms

&space
&exit	Possible exit values:
&begterms
&term	0 : no errors
&space
&term	1 : bad argument, no files specified, or unknown tape pool
&space
&term	2 : error while building admeasure data
&space
&term	3 : unable to create temporary directory
&space
&term	4 : error from ng_admeasure
&space
&term	5 : unable to get srv_Jobber for "destination" cluster
&space
&term	6 : some files not on local cluster
&space
&term	7 : error copying one or more groups to srv_Jobber; some files
	will not be written to tape
&space
&term	8 : error submitting one or more jobs to nawab; some files will
	not be written to tape
&endterms

&space
&see	tp_jobid, tp_makecpio, tp_mkdstjob, tp_poolinfo, tp_sched_1,
	tp_sched_2, tp_sendcpio, tp_writecpio

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	26 Sep 2005 (asm) : New input format, fuller/more correct file
	file selection, limited number files per ng_rmreq call.
&term	27 Sep 2005 (asm) : Tweaks, better logging, end message.
&term	28 Sep 2005 (asm) : Fixed bug (shell expansion doesn't do what I
	want it to do.
&term	30 Sep 2005 (asm) : Changed calling sequence for tpFileMsg;
	resolved ng_log() v. tpMsg(); more/better debugging info.
&term	25 Oct 2005 (asm) : Save ng_admeasure input on error.
&term	28 Oct 2005 (asm) : More logging, specifically for "dump
	explain"
&endterms

&scd_end
------------------------------------------------------------------ */
