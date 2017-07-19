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
# Mnemonic: ng_flist_add
# Author:   Andrew Hume
# Date:     May 2001
#


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN
                                   

# log the files that we had problems with 
function log_errors
{
	typeset x=""

	while read x 
	do
		log_msg "$2: $x"
		ng_log $1 $argv0 "$2: $x"
	done <$badf
}

# ------------------------------------------------------------------------

ustring='$argv0 filename md5sum'

while [[ $1 = -* ]]    
do
	case $1 in 

		--man)	show_man; cleanup 1;;
		-v)	verbose=TRUE; verb=-v;;
		-\?)	usage; cleanup 1;;
		*)	error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac				# end switch on next option character

	shift 1
done                            # end for each option character

#tmp=/tmp/fa$$_$RANDOM
tmp=$TMP/file_add.$$
vtmp=$TMP/file_add.v.$$		# validated list
etmp=$TMP/file_add.e.$$		# stderr

case $# in
	0)	cat;;				# accept md5 filename pair(s) from stdin 
	2)	echo $2 $1;;			# assume  filename md5 on command line
	*)	usage;;
esac > $tmp

#a=`ng_rm_db -p | wc -l`
a=noinfo

	# add size to md5/file pairs and validate things:
	# first strip blank lines, and lines that do not have enough tokens (md5 and filename)
	# then, if  md5 is 32 characters, file size is >= 0, md5 has no / (reversed info?)
	# all is ok, and we ensure that each file is in an arena directory

badf=$TMP/bad.$$			# list of records that were invalid
>$badf
set -o pipefail
awk -v badf=$badf '		# must strip lines that have just one token or rm_size short circuits
	NF == 1 { 
		err++; 
		printf( "not enough tokens (md5, filename needed): %s\n", $0 ) >>$badf;
		#print "ERROR-in-pass-1 bad-info-in-list"		# must do this because pipefail is broken on some systems
		next;
	 }

	NF < 1 { next; }		# skip blank line -- not an error

	{ print; }			# passes all -- let it fly
' <$tmp | ng_rm_size  | awk -v badf=$badf '
	BEGIN {
		err = 0;
	}

	/ERROR-in-pass-1/ { err++; next;  }			# must do because of pipe fail issues

	{
		if( index( $1, "/" ) )			# file and not md5 
		{
			err++;
			printf( "bad md5 info: %s\n", $0 ) >"/dev/fd/2";
		}

		if( length( $1 ) == 32 && $3+0 >= 0 )
			printf( "%s\n", $0 );
		else
		{	
			err++;
			printf( "bad md5 or file info: %s\n", $0 ) >>badf;
		}
	}

	END {
		exit( err > 0 ? 1 : 0 );
	}
'  >$vtmp
errors=$?

if (( $errors > 0 ))			# log any problems with the input list
then
	log_errors $PRI_ERROR "input list error" 
fi

if [[ ! -s $vtmp ]]
then
	log_msg "abort: intput errors left no files to add"
	cleanup 1
fi

badf=$TMP/outside.$$			# list of files outside of the perview of repmgr
>$badf

#cat  $NG_ROOT/site/arena/*|sed 's!$!/!' >$TMP/pat.$$			# arena patterns -- add trailing slash
ng_rm_arena_list -c |sed 's!$!/!' >$TMP/pat.$$			# arena patterns -- add trailing slash

gre -F -f $TMP/pat.$$ -v $vtmp >$badf					# collect files that are not in bounds
if [[ -s $badf ]]
then
	log_errors $PRI_ERROR "file not in repmgr arena:"
	mv $vtmp $TMP/old_vtmp.$$
	gre -F -f $TMP/pat.$$  $TMP/old_vtmp.$$ >$vtmp			# just the files that are in the arena
fi		

if [[ ! -s $vtmp ]]
then
	log_msg "abort: location errors left no files to add"
	cleanup 1
fi


attempt=1
while ! cat $vtmp | ng_rm_db -a $verb  2>$etmp
do					# add file(s) to rm_db; if failure, retry once before giving up.
	if [[ $attempt -gt 0 ]]
	then
		verbose "error adding files to local rm_db; retry in a second"
		sleep 1
	else
		badf=$vtmp						# log_errors lists files in badf
		log_errors $PRI_ERROR "add to rm_db failed"		# log each missed file
		log_msg "error(s) adding file(s) to local rm_db; details in master log"
		cleanup -k 2
	fi

	attempt=0
done


	# now handles multiple md5/file pairs passed from stdin
filer=${srv_Filer:-no_filer}
attempt=2
set -o pipefail
fv="$(ng_ppget RM_FLUSH)"
if (( ${fv:-0} < 1 ))
then
	while ! awk -v mynode=`ng_sysname` ' { printf( "instance %s %s %s\n", mynode, $1, $2 ); } ' < $vtmp | ng_rmreq -s ${filer:-unknown-filer-host}
	do
		if [[ $attempt -gt 0 ]]
		then
			verbose "error communicating with repmgr; retry after a brief pause"
			sleep 5
		else
			badf=$vtmp
			log_errors $PRI_WARN "sending instance(s) to repmgr failed"	# log each possibly missed file
			cleanup 3
		fi
		attempt=$(( $attempt - 1 ))
	done
fi


#b=`ng_rm_db -p | wc -l`
b=noinfo

# all good -- log each file that we added
while read x 
do
	ng_log $PRI_INFO $argv0 "[$a] add $x [$b]"
done <$vtmp

rm $tmp

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_flist_add - Add a file to the local rm_db file list
&name &utitle

&space
&synop  ng_flist_add filename checksum


&space
&desc   &ital(Ng_flist_add) takes a file and checksum and adds it to
	the arena database (or updates it if it is already there).
	The filename supplied on the command line MUST be fully qualified, 
	and the directory must be listed inside one of the replication 
	manager arenas.  If not, an error message will be generated. 

&space
&opts   &ital(Ng_flist_add) script takes two arguments, described below.
	If no arguments are given, then stdin is used and the order of the 
	parameters should be reversed (md5sum filename).
&begterms
&term   fname  : The file's name (only the basename is used).
&term   chksum  : The MD5 checksum for &ital(fname).
&endterms
&space

&exit
	&ital(Ng_flist_add) script will set the system exit code to one
	of the following values in order to indicate to the caller the final
        status of processing when the script exits.
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&space
	>1 : An error occurred while trying to add the file(s) to the local 
	database, or when sending an instance command to replication manager.
&endterms

&space
&examp  ng_flist_add /ningaui/fs00/00/test d7894365ba19aa91fa72081f3cff66cd
&space

&bugs
	If parameters are supplied on the command line the order expected is
	filname md5value, however if parameters are passed via stdandard input
	the order expected is md5value filename.  Inconsistant.
&space
&see    ng_flist_add, ng_flist_del, ng_flist_send, repmgr

&space
&mod
&owner(Andrew Hume)
&lgterms
&term	29 Apr 2004 (sd) :  Added error checking, moved tmp file to $TMP and named
	with .$$ so that cleanup will delete.  Also retries repmgr requests if 
	there are failures.  
&term	19 Oct 2004 (sd) : took out the (commented out) ref to ng_leader
&term	24 May 2005 (sd) : Added checking to verify md5 is correct length and size is >= 0.
		Corrected ustring.
&term	25 May 2005 (sd) : Now forces files to be within the repmgr arean list of filesystems/dirs.
&term	26 May 2005 (sd) : Fixed problem with instance string having too much info.
		Removed set -x left in log_errors function.
&term	22 Jun 2006 (sd) : Fixed log_errors function to better put out inforamation to stderr. 
&term	11 Oct 2006 (sd) : Fixed log message going to stderr rather than master.
&term	15 Nov 2006 (sd) : Fixed bug in one call to log_errors function (it was sending argv0 as second
		argument; probably left from an origianal ng_log call.
&term	19 Nov 2008 (sd) : Corrected exit code if we abort because errors left no files in the list. 
&endterms
&scd_end

