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

# $Id: tp_jobid.ksh,v 1.6 2005/09/30 14:28:01 adamm Exp $

# -----------------------------------------------------------------------------
# Mnemonic:	tp_jobid
# Abstract:	emits an encoded, unique job ID with optional sequence
#		number (at least, we hope its unique)
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

ustring=""


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
		cleanup 0
		;;

	-*)	error_msg "unrecognized option: $1"
		usage
		cleanup 2
		;;
	esac
done

(
    ng_dt -u | ng_tp_n32a
    ng_ps | cksum | awk '{ print $1 }' | ng_tp_n32a

    if [ $1 ] ; then
	echo $1 | ng_tp_n32a
    fi
) |
awk '
{
    if (NR > 1) {
	printf("_")
    }

    printf("%s", $0)
}

END {
    printf("\n")
}'

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(tp_jobid:create a unique tape job id)

&space
&synop	tp_jobid [seq#]

&space
&desc	&This will create an encoded tape job id; because &this uses the
time of day and output from ps(1) to create the id, we can be reasonably
sure that the id will be unique.

&space
	Each part of the id is a base-32 number; the first part is
	the time (in seconds since the epoch), the second part is
	&lit(ng_ps | cksum).

&space
&parms
	&This will accept a sequence number as a positional parameter.
	When supplied, it will be appended to the job id (along with a
	separating underscore).

&space
&mods
&owner(Adam Moskowitz)
&lgterms
&term	18 Aug 2005 (asm) : First production version.
&term	30 Sep 2005 (asm) : Removed call to ng_log.
&endterms

&scd_end
------------------------------------------------------------------ */
