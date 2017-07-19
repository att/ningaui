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
# Mnemonic:	ng_isolate
# Abstract:	isolate a node as best we can from the cluster. this does NOT
#		prepare the repmgr filesystems is is needed when removing the node
#		from the cluster as that must be done before isolation.  Once a
#		node is isolated, its cartulary file must be modified to 
#		allow it to rejoin the cluster.
#		
# Date:		22 Apr 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# wash things from the minimal cartulary in site by commenting them out
function wash_things
{
	if [[ ! -f $NG_ROOT/site/cartulary.min ]]
	then
		log_msg "$NG_ROOT/site/cartulary.min does not exist; not washed"
		return
	fi

	awk -v date="$(date)" '	
	BEGIN {
		vnames = "NG_CLUSTER= NG_LOGGER_PORT=" 			# these var names are washed away
		nlist = split( vnames, list, " " );

		printf( "# === this file was washed by ng_isolate: %s ===\n", date );
	}

	/this file was washed.*isolate/ { next; } 
	{
		for( i = 1; i <= nlist; i++ )
			if( index( $0, list[i] ) )
				break;

		if( i <= nlist )		# found this in the list; scrub it
		{
			printf( "# removed by isolate: %s\n", $0 );
		}
		else
			print $0;
	}
	' <$NG_ROOT/site/cartulary.min >$TMP/PID$$.cmin

	if (( $allow_commands ))
	then
		mv $NG_ROOT/site/cartulary.min $NG_ROOT/site/cartulary.min-
		mv $TMP/PID$$.cmin $NG_ROOT/site/cartulary.min
		log_msg "NG_ROOT/site/cartulary.min was washed; original backed up as $NG_ROOT/site/cartulary.min-"
	else
		log_msg "did not wash site/cartulary.min"
	fi
}


# find any pp variable that references the node and issue a warning
# ignores attribute and service vars
function suss_pp_ref
{
	typeset node=$1
	typeset tfile=""

	get_tmp_nm tfile | read tfile
	ng_ppget | gre "=.*$me"  |gre -v "srv_|NG_CUR_NATTR" >$tfile
	if [[ -s $tfile ]]
	then
		cautioned=1
		log_msg "WARNING: these pinkpages variables still reference this node:"
		cat $tfile >&2
	fi

	rm -f $tfile
}


# find and ditch any pinkpages variables that are specific to this node. this uses 
# narbalek and NOT the pp scripts
#
function ditch_pp_var
{
	typeset pfile=$NG_ROOT/site/isolate.$me.ppv

	ng_nar_get | grep "pinkpages/$me" | sed 's/#.*//' >$pfile
	log_msg "deleting $(wc -l <$pfile) pinkpages variables for $me"
	if (( ${allow_cmds:-0} > 0 ))
	then
		awk '
			{
				split( $0, a, "=" );
				printf( "ng_nar_set %s\n", a[1] );
			}
		' <$pfile |ksh 
	else
		log_msg "-n mode set, pinkpages variables not deleted"
	fi

	log_msg "pinkpages values saved in $pfile"
}

# run the command string passed in if -n was not set on command line
# any redirection needed in the command must be quoted on the command line 
# or the file will be touched which might not be desired.
function forreal
{
	typeset redirect=""

	while [[ $1 == -* ]]
	do
		case $1 in
			-a) redirect=">>$2"; shift;;
			-o) redirect=">$2"; shift;;
		esac
		
		shift
	done

	if (( $allow_cmds > 0 ))
	then
		eval "$@" $redirect
	else
		log_msg "would execute: $@ $redirect"
	fi
}

# -------------------------------------------------------------------
ustring="[-f] [-n] [-r] [-v]"
allow_cmds=1
restart=0
force=0
ng_sysname | read me
vflag=""
allow_logfrag=1
ng_members |wc -w | read member_count
cautioned=0			# set if we issued a warning; forces an 'are you sure prompt' before doing any harm

while [[ $1 == -* ]]
do
	case $1 in 
		-f)	force=1;;
		-n)	allow_cmds=0;;
		-r)	restart=1;;
		-v)	verbose=1; vflag=-v;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


if [[ -z $me ]]
then
	log_msg "abort! cannot determine host name using ng_sysname"
	cleanup 1
fi

if ! amIreally ${NG_PROD_ID:-ningaui}
then
	log_msg "this command must be run as ${NG_PROD_ID:-ningaui}: you appear to be: $(id -nu)"
	cleanup 1
fi

if [[ ! -d $NG_ROOT ]]
then
	log_msg "abort: NG_ROOT seems not to be a directory: ${NG_ROOT:-var-unset}"
	cleanup 1
fi

for x in Steward Alerter Jobber Filer Netif 
do
	if [[ $NG_SRVM_LIST != *"$x"* ]]		# warn only if the service is not under srvmgr
	then
		if ng_test_nattr $x
		then
			cautioned=1
			log_msg "WARNING: this node is currently the $x node which is NOT under service supervision"
		fi
	fi
done

# see if greenlight is running
if ! ng_wrapper -s
then
	log_msg "WARNING: some/all ningaui processes/daemons may not be running; it may not be safe to continue"
	cautioned=1
fi

rdir="$(ng_spaceman_nm)"		# get a directory in repmgr space to test for out of space condition

if [[ ! -d $rdir ]]
then
	log_msg "WARNING: replication manger space appears to be full on this node"
	log_msg "	continuing the isolation process will skip log_fragment processing."
	log_msg "	recommended action is to abort, free space in repmgr and run this "
	log_msg "	command again"
	cautioned=1
	allow_log_frag=0
fi

# search and issue warnings if node referened in ppvars; sets cautioned if a warning is issued
suss_pp_ref

#----------------------------------------------------------------------------------------------------
# DO NOT DO DAMAGE BEFORE THIS PORINT!!!  confirm if we issued a warning that they want to continue!
# ---------------------------------------------------------------------------------------------------
if (( $cautioned > 0 && $force == 0 ))
then
	printf "$(ng_dt -p) ${argv0##*/}: warnings issued; continue isolation anyway? [n]: "
	read a
	if [[ $a != y* ]]
	then
		log_msg "user aborted isolation"
		cleanup 1
	fi
fi

if amIreally ${NG_PROD_ID:-ningaui}			# ditch crontable early to prevent rebuild of cartulary after ppvars removed
then
	log_msg "removing ningaui crontab"
	forreal "echo 'y' | crontab -r "		# the y is needed to prevent an additional prompt on freebsd
else
	log_msg "user is not ${NG_PRODID:-ningaui}; crontab NOT removed"
fi

forreal ng_log_sethoods $vflag -d $me			# reset masterlog hoods away from this node

if (( $allow_logfrag > 0 ))
then
	forreal ng_log_frag -v					# collect last fragment and send it away
else
	log_msg "log fragment process was skipped; replication manager free space too small"
fi

ditch_pp_var						# blow those little variables away (save a list of settings in site)


dc=$(ng_dt -i)				# date component of the file name
if (( ${member_count:-1} < 2 ))
then
	log_msg "this is the last member of the cluster; log fragment file cannot be coppied; recycle (.rec) file saved in $TMP"
	forreal cp "$NG_ROOT/site/log/master.frag $TMP/log_frag.isolate_$me.$dc.rec"
else
	verbose "copy current masterlog fragment to other nodes for log_comb/log_frag processing"
	for n in $srv_Jobber $srv_Steward $srv_Netif
	do
		if [[ $n != $me ]]
		then
			ng_rcmd $n ng_wrapper -r | read rroot
			if [[ $rroot != /* ]]
			then
				log_msg "did not send master log fragment to $n: could not get valid NG_ROOT value"
			else
				forreal ng_ccp $vflag -d $rroot/site/log/log_frag.isolate_$me.$dc.rec $n $NG_ROOT/site/log/master.frag
			fi
		fi
	done
fi

# do NOT do these if this is a clone as it would hurt the real node (clone option removed, but this warning is left as a reminder)
# do these before node stop so that they get into narbalek
log_msg "setting cluster to no_cluster"
forreal ng_ppset -l -c "'set by isolate'" NG_CLUSTER=no_cluster
forreal -a $NG_ROOT/site/cartulary.min echo '"NG_CLUSTER=no_cluster   #set by isolate"'
forreal -a $NG_ROOT/site/cartulary.min echo '"NG_NARBALEK_LEAGUE=isolated   #set by isolate"'
#forreal ng_ppset -l -c "'set by isolate'" NG_SUM_FS_DISCOUNT="-d 100"
forreal ng_add_nattr -p Isolated
forreal ng_sreq -t 5 -s ${srv_Jobber:-no-jobber} nattr ${me:-bad_host_name} "$(ng_get_nattr -p)"	# send our attributes to seneschal

ng_rcmd ${srv_Filer:-filer-node-missing} ng_wrapper -e NG_ROOT | read froot
if [[ -n $froot ]]
then
	log_msg "removing flist file on Filer ($srv_Filer) $froot/site/rm/nodes/$me"
	forreal ng_rcmd ${srv_Filer:-filer-node-missing} "rm -f $froot/site/rm/nodes/$me"
else
	log_msg "could not determine NG_ROOT for filer ($srv_Filer), flist not removed"
fi


if ng_wrapper -s
then
	log_msg "stopping all node daemons"
	if ! ng_test_nattr Filer 
	then
		log_msg "running srv_filer stop to clear any old repmgrbt processes"
		forreal ng_srv_filer stop		# if not the filer, drive here (not driven in ng_node) to force old repmgr bt away it if was still there
	fi
	forreal ng_node terminate
	forreal ng_endless -e ng_spyglass
	sleep 1
	forreal ng_sgreq -e
fi


log_msg "disabling repmgr filelist scripts"
forreal ng_goaway -X ng_flist_send
forreal ng_goaway -X ng_flist_rcv
forreal ng_goaway -X ng_rota


log_msg "rebuilding a minimal cartulary"
if ! cd $NG_ROOT
then
	log_msg "ERROR: cannot cd to $NG_ROOT (NG_ROOT)"
	cleanup 1
fi

forreal rm cartulary
forreal cp $NG_ROOT/lib/cartulary.min cartulary
forreal "echo export NG_ISOLATED=1 >>cartulary"
wash_things

log_msg "releasing node in seneschal"
forreal ng_sreq -t 300 release node $me

log_msg "NOTE: if isolating more nodes, ng_pp_build must be executed on ALL nodes before isolating the next node"
if (( $restart > 0 ))
then
	log_msg "restarting daemons"
	forreal ng_init
else
	log_msg "daemons not restarted; preventing accidental restart"
	forreal ng_goaway -X ng_init			# prevent node from starting any potoroo daemons at all
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_isolate:Drop a node from the cluster)

&space
&synop	ng_isolate

&space
&desc	&This will isolate a node from its current cluster by performing the 
	following tasks:
&space
&beglist
&item	Sets the Isolated attribute for the node.
&item	Drops all masterlog hoods assigned to this node.
&item	Copies the current masterlog fragment to other nodes for log_frag processing at the regularly scheduled time.
&item	Stopps all ningaui daemons
&item	Removes the ningaui crontable
&item	Deletes all pinkpages variables that are local to the node. 
&item	Disables the &lit(ng_flist_send) script (using ng_goaway)
&item	Removes the cartulary, then building a minimal one.
&item	Sets the cluster name &lit(no_cluster) in the node's cartulary file
.** &item	Setting the default leader to &lit(no_leader) in the node's cartulary file.
&item	Restarts the node
&endlist
&space
	Once a node has been dropped into an isolated state, various errors and/or 
	warnings are to be expected.
&space
&subcat	WARNING:
	Before a node is isolated from the cluster, in preparation for complete removal,
	the necessary steps to detach the replication manager filesystems from 
	replication manager's view must be completed.  &This makes no effort to 
	ensure that the replication manager filesystems are properly detached, nor 
	that an empty filelist has been sent to the Filer node. 

&space
&subcat DANGER:
	When isolating more than one node on a cluster extreme care must be taken to prevent
	losing copies of files in replication manager space, and recent master log data. 
	Before the next node is isolated the cartulary must be refreshed on each remaining 
	node in the cluster (execute ng_pp_build on each node to force it), and all replication 
	manager files must stabalise.  
&space

&opts	These options are recognised when placed on the command line:
&begterms
.** clone removed 6/24/08
.** &term	-c : Node has been cloned.  Do not attempt to dislodge the current host name from 
.** its cluster as it is likely running with the old hostname. Also causes a no execution
.** file for ng_init to be set. 
&space
&term	-n : No execution mode. Lists what would be done to the standard error device.
&space
&term	-r : Restart. After isolation, the ningaui daemons are restarted. 
&endterms

&space
&exit	A non-zero return code from this scirpt indicates failure.

&space
&see	ng_init, ng_node, ng_start

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Apr 2004 (sd) : Fresh from the oven.
&term	02 Aug 2005 (sd) : No longer references deprecated things in */adm.
&term	03 Dec 2005 (asm) : Added NG_ISOLATED to cartulary.
&term	05 Dec 2005 (sd) : Changed forreal into a function so that the redirection 
		added on 12/03 will not take place if the -n flag is supplied.
	
&term	18 Feb 2006 (sd) : Added caution if the node is a service provider (Filer, 
	Jobber, etc). Gives user the option to abort before isolating a key node. 
&term	22 Mar 2006 (sd) : Fixed % issue with log_msg (which now uses printf).
&term	18 Jul 2006 (sd) : Added -c (clone) option. Shuts off ng_init, and does not 
	dislodge the node (likely still using the old hostname) from the cluster.
	Also checks for greenlight before attempting to stop processes. 
&term	20 Oct 2005 (sd) : Added a call to ng_sreq to release the node in seneschal.
&term	20 Mar 2007 (sd) : Pulled reference to the logger service.
&term	11 May 2007 (sd) : Corrected bug with setting pinkpages variables. Now sets 
		the node attribute Isolated. Calls ng_log_sethoods to drop this 
		node from the masterlog hood assignments.
&term	11 Jun 2007 (sd) : Now sets NG_SUM_FS_DISCOUNT rather than SUM_FS_DISCOUNT.
&term	06 Sep 2007 (sd) : Added copy of current masterlog fragment file to other nodes
		for processing during next log_frag run.  The file is copied as a 
		recycle file with isolate as part of the name for easy idenfitication.
&term	14 Nov 2007 (sd) : Added warning if the node is referenced in any pinkpages variable value.
		Added removal of file list on Filer, and removal of pinkpages variables that 
		are local to the node.
&term	15 Nov 2007 (sd) : Fixed call to ng_log_sethoods such that it happens before we turn 
		the NG_CLUSTER variable off. 
&term	20 Nov 2007 (sd) : Added a small hack to prevent crontab removal on FreeBSD from propmting.
&term	26 Nov 2007 (sd) : Fixed typo in commands generated to remove pp vars. Now stops spyglass.
&term	10 Mar 2008 (sd) : Now forces a node attribute send to seneschal so that if the node goes down
		before rota sends off the latest set, seneschal will get the new attribute. (bug1050).
&term	15 May 2008 (sd) : Moved crontab removal to the 'top' of processing to prevent rebuild of 
		cartulary after we start the isolate process. 
&term	21 May 2008 (sd) : Fixed reference to host name var which was wrong and generating a null string. 
&term	12 Jun 2008 (sd) : Added function to wash certain vars from the site/cartulary.min if they are there. 
&term	16 Jun 2008 (sd) : No longer set SUM_FS_DISCOUNT.
&term	24 Jun 2008 (sd) : Removed clone option.
&term	15 Jul 2008 (sd) : Now sets narbalek league in cartulary.min; set to isolated.
&term	13 Nov 2008 (sd) : Fixed typo in forreal function that was causing -a to be passed to eval.
&term	31 Jan 2009 (sd) : Tweeked the loop that sends the current masterlog fragment so that the 
		date component of the file name is the same for each.  Also verifies that a 
		full path is returned for the remote NG_ROOT value when saving the log fragment.
&term	16 Apr 2009 (sd) : Now checks for running ningaui processes (greenlight) and issues a caution 
		if the system is not 'up.' 
&term	28 Apr 2009 (sd) : Ng_node is now invoked with the terminate option rather than stop. This allows
		node stop messages to be captured in this log rather than to be put into the node_stop
		log (a new ng_node feature). 
&term	13 Aug 2009 (sd) : Corrected man page.
&term	20 Aug 2009 (sd) : Now invokes srv_filer when NOT the filer to ensure that any leftover repmgrbt
		processes are killed.  If the node is the filer, ng_node will handle; 
		isolate is an extreme case and we dont want ng_node to blast away repmgrbt on a non-filer node.
&endterms

&scd_end
------------------------------------------------------------------ */
