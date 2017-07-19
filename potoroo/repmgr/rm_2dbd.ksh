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
# Mnemonic:	rm_2dbd
# Abstract:	this is the rm_db interface to the database when it is mantained
#		as an in core table by rm_db_tcp. Requests are sent via TCP/IP to 
#		the database using ng_sendtcp.  This script does all of the necessary
#		validation before sending a file name for addition. 
#		
# Date:		1 April 2006 (Im no fool!)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# build a list commands to send to rmdb_tcp based on md5 file size tripples
# or md5 old-name new-name tripples  read  from stdin.  $1 is the action 
# to put on the front of the command
function build_list
{
	typeset md5=""
	typeset fname=""
	typeset newname=""

	while read md5 fname p3 junk
	do
		echo "$1:$md5:$fname:$p3"  	# newname may not be there and that is ok
	done >$TMP/PID$$.list

	if (( ${send_ping:-0} > 0 ))
	then
		echo "PING::::" >>$TMP/PID$$.list
	fi
}

# -------------------------------------------------------------------
ustring="{-A | -a | -d | -D | -e | -f | -p | -r | -z [-s size]} [-S host] [-P port] [-L | Q] [-W] [-v]"
option=print

status=0
port="${NG_RMDB_PORT:-4360}"
host=localhost
end_mark=":end:"
verbose=0
vflag=""
send_ping=0			# when sending unacknowledged commands we need to send a final ping to get an end of data back 
save_on_err=1			# save stuff on error (we dont if option is fetch)
connection_timeout=120
log=$NG_ROOT/site/log/rm_2dbd
force=0				# set when -Z entered to force an init without a prompt
print_with_ts=""		# added end if print/dump command to force dbd to generate timestamps on output

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	option=add;;
		-d)	option=del;;
		-f)	option=fetch;;
		-p)	option=print;;
		-r)	option=rename;;
		-s)	size=1;;
		-z)	option=init;;

			# these are NOT standard for the rm_db interface but are needed 
		-A)	option=ADD;;
		-C)	connection_timetout=$2; shift;;
		-D)	option=dump; dumpf=$2; shift;;
		-e)	option=exit;;
		-P)	port=$2; shift;;		
		-S)	host=$2; shift;;
		-L)	option=inc_verbose;;
		-Q)	option=dec_verbose;;
		-t)	print_with_ts="wts";;		# added to print/dump command to cause dbd to generate timetstamps in output
		-W)	option=stats;;
		-Z)	force=1; option=init;;

		-v)	verbose=1;;
		-V)	vflag="-v$vflag ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

case $option in
	print)
		# added -h to cause an unexpected disconnect during tcp to generate an error
		echo "print$print_with_ts:" | ng_sendtcp -h -c -T $connection_timeout -t 1800 -e "$end_mark" $vflag $host:$port
		cleanup $?
		;;

	exit)
		echo "xit02031961:::" | ng_sendtcp -t 2 -e "$end_mark" $vflag $host:$port
                echo "verbose:+1::" | ng_sendtcp -t 2 -e "$end_mark" $vflag $host:$port >/dev/null 2>&1

		cleanup $?
		;;


	ADD)	
		send_ping=1				# last rec in input is a ping command so we get a final status
		build_list "ADD" 
		sendtcp_opts="$sendtcp_opts-d "		# send with the option to dump in all data before waiting for response
		;;

	add)	build_list "add" 
		;;

	del)	build_list "del" 
		;;

	fetch)	build_list "fetch" 
		save_on_err=0			# we dont care about saving ouput lists on this stuff if not found
		;;

	rename) build_list "rename" 
		;;

	init)
		# no initialisation is needed for the dbd model, slient exit.
		if ! cd $NG_ROOT/site/rm
		then
			log_msg "unable to switch to $NG_ROOT/site/rm"
			cleanup 1
		fi

		ng_rm_db -C 2 -e >/dev/null 2>&1	# stop the current one; rm_dbd_start will cycle it if it was running
		sleep 2					# let it do its last checkpoint

		ng_log $PRI_INFO  $argv0 "repmgr local database initialised"
		if [[ -f rm_dbd.ckpt ]]
		then
			mv rm_dbd.ckpt rm_dbd.ckpt-
		fi
		touch rm_dbd.ckpt
		rm -f rm_dbd.ckpt.rollup	

		log_msg "local repmgr database has been reset; ng_rm_dbd has been cycled if it was running" 

		cleanup 0
		;;

	# ------------------------- non standard options supported by tcp ----------------------------
	stats)	
	#	echo "PING::::" >>$TMP/PID$$.list
		echo "stats:" >>$TMP/PID$$.list
		verbose=1
		;;
	dump)	
		verbose=1
		echo "Dump$print_with_ts:${dumpf-/badfilename}:" >$TMP/PID$$.list 
		;;

	inc_verbose)
		verbose=1
		save_on_err=0
		echo "verbose:+1:" >$TMP/PID$$.list 
		;;

	dec_verbose)
		verbose=1
		save_on_err=0
		echo "verbose:-1:" >$TMP/PID$$.list 
		;;

	*)	log_msg "internal error: unrecognised option: ${option:-not-defined}"
		cleanup 2
		;;
esac

touch $TMP/output.$$
if ! ng_sendtcp $sendtcp_opts -h -c -T $connection_timeout -t 1800 -e "$end_mark" $vflag $host:$port <$TMP/PID$$.list >$TMP/output.$$
then
	save_on_err=1					# save even if option was fetch
	err_str="sendtcp failed option=$option"
	cat $TMP/output.$$ |awk -v id=$$ '{ printf( "\t[%d] %s\n", id, $0 ); next; }' >>$NG_ROOT/site/log/rm_2dbd
	failed=1
else
	grep -v "status:" $TMP/output.$$ 

	gre -c "status: [1-9]" $TMP/output.$$ | read failed
	gre -1 'status:.[1-9]' $TMP/output.$$|read stuff
	err_str="status !0 found in output: option=$option ($stuff)"
fi

if (( ${failed:-0} > 0 || $verbose > 0 )) 
then
	gre "status: [$(( ! $verbose ))-9]" $TMP/output.$$ >&2
	if (( ${failed:-0} > 0 ))
	then
		if (( save_on_err > 0 ))			# save tracks on failure (we dont need fetch fails)
		then
			echo "$(date) [$$] $err_str" >>$log
			while read fn
			do
				echo "$(date) [$$] $option failed: $fn" >>$log
			done <$TMP/PID$$.list
			log_msg "$failed $option request(s) failed"
		fi

		status=1
	fi
fi

cleanup ${status:-0}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_2dbd:Send request to ng_rm_dbd)

&space
&synop	ng_rm_2dbd {-a | -d | -r | -f | -p | -D dumpfile} [-E] [-C sec] [-S host] [-P port]
&break	ng_rm_2dbd -e [-S host] [-P port]
&break	ng_rm_2dbd {-L | -Q} [-S host] [-P port]

&space
&desc	&This reads data from the standard input, if needed, and formats it into 
	requests to be sent to &lit(ng_rm_dbd) (the local replication database daemon).
	Results from the request are formatted and written to standard output. 

&space
&opts	
	&This suports several command line options which affect the behaviour as it 
	executes.  The majority of these options are compatable with older versions of 
	&lit(ng_rm_db), however there are a few options that are only recognised by &this. 
	It will not be possible to completely avoid using the new options, but if a system
	should resort to the old flavour of &lit(ng_rm_db) (rather than the new &lit(ng_rm_dbd)
	daemon) the new options will cause problems.  To help in understanding 
	which are the new options, the description of options below are 
	divided into two groups.
&space
	Old Options
&begterms
&term	-a : Records read from the standard input device are assumed to be newline terminated
	tripples: md5 value, file pathname, file size.  For each record read, an add transation
	is sent to the database daemon. The file pathname must be fully qualified. 
&space
&term	-d : Records from standard input are assumed to be newline terminated pairs of md5 values
	and filenames. The filenames may be fully qualified pathnames; the path portion will be 
	ignored. Each file will be deleted from the database. 
&space
&term	-f : Records read from the standard input are assumed to be newline terminated pairs 
	of md5 values and filenames.  The resulting database tripple (md5 value, file pathname
	and file size) are written to the standard output for each. 
&space
&term	-r : Records read from the standard input are assumed to be newline terminated tripples
	of md5 value, old filename and new name.  For each record, the file is renamed in the 
	database.  The pathname of both old and new filenames is assumed to be the same as it 
	existed in the database prior to the invocation of &this.
&space 
&term	-p : Print.  This option causes a print transaction to be sent to the database daemon 
	and for all records in the database to be written to the standard output.  The order of
	the records is not predictable.  Each record is a newline terminated record with 
	the md5 value, full file pathname, and file size on each record. 
&space
&term	-z : Initialise the database.  If there is a current database, it is reduced to nothing!
	Use with caution -- you have been warned!
&terms
&space
&endterms

&subcat New Options
&begterms
&term	-C sec : Sets the max connection timeout used when attempting a TCP/IP connection with 
	the rm_dbd process.  If not supplied, 120 seconds is used.
&space
&term	-D filename : Causes the database to be printed and saved in the named file. This is 
	identical to the previous print option except that it avoids sending the results back 
	to the user via the TCP/IP session that is established with the database daemon. This 
	makes taking a backup of the database a bit more efficent. 
&space
&term	-E : Hard error on communication failure.  Normally, if &this is not able to communicate 
	with the daemon, add, delete, rename transactions are saved in a 'failure' file and 
	rolled into the database when the daemon is started.   When a transaction is saved in the 
	failed list,  the exit code from this programme is 0 (good). If this option is set, 
	the transaction is not saved, and the exit code in the case of a communications failure
	is set to 1 to indicate failure.  
&space
&term	-e : Send an exit command to the database daemon. 
&space
&term	-L : Increase the verbosity of the database daemon by one.
&space
&term	-P n : The port number to send the message to. If not supplied, NG_RMDB_PORT is used. 
&space
&term	-Q : Decrease the verbosity of the database daemon by one. 
&space
&term	-S host : The name of the node that the command is sent. If not supplied, then the local
	node is used. 
&space
&term	-W : Send a request for &lit(ng_rm_dbd) to write some current statistics to its standard output 
	device. 
&space
&term	-Z : Same as -z option, except that it skips any confirmation prompting that might occur.
&endterms



&space
&exit	The exit code of &this generally reflects the transaction result as returned by the 
	database daemon.  A zero exit code indicates success.  In the cases where multiple 
	files are operated on. a non-zero code indicates that at least one transaction failed.
	Transation processing is NOT short circuited at the first failure. 

&space
&see	ng_rm_dbd, ng_repmgr, ng_repgmrbt

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Apr 2006 (sd) : Its beginning of time (no fooling). 
&term	10 Apr 2006 (sd) : Added man page. 
&term	04 May 2006 (sd) : Updated man page.
&term	10 May 2006 (sd) : Added -c and -T options to sendtcp call to set a continous connection 
		retry for n seconds. Should prevent reset by peer issues. Added -C option that 
		allows the connection timeout on tcp calls to be set. 
&term	15 May 2006 (sd) : Better error logging to private log.
&term	05 Jun 2006 (sd) : Does not track fetch failures unless they are tcp issues.
&term	23 Jun 2006 (sd) : -z option no longer produces a diagnostic.
&term	04 May 2007 (sd) : -z option now zeros an existing database; -Z does the same thing without a prompt.
&term	12 Sep 2007 (sd) : Added -W option.
&term	13 Sep 2007 (sd) : -z no longer issues an 'are you sure' prompt (you best have been sure now!).
&term	09 Dec 2008 (sd) : Added -h flag to some sendtcp commands to cause an error if the tcp session is 
		unexpectedly disconnected (sendtcp defaults to silence on session disconnect).

&scd_end
------------------------------------------------------------------ */
