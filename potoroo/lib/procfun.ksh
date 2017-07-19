#
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


# --------------------------------------------------------------------------------------------
# Mnemonic:	procfun - process oriented standard functions
# Abstract:	These functions are process oriented utilities
#		NOTE: 
#			These functions may create files in the /tmp directory.
#			the caller must either use the cleanup function from 
#			stdfun to exit, or must rm /tmp/*.$$ to remove these 
#			temporary files.
#
#		2nd NOTE:
#			is_alive and get_pid MUST be fed the process listing from 
#			ng_ps.  This allows a single call to ng_ps to be made and 
#			each check compared to it, rather than multiple calls to ps.
#
# Date:		12 August 2003 (collected from scripts where duplicated)
# Author: 	E. Scott Daniels
# Mod:		11 Jan 2005 (sd) : To remove --exec from wrapper call; not needed
#		02 May 2005 (sd) : Added -w to fling.
#		13 Dec 2005 (sd) : Fling now deals with options correctly.
#		17 Jan 2006 (sd) : Added -d flag to endless call if fling started with -p
#		03 Aug 2006 (sd) : Fling now returns 1 if the process is already running
#		09 Aug 2006 (sd) : Changed fling to also look for the endless process if -e option given
#		27 Sep 2006 (sd) : Converted is_alive and get_pids to use ng_proc_info as their core.
#		20 Apr 2009 (sd) : Added more info to log-msg 'started with endless...'
# --------------------------------------------------------------------------------------------

_procfun_ver=2			# ng_init checks this when it depends on changes made to this lib set


# returns with a valid return code if one or more of the named processes ($1) with optional
# arguments ($2) is running.
# ex:  if is_alive ng_df_mon "-c bing" <ps_listing	# returns true if df_mon with the option -c bing is running
function is_alive
{
	typeset n=""

	ng_proc_info -c "$1" "$2" | read n	# get number of processes that match
	return $(( ! $n ))
}

# returns a list of process ids (echod to stdout) for each process running that matches $1
# with optional arguments supplied in $2. 
# ex: 	get_pids "ng_df_mon" "-c bing" <ps_listing | read pids
# note that the argument -c bing can be anywhere in the command string; it will find 
# the df_mon processes started with either of these commands:
#	ng_df_mon -c bing
#	ng_df_mon -v -c bing
#
# now just a wrapper for ng_proc_info; probably best to use proc-info directly
function get_pids
{
	ng_proc_info "$1" "$2"
}

# start something as $1 with $2 args if it is not running
#  we check to see if its running using $1 and $2 only, so if something is a unique process 
#  because of space separated options then the proper way to invoke this funciton is with 
#  the options in quotes.
#  ex: 	df_mon.ksh "-c bing -h ning14"
#  redirection and use of the async operatior (&) should be passed in as $3..n
#  This function accepts one of three options (flags) before the command and its parameters:
#  	the -d flag causes the command to be started as a detached process via ng_wrapper
#  	the -e flag causes the command to be started using ng_endless (it will automatically be detached)
#	the -E flag causes the command to be started using ng_endless; endless is passed the -l flag
#		(for commands like ng_cdmon that manage their own log file)
#  returns 1 if process is already running 
function fling
{
	typeset pids=""
	typeset epids=""

	typeset detach=0
	typeset endless=0
	typeset endless_flg=""
	typeset wash_flg=""

	while [[ $1 = -* ]]
	do
		case $1 in 
			-d)	detach=1;;
			-e)	endless=1;;
			-w)	wash_flg="-w ";;
			-E)	endless=1; endless_flg="$endless_flg-l ";;
			-p)	endless_flg="$endless_flg-d $PWD ";;			# keep in local dir
			*)	;;
		esac

		shift
	done


	typeset now=`ng_dt -i`
	if [[ $(( ${__made_ps_list:-0} + 600 )) -lt $now ]]		# get a new list every  minute
	then
		ng_ps >/tmp/__ps.$$
		__made_ps_list=$now
	fi

	get_pids "$1" "$2" </tmp/__ps.$$ |read pids
	if (( $endless > 0 ))				# need to get pids for endless as well
	then
		get_pids "ng_endless" ".*[ /]$1.*$2" </tmp/__ps.$$ | read epids
	fi

	if [[ -n $pids$epids ]]
	then
		log_msg "$1 $2 already running; pid(s) = $pids $epids"
		return 1
        else
                if [[ ${forreal:-1} -gt 0 ]]		# user can set forreal to 0 to prevent execution; defaults to 'live'
                then
                        msg="$argv0/fling: starting: $@"
                        ng_log ${PRI_INFO:-6} $argv0 "$msg"
                        log_msg "$msg"

			if [[ $detach -gt 0 ]]
			then
				eval ng_wrapper --detach $*
			else
				if [[ $endless -gt 0 ]]
				then
					log_msg "started with endless $wash_flg $endless_flg $@"
					ksh ng_endless $endless_flg $wash_flg -v "$@"
				else
                        		eval "$@"                      # must eval to properly get redirection and async
				fi
			fi
                else
                        log_msg "$argv0/fling: e=$endless d=$detach would start: $*"
                fi
        fi
}

# force a process down if it is running
# -n   argument (e.g -15) that is passed to kill.
# $1 - command name and any arguments if uniqueness is needed (df_mon will kill all df_mon processes
#	where "df_mon -c bing" will kill only the df_mon process that is running with the -c bing options")
#	All commands that match will be forced down.
# $2 - The command to issue to stop the process. If not supplied, then kill -9 is used.
#
function force
{
	typeset pids=""
	typeset p=""
	typeset msg=""
	typeset kill_arg="-9"

	if [[ $1 = -* ]]
	then
		kill_arg="$1"
		shift
	fi

	if [[ ${__made_ps_list:-0} -lt 1 ]]
	then
		ng_ps >/tmp/__ps.$$
		__made_ps_list=1
	fi

	# we chop $1 into command and args (if supplied) for get_pids
	p="$1 "
	get_pids "${p%% *}" "${p#* }" </tmp/__ps.$$ |read pids
	if [[ -n $pids ]]
	then
               	if [[ -n $2 ]]					# a command to run rather than kill
               	then
                       	if [[ $forreal -gt 0 ]]
                       	then
                               	msg="$argv0/force: stopping $1 with $2"
                               	ng_log ${PRI_INFO:-6} $argv0 "$msg"
                               	log_msg "$msg"
                               	eval $2         # assume command supplied
                       	else
                               	log_msg "$argv0/force: would stop $1 == ${owner:-unk} $pid with: $2"
                       	fi
               	else					# assume we kill
			for p in $pids
			do
                       		if [[ $forreal -gt 0 ]]
                       		then
                               		msg="$argv0/force: stopping $1 ==  $p (killed)"
                               		ng_log ${PRI_INFO:-6} $argv0 "$msg"
                               		log_msg  "$msg"
                               		kill $kill_arg $p
                       		else
                              	 	log_msg "$argv0/force: would stop $1 ==  $p (kill $kill_arg)"
                       		fi
			done
               	fi
       	else
               	log_msg "not running: $1"
       	fi
}

