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
# Mnemonic:	spy_ckfage
# Abstract:	check for old files with a given pattern in one or more directories
#		unless the -A option is given, we limit the search just to the directory
#		name(s) given -- we do NOT decend into subdirs. 
#		
# Date:		26 June 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

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


# -------------------------------------------------------------------
age=3600
test_name="newer"
not_flag=""
size=""
list_all=0
find_opts="! -name . -type d -prune -o ! -name . "		# prevent decent into subdirs by default
ustring="-d dir -p pattern [-o | -n] [-a seconds] [-d dlist] [-i ignore-pat] [-s] [-v] [-V]"
while [[ $1 = -* ]]
do
	case $1 in 
		-a)	age=$2; shift;;
		-A)
			find_opts="! -name . ! -type d  "		# prevent decent into subdirs by default
			;;
		-d)	dlist="$dlist$2 "; shift;;
		-i)	ignore_pat="$2"; shift;;
		-p)	pat="$2"; 	shift;;
		-o)	test_name=older; not_flag="!";;
		-n)	not_flag="";;
		-s)	size="! -size 0";;		# file must also be non-zero

		-v)	verbose=1;;
		-V)	list_all=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

tf=$TMP/PID$$.spy_at
mk_oldfile $age $tf 			

rc=0
for d in ${dlist:-.}
do
	
	if cd $d
	then
		tfile=$TMP/ckfage.$$
		dfile=$TMP/spyglass/ckfage.data.$(ng_dt -i)
		find . $find_opts $not_flag -newer  $tf $size -print 2>/dev/null |gre -v ${ignore_pat:-nothing2ignore} >$tfile  
		gre -c "${pat:-.*}" $tfile |read count
		if (( $count > 0 ))
		then
			gre "${pat:-.*}" $tfile >$dfile
			md5sum $tfile | read key junk
			log_msg "dir=$d fpat=${pat:-.*} threshold=$age -- some files ($count) are $test_name than expected [FAIL]"
			log_msg "triggering data saved: $(ng_sysname):$dfile"

			if (( $list_all < 1 ))
			then
                        	log_msg "list of files (truncated at 20 max) that triggered the event follows:"
				# foo42 prevents ls from dumping the whole of cwd if all trigging files removed between above find and here (yes, it's happened!)
                                gre "${pat:-.*}" $tfile |xargs ng_stat -m -n |sort -k 1rn,1 | tail -20 | awk '{print $2}'| xargs ls -alrt foo42 2>/dev/null
			else
                        	log_msg "list of files that triggered the event follows:"
                                gre "${pat:-.*}" $tfile |xargs ng_stat -m -n |sort -k 1rn,1 | awk '{print $2}'| xargs ls -al foo42 2>/dev/null
			fi
				

			echo "key: $key" >&2
			rc=1
		else
			log_msg "dir=$d fpat=${pat:-.*} -- no files are $test_name than expected [OK]"
			echo "key: 0" >&2
		fi
	else
		log_msg "cannot switch to directory: $d"		# ignore this for now
	fi
done

cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_ckfage:Spyglass probe - Check for old/new files)

&space
&synop	ng_spy_ckfage -p pattern [-a age-sec] [-d directory] [-i ignore-pattern] [-o | -n] [-s] [-v] [-V]


&space
&desc	&This will check the directory, or directories, for files that are newer
	than &ital(age-sec) seconds and that match the pattern provided. If older files
	are to be searched for, then the &lit(-o) option should be used.  The result is
	a list of files that do not meet the criteria (older or newer). 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a age : The age threshold for searching. Age must be supplied in seconds. If not supplied
	an age of 3600 seconds (1 hour) is used. 
&space
&term	-A : Search all subdirectories under each directory given.  By default, we look only 
	in the directories that are named with the -d option and do not decend into any 
	subdirectories. 
&space
&term	-d directory : Lists one or more directories to search. If more than one directory 
	is provided the argument must be quoted and the list space separated. If not supplied 
	the current directory (and all sub directories) is used. 
&space
&term	-i pattern : Pattern to ignore. Files that would normally trigger an alarm, and that match this 
	pattern are ignored. 
&space
&term	-p pattern : A regular expression that is used to filter the files matching the older/newer
	criteria. If no pattern is supplied "^.*" is used.
&space
&term	-o : Search for files older than &ital(age) rather than newer files. 
&space
&term	-s : Files must also be nonzero in length to trigger an alarm.
&space
&term	-v : Verbose mode. 
&space
&term	-V : Verbose listing mode. Normally only 20 files maximum are listed. If this option is 
	suppiled, then all of the files that triggered the alarm are listed.  This option should
	NEVER be supplied by a spyglass probe command.
&endterms


&space
&exit	&This will exit with a non-zero return code if it finds any files that match the criteria. This 
	may seem backwards, but as this script is designed to 'raise an alarm' when files match, it is 
	considered normal (non-error) if nothing is found. Thus, a zero indicates that nothing matched
	the criteria.

&space
&see	ng_spyglass, ng_spy_ckdaemons

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Jun 2006 (sd) : Its beginning of time. 
&term	31 Jul 2006 (sd) : Find error messages are now ignored. 
&term	16 Aug 2006 (sd) : Added -s option to force files to have size too. 
&term	18 Sep 2006 (sd) : Corrected find options to do the right thing with size. 
&term	20 Aug 2007 (sd) : Corrected man page. 
&term	04 Mar 2008 (sd) : Added -V option and default truncation to 20 files max.
&term	06 Mar 2008 (sd) : Now presents the 20 oldest after jumping through hoops. 
&term	28 Apr 2008 (sd) : Corrected log message to indicate the right number of old files. 
&term	27 May 2008 (sd) : Added -i option. 
&term	22 Apr 2009 (sd) : Corrected bug that caused all of cwd to be dumpped if the triggering files were 
		deleted between the time they were detected as being out of date and the time that the 
		list command was executed to list the offending file details (yes it happened).
&endterms


&scd_end
------------------------------------------------------------------ */
