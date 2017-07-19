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
# Mnemonic:	ng_shuffle
# Abstract: 	Read a list of src/dest pairs and move them. The files are 
#		removed from the local repmgr database before the move, 
#		and the new instances are added back after the move. If the 
#		move fails, the original filename is added back to the database.
#		The input to the script is:
#		mdt size srcfile destfile
#
#		This script creates a small 'mover' script that actually does the 
#		moving. It does this as we would need to make two passes across
#		the input file if we wanted to move things directly from this 
#		script, and while that is not  a bad thing it means that its 
#		difficult to deal with input on stdin.  In order to accept input
#		on standard in I opted to create a script to do the move, 
#		rather than 'tee-ing' the input in order to make a second pass. 
# Date:		22 Apr 2002
# Author:	E. Scott Daniels
# ----------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# wait for repmgr to update its dump file -- usually indicates that it has pruned things
# and it is safe for us to continue.
function wait_for_dump
{
	ng_rm_where -r ${2:-120} `ng_sysname` dummy_file	# cause cmd on leader to wait for dump
	return
}
# -----------------------------------------------------------------------------

ustring=""
verbose=0
list=""			# input list of pairs (src/dest) to move
keeplist=1		# if unset (-p) then we will trash the list
log2tty=0

# ------------------------ -------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-f)	log2tty=1;;
		-p)	keeplist=0;;
		-v)	verbose=1; v="$v-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

keeplist=1		# TESTING

if [[ $log2tty -lt 1 ]]
then
	verbose "logging to $NG_ROOT/site/log/shuffle"
	exec >>$NG_ROOT/site/log/shuffle 2>&1
fi

# create the lead part of the move script
cat <<endcat >/tmp/script.$$
#!$NG_ROOT/bin/ksh
	# expect src dest md5 size 
	# if move is successful then we will write the dest file name for adding to repmgr
	# if move fails, then we will write the src name for adding back to repmgr
	function move
	{
		echo "mv \$1 \$2" >&2

        	if  mv \$1 \$2
        	then
			reg=\$2			# instance the destination file 
		else
			reg=\$1
                	echo "*** error moving \$1 --> \$2" >&2
                	bad=\$(( \$bad + 1 ))
        	fi
	
		echo \$3 \$reg \$4			# add something to the list to register
        	total=\$(( \$total + 1 ))
	}

	function stats
	{
        echo "\$total moves attempted; \$bad failures" >&2
	}

	bad=0
	total=0
endcat

list=$1

# the things we must do:
# 1) generate a list of old files; pump into ng_rm_db -d (delete)
# 2) flist_send - update repmgr (wait for new dump or our changes will not take)
# 3) move files creating an add list 
# 4) pump add list into ng_rm_db -a 
# 5) flist_send - update repmgr

>/tmp/del.$$				# list of files to delete from rmdb

# input from spaceadj is: md5 size src-file dest-file
if awk \
	-v delf=/tmp/del.$$ \
	-v script=/tmp/script.$$ \
	'
	{
		printf( "%s %s\n", $1, $3 ) >delf;				# delete information (md5 pathname)
		printf( "move %s %s %s %s\n", $3, $4, $1, $2 ) >>script;	# script to run to move things (src, dest, md5, size)
	}
	END {
		printf( "stats\n" ) >>script;	# causes some stats to go to the stderr at end of script
	}
	'<$list
then
	cp /tmp/del.$$ $TMP/shuffle.last_del		# always nice to know what we last did
	cp /tmp/script.$$ $TMP/shuffle.last_script
	>$TMP/shuffle.last_add				# coppied later, but must zero now

	if [[ -s /tmp/del.$$ ]]
	then
		verbose "deleting instances"
		ng_rm_db -d </tmp/del.$$
		ng_flist_send
		wait_for_dump $NG_ROOT/site/rm/dump &		# seems there is some latency after delete before we can safely add
		wfd_pid=$!					# hold this for wait after moving things
		
		verbose "moving files"				# we can safely start moving things as we wait for the dump
		$NG_ROOT/bin/ksh /tmp/script.$$ >/tmp/add.$$	# creates list to be added to rmdb (dest of successful moves, src of failed moves)
		cp /tmp/add.$$ $TMP/shuffle.last_add		

		verbose "moving done, waiting for dump to finish"
		wait $wfd_pid
		verbose "dump completed"

		if [[ -s /tmp/add.$$ ]]
		then
			verbose "adding instances"
			ng_rm_db -a </tmp/add.$$
		else
			log_msg "WARNING: add file had no size: `ls -al /tmp/add.$$`"
			keeplist=1
		fi

		ng_flist_send
	else
		log_msg "WARNING: del file had no size: `ls -al /tmp/del.$$`"
		keeplist=1
	fi

else
	log_msg "awk failure; no action taken!"
	cleanup 1
fi

if [[ -n $list && $keeplist -lt 1 ]]		# delete if not from stdin and not asked to keep
then
	verbose "removing list: $list"
	rm -f $list
else
	if [[ -n $list ]]
	then
		verbose "moving list to: $TMP/shuffle.last_list"
		mv $list $TMP/shuffle.last_list
	fi
fi

verbose "done; cleanup 0"
cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_shuffle:Shuffle files identified by spaceman)

&space
&synop	ng_shuffle <listfile

&space
&desc	&This reads from standard input source/dest information and will move 
	source files to the destination reinstancing the file in the 
	local file list.  File list sends are done at appropriate times to 
	ensure information in replication manager is accurate. 
	The information expected to be passed to &this has the following record
	syntax:
&space
&litblkb
	md5sum-value file-size src-pathname dest-pathname
&litblke
&space
	If a move fails, then the original filename is placed back into the 
	instance list.  In order to get files 'renamed' inside of &ital(replication manager)
	each file must be removed from the local file list, and that list sent 	
	to  &ital(replication manager.) Replication manager must completely remove 
	the instance from its entrails before we can send the next list. 
	We assume that the necessary updates to the &ital(replication manager) 
	internals have been made when it updates its dump file, so before we add
	the list of files, and their new locations, we must wait for the dump 
	to be refreshed.  This does mean that we are vulnerable for the time that the 
	files are "missing" because we have not added them again. The vulerability is 
	mostly limited to having extra files, in unknown places should the node crash
	before the files are added back in.  Should the node crash during this window, 
	&ital(replication manager) will eventually notice that the requested number of
	copies of the files that we are shuffling is nolonger satisified, and will attempt 
	to make new coppies.  

&space
&see	&ital(ng_spaceman) &ital(ng_spaceadj) &ital(ng_repmgr)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	22 Apr 2002 (sd) : Orig.
&term	10 May 2002 (sd) : Added some performance tweeks like starting moves while 
	waiting for the dump file update. 
&endterms

&scd_end
------------------------------------------------------------------ */
