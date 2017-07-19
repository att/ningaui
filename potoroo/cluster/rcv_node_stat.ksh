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
# Mnemonic:	rcv_node_stat.ksh
# Abstract:	Responsible for unpacking the status stuff received from round the 
#		cluster.
# Date:		September 2001
# Author:	Andrew Hume
# Notes:	Original version assumed that if two parms were passed in the data 
#		was from another cluster leader.  The value of the second parm was 
#		insignificant.  We still support that, but have added a -p option 
#		which has the same effect.
# Mods:		14 Mar 2002 (sd) - to allow it to work with remote host data
# -------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN

ustring="[-r] tarfilename [x]"

remote=0; 
peer=0				# data from a peer cluster
dname="$NG_ROOT/site/stats/cnodes"

while [[ $1 = -* ]]
do
	case $1 in
		-p) 	peer=1;;
		-r)	remote=1; dname="$NG_ROOT/site/stats/remote";;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	usage; cleanup 1;;
	esac

	shift
done

exec >>$NG_ROOT/site/log/rcv_node_stat 2>&1
set -ex


tarf=$1
if [[ -n $2 ]]
then
	peer=1
fi

if [[ $peer -gt 0 || $remote -gt 0 ]]
then
	tar -C $dname -xf $tarf
	
	for i in $dname/*		
	do
		if [[ -d $i ]]
		then
			cd $i
			if [[ $remote -lt 1 ]]
			then
				ng_node_summary
			else
				if [[ ! ts || devdetails -nt ts ]]
				then
					ng_dt -d > ts
				fi
			fi
		fi
			
	done
else
	host=`basename $tarf .stats`
	tar -tf $tarf
	tar -C $NG_ROOT/site/stats/nodes -xf $tarf
	
	cd $NG_ROOT/site/stats/nodes/$host
	pwd
	ng_dt -d > ts
	ls -l $NG_ROOT/site/stats/nodes/$host/ts
	ng_node_summary
fi

cleanup 0
exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rcv_node_stat:Receive Node Statistics)

&space
&synop	ng_rcv_node_stat [-r] tarfile [x]

&space
&desc	&This expands &ital(tarfile) into the proper site statistics directory.
	If &ital(x) is supplied as the second command line parameter, the 
	file is assumed to have originated from a different cluster,  and thus 
	the status information is placed into a "collection" directory rather 
	than the directory where the other nodes in the current cluster are 
	kept. This allows a single node to monitor nodes from more than its 
	own cluster. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-r : Inidicates that the data is from a remote (isolated) host which is not 
	a member of any cluster. Data from remote hosts is placed into a remote directory
	so that they may be monitored as a logical group if necessary.
&endterms


&space
&parms	&This expects that the first commandline parmater will be the name of the &lit(tar)
	file which contains all of the stats information.  Optionally, a second parameter
	(generally the character &lit(x)) can be supplied to indicate that the information 
	is not from the local cluster. 
&space
&exit	Currently this script always exits with a good return code.

&space
&see	&ital(ng_node_stat) &ital(ng_host_stat)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term 	00 Sep 2002 (ah) : Original.
&endterms

&scd_end
------------------------------------------------------------------ */
