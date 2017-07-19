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
# Mnemonic:	ng_log_frag
# Abstract:	save log fragments and nack messages for combination later.
#		1) ask ng_logd to close the current adjunct (frag) file and rename it.
#		2) strip broadcast info from nack messages sort together with 
#			current adjunct file and split into fragment files. 
#		3) verify output to fragments matches input from raw adjunct
#		4) register/hood fragment files if verification ok.
#	
#		if we detect a failure (output seems not to be complete), then we 
#		copy the raw fragment messages and the raw nack messages into the 
#		current fragment file. They will then be picked up with the next
#		execution. Copies of the files (raw, nack, and a a single file with 
#		the sorted spilt record) is saved in TMP using a log_frag prefix. 
#		
#		The awk that splits the adjunct file into fragments assumes that
#		there will not be more than two or three output fragments per 
#		input file. Given that a fragment file should only be an hour old
#		this seems reasonable. If a fragment file contains more output 
#		files than the max number of concurrently allowed open FDs, then 
#		the awk will fail. 
#		
# Date:		
# Author:	Andrew Hume   (Original)
#		Scott Daniels (Rewrite 1/22/07)
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function trash_files
{
	rm -f $cyc_frag $raw_frag $nack_log $sorted_nack		# these may not be in tmp and thus do not go poof on cleanup
}


# check the file name. if the last bit is 000000 rather than yymmdd000000 then we 
# likey have bad stuff in the adjunct file. if we see this, we save the adjunct and 
# issue an alert.
function ck_fname
{
	typeset fname=$1
	typeset	savef=$TMP/log_comb/corrupt_adj.$(ng_dt -d)
	typeset host=$(ng_sysname)

	if [[ ${f##*.} == "000000!" || ${f##*.} == "corrupt!" ]]
	then
						# force to log_comb alert receivers
		if (( $forreal > 0 ))
		then
			cp $cyc_frag $raw_frag $savef
			ng_log $PRI_ALERT $argv0 "@NG_ALERT_NN_ng_log_comb  adjunct may have corrupted records. copy saved $host:$savef"
		fi
		log_msg "adjunct may have corrupted messages; saved: $savef [WARN]"
	else
		verbose "split file name seems ok: ${fname##*.} [OK]"
	fi
}

# signal logd to roll over the fragment file; wait for it. once found 
# move off the nack file if there
function roll_frag
{
	verbose "sending roll request to ng_logd"
	echo "%roll $raw_frag;" | ng_sendudp -t 120 -r "+ok" localhost:$NG_LOGGER_PORT	# have ng_logd roll the log fragment to our alternate name

				# once sendudp returns it should be there, but if its not we 
	tries=30			# wait 10 more minutes and then we give up
	verbose "waiting for the adjunct file $raw_frag to appear"
	while [[ ! -f $raw_frag ]]
	do
		if (( $tries < 1 ))
		then
			log_msg "giving up."
			ng_log $PRI_ERROR $0 "could not find closed adjunct file: $raw_frag; giving up"
			trash_files
			cleanup 2
		fi
	
		if (( $tries == 10 ))			# with 10 tries left, resend the command -- maybe first was lost
		then
			log_msg "resending roll command to log daemon, environment info follows:"
			uptime >&2
			ng_ps|grep ${NG_PROD_ID:-ningaui} >&2
			log_msg "----------- end of env info ----------------"
			echo "%roll $raw_frag;" | ng_sendudp -t 120 -r "+ok" local_host:$NG_LOGGER_PORT	
		fi
	
		tries=$(( $tries - 1 ))
		log_msg "unable to find closed log adjunct $raw_frag; pausing a few seconds ($tries attempts remain)"
		sleep 20
	done

	verbose "found closed adjunct file [OK]"
	# do this after we see the adjucnt file rolled to our work filename
	if [ -s $LOG_NACKLOG ]				# capture current set of nack messages
	then
		mv $LOG_NACKLOG $nack_log
	fi

	return 0
}

# -------------------------------------------------------------------
ustring="[-l] [-n] [-p ml-prefix] [-P frag-prefix] [-r] [-t] [-v]"
frag_prefix="mlog"		# prefix for fragment file names registered; allows for testing
mlprefix=masterlog.${NG_CLUSTER:-bad_cluster}		# prefix for masterlog files and hoods
dorecycle=1			# ok to process recycle files (-l turns off -- live frag only)
dofrag=1			# do the live fragment (-r turns off -- recycle only)
forreal=1 
copymode=0			# -c sets and causes a copy of the live fragment stuff

while [[ $1 == -* ]]
do
	case $1 in 
		-c)	copymode=1;;
		-l)	dorecycle=0;;			# live fragment only; turn off do cycle
		-n)	copymode=1; forreal=0;;
		-p)	mlprefix=$2; shift;;
		-P)	frag_prefix=$2; shift;;		# please keep consistant with log_comb -P and -p options
		-r)	dofrag=0;;			# recycle only -- turn off do frag

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

node=$(ng_sysname)
id=$(ng_dt -i)			# used in the file name: <frag-prefix>.<node>.<id>.<masterlog-date>
rmd=$(ng_spaceman_nm)		# directory in repmgr land for ALL of our output fragment files

raw_frag=$LOGGER_ADJUNCT.$$		# raw fragment file, MUST be on same filesystem as original file to avoid odd truncation issues!
cyc_frag=$TMP/log_frag_rec.$$		# stuff from recycle file(s)
sorted_nack=$TMP/snack.$$		# sort nack messages to here
nack_log=$LOG_NACKLOG.hold.$$		# move the live nacklog to here

touch $nack_log				# ok for these to be empty, but they must exist to prevent barfing later
touch $sorted_nack			
touch $cyc_frag

cd $NG_ROOT/site/log

# -------------------------- get a 'working copy' of the fragment file ------------------------------------
if (( $forreal <= 0 ))				# we stay out of repmgr space if in not-really mode
then
	rmd=$TMP/${USER:-nobody}
	if ! mkdir -p $rmd
	then
		log_msg "cannot create a working directory: $rmd"
		cleanup 1
	fi
	verbose "no-exec mode: fragment files will be created in: $rmd"
fi

# if there are any odd master.frag.pid files still hanging about, convert them to recycle files for later pickup
ls master.frag.[0-9]* 2>/dev/null | while read x		
do
	if (( $forreal > 0 ))
	then
		log_msg "converting previous frag file to recycle: $x"
		mv $x log_frag.$x.rec			# convert them to recycle files to pick up later
	else
		log_msg "no-exec mode: would convert previous frag file to recycle: $x"
	fi
done

touch $raw_frag			# dont create raw frag until after we look for old ones above

if (( $copymode > 0 && $dofrag > 0 ))			# -t or -t -t will turn this on. 
then
	verbose "copy mode: copying $nack_log and $raw_frag"			# copy things rather than moving them away
	if [[ -s $LOG_NACKLOG ]]
	then
		cp $LOG_NACKLOG $nack_log
	fi
	cp $LOGGER_ADJUNCT $raw_frag

	cd $TMP
	
	verbose "copymode: raw_frag = $(ls -al $raw_frag 2>&1)"
	verbose "copymode: nac_log = $(ls -al $nack_log 2>&1)"
else
	if (( $dofrag > 0 ))			# not copymode, but only if ok to do live fragment; -r (recyc only) turns this off
	then
		roll_frag			# have ng_logd roll over the current fragment; wait for it

		if [ -f $nack_log ]		# add in any nack'd messages stripping off seq# logname;
		then
			awk '
				/^[0-9][0-9][0-9][0-9] MASTER_LOG; / {		# for each msg that was destined for the master log
					x = index( $0, ";" );  			# strip just the logd header
					print( substr( $0, x+2 ) ); 
				}' <$nack_log |sort -u >$sorted_nack	 	# must capture soreted nack for verification
		fi
	else
		verbose "live fragment skipped -- recycle only mode"
	fi
fi

# pickup recycle files if allowed
if (( $dorecycle > 0 ))		# -l (live frag only) will turn this off
then
	cd $NG_ROOT/site/log
	mv_list=""						# list of files to move for backup after we create big list
	verbose "combining recycle files into $cyc_frag"
	ls log_frag.*.rec 2>/dev/null | while read f
	do
		verbose "recylcing $f"
		if ! cat $f
		then
			log_msg "unable to combine recycle files"
			trash_files
			cleanup 3
		fi

		mv_list="${mv_list}$f "
	done >$cyc_frag
		
	if [[ ! -s $cyc_frag ]]
	then
		verbose "nothing to recycle [OK]"
	else
		sort -u  ${mv_list:-junkfile} | ng_md5 |read omd5 junk		# verify that the single file has everything
		sort -u $cyc_frag | ng_md5 | read cfmd5 junk
		if [[ $cfmd5 != $omd5 ]]
		then
			log_msg "abort: unable to create a single recycle file $omd5(expect) != $cfmd5(got) [FAIL]"
			trash_files
			cleanup 3
		else
			verbose "combined recycle file verified [OK]"
		fi
	
		if [[ -n $mv_list ]] && (( $copymode <= 0 ))
		then
			mv -f $mv_list $TMP/log_comb/		# we let them stack here for backup and tmp cleaner will zap in 2 weeks
		else
			verbose "copy mode: recycle files left in place [OK]"
		fi
	fi

	verbose "$(ls -al $cyc_frag) "
fi


verbose "setup is complete, splitting messages [OK]"

if [[ ! -s $raw_frag && ! -s $sorted_nack && ! -s $cyc_frag ]]		# if all three are empty, bail now
then
	verbose "nothing to do [OK]"
	trash_files
	cleanup 0
fi

(					# spit a unique set of messages and split them
	cat $sorted_nack
	sort -u $raw_frag 
	sort -u $cyc_frag
)| awk 	-v id="$id" \
	-v node="$node" \
	-v rmd="$rmd" \
	-v verbose=$verbose \
	-v frag_prefix=$frag_prefix \
	'

	function cmd( c,	result )
	{
		result = "";
		c|getline result;
		close( c );
		return result;
	}

	# convert a date in mdy format to the day of week (sun==0)
	function mdy2dow( date, 	y, m, d, y19, d19, ld, ld19 )
	{
		y = substr( date, 1, 4 ) + 0;
		m = substr( date, 5, 2 ) + 0;
		d = substr( date, 7, 2 ) + substr( soup, (m*3)-2, 3 );	# sum of days before today + days to the ts day

		y19 = y - 1900;						# years since 1900, before date year
		d19 = y19 * 365;					# normal days since 1900, but before date year
		ld19 = int((y19-1)/4);					# leap days since 1900, not including date year 

		if( (ld = m > 2 ? ( y % 4 == 0 ? 1 : 0) : 0) )		# possible leap day this year?  count only if we are march or beyond
			if( (y % 100 == 0) && (y % 400) )		# century years must also be divisible by 400 to be a leap year
				ld = 0;					# if it is not, then reset ld count

		#printf( "date=%s dow=%d    %d %d %d(y,m,d) %d %d %d(y19,d19,ld19) %d(ld)\n", date, (d19 + ld19 + ld + d) % 7, y, m, d, y19, d19, ld19, ld );
		return (d19 + ld19 + ld + d) % 7;
	}


	# given the timestamp ts, return a filename that is the output prefix with the masterlog date into 
	# which a message with the given timestamp will be placed (previous Monday)
	# the file name is given a trailing bang so that repmgr tools leave it be; until we register it!
	function mkname( ts,	date, m, x )
	{
		x = ts % 864000;
		m = ts - x; 							# midnight of the timestamp day

		if( m < 315360000 || length( sprintf( "%.0f", m ) ) < 10 )	# 1995/01/01 00:00:00 -- anything before this is junk
		{
			if( displayed++ < 10 )
				printf( "bad timestamp: %s \n", $0 ) >"/dev/fd/2";
			return sprintf( "%s.%s!", ofprefix, "corrupt" );	# collect junk in one place
		}

		if( ! suffix[m] )						# only if not already built...
		{								# the system calls make this code expensive; try to limit
			date = substr( cmd( sprintf( "ng_dt -d %.0f", m ) ), 1, 8 );		# get just yymmdd  from the timestamp value
			dow = mdy2dow( date );
	
			if( dow )	
				dow--;		# number of days to back up to previos monday
			else
				dow = 6;	# sunday is a special case
			fdate = m - (dow * 864000);							# the date of the previous monday
			suffix[m] = substr( cmd( sprintf( "ng_dt -d %.0f", fdate ) ), 1, 8 );		# get just yymmdd  from the timestamp value
		}

		return sprintf( "%s.%s000000!", ofprefix, suffix[m] );	# see begin for setup of oprefix
	}

	BEGIN {
	     	       #janfebmaraprmayjunjulaugsepoctnovdec
		soup = "000031059090120151181212243273304334";	# sum of days in previous months in the year (supports mdy2dow())

		ofprefix = sprintf( "%s/%s.%s.%s", rmd, frag_prefix, node, id );	# prefix for each output file 
	}

	{					# split log records by time stamp into a weekly oriented fragment file
		fn = mkname( $1+0 );
		count[fn]++;
		printf( "%s\n", $0 ) >>fn; 
	}

	END {
		for( x in count )
		{
			close( x );
			if( verbose )
				printf( "log_frag: split target: %s  %5d records\n", x, count[x] ) >"/dev/fd/2";
			list = list x " ";
		}

		printf( "%s\n", list );
	}
' | read list

# ----------------------------------------- verify split files match the raw input ----------------------
sort -u $list | ng_md5 |read split_md5 junk
sort -u $raw_frag $sorted_nack $cyc_frag | ng_md5 | read raw_md5 junk

if [[ "$split_md5" != "$raw_md5" ]]
then
	log_msg "md5s for raw and split do not match:  $split_md5(split) != $raw_md5(raw) [FAIL]"

	if (( $forreal > 0 ))		# if not in test mode, put fragment stuff back and save a copy
	then
		rfile=$NG_ROOT/site/log/log_frag.$(ng_dt -i).$$.rec
		log_msg "placing log messages into a recycle file: $rfile"
		ng_log $PRI_WARN $argv0 "unable to verify fragment files after split, created recycle file: $rfile"
		cat $raw_frag >$rfile
		cat $sorted_nack >>$rfile
		cat $cyc_frag >>$rfile

		cp $raw_frag $TMP/log_comb/log_frag.raw.$$
		cp $sorted_nack $TMP/log_comb/log_frag.nack.$$
		cp $cyc_frag $TMP/log_comb/log_frag_rec.$$
		sort $list >$TMP/log_comb/log_frag.split.$$

		cat $list| xargs rm  -fr 		# ditch the files that were bad
	else
		log_msg "no-exec mode: skipped putting fragment records back into current file [OK]"
	fi

	trash_files
	cleanup -k 2
fi


verbose "split output seems sane [OK]"

typeset -u hood_prefix=${mlprefix%.*}		# hood must be in upper case (repmgr requirement) -- must lose the cluster name
for f in $list
do
	ck_fname $f				# if we get something like mlog.node.xxxx.000000  then we may have bad recs in frag; send notice

	hood="${hood_prefix}_${f##*.}"		# hood based on the log date
	hood="${hood%!}"			# strip the dingo and add 'midnight'
	verbose "hood=$hood"
	rfile=$NG_ROOT/site/log/log_frag.$(ng_dt -i).$$.rec	# recycle file if we have issues registering one or more split files

	if (( $forreal > 0 ))
	then
		pub_fname=$(publish_file -e $f)			# undiddle the file name (spaceman marker character on end etc)
		f=$pub_fname
		if ! ng_rm_register -v -H $hood $f		# if registration fails, add just those records back and trash the file
		then
			log_msg "unable to register fragment file: $f ($hood) msgs saved in recycle file: $rfile [FAIL]"
			ng_log $PRI_WARN $argv0 "unable to register fragment file: $f ($hood) saved messages in recycle file: $rfile"

			cat $f >>$rfile				# save data for recycle
			# do NOT remove $f. we leave it for garbage collection (fix-mlog-unattached should take care of it)
			# we dont know the state of what rm-register did (rm-db registration etc) so we cannot just lop it off
		else
			verbose "register file: $f hood=$hood [OK]"
		fi
	else
		verbose "no-exec mode: NOT registered: $f hood=$hood [OK]"
	fi
done
		
	
trash_files		# clean up things that are not in $TMP or /tmp
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log_frag:Save off a log fragment)

&space
&synop	ng_log_frag [-c] [-l] [-n] [-p ml-prefix] [-P frag-prefix] [-r] [-v]

&space
&desc	&This is executed on a periodic basis to capture the log fragment and &ital(nack)
	files and save them in replication manager space.   
	The message are split into files according to the combined master log name that 
	the message will be placed in.  The files, are hooded to the appropriate node
	where the &lit(ng_log_comb) process will combine them with the other fragments 
	and add them to the complete master log file. 
&space
	In additon to the 'live' fragment and nack files, &this will pick up any recycle 
	files in teh &lit(NG_ROOT/site/log) directory that have the filename syntax:
	&lit(log_frag.*.rec.)  In the event of a failure creating and/or registering any 
	of the split files, the affected log messages are written to a recycle file to 
	be processed later. 

&space
&subcat	Corrupted Records
	On occasion the fragment file has corrupted log messages; messages whose timestamp
	is missing or invalid.  The original version of &lit(ng_log_comb) collected these 
	into various files based on what bits of a timestamp translated into a date -- usually 
	a file with a 1994 date. Corrupted timestamps are now detected by &this and sorted 
	into a fragment file that has a single masterlog target (masterlog.corrupt). This should
	make it easy to detect and analyse corrupted records. 

.if [ 0 ]
NOTE -- not for the man pages, but good for the soul --
Corruptions have been introduced by various causes, and all known causes have been corrected. 
The causes to date were:
	* the log-frag process snarfing all of a message to the LAST semicolon treating it
	as the header. 
	* ng_logd adding an extra byte of data when emptying its cached messages after it had
	been paused. 
	* adding messages into the fragment file by a process other than ng_logd while ng_logd
	was running.

While these have been fixed, it is still possible to have corrupted messages. The most likely 
cause, with no fix possible, is a node crash. This is likely to leave crap in the fragment file.  
.fi
	
&opts	These options are supported:
&smterms
&term	-c : Copy mode.  Fragement and recycle files are copied and not removed after processing.
	Mostly this is for testing. This mode is assumed when &lit(-n) is used, but does not 
	imply no-execution mode on its own.
&space
&term	-l : Live fragment, and nack, files only. Recycle files are not processed. 
&space
&term	-n : No execution mode.  Causes &this to go through the motions, but not to do anything that 
	is perminant. 
&space
&term	-p ml-prefix : The prefix of masterlog file names. If not supplied, &lit(masterlog) is assumed.
&space
&term	-P frag-prefix : The prefix of fragment files. If not supplied &lit(mlog) is assumed.
&space
&term	-r : Recycle only mode. Only recycle files are processed. Live fragment and nack files are ignored.
&space
&term	-v : Verbose mode. 
&endterms

&space
&exit	An exit code that is not zero indicates an error. If an error is detected, the fragment 
	and nack information is placed into the current fragment file. 

&space
&see	&ital(ng_logd,)
	&ital(ng_log,)
	&ital(ng_log_comb,)
	&ital(ng_log_sethoods)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 Jun 2003 (sd) : To convert spaceman call to the new format (fname only)
&term	28 Jan 2004 (sd) : To change LOG_NACK_LOG refrerences to LOG_NACKLOG.
&term	18 May 2004 (sd) : Added SCD, and reorderd the close command sent to ng_logd to prevent
		collisions in the adjunct file (partial messages that cause log_comb to fail).
&term	18 Jun 2004 (sd) : To ensure that move is done to a local file system before we send 
		the close to ng_logd. Once the close is sent, then we can move it to repmgr space, but 
		we need to rename it before the close. Timing is everything.
&term 	15 Aug 2004 (sd) : we now pause after sending the close to allow the logd process some 
		time to finish writing any queued messages that are ahead of the close command.
&term	04 Jul 2005 (sd) : Added a bit of debugging, and a resend of the roll command to try to 
		eliminate the 'could not find...' errors. (The problem seemed to be dropped %roll messages
		and a resend during its timeout period seems to have worked -- checking code in with 
		this change).
&term	08 Aug 2004 (sd) : Nack messages still have sequence number and log name id on the head. 
		this must be removed before putting out.
&term	06 Feb 2006 (sd) : Added some environmental capture if we have to resend the roll command.
&term	20 Oct 2006 (sd) : Corrected problem with nack log parse that was causing corrupted log messages
		in the combined files. 
&term	22 Jan 2007 (sd) : Rewrote to add the split funciton for distributed log comb support. 
&term	14 Feb 2007 (sd) : Added check for fragment file name that indicates corrupt log messages in the 
		adjunct file. It saves the file and writes an alert message to the master log.
&term	21 Feb 2007 (sd) : Fixed alert nickname list.
&term	22 Feb 2007 (sd) : Added ability to do recycle files and we now put corrupt records into a single
		fragment file *.corrupt so that they all collect together.
&term	26 Feb 2007 (sd) : Ensured tmp files in site are removed.
&term	05 Mar 2007 (sd) : Corrected the name of the recycle file (it left out site/log). Also ensured cleanup
		of tmp files in site on failure. 
&term	08 Mar 2007 (sd) : Tweeked rm-register failure handling so that it does not remove the file from repmgr space.
&term	06 Sep 2007 (sd) : Changed to pick up stranded mlog.frag.$$ files from previous runs (assumed failures).
		These are converted to recycle files and incorporated normally into the mix.
&term	15 Oct 2007 (sd) : Modified to use NG_PROD_ID rather than ningaui.
&term	20 Mar 2009 (sd) : Added cluster name to the filename in the repmgr area. 
&endterms

&scd_end
------------------------------------------------------------------ */
