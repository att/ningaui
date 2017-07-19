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
# Mnemonic:	sn_mon
# Abstract:	monitor seneschal from round the cluster and send send w1kw updates
#		via ng_parrot running somewhere. 
#		
# Date:		09 Jul 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

export PATH=$PATH:$HOME/bin

if [[ $* = *-r* && $* != *-f* ]]
then
	detach_log=/tmp/$LOGNAME.dlog
	$detach
	exec >$NG_ROOT/site/log/z_snmon 2>&1
fi

# -------------------------------------------------------------------
export NG_WREQ_QUIET=1		#prevent wreq from issuing log messages

ustring="[-f] [-h parrot-host] [-r repeat-seconds] [-s jobberhost]"
phost=$srv_Netif		# default to the network interface for the cluster
repeat=0
cluster=$NG_CLUSTER
jobber=
def_jobber=			# -s overrides us getting one each time.

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster=$2; shift;;
		-f)	;;
		-h)	phost=$2; shift;;
		-r)	repeat=$2; shift;;
		-s)	def_jobber="$2"; shift;;

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

if [[ $phost = *:* ]]
then
	dt_obj=${phost##*:}
	phost=${phost%%:*}
fi

if [[ -z $srv_Jobber ]]
then
	cleanup 1
fi

while true
do
	if [[ -z $def_jobber ]]
	then
		jobber=`ng_ppget -C ${cluster} srv_Jobber`			# follow the jobber if it moves
	else
		jobber=$def_jobber
	fi

	if [[ -z $jobber ]]
	then
		jobber=`ng_rcmd $cluster ng_ppget srv_Jobber`
	fi

	ng_sreq -s ${jobber:-foo} dump 30 | awk '
	/summary/ {next; }
	/node:/ {
		colour = 3;
		split( $2, a, "(" );
		if( index( $0, "SUSPENDED" ) )
		{
			colour = 1;
			state = "SUSP";
		}
		else
			state = sprintf( "%d/%d", $4, $5 );
		printf( "RECOLOUR %s_snjobs %d \"%s\"\n", a[1], colour, state );
	}
	' >/tmp/bcast.$$

	if [[ -s /tmp/bcast.$$ ]]
	then
		if [[ $phost = tty ]]
		then
			cat /tmp/bcast.$$
		else
			ng_sendudp $phost:$NG_PARROT_PORT </tmp/bcast.$$
		fi
	fi

	if [[ $repeat -gt 0 ]]
	then
		sleep $repeat
	else
		cleanup 0
	fi
	
done


cleanup 0



exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_z_snmon:Zookeeper Seneschal monitor)

&space
&synop	ng_z_snmon  [-c cluster] [-f] [-h parrot-host] [-r]  [-s jobber-host] [-v]

&space
&desc	&This makes inqueries to seneschal on the jobber node of a cluster and 
	reports the number of bids per node and the number of active seneschal jobs per node.  
	It will also report when a node is suspended.  The output of this script is routed to a
	W1KW process via the parrot on the node listed on the command line (-h). 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
	-c cluster : Cluster to monitor; sends requests to the jobber node on that 
	cluster.  If not defined, the current cluster is querried.  The -s option will 
	override this option.
&space
&term	-f : Keeps the script running in the 'foreground.'
&space
&term	-h host : The host on which &lit(ng_parrot) is expected to be running on. All 
	update messages are sent to the parrot on this host. 
&space
&term	-r seconds : Repeat.  The script should repeat its task with a delay of &ital(seconds) between. If not 
	supplied, the script will execute once and exit. 
&space
&term	-s host : Defines the name of the host that is expected to be running &lit(ng_seneschal).
	If missing, then the srv_Jobber for the indicated cluster is used. 
&endterms


&space
&exit	An exit code other than 0 is not expected, so any exit code that is not zero
	indicates a pretty severe problem.

&space
&see	ng_parrot, ng_z_dfmon, ng_z_rmmon, ng_z_psmon

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2002 (sd) : Thin end of the wedge.
&term	28 Aug 2006 (sd) : changed name for release, added scd.
&endterms

&scd_end
------------------------------------------------------------------ */
Filesystem                Type   Kbytes     Used    Avail   Cap  Mounted on
/dev/sda2                 ext2  8262068  7530672   311700   96%  /
/proc                     proc        -        -        -     -  /proc
/dev/sda1                 ext2    69995    34243    32138   52%  /boot
/dev/sda3                 ext2 16768408 11653944  5114464   69%  /ningaui/den
/dev/sda6                 ext2 54249340 25039368 29209972   46%  /ningaui/fs00
/dev/sdb4                 ext2 71028276 41466572 29561704   58%  /ningaui/fs01
/dev/sdc3                 ext3  8262068  6367308  1894760   77%  /home
/dev/sdc5                 ext3  2072200   263308  1808892   13%  /tmp
/dev/sdc6                 ext3 60393044 18761772 41631272   31%  /ningaui/fs02
/dev/sdd3                 ext2 70891744 45111516 25780228   64%  /ningaui/fs03
/dev/sde1                 ext2 80023264 30944748 49078516   39%  /ningaui/fs04
/dev/sdf1                 ext2 80023264 30047488 49975776   38%  /ningaui/fs05
/dev/sdg1                 ext2 33649508 29039136  4610372   86%  /ningaui/tmp
/dev/sdg2                 ext2 125873612 47275204 78598408   38%  /ningaui/fs06
/dev/sdd2                 ext2  8467564  7068388   969036   88%  /ningaui/site
/dev/pts                devpts        -        -        -     -  /dev/pts
/dev/shm                 tmpfs   451776        0   451776    0%  /dev/shm
automount(pid843)       autofs        -        -        -     -  /misc
