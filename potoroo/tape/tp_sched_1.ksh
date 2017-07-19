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

# $Id: tp_sched_1.ksh,v 1.20 2009/07/14 17:37:16 andrew Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_sched_1
# Abstract:	schedules the following jobs:
#		    1) on the source cluster:
#			- copy the .cpio file to the destination cluster
#			  (if src != dest)
#
#		    2) on the destination cluster:
#			- schedule a job back to the src cluster to
#			  remove the .cpio file
#			  (if src != dest)
#
#			- write the .cpio file to tape
#
#			- remove the .cpio file
#		
# Date:		18 Aug 2005
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
export ustring="-t pool -j job cpiofile"

export N
export T

ng_log $P $Prog "\$@ = $*"


# -----------------------------------------------------------------------------
#       FUNCTIONS
# -----------------------------------------------------------------------------


### FIX THIS!

#
# $1 -> pool
# $2 -> job
# $3 -> cpio
#
mkSrcJob () {
    log="$TMP/log/ng_tp_sendcpio.$1.$2.\$(ng_dt -u).err"

    echo "programme tpdcpcpio_$2"
    echo ""
    echo "copy: \"copy the .cpio file to the tape cluster\""
    echo "  consumes TPcpcpio=1n"
    echo "  priority = 91"
    echo "  attempts = 5"
    echo "  <- %cpio = $(basename $3)"
    echo "  cmd ng_tp_sendcpio -t $1 -j $2 -c $4 %cpio > $log 2>&1"
} # mkSrcJob


# -----------------------------------------------------------------------------
#       MAIN PROGRAM
# -----------------------------------------------------------------------------


#
# Parse the command line
#
while [[ $1 == -* ]]
do
	case $1 in 
	--man)	show_man
		cleanup 0
		;;

	-c)	destCluster=$2
		shift 2
		;;

	-j)	jobId=$2
		shift 2
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

#
# -t, -j, and $1 are required
#
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

v="$Revision: 1.20 $"
v="$(echo "$v" | awk '{ print $2 }')"
tpMsg $P "$Prog(v$v)" "start: $tapePool $jobId $tpMSGFILE"
tpMsg -d $P "$N" "pid $$ running v$v on $(ng_sysname)"
tpMsg -d $P "$N" "jobId = $jobId"
tpMsg -d $P "$N" "tapePool = $tapePool"
tpMsg -d $P "$N" "cfile = $cfile"

#
# Get the src and dest clusters
#
here=$NG_CLUSTER
#set -- $(ng_tp_poolinfo $tapePool)
cluster=$destCluster
hood=$tapePool
tpMsg -d $P "$N" "here = $here"
tpMsg -d $P "$N" "cluster = $cluster"
tpMsg -d $P "$N" "hood = $hood"

# Temp file to hold jobs
tmp=$(get_tmp_nm tpc.$tapePool.$jobId)
tpMsg -d $P "$N" "tmp = $tmp"

#
# Decide what to do
#
if [[ $here != $cluster ]] ; then
    tpMsg $P "$N" "running tp_sendcpio (via nawab)"

    mkSrcJob $tapePool $jobId $cfile $cluster > $tmp
    tpFileMsg $Prog $tmp

    tpMsg -d $P "$N" "ng_nreq -v -l < $tmp {"
    ng_nreq -v -l < $tmp >> $tpMSGFILE 2>&1
    ret=$?
    tpMsg -d $P "$N" "} ret = $ret"

    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "ng_nreq exited with $ret; aborting"
	tpMsg $P "$N" "finished with errors; exiting with 2"
	cleanup 2
    fi
fi

#
# Always submit a job to the destination cluster
#
cp /dev/null $tmp
tpMsg $P "$N" "running tp_writecpio (via nawab on $cluster)"
tpMsg -d $P "$N" \
    "ng_tp_mkdstjob -t $tapePool -j $jobId -c $cluster -h $hood $cfile"
ng_tp_mkdstjob -t $tapePool -j $jobId -c $cluster -h $hood $cfile > $tmp
tpFileMsg $Prog $tmp

tpMsg -d $P "$N" "ng_nreq -v -c $cluster -l < $tmp {"
ng_nreq -v -c $cluster -l < $tmp >> $tpMSGFILE 2>&1
ret=$?
tpMsg -d $P "$N" "} ret = $ret"

if [[ $ret -ne 0 ]] ; then
    tpMsg $P "$N" "ng_nreq exited with $ret; aborting"
    tpMsg $P "$N" "finished with errors; exiting with 2"
    cleanup 2
else
    tpMsg $P "$N" "finished; exiting with 0"
    cleanup 0
fi


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_sched_1:schedule nawab jobs)

&space
&synop
	tp_sched_1 -t pool -j job cpiofile

&space
&desc
	&This schedules nawab jobs to send a cpio archive to the
	"destination" cluster, write the archive to tape, and delete the
	archive on the "source" cluster

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-j : job id
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
&term	0 : no errors
&space
&term	1 : bad argument of missing file name
&space
&term	2 : unable to schedule job(s)
&endterms

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	27 Sep 2005 (asm) : Better logging, better error messages.
&term	28 Sep 2005 (asm) : Fixed bug (shell expansion doesn't do what I
	want it to do.
&term	30 Sep 2005 (asm) : Changed calling sequence for tpFileMsg;
	resolved ng_log() v. tpMsg(); more/better debugging info.
&endterms

&scd_end
------------------------------------------------------------------ */
