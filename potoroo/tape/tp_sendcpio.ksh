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

# $Id: tp_sendcpio.ksh,v 1.15 2009/07/14 17:37:16 andrew Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_sendcpio
# Abstract:	take a .cpio file and send it to the "destination"
#		cluster (using ng_fcp)
#
#		if this is the "destination" cluster, do nothing
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


export P=$PRI_INFO
export Prog=$(basename $0)
export ustring="-t pool -j job -c cluster cpiofile"

export N
export T

ng_log $P $Prog "\$@ = $*"


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

	-c)	cluster=$2
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
# -c, -t, -j, and $1 are required
#
if [[ ! $cluster ]] ; then 
    echo "cluster not specified"
    usage
    ng_log $P $Prog "cluster not specified; exiting with 1"
    cleanup 1
elif [[ ! $tapePool ]] ; then 
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

v="$Revision: 1.15 $"
v="$(echo "$v" | awk '{ print $2 }')"
tpMsg $P "$Prog(v$v)" "start: $tapePool $jobId $tpMSGFILE"
tpMsg -d $P "$N" "pid $$ running v$v on $(ng_sysname)"
tpMsg -d $P "$N" "jobId = $jobId"
tpMsg -d $P "$N" "tapePool = $tapePool"
tpMsg -d $P "$N" "cfile = $cfile"
tpMsg -d $P "$N" "cluster = $cluster"
hood=$tapePool
tpMsg -d $P "$N" "hood = $hood"

if [[ $cluster == $NG_CLUSTER ]] ; then
    tpMsg $P "$N" "$cluster == $NG_CLUSTER; no action required"
    tpMsg $P "$N" "finished; exiting with 0"
    cleanup 0

else
    tpMsg $P "$N" "copying $(basename $cfile) to $cluster"
    tpMsg -d $P "$N" "ng_fcp -v -c $cluster -H $hood $cfile {"
    ng_fcp -v -c $cluster -H $hood $cfile >> $tpMSGFILE 2>&1
    ret=$?
    tpMsg -d $P "$N" "} ret = $ret"

    if [[ $ret -ne 0 ]] ; then
	tpMsg $P "$N" "ng_fcp exited with $ret; aborting"
	tpMsg $P "$N" "finished with errors; exiting with 2"
	cleanup 2
    else
	tpMsg $P "$N" "finished; exiting with 0"
	cleanup 0
    fi
fi


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_sendcpio:send a cpio archive to the destination cluster)

&space
&synop
	tp_sendcpio -t pool -j job -c cluster cpiofile

&space
&desc
	&This looks for tape pool information in &lit(TP_Pool_Where)
	and, if found, sends the cpio archive to the specified cluster
	(the "destination" cluster). If the current cluster happens to
	be the destination cluster, &this simply logs that fact and
	exits.

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
	&This requires that a file name be supplied as a positional
	parameter.

&space
&exit	Possible exit values:
&begterms
&term	0 : no errors
&space
&term	1 : bad argument or missing archive name
&space
&term	2 : errors copying archive to destination cluster
&endterms

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	27 Sep 2005 (asm) : Better logging, better error messages.
&term	28 Sep 2005 (asm) : Fixed bug (shell expansion doesn't do what I
	want it to do.
&term	30 Sep 2005 (asm) : Resolved ng_log() v. tpMsg(); more/better
	debugging info.
&endterms

&scd_end
------------------------------------------------------------------ */
