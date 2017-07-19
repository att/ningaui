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

# $Id: tp_poolinfo.ksh,v 1.4 2005/09/30 14:28:01 adamm Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_poolinfo
# Abstract:	extract tape pool info from TP_Pool_Where
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


export Prog=$(basename $0)
export ustring="pool"


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

	-\?)	usage
		exit 0
		;;

	-*)	error_msg "unrecognized option: $1"
		usage
		cleanup 1
		;;
	esac
done

#
# $1 (the pool) is required
#
if [[ $# -ne 1 ]] ; then
    echo "${Prog}: tape pool not specified"
    usage
    cleanup 1
fi

tapePool=$1

str=$(echo $TP_Pool_Where | tr ',' '\012' | grep ^${tapePool}:)

if [[ ! $str ]] ; then
    cleanup 2
else
    echo "$str" | sed -e "s/^${tapePool}://" -e "s/:/ /g"
    cleanup 0
fi


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_poolinfo:get tape pool info)

&space
&synop
	tp_poolinfo pool

&space
&desc
	&This parses the pinkpages variable &lit(TP_Pool_Where) and
	extracts information about the specified pool.

&space
	&lit(TP_Pool_Where) is composed of one or more comma-separated
	blocks of information, one block per pool. Each block is
	composed of colon-separated items; the order and meaning of the
	items within a block is fixed. The first item is always the pool
	name; the second and third items are the "destination" cluster
	(where the appropriate tape is, or will be) and hood; if the
	pool in known all three items must be present. A block can also
	contain one of more pairs of items, a drive name and a volume
	id; these items must appear in this order; multiple drives
	and tapes may be specified.

&space
	&This will print all items in the appropriate block, in order,
	separated by spaces.

&space
&parms
	&This requires that a tape pool be supplied as a positional
	parameter.

&space
&exit	Possible exit values:
&begterms
&term	0 : no errors
&space
&term	1 : missing pool name or bad arguments
&space
&term	2 : no info found for specified pool
&endterms

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	30 Sep 2005 (asm) : Removed call to ng_log.
&endterms

&scd_end
------------------------------------------------------------------ */
