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

# Mnemonic:	spy_ckmisc
# Abstract:	misc checks that really do not warrent their own script.
#		invoke with the name of the function, followed by any parms that
#		the function needs. 
#		
# Date:		28 June 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# check for an excessive number of jobs queued against the named woomera queue
# $1 queue name, $2 threshold when limit==0, $3 threshold when limit>0
# if $3 is not supplied $2 threshold applies regardless of limit. If nether $2 nor
# $3 are supplied, threshold defaults to 250
function excessive_wqueue
{
	typeset count=0
	typeset threshold=${2:-250}		# default to this limit

	rc=0;
	ng_wstate -l -q ${1:-NO_RESOURCE} | read limit count

	if [[ -n $3 ]] && (( $limit > 0 ))
	then
		threshold=$3
	fi

	if (( ${count:-0} >= $threshold ))
	then
		log_msg "excessive jobs queued for woomera resource: $1 limit=$limit queued=$count [FAIL]"
		rc=1
	else
		verbose "number of jobs queued for woomera resource is sane: $1 limit=$limit queued=$count [OK]"
	fi
}

# if more than n ($1) processes for user ($2:-ningaui) then alarm. If user is ANY, then 
# we alarm if any user is over the limit.
function excessive_proc
{
	typeset user="${2:-ningaui}"
	typeset maxp=${1:-250}

	rc=0
	ng_ps >$TMP/PID$$.spy_eproc 
	awk -v max=$maxp -v user=$user '
		/USER.*PID.*PPID/ { next; }
		{
			l = length( $NF );
			if( substr( $NF, l ) != "]" )		# we ignore processes that are in []
				count[$1]++;	
		}

		END {
			ec = 0;
			for( x in count )
			{
				if( (user == "ANY" || x == user) && count[x] > max )
				{
					ec = 1;
					printf( "excessive process count for %s: %.0f   [FAIL]\n", x, count[x] );
				}
			}

				
			exit( ec );				
		}
	' <$TMP/PID$$.spy_eproc

	if (( $? != 0 ))
	then
		rc=1
		mv $TMP/PID$$.spy_eproc $TMP/spyglass/spy_eproc.$$.alert

		log_msg "ps listing saved: $TMP/spyglass/spy_eproc.$$.alert"
	else
		log_msg "counts for user=$user are within tollerances ($maxp) [OK]"
	fi

}

# verify the sanity of the ng_dosu binary. We assume that this test is not run on a satellite node, 
# as we assume satellite nodes not to have suid-root binaries allowed. This check need be run only 
# at startup time.
function dosu
{
	rc=0

	if [[  -f $NG_ROOT/bin/ng_dosu ]]
	then
		if [[ -f $NG_ROOT/bin/ng_FIXUP_dosu ]]
		then
			ng_dosu -? 2>&1|gre version | read dosu_vin
			ng_FIXUP_dosu -? 2>&1|gre version| read dosu_vnew
		
			if [[ $dosu_vin != $dosu_vnew ]]
			then
				log_msg "ng_dosu is out of date: new version of ng_FIXUP_dosu needs to be installed [FAIL]"
				rc=1
			else
				log_msg "ng_dosu version matches the latest installed FIXUP version [OK]"
			fi
		fi

		ls -al $NG_ROOT/bin/ng_dosu | read junk1 junk2 userid junk3
		if [[ $userid != root ]]
		then
				log_msg "root does NOT own ng_dosu and it must [FAIL]"
				rc=1
		else
				log_msg "ng_dosu is owned by root as expected [OK]"
		fi
	else
		log_msg "$NG_ROOT/bin/ng_dosu does not exist [FAIL]"
		rc=1

	fi

	return $rc
}

# tries to validate the PATH that is supplied in the cartulary.
function validate_path
{
	gre " PATH=" ${NG_ROOT:-/ningaui}/cartulary | tail -1 | awk ' { gsub( "^.*=\"", "", $0 ); gsub( "\".*", "", $0 ); gsub( ":", " ", $0 ); print $0; } ' | read dirs

	for d in $dirs 
	do
		if [[ ! -d $d ]]
		then
			rc=1				# global exit value -- exit bad please
			log_msg "directory in PATH smells: $d [FAIL] $bad"
		else
			log_msg "valid directory in PATH: $d [OK]"
		fi
	done
}

# alarm if cartulary age is more than $1 seconds. age is determined by the timestamp
# placed in the file by pp-build
function cartulary_age
{
	rc=0

	# expected line in cartulary: 
	# 4737873070 2009/01/05 15:35:07 /ningaui/bin/ng_pp_build generated file
	gre "ng_pp_build generated file" $NG_ROOT/cartulary | read csym timestamp junk
	if [[ -n $timestamp ]]			# no alarm if running with old ppbuild
	then
		now=$(ng_dt -i)
		threshold=${1:-300}
		now=$(( $now/10 ))
		timestamp=$(( $timestamp / 10 ))
		if (( $(( $now - $timestamp )) > $threshold ))
		then
			log_msg "cartulary is older than threshold: age=$(( $now - $timestamp ))  threshold=$threshold [FAIL]"
			rc=1
		else
			log_msg "cartulary age is within tolerence: age=$(( $now - $timestamp ))  threshold=$threshold [OK]"
		fi
	else
		log_msg "WARNING: pp_build is old and/or is not generating an update timestamp in the cartulary"
	fi

	return rc
}

# peeks in repmgr space and counts mlog fragment files.  Not good if there are 
# more than $1 of them. 
function mlog_frag
{
	typeset count=""

	if [[ -f $NG_ROOT/site/rm/dump ]]
	then
		gre -c "^file.*mlog" $NG_ROOT/site/rm/dump | read count
		if (( $count > $1 ))
		then
			log_msg "found $count mlog fragement files listed in repmgr dump (threshold = $1) [FAIL]"
			rc=1
		else
			log_msg "found $count mlog fragments, less than threshold ($1) [OK]"
		fi
	fi
}

function service_locations
{
	typeset sfile=$TMP/spyglass/service_state.data
	typeset now=$(ng_dt -i)
	typeset expiry_sec=1800
	
	while [[ $1 = -* ]]
	do
		case $1 in 
			-s)	expiry_sec=$2; shift;;
		esac

		shift
	done
	
	if [[ ! -f $sfile ]]
	then
		echo "timestamp=$now" >$sfile
		env | gre "^srv_" |gre -v "srv_Tapem_">> $sfile	# just save and return good if we dont have a previous state 
		rc=0
		return
	else
		echo "timestamp=$now" >$sfile.$$		# build current state to compare to previous state
		env | gre "^srv_" |gre -v "srv_Tapem_">> $sfile.$$	
	fi


	awk -v now=$now -v sfile="$sfile" -v expiry_sec=${expiry_sec}0 '
		BEGIN {
			while( (getline <sfile) > 0 )
			{
				split( $1, a, "=" );
				was[a[1]] = a[2];
			}
			close( sfile );

			alert = 0;

			if( now - was["timestamp"]  > expiry_sec )		# older than 30  min (tseconds) then we ignore it 
			{
				printf( "saved data file too old (%ds) to use; just catpured new data\n", (now - was["timestamp"])/10 ); 
				search4unref=0
				exit( 0 );
			}

			search4unref=1;
		}

		/srv_Tapem_/	{ next; }		# these are tape mount psuedo services and we dont track them 

		/^srv_/ {
			split( $1, a, "=" );
			if( was[a[1]] != a[2] )
			{
				if( was[a[1]] =="" )
					printf( "service has been enabled: %s  assigned to %s   [ALERT]\n", a[1], a[2] );
				else
				if( a[2] == "" )
					printf( "service has no host: %s  was assigned to %s   [ALERT]\n", a[1], was[a[1]]);
				else
				printf( "service has moved: %s  from %s to %s   [ALERT]\n", a[1], was[a[1]], a[2] );

				changed = changed a[1] " ";
				alert = 1;
				was[a[1]] = "";				# prevent picking up in search later
			}
			else
			{
				printf( "service unchanged: %s  hosted on %s   [OK]\n", a[1], was[a[1]] );
				was[a[1]] = "";
			}
		}

		END {
			if( search4unref )
				for( x in was )
					if( x != "timestamp"  && was[x] != "" )
					{
						changed = changed x " ";
						printf( "service undefined: %s (was %s) dropped?  [ALERT]\n", x, was[x] );
						alert = 1;
					}

			if( changed )
				printf( "subject_data: service location change noticed for: %s\n", changed );		# used as dynamic subject lin
			exit( alert );
		}
	' <$sfile.$$ >$TMP/output.$$
	rc=$?

	if (( $rc == 0  || $verbose > 0 ))
	then
		cat $TMP/output.$$
	else
		gre 'subject_data|ALERT' $TMP/output.$$
	fi

	mv $sfile.$$ $sfile			# save for next time
}

function boot_check
{
	echo "checking for reboots.... "
	rc=0

	dataf=$TMP/spyglass/reboot.data

	now=$( ng_dt -i )
	if [[ ! -f $dataf ]]
	then
		lastal=0
	else
		read lastal <$dataf		# last time we alarmed		
	fi

	echo $now >$dataf

	uptime | awk -v now=$now '
		function sec2hms( sec, 		h, m )
		{
			h = int( sec/3600 );
			sec -= h * 3600;
			m = int( sec/60 );
			sec -= m * 60;
			return sprintf( "%dh%dm%ds", h, m, sec );
		}
		{
			if( $4 == "mins," )		 #8:15  up 40 mins, 1 user, load averages: 0.42 0.30 0.26
				sec = $3 * 60;
			else
			if( split( $3, a, ":" ) > 1 )
				sec = (a[1] * 3600) + (a[2] * 60);

			printf( "%d %s\n", sec, sec2hms( sec ) )
			exit( 0 );
		}
	' |read sec_since hms			# seconds since last reboot

	if (( ${sec_since:-0} > 0 ))
	then
		lastrb=$(( $now - ($sec_since * 10) ))
		if (( $lastrb > $lastal ))		# a more recent reboot
		then
			log_msg "$mynode was rebooted $hms ago ($(ng_dt -p $lastrb)) [ALARM]"
			echo $now >$dataf
			rc=1
		else
			log_msg "node was rebooted $hms ago ($(ng_dt -p $lastrb)) already reported [OK]"
		fi
	else
		log_msg "$mynode up more than 1 day; assuming we reported things already [OK]" 
	fi
}

# check for critical errors in the master log
# assume seconds supplied as $1
function crit_check
{
	typeset sec=${1:-3600}
	typeset tfile=$TMP/crit_ck.$$

	rc=0
	ng_glog -l $log_file -d $sec | gre "CRIT.*$mynode" >$tfile 
	if [[ -s $tfile ]]
	then
		cat $tfile
		rc=1
	fi
}

# like crit-check, check for ALERT messages in the master log. For each that 
# we find, we send an email to the user(s) that are associated with the alert 
# users are associated by a pinkpages variable supplied as the first token after
# the pid ($5) which must be of the form @variable-name, or by the generic 
# pinkpages variable name NG_ALERT_NN_app where app is the name of the programme
# that generated the alart ($4). The list may be fully qualified names foo@bar.com
# or user names which are comma separated.  An unqualified name is sent to 
# the domain indicated by NG_DOMAIN. If NG_DOMAIN is not set, the current host
# name might be used if it contains the domain.  We build one message per user
# for all of the alert messages we find in the bit of masterlog that is sussed 
# through. We also include a distribution map at the end of the message to 
# attempt to communicate the receiver list for each alert type that we noticed.
#
# 13 Feb 2008 -- at Beth's request, if NG_ALERT_NN_default is set, that list is 
#	used as a fallback if no specific list is defined for the programme 
#	and if the list is not supplied in the message. 
function alert_check
{
	typeset sec=${1:-3600}
	typeset tfile=$TMP/alert_ck.$$
	typeset mbase=$TMP/alert_mail

	domain="$(ng_sysname -f)"	# overridden later if NG_DOMAIN is defined; sysname -f does not return full domain on some nodes
	domain=${domain#*.}		# just the domain for this not the node 
	
	ng_ppget >$tfile
	ng_glog -l $log_file -d $sec | gre "[[]ALERT".*$mynode |sort -u | awk \
		-v domain=$domain \
		-v mbase="$mbase" \
		-v tfile=$tfile \
		-v node=$mynode \
		-v tmp=$TMP/body.$$ \
		'
		BEGIN {
			while( (getline < tfile) > 0 )			# snarf in all ppvars
			{
				x = split( $0, a, "\"" );
				split( a[1], b, "=" );

				ppv[b[1]] = a[2];
			}
			close( tfile );

			if( ppv["NG_DOMAIN"] )				# override if set in pinkpages
				domain = sprintf( "%s", ppv["NG_DOMAIN"] );
		}

		# everything before [xxx] and the basename of that  (reduce /ningaui/bin/ng_foo[4360] to just ng_foo)
		function chop( s,		x, y, z )
		{
			split( s, z, "[" )
			x = split( z[1], y, "/" );
			return y[x];
		}

		{			
			app = chop( $4 );
			if( substr( $5, 1, 1 ) == "@" )		# @pinkpages-varname supplied in message, look up list using it
				list = ppv[substr( $5, 2 )];
			else
				if( (list = ppv["NG_ALERT_NN_"app]) == "" ) 	# generic list for the given application
					list = ppv["NG_ALERT_NN_DEFAULT"];

			data_host = data_path = "";			# if last token is host:/path/path/file we assume snarfable data
			if( split( $NF, data_loc, ":" ) == 2 )
			{
				if( substr( data_loc[2], 1, 1 ) == "/" )
				{
					data_host = data_loc[1];
					data_path = data_loc[2];
				}
			}
				

			if( list )
			{
				x = split( list, a, "," );
				for( i = 1; i <= x; i++ )	# for each recip, add the message to their set; we send one message with all at end
				{
					to = "";
					if( index( a[i], "@" ) )  		# assume fully qualified when present
						to = sprintf( "%s ", a[i] );
					else
						if( domain )			# if NG_DOMAIN not set, dont send blindly
							to = sprintf( "%s@%s ", a[i], domain );

					if( to )	
					{
						if( ! subj[to] )		# initial subject string for recipiant
						{
							idx[to] = 0;
							didx[to] = 0;
							subj[to] = "alert msgs on " node ": ";
						}
	
						if( ! seen[to,app]++ )		# add each application name, just once though
						{
							subj[to] = subj[to] app " ";
							distinfo[to,didx[to]] = app " msg(s) sent to: " list;
							didx[to]++;
						}

						msg[to,idx[to]] = $0;
						if( data_path != "" )
						{
							msg_datap[to,idx[to]] = data_path	# save location of data with msg info
							msg_datah[to,idx[to]] = data_host
						}
						idx[to]++;
					}
				}
			}
		}

		END {
			for( t in subj )	# for each target
			{
				for( i = 0; i < idx[to]; i++ )
				{
					printf( "%s\n\n", msg[to,i] ) >tmp;
					if( msg_datap[to,i] != "" )
					{
						printf( "Related data from %s:%s:\n", msg_datah[to,i], msg_datap[to,i] ) >>tmp;
						printf( "===========================================\n" ) >>tmp;
						close( tmp );
						cmd = "ng_rcmd " msg_datah[to,i] " \"head -10 " msg_datap[to,i] " \" >>" tmp " 2>&1";
						system( cmd );
					}
					printf( "===========================================\n" ) >>tmp;
				}

				printf( "\n-------------- distribution info ----------\n" ) >>tmp;
				for( i = 0; i < idx[to]; i++ )
					printf( "%s\n", distinfo[to,i] ) >>tmp;

				close( tmp );
				cmd = sprintf( "ng_mailer -v -T %s -s \"%s\" %s\n", t, subj[t], tmp );
				system( cmd );
			}
		}
	'
	rc=0			# this always exits good.
}

# compare time on all members against the time on this node; scream if a member is 
# different by more than 1s.  If the node on this node is bad, it will appear that
# all nodes are bad. This is unavoidable if trying to keep the probe simple, and 
# does produce an alarm as it should.  
#
# using ntpdate would have been the *right* approach (and reporting when its output 
# indicates a node is significantly out of whack, but some fool deprecated ntpdate
# (with the claim that ntpd -q is a replacement, which is is NOT). Grumble.
function time 
{
	otz=$( date +%H )		# current hour here for timezone difference 
	m="$( ng_members -N )"
	(
		for node in $m 
		do
			start=$(ng_dt -u)
			ng_rcmd $node 'echo "$(ng_sysname) $(ng_dt -u)"' | read stuff
			end=$(ng_dt -u)

			if (( $end - $start < 1 ))		# took longer than 1s -- slow system; dont report
			then
				if [[ -n $stuff ]]
				then
					echo "${stuff:-$node -1 0} $end"		# changed from start to end
				else
					echo "invalid response (empty) from $node; ignored" >&2		# assume net/daemon/greenlight will scream
				fi
			else
				echo "data from $node ignored; response too slow: rt=$(( $end-$start ))s diff=$(( ${start:-0} - ${stuff##* } )) ($end $start)" >&2
			fi
		done 
	) | awk -v otz=$otz '
			BEGIN { 
				tollerate = 1;			# allow clocks to show a 1s difference without alarm
			}

			{				# nodename ng-tstamp(remote) reftime(local)
				t = $3+0;		# time on this system that the remote time request was made
				rt = $2+0;		# time from remote system


				if( (d = t - rt) < 0 )			# difference -- keep positive
					d *= -1;

				if( d > tollerate )
				{
					printf( "timestamp for %s off by %ds [FAIL]\n", $1, d);
					state++;
				}	
				else
				{
					printf( "timestamp for %s by %ds [OK]\n", $1, d );
				}
			}
			END {
				exit( state > 0 )
			}
		'
	rc=$?
}



# assume the input file has system log timestamps (month day hh:mm:ss)
# spit out the lines that are after the current time - the threshold (minutes)
# $1 file, $2 threshold (5min default)
# because syslog messages do not cary the year, the effort to look back over the 
# year boundry is more than I want to put into this. So, if the threshold is 15 minutes, 
# this will not report anything until after 00:15 on January 1st, and if it is used for a 
# 'last 24 hour' check, it will fail until Jan 2.  
#-----------------------------------------------------------------------------------------
function syslog_extract
{
	typeset file=$1
	typeset threshold=${2:-5}
	typeset	day=""
	typeset hr=""
	typeset min=""
	typeset sec=""

	date "+%b %d %H %M %S %Y"| read mname day hr min sec year

	awk -v threshold=$threshold \
	-v day=$day \
	-v mname=$mname \
	-v hr=$hr \
	-v min=$min \
	-v sec=$sec \
	-v year=$year \
	'

	function mk_ts( hr, min, sec )
	{
		return (hr * 3600) + (min * 60) + sec;
	}

	# given m and d return seconds before the current day since jan 1
	# unfortunately m is expected as Jan, Feb...
	function sbefore( m, d,		i, db )
	{
		i = index( soup, m );
		return (substr( dsoup, i, 3 ) * 86400) + ((d-1) * 86400) ;		# seconds before this date
	}

	BEGIN {
		soup = "JanFebMarAprMayJunJulAugSepOctNovDec";		# month for easy conversion of name to number
		if( year % 4 == 0 && ( (year % 100) || (year % 400 == 0 )) )	# leap year
			dsoup = "000031060091121152182213244274305335";		# number of days before each month in a leap year
		else
			dsoup = "000031059090120151181212243273304334";		# number of days before each month
		
		start_ts =  (sbefore( mname, day ) +  mk_ts( hr, min, sec )) - (threshold * 60); 

		if( start_ts < 0 )
		{
			printf( "cannot compute -- threshold too big for current time" ) >"/dev/fd/2";
			exit( 1 );
		}
	}

	{
		split( $3, a, ":" );
		ts = sbefore( $1, $2 ) + mk_ts( a[1], a[2], a[3] );
		#printf( "start=%.0f ts=%.0f %s\n", start_ts, ts, $0 ) >"/dev/fd/2";
		if( ts+0 > start_ts+0 )
			print;
	}
	'<$file
}

# check the last n ($2) minutes for the system log for a specific string
function syslog_check
{
	typeset count=0
	typeset tfile

	get_tmp_nm tfile | read tfile

	if [[ -f /var/log/system.log ]]		# macs have to be bloody different
	then
		file=/var/log/system.log
	else
		file=/var/log/messages
	fi

	syslog_extract $file ${2:-5} | gre  "$1"  >$tfile
	if [[ -s $tfile ]]
	then
		log_msg "pattern matched in syslog file [ALARM]"
		cat $tfile
		rc=1
	fi
}

# ========================================================================================================

function usage
{
	cat <<endKat
usage:	$argv0 service_locations
	$argv0 mlog_frag n 
	$argv0 service_locations
	$argv0 crit_check [seconds]
	$argv0 alert_check [seconds]
	$argv0 boot_check
	$argv0 dosu
	$argv0 excessive_proc
	$argv0 time
	$argv0 syslog_check "string" [minutes]
	$argv0 validate_path 
endKat
}

# -------------------------------------------------------------------
ustring="test-name [testparms]"
verbose=0
log_file=$NG_ROOT/site/log/master

while [[ $1 = -* ]]
do
	case $1 in 

		-l)	log_file=$2; shift;;
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

mynode="$(ng_sysname)"

rc=0
"$@"

cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_ckmisc:Misc checks driven by spyglass)

&space
&synop	ng_spy_ckmisc test-name [test-parms]

&space
&desc	This script wraps several miscellanous tests that are driven by 
	ng_spyglass; these tests are not significant enough to merit their
	own script and thus are bundled here.  The following lists the 
	supported tests and the parameters that should be passed to them.
&space
&begterms
&term	mlog_frag n : Checks mlog fragments currently registered with repmgr and 
	returns a non-zero exit code when the count is greater than n. This 
	should be run only on the Filer. 
&space
&term	service_locations : Checks the location of services and reports an alert
	if one is different than the last time this script was executed.  If the 
	last execution was more than 30 minutes old, this script just captures
	a snapshot of the information and exits ok. The command can be given 
	the option -s n, where n is seconds, to define the maximum age of the 
	cached service information.  This value should be longer than the 
	frequency with which this command is run, or it will never do anything 
	more than collect statistics. 
&space
&term	crit_check sec : Checks the master log for critical  messages on the 
	current node. The optional parameter &ital(sec) is the number of 
	seconds to search back in the log for messages.  If &ital(s) is not 
	supplied, 3600 (1 hour) is used. 
&space
&term	alert_check sec : Checks the last &ital(sec) seconds in the master log
	for &lit(ALERT) messages. If it finds any it will send eMail to 
	targets referenced by @ppvarname ast the fifth token in the message, 
	or if the pinkpages variable &lit(NG_ALERT_NN_prog) is defined where
	&ital(prog) is the programme name (e.g. ng_break). 
	If both of those result in an empty list, the list assigned to the 
	pinkpages variable &lit(NG_ALERT_NN_DEFAULT) will be used.
	In all cases, the pinkpages variable is expected to contain a list of target user IDs 
	that the message will be sent to.  If the user id contains an at sign, 
	it is assumed to be fully qualified; otherwise the current value for 
	NG_DOMAIN is added. If more than one user ID is supplied via the 
	variable, they are expected to be comma separated. 
&space
	If the last token of an alert message is of the form:
&space
&litblkb
    node:/path/filename
&litblke
&space
	Then the first ten (10) lines of the file are included in the email message. 
&space
	If a user is to receive notification of multiple alerts, they will 
	be sent only a single email containing all of the alerts rather than 
	an email for each alert. The subject line will contain a generic 
	"alerts from node" string followed by the name of each programme that 
	caused an alert. 
&space
&term	boot_check : Exits with a non-zero return code if the node has been 
	rebooted during the past 24 hours and if it has not been reported. 
	The file $TMP/spyglass/reboot.data is kept to track when we last
	reported a reboot.
&space
&term	time : Queries the time on each node of the cluster and compares it 
	to the time on the current node. An alarm is raised if one or more 
	nodes are more than a second different than this node.  
&space
&term	cartulary_age : Verifies that the cartulary is recently created. The 
	default threshold is 301 seconds. If a different threshold is desired, 
	the thrshold can be supplied on the command line as the first positional
	parameter.
&space
&term	dosu : Verifies that the ng_dosu binary is installed and has the correct
	file mode. It also compares the current version with the last version 
	in packages (it is not automatically overlaid) and errors if the versions
	do not match. The check needs to be run only at startup.
&space
&term	excessive_proc n ^[user] :  Checks process counts and if more than &ital(n) 
	processes, owned by &ital(user) are found it alarms.  If user is omitted, 
	&lit(ningaui) is assumed. If user is "ANY," then we alarm if any user is 
	over the limit.

&space
&term	excessive_wqueue rname t0 tn : Checks the woomera resource &ital(rname) to 
	ensure that the current number of jobs queued is less than one of the supplied threshold
	values (t0 or tn). 
	The &lit(t0) threshold is used if the current limit for the resource is 0, or &lit(tn)
	is not supplied.  When &lit(tn) is supplied, this threshold is used when the 
	resource limit is positive. An alarm is generated when the number of queued 
	jobs for the resource exceeds the selected threshold. 

&space
&term	validate_path : Checks the PATH entry from the cartulary and ensures that 
	each component of the path is a valid directory.  

&space
&term	system_log string n : Checks the last n minutes of the system log for the 
	string (gre pattern). Alarms if the pattern is matched. 
&endterms

&files
	These files are maintained in &lit($TMP/spyglass) to track information
	between executions:
&space
&lgterms
&term	service_state.data : Contains state information of each service (srv_*)
	that we are reporting on.
&space
&term	reboot.data : Information about the last time &this returned an alert
	condition (non-zero) because it appears that the node had been rebooted.
&endterms

&space
&exit	An exit code of 0 indicates that the check found no issues. Any non-zero
	exit code indicates that there was one or more problem detected and 
	that human action might be needed. 

&space
&see	&seelink(spyglass:ng_spyglass), &seelink(spyglass:ng_spy_ckdaemon), &seelink(spyglass:ng_spy_ckfsfull)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Jun 2006 (sd) : Its beginning of time. 
&term	19 Jul 2006 (sd) : Added service location function.
&term	31 Jul 2006 (sd) : Added critical error message checking.
		Added check for node reboot.
&term	14 Aug 2006 (sd) : Fixed problem with boot-time interpretation of uptime string.
&term	18 Sep 2006 (sd) : Fixed bug with critcal message check. 
&term	01 Feb 2007 (sd) : Added alert_check to run the master log looking for alerts and to 
		send mail based on pp variables referenced in the message, or in pinkpages.
&term	22 Mar 2007 (sd) : Added support to check time differences. 
&term	05 Apr 2007 (sd) : Modified time check to better deal with response time of slow nodes.
&term	10 May 2007 (sd) : Corrected issue when a node does not respond when checking the current clock setting (time).
&term	14 May 2007 (sd) : Added ng_dosu check and excessive processes check.
&term	27 Jun 2007 (sd) : Fixed issue in time function; timezone difference was causing an issue under 
		certian circumstances. 
&term	19 Jul 2007 (sd) : Weeds out duplicate messages now. 
&term	13 Aug 2007 (sd) : Change to use NG_DOMAIN if set -- on some hosts the O/S does not return the full domain. 
&term	16 Aug 2007 (sd) : Corrected a typo that was causing bad addresses to be created. 
		Also added distribution information at the end.
&term	03 Dec 2007 (sd) : We ignore srv_Tapem_* services as they are tape mount psuedo services and we dont
		want to announce their movement. 
&term	07 Jan 2007 (sd) : Added system log check.
&term	13 Feb 2008 (sd) : Added default alert nickname list.
&term	18 Feb 2008 (sd) : Added leapyear check to syslog check so that it will work after 2/28 on a leap year.
&term	01 Mar 2008 (sd) : Fixed a problem that I swear I fixed before. Was not doing the right thing on the first
		of the month.
&term	12 Mar 2008 (sd) : Added sending of data from file on alert message if last token is host:/path/path/file.
&term	01 Aug 2008 (sd) : Added a function that validates the PATH stored in the cartulary.
&term	04 Aug 2008 (sd) : Corrected typo in awk; C style comment uses rather than #.
&term	29 Sep 2008 (sd) : We now capture top info when we detect an eccessive proc situation.
&term	05 Jan 2009 (sd) : Added cartular age check based on internal timestamp and not filesystem mod time.
&term	19 Jan 2009 (sd) : Tweeked the latency tollerence in the time function to less than 1s rather than less than 2s.
&term	22 Jan 2009 (sd) : Added the excessive woomera queue check.
&term	21 Oct 2009 (sd) : We now do not count processes in square brackets when checking for excessive process counts. 
&endterms


&scd_end
------------------------------------------------------------------ */
