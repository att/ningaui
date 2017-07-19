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

# $Id: tp_makecpio.ksh,v 1.21 2009/06/11 18:28:44 andrew Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_makecpio
# Abstract:	take a list of files, create a .cpio file, verify it,
#		and register it with repmgr
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
export ustring="-t pool -j job -o outputfile inputfile"

export N
export T
qtflag=0

ng_log $P $Prog "\$@ = $*"


# -----------------------------------------------------------------------------
#	MAIN PROGRAM
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

	-j)	jobId=$2
		shift 2
		;;

	-o)	ofile=$2
		shift 2
		;;

	-qt)	qtflag=1
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

#
# -t, -j, -o, and $1 are required
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
elif [[ ! $ofile ]] ; then
    echo "output file not specified"
    usage
    ng_log $P $Prog "output file not specified; exiting with 1"
    cleanup 1
elif [[ $# -ne 1 ]]; then
    echo "input file not given"
    usage
    ng_log $P $Prog "input file not given; exiting with 1"
    cleanup 1
fi

ifile=$1

ng_log $P $Prog "MAKE_CPIO_JOB $jobId $ofile"
N="$Prog($jobId)"
T=$(ng_dt -u)

tpMSGFILE=$TMP/log/$Prog.$tapePool.$jobId.$T.log

v="$Revision: 1.21 $"
v="$(echo "$v" | awk '{ print $2 }')"
tpMsg $P "$Prog" "start(v$v): $tapePool $jobId $tpMSGFILE"
tpMsg -d $P "$N" "pid $$ running v$v on $(ng_sysname)"
tpMsg -d $P "$N" "jobId = $jobId"
tpMsg -d $P "$N" "tapePool = $tapePool"
tpMsg -d $P "$N" "ifile = $ifile"

if [[ $qtflag -eq 1 ]]; then
	cpio=$TMP/$ofile
else
	cpio=$(ng_spaceman_nm $ofile~)
fi
tpMsg -d $P "$N" "cpio = $cpio"

# Make our copy of __EXTRA__
xtra=$(get_tmp_nm tpm.$tapePool.$jobId)
dir=$xtra.d
tpMsg -d $P "$N" "xtra = $xtra"
tpMsg -d $P "$N" "dir = $dir"

mkdir $dir
### CHECK ERRORS

echo $(ng_sysname) $T $NG_CLUSTER > $dir/__EXTRA__
tpFileMsg $Prog $dir/__EXTRA__

tpMsg -d $P "$N" "pax -v -w -s ',.*/,,' -x cpio {"
# Use pax to create the archive (in AST-extended cpio format)
(
    echo $dir/__EXTRA__
    cat $ifile
) |
pax -v -w -s ',.*/,,' -x cpio > $cpio 2>> $tpMSGFILE
ret=$?
tpMsg -d $P "$N" "} ret = $ret"

# We don't need this any more
rm -rf $dir

if [[ $ret -ne 0 ]] ; then
    tpMsg $P "$N" "pax exited with $ret; aborting"
    tpMsg $P "$N" "finished with errors; exiting with 2"
    rm -f $cpio
    cleanup 2
fi

if [[ $qtflag -eq 0 ]]; then
	# Verify the archive
	tpMsg -d $P "$N" "ng_cpio_vfy -v {"
	ng_cpio_vfy -v < $cpio >> $tpMSGFILE 2>&1
	ret=$?
	tpMsg -d $P "$N" "} ret = $ret"
	
	if [[ $ret -ne 0 ]] ; then
	    tpMsg $P "$N" "ng_cpio_vfy exited with $ret; aborting"
	    rm -rf $cpio
	    tpMsg $P "$N" "finished with errors; exiting with 3"
	    cleanup 3
	fi
fi

if [[ $qtflag -eq 0 ]]; then
	hood=$tapePool
	tpMsg -d $P "$N" "hood = $hood"
	
	tpMsg -d $P "$N" "ng_rm_register -v -H $hood $cpio {"
	ng_rm_register -v -H $hood $cpio >> $tpMSGFILE 2>&1
	ret=$?
	tpMsg -d $P "$N" "} ret = $ret"
else
	echo $cpio
	ret=0
fi

if [[ $ret -ne 0 ]] ; then
    md5=`ng_md5 < $cpio`
    ng_rm_del_file $cpio $md5			# clean up
    tpMsg $P "$N" "ng_rm_register exited with $ret; aborting"
    tpMsg $P "$N" "finished with errors; exiting with 4"
    cleanup 4
else
    tpMsg $P "$N" "finished; exiting with 0"
    cleanup 0
fi


/* ---------- SCD: Self Contained Documentation -------------------
&scd_start
&doc_title(tp_makecpio:create a cpio archive)

&space
&synop
    tp_makecpio [-qt] -t pool -j job -o outputfile inputfile

&space
&desc
	&This takes a list of files, writes them to a cpio archive
	(using AST pax), verifies the archive and, if correct, registers
	the archive with repmgr.

&space
	&This create the file &lit(__EXTRA__) with the following
	contents: &ital(node name) &ital(time in epoch seconds>) &ital(cluster name>)
	This file will be written as the first file in the archive, and
	is used by &lit(ng_tape_vet) to provide additional information
	about the archive. A private copy of this file is created for
	each archive.

&space
	File paths are not recorded in the archive.

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-j : job id
&space
&term	-o : output file (the archive)
&space
&term	-qt : place the cpio file in $TMP, don't register it or verify it, and echo teh cpio file's name on stdout.
&space
&term	-t : tape pool
&endterms

&space
&parms
	&This requires that a file name be supplied as a positional
	parameter. The list of files to be written to the archive will
	be taken from this file.

&space
&exit	Possible exit values:
&begterms
&term	0 : no errors
&space
&term	1 : bad argument of missing file name
&space
&term	2 : there was an error while creating the archive
&space
&term	3 : the archive could not be verified
&space
&term	4 : there was an error while registering the archive
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
