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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
ustring='file [file...]'
while [[ $1 == -* ]]
do
	case $1 in 
		--man)	show_man; cleanup 1;;
		-v)	verbose=1; verb=-v;;
		-\?)	usage; cleanup 1;;
		*)		
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac

	shift 
done	

tmp=$TMP/dedup.$$
> $tmp

#	goal is end up with exactly one file with a given (basename, md5).
# a secondary goal is stability; if there is one already in the rmdb database,
# choose that one. because we are removing files, and we're paranoid, we
# check every md5 before relying on it.

# first, form equivalence classes in $tmp.a
for i in $*
do
	md5sum $i | read md5 x
	case "$md5" in
	?*)	;;
	*)	continue;;
	esac
	echo "file|$md5|$i"
done > $tmp.a

# prepare a list of (relevent) files in rmdb in $tmp.b
for i in $*
do
	echo " $i "
done > $tmp.1
ng_rm_db -p | gre -F -f $tmp.1 | awk '{ print "db|" $1 "|" $2}' > $tmp.b

# for each equivalence class, pick the exemplar
cat $tmp.b $tmp.a | awk '-F|' '
$1=="db" {
	b = $3; sub(".*/", "", b)
	prefer[$2,b] = $3
}
$1=="file" {
	b = $3; sub(".*/", "", b)
	md5[$3] = $2
	if(x = prefer[$2,b]){
		if(x == $3)
			found[$2,b] = 1
	}
	stooge[$2,b] = $3
}
END {
	# first, assign exemplars where there isnt an entry in rmdb
	for(j in prefer)
		if(found[j] != 1){
			prefer[j] = stooge[j]
			print md5[stooge[j]], stooge[j]
		}
	# now just run through all our files, removing non-exemplars
	for(i in md5){
		b = i; sub(".*/", "", b)
		if(i != prefer[md5[i],b])
			print "rm", i
	}
}' > $tmp.c

# remove unnecessary files
gre '^rm ' $tmp.c | ksh

# add new exemplars
gre -v '^rm ' $tmp.c | ng_rm_size | ng_rm_db -a

rm -f $tmp.?

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_dedup:Remove duplicate files)

&space
&synop  ng_rm_dedup file [file...]


&space
&desc   &This accepts one or more basenames and ensures that only one copy
	of the file exists in the replication manager space on the current 
	node. 
	Equivalence is determined by basename and checksum.
	Preference is given to preserving the current &ital(ng_rm_db) entry.

&space
&see    ng_rm_sync, ng_repmgr, ng_rm_db, ng_rm_dbd

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	04 Jun 2004 (ah) : Action is now purely local, and more paranoid.
&term	19 Jul 2004 (sd) : Added rm_fstat use rathter than rm_where.
&term	01 Aug 2006 (sd) : Added --man option.
&endterms

&scd_end
