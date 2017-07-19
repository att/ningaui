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
# Mnemonic:	spy_ckdaemon
# Abstract:	check daemons on the node
#		
# Date:		26 June 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
PROCFUN=${NG_STDFUN:-$NG_ROOT/lib/procfun} 
. $STDFUN
. $PROCFUN

# create a file that is $1 seconds old
# usage: mk_oldfile seconds-old filename
function mk_oldfile
{
	now=`ng_dt -i`
	then=$(( $now - ($1 * 10 ) ))			# tenths of seconds
	then=`ng_dt -d $then`				# to yyyymmddhhmmss
	then=${then%??}					# chop sec because bloody touch wants .ss which is a pain
	touch -t $then $2
}

function ck4running
{
	typeset count=""

	ng_proc_info -c "$1" "$2" <$psfile | read count
	if (( ${count:-0} > 0 ))
	then
		log_msg "$1 is running	[OK]"
	else
		log_msg "$1 is NOT running  [FAIL]"
		gre "$1" $psfile >&2
		rrc=1
		rc=1
	fi
}

function ck_catman
{
	typeset count=0

	rrc=0
	ck4running ng_catman
	ck4running ng_start_catman

	if (( $rrc == 0 ))		# if both are running
	then
		ng_catalogue -c -g 12345 | wc -l |read count
		if (( $count < 1 ))
		then
			log_msg "unable to communicate with catman [FAIL]"
			rc=1
		else
			log_msg "communication with catman successful [OK]"
		fi
	fi
}

# nnsd -- narbalek name service daemon -- runs only on nodes listed in NG_NNSD_NODES
# if we are in that list then we test
function ck_nnsd
{
	rrc=0
	if [[ "$NG_NNSD_NODES" == *"$mynode"* ]]
	then
		ck4running ng_nnsd
		if (( ${rrc:-0} == 0 ))
		then
			echo "0:list:"| ng_sendudp -q 1 -t 5 -r "." localhost:${NG_NNSD_PORT:-29056} >$TMP/spy_nnsd_data.$$
			if [[ -s $TMP/spy_nnsd_data.$$ ]]
			then
				log_msg "legit nnsd data received from query [OK]"
				cat $TMP/spy_nnsd_data.$$
			else
				log_msg "no data received from direct query to nnsd [FAIL]"
				rc=1
			fi
		fi
	else
		log_msg "no test needed this node ($mynode) not in list: $NG_NNSD_NODES  [OK]"
	fi
}

function ck_equerry
{
	if gre "^#equerry-begin" /etc/fstab >/dev/null		# we only care if the cookie is there
	then
		rrc=0						# running state
		ck4running ng_equerry				# sets both rc and rrc to 1 if not running and writes message
		if (( ${rrc:-0} == 0 ))
		then
			ng_eqreq -t 30 list | wc -l | read c		# if it responds it is alive and functioning
			if (( ${c:-0} > 0 ))
			then
				log_msg "list data received from equerry; responding [OK]"
			else
				log_msg "equerry did not respond to a list request [FAIL]"
				rc=1
			fi
		fi
	else
		log_msg "no test needed, equerry cookie (^#equerry-begin) not in /etc/fstab [OK]"
	fi
}

function ck_srvmgr
{
	rrc=0
	if [[ -z $NG_SRVM_LIST ]]		# no list then we dont care; report good
	then
		log_msg "NG_SRVM_LIST is not defined, no sense in checking [OK]"
		rc=0
		return
	fi
	
	ck4running ng_srvmgr
	if (( $rrc == 0 ))
	then
		ng_smreq -s localhost dump|wc -l|read count
		if (( ${count:-0} < 1 ))
		then
			log_msg "unable to communicate with srvmgr [FAIL]"
			rc=1
		else
			log_msg "srvmgr responded to dump [OK]"
			rc=0
		fi
	fi
}

function ck_woomera
{
	typeset count=0

	rrc=0
	ck4running ng_woomera
	ck4running ng_wstart

	if (( $rrc == 0 ))		# if both are running
	then
		ng_wreq explain foo| gre -c foo|read count
		if (( $count < 1 ))
		then
			log_msg "unable to communicate with woomera [FAIL]"
			rc=1
		else
			log_msg "communication with woomera successful [OK]"
		fi
	fi
}

function ck_narbalek
{
	typeset count=0

	rrc=0
	ck4running ng_narbalek

	if (( $rrc == 0 ))		# if both are running
	then
		ng_nar_get | wc -l |read count
		if (( $count < 1 ))
		then
			log_msg "unable to communicate with narbalek [FAIL]"
			rc=1
		else
			log_msg "communication with narbalek successful [OK]"
		fi
	fi
}

function ck_seneschal
{
	typeset count=0

	rrc=0
	ck4running ng_seneschal

	if [[ ${NG_SRVM_LIST:-none} != *Jobber* ]]	# if under service manager control, there is no s_start up all the time
	then
		ck4running ng_s_start
	else
		log_msg "jobber is under service supervision; not checking for s_start [OK]"
	fi

	if (( $rrc == 0 ))		# if both are running
	then
		ng_sreq explain foo | gre -c foo| read count
		if (( $count < 1 ))
		then
			log_msg "unable to communicate with seneschal [FAIL]"
			rc=1
		else
			log_msg "communication with seneschal successful [OK]"
			ng_sreq dump | gre -c SUSPEND | read count
			dataf=$wdir/ckd_sene.data 
			if (( $count > 0 ))
			then
				if [[ ! -f $dataf ]]
				then
					log_msg "seneschal is suspended; first time noticed [OK]"
					ng_dt -i >$dataf		# just save the first time we noticed it
				else
					read ts <$dataf			# get the timestamp of when we noticed it
					elapsed=$(( $(ng_dt -i) - ts ))
					if (( elapsed > 12 * 36000 ))		# if suspended more than 12 hours (tenths of seconds)
					then
						log_msg "seneschal has been suspended for more than 12 hours [FAIL]"
						rc=1
					else
						log_msg "seneschal suspended $(($elapsed/10))s is less than 12 hour threshold [OK]"
					fi
				fi
			else
				log_msg "seneschal is not suspended [OK]"
				rm -f $dataf
			fi
		fi
	fi
}

function ck_nawab
{
	typeset count=0

	rrc=0
	ck4running ng_nawab
	if [[ ${NG_SRVM_LIST:-none} != *Jobber* ]]	# if under service manager control, there is no n_start up all the time
	then
		ck4running ng_n_start
	else
		log_msg "jobber is under service supervision; not checking for n_start"
	fi

	if (( $rrc == 0 ))		# if both are running
	then
		ng_nreq explain foo | gre -c foo| read count
		if (( $count < 1 ))
		then
			log_msg "unable to communicate with nawab [FAIL]"
			rc=1
		else
			log_msg "communication with nawab successful [OK]"
		fi
	fi
}

function ck_parrot
{
	typeset count=0

	rrc=0
	ck4running ng_parrot
	if (( $rrc == 0 ))	
	then
		ng_rcmd -t 30 localhost ls | wc -l |read count
		if (( $count <= 0 ))
		then
			log_msg "unable to communicate with parrot [FAIL]"
			rc=1
		else
			log_msg "communication with parrot successful [OK]"
		fi
	fi
}

function ck_repmgr
{
	typeset count=0

	rrc=0
	ck4running ng_repmgr
	ck4running ng_repmgrbt
	ck4running ng_rm_start
	ck4running ng_rmbt_start

	if (( $rrc == 0 ))		# if all are running
	then
		typeset tf=$TMP/test_rm.$$		
		mk_oldfile 900 $tf 			# a 15 minute old file for test of dump and log
		if [[ $NG_ROOT/site/rm/dump -nt $tf ]]
		then
			log_msg "dumpfile updated in the last 15 minutes [OK]"
		else
			log_msg "dumpfile NOT updated in the last 15 minutes [FAIL]"
			log_msg "$(ls -al $NG_ROOT/site/rm/dump)"
			rc=1
		fi

	
		echo "dump1 spyglass.test.nonexistant.file!" | ng_rmreq  -a >$TMP/ckrm.$$
		if gre "not found: spyglass" $TMP/ckrm.$$ >/dev/null
		then
			log_msg "communication with repmgr (dump1) returned legit data [OK]"
		else
			log_msg "no data or bad data from dump1 rquest [FAIL]"
			rc=1
		fi
	fi
}

function ck_dbd
{
	typeset count=0

	ls -al $NG_ROOT/bin/ng_rm_db | awk '
                /ng_rm_dbd/ { exit( 0 ); }
                /ng_rm_db_/ { exit( 1 ) }       # one of the other puppies that is non-daemon
        '
        if (( $? > 0 ))
        then
                log_msg  "rm_dbd not being used on this node; no test"
                rc=0
                return
        fi


	rrc=0
	ck4running ng_rm_dbd
	ck4running ng_rm_dbd_start

	if (( $rrc == 0 ))		# if all are running
	then
		ng_rm_db -p|wc -l |read count
		if (( $count > 0 ))
		then
			log_msg "rmdb (ng_rm_dbd) is responsive to -p command [OK]"
		else
			log_msg "rmdb (ng_rm_dbd) is NOT responsive to -p command [FAIL]"
			rc=1
		fi
	fi
}

# shunt does not have an easy ping interface, and may not update its log file very often to use that 
# as an indicator as to its being hung.
function ck_shunt
{
	typeset count=0

	rrc=0
	ck4running ng_shunt
}

# -------------------------------------------------------------------
rc=0
mynode="$(ng_sysname)"
ustring="ng_spy_ckdaemon -v {all| catman | dbd | equerry | narbalek | nawab | parrot | repmgr | seneschal | shunt | woomera }"

if [[ -d $TMP/spyglass ]]		# should be there, but lets not fail if it isnt
then
	wdir=$TMP/spyglass
else
	wdir=$TMP
fi

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

psfile=$TMP/ps.$$
ng_ps >$psfile			# we will also use the timestamp on the file to test for current log file updates
if [[ $1 == "all" ]]
then
	set equerry woomera seneschal nawab repmgr parrot dbd shunt nnsd
fi

while [[ -n $1 ]]
do
	log_msg "testing $1"
	ck_$1
	echo ""
	shift
done
cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_ckdaemon:Check daemons for ng_spyglass)

&space
&synop	ng_spy_ckdaemon {all| catman | dbd | narbalek | nawab | nnsd | parrot | repmgr | seneschal | shunt | woomera }

&space
&desc	&This tests to see if the named daemon on the command line is running, and
	if it is, a test is made to determine if the daemon seems to be hung.  For
	daemons with TCP/IP interfaces (woomera, seneschal, nawab), a test message is 
	sent by the ng_Xreq command and the output is examined.  For daemons like 
	replication manager, the log file or dump file is tested to see if it has been 
	updated in the last 15 minutes. 
&space
	While these tests are not  fool-proof, they should give a general idea of 
	the state of the daemon. 

&space

&space
&parms	&This expects one or more daemon names on the command line.  If the word 'all' is 
	used, then all daemons are tested.  
	Daemon names that are recognised are: woomera, seneschal, nawab, repmgr, parrot, dbd, 
	shunt and nnsd. 

&space
&exit
	An exit code that is not zero indicates that the one or more of the tested daemons is not 
	running, or a needed support/start script is not running. An exit code of 0 indicates
	that the daemon, and its support processes, seem in good shape. 

&space
&see	ng_spyglass, ng_spy_ckfage, ng_spy_ckfsfull

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Jun 2006 (sd) : Its beginning of time. 
&term	14 Aug 2006 (sd) : Added check for nnsd daemon.
&term	17 Aug 2006 (sd) : Corrected to deal with jobber running under service management.  Added service mananager check.
&term	04 Oct 2006 (sd) : Set sane limits on all ng_rcmd invocations.
&term	17 Oct 2006 (sd) : Added -c to proc_info call to get count rather than pid. 
&term	04 Jan 2008 (sd) : Added check for suspended seneschal to alarm if suspended more than 12 hours.
&term	03 Apr 2008 (sd) : Added check for equerry. If equerry cookie is not in the fstab, then the check is 
		successful regardless of the process state. 


&scd_end
------------------------------------------------------------------ */
