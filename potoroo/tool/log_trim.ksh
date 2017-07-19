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
# Mnemonic:	trim_log
# Abstract:	pauses the log daemon and lops off a bit of the log, keeps the last 
#		two weeks or so.
#		
# Date:		Original date unknown (circa 2001)
# Author:	Andrew Hume
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# -------------------------------------------------------------------
sys=$(ng_sysname)
frag=mlog.$sys.$(ng_dt -d)
tmp=$TMP/frag$$
ustring=""

while [[ $1 = -* ]]
do
	case $1 in 

		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

cd $NG_ROOT/site/log

echo "%pause;" | ng_sendudp $(ng_sysname):$NG_LOGGER_PORT	# suspend logging 

awk -v now=$(ng_dt -i) '
BEGIN	{
		day = 36000*24
		week = 7*day
		# find last sunday (epoch was a saturday)
		sun = int((now-day)/week)*week + day
		# want previous sunday
		sun -= week
		printf("cutoff = %.0f\n", sun) >"/dev/fd/2"
	}
	{ if(sun <= 0+$1) print $0 }
' < master | time sort -u -T $TMP > master.new
rc=$?
if (( $rc == 0 ))		# move old to backup and insert new; DONT stop on error so we can resume logging
then
	if mv master master.bak 
	then
		mv master.new master
	else
		log_msg "failed to move old master to master.bak"
		rc=1
	fi
else
	log_msg "trim failed, masterlog not trimmed" 	
fi

echo "%resume;" | ng_sendudp $(ng_sysname):$NG_LOGGER_PORT	# restart logging

if (( $rc == 0 ))
then
	ng_log $PRI_INFO $0 "master log trimmed"
else
	ng_log $PRI_WARN $0 "unable to trim master log"
fi

exit $rc


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log_trim:Trim the ningaui master log)

&space
&synop	ng_log_trim

&space
&desc	&This pauses logging by &lit(ng_logd) and trims the current master log
	(NG_ROOT/site/log/master) to an epoch of the Sunday before last at 
	midnight (2 weeks if trimmed near midnight on Sunday morning).

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : verbose mode; &this might chat a bit to the standard error device when this 
	is set. 
&endterms


&space
&exit	An exit code that is not zero indicates an error. 

&space
&see	ng_log, ng_logd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	18 Dec 2006 (sd) : Added man page info.


&scd_end
------------------------------------------------------------------ */
