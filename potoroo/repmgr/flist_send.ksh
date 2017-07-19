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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function set_tmp
{
	typeset used=""
	typeset msg=""

	ng_df -h $TMP | awk '{printf( "%d\n", $6 )}'| read used

	if (( ${used:-100} > 95 ))		# um, trouble, check /tmp and use if its not too high
	then
		ng_df -h /tmp | awk '{printf( "%d\n", $6 )}'| read used
		if (( ${used:-100} < 90 ))
		then
			msg="TMP is too full ($used%); switching to /tmp"
			ng_log $PRI_INFO $argv0 "$msg"
			verbose "$msg"
			TMP=/tmp
		fi
	fi
}

# ------------------------------------------------------------------------------
#
ustring='[-v]'
test=0
verbose=0
testing=0


while [[ $1 = -* ]]    
do
	case $1 in 
	-t)	testing=1;;
	-v)	verbose=1; verb=-v;;

	--man)	show_man; cleanup 1;;
	-\?)	usage; cleanup 1;;
	*)		# invalid option - echo usage message 
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac				# end switch on next option character

	shift 
done				

set_tmp			# we change TMP to /tmp if $TMP is too full
me=`ng_sysname -s`
now=`ng_dt -i 2>>$NG_ROOT/site/log/flist_send.dt_err`
if [[ -z $now ]]
then
	echo "`date` ng_dt result was empty; retry in 1 sec" >>$NG_ROOT/site/log/flist_send.dt_err
	sleep 1
	now=`ng_dt -i 2>>$NG_ROOT/site/log/flist_send.dt_err`
fi

if [[ -z $now || -z $me ]]
then
	msg="unable to get sysname($me) or date($now) for filename"
	ng_log $PRI_ERROR $argv0 "$msg"
	log_msg "abort: $msg"
	cleanup 1 
fi

flist=flist.$me.$now			# basename where flist will be put
tmp=$TMP/$flist.$$			# where we build local copy (cleanup will erase in $TMP with $$ suffix)

ldr=${srv_Filer:-no_filer}		# where we think repmgr is running, filer takes preference if set

ng_get_nattr -c |read mynattrs 	 	# get nattrs, comma separated

# the redirect in from dev/null seems to help when rm_db is a gdbm based app
(
	set -e
	# node name
	echo $me
	# version
	echo 1
	# expiry timestamp
	#( echo `ng_dt -u` $RM_NODE_LEASE ) | awk '{ printf("ts=%.0f lease=%.0f\n",  $1, $1 + 3600*$2) }'
	echo $RM_NODE_LEASE | awk '{ x = systime(); printf("ts=%.0f lease=%.0f\n",  x, x + 3600*$1) }'
	# idle?
	bad=0
	case `ng_wreq explain RM_DELETE 2>/dev/null | tail -1` in
	*all=0*)	;;
	*all=*)		bad=1;;
	*)		;;
	esac
	case `ng_wreq explain RM_DEDUP 2>/dev/null | tail -1` in
	*all=0*)	;;
	*all=*)		bad=1;;
	*)		;;
	esac
	if [[ $bad -eq 0 ]]
	then
		echo idle=delete
	fi
	# free space and high level dirs
	d=0
	for i in `ng_get_nattr`
	do
		case $i in
		Tape)		dd=50;;
		Macropod)	dd=75;;
		Filer)		dd=25;;
		Catalogue)	dd=70;;
		Relay)		dd=100;;
		*)		dd=0;;
		esac
		if [[ $dd -gt $d ]]
		then
			d=$dd
		fi
	done
	case "$NG_SUM_FS_DISCOUNT" in
	?*)	args="$NG_SUM_FS_DISCOUNT";;
	*)	args="-d $d";;
	esac

	if (( ${NG_RM_NATTR_OK:-1} > 0 ))	# old versions of repmgr dont support this, so for now we must test
	then
		echo "nattr=$mynattrs"
	fi

	ng_sumfs $args
	# file list
	echo "instances=below"
	set -o pipefail
	ng_rm_db -p | awk -v cf=$TMP/count.$$ '{ count++; print; } END { print count+0 >cf; }'  
)</dev/null > $tmp
# NO commands between above block and if please
if [[ $? -gt 0 || ! -s $tmp  ]]
then
	log_msg "flist_send: abort: unable to create flist in $tmp"
	cleanup 1
fi

head -1 $TMP/count.$$ | read fcount		# number of files we read from rm_db and think we wrote to the file

if (( $fcount == 0 ))				# if fcount is 0, then this is an error unless the ckpt file is also 0
then
	if [[  -s $NG_ROOT/site/rm/rm_dbd.ckpt ]]
	then
		log_msg "flist_send: abort: flie list from ng_rm_db was 0, but the checkpoint file is non-empty"
		cleanup 1
	fi

	verbose "0 files listed by rm_dbd; acceptable as rm_dbd.ckpt is also zero in size [OK]" 
fi

awk ' 						# count the file info recs that actually made it to $tmp
	/instances=below/ { snarf = 1; next; }
	snarf > 0 { count++; }
	END { print count; }
'<$tmp | read flcount			

if (( ${flcount:-0} != ${fcount:-0} ))			
then			
	log_msg "flist_send: abort: file list seems to have been truncated; expected $fcount found $flcount"
	log_msg "`ls -al $tmp`"
	ng_log $PRI_ERROR $argv0 "file list appears truncated: $fcount(expected) !=  $flcount(actual)"
	cleanup 2
else
	verbose "file list appears valid: $fcount(expected) ==  $flcount(actual)"
fi

ng_rcmd -t 45 -c 30 $ldr 'ng_wrapper eval echo \$TMP' | read ddir
if [[ -z $ddir ]]
then
	log_msg "attempt to get TMP on $ldr failed; pausing, then 1 retry"
	sleep 15
	ng_rcmd -v -v -v -c 30 $ldr 'ng_wrapper eval echo \$TMP' | read ddir
	if [[ -z $ddir ]]
	then
		log_msg "abort: did not get TMP name on $ldr"
		cleanup 1
	fi
fi
	
ddir=$ddir/repmgr				# we assume that site_prep creates $TMP/repmgr for things like these

if [[ $testing -gt 0 ]]
then
	log_msg "testing; nothing sent"
	log_msg "would execute: ng_ccp -s -d $ddir/$flist $ldr $tmp"
	log_msg "would execute: ng_wreq -s $ldr job: pri 75 RCV_FLIST cmd ng_rm_rcv_flist $ddir/$flist"
	head $tmp
	rm -f $tmp
	cleanup -k 0
fi

#if ! ng_ccp -s -d $tmp.x $ldr $tmp
if ! ng_ccp -s -d $ddir/$flist $ldr $tmp
then
	log_msg "abort: cannot ccp file ($tmp) to $ldr:$ddir/$flist"
	cleanup 1
fi

ng_wreq -s $ldr "job: pri 75 RCV_FLIST cmd ng_rm_rcv_flist $ddir/$flist"

rm -f $tmp

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_flist_send:Send Repmgr Filelist)
&name &utitle

&space
&synop  ng_flist_send [-v]


&space
&desc   &This will send a current copy of the local Replication manager file
	list to the node that is currently running Replication manager. 
	The file list contains current filesystem information, including 
	free space, and the node's atrributest. The &lit(NG_RM_NATTR_OK) pinkpages 
	variable can be set to zero to prevent attributes from being sent
	(necessary only if the replication manager is very old).

&exit
	Any exit code that is not zero indicates an error and should be 
	accompanied with an error message to standard error.

&space

&space
&see    ng_repmgr ng_repmgrbt ng_rmreq ng_rm_db
&mods

&owner(Andrew Hume)
&lgterms
&term	12 May 2003 (sd) : Modified to cleanup if ccp fails.
&term	29 May 2003 (sd) : Rearranged set -e to subshell block, and added status
	check of subshell in order to clean away the tmp file if we abort.
&term	07 Jan 2004 (sd) : Will check the variable SUM_FS_DISCOUNT and set it as the 
	discount amount.  Var must be set using '-d percent'. Will auto set a discount 
	of 20 percent if the Netif or Tape node attribute is on.
&term	05 May 2004 (sd) : Now send node attributes as nattr=data. Data are attribute
	tokens generated by n_get_nattr.  Tokens are comma separated. Added manpage.
&term	10 May 2004 (sd) : Corrected the reduction of mutiple blanks -- shell magic 
	did not work. Converted $TMP usage such that flists are sent to TMP/repmgr
	and it will also work on nodes where $TMP is not the same as on the filer. 
&term	19 Jun 2004 (sd) : Added some verification of the size of the file we send to 
	prevent sending truncated files.
&term	12 Aug 2004 (sd) : Changed discount to 25%; now applies only to tape nodes.
&term	19 Oct 2004 (sd) : pulled the ng_leader reference (was already commented out)
&term	09 Nov 2004 (sd) : Added check to ensure flist name was built properly.
&term	16 Mar 2005 (sd) : Added a 30 second connection timeout to the ng_rcmd call. Also
		added retry logic if ddir is empty.
&term	01 Jun 2005 (sd) : Converted date -e (ast) to ng_dt -u.
&term	23 Jun 2005 (sd) : Fixed one remaining date  -e call.
&term	03 Aug 2005 (sd) : No need to ship with a forced early exit; setup sets a goawy-nox file.
&term	05 Aug 2005 (sd) : Added a bit of debugging/retry for ng_dt command.
&term	09 May 1006 (sd) : Added test to see if TMP is too full; switches to /tmp if it is and 
		/tmp is not above 90%.
&term	22 May 2006 (sd) : Changed the way the new flist is validated to prevent truncation. 
&term	23 Jun 2006 (sd) : Added ng_log msg if switching from TMP to /tmp.
&term	10 Oct 2006 (sd) : Cleaned up comments and now default to sending the node attributes.
&term	13 Nov 2006 (sd) : Added defaults to set_tmp function arithmetical if statements.
&term	18 Dec 2006 (sd) : Added defaults to f[l]count when testing to allow for new system (no instances).
		Added --man support.
&term	20 Mar 2007 (sd) : Pulled reference to Logger.
&term	23 Apr 2007 (sd) : Pulled extra verbose on ng_rcmd call.
&term	23 May 2007 (sd) : Added catalogue to the list of attributes for which we compute a space 'discount.'
&term	11 Jun 2007 (sd) : Changed SUM_FS_DISCOUNT refrences to NG_SUM_FS_DISCOUNT.
&term	12 Jul 2007 (sd) : Fixed SUM_FS_DISCOUNT reference in the one I missed earlier.
&term	02 Feb 2009 (sd) : Added pipe fail catch and an extra check to guard against empty flists. 
&term	10 Mar 2009 (sd) : Corrected a problem that caused an expression check if file count is 0.
&term	30 Mar 2009 (sd) : Added recognition of node attribute 'Relay' which causes an automatic 'discount' of 100 
		to be applied (keeps most files from being copied to the node).
&endterms
&scd_end

