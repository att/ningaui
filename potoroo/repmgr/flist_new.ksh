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
#
# Mnemonic: ng_flist_new
# Author:   Andrew Hume
# Date:     May 2001
#


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
                                   

# ------------------------------------------------------------------------

ustring='[-v]'
while [[ $1 == -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 1;;
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

tmp=/tmp/fnew$$

# first clear existing database
> $NG_RM_DB

# now, search arena
cd $NG_FS_ARENA
for i in *
do
	find `cat $i`/[0-9]* -type f -print | md5sum -l
done > $tmp
ng_rm_db -s -a $verb < $tmp && rm -f $tmp

b=`ng_rm_db -p | wc -l`

ng_log $PRI_INFO $0 "check [$b]"

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_flist_new : Initialise a new arena database)

&space
&synop  ng_flist_new [-v]


&space
&desc   The &ital(ng_flist_new) script will create and initialise the arena
	database (in $NG_RM_DB) to all the files in the arena.
&space

&exit

&space
&examp  ng_flist_new

&space
&see    ng_flist_add, ng_flist_del, ng_flist_send, ng_repmgr
&owner(Andrew Hume)
&lgterms
&term	01 Aug 2006 (sd) : Cleaned up options processing; added --man.
&endterms
&scd_end

