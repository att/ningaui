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

# ------------------------------------------------------------------------
#
# Mnemonic: ng_rm_syncverify
# Author:   Andrew Hume
# Date:     May 2001
#

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

#
# ------------------------------------------------------------------------

ustring='ng_rm_syncverify file checksum'
while [[ $1* = -* ]]                    # while "first" parm has an option flag (-)
do
        case $1 in
        -v)     verbose=TRUE; verb=-v;;
        -\?)    usage; cleanup 1;;
        --man)  set -x
                show_man
                exit 1
                ;;
        *)              # invalid option - echo usage message
                error_msg "Unknown option: $1"
                usage
                cleanup 1
                ;;
        esac                            # end switch on next option character

        shift
done

file=$1
if [[ -z $file ]]
then
        ng_log $PRI_ERROR $argv0 "input filename not specified"
        cleanup 1
fi

checksum=$2
if [[ -z $checksum ]]
then
        ng_log $PRI_ERROR $argv0 "input checksum not specified"
        cleanup 1
fi

if [[ ! -e $file ]]
then
        ng_log $PRI_WARN $argv0 "input file not found $file"
        cleanup 1
fi

md5sum $file | 2> /dev/null read csum junk

case "$csum" in
$checksum)	# checksums match; nothing to do

	# write to log or you won't know the job succeeded 
        ng_log $PRI_INFO $argv0 "verified $csum $file"
	;;

?*)	# they don't match!
	echo $checksum $file | ng_rm_db -d	# delete database entry
	echo $csum $file | ng_flist_add

	now="$(ng_dt -d)"
	node=`ng_sysname`
	nfile="$TMP/${argv0##*/}.note.$now"
        echo "rm_syncverify found discrepancy $file disk=$csum rmdb=$checksum" >> $nfile
	ng_log $PRI_ALERT $argv0 "found checksum discrepancy.  See $node:$nfile"  
	;;

*)      # couldn't verify
        ng_log $PRI_WARN $argv0 "couldn't verify $file disk=$csum rmdb=$checksum"
	cleanup 1
	;;
esac

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_syncverify:Synchronize disk with instance database by checking file contents)

&space
&synop  ng_rm_syncverify file checksum

&space
&desc   &ital(Ng_rm_syncverify) is called by &ital(ng_rm_sync) to synchronize 
	the checksum of a file on disk with that reported in the node's instance 
	database maintained by &ital(ng_rm_db).  If there is a discrepancy, the
	rm_db database will be updated and an alert will be issued into the log.

&exit
	&ital(Ng_rm_syncverify) returns the following exit codes:
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the caller's request.
&space
&term   1 : The parameter/options supplied on the command line were invalid or missing. 
&endterms

&space
&see    ng_rm_sync, ng_rm_db

&mods
&owner(Andrew Hume)
&lgterms
&term	09 Apr 2007 (bsr) : Added man page and input parameter processing.
&term	01 Oct 2007 (sd) : Enabled man page.
&endterms
&scd_end
