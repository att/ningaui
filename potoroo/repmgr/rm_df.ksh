#!/usr/common/ast/bin/ksh
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


#!/usr/common/ast/bin/ksh
# Mnemonic:	ng_rm_df
# Abstract:	generate a df like listing for replication manager filesystems on 
#		one or more clusters
#		
# Date:		12 May 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# given a set of output from ng_sumfs, figure used space
# output is total space in gig and percentage used
# sumfs amounts assumed to be in mbytes
function rm_used_space
{
	awk  -v set_alarm=$alarm -v cluster_alarm=$calarm -v node_alarm=$nalarm -v graph=$graph '
		BEGIN {
			oidx = 0;
		}

		function gen_bar( n, 		i, dots, vref, gwidth )
		{
			gwidth = 45;			# width in characters
			vref = int( gwidth/4 );
	
			dots = gwidth * (n/100);
			#printf( "|" );
			for( i = 0; i < dots; i++ )
				printf( "%s", i % vref ? "*" : "|" );
				#printf( "*" );
			for( ; i < gwidth; i++ )
				printf( "%s", i % vref ? " " : "|" );

			printf( "\n" );
		}
	
		function scale( )
		{
			printf(  "0                     50%%                 100%%\n" );
		}

		/node /	{ 
			node = $2; 
			if( !seen[node]++ )
				order[oidx++] = node;	
			next;
		 }

		/totspace=/ {
			split( $1, a, "=" );
			split( $2, b, "=" );

			tot += a[2];
			free += b[2];

			node_tot[node] = a[2];
			node_free[node] = b[2];
			node_pused[node] = a[2] == 0 ? 0 : (((a[2]-b[2])/a[2])) * 100;
			
			flag_node[node] = 0;
			if( node_pused[node] > node_alarm )
			{
				if( ! graph  && set_alarm )
					flag_node[node] = 1;	
				alarm = 1;
			}

			count++;
			next;
		}
	
		function print_val( fmt, amt,		v, suffix )
		{
			if( (v = amt/(1024*1024)) > 1 )
				suffix = "T";
			else
			{
				v = amt/1024;
				suffix = "G";
			}
			
			printf( fmt, v, suffix );
		}

		END {
			printf( "%10s %8s %8s %8s %8s\t", "Node", "Total", "Used", "Free", "%Cap" );
			if( graph )
				scale( );
			else
				printf( "\n" );

			if( count )
			{
				for( i = 0; i < oidx; i ++ )		# same order as seen on input 
				{
					n = order[i]; 

					printf( "%10s ", n );
					print_val( "%7.2f%s ", node_tot[n] );
					print_val( "%7.2f%s ", node_tot[n] - node_free[n] );
					print_val( "%7.2f%s ", node_free[n] );
					printf( "%7.2f%%%s%s", node_pused[n], flag_node[n] ? " <<<" : "", graph ? "\t" : "\n" );
					if( graph )	
						gen_bar( node_pused[n] );
						
				}
	
				cluster_pused = tot == 0 ? 0 : (((tot-free)/tot) * 100);

				printf( "%10s ", "Total:" );
				print_val( "%7.2f%s ", tot );
				print_val( "%7.2f%s ", tot-free );
				print_val( "%7.2f%s ", free );
				printf( "%7.2f%%%s%s", cluster_pused, cluster_pused > cluster_alarm ? " <<<" : "", graph ? "\t" : "\n" );
				if( graph )
					gen_bar( cluster_pused );
			}
			else
				printf( "no input\n" );

			if( alarm || cluster_pused > cluster_alarm )
				exit( 1 );
		}
	' 
	return $?
}

# -------------------------------------------------------------------
ustring="[-A [-C threshold] [-N threshold]] [-a] [-c cluster] [-g] "
alarm=0
graph=0
nalarm=${NG_RMDF_NALARM:-95.0}		# default alarm thresholds -- individual node
calarm=${NG_RMDF_CALARM:-90.0}		# whole cluster 
def_cluster=$NG_CLUSTER

while [[ $1 = -* ]]
do
	case $1 in 
		-A) 	alarm=1;;
		-a)	cluster=$( ng_nar_map -c );;
		-c)	cluster="$cluster$2 "; shift;;
		-C)	calarm=$2; shift;;
		-g)	graph=1;;
		-N)	nalarm=$2; shift;;

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

state=0
for c in ${cluster:-$def_cluster}
do
	echo "$c:"
	for n in $(ng_members -N -c $c )
	do
		echo "node $n"
		ng_rcmd $n ng_sumfs 
	done |rm_used_space 
	state=$(( $state + $? ))
	echo " "
done

if (( $alarm > 0 && $state > 0 ))
then
	cleanup 1
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_df:Generate a df like listing for replication manger filesystems)

&space
&synop	ng_rm_df [-A [-C cluster-treshold] [-N node-threshold]] [-a] [-c cluster] [-g]

&space
&desc	&This generates a listing similar to the system &lit(df) command sumarising all
	replication manager filesystems on a per node basis, and presenting one cluster 
	wide total.  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-A : Alarm mode.  Will cause this script to exit with a non-zero return code
	if any node is more than 94.9% full, or if the cluster as a whole is more than 89.9%
	full.  
&space
&term	-a : All clusters. All clusters are examined and presented. 
&space
&term	-c cluster : Specifies the cluster to examine.  If not supplied, the current cluster is
	examined. More than one -c option can be supplied.
&space
&term	-C threshold : Supplies the cluster alarm threshold (see -A). If the cluster utilisation 
	is more than the value supplied, the script will exit with a bad (1) return code. If -A is 
	not set, this value is meaningless. 
&space
&term	-g : Graph mode. A visual represation of each node, and the total, is presented along
	with the textual data. 
&space
&term	-N threshold : Supplies the node alarm threshold (see -A). If any node's utilisation 
	is more than the value supplied, the script will exit with a bad (1) return code. If -A is 
	not set, this value is meaningless. 

&endterms

&space
&exit	A non-zero exit code is generated only when running with the -A option and indicates that 
	one of the nodes, or the entire cluster, is at a higher level of utilisation than is 
	desired. 

&space
&see	ng_repmgr, ng_spaceman, ng_spaceman_nm, ng_df

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 May 2006 (sd) : Its beginning of time. 
&term	16 Apr 2007 (sd) : Added eye catcher to the nodes that trigger an alarm if -A is used.
&term	25 Jul 2007 (sd) : Added support for NG_RMDF_NALARM and NG_RMDF_CALARM to supply default 
	thresholds.

&term	03 Jun 2008 (sd) : Corrected potential divide by zero problems. 

&scd_end
------------------------------------------------------------------ */
