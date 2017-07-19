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
# Mnemonic:	z_dfmon
# Abstract:	monitor df from round the cluster and send update to parrot
#		
# Date:		08 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

export PATH=$PATH:$HOME/bin

undercover=0
if [[ $* = *-r* && $* != *-f* ]]
then
	detach_log=/tmp/$LOGNAME.dfmon
	$detach
	undercover=1
fi

function purge_log
{
	if [[ $undercover -gt 0 ]]
	then
		tail -1000 $detach_log >$detach_log.x
		mv $detach_log.x $detach_log
		exec >>$detach_log 2>&1
	fi
}

# -------------------------------------------------------------------
export NG_WREQ_QUIET=1		#prevent wreq from issuing log messages

ustring="-h parrot-host] [-i viewvariable] [-l resource-name-list] [-n node-numbers] [-r repeat-seconds] "
phost=
list="RM_FS RM_RENAME RM_SEND"
id="info"				# the drawing thing name with each graph
repeat=0
stats=""				# if set to path, stats collected in that file
clust=${NG_CLUSTER:-no-ng_cluster-defined}
name_fmt="%02d"
dt_obj="alert_msg"			# drawing thing object to update with total used
nodes=""
vlevel=0
refresh_nodes=0

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	clust=$2; shift;;
		-f)	;;
		-F)	name_fmt=$2; shift;;
		-h)	phost=$2; shift;;
		-i)	id=$2; shift;;
		-n)	nodes="$2"; shift;;
		-r)	repeat=$2; shift;;
		-s)	stats="/tmp/df_mon_stats.$LOGNAME";;

		-v)	verbose=1; vlevel=$(($vlevel + 1 ));;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -z $nodes ]]
then
	refresh_nodes=1			# nodes will be picked up in loop to get changes without restarting 
fi

if [[ $phost = *":"* ]]
then
	dt_obj="${phost##*:}"
	phost="${phost%%:*}"
fi

ng_goaway -r ${argv0##*/}
totalsf=/tmp/totals.$$
while true
do
	purge_log

	if ng_goaway -c ${argv0##*/}
	then
		cleanup 0
	fi

	ng_dt -d|read now
	>/tmp/bcast.$$

	if [[ refresh_nodes -gt 0 ]]
	then
		nodes=`ng_rcmd ${clust:-$NG_CLUSTER} ng_species !Satellite`	# list all nodes except satellite nodes
	fi

	# we dont use ng_rm_df as we want a specific maximum fs on each node rather than a aggrigated percentage.
	verbose  "nodes=$nodes "
	for x in $nodes
	do
		ng_rcmd -t 40 -c 2 -f $x "ng_df " 2>/dev/null |gre "/fs|/rmfs" |awk  \
			-v totalsf=$totalsf \
			-v cluster="$clust" \
			-v dt_obj="$dt_obj" \
			-v stats="$stats" \
			-v bcast=/tmp/bcast.$$ \
			-v id="$id" \
			-v node=$x \
			-v now=$now \
			-v name_fmt="$name_fmt" \
			-v verbose=$vlevel \
			'
			BEGIN { 
				maxn="XXX"; 
				max=-1; 

				total = 3;				# ast df differs from /bin/df
				used = 4;
				avail = 5;
				type = 2;		# only in ast


				red = 1;		# clut values that w1kw uses as default
				orange = 2;
				peach = 3;
				cyan = 4;
				blue = 5
				green = 6;	
				yellow = 8;
				purple = 9;
			}

			/[0-9]%/ {
				p = $(NF-1) + 0;
				if( $NF == "/" )
				{
					x = 1;
					a[x] = "root";
				}
				else
					x = split( $NF, a, "/" );

				if( stats )
					printf( "%.0f %s %s(%s) %.0f(t) %.0f(u) %.0f(a) %s\n", now, node, a[x], $NF, $(used), $(used), $5, $(NF-1) ) >>stats;
				if( substr( a[x], 1, 2 ) == "fs" )		# repmgr filesystem we assume xx/xx/xx/fs00
				{
					$(total) = $(used) + $(avail);		# to account for bsd fast filesystems which reserve some

					if( verbose > 1 )	
					{
						if( last_node && last_node != node )
						{
							printf( "\t%.2f(total) %.2f(used) %.2f%%\n", ntot, nuse, ntot ? (nuse*100)/ntot: 0 )>"/dev/fd/2";
							ntot = 0;
						 	nuse = 0;
						}
						printf( "considered: %s %s (computed: %.2f%%)\n", node, $0, ($(used)*100)/$(total) )>"/dev/fd/2";
						ntot += $(total);
						nuse += $(used);
					}
					totalk += $(total);
					usedk += $(used);	
				}
				last_node = node;

				#if( p > max && $NF != "/proc" && $(NF) != "/dev" )
				if( p > 90 )
					over++;
				if( p > max  )
				{
					max = p;
					maxn = a[x];
				}
			}
	
			END { 
				if( verbose > 1 )
					printf( "\t%.2f(total) %.2f(used) %.2f%%\n", ntot, nuse, ntot ? (nuse*100)/ntot: 0 )>"/dev/fd/2";

				if( max > 98 )
					colour = yellow;
				else
					colour = (max > 94) ? (over > 1 ? red : cyan) : (max < 0) ? cyan : green;	
				
				fmt = sprintf( "RECOLOUR %%s_%%s %%d \"%%d %%s\"\n", name_fmt );
				printf( fmt, node, id, colour, max,  maxn  ); 
				printf( "%.2f %.2f %s %6.2f%%\n", usedk, totalk, node, totalk ? usedk*100/totalk:0 ) >>totalsf;

			}
		' >>/tmp/bcast.$$
	done


	if [[ -s /tmp/bcast.$$ ]]
	then
set -x
		if [[ $phost = tty ]]
		then
			cat /tmp/bcast.$$
		else
			if [[ -n $phost ]]
			then
				ng_sendudp $phost:$NG_PARROT_PORT </tmp/bcast.$$
			fi
		fi
set +x
	fi

	date "+%H %M" |read hr min
	if [[ -s $totalsf ]]			# get totals and present a cluster wide disk usage summary
	then
		if [[ $verbose -gt 0 ]]
		then
			cat $totalsf >&2
		fi

		awk \
			-v dt_obj="$dt_obj" \
			-v verbose=$verbose \
			-v phost=$phost \
			-v clust=$clust \
			-v min=$min \
			-v hr=$hr \
			'
			{ 
				totalk += $2;
				usedk += $1;
			}

			END {
				if( totalk <= 0 )
					totalk=1;

				red = 1;		# clut values that w1kw uses as default
				orange = 2;
				peach = 3;
				cyan = 4;
				blue = 5
				green = 6;	
				yellow = 8;
				purple = 9;

				p = (usedk/totalk)*100;
				if( verbose )
					printf( "%.0f %.0f %.3f\n", usedk, totalk, p );

				if( p > 90 )
					colour = p > 98 ? yellow : red;
				else
					if( p > 85 )
						colour = orange;
					else
						colour = green;
				if( phost )
				{
					cmd = sprintf( "ng_zoo_alert  -o %s -c %s -s %s %.5s@%02d:%02d %5.1f%%",  dt_obj, colour,  phost, clust, hr, min, p );
					system( cmd );
				}
			}
		'<$totalsf 
		>$totalsf

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
&doc_title(ng_z_dfmon:Disk usage query monitor)

&space
&synop	ng_z_dfmon [-h parrot-host] [-i viewvariable] [-n nodes] [-r repeat-seconds] 

&space
&desc	&This executes a &lit(ng_df) command on each node in the named cluster (-c) and generates a summary
	of replication manager filesystem usage.  By default, output is written in tabular format to the 
	standard output device, and &ital(zookeeper) view update messages are sent to the &lit(ng_parrot)
	process running on the &ital(parrot-host) (-h). If the repeat command line option is specified,
	&this drops itself into the background and executes in a loop every &ital(repeat-seconds) seconds.
&space
	One bug, satellite nodes are not querried.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : The cluster to query.  If not supplied, the value assigned to NG_CLUSTER is used.
&space
	-F format : Name format string.  Once upon a time not all nodes had the form %s%02d as a node naming
	scheme.  This allowes odd names to be accounted for.
&space
&term	-h host : The name of the host running the &lit(ng_parrot) process that zookeeper view update 
	messages should be sent to.
&space
&term	-i view-var : Supplies the name of the view variable that &this is to use as the target for a general 
	disk percentatge used message. 
&space
&term	-n node(s) : A list of nodes on which to execute the command.  If this option is not supplied, then all nodes
	on the named cluster are queried. 
&space
&term	-r sec : Defines the number of seconds to delay before repeating the command.  In repeat mode, 
	&this drops into the background and runs as a continuous monitor process. 
&endterms


&space
&see	ng_zoo 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 May 2003 (sd) : Thin end of the wedge.
&term	02 Aug 2006 (sd) : Default to no parrot host.
&term	28 Aug 2006 (sd) : changed name for release, changed the alarm bug names.
&term	14 Aug 2007 (sd) : Mod to check rmfs filesystems too.
&endterms

&scd_end
