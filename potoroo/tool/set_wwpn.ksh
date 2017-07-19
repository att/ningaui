#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
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

dosu=`whence ng_dosu`

case `uname -s` in
Linux)
	(
		cat /sys/class/fc_remote_ports/*/port_name | sed 's/^/use /'
		for i in /dev/sd?
		do
			echo disk $i `$dosu sg_inq $i | grep 'Unit serial' | sed 's/.*: //'`
		done
		ng_df | sed 's/^/mount /'
	) | awk '
	$1=="disk" { wwpn[$2] = $3 }
	$1=="use" { valid[toupper(substr($2, length($2)-5))] = 1 }
	$1=="mount" { dev[$8] = $2; fs[$8] = $3 }
	END {
		for(i in dev){
			x = dev[i]; sub("[0-9]*$", "", x)
			if(wwpn[x] && (valid[substr(wwpn[x], 1, 6)] == 1))
				print "wwpn", i, fs[i], wwpn[x]
		}
	}'
	;;
FreeBSD)
	(
		list=`$dosu cc_devlist | sed 's/.*[(,]da/da/;s/[,)].*//' | gre '^da'`
		for i in $list
		do
			echo disk $i `$dosu cc_inq $i | grep 'Serial Number' | sed 's/.* //'`
		done
		geom LABEL status | sed 's:^ *:map /dev/:'
		ng_df | sed 's/^/mount /'
	) | tee /tmp/agh1 | awk '
#	$1=="disk" { wwpn[$2] = $3 }
#	$1=="map" { sub("[a-z]$", "", $4); fsmap[$4] = $2 }
#	$1=="mount" { mt[$2] = $8; fs[$2] = $3 }
	$1=="disk" { wwpn[$2] = $3 }
	$1=="map" { sub("[a-z]$", "", $4); fsmap[$2] = $4 }
	$1=="mount" { mt[$2] = $8; fs[$2] = $3 }
	END {
#		for(i in wwpn){
#			f = fsmap[i]
#			if((fs[f] == "Unavail") && index(f, "/dev/ufs/"))
#				fs[f] = "ufs"
#			if(f){
#				if(mt[f] == ""){
#					mt[f] = "not_mounted"
#					fs[f] = "unknown"
#				}
#				print "wwpn", mt[f], fs[f], wwpn[i]
#			}
#		}
		for(i in mt){
			if((fs[i] == "Unavail") && index(i, "/dev/ufs/"))
				fs[i] = "ufs"
			if(fs[i] && fsmap[i] && wwpn[fsmap[i]])
				print "wwpn", mt[i], fs[i], wwpn[fsmap[i]]
		}
	}'
	;;
*)
	echo "unknown system `uname -s`; exiting" 1>&2
	exit 1
	;;
esac

cleanup 0

exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_set_wwpn:Set WWPNs for disks)

&space
&synop	ng_set_wwpn

&space
&desc	&This will generate &lit(wwpn) entries for teh &lit(site/config) file.
	The format of each line is
&space
	&lit(wwpn) &ital(mountpoint) &ital(filesystem type) &ital(world-wide port number)
&space
	for example
&space
	&lit(wwpn /ningaui/pk14j ext3 0C4EDA413CD14102)
&space
	This is a nettlesome script which necessarily peers at the operating system's naughty bits.
	(Accordingly, it must be run as user &lit(ningaui).)
	It will likely need updating any time the OS version changes.
	Currently, it only supports FreeBSD and Linux.

&space
&see	limn_checker(TBD)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	15 Feb 2009 (agh) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
