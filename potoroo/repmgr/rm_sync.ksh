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
# ------------------------------------------------------------------------
#  Synchronize disk and instance database for replication manager
# ------------------------------------------------------------------------
#

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
solo

ustring='ng_rm_sync [-vfrnk]'
notouch=false
chkcontents=false
scheduled=false
nomail=true
tmp=$TMP/rmsync.$$
while [[ $1* = -* ]]			# while "first" parm has an option flag (-)
do
	case $1 in
	-f)	chkcontents=true;;
	-n)	notouch=true;;
	-m)	nomail=false;;
	-s)	scheduled=true;;
	-r)	tmp=$NG_ROOT/site/rm/rmsync;;
	-p*)	getarg input "$1" "$2";;
	-i*)	getarg input "$1" "$2";;
	-v)	verbose=TRUE; verb=-v;;
	-\?)	usage; cleanup 1;;
	--man)	set -x 
		show_man
		exit 1
		;;
	*)		# invalid option - echo usage message 
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac				# end switch on next option character

	shift
done					# end for each option character


function sendit
{
	cd $TMP

	now="$(ng_dt -d)"
	nfile="$TMP/${argv0##*/}.note.$now"
	mailit=0
        node=`ng_sysname`

	limit_gone=300
	limit_found=2000
	limit_dup=1500

	# file is listed in the local database but is not on disk
	num=`grep -v "\.cpt | grep -v pf-tape-filecount" $tmp.1 | wc -l`
	if [[ $num -gt ${limit_gone} ]]
	then
		mailit=1
		echo "rmsync.1 (no instances) at $num exceeded limit of ${limit_gone}" >> $nfile
	fi

	# file has been found on disk but is not listed in the local database
	num=`grep -v "\.cpt" $tmp.2 | wc -l`
	if [[ $num -gt ${limit_found} ]]
	then
		mailit=1
		echo "rmsync.2 (found instances) at $num exceeded limit of ${limit_found}" >> $nfile
	fi

	# duplicate filenames are listed multiple times in the local database 
	num=`grep -v "\.cpt" $tmp.5 | wc -l`
	if [[ $num -gt ${limit_dup} ]]
	then
		mailit=1
		echo "rmsync.5 (duplicate instances) at $num exceeded limit of ${limit_dup}" >> $nfile
	fi

	if [[ $mailit -gt 0 ]]
	then
		ng_log $PRI_ALERT $argv0 "@NG_ALERT_NN_ng_rm_sync rmsync exceeded tolerance.  See $node:$nfile"	
	fi
}


> $tmp.1	# files in database but not on disk 
> $tmp.2	# files on disk but not in database
> $tmp.3	# list to verify
> $tmp.4	# files on disk excluding transient files (which end in ! or ~)
> $tmp.5	# files with same basename and md5sum
> $tmp.6	# files in database
> $tmp.7	# transient files on disk that are older than 4 days 
> $tmp.8	# same file (including full pathname) in database twice

rsrc=RM_SYNC_`ng_dt -d`			# unique id based on ningaui date
echo $rsrc > $tmp.rsrc

# generate file lists
case "$input" in
?*)	cat $input
	;;
*)	
	# files in instance database
	ng_rm_db -p > $tmp.6
	if [[ $? -ne 0 ]]
	then
		ng_log $PRI_ERROR $argv0 "ng_rm_db -p failed"
		cleanup 1
	fi

	# list of files actually in each filesystem
	cd $NG_FS_ARENA			# yields location of ningaui filesystems
	for i in *
	do
		# find the transient files older than 4 days 
		/usr/bin/find `cat $i`/[0-9]* -name "*[~!]" -mtime +4 -print >> $tmp.7  
		if [[ $? -ne 0 ]]
		then
			ng_log $PRI_ERROR $argv0 "find (1) failed"
			cleanup 1
		fi

		# find all the non-transient files
		/usr/bin/find `cat $i`/[0-9]* -type f -print | gre -v '[!~]$' >> $tmp.4
		if [[ $? -ne 0 ]]
		then
			ng_log $PRI_ERROR $argv0 "find (2) failed"
			cleanup 1
		fi
	done 

	;;
esac 

# look for the exact same file being listed in rmdb (old rmdb bug)
ng_prcol 2 < $tmp.6 | sort | uniq -d | sed 's/^/dup /' > $tmp.8

cat $tmp.8 $tmp.4 $tmp.6 | awk -v delf=$tmp.1 -v addf=$tmp.2 -v verf=$tmp.3 -v dedupf=$tmp.5 -v rsrc=$rsrc '
NF==2   {    # exact same file listed twice in database  
                duppath[$2] = 1
        }
NF==1	{    # files from find
		file[$1] = 1
                if(duppath[$1] == 1)
                        next

		base = $1
		sub(".*/", "", base)
		if(++nn[base] > 1){
			if(rand() < .5)
				flist[base] = $1 " " flist[base]
			else
				flist[base] = flist[base] " " $1
		} else
			flist[base] = $1
	}
NF==3	{     # files in database plus checksums
                if(duppath[$2])
                        print $1, $2 >delf
                else
                        md5[$2] = $1
	}
END	{
	rdedup = "RM_DEDUP"
	print "limit", rsrc, "4" >addf
	print "limit", rsrc, "4" >verf
	print "limit", rsrc, "4" >dedupf
	print "limit", rdedup, "4" >dedupf
	
	# find files listed in the instance database that do not actually
	# exist on disk -- remove them from the instance database
	for(j in md5){
		if(! file[j]) 
			print md5[j], j >delf
	}
	# handle dups (runs first at high priority)
	for(j in nn){
		if(nn[j] > 1){
			print "job:", rsrc, rdedup, "pri 42 cmd ng_rm_dedup", flist[j] >dedupf
			didit[j] = 1
		}
	}
	# find files in file system but not in database (runs after dedup at high priority)
	for(j in file){ 
		if(!md5[j] && (didit[j] != 1)){
			print "job: RM_RCV_FILE", rsrc, "after RM_DEDUP, pri 25 cmd ng_rm_rcv_file", j >addf
			didit[j] = 1
		}
	}
	# verify files that are there (runs after dedup at low priority)
	for(j in file){
		if(md5[j] && (didit[j] != 1)){
			print "job: RM_RCV_FILE", rsrc, "after RM_DEDUP, pri 5 cmd ng_rm_syncverify", j, md5[j] >verf
		}
	}
}'

ng_log $PRI_INFO $0 "RM_SYNC $rsrc  delete `wc -l < $tmp.1` add `wc -l < $tmp.2` dedup `wc -l < $tmp.5` verify `wc -l < $tmp.3`"
echo "rm_sync: delete `wc -l < $tmp.1`, add `wc -l < $tmp.2`, dedup `wc -l < $tmp.5`, trashing `wc -l < $tmp.7`, exactdup `wc -l < $tmp.8` (disk=`wc -l < $tmp.4` rmdb=`wc -l < $tmp.6`)" | sed 's/  */ /g'

# Send email if something is out of whack
if [[ $nomail == false ]]
then
	sendit
fi

# Do not try to fix things
if [[ $notouch == true ]]
then
	wc $tmp.?
	exit 0
fi

ng_rm_db -d < $tmp.1
ng_flist_send
cat $tmp.[25] | ng_wreq

# Delete 4+ day old files ending in ! or ~
if test -s $tmp.7
then
	cat $tmp.7 | xargs rm -f
fi

if [[ $chkcontents == true ]]
then
	ng_wreq < $tmp.3
fi

ng_wreq "job: after $rsrc, cmd ng_log $PRI_INFO $0 'RM_SYNC $rsrc done'"

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_rm_sync - Synchronise disk and instance database
&name &utitle

&space
&synop  ng_rm_sync [-vfrnk]


&space
&desc   &ital(Ng_rm_sync) brings a node's instance database maintained by &ital(ng_rm_db)
	into agreement with the actual contents of its datastore filesystems.
	This is, in principle, a very slow process as it involves calculating
	md5 checksums for every file; it may easily take days to complete.
	Accordingly, the work is split into two phases; file lists and file contents.
&space
	File list differences are generated by simply comparing the lists of
	files in the database and in the filesystems and doing the appropriate updates.
	For files in both the database and filesystems, we then verify their
	contents (md5 checksums).
&space
	Files whose names end with ! or ~ are special cases.  They are ignored for the 
	purpose of synchronization and deleted if their modification time is greater 
	than 4 days.  Applications are encouraged to use filenames ending with ! or ~ 
	when generating files in repmgr space.  When the file is complete, applications 
	should rename the file and register it.  This prevents &ital(Ng_rm_sync) from 
	messing with files before they're ready.

&space
&opts   &ital(Ng_rm_sync) will only do the file list differences;
	&cw(-f) forces the file content pass. The &cw(-n) option
	prepares files ready for action, but exits before doing anything.
	Normally, &this uses working files in &cw($TMP); the $cw(-r) option
	changes that to &cw($NG_ROOT/site/rm).

&exit
	&ital(Ng_rm_sync) returns the following exit codes:
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&endterms

&space
&see    ng_rm_db

&mods
&owner(Bethany Robinson)
&lgterms
&term	01 Nov 2004 (sd) : Added --man ability.
&term	18 Aug 2006 (sd) : updated options loop.
&term	20 Feb 2007 (sd) : Changed to issue an alert log message rather than sending email.
&term	22 May 2007 (bsr) : Repair case when exact same file is listed in database twice (old rmdb bug).
&term	22 May 2007 (bsr) : Files on dedup list no longer get put on rcvfile or syncverify lists.
&term	15 May 2007 (bsr) : Increased syncverify limits plus set different limits per filesystem. 
&term	15 May 2007 (bsr) : Fixed usage string.
&endterms
&scd_end
