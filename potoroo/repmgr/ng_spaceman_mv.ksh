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
#
# Mnemonic:	spaceman_mv
# Abstract:	moves  files from a source file to a named desitination file in 
#		repmgr space, or to a randomly selected destination in repmgr space.
#		It then adds the file as an instance on the node (via rm_rcv_file).
#		If a source  input file does not exist, that is ok as we assume the file was 
#		scheduled for movement twice.
#		in random mode (-r), we will generate a random repmgr file name.
#		md5 for src/dest is compared before declaring success
#		Processes all pairs on the command line
# Date:		10 April 2002
# Author:	E. Scott Daniels
# -----------------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# override of standard function
function usage
{
	cat <<endcat
	usage: $argv0 [-n] [-v] srcfile1 destfile1 [srcfile2 destfile2...]
	       $argv0 [-n] [-v] -r srcfile1 [srcfile2...]
endcat
	cleanup 1
}

function moveit
{
	src=$1
	if [[ $random -gt 0 ]]		# generate a random landing place
	then
		dest=`$NG_ROOT/bin/ng_spaceman_nm -r ${src##*/}`		# we DO NOT use the stdfunction spaceman_nm so must be fully qualified name in bin
		verbose "random destination generated: $dest"
	else
		dest=$2
	fi

	if [[ $src != /* ]]
	then
		src="`pwd`/$src"			# assume in this directory; we assume fully qualified paths from here on out
	fi

	if [[ -z $dest ]]
	then
		log_msg "*** error creating dest name; empty"
		return 1
	fi

	if [[ ! -f $src ]]
	then
		log_msg "source file missing: $src -> $dest"
		return 0					# this is ok
	else
		ng_rm_where -r 300 -5 $myhost ${src##*/} | read name srcmd5
		if [[ $name = "MISSING" || -z $name ]]
		then
			verbose "rm_where could not find src for md5sum: $src; running md5"
			md5sum $src | read srcmd5 name
		fi

		if ng_rm_db -p | grep "$src" >/dev/null			
		then
			verbose "uninstancing old stuff from rm_db"
			if [[ $for_real -gt 0 ]]
			then
				ng_flist_del $src $srcmd5
			fi
		fi
	fi

	if [[ -z $dest ]]
	then
		log_msg "*** missing destination for last parameter: $src"
		ng_log $PRI_ERROR $argv0 "missing destination: $src(from)"
		return 1
	fi
	
	if [[ $for_real -gt 0 ]]
	then
		if cp $src $dest
		then
			md5sum $dest | read destmd5 destname		# dont trust interfilesystem copies
			msg="$src=$srcmd5(src) $dest=$destmd5(dest)"

			if [[ "$srcmd5" != "$destmd5" ]]			# mismatch - we have seen bad numbers from repmgr so recalc src ourselves to be sure its bad
			then
				log_msg "=== recalcing src md5 ourself becaue mismatch of md5sums: $msg"
				md5sum $src | read srcmd5 junk
				msg="$src=$srcmd5(src) $dest=$destmd5(dest)"
			fi

			if [[ "$srcmd5" == "$destmd5" ]]			# good clean fun
			then
				verbose "md5 match after copy: $msg"
				if ! ng_rm_rcv_file $dest
				then
					log_msg "*** failure from rm_rcv_file for $dest"
					ng_log $PRI_ERROR $argv0 "failure from rm_rcv_file for $dest"
					return 1
				else
					rm -f $src >/dev/null				# finally safe to remove the src file
					verbose "successfully moved: $src --> $dest"
				fi
			else
				log_msg "*** md5 mismatch after copy: $msg"
				ls -al $src $dest >&2
				ng_log $PRI_ERROR $argv0 "md5 mismatch after copy: $msg"
				return 1
			fi
		else
			log_msg "*** copy failed: $src -> $dest"
			ng_log $PRI_ERROR $argv0 "copy failed: $src(from) $dest(to)"
			rm -f $dest							# seems if we run out of space, the file is left in a partial state
			return 1
		fi
	else
		log_msg "wanted to move: $src --> $dest"
	fi

	return 0
}

# ------------------------ -------------------------------------------
for_real=1
log2tty=0
random=0
myhost=`ng_sysname`

verbose=1		# until we are sure its doing what we want

while [[ $1 = -* ]]
do
	case $1 in 
		-n)	for_real=0;;
		-r)	random=1;;
		-t)	log2tty=1;;
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

if [[ $for_real -gt 0 && log2tty -lt 1 ]]
then
	exec 2>>$NG_ROOT/site/log/spaceman_mv
fi

verbose "starting: $@"
rc=0

while [[ -n $1 ]]
do
	if ! moveit $1 $2		# we do them all, but remember if any failed
	then
		rc=1
	fi

	if [[ $random -gt 0 ]]
	then	
		shift
	else
		shift 2
	fi
done

cleanup $rc

exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spaceman_mv:Spaceman File Move Utility)

&space
&synop	ng_spaceman_mv	[-v] srcfile destfile [srcfile2 destfile2...]
&break	ng_spaceman_mv	[-v] -r srcfile  [srcfile2 srcfile3...]

&space
&desc	&This moves files from &ital(srcfile) to &ital(destfile) and causes an 
	instance of the file to be created in the local instance database 
	via a call to &lit(ng_rm_rcv_file.)  
	If &ital(srcfile) is already registered in the local 
	instance database, it is unregistered.
	The &ital(dest) file is assumed to be a fully quallified path name, and 
	if &ital(srcfile) is not, then it is assumed to be a file in the current 
	directory.

&space
	The move is accomplished as a three step process: the source file is 
	coppied to the destination filename. If the copy is successful the 
	MD5 checksum of the new file is compared to the original file and if 
	they are the same the original file is removed. If the copy, the 
	MD5 checksum or the attempt to add the file to the instance database
	fails, a failure is logged and while the script will continue the 
	return code will indicate an error.  At least one message is written 
	to the standard erorr device, and master log file, for each failed
	attempt. 

&space
	If invoked in random destination mode (-r option) then the list of files 
	on the command line are assumed to be source files and for each source 
	file, a destination file path name is selected randomly from one of the 
	&ital(replication manager's) filesystems.

&space
	&This does not consider it a failure if the indicated &ital(srcfile) is 
	missing as it is possible that the move was scheduled twice. 
	Further, if multiple files are supplied on the command line (in either 
	source source/dest mode) all moves are attempted even if there are 
	failures. The resulting exit code will be zero if &stress(all) moves
	were successful, and a non-zero exit code will indicate that at least 
	one move attempt failed. 

&space
&opts	The following commandline parameters are required:
&begterms
&term	srcfile : The name of the file that should be moved.
&space
&term	destfile : The name that the new file should be given. 
&endterms


&space
&exit	&This will exit with a non-zero return code if there were errors.
	All file moves are attempted, even after an error is detected. The 
	return code will be zero only if &stress(all) moves were successful.

&space
&see	&ital(ng_spaceman,) &ital(ng_repmgr) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	15 Apr 2002 (sd) : First try.
&term	06 Jun 2002 (sd) : Re calcs the src md5 value if a mismatch is detected.
	The latency in repmgr was causing the source value to be wrong in some
	cases.
&term	27 Mar 2007 (sd) : Corrected man page.
&endterms

&scd_end
------------------------------------------------------------------ */
