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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
ustring=''
while [[ $1 == -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 99;;
		-v)	verbose=1; verb=-v;;
		-\?)	usage; cleanup 1;;
		*)		
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac	

	shift 
done	

filer=${srv_Filer:-no-filer}

ng_log $PRI_INFO $0 "$1,$2"
set -x
echo $2 $1 | ng_rm_db -f | read md5 path sz junk
case "x$path" in
x)	ng_rmreq -s $filer uninstance `ng_sysname -s` $2 $1	# repmgr thinks we have one; correct
	exit 0
	;;
esac
if [[ ! -f $path ]]							# we don't have one; correct
then
	echo $md5 $path | ng_rm_db -d
	ng_rmreq -s $filer uninstance `ng_sysname -s` $2 $1
	exit 0
fi

ng_log $PRI_INFO $0 "$1,$2 -> $path sent to $4 on $3 $sz bytes"
logf=$TMP/repmgr/ccp_`basename $1`
set -e
ng_ccp -s -d $4 $3 $path 1> $logf 2>&1
ng_log $PRI_INFO $0 "$1 sent"

rm -f $logf

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_send_file:Send a file to another node)

&space
&synop  ng_rm_send_file file md5 host dest-path

&space
&desc   &This is the replication manager's wrapper to ng_ccp.  This script is 
	intended to be invoked only from within replication manager.  

&space
	The script will cause the named file to be coppied from the current node
	to the destination node and to be placed into the given path. Ng_ccp 
	is used to do the actual copy.
&space
	This script is designed to be invoked only by replication manager. 
	Odd things might result if invoked manually.


&space
&see    ng_ccp, ng_rm_req, ng_repmgr, ng_rm_db, ng_flist_send
&owner(Andrew Hume)
&lgterms
&term	19 Oct 2004 (sd) : remvoved ng_leader references
&term	15 Nov 2004 (sd) : writes log files to TMP/repmgr.
&term	01 Aug 2006 (sd) : Fixed options loop; includes --man. Corrected man page.
&endterms
&scd_end

