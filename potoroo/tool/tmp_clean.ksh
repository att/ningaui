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
# Mnemonic:	tmp_clean
# Abstract:	Cleans the directory, and all below it, based on some age rules:
#			1) all files ending with .[0-9]+$ cleanded after mtime >4 days
#			2) all other files cleared if atime > 7 days.
#		
# Date:		Original: 2001 sometime, revised Jan 08 2004
# Author:	Original: Andrew Hume, Revision: E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function exclude
{
	gre -v "$exclude_pat"
}

# -------------------------------------------------------------------

ustring="[-a file-age] [-n] dir [dir...]"
forreal=1			# default to green light
logf=$NG_ROOT/site/log/tmp_clean
log_suffix=""
fage=7				# normal files are kept this many days
pid_age=4			# files with pid suffixes are kept this many days
use_mtime=0			# -m to use mtime rather than atime and ctime

# we exclude anything in the ftp directory, and goaway/no-execute files as well as cvs oriented development/build directories
# NOTE: Do NOT specify full paths (not even $TMP/foo) because if /ningaui/tmp is a 
#	symlink, we will miss the exclusion and blow things away.
exclude_pat="/ftp/|/[.]nox$|[.]goaway$|/build/|/dev/|/export/|/study/|/billabong/"		# those that mess with this will be smacked!


while [[ $1 = -* ]]
do
	case $1 in 
		-a)	fage=$2; shift;;
		-A)	pid_age=$2; shift;;
		-m)	use_mtime=1;;
		-n)	forreal=0;;
		-l)	log_suffix=".$2"; shift;;

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

logf=$logf$log_suffix

if [[ -z $1 ]]
then
	set $TMP			# default to /ningaui/tmp so we dont have to change rota just yet
fi

tmp=$TMP/x.$$

if (( $forreal  > 0 ))
then
	ng_roll_log -n 7 ${logf##*/}					# keep a rolling backup of 7 logs
fi

if ! cd ${TMP:-/foobar}			# for safety sake, make our cwd something sane; prevents absolute disaster
then
	log_msg "abort: must be able to get to TMP ($TMP) and cannot"
	cleanup 3
fi

if (( $forreal <= 0 )) 
then
	log_msg "no execute (-n) option set; nothing will be deleted"
fi

listf=$TMP/tc_list.$$
while [[ -n $1 ]]
do
	(
	if cd $1			# cannot give $1 to find as /tmp on macs is a symlink
	then
					# pump output from find|exclude through ls because sgi does not support -ls option
		if (( $use_mtime > 0 ))
		then
			time_opts="-mtime +$fage"
			ptime_opts="-mtime +$pid_age"
		else
			time_opts="-atime +$fage -ctime +$fage"
			ptime_opts="-atime +$pid_age -ctime +$pid_age"
		fi

		find . -name "*\\.[0-9]*" -type f $ptime_opts 2>/dev/null | awk '/\.[0-9]+$/{print}' |exclude >$listf 
		find . $time_opts -type f 2>/dev/null |exclude >>$listf
		if [[ -s $listf ]]
		then
			awk '{printf( "\"%s\" ", $0) }' <$listf | xargs ls -al >>$tmp	# must quote in case some wanker drops spaces into a name
	
			if (( $forreal > 0 )) && ! ng_goaway -c $argv0		# if ok to run, and goaway file not set
			then
				verbose "scratch list for $1:"
				if [[ -s $tmp ]]
				then
					echo "scratch list for $1" >>$logf
					cat $tmp >>$logf
					verbose "starting removal for $1..."
					awk '{printf( "\"%s\"\n", $0) }' <$listf | sort -u | xargs rm -f
					verbose "finished removal for $1"
				else
					log_msg "no files matched scratch criteria" >>$logf
				fi
			else
				log_msg "no execution mode! these would be scratched from: $1"
				cat $tmp
			fi
		else
			verbose "nothing found to delete in $1"
		fi
	else
		log_msg "cannot cd to: $1"
	fi
	)

	shift
done

cleanup 0
exit



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tmp_clean:Clean Old files)

&space
&synop	ng_tmp_clean [-a age] [-m] [-n] [directory [directory...]]

&space
&desc	&This will remove each file from the directories listed on the command line
	whose last access time (ctime and atime) is greater than 7 days. 
	Files with names that seem to be of the form name.<pid>  will be removed when
	both the ctime and the atime for the file is greater than 4 days. 
	
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a n : Set the age to n days for regular files. 
&space
&term	-A n : Set the age to n days for files that have *.<pid> names.
&space
&term	-l suffix : Log suffix to be used when creating the private log in $NG_ROOT/site/log.
&space
&term	-m : Force to use last modified time when searching for files to delete rather than 
	the ctime and atime values. 
	If not supplied the last atime and last ctime must both be older than the indicated age. 
&space
&term	-n : No execute mode. Only a list of files to be deleted is generated.
&space
&term	-v : Be chatty (verbose mode).
&endterms


&space
&parms	One or more directories may be placed on the command line. If no directory 
	names are placed on the command line, then /tmp is assumed.

&space
&exit	0 for good, not zero for bad.


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 Jan 2004 (sd) : Revision and doc added
&term	04 Mar 2004 (sd) : Changed NG_ROOT/tmp references to TMP.
&term	30 Sep 2004 (sd) : Added log suffix (-l). (HBD KDG)
&term	02 Dec 2004 (sd) : Now excludes build/development directories
&term	05 Apr 2006 (sd) : Now allow the top level to be a sym-link. This supports
		the macs that link /tmp to something else (grrr).  
&term	12 Apr 2006 (sd) : Fixed log name suffix bug.
&term	08 May 2006 (sd) : Uncommented the bit of code that was commented out for testing.
&term	09 May 2006 (sd) : Fixed bug in the find for pid terminated file names. 
&term	21 Jun 2006 (sd) : SGIs find seems not to support -ls as an option -- corrected that.
&term	13 Aug 2006 (sd) : Fixed problem with 6/21 code to work round sgi defecency.  The script 
		was deleting everything if nothing met the criteria.
&term	05 Sep 2007 (sd) : Now can deal with files that have spaces in their names.
&term	13 Nov 2007 (sd) : Added -m option, fixed man page.
&term	05 May 2008 (sd) : Changed list of protected directories to include /billabong/ for core files under investigation.
&endterms

&scd_end
------------------------------------------------------------------ */
