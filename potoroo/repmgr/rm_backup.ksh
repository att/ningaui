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
# Mnemonic: ng_rm_backup
# Author:   Andrew Hume
# Date:     Apr 2002
#


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
                                   

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
ustring='[-d]'
force_daily=0

while [[ $1 == -* ]]    
do
	case $1 in 
	-d)	force_daily=1;;
	-v)	verbose=TRUE; verb=-v;;
	--man)	show_man; cleanup 1;;
	-\?)	usage; cleanup 1;;
	*)		# invalid option - echo usage message 
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac				# end switch on next option character

	shift

done                            # end for each option character

case $# in
0)	true;;
*)	usage;;
esac

x=$(date +%H)
hourly=$NG_RM_DB.h$x
touch $hourly.new			# file must exist in case nothing in the database
ng_rm_db -p | awk -v f=$hourly.new ' 
	BEGIN { c = 0; }
	{ print >f; c++}
	END { print c; }
' |read wcount				# number we think we wrote out
wc -l <$hourly.new | read vcount	# number that made it (we have had issues with not getting errors when disk fill

verbose "${argv0##*/} backup wrote $wcount records; verification pass found $vcount records"

if (( ${vcount:-99} != ${wcount:--1} ))
then
	ng_log $PRI_CRIT $argv0 "unable to create backup: bad record count (vcount=${vcount:-missing} wcount=${wcount:-missing})"
	log_msg "unable to creae backup: bad record count (vcount=$vcount wcount=$wcount)"
	cleanup 1
fi


#	 backup looks good, overlay the old one
mv $hourly $hourly.old
if mv $hourly.new $hourly
then
	rm $hourly.old
	chmod 664 $hourly
fi

if (( $x == 23  ||  $force_daily > 0  ))
then
	ng_sysname -s | read mynode
	bname="rmdb.$mynode.d$(date  +%d).cpt"
	ng_spaceman_nm $bname | read daily
	cp $hourly $daily
	log_msg "registering daily backup: $daily"
	ng_rm_register $verb $daily
fi

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_rm_backup - Backup the rm_db
&name &utitle

&space
&synop  ng_rm_backup [-d]


&space
&desc   &This creates a backup of the local &ital(replication manager)
	database.  
	Backup files are placed in the same directory 
	as the database file, and are named db.hxx where &lit(xx) is the 
	current hour. 

&space
	If the -d option is given, a daily backup is created regardless of 
	the time. A daily backup is automatically taken if this script 
	is invoked during the final hour of the day.  Daily backups
	are named &lit(rmdb.<node>.d<dayofmonth>.cpt) and replicated within 
	the replication manager space. 
&space
&exit
	An exit code of zero indiates that all processing was successful.
	Any other exit code indicates a failure. 

&space
&see    ng_repmgr
&mods
&owner(Andrew Hume)
&lgterms
&term	09 May 2003 (sd) : Changed to prevent zero length backups and to issue 
	a critical error if they would have occurred.
&term	18 May 2003 (sd) : Changed name of daily backup so as not to collide with other nodes.
&term	04 Jun 2003 (sd) : removed reference to propylaeum.
&term	10 Jun 2003 (sd) : Corrected force-daily check bug.
&term	07 May 2004 (sd) : Added check to ensure that num recs in backup is approximately the 
	same as indicated by ng_rm_db (prevents a partial file if out of space on fs).
&term	01 Jun 2005 (sd) : Fixed date command to not be AST specific. 
&term	12 Dec 2005 (sd) : Fixed date command in daily backup not to be AST specific. 
&term	18 Aug 2006 (sd) : Updated options loop.
&term	28 May 2007 (sd) : Two lines of code replaced that seemed to have been deleted in the 
		daily backup processing. 
&term	27 Jul 2009 (sd) : Corrected problem with error checking; if verification file was not created, 
		the count variable was ending up blank rather than 0 causing an error in $(()) processing.
&endterms
&scd_end

