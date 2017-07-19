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

###
### check for no dates!
###
### check for $? = 1 from ng_dateops
###


# $Id: tp_dump_date.ksh,v 1.2 2008/06/17 13:13:49 daniels Exp $


# -----------------------------------------------------------------------------
# Mnemonic:	tp_dumpfeeds
# Abstract:	find all feed files for a range of dates and, for those
#		that aren't already on tape, hand them to ng_tp_dump
#		
# Date:		12-Sep-2005
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


export date1
export date2
export J=$(ng_tp_jobid)
export logdir=$TMP/log
export N=50
export POOLS="FEEDS_CVE FEEDS_OTHER FEEDS_PLATYPUS FEEDS_RICS"
export Pri=$PRI_INFO
export Prog=$(basename $0)
export SHOW=0
export ustring="[ -c cluster ] [ -n ] [ -p pool ] y | w | date1 [ date2 | 0 ]"
export Verbose=0
export srcNode
export srcCluster
export dstCluster=$NG_CLUSTER
export filelist=''


main () {
    ng_log $Pri $Prog "called with $@"

    # Parse the command line
    while [[ $1 == -* ]]
    do
	case $1 in
	--man)  show_man
		cleanup 0
		;;

	-c)	dstCluster=$2
		shift 2
		;;

	-f)	filelist=$2
		shift 2
		;;

	-n)	SHOW=1
		shift
		;;

	-p)	POOLS=$2
		shift 2
		;;

	-v)	Verbose=1
		shift
		;;

	-\?)    usage
		cleanup 0
		;;

	-*)	error_msg "unrecognized option: $1"
		usage
		cleanup 1
		;;
	esac
    done

    if [[ $Verbose -eq 1 ]] ; then
	echo "dstCluster = $dstCluster" 1>&2
	echo "SHOW = $SHOW" 1>&2
	echo "POOLS = \"$POOLS\"" 1>&2
    fi

    # Check how many arguments are left
    if [[ $# -gt 2 ]] ; then
	ng_log $PRI_ERROR $Prog "\$# = $#; aborting"
	usage
	cleanup 1
    fi

    POOLS=$(echo $POOLS | tr ',' ' ')
    nP=$(echo $POOLS | tr ' ' '\012' | gre -c .)
    ng_log $Pri $Prog "POOLS = $POOLS"
    ng_log $Pri $Prog "nP = $nP"
    if [[ $Verbose -eq 1 ]] ; then
	echo "nP = $nP, POOLS = \"$POOLS\"" 1>&2
    fi

    # Figure out where we are and where we're going
    srcNode=$(ng_sysname)
    srcCluster=$NG_CLUSTER
    if [[ $Verbose -eq 1 ]] ; then
	echo "srcNode = $srcNode, srcCluster = $srcCluster" 1>&2
    fi

    # Make sure we're on srv_FLOCK_WALDO
    if [[ $srv_FLOCK_WALDO != $srcNode ]] ; then
	ng_log $Pri $Prog "not on srv_FLOCK_WALDO ($srv_FLOCK_WALDO); aborting"
	echo "${Prog}: not running on srv_FLOCK_WALDO ($srv_FLOCK_WALDO)" 1>&2
	cleanup 1
    fi

    # Set the starting and ending dates
    setDates "$@"
    ng_log $Pri $Prog "date1 = $date1, date2 = $date2"
    if [[ $Verbose -eq 1 ]] ; then
	echo "date1 = $date1, date2 = $date2" 1>&2
    fi

    # Cons up a bunch of temp files
    ng_log $Pri $Prog "making temporary files"
    export tmpB=$(get_tmp_nm tpdf-${date1}_$date2-bad)
    export tmpF=$(get_tmp_nm tpdf-${date1}_$date2-files)
    export tmpS=$(get_tmp_nm tpdf-${date1}_$date2-strs)
    export tmpT=$(get_tmp_nm tpdf-${date1}_$date2-tmp)
    export tmpW=$(get_tmp_nm tpdf-${date1}_$date2-waldo)
    if [[ $Verbose -eq 1 ]] ; then
    	echo "tmpB = $tmpB" 1>&2
    	echo "tmpF = $tmpF" 1>&2
    	echo "tmpS = $tmpS" 1>&2
    	echo "tmpT = $tmpT" 1>&2
    	echo "tmpW = $tmpW" 1>&2
    fi

	    # An array of temp files, to make looping easier
	    set -A tmpP
	    for p in $POOLS ; do
		n=$(getPoolNum $p)
		tmpP[$n]=$(get_tmp_nm tpdf-${date1}_$date2-$p)
		if [[ $Verbose -eq 1 ]] ; then
		    echo "tmpP[$n] = ${tmpP[$n]}" 1>&2
		fi
	    done

if [[ -z "$filelist" ]]
then
	
	    # Build the patterns, one per day
	    ng_log $Pri $Prog "building date patterns"
	    mkDateStrs $date1 $date2 > $tmpS
	    cp $tmpS $logdir
	    if [[ $Verbose -eq 1 ]] ; then
		echo "${tmpS}:" 1>&2
		cat $tmpS 1>&2
	    fi
	
	    #
	    # Look in waldo.canon for all files that contain the desired dates
	    # (possibly followed by the time) and are stored on this cluster;
	    # then, strip out any files that are already on tape (specifically,
	    # on the StorageTek), save the the first three fields of the lines
	    # from waldo.canon, then save just the file names in a separate file
	    # (we'll need the waldo.canon lines to pass to tp_dump, and to get
	    # some statistics, so this subset file will save us some time later).
	    #
	    # Oh, and do all of this one day at a time so we don't run into the
	    # worst-case behavior of gre
	    #
	    ng_log $Pri $Prog "finding files"
	    cd $NG_ROOT/site/waldo
	    if [[ $Verbose -eq 1 ]] ; then
	    	pwd 1>&2
	    fi
	
	    #
	    # Stoopid shell quotes!
	    #
	    rm -f $tmpS.x[a-z][a-z][a-z][a-z][a-z]
	    split -a 5 -l 1 $tmpS $tmpS.x
	    if [[ $Verbose -eq 1 ]] ; then
	    	ls $tmpS.x[a-z][a-z][a-z][a-z][a-z] 1>&2
	    fi
	
	    for xf in $tmpS.x[a-z][a-z][a-z][a-z][a-z] ; do
		if [[ $Verbose -eq 1 ]] ; then
		    echo $xf 1>&2
		    cat $xf 1>&2
		fi
	
		gre -f $xf waldo.canon |
		gre -v ' tape\.STK' |
		awk '{ print $1, $2, $3 }' |
		tee -a $tmpW |
		awk '{ print $2 }' >> $tmpF
		rm -f $xf
	
		if [[ $Verbose -eq 1 ]] ; then
		    wc -l $tmpF 1>&2
		fi
	    done < $tmpS
	    cp $tmpW $logdir
	
	    # Sort our waldo lines
	    sort +1 -2 -o $tmpW $tmpW
	    if [[ $Verbose -eq 1 ]] ; then
	    	wc -l $tmpF 1>&2
	    fi
    else
	    rm -f $tmpS.x[a-z][a-z][a-z][a-z][a-z]
	    split -a 5 -l 20000 $filelist $tmpS.x
	    if [[ $Verbose -eq 1 ]] ; then
	    	ls $tmpS.x[a-z][a-z][a-z][a-z][a-z] 1>&2
	    fi
	    for xf in $tmpS.x[a-z][a-z][a-z][a-z][a-z] ; do
		gre -F -f $xf waldo.canon |
		gre -v ' tape\.STK' |
		awk '{ print $1, $2, $3 }' |
		tee -a $tmpW |
		awk '{ print $2 }' >> $tmpF
		rm -f $xf
	    done
    fi

    # Strip out the bad files
    ng_log $Pri $Prog "sorting then mapping out bad files"
    sort -u $tmpF > $tmpT
    ng_cuscus -b $tmpB -M $tmpT
    comm -13 $tmpB $tmpT > $tmpF
    cp $tmpF $logdir
    cp /dev/null $tmpT
    if [[ $Verbose -eq 1 ]] ; then
	wc -l $tmpB 1>&2
	wc -l $tmpF 1>&2
    fi

    # The first three pools are easy
    ng_log $Pri $Prog "using ng_cuscus to classify files"
    for p in $POOLS ; do
	if [[ $p != "FEEDS_OTHER" ]] ; then
	    if [[ $Verbose -eq 1 ]] ; then
	    	echo "p = $p" 1>&2
	    fi
	    n=$(getPoolNum $p)
	    ng_cuscus -S -M $tmpF -a $p |
	    sort > ${tmpP[$n]}
	    if [[ $Verbose -eq 1 ]] ; then
	    	wc -l ${tmpP[$n]} 1>&2
	    fi
	fi
    done

    #
    # FEEDS_OTHER takes a bit more work
    #
    # What we're doing is successively eliminating the files in each pool;
    # because cuscus returns only feed files, this also gets rid of random
    # files that just happen to contain the date in their name
    #
    n=$(getPoolNum "FEEDS_OTHER")
    if [[ $n ]] ; then
	ng_log $Pri $Prog "classifying files as FEEDS_OTHER"
	ng_cuscus -S -M $tmpF -a !FEEDS_CVE > $tmpT
	mv $tmpT $tmpF
	ng_cuscus -S -M $tmpF -a !FEEDS_PLATYPUS > $tmpT
	mv $tmpT $tmpF
	ng_cuscus -S -M $tmpF -a !FEEDS_RICS |
	sort > ${tmpP[$n]}
	rm $tmpF
	if [[ $Verbose -eq 1 ]] ; then
	    echo "p = FEEDS_OTHER" 1>&2
	    wc -l ${tmpP[$n]} 1>&2
	fi
    fi

    for p in $POOLS ; do
	n=$(getPoolNum $p)
	f=${tmpP[$n]}
	cp $f $logdir

	if [[ $Verbose -eq 1 ]] ; then
	    echo "p = $p, n = $n" 1>&2
	    echo "f = $f" 1>&2
	fi

	if [[ -s $f ]] ; then
	    # Get the waldo lines
	    join -1 1 -2 2 $f $tmpW |
	    sed 's/^\([^ ]*\) \([^ ]*\) /\2 \1 /' > $tmpT
	    if [[ $Verbose -eq 1 ]] ; then
		sed 5q $f; tail -5 $f
		echo ====
		sed 5q $tmpW; tail -5 $tmpW
		wc -l $tmpT 1>&2
	    fi
	    # Do a little bit of math
	    pL=$(gre -c . $tmpT)
	    pG=$(cat $tmpT | ng_sumcol -g 3)
	    if [[ $Verbose -eq 1 ]] ; then
		echo "pL = $pL" 1>&2
		echo "pG = $pG" 1>&2
	    fi

	    # Write the files or just log them and show the stats?
	    if [[ $SHOW -eq 1 ]] ; then
		if [[ $n -gt 1 ]] ; then
		    echo
		fi

		log=$TMP/$p.${date1}-$date2.$J
		echo "     pool: $p"
		echo "    files: $pL"
		echo "gigabytes: $pG"

		awk '{ print $2 }' $tmpT > $log
		echo "     list: $log"
	    else
		ng_log $Pri $Prog "sending files for $p to ng_tp_dump"
		echo "pool $p files $pL gigabytes $pG"
		echo

if [[ $srcCluster == $dstCluster ]] ; then
    awk '{ print $1, $2 }' $tmpT |
    ng_tp_dump -t $p -c $dstCluster
    echo
else
    rTmp=$(ng_rcmd $dstCluster echo '$TMP')
    if [[ $Verbose -eq 1 ]] ; then
	echo "rTmp = $rTmp" 1>&2
    fi

    if [[ ! $rTmp ]] ; then
	echo "${Prog}: unable to get \$TMP for dstCluster ($dstCluster)" 1>&2
	rm -f $tmpT
	continue
    fi

    ng_ccp -d $rTmp/ $dstCluster $tmpT
    ret=$?
    if [[ $ret -ne 0 ]] ; then
	echo "${Prog}:${ret}: unable to copy file list to $dstCluster" 1>&2
	rm -f $tmpT
	continue
    fi

    ng_rcmd $dstCluster ng_tp_dump -t $p -c $dstCluster -f $rTmp/$(basename $tmpT)
    ret=$?
    if [[ $ret -ne 0 ]] ; then
	echo "${Prog}:${ret}: ng_rcmd to $dstCluster failed" 1>&2
	rm -f $tmpT
	continue
    fi
    echo

    ng_rcmd $dstCluster rm -f $rTmp/$(basename $tmpT)
fi
	    fi

	    rm -f $tmpT
	else
	    if [[ $SHOW -eq 1 ]] ; then
		echo "     pool: $p"
		echo "    files: 0"
		echo "gigabytes: 0"
	    else
		ng_log $Pri $Prog "skipping $p; no files found"
		echo "pool $p files 0 gigabytes 0"
	    fi
	fi
    done

    # schedule a goad to make sure they run
	dfiler=$( ng_rcmd $dstCluster 'ng_ppget srv_Jobber' )		# goad best on jobber
	while [[ -z $dfiler ]]
	do
		verbose "unable to determine Filer on $dstCluster; trying again in 20s"
		sleep 20
		dfiler=$( ng_rcmd $dstCluster 'ng_ppget srv_Filer' )
	done

    #ng_wreq -s `ng_rcmd $dstCluster 'ng_ppget srv_Filer'` 'job: cmd sleep 900; ng_goad | ng_rmreq'
    ng_wreq -s ${dfiler:-no-host-name} 'job: cmd sleep 900; ng_goad | ng_rmreq'

    ng_log $Pri $Prog "finished; exiting with 0"
    cleanup 0
} # main


getPoolNum () {
    echo $POOLS | tr ' ' '\012' | gre -n $1 | sed 's/:.*//'
} # getPoolNum


mkDateStrs () {
    t="[0-9]\{6\}"
    md5="(\.[0-9a-f]\{32\})"
    c=" ${dstCluster}( |:s)"
    if [[ $Verbose -eq 1 ]] ; then
	echo "\$1 = $1, \$2 = $2, c = \"$c\"" 1>&2
    fi

    d=$2
    while [[ $d -ge $1 ]] ; do
	if [[ $Verbose -eq 1 ]] ; then
	    echo "d = $d" 1>&2
	fi
	echo " [^ ]*\.${d}${t}${md5}?\..z .*!.*${c}"
	d=$(ng_dateops yesterday $d)
    done
} # mkDateStrs


setDates () {
    if [[ $Verbose -eq 1 ]] ; then
	echo "setDates(): \$# = $#, \$1 = \"$1\", \$1 = \"$2\"" 1>&2
    fi

    if [[ $# -eq 1 ]] ; then
	case $1 in
	y)
	    date1=$(ng_dateops yesterday)
	    date2=$date1
	    ;;

	w)
	    date1=$(date +%Y%m%d)
	    for i in 1 2 3 4 5 6 7 8 ; do
		date1=$(ng_dateops yesterday $date1)
		if [[ $i -eq 2 ]] ; then
		    date2=$date1
		fi
	    done
	    ;;

	2[0-9][0-9][0-9][0-9][0-9][0-9][0-9])
	    date1=$1
	    date2=$date1
	    ;;
	esac

    else
	date1=$1
	if [[ $2 -eq 0 ]] ; then
	    date2=$(ng_dateops yesterday $(ng_dateops yesterday))
	    if [[ $date1 -ge $date2 ]] ; then
		ng_log $PRI_ERROR $Prog "$date1 >= $date2; aborting"
		echo "date1 >= date2"
		cleanup 1
	    fi
	else
	    date2=$2
	fi
    fi

    if [[ $Verbose -eq 1 ]] ; then
	echo "setDates(): date1 = $date1, date2 = $date2" 1>&2
    fi
} # setDates


main "$@"



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_dumpfeeds:send feed files to ng_tp_dump)

&space
&synop
	tp_dumpfeeds [-c cluster] [-n] [-p pool] [-f file] [y | w | date1 [ date2 | 0 ]]

&space
&desc
	&This takes a date or range of dates, finds all feed files for
	the time period given, and for any files not already stored on
	tape sends them to ng_tp_dump. &This will separate files into
	feed "pools" (currently FEEDS_CVE, FEEDS_PLATYPUS, FEEDS_RICS,
	and FEEDS_OTHER) and call ng_tp_dump once for each pool.

&space
&opts	The following options are recognized by &this when placed on the
command line:
&begterms
&term	-c cluster : select files that reside on the specified cluster
&space
&term	-n : gather files and statistics but don't write anything to
	tape
&space
&term	-p pool : process files for only the specified pool(s)
&endterms

&space
&parms	If a single date, in the form YYYYMMDD, is specified, the feed
	files for that date will be processed. If two dates are given
	(start, end), files for all dates in that range (inclusive) will
	be processed; if the second date is zero (0), the end date will
	automatically be set to the day before yesterday.
&space
	The single date &bold(y) means yesterday, which is teh 24 hour period ending
	just prior to the last midnight.
&space
	The single date &bold(w) means the last week, defined as the seven days
	preceding (and including) the day before yesterday.
&space
	The "-p" switch can take a single pool name or a comma-separated
	list of pool names.

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	13 Sep 2005 (asm) : First production version.
&term	XX Sep 2005 (asm) : Lots of changes.
&term	XX Oct 2005 (asm) : More changes.
&term	28 Oct 2005 (asm) : Added comma separation for pool names,
	updated man page.
&term	17 Jun 2008 (sd) : Chaned goad scheduling to run on jobber and to prevent wreq from 
		being passed an empty host name. 
&endterms

&scd_end
------------------------------------------------------------------ */
