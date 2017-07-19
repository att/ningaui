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
#!/ningaui/bin/ksh
#!/ningaui/bin/ksh
# Mnemonic:	rm_arena
# Abstract:	perform simple arena management tasks
#		
# Date:		11 June 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function master_log
{
	if (( $quiet < 1 ))
	then
		ng_log $1 $argv0 "$2"
	fi
}

# attempt a command; return 0 if it was ok. write error message and set error_state for easier testing
# after executing several try commands
function tryit
{
	if (( $forreal < 1 ))
	then
		log_msg "no execute mode, not run: $@"
	else
		if ! "$@"
		then
			if (( $force > 0 ))
			then
				log_msg "force mode: error ignored"
			else
				msg="unable to properly initialise the repmgr arena in $arenad_prod"
				log_msg "$msg"
				master_log $PRI_CRIT "$msg"
				error_state=1
				return 1
			fi
		fi
	fi

	return 0
}

# ensure dir ($1) exists, create if it does not create. always set owner/grp to $3/$4
function ensure
{
        typeset perm="775"
        if [[ $1 == "-p" ]]
        then
                typeset perm="$2"
                shift 2
        fi

	typeset own=${prod_user:-bad-prod-user-name}
	typeset grp=${prod_user:-bad-prod-user-name}

	while [[ -n $1 ]]
	do
		typeset dir=$1
		
		if [[ ! -d $dir ]]
		then
			ensure ${dir%/*}			# ensure parent is there too
			log_msg "rm_arena: creating: $dir"
			if ! tryit mkdir $dir
			then
				master_log $PRI_CRIT "cannot make directory: $dir"
				log_msg "rm_arena: cannot make dir: $dir [FAIL]"
				return 1
			fi
		else
			if (( $verbose > 0  ))
			then
				log_msg "rm_arena: directory existed: $dir [OK]"
			fi
		fi
	
	
		error_state=0
		tryit chown $own:$grp $dir #2>/dev/null		
		tryit chmod $perm $dir

		if (( $error_state > 0 ))		# one of the tryits failed
		then
			log_msg "ensure: unable to chown/chmod $dir"
			return 1
		fi
	
		ls -ald $dir

		shift
	done

	return 0
}

# -------------------------------------------------------------------
ustring="[-l] [-n] [-q] [-v] {init | list | pkinit | pklist |help}"
prod_user=${NG_PROD_ID:-ningaui}
quiet=1
forreal=1
force=0
verbose=0

while [[ $1 == -* ]]
do
	case $1 in 
		-f)	force=1;;
		-l)	quiet=0;;			# ok to master log events
		-n)	forreal=0;;
		-q)	quiet=1;;

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

arenad_prod=$NG_ROOT/site/arena		# final production name
arenad=$NG_ROOT/site/arena.new		# initial spot to make so as not to interrupt things too much
	
task=${1:-list}

case $task in
	init)			# initialise the $NG_ROOT/site/arena directory
		if (( $force < 1 )) && ! amIreally ${prod_user}
		then
			if ! amIreally root
			then
				log_msg "arena initialisation must be run using the $prod_user user account"
				cleanup 1
			fi
		fi

		rm -fr $arenad				# start from scratch each time
		mkdir $arenad
		found_fs=0				# we assume that a node with $NG_ROOT/fsxx filesystems does not have dirs
		search_pat="$NG_ROOT/fs[0-9][0-9]|$NG_ROOT/rmfs[0-9][0-9][0-9][0-9]|$NG_ROOT/rmfs[0-9][0-9][0-9][0-9]"
		for i in $(ng_df | gre "$search_pat" | awk '{ print $NF }')
		do
			verbose "found filesystem: $i"
			found_fs=1				# mark even if we dont include because of errors
			if ! tryit touch $i/.arena.test	
			then
				rm -f $i/.arena.test
				msg="$i not included in arena: directory not writable"
				master_log $PRI_ERROR "$msg"
				log_msg "$msg"
			else
				rm -f $i/.arena.test
				if ensure $i/00 $i/00/00		# must be at least one high level directory and one second level dir
				then					# spaceman (driven by rota) will make others 
					echo $i >$arenad/${i##*/}		# we now use full names, not just 00, 01 as we used to 
				else
					msg="error: $i not included in arena due to errors"
					master_log $PRI_ERROR "$msg"
					log_msg "$msg"
				fi
			fi
		
			if ng_test_nattr Satellite
			then
				ensure $i/hoard
			fi
		done
		
		# probe for things mounted in NG_LOCAL_MTDIR with symlinks, or directories if we allow directories
		verbose "probing for $NG_ROOT/[rm]fs directories"
		for i in $(ls -d $NG_ROOT/fs* $NG_ROOT/rmfs* 2>/dev/null)
		do
			if [[ -f $i/MOUNT_POINT ]]
			then
				log_msg "directory is a mountpoint with missing mount: $i"
			else
				if [[ -L $i ]] ||  (( ${NG_ARENA_ALLOW_DIR:-0} > 0 ))
				then
					verbose "found directory/symlink: $i"
					if ensure $i/00 $i/00/00		# must be at least one high level directory and one second level dir
					then					# spaceman (driven by rota) will make others 
						found_fs=1			# if still zero after this we turn off flist send
						echo $i >$arenad/${i##*/}		# we now use full names, not just 00, 01 as we used to 
						else
						msg="error: $i not included in arena due to errors"
						master_log $PRI_ERROR "$msg"
						log_msg "$msg"
					fi

					echo $i >$arenad/${i##*/}		# we now use full names, not just 00, 01 as we used to 
				else
					log_msg "ignored: directories not allowed for repmgr arena components: $i"
				fi
			fi
		done
		
		if (( $found_fs < 1  ))
		then
			msg="did not find any $NG_ROOT/fsxx filsystems/directories; disabling ng_flist_send with a goaway nox file"
			master_log $PRI_WARN  "$msg"
			log_msg "WARNING: $msg"
			tryit ng_goaway -X ng_flist_send
		else
			if ng_goaway -X -c ng_flist_send
			then
				#msg="ng_flist_send is disabled with a goaway file; did not change it."
				#master_log $PRI_WARN site_prep "$msg"
				#log_msg "WARNING: $msg"
				log_msg "cleared goaway for flist_send"
				ng_goaway -r -X ng_flist_send
			fi
		fi

		error_state=0				# clear, we dont care if ng_goaway failed earlier
		tryit chown $prod_user:$prod_user $arenad $arenad/*
		tryit chmod 775 $arenad
		tryit chmod 664 $arenad/*
		if (( $error_state > 0 ))
		then
			log_msg "rm_arena: abort: unable to set permissions/owner on new diretory: $arenad"
			cleanup 1
		fi

		if (( forreal < 1 ))
		then
			log_msg "no execute mode (-n): did not move new directory ($arenad) to production location ($arenad_prod)"
			cleanup 0
		fi

		rm -fr $arenad_prod-
		if [[ -d $arenad_prod ]] && ! ng_rename $arenad_prod $arenad_prod-
		then
			log_msg "could not move old arena directory ($arenad_prod)"
			cleanup 1
		fi

		if ! ng_rename 	$arenad $arenad_prod
		then
			log_msg "could not move new ($arenad) to prodcution ($arenad_prod)" 
			cleanup 1
		fi
		;;

	list)
		ng_rm_arena_list
		;;

	help)
		usage
		cleanup 1
		;;

	pkinit)				# initalise the site/pkarena directory
		arenad=$NG_ROOT/site/pkarena
		if mkdir $arenad.new
		then
			for p in $(ls -d $NG_ROOT/pk[0-9]* $NG_ROOT/pkfs[0-9]* 2>/dev/null)
			do
				if [[ -d $p/incoming && -f $p/fs.rm && -f $p/fs.md5 ]]
				then
					verbose "adding $p to pkarena"
					echo "${p}" >$arenad.new/${p##*/}
				else
					log_msg "skipped: $p: missing incoming and/or fs files"
				fi
			done
		else
			log_msg "unable to make new pkarena directory: $arenad.new"
			cleanup 1
		fi

		error_state=0						# set if any of the tryits fails
		alist="$(ls $arenad.new/* 2>/dev/null)"			# it could be empty, this prevents false errors from tryit cmds
		tryit chown $prod_user:$prod_user $arenad.new $alist
		tryit chmod 775 $arenad.new
		if [[ -n $alist ]]
		then
			tryit chmod 664 $alist
		fi
		if (( ${error_state:-0} > 0 ))
		then
			log_msg "could not set mode/owner for $arenad.new and/or its contents"
			cleanup 1
		fi

		if (( $forreal < 1 ))
		then
			log_msg "no execute mode set (-n); pkarena contents were not installed:"
			ls -al $arenad.new
			rm -fr $arenad.new
			cleanup 0
		fi

		rm -fr $arenad-
		if [[ -d $arenad ]] && ! ng_rename $arenad $arenad-
		then
			log_msg "could not move old prod dir ($arenad) out of the way"
			cleanup 1
		fi

		if ! ng_rename $arenad.new $arenad
		then
			log_msg "could not move new ($arenad.new) to production $(arenad)"
			cleanup 1
		fi
		;;

	pklist)				# list parking lots on this node
		if [[ -d $NG_ROOT/site/pkarena ]]
		then
			for f in $NG_ROOT/site/pkarena/*		# assume each file in the directory has full path as first line
			do
				head -1 $f
			done
		fi
		;;
esac

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_arena:Manage the repmgr arena filesystems)

&space
&synop	ng_rm_arena [-l] [-n] [-q] {init | list | pkinit | pklist | help}

&space
&desc	&This is used to manage the arena (NG_ROOT/site/arena) which defines the filesytems or directories
	that replication manager uses.

&space
&subcat Initialisation
	One of the primary tasks that &this can do is to initialise the arena directory.  &lit(NG_ROOT/site/arena)
	is a directory which contains one file per replication manager filesystem.  The directory is generally created
	at node start time, &lit(ng_site_prep) invokes &this script, but it can be reiniitalised at any time.  The 
	initialisation process causes all mounted filesystems to be examined for either &lit($NG_ROOT/fs??) or &lit($NG_ROOT/rmfs????)
	filesystems. Filesystems that are currently mounted are then listed in the arena directory which then becomes
	offical list of active replication manager filesystems. 
&space
	If no &lit(fs) or &lit(rmfs) filesystems are found, and the pinkpages variable &lit(NG_ARENA_ALLOW_DIR) is set and 
	greater than zero, &this will look to see if there are any directories under &lit(NG_ROOT) that have the &lit(fs??) or
	&lit(rmfs????) name syntax.  If &this finds directories that match this syntax, they are listed in the arena. 
	Under most circumstances, replication manager file space should be mounted file systems, however it is necessary 
	to use directories instead of filesystems, this mechanism allows for that with out the risk of using the mountpoint 
	directory for files should a mount fail or be lost. 

	
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f : Force.  Depending on the task it may have different meanings. Generally it causes errors to be ignored,
	and if the task requires that it be executed using the production user account (usually ningaui), this option might
	drop that restriction.
&space
&term 	-l : Allow logging to master log.  If not supplied, log messages are written only to the tty device.
&space
&term	-n : No execute mode.  &This will go through the motions, but will not acutally put anything into place. 
&space
&term	-q : Quiet mode.  Does not allow messages to be logged to the master log (default).
&endterms


&space
&parms	&This exepects a command token as the only positional parameter on the command line. The token must be one of the 
	following:
&space
&begterms
&term	init : Initalise the arena.  This is automatically invoked at node start by &lit(ng_site_prep,) but may be invoked
	manually at anytime to refresh the arena definitions without having to stop and restart the node. 
	This task must be executed using the prodcution user ID (ningaui). 
&space
&term	list : List all of the filesystems currently defined in the arena. 
&space
&term	pkinit : Initialise the parking lot arena directory (NG_ROOT/site/pkarena).
&space
&term	pklist : List the parking lot filesytems/directories that are currently 'installed' in the parking lot arena. 
&space
&term	help : Might print some useful information to the standard output or error device. 
&endterms

&space
&exit	An exit code that is not zero indicates an error.  Where possible, system or other error messages are written 
	to the standard error device before an error exit is taken. 

&space
&see	ng_repmgr, ng_repmgrbt, ng_rmreq, ng_rmbtreq, ng_rm_btstat, ng_rm_register, ng_rm_where, ng_spaceman, ng_spaceman_nm

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	11 Jun 2007 (sd) : First try.
&term	12 Jun 2007 (sd) : If we have errors on a filesystem setup it is dropped from the arena list and we continue. 
&term	03 Aug 2007 (sd) : Altered to allow rmfsXXX or rmfsXXXX names.
&term	16 Oct 2007 (sd) : Changed warning message to read more clearly.
&term	02 Nov 2007 (sd) : Updated references in man page, and added check to allow this to be run as either the production 
		user or as root.
&term	29 Feb 2008 (sd) : Fixed problem with msg= assignment (had spaces).
&term	01 Mar 2008 (sd) : Clears goaway for flist_send if it finds filesystems.
&term	05 Mar 2008 (sd) : Fixed test to see if goaway was set. 
&term	29 Apr 2008 (sd) : Fixed so that it will work with /l mounted filesytems.
&term	09 May 2008 (sd) : Mod to allow rmfs0000b named filesystems. 
&term	02 Jun 2008 (sd) : Added initial support to manage parking lot filesystems with repmgr. 
&term	17 Jun 2008 (sd) : We now need to support both symlinks and mounts in NG_ROOT.
&endterms


&scd_end
------------------------------------------------------------------ */

