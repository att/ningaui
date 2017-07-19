#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
#!/usr/bin/env ksh
# Mnemonic:	faregate
# Abstract:	provides an 'entrance gate' for files into replication manager space. 
#		similar in nature to propaleaum, but more simple and it allows for 
#		inbound file pattern specification and concurrent execution such that 
#		multiple applications can start a daemon against their landing spot.  
#		Designed to support the droppings from a video capture environment, 
#		but should be generic enough to support anything.
#
#		unlike the feeds ftpmon daemon, this script does not have the complexity
#		of managing a list of received files, or processing 'sent lists' as it assumes
#		the files are being streamed in 'live' and there is no mechanism for 
#		requesting missed data. 
#
#		in addition to moving the files into repmgr space, this script provides
#		the ability to execute a command -- assumed to be something like 
#		aether_receive that simply generates a nawab job to deal with the file.
#		
# Date:		16 March 2009 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# -------------------------------------------------------------------
ustring="[-c cmd] [-d dir] [-l eyecatcher] [-p pattern] [-R] [-r rep-count] [-w sec] [-v]"
lzd=$TMP/faregate		# the landing zone directory
fpat="."
max_per=10			# number of files to do per round
idle_wait=60			# seconds to wait if we detect an idle condition
repcount=2
vflag=""
keep=0				# keep the file in the landingzone after registration -- give it a .mvd! suffix
receipt_command=""		# command executed after file moved and registered
eyecatcher=""			# -l defines and causes us to log in master log with this eyecatcher
ignore="!"			# we will never pick a file that has this character as the last in the filename (-i changes)
register=1			# -R turns off to prevent registation; file is still coppied to repmgr space

while [[ $1 == -* ]]
do
	case $1 in 
		-c)	receipt_command="$2"; shift;;
		-d)	lzd="$2"; shift;;
		-i)	ignore="$2";;
		-k)	keep=1;;
		-l)	eyecatcher="$2"; shift;;
		-p)	fpat="$2"; shift;;
		-r)	repcount=$2; shift;;
		-R)	register=0;;
		-w)	idle_wait=$2; shift;;

		-v)	verbose=1; vflag="-v";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


if ! cd $lzd
then
	log_msg "unable to swtich to landing spot directory: $lzd"
	cleanup 1
fi

ng_goaway -r -u ${lzd##*/} ${argv0##*/}			# clear goaway from previous run 

get_tmp_nm list | read list

while true
do
	count=0						# assume idle if we dont process anything
	ls -tr | gre "$fpat" |gre -v ".*$ignore" | head -$max_per >$list	# get a few to chew on
	while read f
	do
		if ng_goaway -c -u ${lzd##*/} ${argv0##*/}
		then
			# DONT remove the goaway file -- if for some reason someone started multiple faregates, we leave it for the others to notice
			log_msg "exiting: found goaway file (${lzd##*/})"
			cleanup 0
		fi

		rfn=$( ng_spaceman_nm $f )
		log_msg "process: $f --> $rfn"
		count=$(( $count + 1 ))

		if cp $f $rfn
		then
			verbose "file copy to repmgr space was successful [OK]"
			if (( $register == 0 )) || ng_rm_register $vflag -c $repcount $rfn
			then
				verbose "registration (if requested) was successful [OK]"
				dispose=1
				if [[ -n $receipt_command ]]
				then
					if ${receipt_command//%s/$rfn}
					then 
						verbose "receipt command successful: ${receipt_command//%s/$rfn} [OK]"
					else
						dispose=0
						log_msg "processing of receipt command failed: '$receipt_command' [FAIL]"
						# we could unregister the file in repmgr space, but seems harmelss to leave it
					fi
				fi

				if (( $dispose > 0 ))				# processing ok, dispose by removal or rename to avoid processing again
				then
					if [[ -n $eyecatcher ]]
					then
						ng_log $PRI_INFO $argv0 "$eyecatcher processing complete: $f --> $rfn"
					fi
					if (( $keep > 0 ))
					then
						mv $f $f.mvd$ignore
					else
						rm -f $f
					fi
				fi
			else
				log_msg "unable to register the file: $rfn [FAIL]"
				rm -f $rfn
			fi
		else
			log_msg "unable to move $f to repmgr space: $rfn [FAIL]"
		fi
	done <$list

	

	# if we have any housekeeping things to do between batches, do them here



	if (( count > 0 ))					# go right round if we were not empty
	then
		verbose "processed $count files"
	else
		#verbose "idle -- pausing before retry"
		sleep $idle_wait
		if ng_goaway -c -u ${lzd##*/} ${argv0##*/}
		then
			# DONT remove the goaway file -- if for some reason someone started multiple faregates, we leave it for the others to notice
			#ng_goaway -r -u ${lzd##*/} ${argv0##*/}
			log_msg "exiting: found goaway file (${lzd##*/})"
			cleanup 0
		fi
	fi
done


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_faregate:Provide an entrance to repmgr space for realtime streaming files)

&space
&synop	ng_faregate [-c receipt-command] [-d dir] [-i ignore] [-k] [-l eyecatcher] [-p pat] [-R] [-r count] [-w sec] [-v]

&space
&desc	&This watches the directory supplied on the command line, or &lit($TMP/faregate) by default, 
	for files that match the supplied pattern.  When files are noticed, they are moved to replication 
	manager space and registered. If a receipt command is specified, the command is executed. 
	A log message is generated in the masterlog only if the &lit(-l) option is supplied with an 
	eyecatcher. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c receipt-cmd : Supplies the command string that is executed following a successful file 
	registration.  The string may contain the pattern %s which will be replaced with the 
	fullyqualified pathname of the file in replication manager space. As an example:
&space
&litblkb
  aether_rcv -v %s
&litblke

&space
&term	-d dir : Supplies the pathname of the directory to watch.  If not supplied, the directory
	^lit($TMP/faregate) is assumed to exist and will be monitored. 
&space
&term	-i ch : Provides the ignore character. Filenames ending with this character will not be selected 
	for processing. By default, this character is a bang (!).  The assumption is that the process(es) that 
	create files in the directory do so using a filename that has a trailing ignore character, and then move 
	the file to the proper filename once the file is closed and complete.  There are other methods of doing 
	this; the ignore character is what &this directly supports. 
&space
&term	-k : Files in the directory are not deleted after successfully being registered with replication manager, but are
	renamed to have a &lit(.mvd!) suffix. 
&space
&term	-p pat : A gre/grep pattern that will be used to match files that are ready to be moved. It is assumed
	that processes that are creating files in the directory are doing so with a trailing bang (!) and renameing the 
	file after the file has been finished and closed.  
&space
&term	-R : No registration mode.  The file is moved to replication manager space, but is not registered.  It is assumed
	that the receipt-command processing will register the file whith options (hints/hoods) that are beyond the 
	scope of this generic tool.
&space
&term	-r count : The replication count for files registered with replication manger. If not supplied the default is 2. 
&space
&term	-w sec : The number of seconds to pause if no files are found. The default it 60s. 
&space
&term	-v : Verbose mode. Causes &this to be chatty on stderr.

&endterms


&space
&exit	An exit code that is not zero indicates an error.

&space
&see
	&seelink(repmgr:ng_repmgr)
	&seelink(repmgr:ng_rm_register)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Mar 2009 (sd) : Its beginning of time. 
&term	18 Mar 2009 (sd) : Tweek to set count and detect idle properly.
&term	20 Mar 2009 (sd) : Added the no registration (-R) option.
&term	02 Jun 2009 (sd) : We now leave the goaway file and clean them up when started. This allows multiple
	faregates to be executed in the same directory, and for all to die when the exit is given.  
&endterms


&scd_end
------------------------------------------------------------------ */

