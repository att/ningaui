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
                                   
function recoverdb
{
	cd $NG_ROOT/site/rm
	ng_rm_db -z
	cat `ls -tr db.h??` | ng_rm_db -a
}

#
# ------------------------------------------------------------------------
ustring=''
while [[ $1 = -* ]]
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
	esac				# end switch on next option character

	shift 
done					# end for each option character


tmp=$TMP/rm_db_vet

verbose "scanning rm_db database"
if ! ng_rm_db -p >$tmp 2>$tmp.1
then
	log_msg "rm_db database could not be parsed:"
	cat $tmp.1 >&2
	recoverdb			# go fix it up
	ng_rm_sync >$tmp.2		# just run it; nothing else to do
	log_msg "repaired broken db on $me; rm_sync in progress:"
	cat $tmp.2 >&2
	rm $tmp*
	cleanup 0
fi

log_msg "database seems okay, now just sync"
ng_rm_sync

rm $tmp*
cleanup 0
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_db_vet : Vet the local repmgr database)

&space
&synop  ng_rm_db_vet [-v]

&space
&desc   &This checks the local replication manager databae (rm_db) and if 
	problems are detected will restore the database from the last 
	checkpoint file and drive &lit(ng_rm_sync) to bring the database
	back into sync. 
&space

&space
&opts   The following options are recognised and change the behaviour of
	&this.

&exit
	Regardless of action taken, this script exits with a zero return code. 

&space
&see    ng_rm_db, ng_rm_dbd, ng_repmgr

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	18 Aug 2006 (sd) : Added man page, cleaned up for release. 
&term	15 Oct 2007 (sd) : Changed hardcoded path to $NG_ROOT.
&endterms
&scd_end
