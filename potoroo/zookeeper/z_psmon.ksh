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
# Mnemonic:	ps_mon
# Abstract:	monitor processes across the cluster
#		
# Date:		08 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


export PATH="$PATH:$HOME/bin"

undercover=0
if [[ $* = *-r* && $* != *-f* ]]
then
	detach_log=/tmp/$LOGNAME.psmon
	$detach
	undercover=1
	exec >$NG_ROOT/site/log/z_psmon 2>&1
fi

# -------------------------------------------------------------------
export NG_WREQ_QUIET=1		#prevent wreq from issuing log messages

ustring="[-c cluster-list | -n node-list] [-f] [-h parrot-host]  [-r repeat-seconds] "
phost=srv_Netif
nodes=""
id="rmjobs"
repeat=0
dfmon=0;
dftimer=1
clust=""
send=1
verbose=0

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	clust="$clust$2 "; shift;;
		-f)	;;			# foreground ignore
		-h)	phost=$2; shift;;
		-n)	nodes="$nodes$2 "; shift;;
		-r)	repeat=$2; shift;;
		-t)	send=0;;

		-v)	verbose=$(( $verbose + 1 ));;
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
	refresh_nodes=1
fi

clust="${clust:-$NG_CLUSTER}"

ng_goaway -r ${argv0##*/}
while true
do
	if ng_goaway -c ${argv0##*/}
	then
		cleanup 0 
	fi

	>/tmp/bcast.$$
	for c in $clust
	do
		ng_ppget -C $c srv_Jobber|read jobber
		ng_ppget -C $c srv_Filer|read filer
		ng_ppget -C $c srv_Netif|read netif
		ng_ppget -C $c srv_Catalogue|read catter

		if [[ refresh_nodes -gt 0 ]]
		then
			nodes=`ng_rcmd ${c:-junk} ng_members -R`
		fi

		last_nodes="$nodes"
	
		for x in $nodes
		do
			set -o pipefail
			if [[ $verbose -gt 1 ]]
			then
				ng_rcmd -t 40  $x 'ng_ppget srv_Netif; ng_loadave; ng_ps -c' 
			fi

			ng_rcmd -t 40  $x 'ng_loadave; ng_ps -c' |awk \
			-v who=$x \
			-v cathost=$catter \
			-v jobhost=$jobber \
			-v filehost=$filer \
			-v nethost=$netif \
			'
			BEGIN {
				lave = "missing";
							# values are priority for display; if more than one down, lower number shows on gui
							# 1-10 are reserved for cluster wide services like repmgr/seneschal
				priority = 11;
				need["ng_parrot"] = priority++;
				need["ng_rm_dbd"] = priority++;
				need["ng_woomera"] = priority++;
				need["ng_shunt"] = priority++;
				need["ng_logd"] = priority++;
				need["ng_argus"] = priority++;
				#need["ng_spyglass"] = priority++;

				if( who == filer )
				{
					need["ng_repmgr"] = 1;
					need["ng_repmgrbt"] = 2;
				}

				if( who == jobber )
				{
					need["ng_seneschal"] = 3;		# expected on the jobber
					need["ng_nawab"] = 4;
				}

				if( who == cathost )
				{
					need["ng_catman"] = 5;		# expected only on the catalogue host
				}

				#if( who == nethost )
				#{
				#	need["ng_ftpmon"] = priority++;		# things expected only on netif node
				#}
			}

			function basename( what,		a, x )
			{
				x = split( what, a, "/" );
				return a[x];
			}

			/load ave/ {
				gsub( ",", "", $0 );
				lave = sprintf( "%s %s", $(NF-2), $(NF-1) );
				next;
			}

			{
				if( index( $1, "/ksh" ) )
					proc = basename( $2 );
				else
					proc = basename( $1 );


				found[proc]++;
				if( netif[proc] )
					isnetif++;
			}

			function check( what,	x )
			{
				for( x in what )
				{
					if( what[x] && !found[x] )
					{
						if( what[x] < missing )
						{
							mproc = x;
							missing = what[x];
							mcount++;
						}
					}
				}
			}

			END {
				missing = 99; 
				mcount = 0;
				for( x in need )
				{
					if( !found[x] )
					{
						if( need[x] < missing )
						{
							missing = need[x];
							mproc = x;
						}

						mcount++;
					}
				}


				if( mcount )		# missing processes -- overlay load average info
				{
					colour = 8;		# yellow -- screams louder than red if ya ask me!
					if( lave == "missing" )		# no load average -- system down?
						psinfo = sprintf( "down? (%d)",  mcount );
					else
						psinfo = sprintf( "%s(%d)",  mproc, mcount );
				}
				else
				{
					if( lave+0 > 3.5 )
						colour = 8;		# yellow
					else	
						colour = 9;		# cool purple

					psinfo = sprintf( "%s", lave );
				}
				printf( "RECOLOUR %s_nm 0 \"%s\"\n", who, who );
				printf( "RECOLOUR %s_psinfo %d \"%s\"\n", who, colour, psinfo );
			}
			' >>/tmp/bcast.$$
		
		done		# for each node in the cluster
	done			# for each cluster
	
	if [[ -s /tmp/bcast.$$ ]]
	then
		if [[ $send -gt 0 ]]
		then
			ng_sendudp $phost:$NG_PARROT_PORT </tmp/bcast.$$
			cp /tmp/bcast.$$ /tmp/$LOGNAME.psmon.bcast
		else
			cat /tmp/bcast.$$
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
&doc_title(ng_z_psmon:Zookeeper process monitor)

&space
&synop	ng_z_psmon [-c cluster] [f] [-h parrot-host] [-r seconds]

&space
&desc	&This looks at the nodes belonging to the indicated cluster and reports 
	processes that it feels should be running but are not.  Output from this 
	script is in the form that can be sent to W1kW to have it update one or 
	more displayed 'views.'  Messages are routed to listening W1kW processes
	via the indicated parrot.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : Looks at all nodes on &ital(cluster). If this option is omitted, 
	the cluster that the current node belongs to is examined.  
&space
&term	-f : Prevents the script from detaching from the TTY (runs in foreground).
&space
&term	-h node : The node running the parrot that &this should send update information 
	to. If not defined, the network interface node for the cluster (defined by 
	the srv_Netif pinkpages variable) will be used. 
&space
&term 	-r n : Repeat delay. Causes &this to repeat execution every &ital(n) seconds. 
&space
&term	 -v ": Verbose mode. 

&endterms


&space
&exit	Should always exit with a zero return code. 

&space
&see	ng_parrot, ng_z_dfmon, ng_z_rmmon

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2002 (sd) : Thin end of the wedge.
&term	11 Aug 2006 (sd) : Added spyglass and rmdbd to the list of processes monitored. 
&term	28 Aug 2006 (sd) : changed name for release, added doc.
&term	17 Mar 2008 (sd) : Changed to get all registerd members, not just active ones.
&endterms

&scd_end
------------------------------------------------------------------ */
