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

# $Id: tp_writecpio.ksh,v 1.26 2005/10/19 20:34:38 adamm Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_writecpio
# Abstract:	take a .cpio file and write it to tape
#
#		if no tape is available, or we hit EOT, or if we get a
#		tape write error, reschedule the job
#
#		if the write succeeds and we're invoked with -r, remove
#		(unregister) the .cpio file
#		
# Date:		03 Oct 2005
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


export P=$PRI_INFO
export Prog=$(basename $0)
export ustring="[-r] [-n name] -t pool -j job cpiofile"

export N
export T

ng_log $P $Prog "\$@ = $*"


# -----------------------------------------------------------------------------
#       FUNCTIONS
# -----------------------------------------------------------------------------


#
# $1 -> tapePool
#
# Returns:
#   0 -> got a valid tape
#   1 -> no pool info
#   2 -> no tape for pool
#   3 -> no tape for pool on this node
#
getTape () {
    pool=$1

    # See what tp_poolinfo has to say
    set -- $(ng_tp_poolinfo $pool)

    # No results means an unkown pool
    if [[ $# -eq 0 ]] ; then
	return 1

    # $# < 4 means no tapes are associated with this pool (now)
    elif [[ $# -lt 4 ]] ; then
	return 2
    fi

    # Get our node name
    node=$(ng_sysname)

    # See if any of the tapes are mounted here
    shift 2
    set -A tmp $*
    n=$(( $# / 2 ))

    i=0
    while [[ $i -lt $n ]] ; do
	j=$(( $i * 2 ))
	set -- $(eval echo \$${tmp[$j]} | tr ',' ' ')
	if [[ $1 == $node ]] ; then
	    break
	fi

	i=$(( $i + 1 ))
    done

    # Did we find one?
    if [[ $i -eq $n ]] ; then
	return 3
    fi

    # Show what we got
    echo ${tmp[$j]} $1 $2 ${tmp[$(( $j + 1 ))]}
} # getTape


#
# $1 -> tape pool
# $2 -> job id
# $3 -> file
#
resched () {
    tpMsg $P "$N" "rescheduling tp_writecpio for file $f"

    t=$1
    j=$2
    f=$3
    tpMsg -d $P "$N" "t = $t"
    tpMsg -d $P "$N" "j = $j"
    tpMsg -d $P "$N" "f = $f"

    tpMsg -d $P "$N" "ng_tp_poolinfo $t = $(ng_tp_poolinfo $t)"
    set -- $(ng_tp_poolinfo $t)
    c=$1
    h=$2
    tpMsg -d $P "$N" "c = $c"
    tpMsg -d $P "$N" "h = $h"

    tmp=$(get_tmp_nm tpw.$t.$j)
    tpMsg -d $P "$N" \
	"ng_tp_mkdstjob -u -t $t -j $j -c $c -h $h $f"
    ng_tp_mkdstjob -u -t $t -j $j -c $c -h $h $f > $tmp
    tpFileMsg $Prog $tmp

    tpMsg -d $P "$N" "ng_nreq -v -c $c -l < $tmp {"
    ng_nreq -v -c $c -l < $tmp >> $tpMSGFILE 2>&1
    ret=$?
    tpMsg -d $P "$N" "} ret = $ret"

    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "ng_nreq exited with $ret; aborting"
	return $ret
    else
	return 0
    fi
} # resched


# -----------------------------------------------------------------------------
#       MAIN PROGRAM
# -----------------------------------------------------------------------------


remove=0

#
# Parse the command line
#
while [[ $1 == -* ]]
do
	case $1 in 
	--man)	show_man
		cleanup 0
		;;

	-j)	jobId=$2
		shift 2
		;;

	-n)	jobName=$2
		shift 2
		;;

	-r)	remove=1
		shift
		;;

	-t)	tapePool=$2
		shift 2
		;;

	-v)	shift
		;;

	-\?)	usage
		cleanup 0
		;;

	-*)	error_msg "unrecognized option: $1"
		usage
		ng_log $P $Prog "finished with errors; exiting with 1"
		cleanup 1
		;;
	esac
done

if [[ ! $tapePool ]] ; then 
    echo "tape pool not specified"
    usage
    ng_log $P $Prog "tape pool not specified; exiting with 1"
    cleanup 1
elif [[ ! $jobId ]] ; then 
    echo "job id not specified"
    usage
    ng_log $P $Prog "job id not specified; exiting with 1"
    cleanup 1
elif [[ $# -ne 1 ]]; then 
    echo "cpio file not given"
    usage
    ng_log $P $Prog "cpio file not given; exiting with 1"
    cleanup 1
fi

cfile=$1

N="$Prog($jobId)"
T=$(ng_dt -u)

tpMSGFILE=$TMP/log/$Prog.$tapePool.$jobId.$T.log

v="$Revision: 1.26 $"
v="$(echo "$v" | awk '{ print $2 }')"
tpMsg $P "$Prog(v$v)" "start: $tapePool $jobId $tpMSGFILE"
tpMsg -d $P "$N" "pid $$ running v$v on $(ng_sysname)"
tpMsg -d $P "$N" "jobId = $jobId"
if [[ $jobName ]] ; then
    tpMsg -d $P "$N" "jobName = $jobName"
fi
tpMsg -d $P "$N" "tapePool = $tapePool"
tpMsg -d $P "$N" "cfile = $cfile"

#
# Try (twice, with a pause) to get our tape
#
s="$(getTape $tapePool)"
ret=$?
if [[ $ret -ne 0 ]] ; then
    sleep 300
    s="$(getTape $tapePool)"
    ret=$?
fi
tpMsg -d $P "$N" "getTape() returned $ret"

#
# Reschedule if no tape was found
#
if [[ $ret -ne 0 ]] ; then
    tpMsg $P "$N" "no tape available for pool $tapePool; rescheduling"

    resched $tapePool $jobId $cfile
    ret=$?
    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "resched() exited with $ret; aborting"
	tpMsg $P "$N" "finished with errors; exiting with 2"
	cleanup 2
    else
	tpMsg $P "$N" "finished; exiting with 0"
	cleanup 0
    fi
fi

# Get on with it
set -- $s
drive=$1
node=$2
device=$3
volid=$4

tpMsg -d $P "$N" "drive_id = $drive"
tpMsg -d $P "$N" "node = $node"
tpMsg -d $P "$N" "device = $device"
tpMsg -d $P "$N" "volid = $volid"

#
# Write a segment to tape
#
tpMsg -d $P "$N" "ng_wrseg -v -s -i $drive $volid $device $cfile {"
ng_wrseg -v -s -i $drive $volid $device $cfile >> $tpMSGFILE 2>&1
ret=$?
tpMsg -d $P "$N" "} ret = $ret"

#
# Check the return code and decide what to do
#
case $ret in
0)	# success
    tpMsg $P "$N" "ng_wrseg -i $drive $volid $device $cfile; OK"
    if [[ $remove -eq 1 ]] ; then
	tpMsg $P "$N" "unregistering $(basename $cfile)"
	tpMsg -d $P "$N" "ng_rm_register -v -u -f $(basename $cfile) {"
	ng_rm_register -v -u -f $(basename $cfile) >> $tpMSGFILE 2>&1
	ret=$?
	tpMsg -d $P "$N" "} ret = $ret"

	if [[ $ret -ne 0 ]] ; then
	    tpMsg $P "$N" "ng_rm_register -v -u -f $(basename $cfile); FAILED"
	fi
    fi
    tpMsg $P "$N" "finished; exiting with 0"
    cleanup 0
    ;;

1)	# error, but not a write error
    tpMsg $PRI_ERROR "$N" "ng_wrseg returned 1; results in $tpMSGFILE"
    tpMsg $PRI_ERROR "$N" "job will not be rescheduled"
    tpMsg $P "$N" "finished with errors; exiting with 3"
    cleanup 3
    ;;

4)	# write error
    tpMsg $PRI_CRIT "$N" "ng_wrseg returned 4; results in $tpMSGFILE"
    tpMsg $PRI_CRIT "$N" "ejecting tape $volid; job will be rescheduled"
    tpMsg -d $P "$N" "ng_tp_eject $tapePool $volid $drive $device"
    ng_tp_eject $tapePool $volid $drive $device

    resched $tapePool $jobId $cfile
    ret=$?
    if  [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "resched() exited with $ret; aborting"
	tpMsg $P "$N" "finished with errors; exiting with 2"
	cleanup 2
    else
	tpMsg $P "$N" "finished; exiting with 0"
	cleanup 0
    fi
    ;;

5)	# EOT
    tpMsg $P "$N" "ng_wrseg -i $drive $volid $device $cfile; EOT"
    tpMsg $P "$N" "ejecting tape $volid; job will be rescheduled"
    tpMsg -d $P "$N" "ng_tp_eject $tapePool $volid $drive $device"
    ng_tp_eject $tapePool $volid $drive $device
    resched $tapePool $jobId $cfile
    ret=$?
    if  [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "resched() exited with $ret; aborting"
	tpMsg $P "$N" "finished with errors; exiting with 2"
	cleanup 2
    else
	tpMsg $P "$N" "finished; exiting with 0"
	cleanup 0
    fi
    ;;
esac


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_writecpio:write a cpio archive to tape)

&space
&synop
	tp_writecpio [-r] -t pool -j job cpiofile

&space
&desc
	&This writes a cpio archive to tape and (optionally) deletes the
	archive if the write is successful. &This will automatically
	determine which tape and drive to use (based on TP_Pool_Where).

&space
	If &this encounters a tape error or hits EOT, the tape will be
	ejected and the job rescheduled; other errors are logged and the
	job is not rescheduled.

&space
	In order to cooperate with nawab, an exit value of zero (0) may
	indicate that the job completed successfully -- or it may mean
	that the job failed and was successfulyl rescheduled. Other than
	reading the log file entries there is no way to distinguish
	between these two outcomes.

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-j : job id
&space
&term	-n : seneschal job name
&space
&term	-r : remove the archive if the write to tape succeeds
&space
&term	-t : tape pool
&endterms

&space
&parms
	&This requires that the name of the cpio archive be supplied as
	a positional parameter.

&space
&exit	Possible exit values:
&begterms
&term	0 : "didn't fail"
&space
&term	1 : bad argument or missing file name
&space
&term	2 : couldn't reschedule job
&space
&term	3 : some error other than a tape write error or EOT
&endterms

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	27 Sep 2005 (asm) : Fixed a bug in the rescheduling logic,
	better logging, better error messages.
&term	28 Sep 2005 (asm) : Fixed bug (shell expansion doesn't do what I
	want it to do.
&term	29 Sep 2005 (asm) : Added -v and -s to wrseg for even more
	debugging output.
&term	30 Sep 2005 (asm) : Changed calling sequence for tpFileMsg;
	resolved ng_log() v. tpMsg(); more/better debugging info.
&term	30 Sep 2005 (asm) : Added -n
&term	03 Oct 2005 (asm) : Fixed fixed a bug in how ng_tp_eject was
	called.
&endterms

&scd_end
------------------------------------------------------------------ */
