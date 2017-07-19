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

# $Id: tp_mkdstjob.ksh,v 1.13 2006/05/23 00:30:33 andrew Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_mkdstjob
# Abstract:	builds a nawab job to be sent from the "destination"
#		cluster to the "source" cluster; the job will:
#		    2) on the destination cluster:
#		    1) schedule a job back to the src cluster to
#		       remove the .cpio file
#		       (if src != dest)
#
#		    2) write the .cpio file to tape
#
#		    3) remove the .cpio file
#		
# Date:		30 Sep 2005
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

export P=$PRI_INFO
export PID=$$
export Prog=$(basename $0)
export ustring="[ -u ] -t pool -j job -c cluster -h hood cpiofile"

v="$Revision: 1.13 $"
v="$(echo "$v" | awk '{ print $2 }')"
ng_log $P $Prog "\$@ = $*"

#
# Parse the command line
#
while [[ $1 == -* ]]
do
	case $1 in 
	--man)	show_man
		cleanup 0
		;;

	-c)	cluster=$2
		shift 2
		;;

	-h)	hood=$2
		shift 2
		;;

	-j)	jobId=$2
		shift 2
		;;

	-t)	tapePool=$2
		shift 2
		;;

	-u)	unique="_"
		shift
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
# -t, -j, -c, -h, and $1 are required
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
elif [[ ! $cluster ]] ; then
    echo "cluster not specified"
    usage
    ng_log $P $Prog "cluster not specified; exiting with 1"
    cleanup 1
elif [[ ! $hood ]] ; then 
    echo "hood not specified"
    usage
    ng_log $P $Prog "hood not specified; exiting with 1"
    cleanup 1
elif [[ $# -ne 1 ]]; then 
    echo "cpio file not given"
    usage
    ng_log $P $Prog "cpio file not given; exiting with 1"
    cleanup 1
fi

cfile=$1

ng_log $P "$Prog(v$v)" "start: $cluster $hood $jobId $tapePool $unique $cfile"

echo "programme tpdwrcpio_${jobId}${unique}"
echo ""
echo "%version = $(echo "$Revision: 1.13 $" | awk '{ print $2 }')"
echo "%pid = $PID"

# Do nothing if src == dest
if [[ $cluster != $NG_CLUSTER ]] ; then
    log="$TMP/log/ng_tp_sched_2.$tapePool.$jobId.\$(ng_dt -u).err"

    echo ""
    echo "submit: \"delete the src .cpio file\""
    echo "  <- %f1 = $(basename $cfile)"
    echo "  cmd ng_tp_sched_2 -t $tapePool -j $jobId -c $NG_CLUSTER %f1 > $log 2>&1"
fi

# log="$TMP/log/ng_tp_writecpio.$tapePool.$jobId.\$(ng_dt -u).err"
# 
# echo ""
# echo "write: \"write the .cpio file to tape\""
# echo "  consumes TPwrcpio=1n"
# echo "  priority = 91"
# echo "  attempts = 1"
# echo "  nodes { $hood }"
# echo "  <- %f2 = $(basename $cfile)"
# echo "  cmd ng_tp_writecpio -r -n %j -t $tapePool -j $jobId %f2 > $log 2>&1"

ng_log $P $Prog "finished; exiting with 0"
cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_mkdstjob:emit a nawab job)

&space
&synop	tp_mkdstjob -t pool -j job -c cluster -h hood cpiofile

&space
&desc
	&This supports certain tape programs and is not intended to be
	called directly by users.

&space
	&This emits a nawab job suitable for being submitted to the
	"destination" cluster; see the output to understand what the job
	will do.

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-c : destination cluster
&space
&term	-h : hood
&space
&term	-j : job id
&space
&term	-t : tape pool
&space
&term	-u : add a trailing underscore ("_") to the end of the nawab
	"programme" name to ensure a unique name is generated
&endterms

&space
&parms
	&This requires that a file name be supplied as a positional     
	parameter.

&space
&exit	Possible exit values:
&begterms
&term	0 : no errors
&space
&term	1 : invalid or missing arguments
&endterms

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	27 Sep 2005 (asm) : Better logging, better error messages.
&term	30 Sep 2005 (asm) : Yet more debugging improvements.
&term	30 Sep 2005 (asm) : Added -n to call to ng_tp_writecpio.
&endterms

&scd_end
------------------------------------------------------------------ */
