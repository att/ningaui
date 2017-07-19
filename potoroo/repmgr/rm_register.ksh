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
# Mnemonic:	rm_register
# Abstract: 	Interface to replication manager to make registeration/unregistration
#		of a file easy.
#		in general we try to exit with an error code that indicates the number of 
#		files that could not be registered.  If we abort because of parm errors
#		or similar we cleanup with a code of 99.
# Date:		5 September 2001  (hbd KZ)
# Author:	E. Scott Daniels
# --------------------------------------------------------------------------------------


function ck_info
{
	typeset msg=""

	if [[ $force -gt 0 ]]
	then
		verbose "force mode: file info check skipped"
		return 0
	fi

	if [[ $# -lt 3 ]]
	then
		log_msg "missing file stats; expected md5, file, size, got ($*)"
		return 1
	fi

	if [[ -f $2 ]]	# file exists
	then
		if [[ ${#1} = 32 ]]		# md5 has good length
		then
			if [[ $3 -ge 0 ]]	# good (pos/zero) file size
			then
				verbose "all stats pass check: $1(md5) $2(file) $3(size)"
				return 0	# all pass; return good
			else
				msg="file size seems bad"
			fi
		else
			msg="MD5 value too short/long"
		fi
	else
		msg="cannot find file"
	fi

	log_msg "all stats do NOT pass check: $msg: $1(md5) $2(file) $3(size)"
	return 1
}

# attempt to dot in (source) filename.  We exit after $1 attempts tries to load $2 (filename)
function import
{
	typeset try=$1
	shift

	while !  command . ${1:-$NG_ROOT/cartulary} 2>/dev/null
	do
		try=$(( $try - 1 ))
		if [[ $try -gt 0 ]]
		then
			ng_log $PRI_INFO $argv0 "read of cartulary required retry" 
			sleep 1
		else
			ng_log $PRI_ERROR $argv0 "could not read  cartulary" 
			exit 5
		fi
	done

	return 0
}

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
#. $CARTULARY
import 3 $CARTULARY			# make three attempts to import the cartulary

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function log_it
{
	log_msg "error: $@"
	ng_log $PRI_WARN ${argv0##*/} "$@"
	errors=$(( errors + 1 ))
}

# sends the registration commands to repmgr with a small retry loop
# returns 1 (bad) if we never get it in
function send2rm
{
        typeset tries=$retries           # tries after the initial attempt
        typeset naptime=15              # initial nap time
	typeset rmcmds=$1

	while ! ng_rmreq -s $filer <$rmcmds
        do
		verbose=1
                tries=$(( $tries - 1 ))
                if [[ $tries -le 0 ]]                           # end of our patience -- log error and get out
                then
                	log_msg "${argv0##*/}: repmgr communication error; max retries attempted; giving up"   # msg to stderr
			return 1
                fi

                log_msg "${argv0##*/}: repmgr communication error; retry in $naptime seconds"   # msg to stderr
                sleep $naptime
                naptime=$(( int($naptime * 2.5) ))              # next time, if another failure, sleep longer by 2.5 times
        done

        return 0
}


# registers all files on the command line. Sets errors to the number of files that could not be registered
# we do this by creating a list of hood/hint and instance commands, then sending that list to repmgr.
function register
{
	typeset fname=""
	typeset fdname=""			# directory that the file lives in
	typeset rmcmds=/tmp/rmcmds.$$

	while [[ -n $1 ]]			# for each file passed in
	do
		>$rmcmds			# dont pick up things from last go round

		if [[ $1 != /* ]]			# not fully qualified
		then
			fname=`pwd`"/$1"		# we must assume it is here
		else
			fname="$1"
		fi
		publish_file -e $fname|read fname	# strip any !/~ that is trailing on the filename and rename on disk
		if (( $emit_pubname > 0 ))		# must echo the published name to stdout 
		then
			echo $fname
		fi

		fdname=${fname%/*}			# directory name

		if [[ -r $fname && -f $fname ]]			# it does exist and is a regular file (no directories/links)
		then
			if [[ -n $NG_RM_MODE ]]
			then
				verbose "setting mode: $NG_RM_MODE $fname"
				chmod $NG_RM_MODE $fname			# not a fatal error if it fails
			fi

			# generate two tuples of md5 and path. one for the file command and one for the instance command
			# they will differ when user supplies md5 and sets the verify user md5 flag
			file_size=$( ng_rm_size -l $fname )

			if [[ -n $user_md5 ]]			
			then
				if (( $verify_usermd5 > 0 ))			# we are given the md5 only to put it into the file command
				then						# and are expected to compute the files md5 for the instance cmd
					verbose "user supplied md5 value will be used only on the registration"
					verbose "computing md5 value to be used just for the instance"
					instance_stuff="$(md5sum $fname)"  	# use value computed locally for instance
					file_stuff="$user_md5 $fname"		# use users md5 value for the file registration
				else
					verbose "using user supplied md5 value for both file and instance"
					instance_stuff="$user_md5 $fname"		# for use on instance command
					file_stuff="$instance_stuff"
				fi
			else
				verbose "calculating md5 value for both registration and the instance"
				instance_stuff="$(md5sum $fname)"  	# use value computed locally for instance
				file_stuff="$instance_stuff"
			fi

			
			if ! ck_info $file_stuff $file_size
			then
				log_msg "stats did not pass check; $filename not registered"
				errors=$(( $errors + 1 ))
			else
				
				verbose "file: $file_stuff $file_size"
				echo "file $file_stuff $file_size $count" >>$rmcmds		# causes it to register in repmgr
	
				if [[ -n $hint ]]					# hint given
				then
					verbose "hint: $file_stuff ${lease:-86400} $hint"	# hint md5 filanme noccur hint
					echo " hint $file_stuff ${lease:-86400} $hint" >>$rmcmds
				fi
			
				if [[ -n $hood ]]
				then
					verbose "hood: $file_stuff ${hood#,}"			# hood md5 filename filesize target
					echo "hood $file_stuff ${hood#,}" >>$rmcmds		# ensure lead comma is removed from hood list
				fi
	
				verbose	"flist_add: $instance_stuff"
				if  echo "$instance_stuff" | ng_flist_add $v		# add instance command to rmreq input
				then
					verbose "registering file: $file_stuff $count"
					if send2rm $rmcmds 
					then
						verbose "registered: $file_stuff $file_size $count"
					else
						log_it "registration failed: $file_stuff"  	#  log & inc err count 
						echo $flist_stuff | ng_flist_del $v		# trash from the local database 
					fi
				else
					log_it "flist_add failed: $instance_stuff $file_size"
				fi
			fi
		else
			log_msg "cannot find/read file, or file is a directory or link: $fname not registered"
			errors=$(( $errors + 1 ))
		fi

		shift
	done
}


#	look up basename($1) and unregister it if it exists
function unregister
{
	typeset target=""
	typeset node=""
	typeset path=""

	while [[ -n $1 ]]
	do
		target=${1##*/}

		ng_rm_where -n -s -5 -S $target | read fname nodes md5 size junk
		if [[ -n $fname && $fname != "MISSING" ]]
		then
			verbose "unregister: $md5 $fname $size 0"
			echo "file ${md5:-junk} ${fname:-junk} ${size:-0} 0" >/tmp/rmcmds.$$
			if send2rm /tmp/rmcmds.$$
			then
				verbose "unregistered: $fname"
			else
				log_it "unregister failed: $fname"		# counts errors for bad return at cleanup
			fi
		else
			log_msg "file not known to repmgr, not unregistered: $1"
		fi

		shift
	done
}

# --------------------------------------------------------------------

ustring="[-c count] [-C cksum] [-d] [-e] [-E] [-f] [-H hood-node-list] [-h node[,node1,...noden] [-u] [-v] file [file...]"
tf=/tmp/stuff.$$
unreg=0
count="-2"		# number of replicaitons (-2 is default)
verbose=0
replace=0		# set to 1 to cause use to look for the file first and delete it if it exists 
force=0			# force unregster, or skip file stats check (register)
rc=0
hint=""			# hint for the file
hood=""			# neighbourhood list for the file
incleader=0		# DEPRECATED; kept for back compat (see -l)
thishost=`ng_sysname`
user_md5=""
retries=3		# number of times we reattempt to send the registration
errors=0		# number of files that were not registered
emit_pubname=0		# -E sets to cause the published name (could be different) to be emitted to stdout
verify_usermd5=0	# if user gives md5 it is not verified (recomputed) unless -M is given which sets this

filer="${srv_Filer:-no_filer}";		# node where repmgr runs (srv_Filer)
if [[ -z $filer ]]
then
	log_it "unable to determin repmgr host"
	cleanup 99
fi

while [[ $1 = -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 99;;
		-c)	count="$2"; shift;;	# number of copies other than default
		-c*)	count="${1#-?}";;

		-C)	user_md5=$2; shift;;	# user supplied md5sum -- we will not recalc
		-C*)	user_md5="${1#-?}";;
						# setting the instance with what we calculate.

		-d)	;;			# allow user to specify "default"
		-e)	count="-1";;		# copy on every node
		-E)	emit_pubname=1;;	# eccho published filename to stdout 
		-f)	force=1;;		# causes unregister from anywhere

		-h)	hint="$2"; shift;;
 		-h*)    hint="${1#-?}";;		# register a hint

		-H)	hood="$2"; shift;;
 		-H*)    hood="${1#-?}";;		# specified hood 


		-l)	count="-3";;		# use the larg default (RM_LARGENREPS)

		-L)	lease="$2"; shift;;
 		-L*)    lease="${1#-?}";;		# lease hint

		-r)	replace=1;;			# replace file by deleting it first
		-u)	unreg=1; count=0;;		# unregister
		-V)	verify_usermd5=1;;	# force verification of user md5 by registering with user supplied md5 and 

		-v) 	verbose=1; v="-v";;
		-\?)	usage; cleanup 99;;
		*)	echo "unknown: $1"; usage; cleanup 99;;
	esac

	shift
done


if [[ $# -lt 1 ]]
then
	usage
	cleanup 99
fi

verbose "rm_register started: $@ $hood(hood) $hint(hint) $count(count)"

if [[ $unreg -gt 0 ]]
then
	unregister "$@"
else
	if [[ $replace -gt 0 ]]			# this is a stretch, almost a kluge,
	then
		unregister "$@"			# send them all packing
		for x in "$@"
		do
			x=${x##*/}
			verbose "waiting for removal of: $x"

			ng_rm_where -n $x|read file nodes
			while [[ -n $nodes ]]
			do
				sleep 60				# these are expensive, dont do more than 1/min
				ng_rm_where -n $x|read file nodes
			done
		done
	fi

	register "$@"
fi

cleanup $errors				# number of files that were not registered

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_register:Register/Unregister a file with Replication Manager)

&space
&synop	ng_rm_register [-c count | -l | -e] [-C cksum] [-d] [-E] [-f] [-H hood-node] [-h hint-node] [-r] [-u]  [-v] file [file...]

&space
&desc	&This is used to register and unregister one or more files with 
	the replication manager. When registering files, &this first 	
	adds the file to the local file list (&ital(ng_flist_add)) and 
	then schedules the file for replication across the cluster.  
	When unregistering a file (&lit(-u)) the file is scheduled for 
	deletion from remote nodes, and then removed from the local file
	list.

&space
&subcat File Mode
	If the pinkpages variable &lit(NG_RM_MODE) is set, &this will attempt
	to change the mode of the file being registered to this value. 
	This can be dangerious if the user registering the file is not 
	a member of the production ID group, or the change of the 
	mode will prevent the production user ID from reading/deleting 
	the file. 

.if [ 0 ] 
IT MAY STILL WORK THIS WAY, BUT PROPY IS LONG GONE...
&space
	If the file(s) named on the command line are currently in the 
	&lit(NG_RM_PROPYLAEUM) directory, then the files are registered with 
	the &ital(replication manager,) and marked for the propylaeum daemon
	to move into a &ital(replication manager) filesystem. These files 
	are not instanced locally until the daemon moves them.  If the file(s)
	is not in a propylaeum directory, then the file is also instanced locally
	and the file is not marked for the daemon.
.fi

&space
&opts	The following options are recognised by &this:
&begterms
&term	-c count : The number of replications that should be created for the 
	file. If not supplied, the file is replicated the default number of
	times.
&space
&term	-C cksum : By default, &this computes the 
	file's checksum and sends that to replication manger on both the file and
	instance commands.  When the -C option is used, the supplied value is 
	assumed and the md5 computation is skipped.  If the -V (verify the 
	user md5) option is used in conjunction with this option, the user supplied
	checksum is supplied only on the file (registration) command to replication
	manager. The md5 value is computed and supplied on the instance 
	command. This informs replication manager of the expected value (that 
	supplied with the -C option) and it will only attach the instance to 
	the registration if the computed value value matches the expected value.
&space
&term	-d : Default number of copies. This is the default and exists for 
	programmes that need to specify something!
&space
&term	-e : Causes the file to be replicated on every node that is a 
	member of the cluster.
&space
&term	-E : Emit published name to stdout.  As ng_spaceman_nm now adds a 
	special character to the end of generated file names, setting this 
	option causes &this to echo to the standard output device the 
	name that was actually registered (without the special character).
&space
&term	-f : Force. Causes the file to be unregistered even if it does not
	reside on the current node.  If registering a file, this flag causes
	the sanity checks on file size, md5sum and file location to be skipped.
&space
&term	-H nodes : 
	Supplies one or more nodes (comma separated) that are 
	given to the &ital(replication manager) as neighbourhood nodes (hoods). 
&space
&term	-h nodes : 
	Supplies one or more nodes (comma separated) that are 
	given to the &ital(replication manager) as hints. Nodes listed here
	will receive a copy of the file it is possible.
&space
&term	-l : Use the large default replication count (as defined by the pinkpages
	variable RM_LARGENREPS). If not defined the default replication count is 
	used unless specified with either the -c or -e options.
	If this option is used in conjunction with the -c or -e options undesired
	results may occur. 

&space
&term	-L seconds : Seconds that the lease on a hint should be set for.
&space
&term	-r : Replace. Causes &this to search the &ital(replication managemer)
	environment for the file(s) passed in, and to unreplicate any instance 
	of these files that are found before the new file is registered.
	Caution: once the list of files has been sumbitted for un-registration, 
	&this will &stress(block) until all of the files have disappeared from 
	the the replication manager's scope (no longer in the dump file). This 
	may not be a quick operation, and for applicaitons that cannot afford
	to block, then the new file should just be registered with the knowledge
	that the old copy will become an unattached instance.
&space
&term	-u : Unregister. The list of files supplied on the command line are 
	unregistered with replication manager.  There could be a delay of 
	several minutes between the time that the unregister command(s) are 
	submitted and the file(s) are actually removed from disk.
&space
	-V : Verify user supplied md5 value.  If set, and the -C option is used
	to supply the md5 value of the file this option will cause the file to 
	be registered with the supplied value, but the md5 value of the file 
	will be recomputed and the computed value of the file will be included
	on the instance command sent to replication manager. 
&space
&term	-v : Verbose mode.
&endterms

&space
&parms	Following any necessary commandline options, the user may supply one 
	or more filenames. 

&space
&exit	A return code of 0 indicates that there were no detected errors during 
	processing.  Any non-zero return code indicates that some error or 
	errors were encountered and that not all processing may have been 
	completed or attempted. In general, but not always, the exit code 
	will indicate the number of files that could not be registered. 
	An exit code of 99 is set when &this is unable to function properly 	
	because of bad command line input or other non-registration related 
	issues.
&space
	If a file is given to &this to be unregistered (-u), and the file 
	is not known to replication manger, a warning message is issued to 
	the standard error device, but does not cause an error to be incicated
	in the exit value.  

&space
&see	ng_repmgr, ng_rmreq, ng_flist_add, ng_flist_del

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Sep 2001 (sd) : original code.
&term	17 Sep 2001 (sd) : Added relative path support.
&term	26 Sep 2001 (sd) : To direct rmreq to the leader.
&term	31 Oct 2001 (sd) : Added a -d option as its easier for seneschal
	to always suppy some parameter.
&term	14 Dec 2001 (sd) : To add hint option
&term	01 Mar 2002 (sd) : To add replace option
&term	11 Apr 2002 (sd) : To add support for the propyleaum.
&term	24 Apr 2002 (sd) : To support user supplied md5 value.
&term	30 May 2002 (sd) : To support supplying a neighbourhood for the file.
&term	10 Jun 2002 (sd) : to include -s leader on hint request.
&term	29 Dec 2003 (sd) : Added retries to registration, and failure indication if flist 
	add fails.  Return code indicates number of files that were NOT registered. (0 being 
	a perfect score).
&term	22 Mar 2004 (sd) : Added import function to try to curb registration failures because 
	the cartulary was missing on relay nodes. 
&term	25 Jun 2004 (sd) : Now will do an flist_del if the rmreq command fails.
&term   18 Aug 2004 (sd) : Converted srv_Leader references to corect srv_ things.
&term	19 Oct 2004 (sd) : removed ng_leader ref.
&term	16 Nov 1004 (sd) : Added some sanity checking to ensure md5 seems right, file has good size.
&term	08 Jun 2005 (sd) : Modified to remove ! or ~ from the end of the file. Added -E which causes
	the published filename to be emitted on stdout (it could have changed if the user submitted 
	it with a trailing ~.
&term	16 Jun 2005 (sd) : Changed errors to warnings.
&term	10 Aug 2005 (sd) : Fixed bug in ungeg -- it now works.
&term	27 Oct 2006 (sd) : Fixed usage string to include -E; corrected man page. 
&term	27 Mar 2007 (sd) : Updated man page.
&term	10 Sep 2007 (sd) : Added -V to force verification of the user supplied md5 value. 
&term	27 Nov 2007 (sd) : -l now sets the large default value (RM_LARGENREPS).
&term	30 Jul 2008 (sd) : Added check for NG_RM_MODE and will set file's mode in repmgr space to this value if it is not empty. .** HBD AKD
&endterms

&scd_end
------------------------------------------------------------------ */

