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

# DEPRECATED  5/5/05  BSR
echo "deprecated command:  uses 'df -m' which isn't safe on several of our nodes" 
exit 0

#
# ------------------------------------------------------------------------
#  Compare disk and instance database for replication manager
# ------------------------------------------------------------------------
#

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN

(
	# filesystem summary 
	df -m

	# files in instance database
	ng_rm_db -p

	# list of files actually in each filesystem
	for i in `cat $NG_FS_ARENA/*`
	do
		find $i/[0-9]* -type f -print
	done | xargs ls -l

) | awk '
/^\/dev/ {	
		# determine amount of space used according to df
		if(index($0, "/ningaui/fs")){
			df_used[$7] = $4+0
		}
	}
NF==3	{	
		fs = substr($2, 1, 13)		# horrible code
		base = $2; sub(".*/", "", base)

		# calculate amount of space per filesystem via database
		db_used[fs] += $3
		db[base]++	
	}
NF==9	{	
		fs = substr($9, 1, 13)		# horrible code
		base = $9; sub(".*/", "", base)

		# calculate amount of space used per filesystem via ls
		fs_used[fs] += $5
		nbase[base]++		
	}
END	{

	# compare filesystem space from df, ls, rm_db
	meg = 1024*1024
	for(i in fs_used) {

		# DIVISION BY ZERO for satellite nodes which do not have a /ningaui/fs
		# Some satellites have repmgr have the fs mounted but others do not
		x = 100/df_used[i]

		printf("%s: %dMB used (via df); ls yields %.1f%%(%.0fMB), ng_rm_db yields %.1f%%(%.0fMB)\n", i, df_used[i], x*fs_used[i]/meg, fs_used[i]/meg, x*db_used[i]/meg, db_used[i]/meg)
	}

	for(i in db){
		# files in database multiple times
		if(db[i] > 1) db_mult++

		# files listed in database but are missing from ls
		if(nbase[i] == "") fs_miss++

		# files in database that are on the filesystem multiple times
		if(nbase[i] > 1) dbfs_mult++

		# total number of files in database
		n_db++
	}

	for(i in nbase){
		# files from ls that are not in database
		if(db[i] == "") db_miss++

		# files in database that are on the filesystem multiple times
		if(nbase[i] > 1) fs_mult++

		# total number of unique files in ls
		n_fs++
	}
	printf("%d in db:  %d multiply, %d not in fs, %d db file in fs multiply\n", n_db, db_mult, fs_miss, dbfs_mult)
	printf("%d in fs:  %d multiply, %d not in db\n", n_fs, fs_mult, db_miss)

}'

# Get stats for the last time ng_rm_sync was run
tac $NG_ROOT/site/log/master | grep "`ng_sysname -s`.*RM_SYNC" | sed 1q | awk '{print $6,$10}' | read rsrc n
echo "from last sync, we're adding $n files to the database"
ng_wreq explain $rsrc

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_rm_sanity - check disk and instance database 
&name &utitle

&space
&synop  ng_rm_sanity


&space
&desc   &ital(ng_rm_sanity) checks files found on the local filesystems
with those in the instance database. 

&exit
	&ital(ng_rm_sanity) script always returns 0.

&space
&see    ng_rm_db
&owner(Andrew Hume)
&scd_end

