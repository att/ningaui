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
#!/ningaui/bin/ksh

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
#
ustring='oldname md5 newname'
while [[ $1 = -* ]]
do
	case $1 in 
		-v)	verbose=1; verb=-v;;
		--man)	show_man; cleanup 99;;
		-\?)	usage; cleanup 1;;
		*)
			log_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac

	shift 
done	

echo $2 $1 | ng_rm_db -f | read junk path junk
new=${path%/*}/$3
ln $path $new				# create new
ng_flist_add $new $2			# add new
ng_flist_del $1 $2			# remove old
rm -f $path				# delete old file

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_mv_file:Rename a file and perform bookkeeping)

&space
&synop  ng_rm_mv_file oldname md5 newname


&space
&desc   &This renames the specified file and performs local node database 
	(ng_rm_db) bookkeeping functions for the replication manager. This script
	does &stress(NOT) attempt to make any changes to the registration 
	information within replication manager; this script assumes that 
	it has been invoked &stress(BY) replication manager. 

&exit
	The return code is always 0.
&space
&examp  ng_rm_mv_file pf-abs-12-xx+0 07632ad09efg78271 pf-abs-12-xx

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Aug 2006 (sd) : Cleaned up options parse and added --man.  Fleshed out manual page. 
&endterms
&scd_end

