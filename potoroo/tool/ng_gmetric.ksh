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
# advertise sensechal metrics to ganglia.  the command is buried in the
# regex below, sorry.
# each metric in ng_sreq dump 0 is advertised as
# ng_seneschal_$METRIC, a 32-bit int (overkill, i know)
# units are not currently advertised.
# Mods:	
#		20 Sep 2004 (sd) : added some standard things to allow this to 
#			be stopped at node stop time
# ---------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
GMETRIC=/usr/common/bin/gmetric
GMONDCONF=/usr/common/etc/gmond.conf
HEALTHDC=/usr/local/bin/healthdc

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

ng_goaway -r $argv0		# trash go away file if left over from long ago

while true; do
	. $CARTULARY			# periodic refresh

	if ng_goaway -c $argv0
	then
		log_msg "found goaway file; exiting"
		ng_goaway -r $argv0
		cleanup 0 
	fi

 	GMPORT=`grep -i mcast_port $GMONDCONF | awk '{print $NF}'`
 	GANGLIA_IF=`grep -i mcast_if $GMONDCONF | awk '{print $NF}'`

 	# for the non-node-specific stuff, only run on the netif
 	if [ x`hostname | sed 's/\..*//'` = x$srv_Netif ]; then
		for foo in `ng_sreq -s $srv_Jobber dump 1 | sed 's/.*://'`; do
			c=`echo $foo|sed  's/\([^(]\{1,\}\)(\([^)]\{1,\}\))/-nng_seneschal_\2 -v\1/;s/[<>]//g'`
			$GMETRIC -p$GMPORT -i$GANGLIA_IF -tint32 $c
  		done

  		$GMETRIC -p$GMPORT -i$GANGLIA_IF -tint32  -n'mlog files in repmgr' -v`ng_rm_where -n -p mlog | wc -l`
 	fi

 	if [ -x $HEALTHDC ]; then
		for c in `$HEALTHDC -h | grep '^<TR>' | sed 's/<TR><TD ALIGN=RIGHT>/-n/;s/<\/TD><TD>/\ -v/;s/&deg;C/-uCelsius/;s/<.*//;s/\#//g;s/\ /_/g'`; do
   			$GMETRIC -p$GMPORT -i$GANGLIA_IF -tfloat `echo $c | sed 's/_-/\ -/g'`
  		done
  		for foo in `/bin/df -k | grep '^/dev' |sed 's/\%//' | awk '{print $NF "|" $(NF-1)}'|sed "s/^\/|/root|/"`; do
   			$GMETRIC -p$GMPORT -i$GANGLIA_IF -u'%' -tuint8 -n"pct used "`echo $foo | sed 's/^\///;s/\//_/g;s/|/ -v/'`;
  		done
 	fi

 	$GMETRIC -p $GMPORT -i$GANGLIA_IF -n"seneschal attrs" -tstring -v"`ng_get_nattr`"

 	sleep 15

done

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_gmetric:Send Metric Info To Ganglia)

&space
&synop	ng_gmetric

&space
&desc	&This gathers information and passes it along to ganglia for its consumption.

&space
&exit	&This is not designed to exit. If it does so, it was because ng_goaway was 
	used to cause it to stop, or there was a hard error. 

&space
&mods
&owner(Dave Kormann)
&lgterms
&term	18 Dec 2006 (sd) : Added man page. 


&scd_end
------------------------------------------------------------------ */
