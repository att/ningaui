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
#    #! will be inserted automatically by ng_install
#
# Mnemonic:	ng_ckpt_prep
# Abstract:	searches for and creates a recovery file in preparation for 
#		the use of a checkpoint file. assumes two record types are in 
#		the master log:
#			recovery - data needed to roll forward from ckpt file
#			ckpt     -  contains name of ckpt file ($NF)
#
#		We still output tumbler data based on the most recent checkpoint 
#		filename found in the master log, but in most cases users 
#		should ignore this and use the NG_XX_TUMBLER data as that is 
#		the definative value.
#		
# Date:		08 Mar 2003
# Author: 	E. Scott Daniels
# -------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# ----------------------------------------------------------------------------


# old method -- deprecated, but kept to provide back compatability while we convert
# older versions of things.  This will be invoked if -M is not present on the command 
# line.  
function find_it
{
	$trace_on 

	if [[ $trace -gt 0 ]]
	then
		set -x
	fi

	ckptseed="0,0"

	# make a backwards pass through the log.  we save all status messages until we find a 
	# checkpoint file that we can both find and read. The status messages are then sorted
	# into chronological order and de-duped and become the recovery file for the caller to use.

	verbose "searching local repmgr db for $ckpt_filepat(ckpt-filepat) files"
	ng_rm_db -p | gre "${ckpt_filepat:-no_pattern}" | awk '{print $2}' |xargs ng_stat -m -n | sort -k 1nr,1 >/tmp/list.$$	# to file for easy debug

	verbose "found $(wc -l </tmp/list.$$) checkpoint files on this host"
	if (( $verbose  > 2 ))
	then
		echo "------------ list.$$ -----------" >&2
		cat /tmp/list.$$ >&2
		echo "--------------------------------" >&2
	fi

	if [[ ! -s /tmp/list.$$ ]]
	then
		log_msg "abort: no checkpoint files in rmdb -- is rmdb down?"
		echo "/no-cpt-flies-found"			# bad name should cause problems elsewhere
		cleanup 1
	fi

	verbose "searching $mlog for $recovery_key(reco-key) $ckpt_key(ckpt-key) ckpt file information..."
	>$rfile					# must exist even if its empty

	tac $mlog  2>/dev/null | awk \
		-v recovery_key="$recovery_key" \
		-v ckpt_key="$ckpt_key" \
		-v snarf_all=$snarf_all \
		-v snarf_ts=$snarf_ts \
		-v rfile=$rfile \
		-v full_target=$target \
		-v target="${target##*/}" \
		-v ckptlist=/tmp/list.$$ \
		-v fname_field=$fname_field \
		-v verbose=$verbose \
		'
		BEGIN {
			if( verbose > 1 )
				printf( "recovery key=%s\n", recovery_key )>"/dev/fd/2";

			high = 9999;			# tumbler values (low and high frequency)
			low = 9999;			# if weve never made one, these should force appl to roll to beginning

			found_hi = found_low = -1;	# must search until we find a checkpoint file of each kind
			need_data = 1;			# need to snarf data until we find a ckpt file we can read

			rc = 1;				# assume bad exit code - not found
			if( target )
				ckptfile = full_target;		# user supplied a chkpt filename that we want roll back data up to
			else
				ckptfile = "nosuchfile";	# no targe, bogus value if we find nothing

			count = 0;
			while( (getline <ckptlist) > 0 )		# where we expect to find the file on this host
			{						# records are mod-time pathname, sorted by mtime
				count++;
				f = basename($2);
				if( ! ckptpath[f] )			# save the first one encountered -- most recent
					ckptpath[f] = $2;
			}

			if( verbose )
				printf( "ng_ckpt_prep: %d ckpt files live on this host\n", count )>"/dev/fd/2";
			close( ckptlist );
		}

		function snarf_data( key, 		i )	# snarf data from a status record 
		{
			if( snarf_all )				# save entire message?
				printf( "%s\n", $0 )>rfile;
			else
			{
				if( snarf_ts )
					printf( "%s ", $1 )>rfile;			# short, but keep timestamp 
				for( i = 1; i <= NF && $(i) != key; i++ );		# skip until recognised keyword
				for( i++; i <= NF; i++ )				# snatch all after that
					printf( "%s ", $(i) )>rfile;
				printf( "\n" )>rfile;
			}
		}
	
		function basename( n,  a, x )
		{
			x = split( n, a, "/" );
			return a[x];
		}

		{	
			if( fname_field == 0 )
				fname_field = NF;

			if( need_data && match( $0, recovery_key )  )	# recovery record?
			{
				snarf_data( recovery_key ); 
				next; 
			}
			else
			if( match( $0, ckpt_key ) )
			{
				ckptfile_base = basename( $(fname_field) );	

				x = split( $(fname_field), b, "." );			# split nodes (/path/stuff/what.a000.cpt)
				if( found_hi < 0 && substr( b[2], 1, 1 ) == "a" )		# not found a high value yet
					found_hi = high = substr( b[2], 2 ) + 0;
				if( found_low < 0 && substr( b[2], 1, 1 ) == "b" )
					found_low = low = substr( b[2], 2 ) + 0;
		
				if( need_data && target == "" || ckptfile_base == target )	# reset if we have not found the target 
				{
					if( ckptpath[ckptfile_base] )
					{
						if( system( "test -r " ckptpath[ckptfile_base] ) == 0 )	
						{					 # its there and it can be read 
							close( rfile );
							ckptfile = ckptpath[ckptfile_base];		# full path to the stars
							rc = 0;
							need_data = 0;					# dont need data
						}
						else
						{
							if( verbose )
							{
								printf( "\tckpt in file, not readable on node: %s\n", ckptpath[ckptfile_base] ) >"/dev/fd/2"; 
							}
						}
					}
					else
						printf( "\tckpt in file, not on node: %s (%s)\n", ckptfile_base, $0 ) >"/dev/fd/2"; 
				}
	
	
				if( !need_data && found_hi >= 0 && found_low >= 0 )	# all the info we could want, we have
					exit( 0 );					# spit it out and be done!
			}
		}

		END { 
			if( found_hi < 0 || found_low < 0 )
				printf( "WARNING: did not find %s checkpoint filename reference in log to generate tubmler data\n", found_hi >= 0 ? "low freq(a)" : "high freq(b)" ) >"/dev/fd/2";
			printf( "%s %d,%d %d\n", ckptfile, low, high, rc ); 
			exit( rc );
		}
	' | read ckptfname ckptseed rc

	sort -u <$rfile >$rsfile		# sort the roll up file removing dups

	if [[ $ckptfname = "nosuchfile" ]]			
	then
		verbose "WARNING: checkpoint info for recovery-key($recovery_key) ckpt-key=($ckpt_key) not found in master log"
		ckptfname=""
	fi

	if (( $verbose > 0 ))
	then
		verbose "recovery file contains $(wc -l <$rsfile) records"
	fi
	verbose "log search done. (${ckptfname:-no-ckpt-name}, $ckptseed)"

	return $rc				# send back the search result code
}

# search for the most recent, or target, ckpt file on this node. the new technique assumes that the 
# ckpt file's md5 value is now put into the log message. this allows us to match the path and md5
# to the rmdbd information in order to select the most recent checkpoint file on the node. 
# this method, unlike the old method, should not have issues if a filesystem is unmounted, or a 
# checkpoint file is removed physically but left in the rmdb.  
#
# logic:
# 1) generate a 'mini masterlog' with just the stuff we need ($1 and $2 from commandline); sorted
# 2) generate a list of all files matching the ckpt filename pattern ($3 on commandline) from rmdb
# 3) search the masterlog successful checkpoint messages, newest to oldest, for a path md5 combo 
#	that matches rmdb info. we select the first we find as the target. 
# 4) using the timestamp on the master log record of the selected file, pass through the 
#	mini masterlog and put all recovery related entries written after the selected timestamp
#	into the roll forward file. 
#
# if target is set, we assume the file pattern key was a full path or filename and not a pattern
# (given with the -t option to the script). In this case, there should be a small few (we hope 
# just one) that match the target. in this case, there will be lots of verbose messages indicating
# that more recent ckpt files were skipped
#
function find_it_new
{
	get_tmp_nm smlog rmdbinfo | read smlog rmdbinfo
	verbose "searching $mlog for $recovery_key(reco-key) $ckpt_key(ckpt-key)"
	# extract just what we need and sort it. must sort twice to force timestamp order that is not based on stuff in [...]
	# if we sort -u -k 1n,1 we drop duplicate timestamps!
	gre "$recovery_key|$ckpt_key" $mlog | sort -u |sort -k 1n,1 >$smlog	

	verbose "searching rmdb for ${ckpt_filepat:-no-pattern}(file-pat)"	# filepat is filename if a specific target is specified
	ng_rm_db -p | gre "${ckpt_filepat:-no_pattern}" >$rmdbinfo		# extract the ckpt files that live on this node
	verbose "$(wc -l <$rmdbinfo) checkpoint files with pattern ${ckpt_filepat} were listed in rmdb"

	verbose "searching for most recent checkpoint file"
	gre "$ckpt_key" $smlog | sort -k 1nr,1 2>/dev/null | awk \
	-v rmdbf=$rmdbinfo \
	-v fname_field=$fname_field \
	-v md5_field=$md5_field \
	-v verbose=$verbose \
	-v target="$target" \
	-v force=$force \
	'
		function base( n, 	a, x )
		{
			x = split( n, a, "/" );
			return a[x];
		}

		BEGIN {
			while( (getline<rmdbf) > 0 )			# map each full path in dbd list by md5
			{
				if( dbname[$1] != "" )			# shouldnt have dup md5, but warn if we do
					printf( "\tWARNING: duplicate md5 values: %s %s %s\n",  $1, dbname[$1], $2 ) >"/dev/fd/2";
				dbname[$1] = $2;			# index to full path via md5 value
				#printf( "\tlogging m5=%s fname=%s %s\n", $1, dbname[$1], $2 ) >"/dev/fd/2";
			}
			close( rmdbf );

			if( target )
				printf( "\tsearching for a user supplied specific target file: %s\n", target )>"/dev/fd/2";
		}

		{
			fname = base($(fname_field));		
			m5 = $(md5_field);			# md5 as recorded by programme
			dname = dbname[m5];			# find full path as was recored by rm_dbd

			#printf( "\tchecking m5=%s fname=%s dname=%s\n", m5, fname, dname ) >"/dev/fd/2";
			if( dname != "" && (m5 != "" || force) )			# we have a hit on this md5 value in repmgr db
			{
				# we might check to see that filenames match, but the odds of dup md5 values are slim so we dont
				if( system( "test -r " dname ) == 0 )			# and file is readable
				{
					select = dname;
					ts = $1 + 0;					# save log timestamp for message
					if( verbose )
						printf( "\tselecting %s %s ts=%s\n", select, m5, $1 ) >"/dev/fd/2";
					if( verbose > 1 )
						printf( "\t%s\n", $0 ) >"/dev/fd/2";
					exit( 0 );
				}
				else
				{
					if( verbose  )
						printf( "\nckpt not readable: %s %s\n", dname, m5 ) >"/dev/fd/2";
				}
			}
			else
			{
				if( verbose )
				{
					if( target )
						printf( "\tmasterlog entry did not match user supplied target: %s\n", fname ) >"/dev/fd/2";
					else
						if( verbose )
							printf( "\tnot on node: %s %s %s\n", fname, m5, $1 ) >"/dev/fd/2";
				}
			}
		}

		END {
			if( select )
			{
				printf( "%.0f %s\n", ts, select );
				exit( 0 );
			}

			printf( "\tunable to find a checkpoint file on the node [FAIL]\n" ) >"/dev/fd/2";
			exit( 1 );
		}
	' | read ts cptfname

	if [[ -n $ts && -f ${cptfname:-no-such-file} ]]
	then					# build the roll forward file
		rc=0
		verbose "selected file: $(ls -l $cptfname)"
		
		verbose "building roll forward file: $rfile"
		>$rfile
		gre "$recovery_key" $smlog | awk \
		-v start_ts=$ts \
		-v snarf_all=$snarf_all \
		-v snarf_ts=$snarf_ts \
		-v rfile=$rfile \
		-v recovery_key="$recovery_key" \
		'

		$1+0 < start_ts { next; }		# skip anything before 
		{
			if( snarf_all )				# save entire message?
				printf( "%s\n", $0 )>rfile;
			else
			{
				if( snarf_ts )
					printf( "%s ", $1 )>rfile;			# short, but keep timestamp 

				for( i = 1; i <= NF && $(i) != recovery_key; i++ );	# skip until recognised keyword
				for( i++; i <= NF; i++ )				# snatch all after that
					printf( "%s ", $(i) )>rfile;

				printf( "\n" )>rfile;
			}
		}


		'
		verbose "roll forward file contained $(wc -l <$rfile) records"
	else
		rc=1
	fi

	echo ${cptfname:-missing}		# output to caller
	return $rc
}

# -----------------------------------------------------
logd=$NG_ROOT/site/log
TMP=${TMP:-/tmp}


# ------------- initialise and parse the command line -----------------
ustring="[--man] [-m logname] [-r recovery-file] [-{s|S}] [-v] -f filename-field -M md5-field recovery_key ckpt_key ckpt_fname_pat"
mlog="$NG_ROOT/site/log/master"

thishost=`ng_sysname -s`
verbose=0
#rfile=$TMP/rfile.$$ 			# rcovery file -- unsorted w/dups (trashed before exit)
rfile=$TMP/$LOGNAME.recovery		# sorted recovery file (kept on exit) -r overrides
snarf_all=1				# causes entire master log record to be snarfed
snarf_ts=0				# snarf short record, but with timestamp
target=""				# user supplied target file
fname_field=0				# file name in message -- number of fields back from end
md5_field=0
force=0
use_old_way=1				# old way of computing to provide backwards compatablity; -M turns it off

while [[ $1 = -* ]]
do

	case $1 in 
		-F)	force=1;;
		-f)	fname_field=$2; shift;;
		-M)	md5_field=$2; use_old_way=0; shift;;
		-m)	mlog=$2; shift;;
		-r)	rfile="$2"; shift;;			# user supplied recovery file
		-s)	snarf_all=0;;				# capture only a portion of the master log record
		-S)	snarf_all=0; snarf_ts=1;;		# short record, but save timestamp
		-t)	target=$2; shift;;			# target chkpt file  -- we assume its a full path, but it could be relative
		-v)	verbose=$(( $verbose + 1)); vflag="-v $vflag" ;;

		--man) show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	usage; cleanup 1;;
	esac

	shift
done

if [[ $# -lt 3 ]]
then
	log_msg "ckpt_prep: missing parameters; expected 3, got $*"
	usage
	cleanup 1
fi

recovery_key=$1
ckpt_key=$2
ckpt_filepat=$3

if [[ -n $target ]]
then
	verbose "specific target checkpoint file requested: $target"
	ckpt_filepat="$target"
fi

if (( ${use_old_way:-0} > 0 ))
then
	rsfile=$rfile
	rfile=$TMP/PID$$.rfile
	log_msg "using the old (deprecated) method of locating checkpoint  [WARN]"
	find_it
	echo ${ckptfname:-missing}
	cleanup $?
fi

if (( $fname_field == 0 || $md5_field == 0 ))
then
	log_msg "fname field (-f) and md5 field (-M) must be supplied on command line."
	cleanup 1
fi

find_it_new 			# find reference in master log and build a roll forward file 
rc=$?
cleanup $rc


exit 0
&scd_start
&doc_title(ng_ckpt_prep:Prep for recovery from a chkpt file)

&space
&synop	ng_ckpt_prep [-m logname] [-r recovery-file] [-{s|S}] [-v] -f name-field -M md5-field recovery_key ckpt_key ckpt_fname_pat

&space
&desc	
	&This will attempt to find the most recent checkpoint file on the 
	current node, and to build a list of 'roll forward' (recovery) commands
	based on information in the master log. 
	The master log is searched for records matching the &ital(recovery_key) 
	and &ital(ckpt_key) strings supplied on the command line.  The checkpoint 
	filename pattern supplied on the command line is used to search the local 
	replication manger database for checkpoint files.  The resulting output,
	written on the standard output device, 
	is the name of the most recent checkpoint file on the current node. 
&space
&subcat	Recovery Records
	Recovery, or roll forward records, are all of the master log records
	that are timestampped after the checkpoint message that matches the selected
	checkpoint file, and that match the recovery-key supplied on the command
	line.  The recovery key can be any regular expression  that can be given 
	to &lit(gre.)
&space
	Records placed into the recovery file are sorted using the record timestamp
	as the key; duplicate records are also removed.  The consumer of the file
	must be capable of handling masterlog records as a means to roll forward 
	from the checkpoint state. 

&space
&subcat	Checkpoint Records	
	Checkpoint records in the masterlog are records which will match the 
	&lit(checkpoint-key) supplied on the command line. The record is 
	assumed to contain the pathname of the checkpoint file, in the field
	indicated by the -f option and the md5 checksum of the file in the 
	field indicated by the -M option.  The checkpoint key may be a regular
	expression, but the filename and md5 information must be in the same 
	field for all matching records. 

&space
&opts	The following options are recognised when entered on the 
	command line:
&space
&begterms
&term	-f field :  For master log records that log the checkpoint file name, this 
	sets the field number where the left most field in the record is 1. This flag
	is required. 
&space
&term	-M field : Supplies the field number that the md5 value appears in the master
	log records. If this option is omitted, a backwards compatable method for finding
	the checkpoint file is used. The deprecated method is prone to errors if one or 
	more of the local replication manger filesystems are not mounted.  Thus, all
	new programs should create checkpoint masterlog records with the md5 checksum
	and use this option when invoking &this.
&space
&term	-m log : Allows an alternate log file to be searched.
&space
&term	-r file : Supplies the name of the file to place recovery log entries into. 
	If omitted, then the file &lit($LOGNAME.recovery) is created in the &lit($TMP)
	directory.
&space
.if [ 0 ]
&term	-s : Short recovery information.  Causes only the tokens that exist on the 
	log messages following the recovery key string to be placed into the recovery
	file. The resulting file will have duplicates eliminated, but cannot be 
	guarenteed to be in the same order as the messages were written to the master
	log.  
&space
.fi
&term	-t [path/]file : Supplies the target checkpoint file name to use.  
	The filename may include a path to make it most specific. 
	If the target checkpoint name cannot be found on the local disk, or in the 
	current masterlog, &this will end with an error condition and will not select
	an alternate checkpoint file.
&space
	When a target filename is given, it is expected that there might be a fair few
	verbose messages indicating that newer checkpoint files were not considered.
&space
&term	-v : Verbose mode. Lists files that are skipped because they are not on the 
	node, do not match the target, or have a md5 value (registered with repmgr)
	that differs from the md5 value recorded in the masterlog.  This mode also
	writes messages that indicate what &this is doing. 
&endterms

&space
&exit	
	An exit code that is not zero indicates an error or that a checkpoint file 
	was not found. 

&see
	&lit(ng_tumbler)
&space

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	28 Feb 2002 (sd) : First acorns off of the tree.
&term	25 Aug 2003 (sd) : Changed ng_rm_where call to ng_rm_db direct call to suss
	the checkpoint names.  If there are duplicate basenames for ckpt files on 
	the local host rm_where cannot use rm_db ouput and sends off to leader to 
	resolve each.  This breaks at init time (esp on leader).
&term	22 Aug 2004 (sd) : Added -t (target) option.
&term	23 Dec 2004 (sd) : If no ckpt references are in the master log, then we now 
		look for the most recent one that is listed in the repmgr database.
&term	05 Oct 2005 (sd) : Converted tail -r to tac call.
&term	03 Oct 2006 (sd) : Added -f option to allow ckpt fname in master log record
		to be in a field other than NF.
&term	04 Oct 2006 (sd) : Converted to use ng_stat; suffered from the same problem
		that ng_rm_start did (possible to find the wrong checkpoint file
		if arglist was too long).
&term	10 Oct 2006 (sd) : Fixed bug if the checkpoint filename pattern was an 
		empty string. 
&term	21 Feb 2007 (sd) : Changed grep to gre as it was not stopping if a downstream
		pipe broke. 
&term	03 Mar 2007 (sd) : Fixed case where gre did like an empty ("") pattern.
&term	19 Sep 2007 (sd) : Enhanced to support md5 values in the master log for verification.
&term	05 Nov 2007 (sd) : Corrected problem; was looking for full pathname and not basename which
		prevented proper startup if service switched nodes. 
&term	05 Mar 2008 (sd) : Corrected problem with sort of master log data.
&term	07 Jan 2009 (sd) : Corrected issue where duplicate ckpt name in rm_db list was only being 	
		checked once preventing the selection of the good one if it was seen last.
&endterms

&scd_end
------------------------------------------------------------------ */

