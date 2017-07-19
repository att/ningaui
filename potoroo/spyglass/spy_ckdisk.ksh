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
# Mnemonic:	spy_ckdisk
# Abstract:	check to see if we can detect any reported disk failures/errors
#		check to see if permissions on a file/dir are bad
#		
# Date:		10 Jan 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# desired-perm filename [filename...]
# Mdeisred-mask filename...
#
# if M004, then we are good if the read bit for other is on ignoring all other bits
# when a mask is given, the full set of mode bits, not just permissions, can be tested.
function ck_perm
{
	typeset mask=0
	typeset p=""
	typeset rc=0

	typeset dperm=$1;		# desired perm
	shift

	if [[ $dperm == M* ]]		# value is a mask
	then
		mask=1
		dperm="${dperm#?}"
	fi

	while [[ -n $1 ]]
	do
		if [[ ! -e $1 ]]		# missing files do not produce errors
		then
			shift
			continue 
		fi

		p=$( ng_stat -M $1 )		# current settings
		if (( mask == 0 ))		# we must match exactly what was put in
		then
			p=${p/?(*)@(???)/\2}		# chop everything before the last three characters
			if [[ $dperm == $p ]]
			then
				log_msg "$1 $p(actual) == $dperm(desired)  [OK]"
			else
				log_msg "$1 $p(actual) == $dperm(desired)  [FAIL]"
				rc=1
			fi
		else
			if [[ $(( 0$p & 0$dperm )) == $(( 0$dperm )) ]]		# if current perms AND mask == mask, then we are good
			then
				log_msg "$1 $p(actual) has mask $dperm(desired)  [OK]"
			else
				log_msg "$1 $p(actual) does not have mask $dperm(desired)  [FAIL]"
				rc=1
			fi
		fi
		
		shift
	done

	return $rc
}

# ck_access {read|write|both} filelist
# tests to see if desired access is allowed for the file(s) 
function ck_access
{
	typeset -A aname=( ["r"]="read" ["w"]="write" )
	case $1 in 
		read)
			typeset access="-r"
			typeset access="-w"
			;;

		write)
			typeset access="-w"
			;;

		*)
			typeset access="-r -w"
			;;
	esac
	shift

	typeset rc=0

	for f in "$@" 
	do
		for a in $access 
		do
			ai=${a#?}
			# must eval to expand $access or we get a parse error
			eval "if  [[  $a  $f ]]
			then
				log_msg \"${aname[$ai]} access to $f is permitted [OK]\"
			else
				eval log_msg \"${aname[$ai]} access to $f not permitted [FAIL]\"
				rc=1
			fi"
		done
	done

	return $rc
}

# darwin only; run diskutil and look for a good status for the SMART catigory
function ck_diskinfo
{

	typeset tfile=""
	typeset dupid=""

	get_tmp_nm tfile | read tfile

	diskutil list >$tfile 2>&1 &	# these sometimes hang on lepoard; start in bg and if not cleared in a few seconds kill it
	dupid="$!"
	
	verbose "waiting for diskutil -- ensuring not hung"
	sleep 3
	
	
	if ng_ps|gre "$dupid.*[d]iskutil.list" >/dev/null 2>&1
	then
		log_msg "diskutil appears hung; killing it"
		kill -9 $dupid
		return 0
	fi

	gre "/" <$tfile | gre -v "$ignorepat" | while read dev
	do
		echo "CHECKING DEV: $dev"		# message for awk to key on
		diskutil info $dev
	done | awk -v verbose=$verbose '
		BEGIN { error = 0; }
		/CHECKING DEV:/ { dname = $NF; next; }
		/SMART/ { 
			if( $NF == "Verified" || ($(NF-1) == "Not" && $NF == "Supported") )
			{
				printf( "%s: %s [OK]\n", dname, $0 );
			}
			else
			{
				printf( "%s: %s [FAIL]\n", dname, $0 );
				domore[dname]++;
				error = 1;
			}
		}
		END {
			if( error && verbose )
				for( x in domore )
				{
					printf( "mountpoints on failing disk %s\n", x );
					system( "ng_df|gre " x ";echo \" \"" );
				}
						
			exit( error )
		}
	'
	return $?
}

# deprecated; ng_eqreq verify mounts is now preferred
# check all filesystems listed in the arena to ensure that they are mounted
function ck_rmmounts
{
	typeset list1=""

	if (( NG_ARENA_ALLOW_DIR > 0 ))
	then
		return 0			# if we allow directories, we cannot tell good from bad, so dont try
	fi

	ng_rm_arena list | read list1
	for a in $list1
	do
		if [[ -L $a ]]
		then
			arena_list="${arena_list}$(ls -al $a|awk '{print $NF}') "
		else
			arena_list="${arena_list}$a "	
		fi
	done
		

	awk -v verbose=$verbose -v arena_list="$arena_list" '
	{
		seen[$NF] = 1;
		next;
	}

	END {
		state = 0;
		x = split( arena_list, a, " " );
		for( i = 1; i <= x; i++ )
		{
			if( ! seen[a[i]] )
			{
				printf( "ck_rmmounts: cannot find in df listing: %s [FAIL]\n", a[i] ) >"/dev/fd/2";
				state = 1;
			}
			else
				if( verbose )
					printf( "ck_rmmounts: found in df listing: %s [OK]\n", a[i] ) >"/dev/fd/2";
		}

		exit( state );
	}
	'<$dff

	return $?
}

# -------------------------------------------------------------------
ustring="{diskinfo | arena_mounts}"
ignoref=$NG_ROOT/site/spyglass_ckdisk.ignore		# contains vert bar separated names of filesysems to ignore
ignorepat="nothing2ignore"
what=diskinfo				# default to this -- original script did only this
verbose=0

while [[ $1 == -* ]]
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

if [[ -n $1 ]]
then
	what=$1
	shift
fi

dff=$TMP/df.$$		#only one call per invocation goes to this file
ng_df $inodes >$dff


case $what in 
	dev_access)			# check access to /dev things specfified in site/config. we assume root owns, so checks are based on group access
		access=$1
		case $access in 
			read)	mask="M040";;
			write)	mask="M020";;
			*)	mask="M060"; access=both;;
		esac

		list=""
		error=0
		gre "/dev/" $NG_ROOT/site/config | while read name dev
		do
			list="$list$dev "
		done	

		if [[ -z $list ]]
		then
			log_msg "no device special files listed in $NG_ROOT/site/config [OK]"
		else
			if ! ck_perm  $mask $list
			then
				error=1
			fi
	
			
			if ! ck_access $access $list
			then
				error=1
			fi
		fi
		;;


	perms)
		if ! ck_perm "$@"
		then
			error=1
		fi
		;;

	diskinfo)				
		if [[ -f $ignoref ]]
		then
			read ignorepat <$ignoref
		fi

		error=0
		if ng_test_nattr Darwin
		then
			verbose "running diskinfo check"
			if ! ck_diskinfo			# run diskinfo check on each filesystem
			then
				error=1
			fi
			echo " "
		fi
		;;

	arena_mounts)	
		if ! ck_rmmounts
		then
			error=1
			if (( $verbose > 1 ))
			then
				cat $dff
			fi
		fi
esac


cleanup $error



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_ckdisk:Check for disk issues)

&space
&synop	ng_spy_ckdisk	diskinfo
&break	ng_spy_ckdisk [-v] arena_mounts	(deprecated; use ng_eqreq verify mounts)
&break	ng_spy_ckdisk perms {perms|Mmask} file [file...]
&break	ng_spy_ckdisk dev_access {read|write|both}

&space
&desc	This script performs various disk related checks based on the command line 
	paramter given.  The following lists the valid parameters and the tests
	that each causes to be executed: 

&space
&begterms
&term	diskinfo : Currently this is supported only on Darwin (Mac OSX) systems. 
	When this parameter is specified &lit(diskutil)  is executed; &this will 
	exit with a non-zero return code if &lit(disutil) indicates a problem.
&space
&term	arena_mounts : Causes the replication manager arena filesystem mounts to 
	be verified.  If a filesystem listed in the arena does not appear in a &lit(df) 
	listing, an error will be signaled. 
&space
&term	perms : Accepts a permission setting (e.g. 775) or a mask (e.g. M440) and 
	followed by the files and/or directories to verify.  An error is indicated if any of the listed
	files or directories does not have the indicated permisison setting, or whose
	permissions do not have all of the mask bits set. 
&space
	When a mask is gien, the full file mode, not just the permissions, can be tested for. 
	This includes file type and setXid bits.  See fstat(2) for bits to test file types.
&space
&term	dev_access : Examines the NG_ROOT/site/config file for all &lit(/dev/) devices and 
	ensures that the desired access type (read, write, both) is permitted. 
&endterms

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : verbose -- might chat on about things.
&endterms


&space
&exit	A non-zero exit indicates that one or more errors were discovered. 

&space
&see	&seelink(spyglass:ng_spyglass) &seelink(spyglass:ng_spy_ckdaemons) &seelink(spyglass:ng_spy_ckfsfull)
	fstat(2)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 Jan 2007 (sd) : Its beginning of time. 
&term	02 Jul 2007 (sd) : Added arena check.
&term	28 Apr 2008 (sd) : Added check for hung diskutil -- seems to hang on lepoard. 
&term	17 Nov 2008 (sd) : Enhanced to allow arena to be a symlink in $NG_ROOT.
&term	05 May 2009 (sd) : Added dev_access check.
&endlist


&scd_end
------------------------------------------------------------------ */
