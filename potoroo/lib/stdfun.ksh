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
# Mnemonic: stdfun
# Abstract: This file contains several standard functions that are included
#           and used by many scripts.  The file should be in the user's 
#           path and can be included using the following command:
#                   . stdfun.ksh
#           The following functions are contained in this file:
#                getarg, error_msg, usage, cleanup, lockfile, unlockfile
#                verbose, log_msg, incseq, yesterday, startup, solo
#
#           In addition to providing standard functions any aliases for 
#           the rm, cp, and mv commands are removed, and a trap is set 
#           for signals 1,2,3 and 15. If any of these signals are caught
#           the cleanup function is invoked.
#
# Mods:		13 Apr 2001 (sd) : Ported into Ningaui.
#		26 Mar 2002 (sd) : added spaceman_nm function
#		15 May 2002 (sd) : ensure solo name has solo in it to use as
#		a delete mask during system start as /tmp is not cleared on 
#		linux.
#		17 Jun 2002 (sd) : To ensure -r option supported on spaceman_nm
#		26 Feb 2002 (sd) : added space before -x in trace-on test.
#		10 Jun 2003 (sd) : Removed spaceman_nm function (now tool)
#		16 Sep 2003 (sd) : Added attribute sussing function 
#		18 Sep 2003 (gus) : Added GET_MVS_BASENAME funtion, for relay mach
#		04 Aug 2004 (sd) : assume awk can be used everywhere; log msg if not in /usr/common/bin.
#		11 Jan 2005 (sd) : Detach now assumes script and invokes ksh to run the detached
#				process. It also strips leading path from argv0 to avoid truncated info
#				in the process table list on suns.
#		10 Feb 2005 (sd) : Pulled the check for awk in usr/common.
#		14 Feb 2005 (sd) : added argv0 (base) to the log msg 
#		03 Mar 2005 (sd0 : Added safe2run funcion.
#		09 Mar 2005 (sd) : added get_tmp_nm function. Also added an ERR trap.
#		10 Mar 2005 (sd) : Added skip of nox check if igore_nox is > 0. 
#		10 May 2005 (sd) : Fixed solo -- broken when ported from gecko. 
#		12 May 2005 (sd) : Now does not reference adm
#		07 Jun 2005 (sd) : Added publish_file
#		08 Aug 2005 (sd) : Added time_diff 
#		18 aug 2005 (asm) : Added tpFileMsg, tpMsg
#		13 Sep 2005 (sd) : tmp_nm now sets file perms to 664
#		30 Sep 2005 (asm) : Changed calling sequence for
#			tpFileMsg; added error handling to tpFileMsg, tpMsg.
#		14 Nov 2005 (sd) : Added echo funciton to support Tiger differences
#		20 Feb 2006 (sd) : Changed the echo function as print does not seem to work under 
#			bash on the windows node. 
#		06 Mar 2006 (sd) : added nox check for pinkpages variable (NG_GOAWAY_thing_nox)
#		20 Mar 2006 (sd) : added amIreally function to check if the current user is that 
#					which is passed in as $1. 
#		04 May 2006 (sd) : Tweeked the function name of cleanup to cleanup() so that exit 
#			trap handling would work right when ctl-c used to kill the script.
#		02 Jun 2006 (sd) : Corrected -k processing on cleanup function. 
#		09 Aug 2006 (sd) : Corrected bug with parameter passing in detach function.
#		11 Apr 2007 (sd) : Cleanup function now removes PID$$.* files in an attempt 
#			to shift away from using name.$$; the removal of *.$$ was cleaning up 
#			rm_sync's tmp files of the form name.$$.5.  
#		22 May 2007 (sd) : Added wash_env function
#		15 Oct 2007 (sd) : Ensures that NG_PROD_ID is set to something; ningaui is default. 
#		04 Jun 2008 (sd) : We now pluck off readonly vars and do ot try to unset them in wash_env; this was causing
#			ksh some grief and it was not letting us unset anything after we hit the first one. 
#		10 Jul 2008 (sd) : Added ask_user function. 
#		05 Sep 2008 (sd) : Added an 'exec' before the re-execution of the command in the _detach function 
#			to prevent a second process from 'appearing' when using ksh s+ or later.
#		27 Oct 2008 (sd) : ng_test_nattr now inludes state in the list of typeset local vars. 
#		04 Feb 2009 (sd) : Ditched the ng_leader function as that concept no longer exists. 
#		20 Aug 2009 (sd) : wash_env now preserves RUBYOPT and RUBYLIB
#		08 Sep 2009 (sd) : Now allow user to supply a 'dryclean' list that is added to the default in wash_env.
#		17 Sep 2009 (sd) : Added SHLVL to the dryclean list; unsetting causes ksh to crash
# -------------------------------------------------------------------------
#  WARNING:  stdfun.ksh canNOT be documented using scd as it is an embedded
#            script segment. The SCD documenttion for stdfun.ksh is in the 
#            file stdfun.xfm in this directory.
# -------------------------------------------------------------------------

# DEPRECATED
# Mnemonic: getarg
# Abstract: find a value associated with an option in either -xvalue or 
#           -x value form.  It will assign the value to the variable name
#           passed as the first argument. The variable shift count is set
#           to the number of arguments that were used ($2 == 1; $2 and $3 == 2)
#           so the caller knows what to shift off. 
# Parms:    $1 - Name of variable to assign value to
#           $2 - -x or -xvalue option (where x is the option letter)
#           $3 - Value if $2 is only -x
# Returns:  Nothing.
# Date:     9 April 1997
# Author:   E. Scott Daniels
# -------------------------------------------------------------------------
function getarg 
{             
 shiftcount=1                  # in case user did not set this (they should!!!)
 a="${2#-?}"                   # strip -x prefix from -xvalue 
 if [[ -z "$a" ]]              # if there was no suffex on the -x option
  then                         # assume $3 has it
   a="$3"
   shiftcount=2                # caller must shift two cmd line args 
  fi
 eval $1=\"$a\"                # assign value of argument to caller's variable
}

# ---------------------------------------------------------------------------------
# Mnemonic: 	wash_env
# Abstract: 	purge the environment of everything except a few variables. 
#		should be invoked before including the cartulary for normal scripts
#		but after sourcing the cartulary for long running scripts or wrappers
#		that start/restart daemons.
#		Caller may supply the 'dry clean only' list of colon separated 
#		environment variable names to keep from being washed (e.g. NG_ROOT:USER:LOGNAME)
# Date:		22 May 2007
# Author: 	E. Scott Daniels
# ---------------------------------------------------------------------------------

function wash_env
{
	# we keep only these from being washed away;  
	typeset DRY_CLEAN_ONLY=""
	typeset rolist=""			# read only list; ksh barfs if we try to unset a readonly var

        readonly | while read vname 			# list of vars that we cannot unset 
        do
                rolist="^${vname%%=*}=|$rolist"
        done

	DRY_CLEAN_ONLY="^TZ=|^CVS_RSH=|^DISPLAY=|^HOME=|^LANG=|^DYLD_LIBRARY_PATH|^LD_LIBRARY_PATH=|^LOGNAME=|^MAIL=|^NG_ROOT=|^TMP=|^PATH=|^PS1=|^PWD=|^SHELL=|^SHLVL=|^TERM=|^USER=|^RUBYOPT=|^RUBYLIB="
	if [[ -z $1 ]]
	then
		typeset sitedc=""
		if [[ -f $NG_ROOT/site/wash_env.lst ]]			# allow a site specific list
		then
			read  sitedc < $NG_ROOT/site/wash_env.lst
			if [[ sitedc == "+"* ]]
			then
				DRY_CLEAN_ONLY="${DRY_CLEAN_ONLY}|${sitedc#+}"		# add theirs to ours
			else
				DRY_CLEAN_ONLY="$sitedc"				# no +, just use theirs
			fi
		fi
	else
		if [[ $1 == "+"* ]]						# if user list is +*, then we add it to our list
		then
			ulist="^${1//:/=|^}="					
			DRY_CLEAN_ONLY="${DRY_CLEAN_ONLY}|${ulist#+}"	
		else
			DRY_CLEAN_ONLY="^${1//:/=|^}="			# else, just use it and trash our default list
		fi
	fi

	env | gre -v "${rolist}$DRY_CLEAN_ONLY" | while read x
	do
		unset ${x%%=*}
	done
}

# ----------------------------------------------------------------------------
# Mnemonic: amIreally
# Abstract: test to see if we are really the user id indicated in $1
# Exit:		returns 0 if we are; 1 if not
# ----------------------------------------------------------------------------

function amIreally
{
	typeset _myuser
	if ! id -u -n 2>/dev/null |read _myuser
	then
		id | read _myuser			# bloody suns
		_myuser=${_myuser%%\)*}
		_myuser=${_myuser#*\(}
	fi

	if [[ $_myuser == "${1:-ningaui}" ]]		# yes this IS hardcoded to ningaui on purpose!
	then						# most will call with $NG_PROD_ID and if it is not set default is ningaui
		return 0
	else
		return 1
	fi
}
  
# ----------------------------------------------------------------------------
# Mnemonic: echo -- silly, but Tiger changed the world and echo buggers up 
#		when the lead token(s) have a dash as the first character
# ---------------------------------------------------------------------------
function echo
{
	printf "%s\n" "$*"		# using $@ does NOT work
}

#
# ----------------------------------------------------------------------------
# Mnemonic: lockfile
# Abstract: This function will attempt to lock a file by creating a directory 
#           with the same name as the file and a .lck extension.  If the 
#           directory can be made then it will continue (as only one directory
#           can be made and thus only one process can successfully create 
#           the directory). If the directory cannot be made, then it will 
#           wait until it can make the directory (unless the NOWAIT string 
#           is passed in as $2). Once the directory can be made, a file 
#           with the current process ID is created in the directory which 
#           identifies the owner of the lock.
# Parms:    $1 - The file name to lock
#           $2 - The word NOWAIT if the function should return immediatly
#                The word MAXWAIT if the function should return a failure
#                if unable to get the lock after MAX_LOCKWAIT attemtps. 
# Returns:  Exit code 0 indicates that the lock was successfully obtained
#           exit code 1 indicates that the lock could not be obtained
#           exit code 2 indicates that the file name was not passed in
# Date:     3 July 1997
# Author:   E. Scott Daniels
# Mods:     26 Dec 1997 (sd) - To add log msg if waiting a long time and 
#                              ability to escape after MAX_WAIT (I67)
#           29 Dec 1997 (sd) - To add log msg if lock cleared after alarm
#                              and to capture process name in lock file.
# ----------------------------------------------------------------------------
function lockfile
{
 if [[ -z "$1" ]]
  then
   return 2 
  fi

 if ! mkdir $1.lck >/dev/null 2>&1             # make a directory
  then
   if [[ -n "$2"  && "$2" = "NOWAIT" ]]        # yes - user elected not to wait
    then                     
     return 1                                  # so immediate error return
    else
     _alrm=${MAX_LOCKWAIT:-60}                 # alarm if waiting a long time
     unset _opid

     while ! mkdir $1.lck >/dev/null 2>&1      # wait until we can make dir
      do
       if [[ -z "$_opid" ]]                    # get initial owner of the lock
        then
          _x=`ls $1.lck/lock.*`                        
          _opid=${_x##*.}    
        fi
       
       if [[ "$_alrm" -le 0 ]]                 # its been too long
        then
          _x=`ls $1.lck/lock.*`                        
          _cpid=${_x##*.}                      # get current owner of lock 

         if [[ "$_opid" = "$_cpid" ]]          # stagnant owner?
          then 
           if [[ "$2" = "MAXWAIT" ]]           # caller wanted only a max wait 
            then 
             return 1                          # fail now - no alarm
            fi
 
           ng_log $PRI_CRITICAL $argv0 "long wait on lock: $1 owner=$_opid UT0007"
           _alrm=${MAX_LOCKWAIT:-180}          # alarm if waiting a long time
           _msg=1
           _opid=$cpid                         # save current for next time
          fi
         fi                                    # end if owner not changing

       sleep 5                                        # nite nite
       _alrm=$(( $_alrm - 1 ))
      done

     if [[ -n "$_msg" ]]                      # we issued a long wait message 
      then
       ng_log $PRI_NOTICE $argv0 "lock cleared, no problems now"
      fi
    fi                         # end if nowait
  fi                           # end if lock existed

 #touch $1.lck/lock.$$          # establish owner of the lock with pid on file
 echo $argv0 >$1.lck/lock.$$       # lock with pid, process name into file
 if [[ ! -f $1.lck/lock.$$ ]]  # file did not get created
  then
   ng_log $PRI_ERROR $argv0 "lockfile: unable to create file in $1.lck: lock.$$"
  fi
 lock_list="$lock_list $1"     # keep list of locked files for cleanup
 return 0 
}                              # end lockfile

#
#---------------------------------------------------------------------------
# Mnemonic: unlockfile
# Abstract: This routine will unlock a given file if the lock directory 
#           contains a file called lock.$$ (indicating that this process 
#           is the owner). 
# Parms:    $1 file name
# Returns:  0  - Success
#           1  - Unable to unlock
#           2  - No file name passed in 
#           3  - File was not locked - no action
# Date:     3 July 1997
# Modified: 14 May 1998 - To mv lck dir prior to rm to prevent false 
#                         messages when busy
# Author:   E. Scott Daniels
# --------------------------------------------------------------------------
function unlockfile
{
 if [[ -z "$1" ]]                        # ensure caller passed name in
  then
   return 2 
  fi

 if [[ -d $1.lck ]]                      # ensure directory is there 
  then
   if [[ -f $1.lck/lock.$$ ]]            # ensure we own the lock
    then
     mv $1.lck $1.lckx$$                   # effectively delete it
     rm -rf  $1.lckx$$                     # then we can really delete it
     if [[ -d $1.lckx$$ ]]                 # ensure it went away 
      then
       ng_log $PRI_ERROR $argv0 "unlockfile: remove left lock hanging: $1.lckx$$"
      fi
   else
    return 1                             # dont own it - get out w/error 
   fi                                    # end if lock existed
  else                                   # no lock file existed
   return 3
  fi

 return 0 
}

# --------------------------------------------------------------------------
# Mnemonic:	publish_file
# Abstract:	removes a trailing ! or ~ from the filename to make it published
#		in repmgr space. If the file does not have a trailing special 
#		character no harm.  If -e given as first parm, then it also 
#		echos the stripped name to stdout
# Date:		7 June 2005 (sd)
# Returns:	result of mv command or success if no move attempted
# ---------------------------------------------------------------------------
function publish_file
{
	typeset ec=0
	emit=0
	if [[ $1 == -e ]]	# echo the modified name
	then
		emit=1
		shift
	fi

	case $1 in
		*!)	mv $1 ${1%!}; 	ec=$?;;
		*\~)	mv $1 ${1%\~};	ec=$?;;
		*)	nn=$1;;
	esac

	if (( $emit > 0 ))
	then
		echo "${1%[\~\!]}"
	fi

	return $ec
}


# ------------------------------------------------------------------------------------------------
# Mnemonic:	time_diff
# Abstract:	return the difference in time between $1 and $2.  If $2 is missing now is assumed
#		$1 is assumed to be the earlier time. Times assumed to be yyyymmdd[hhmmss]
# Date:		08 Aug 2005
# Author:	E. Scott Daniels
# ------------------------------------------------------------------------------------------------
function time_diff
{
	typeset et=""					# early time
	typeset lt=""					# later time
	typeset n=""
	typeset fract=""
	typeset divisor=864000;				# default to days

	while [[ $1 = -* ]]
	do
		case $1 in 
			-f)	fract=1;;
			-d)	divisor=864000;;		# days
			-h)	divisor=36000;;			# hours
			-m)	divisor=600;;			# minutes
			-s)	divisor=10;;			# seconds
		esac

		shift
	done


	if (( ${#1} < 9 ))		# if just yyyymmdd add hhmmss as midnight
	then
		et=`ng_dt -i ${1}000000`
	else
		et=`ng_dt -i ${1}`
	fi

	if [[ -z $2 ]]
	then
		lt=`ng_dt -i`
	else
		if (( ${#2} < 9 ))		# if just yyyymmdd add hhmmss as midnight
		then
			lt=`ng_dt -i ${2}000000`
		else
			lt=`ng_dt -i $2`			# use their number -- assume it is yyyymmddhhmmss
		fi
	fi

	if [[ -n $fract ]]
	then
		printf "%.1f\n"  $(( ($lt - $et)/$divisor.0 ))		# print difference between then and now in days
	else
		printf "%.0f\n"  $(( ($lt - $et)/$divisor ))		# print difference between then and now in days
	fi
}

# ---------------------------------------------------------------------------
# Mnemonic: incseq
# Abstract: This routine will lock and increase the sequence number in the 
#           file whose name is passed in. If the current sequence value is 
#           >= to  the wrap value, the sequence number will be reset
#           to 0. If the sequence file does not exist, then it is intialized
#           at 1 and 0 is returned.
# Parms:    $1 - The name of the sequence file
#           $2 - The wrap value - 0 if no wrap needed (0 default).
#           $3 - The base for the sequence 0 default
#           $4 - The string NOWAIT if we should not wait on a lock
# Returns:  The sequence value is returned via an echo statement... the user
#           must assign the results of this function to a variable or it 
#           will be echoed to the stdout device.  If there is an error, -1
#           is echoed. 
# Date:     3 July 1997
# Author:   E. Scott Daniels
#
# Modified: 14 Dec 1997 - To prevent invalid stuff in sequence file from 
#                         causing errors.
#            7 Jun 1998 - To wait to unlock seqfile until after update.
#           22 Jun 1998 - To lock file FIRST 
#           23 Sep 1998 - To support _SEQ_BASE variable.
#		03 Jul 2002 - To adjust for KSH  <$1 difference.
# Example:  snum=`incseq $seq_file $max_seq`  # get the sequence number
# ----------------------------------------------------------------------------
function incseq
{
 typeset max=${2:-0}             # max value - roll point
 typeset base=${3:-0}            # index base 
 typeset seq=$base

 if lockfile $1 $4                                 # ensure lock on file
  then
   if [[ -f "$1" ]]                                # if $1 is a file 
    then
     read seq <${1:-/dev/fd/0}                                  # get current value
    fi
  else
   echo -1
   return 1                                      # error if cannot get lock
  fi

 seq=${seq%%[!0-9]*}                # ensure seq is numeric, "" if not
 if [[ -z "$seq" ]]                 # prevent failure in $(( )) later (I39)
  then
   seq=$base                        # nothing - set at base
  fi

 if [[ "$max" -ne 0  && $((seq + 1)) -ge $max ]]   # need to roll it over
  then
   echo $base >$1                  # save rolled value
  else
   echo $((seq + 1)) >$1                           # save next value
  fi

 unlockfile $1                                     # release file

 echo $seq                                         # Pass back sequence #
 return 0                                          # good return code
} 

# ----------------------------------------------------------------------------
#
# Mnemonic: get_tmp_nm
# Abstract: This function passes all command line parms to ng_tmp_nm and 
#		echos the output names to stdout.  It keeps a copy of all of
#		the names generated for cleanup to trash on the way out.
#		we must keep the list in a file in order to allow this 
#		function to be called like this:
#			get_tmp_nm foo bar | read t1 t2
#		
# Parms:    What ever ng_tmp_nm needs/wants -- passed through
# Returns:  echos the list of files to the stdout device
# Date:     9 March 2005
# Author:   E. Scott Daniels
# Mod:
# ----------------------------------------------------------------------------
function get_tmp_nm
{
	$trace_on
	typeset stuff=""

	ng_tmp_nm -p $$ "$@" | read stuff
	if [[ -z $stuff ]]
	then
		log_msg "tmp_nm failure"
		cleanup 10
	fi

	if [[ -n $_cleanup_list ]]
	then
		echo "$stuff" >>$_cleanup_list
	fi

	chmod 664 $stuff 2>/dev/null	# linux and bsd leave files with different perms; this keeps consistant 
	echo "$stuff"
}


# ----------------------------------------------------------------------------
#
# Mnemonic: cleanup and exit_cleaner
# Abstract: This function should be called in place of the exit function 
#           so that any tmp files created will be deleted before the script 
#           exits. Typical use is to list this function on the trap command.
# Parms:    $1 - Return code to exit with
# Returns:  Does not return to the caller - exits the process
# Date:     9 April 1997
# Author:   E. Scott Daniels
# Mod:		16 Jun 2004 (sd) : Added ability to clean multiple user supplied 
#			directories in _cleanup_dirs
#		09 Mar 2005 (sd) : Now cleans from tmp file list in $_cleanup_list
#			if it is not empty. If empty old behaviour of cleaning
#			*.$$.
# --------------------------------------------------------------------------
#
#  WARNING:  do NOT use function cleanup as it does not do the right thing with traps when named this way
cleanup()                # ensure temp files are deleted before leaving
{
	trap - EXIT 1 2 3 15		# turn all traps off
	$trace_on
	if [[ $1 = -k ]]		# -k now preserves only unique tmp named files!
	then
		unset _cleanup_list		# no list to clean up 
		__cu_keep=1
		shift
	fi

	exit_cleaner		# cleanup stuff and bail with user supplied exit code
	exit ${1:-0}
}

function exit_cleaner		#invoked by exit trap when script exits (including popoffs for -e)
{
	$trace_on

	if (( ${__cu_keep:-0} < 1 ))
	then
		if [[ -s ${_cleanup_list:-nosuchfile} ]]		# if it exists, assume all tmp files are listed 
		then
			while read x 
			do
				rm -f $x >/dev/null 2>&1
			done <$_cleanup_list
		fi
		for d in /tmp ${TMP:-$NG_ROOT/tmp} $_cleanup_dirs
		do
			rm -f $d/PID$$.* $d/*.$$ >/dev/null 2>&1
		done
	fi

	rm -f ${_cleanup_list:-nosuchfile} >/dev/null 2>&1

	if [[ ${__cu_keep:-0} -eq 0 && -e ${_trace_log:-nosuchfile} ]]
	then
		rm -f $_trace_log
	fi

	if [[ -f ${_solo_file:-nosuchfile} ]]          # solo file still lives?
	then
		rm -f $_solo_file >/dev/null 2>&1         # nuke it
	fi

	for f in $lock_list                         # attempt to unlock each listed 
	do
		if unlockfile $f                          # successful unlock is "bad"
		then
			ng_log $PRI_WARN $argv0 "lockfile existed at cleanup: $f"
		fi
	done
}

# ----------------------------------------------------------------------------
#
# Mnemonic: log_msg
# Abstract: This function will echo the message passed in to the process'
#           log file (assumed to be stdout unless LOG_MSG_FILE is defined)
#           The user message is prefixed with the current date/time.
# Parms:    Message text
# Returns:  Nothing.
# Author:   E. Scott Daniels
# Mod:		25 Jan 2002 - to cope with a problem when writing to /dev/fd/2
#		02 Jul 2002 - to convert to printf and change $@ to $*
#		14 Feb 2005 (sd) - added __lmid to message.
# ----------------------------------------------------------------------------
function log_msg                
{
	if [[ $LOG_MSG_FILE == "/dev/fd/2" ]]
	then
		printf "`ng_dt -p` [$$] $__lmid: %s \n" "$*"  >&2		# do NOT use $@
	else
		printf "`ng_dt -p` [$$] $__lmid: $* \n"  >>$LOG_MSG_FILE
	fi
}

# ----------------------------------------------------------------------------
#
# Mnemonic: error_msg
# Abstract: This function will echo the message passed in to the standard
#           error device. The message is prefixed with the script name ($0).
# Parms:    Message text
# Returns:  Nothing.
# Author:   E. Scott Daniels
#		02 Jul 2002 (sd) : convert to $* and printf
# ----------------------------------------------------------------------------
function error_msg                  # generate an error message to stderror
{
 printf "`ng_dt -p` [$$] ${argv0##*/}: $*\n" >&2
}

# ----------------------------------------------------------------------------
#
# Mnemonic: usage
# Abstract: This function will generate a usage message to the standard error
#           device. The string "Usage:" and the script name ($0) are prepended
#           to the usage string. The usage string may be passed in on the 
#           command line or the function will look for it in the $ustring 
#           variable.
# Parms:    $1 - The string to echo ($ustring used if omitted)
# Returns:  Nothing.
# Author:   E. Scott Daniels
# ---------------------------------------------------------------------------
function usage
{
 echo "Usage:" `basename $argv0` "${1:-$ustring}" >&2
}

#
#---------------------------------------------------------------------------
#
# Mnemonic: verbose
# Abstract: This funton will echo the parameters passed to it if
#           the verbose variable is set to "true".  The string is 
#           echoed to STANDARD OUTPUT or the file defined by MSG_LOG_FILE.
#           If the first parameter ($1) is an at sign, the the parm list
#           is assumed to be a command. It will be executed if in verbose 
#           mode. Will also get chatty if in debug mode. 
# Parms:    Stuff to echo - all parameters echoed
# Author:   E. Scott Daniels
# ---------------------------------------------------------------------------
function verbose
{
 if [[ "$verbose" = "true"  || "$verbose" = "TRUE" || "$verbose" -gt 0 || "$_debug" = "true" ]]
  then
   if [[ "$1" = "@" ]]
    then
     shift
	if [[ $LOG_MSG_FILE = "/dev/fd/2" ]]
	then
		"$@" >&2
	else
     		"$@" >>$LOG_MSG_FILE
	fi
    else
     log_msg "$@" 
    fi
  fi
}

#
# ---------------------------------------------------------------------------
#  Mnemonic: safe2run
#  Abstract: Checks to see if the green_light process is running and if it is
#	then we are considered safe to run and the function returns good.
#  Date:     03 March 2005
#  Author:   E. Scott Daniels
#
#  Usage:    if safe2run; then...
#  Modified: 
# ---------------------------------------------------------------------------
function safe2run
{
	case `ng_ps|grep -v grep|grep -c ng_green_light ` in
		0)	return 1;;
		*)	return 0;;
	esac

	return 1
}

#
# ---------------------------------------------------------------------------
#  Mnemonic: yesterday.ksh
#  Abstract: This file can be . included into a script to provide a yesterday
#            function.  The function will calculate the date previous to 
#            either the current date or the m d y date passed in on the 
#            command line. 
#  Parms:    $1 - Month (current date assumed if $1, $2, $3 are omitted)
#            $2 - day
#            $3 - Year (1900 + y assumed if $3 is < 10)
#  Returns:  Echos the date of yesterday.
#  Date:     25 September 1995
#  Author:   E. Scott Daniels
#
#  Usage:    varname=`yesterday [m d y]`
#  Modified: 13 Aug 1997 - To convert to kshell from Bourne shell.
#            22 Oct 1997 - To accept date in yyyy mm dd OR mm dd yy and to 
#                          return date in yyyy mm dd format
# ---------------------------------------------------------------------------

function yesterday
{
 if [[ $# -lt 3 ]]                         # none/not enough command line parms
  then
   date "+%m %d %Y" | read y_m y_d y_y     # get current date
  else
   if [[ "$1" -gt 12 ]]                    # assume its is in yyyy mm dd
    then
      y_y=$1
      y_m=$2
      y_d=$3                                  # get user's date to work with
    else
      y_m=$1
      y_d=$2
      y_y=$3                                  # get user's date to work with
    fi
   if [[ $y_y -lt 100 ]]
    then
     y_y=$(( $y_y + 1900 ))                # want to work with 4 digit years
    fi
  fi

 y_d=$(( $y_d - 1 ))             # bump back to yesterday
 if [[ $y_d -lt 1 ]]
  then
   y_m=$(( $y_m - 1 ))           # crossed a month boundry
   if [[ $y_m -lt 10 ]]          # need to add a lead 0
    then
     y_m=0$y_m
    fi

   if [[ $y_m -lt 1 ]]
    then
     y_m=12                      # crossed a year boundry
     y_y=$(( y_y - 1 ))
    fi

   case $y_m in                             # set to last day in new month
    04 | 06 | 09 | 11)            y_d=30 ;;
    01|03|05|07|08|10|12)         y_d=31 ;;
    02)                           y_d=$(( 28 + `leapdays $y_y`)) ;;
   esac
  else
   if [[ $y_d -lt 10 ]]
    then 
     y_d="0$y_d"
    fi
  fi

 echo "$y_y $y_m $y_d"            # echo date in ningaui standard format
}

# --------------------------------------------------------------------------
# Mnemonic: leapdays
# Abstract: Echo the number of leap days in the year $1
# Parms:    $1 - year to check
#
# --------------------------------------------------------------------------

function leapdays
{
 echo `awk "END {printf \"%d\", $1 % 4 ? 0 : ($1 % 100 ? 1 : ($1 % 400 ? 0 : 1))}" </dev/null`
}
 
#
# -------------------------------------------------------------------------
# Mnemonic: cvt_date 
# Abstract: Converts from one date type to another. If a julian date is 
#           passed in it is converted to month, day year. If month,day,
#           year is passed in it is converted to julian. All years are 
#           assumed to be 4 digit.
# Parms:    Three styles of parameters may be passed in:
#           1.  $1 = m  $2 = d $3 = y
#           2.  $1 = y  $2 = m  $3 = d
#           3.  $1 = yyyynnn    (nnn is the day number 1-365)
#           
# Returns:  Echos the converted date to stdout
# Date:     8 October 1997
# Author:   E. Scott Daniels
# -------------------------------------------------------------------------
#
function cvt_date
{
  _sums="000031059090120151181212243273304334366"         # sums by month

 if [[ $# -ge 3 ]]       # assume m d y or y m d passed in
  then
   if [[ $1 -gt 12 ]]    # assume year first if > 12
    then
     _y=$1                # allow either format ymd or mdy
     _m=$2
     _d=$3
    else
     _m=$1
     _d=$2
     _y=$3
    fi

   if [[ $_m -gt 2 ]]
    then
     d=$(( `leapdays $_y` + $_d ))           # need to add leap day if leap yr
    fi

   echo $_y`awk -v sums=$_sums -v m=$_m -v d=$_d 'END { 
       printf "%03d", substr( sums, ((m-1)*3)+1, 3 ) + d }'</dev/null`

  else
   if [[ $# -ge 1 ]]                           # assume julian date passed in 
    then
     _y=${1%%???}                                   # pick off the year
     _days=${1#????}                                # pick off days 

     if [[ `leapdays $_y` = 1 ]]
      then
       _sums="000031060091121152182213244274305335367"       # leap year sums
      fi

     echo "$_y "`awk -v sums="$_sums" -v days=$_days 'END {
       for( i = 1; days > substr( sums, (i*3)+1, 3)+0; i++ );
       printf "%02d %02d", i, days - substr( sums, ((i-1)*3)+1, 3)}
       '</dev/null`
     #echo $_y $_m $_d
    fi
  fi

 unset _m _d _y _days _sums
}                           # end cvt_date 

# --------------------------------------------------------------------------
# Mnemonic: start_msg
# Abstract: This function will generate a standard starting message to the 
#           ningaui master log. All parameters passed in will be written 
#           on the log message. If one or more parameters is enclosed in 
#           curley braces (space seperated), then it is assumed to be a 
#           variable name and is expanded and labled in the log message. 
# Example:  start_msg $* { admin_dir cache_dir }  will display all parms
#           passed to the script, and expand the admin_dir and cache dir 
#           variables.
# Parms:    All parameters will be added to the message.
# Returns:  Nothing.
#---------------------------------------------------------------------------
function start_msg
{
 eval indicator=\$$(($#-1))
 typeset _ex=false 
 typeset _stuff=""


 while [[ -n "$1" ]]
  do
   case $1 in
    {*})                             # expand single name
       _x=${1#\{}
       _x=${_x%\}} 
       if [[ -n "$_x" ]]
        then
         eval _stuff=\"$_stuff $_x=\(\$$_x\)\"
        fi
       ;;
    {*) _ex=true                      # start expanding variable names
       _x=${1#\{}                     # see if butted against first var
       if [[ -n "$_x" ]]
        then
         eval _stuff=\"$_stuff $_x=\(\$$_x\)\"
        fi
       ;;

    *}) 
       if [[ "$_ex" = "true" ]]       # must be in expand mode to stop 
        then
         _ex=false                     # stop expanding variable names
         _x=${1%\}}
         if [[ -n "$_x" ]]
          then
           eval _stuff=\"$_stuff $_x=\(\$$_x\)\"
          fi
        fi
       ;;

    *) if [[ $_ex = "true" ]]        # add var name and it value to stuff
        then
         eval _stuff=\"$_stuff $1=\(\$$1\)\"
        else
         _stuff="$_stuff $1"      # just add the parameter to stuff
        fi
        ;;
   esac
  shift
  done

 ng_log $PRI_INFO $argv0 "starting: $_stuff CONFIG=($CONFIG) STDFUN=($STDFUN)"

 unset _ex _stuff  
}


#
#---------------------------------------------------------------------------
# Mnemonic:	$trace_on/$trace_off/_trace_init
# Abstract:	These functions and "macros" will establish and facilitate the 
#		trace environment. Sets up several "global" things:
#		trace_on - turns on tracing (or null)
#		trace_off- turns off tracing (or null)
#		_switched_log - set to indicate that log file was switched
#		_trace_log - set to name of tracing log for cleanup
#		_trace_init - function to initialise the log file etc.
#		
#		If the -x option is set in the script's command line then the 
# 		trace_on/off macros are setup; otherwise they are null. The 
#		using script must invoke $trace_on to start tracing (suggested
#		at the top of each function and right after the standard funcations
#		are included.
# Parms:   	None; if user sets the varible trace_log (note not _trace_log which is 
#		our stdfun var) before the first call
#		to trace_on, then that file will be used for the log file (detached
#		scripts ONLY. If unset, then $NG_ROOT/site/log/<argv0>.$$ is used.
#		Cleanup will delete this log file unless it is called with 
#		the -k option or the return code is > 0.
# Returns: 	Nothing.
# Date:     	5 July 2001
# Author: 	E. Scott Daniels
# -------------------------------------------------------------------------
#
_trace_switched_log=0
function _trace_init
{
	# DONT call trace_on here as it will become recursive and die
	if [[ -n "$trace_on" ]]
	then
		set -x
	fi

        if [[ $_trace_switched_log -lt 1 ]]	# if we have not switched the log yet
        then
                _trace_switched_log=1

                if [[ $_detached -gt 0 ]]	# we switch only for daemons
                then
                        typeset shortname=${argv0##*/}
                        _trace_log=${trace_log:-$TMP/log/$shortname.$$}
                        echo  "switching log to: $_trace_log" >&2
                        exec >>$_trace_log 2>&1
                fi
        fi
}

# older scripts might have used -x, but they will not have used trace_on/off, so this should be safe
if [[ "$@" = *" -x"* ]]
then
        trace_on="eval _trace_init; set -x"	# references will start/stop tracing when -x is on
        trace_off="eval set +x"
else
        trace_on=""				# harmless if -x is not on 
        trace_off=""
fi



#---------------------------------------------------------------------------
# Mnemonic: $detach
# Abstract: This function will cause the script to detach and restart 
#           itself in order to have the script run detached from the 
#           orignal parent process. It will exec stdout and stderr to 
#           what ever detach_log is set to or /dev/null if it is not set. 
#           The calling script needs to remember to do a shift immediatly
#           after the return so they can ignore the DETACHED cookie.
# Parms:    all parms that were passed in from the command line.
# Returns:  Nothing. Exits with a 0 return code if this is the first 
#           invocation (not our restart)
# Date:     14 October 1997
# Author:   E. Scott Daniels
# -------------------------------------------------------------------------
#detach='eval _detach $*; shift'   # sneeky stuff to make it simple for user
detach='eval _detach "$@"; shift'   # sneeky stuff to make it simple for user
function _detach
{
	$trace_on

	detach_log=${detach_log:-/dev/null}  # no log file specified then set to null
	if [[ "$1" != "DETACHED" ]]             # this is not the restart
	then
		if [[ -n "$_solo_file" ]]             # user called solo prior to detaching
		then
			rm -f $_solo_file >/dev/null 2>&1	# trash it so we can solo again
		fi

		exec ${argv0##*/} "DETACHED" "$@" >>$detach_log 2>&1&      # restart ourself, marked as detached
		exit 0						# original process should just exits nicely
	else
		_detached=1		# flag so script can know if necessary
	fi

	_trace_switched_log=0
	$trace_on			# force init to run to reset log after detached flag is set (new pid too)
}

# --------------------------------------------------------------------------
#  Mnemonic:  hibernate
#  Abstract:  This function will check for the existence $HIBERNATE (set in
#             .config file).  The calling function should rely on the return
#             values, and do whatever it needs to do.
#  Parms:     none.
#  Returns:   0 - hibernate file exists
#             1 - hibernate file does not exist
#  Date:      19 February 1998
#  Author:    Gus MacLellan
# ---------------------------------------------------------------------------
function hibernate
{
 if [[ -f "$HIBERNATE" ]]
 then
   return 0
 else
   return 1
 fi
}

#
# -----------------------------------------------------------------------
# Mnemonic: solo
# Abstract: Ensure that this process is not already running. Error and 
#           exit if it is. The script is considered to be running if 
#           a file exists in the /tmp directory (so that it is deleted
#           if the machine crahses)
# opts:		-w  -- will wait a few seconds if solo file exists figuring 
#		its a transient process and will clear
# Parms:    File name to look for in tmp directory... if file exists exit.
#           Uses the script name if omitted, logs a warning if it has 
#           a path.
# Returns:  If it returns it always returns success
# Date:     3 April 1998 (HBDTM!)
# Author:   E. Scott Daniels
# Modified: 23 Jun 1998 - To echo error message to stderr too.
# -----------------------------------------------------------------------
function solo
{
	$trace_on

	typeset attempts=1
	typeset get_out=cleanup		# force an exit by default
	while [[ $1 = -* ]]
	do
		case $1 in 
			-e)	get_out=return;;	# user wants to trap error so just return
			-w)	attempts=12;;
		esac

		shift
	done

	if [[ -z "$1" || "${1##*/}" = "$1" ]]  			# missing or not fully qualified
	then
		f=/tmp/solo.${1:-`basename $argv0`}           	# file to check for; MUST be in /tmp
	else
		if [[ "${1#/tmp}" = "$1" ]]          # fully qualified, but not in /tmp
		then
			f="/tmp/solo.`basename $1`"
			ng_log ${PRI_WARN:-3} $argv0 "solo: file must be in /tmp; changed $1 to $f"
		else
			f=${1%/*}/solo                          # user supplied fully qualified name
			f=$f.${1##*/}				# add solo. to their basenam
		fi
	fi

	while [[ $attempts -gt 0 ]]
	do
		attempts=$(( $attempts - 1 ))

		if [ -f $f.[0-9]* ]                     # dont use [[ ]]
		then
			if (( $attempts <= 0 ))
			then
				eval ng_log ${PRI_ERROR:-3} $argv0 \"solo: $argv0 may already be running. found file: $f.[0-9]* \"
				eval log_msg \"solo: not started, already running?? found solo file: $f.[0-9]* \"
				$get_out 1			# cleanup unless -e specified 
			else
				log_msg "solo: found solo file; waiting 5s and then will retry ($f)"
				sleep 5
			fi
				
		else
			touch $f.$$				# create the solo file
			_solo_file=$f.$$                       # global for cleanup
			return 0
		fi
	done

	$get_out 1
}

# --------------------------------------------------------------------------
# Mnemonic: ed_mark
# Abstract: Add a marker to the edition log
# Parms:    $1 edtion number, $2 marker name, $3...$n user text
# Returns:  Nothing
# Date:     18 August 1998
# Author:   E. Scott Daniels
# --------------------------------------------------------------------------
function ed_mark
{
 typeset ed=$1
 typeset m=$2
 shift 2
 ng_log $ED_LOG_MARK $argv0 "e=$ed marker=$m $*"            # DONT use $@
}

# --------------------------------------------------------------------------
# Mnemonic: show_man
# Abstract: Causes the scd section of a script to be formatted and displayed
# Parms:    none
# Returns:  Nothing
# Date:     13 Apr 2001
# Author:   E. Scott Daniels
# --------------------------------------------------------------------------
function show_man
{
	if [[ -n $argv0 ]]
	then
		(export XFM_IMBED=$NG_ROOT/lib; tfm $argv0 stdout .im $XFM_IMBED/scd_start.im 2>/dev/null | ${PAGER:-more})
	fi
}


function ng_cluster
{
	typeset c=""

	echo ${NG_CLUSTER:-NoCluster}			# from cartulary we hope
}

# ---------------------------------------------------------------------------
# Mnemonic:	chk_parms
# Abstract:	Runs the list of tokens on the command line and determines if 
#		they have been set. Generates a message and bad return if any
#		are missing.  
# Parms:	Tokens to check of the syntax: flag.parmname.varname (flag may
#		be omitted). to check that the variable scooter has been set
# 		and to issue an error message that -s value is what is used 
#		to set this parm, the token "-s.value.scooter" would be passed
#		in. 
# Date:		13 December 2001
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------
function chk_parms
{
        typeset errors=0
        typeset missing=""

        while [[ -n $1 ]]
        do

                flag=${1%%.*}
                parm=${1%.*}
                parm=${parm#*.}
                x=${1##*.}
                eval try=\$$x
                if [[ -z $try ]]
                then
                        missing="$missing$flag $parm "
                        errors=$(( $errors + 1 ))
                fi

                shift
        done

        if [[ $errors -gt 0 ]]
        then
                error_msg "required command line information missing: $missing"
                usage
                return 1
        fi

        return 0
}

# ----------------------------------------------------------
# Mnemonic:	ng_test_nattr
# Abstract:	checks to see if indicated attributes are set in 
#		the NG_NATTR cartulary variable or user supplied list
# Opts:		[-a]  		test for all atributes
#		[-l list]	list of attributes to check against ($NG_NATTR used if omitted)
# Parms:	attribute names to check against the list
# Retrns:	true if the attribues are set. If -a is given, 
#		true indicates all attributes listed on cmd line exist in the list.
# See:		ng_get_nattr (script)
# Author: 	E. Scott Daniels
# Mod:		27 Jan 2004 (sd) - now allows "!attr" testing
#		02 Feb 2004 (sd) - picks up list from ng_get_nattr as a default so that 
#			we also get os type (not in NG_NATTR). Revampped algorithm.
# ---------------------------------------------------------------------------------------
function ng_test_nattr
{
        typeset all=0				# must match all nattrs passed in if 1
	typeset list=""				# if empty after parms, then we fill with local nattrs
        typeset this=""
        typeset tru=""				# real true/false based on a !nattr 
	typeset fls=""
	typeset rc=""				# return code if we test all from cmd line
	typeset state=""

        while [[ $1 = -* ]]
        do
                case $1 in
                        -a)	all=2;;			# MUST be 2!
                        -l)	list="$2"; shift;;	# user supplied list (from another node?)
                esac

                shift
        done

	list=${list:-`ng_get_nattr`}		# get local node attrs if list is not supplied

        while [[ -n $1 ]]
        do
                this=$1
                tru=1				# default -- inc found if true ($1 is in attr list)
		fls=0
                if [[ $this = \!* ]]
                then
                        tru=0			# swap truth and fiction and drop ! for compare
			fls=1
                        this="${this#?}"	# loose the !
                fi

		state=$fls
		rc=$tru					# return code is opposite of default state because rc of 0 is good
                if [[ " "$list" " = *" "$this" "* ]]	# check next attr from cmd line against list
                then
			state=$tru
			rc=$fls				# rc is the opposite of state because 0 is good
                fi    
                  
		case $(( $all + $state )) in		# all MUST be 0 or 2 (set by -a)
			0) ;;			# need one, this did not match, so we try again
			1) return 0;;		# need one, this mathced so short circuit good
			2) return 1;; 		# need all, no match here, so we can fail now
			3) ;;			# need all, this matched, keep going
			*) log_msg "unexpected result in test_nattr $(( $all + $state ))";;	# give warning
		esac 

                shift
        done
	
	return $rc		# rc set by last test should incase we fall to here
}

# ---------------------------------------------------------------------------------------------------
# Mnemonic:	ask_user
# Abstract	prompt for info, response echo'd to stdout; prompts are written to /dev/tty in order
#		to allow answer to be written to stdout and captured by caller in a var. 
# Opts:
#  		-e 		allows empty reponse
#  		-d string	retruns string if user supplies empty
#  		-l file 	write info to file about prompt and responses
#  		-y 		forces yes/no response, guarentees either yes or no is returned
# Author: 	E. Scott Daniels
# Date:		10 July 2008 
# ---------------------------------------------------------------------------------------------------
#
function ask_user
{
	typeset allow_empty=0
	typeset yes_no=0
	typeset ans=""
	typeset default=""
	typeset pd=""
	typeset log=/dev/null

	while [[ $1 == -* ]]
	do
		case $1 in 
			-d)	allow_empty=1; default="$2"; pd=" [$default]"; shift;;
			-e)	allow_empty=1;;
			-l)	log=$2; shift;;
			-y)	yes_no=1;;
		esac

		shift
	done


	echo "PROMPT: $@$pd" >>$log
	while true
	do
		printf "%s: " "$@$pd" >/dev/tty
		read ans

		if [[ -z $ans ]] 
		then
			if (( $allow_empty > 0 ))
			then
				echo "EMPTY RESPONSE default=${default:-not-supplied}" >>$log
				echo "$default"
				return
			fi
		else
			if (( $yes_no > 0 ))
			then
				case $ans in 
					y|Y|yes|YES|Yes) 
							echo "RESPONSE $ans translated to yes" >>$log
							echo "yes"
							return
							;;

					n|N|no|NO|No) 	
							echo "RESPONSE $ans translated to no" >>$log
							echo "no";
							return
							;;

					*)	echo "answer must be yes or no" >&2
				esac
			else
				echo "RESPONSE $ans" >>$log
				echo "$ans"
				return
			fi
		fi
	done
}


# -------------------------------------------------------------------------
# Mnemonic: GET_MVS_BASENAME
# Abstract: finds the base name of an MVS feed file without the GDG
#               portion when passes as $1 the file name
# Parms:    $1 - Name of MVS file name
# Returns:  Basename of file.
# Date:     12 December 2001
# Author:   Angus Maclellan
# -------------------------------------------------------------------------

function GET_MVS_BASENAME
{
        # will echo out the MVS filename without the GDG
        # portion when passes as $1 the filename

        if [[ -n "$1" ]]
        then
          echo $1 | nawk '
                {
                        name=$1
                        if(match(name, "\\.G[0-9][0-9][0-9][0-9]V[0-9][0-9]")) {
                                print substr(name, 1, RSTART-1)
                        } else {
                                if(match(name, "\\.[0-9][0-9]*\.[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]")) {
                                        print substr(name, 1, RSTART-1)
                                } else {
                                        print "unknown"
                                }
                        }
                }'
        else
                print "unknown"
        fi
}


# -----------------------------------------------------------------------------
# Mnemonic: tpFileMsg
# Abstract: write a file to the debugging log
#           (used only by Adam's new tape stuff)
#           expects tpMSGFILE to be in the environment
# Parms:    $1 - program name
#           $2 - file to be logged
# Returns:  Nothing.
# Date:     30-Aug-2005
# Author:   Adam Moskowitz
# Mods:     30 Sep 2005 (asm) : Changed calling sequence, added error handling.
# -----------------------------------------------------------------------------
function tpFileMsg {
	if [[ $tpMSGFILE ]] ; then
		echo "[$(ng_dt -u)] ${1}: $2 {" >> $tpMSGFILE
		cat $2 >> $tpMSGFILE
		echo "}" >> $tpMSGFILE
	else
		echo "tpFileMsg($1, $2): tpMSGFILE not defined!" 1>&2
		ng_log $PRI_ERROR "$1" "tpFileMsg($1, $2): tpMSGFILE not defined!"
	fi
} # tpFileMsg


# -----------------------------------------------------------------------------
# Mnemonic: tpMsg
# Abstract: write a message to the master log and/or the debugging log
#           (used only by Adam's new tape stuff)
#           expects tpMSGFILE to be in the environment
# Parms:    -d - write message only to debug log
#           (*MUST* be first arg!)
#           $1 - message priority (after -d is removed)
#           $2 - program name
#           $3 - message
# Returns:  Nothing.
# Date:     30-Aug-2005
# Author:   Adam Moskowitz
# Mods:     30 Sep 2005 (asm) : Added error handling.
# -----------------------------------------------------------------------------
function tpMsg {
	if [[ X$1 == X-d ]] ; then
		shift
	else
		ng_log $1 "$2" "$3"
	fi

	if [[ $tpMSGFILE ]] ; then 
		echo "[$(ng_dt -u)] ${Prog}: $3" >> $tpMSGFILE
	else
		echo "tpFileMsg($1, $2, $3): tpMSGFILE not defined!" 1>&2
		ng_log $PRI_ERROR "$2" "tpMsg($1, $2, $3): tpMSGFILE not defined!"
	fi
} # tpMsg

# ------ end functions -----------------------------------------------------

# ---- this code is EXECUTED at "load" time; no funcitons
#

# gawk should be installed as awk in /usr/common/bin. 
# 
#
alias nawk=awk

# nobody was looking at this anyway - so why waste the cycles
#if [[ `whence awk`  != "/usr/common/bin/awk" ]]
#then
#	ng_log $PRI_WARN $0 "wence does not find awk in /usr/common/bin"
#fi
	

#if  whence nawk >/dev/null 2>&1
#then
#	alias awk=nawk		# blasted sun could not do it right
#else
#	alias nawk=awk	  # just in case I missed one during the port
#fi

argv0=$0             				# allow script name access in script functions
NG_PROD_ID=${NG_PROD_ID:-ningaui}		# the production id if script needs to check

alias rm=""		# this is silly, but unalias returns 1 if they are not 
alias cp=""		# aliased and causes -e trap to be taken.
alias mv=""
unalias rm 2>>/dev/null	
unalias cp 2>>/dev/null
unalias mv 2>>/dev/null

umask 002
ustring="Idiot programmer omitted usage string, sorry Charlie! (try $argv0 --man)"

if [[ -z "$_NOTRAP" ]]
then
	#trap "exit_cleaner 254" EXIT 1 2 3 15      # ensure locks are gone etc on signals
	trap "cleanup 254" 1 2 3 15
	trap "exit_cleaner" EXIT
fi

_debugf=${argv0##*/}
_debugf=$NG_ROOT/site/${_debugf%%.*}.debug
__lmid=${argv0##*/}
ng_tmp_nm cleanup | read _cleanup_list		# DONT use the std function for this
rm -f ${_cleanup_list:-nosuchfile}		# first use of get_tmp_nm will create again; this prevents poorly coded script from leaving it

# ---------------------------------------------------------------------
#  Check to see if we should be very noisy.  Do NOT redirect stdout
#  or some scripts will not function properly.
# ---------------------------------------------------------------------
if [[ -e $NG_ROOT/site/debug && -z "$_NODEBUG" ]]
 then
  _debug=true                        # force verbose messages
  _l="$TMP/`basename $argv0`.$$.!!"      # write stderr to tmp directory
  stdio - $_l                        # write stderr to tmp directory
  LOG_MSG_FILE=$_l                   # cause verbose messages to go here
  read _x <$NG_ROOT/site/debug          # see if intense mode on
  verbose "$@"  
  if [[ "$_x" -gt 0 ]]
   then
    set -x
   fi
  unset _x _l
 else
	LOG_MSG_FILE="/dev/fd/2"	# default to standard error
 fi

# ----------------------------------------------------------------------
# if someone has created a nox file via ng_goaway or similar, then we 
# exit immediately.
# ----------------------------------------------------------------------
# do NOT test with ng_goaway or it will recurse to china, do NOT use ppget for the same reason
if (( ${ignore_nox:-0} <= 0 ))
then
	typeset _sname=${argv0##*/}
	_noxvn="NG_GOAWAY_${_sname%%.*}_nox"
	eval _noxv=\${NG_GOAWAY_${_sname%%.*}_nox}			# if set in cartulary; we CANNOT use ppget!
	if  [[ -e $TMP/${argv0##*/}.nox ]] || (( ${_noxv:-0} > 0 ))
	then
        	ng_log $PRI_INFO $argv0 "not started, found nox file: $TMP/${argv0##*/}.nox"
        	if [[ -r $TMP/${argv0##*/}.nox ]]
		then
        		log_msg "not started, found nox file: $TMP/${argv0##*/}.nox"
        		read rc <$TMP/${argv0##*/}.nox
		else
			log_msg "not started, nox cartulary variable exists: $_noxvn"
		fi
        	cleanup ${rc:-0}
	fi

	unset _sname
	unset _noxv
	unset _noxvn
fi


