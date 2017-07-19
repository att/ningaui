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


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
                                   

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
ustring='filename cmd5sum'
while [[ $1 == -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 1;;
		-v)	verbose=1; verb=-v;;
		-\?)	usage; cleanup 1;;
		*)		# invalid option - echo usage message 
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac

	shift 
done       

tmp=/tmp/fd$$_$RANDOM
case $# in
0)	cat;;
2|3)	echo $2 $1;;
*)	usage;;
esac > $tmp

filer=${srv_Filer:-no-filer}

ng_rm_db -d $verb < $tmp && rm $tmp
if [[ `ng_ppget RM_FLUSH` -lt 1 ]]
then
	ng_rmreq -s $filer uninstance `ng_sysname -s` $2 $1
fi

ng_log $PRI_INFO $0 "del $1 $2 $3"

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_flist_del : Delete a file from the arena database)

&space
&synop  ng_flist_del filename checksum


&space
&desc   &ital(Ng_flist_del) takes a file and checksum and removes it from
	the local replication manager database.

&space
&opts   &ital(Ng_flist_del) script takes two required options:
&begterms
&term   fname  : The file's name (only the basename is used).
&term   chksum  : The MD5 checksum for &ital(fname).
&endterms
&space

&exit
	&ital(Ng_flist_del) script will set the system exit code to one
	of the following values in order to indicate to the caller the final
        status of processing when the script exits.
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&endterms

&space
&examp  ng_flist_del test d7894365ba19aa91fa72081f3cff66cd

&space
&see    &ital(ng_flist_add), &ital(ng_flist_del), &ital(ng_flist_send), &ital(ng_repmgr), &ital(ng_repmgrbt),
	&ital(ng_rmreq), &ital(ng_rmbtreq)

&space
&owner(Andrew Hume)
&lgterms
&term	19 Oct 2004 (sd) : changed ref to ng_leader to the right thing.
&term	24 May 2005 (sd) : Corrected usage message.
&term	01 Aug 2006 (sd) : Cleaned up options processing. 
&endterms
&scd_end

