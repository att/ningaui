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
# Mnemonic:	get a bunch of messages from the master log
# Abstract:	
#		
# Date:		ported to ningaui: 25 Aug 2003 (HBD RH)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# set variables to search all messages in all files (-a or -R options)
function set4all
{
	si=0; ei=$(ng_dt -i 20250203020000)		# all log messages
	sd=19950101000000; ed=20250203020000
	smld=19950101000000; emld=20250203020000
}

# -------------------------------------------------------------------
ustring="[-a] [-F] [-d duration(seconds)] [-l logname] [-L log-list-file] [-p pattern]  [-r outputfile [-R count]] [yyyymmddhhmmss_start [yyyymmddhhmmss_end]]"

typeset -f one_week=$(( 864000 * 7 ))	# one week in tenths of seconds

typeset -f 0 duration=0
rollup_out=""			# -r turns on -- we generate a list of masterlog files and invoke ourself via rollup
fast=0
inlog=""			# list of masterlog files to suss from the command line
flist="$TMP/glog_flist.$$"	# list of master log files in a flist file
uflist=""			# user file list file
pat=""
vflag=""
recent_count=0			# -R sets and causes exactly the last n logs to be sussed (rollup only)
pass_option=""

				# starting and ending date defaults
typeset -f ei=$(ng_dt -i)                	# end integer time (default to just a few min allowing overrides on cmd line)
typeset -f si=$(( $ei - 3000 )) 		# start int time; start 5 minutes ago
typeset -f emld="$(ng_dt -d $(( $ei + $one_week)) )"	# end/start in format used to determine range of masterlog files in rollup mode
typeset -f smld="$( ng_dt -d $(( $ei - $one_week )) )"
sd=$(ng_dt -d $si)
ed=$(ng_dt -d $ei)
ignore_list="";

while [[ $1 = -* ]]    
do
	case $1 in 
		-a)	set4all;;			# set vars to suss all messages regardless of date

		-d)	typeset -f duration="$2" ; shift;;
		-F)	fast=250000;;
		-f)	fast=$2; shift;;
		-i)	ignore_list="-i $2"; shift;;	# pass into rollup
		-L)	uflist=$2; shift;;		# list of masterlog files to suss through
		-l)	inlog="$inlog$2 "; shift;;	# add a file to the list to do
		-n)	noexec=1;;

		-r)	rollup_out="$2"; shift
			;;

		-R)	pass_option="-a$pass_option "; set4all; recent_count=$2; shift;;

		-p)	pat="$pat$2+=+"; shift;;		# pattern to further limit output
		-v)	verbose=1; vflag="-v";;

		-\?) usage                          # standard 
			cleanup 1                  # get out - dont execute anything
			;;

		--man)	show_man; cleanup 1;;
		-*)                                 # invalid option - issue usage message 
			error_msg "Unknown option: $1"
			usage
			cleanup 1
		;; 
	esac                         

	shift

done                            # end for each option character

if [[ -z $pat ]]
then
	pat=".*"		# match all in the date range
else
	pat="${pat%+=+}"	# chop last bit of magic separator added by -p
fi

if [[ -n $uflist ]]		# if user supplied a list, make a working copy so we dont trash it
then
	if ! cat $uflist >$flist			
	then
		cleanup 1
	fi
fi
if [[ -n $inlog ]]		# add any log names to the list
then
	for x in $inlog
	do
		echo $x 
	done >>$flist
fi

if (( $recent_count > 0 ))
then
	ng_rm_where -p -n masterlog| awk '{ x=split( $2, a, "/"); print a[x];}' |sort -u | tail -$recent_count >>$flist
fi

# we must compute three sets of timestamps:
#	1) si/ei  integer timestamps used when picking at the log file
#	2) sd/ed  yymmddhhmmss  timestamps used when invoking rollup to give on glog cmd line
#	3) smld/emld  yymmddhhmmss timestamps used to select masterlog files from registered; must back up one week
#		from start, and extend one week from finish to ensure that all sd/ed timestamps are found
#-----------------------------------------------------------------------------------------------------------
if [[ -n "$1" ]]                # a start and maybe end time supplied
then
	si=`ng_dt -i $1`      				# build user supplied start
	sd=$1						# starting date of the search
	smld=$( ng_dt -d $(( $si - $one_week )) )	# starting date of the masterlog file
	if [[ -n "$2" ]]
	then
		ei=$(ng_dt -i $2)    # build end time
		ed=$2
		emld=$( ng_dt -d $(( $ei + $one_week )) )
	else            		        # no ending time given
		if (( $duration > 0 ))
		then
			ei=$(( $si + (10 * $duration) ))
			ed=$( ng_dt -d $ei )
			emld=$( ng_dt -d $(( ($si + (10 * $duration)) + $one_week )) )
		else                     		# no duration either - go forever (or for about 20 years)
			ed=20250203020000
			ei=`ng_dt -i $ed`	
			emld=$ed
		fi
	fi
else			# start date not given, we see if duration must be subtracted from default end to give the start date
	if (( ${duration:-0} > 0 ))
	then
		duration=$(( $duration * 10 ))		# convert to tenths
		typeset - f si=$(( $ei - $duration ))		# start date is 'duration' ago
		sd=$( ng_dt -d $si )
		smld=$( ng_dt -d $(( $si - $one_week )) )
	fi
		
fi  

verbose "sd=$sd smld=$smld  ed=$ed emld=$emld"
if [[ -n $rollup_out ]]			# invoke rollup to drive us across the cluster
then
	if [[ ! -s $flist ]]		# if nothing specific specified, build a list based on dates
	then
		ng_rm_where -p -n masterlog |  awk -v sd=$smld -v ed=$emld '
		{
			x = split( $2, a, "/" );		# basename
			y = split( a[x], b, "." );		# separate masterlog.yyyymmdd000000
			if( b[y]+0 >= sd+0  &&  b[y]+0 <= ed+0 )
				print a[x];
		}
	' |sort -u  >$flist
	fi

	verbose "giving $(wc -l <$flist) files to ng_rollup $(head -1 $flist) - $(tail -1 $flist)"

	if (( ${noexec:-0} > 0 ))
	then
		cat $flist
		cleanup 1
	fi

	ng_rollup $ignore_list $vflag -f $flist -F "cat ==rollup_flist== | xargs sort -u" -m "ng_glog -p '$pat' -L ==rollup_flist== $sd $ed " -P $rollup_out		# start rollup

	cleanup $?
fi

				# not via rollup, suss the listed logs, or the master by default
if [[ ! -s $flist ]]		# default to master if no list of masterlog files provided by either -L or -l options
then
	echo $NG_ROOT/site/log/master >$flist
fi

while read lf
do
	(
		if [[ $fast -gt 0 ]]
		then
			tail -$fast $lf  	# faster, but not always accurate
		else
			cat $lf
		fi
	) | gre "${pat//+=+/|}" | awk -v si=$si -v ei=$ei  '  
		{ 
			if( $1+0 >= si && $1+0 <= ei ) 		# in date/time range
					print 
		}
	' 
done <$flist | sort -T $TMP



cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_glog:Get From Master Log)

&space
&synop	ng_glog [-a] [-d seconds] [-F | -f msgs] [-p pattern] [-r output-file] [[-R n] | start-timestamp [end-timestamp]]

&space
&desc	&This will search some or all of the current master log ($NG_ROOT/site/log/master)
	for messages that fall between the start and end timestamps. If a pattern is given
	using the &lit(-p) option, then output is further limited to records that contain
	matches. 

&space
	Messages that are extracted from the log file(s) searched are written only if they 
	have a timestamp that falls inside of the window defined by the starting and ending 
	times. The start and end times default to five minutes before the current time and 
	end with the current time (the time that &this was invoked). The window can be altered
	using several different combinations of options and parameters:

&space
&beglist
&item	The &lit(-a) (all) option causes the window to be set such that all messages are inside of the window.
	(Useful when supplying a pattern and multiple log files.)
&space
&item	The start time is supplied in &lit(yyyymmddhhmmss) format as the only positional parameter
	on the command line. The ending time is calculated to five minutes past the starting time.
&space
&item	The start time is supplied as the only positional parameter and the duration is defined using 
	the &lit(-d) option. 
&space
&item	Both the start and ending timestamps are given as the two positional parameters on the 
	command line. 
&space
&endlist
&space
&subcat Seaching Distributed Master Logs
	The set of &lit(masterlog) files kept in replication manager space can be easily searched by using the 
	&lit(-r) option. When the &lit(-r) option is used, &this creates a list of the masterlog files 
	to search, and starts an &lit(ng_rollup) process to generate the &ital(output-file) named on the 
	commandline. All of the starting and ending date options apply, as does the use of the pattern. 
	Using &lit(ng_rollup) can require several minutes, and if verbose mode is indicated will be quite 
	chatty on the standard error device.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : All mode.  The start and ending timestamps are set such that all records 
	in the log file are considered to fall within the desired range. 
&space
&term	-d seconds : Supplies a duration in seconds.  Regardless of the number of messages
	that are scanned, only the messages that were written not more than &ital(seconds)
	ago are printed. If a start time is supplied as a command line parameter, then 
	the messages written during &ital(seconds) following the start time are 
	printed. If a start time is not supplied on the command line, then the start time
	is computed to be &ital(seconds) before the current time. 
&space
&term	-f msgs : Fast mode. Rather than searching the whole master log, only the last
	&ital(msgs) are scanned.  The start and ending timestamps may still be supplied
	and will possibly limit the output further. 
	(Fast mode might not print all messages form the 
	window if the number of messages requested does not go back far enough in the
	log. 
&space
&term	-F : Fast mode with a fixed scan of 250,000 log messages. The same inaccuracy 
	possibilities exists with this mode as with -f.
&space
&term	-i node-list : Sets the list of nodes that will not be used to exptract from the log files
	when ng_rollup is invoked.  Use only if the node is known to have a problem processing the 
	requiest(s). 
&space
&term	-l log-name : Supplies the name of the log file to search. More than one of these options
	may be supplied. When supplied, the current master log is not searched unless it is also
	listed with a &lit(-l) option.
&space
&term	-L filename : Supplies the file containing the list of log names to search.  When supplied
	the current master log file is not searched unless included in the list.
	
&space
&term	-p pattern : Each record that falls between the start and ending timetamps will be 
	written to the output only if the record contains the supplied pattern. If this option
	is not given, then all records falling within the start and end timestamps will be 
	written.  If multiple patterns are to be sussed out (e.g. ERROR or WARN) then two 
	&lit(-p) options should be used: -p ERROR -p WARN.  
&space
&term	-r out-file : Uses &lit(ng_rollup) to search through &ital(masterlog) files that are 
	stored in replication manager space, placing any output into the named file. 
&space
&term	-R n : This option indicates that the most recent &lit(n) log files be searched. This 
	option is intended for use with the -r option; results are not guarenteed if -r is not 
	also used. 

&endterms


&space
&parms
	A time window can be indicated by supplying a starting and ending timestamps
	(yyyymmddhhmmss) on the command line.  If only a starting timestamp is supplied
	the messages written to the log after that timestamp are printed using the 
	duration seconds if supplied (-d).

&space
&examp
	Search the current master log for all &lit(ng_shunt) messages that were written between 9:00am
	and 10:00pm on the 30th of April 2006. This assumes that the date range is currently in the 
	master log.
&space
&litblkb
   ng_glog -p ng_shunt 20060430090000 20060430220000 
&litblke
&space

	Search all masterlog files, archived in replication manager space, for ng_shunt messages 
	that occrred during April of 2006
&space
&litblkb
   ng_glog -p ng_shunt -r /tmp/output.apr  20060401000000 20060430235959
&litblke
&space
	Find all error or warning messages generated in the last day:
&litblkb
   ng_glog -p ERROR -p WARN -d 86400
&litblke

&space
&see	&seelink(tool:ng_logd), &seelink(lib:ng_log), &seelink(tool:ng_log),
	 &seelink(tool:ng_log_frag), &seelink(tool:ng_log_comb), &seelink(tool:ng_log_sethoods

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Aug 2003 (sd) : Converted from Gecko (HBD RH).
&term	18 Sep 2006 (sd) : fixed man page.
&term	03 Feb 2007 (sd) : Extended to support log files distributed round the cluster. Added -r
	option to allow rollup processing of distributed masterlog files.  It still will suss out 
	of the current master by default.
&term	06 Mar 2007 (sd) : Corrected the way pattern was passed on rollup command so that special 
		characters would not be parsed by the shell.
&term	16 Apr 2007 (sd) : Added -R option.
&term	12 Jun 2007 (sd) : Made change to allow multiple -p pattern options as logical or patterns to gre.
&term	03 Jan 2007 (sd) : Fixed bug with passing pattern to rollup so that spaces can be included in a pattern.
&term	22 Jun 2009 (sd) : Corrected problem with beg/end dates out of range for ng_dt. 
&term	12 Oce 2009 (sd) : Added ignore option.
&endterms

&scd_end
------------------------------------------------------------------ */
