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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
ustring=""
verbose=0
while [[ $1 = -* ]]
do
        case $1 in
                -v)     verbose=1;;
                --man)  show_man; cleanup 1;;
                -\?)    usage; cleanup 1;;
                -*)     error_msg "unrecognised option: $1"
                        usage;
                        cleanup 1
                        ;;
        esac

        shift
done

set -x
tmp=$TMP/flow1.$$

(
	ng_rm_where -v -p `ng_sysname` pf-perf.* | sed 's/^/ instance /'
	tail -r $NG_ROOT/site/log/master | grep 'SENESCHAL_AUDIT.*perfdump'
) | awk '
$1=="instance" {
	x = $3; sub(".*/", "", x); seen[x] = $3
	next
}
/perfdump/ {
	x = $7; sub(".*-", "", x)
	if(first == "")
		first = x
	y = "pf-perf-seneschal-" x
	if(seen[y]){
		printf("%02d %02d %s\n", x, (first+1)%30, seen[y])
		exit(0)
	}
	printf("ignoring %s; no instance found\n", $0) >"/dev/stderr"
}' | read onum num cpt
echo "onum=$onum num=$num cpt=$cpt" 1>&2

if false				# true to redo whole master log; false to do incremental
then
	cpt=/dev/null
	bof=BOF
else
	bof=perfdump-$onum
fi

# put in our marker
ng_sreq -C perfdump-$num

grep SENESCHAL_AUDIT $NG_ROOT/site/log/master | sort -u | awk -v mark1=$bof -v mark2=perfdump-$num '
function field(id	,x,y){
	x = "[^ ]*[(]" id "[)]"
	if(y = match($0, x)){
		x = substr($0, y)
		sub("[(].*", "", x)
	} else
		x = ""
	return x
}
BEGIN	{
	n["job",0] = 1
	runt["job",0] = 1
	size["job",0] = 1
	if(mark1 == "BOF")
		state = 1
	else
		state = 0
	printf("processing between %s and %s\n", mark1, mark2) >"/dev/stderr"
}
$6=="usermsg" {
	printf("usermsg: state=%d $7=%s\n", state, $7) >"/dev/stderr"
	if((state == 0) && ($7 == mark1)){
		state = 1
		next
	}
	if((state == 1) && ($7 == mark2)){
		state = 2
		next
	}
	next
}
$6=="scheduled" {
	job = field("job")
	att = field("att")+0
	start[job,att] = $0+0
#if(index(job, "pupdate")) printf("%s att=%d starts at %d\n", job, att, start[job,att]) >"/tmp/agh77"
	jsize[job] = field("size")
	next
}
$6=="status" {
	if(state != 1) next
	if(field("status") != "0") next
	job = field("job"); jc = job; sub("[_.].*", "", jc)
	if(jc == "EJ") next
	jobclass[jc] = 1
	node = field("node")
	nodes[node] = 1
	att = field("att")+0
	t = $0+0
	day = int(t/(24*3600*10))
if(index(job, "pupdate")) printf("%s att=%d status at %d node %s %s\n", job, att, $0+0, node, $0) >"/tmp/agh77"
	days[day] = 1
	if(start[job,att]) elapsedt[jc,day,node] += (t-start[job,att])/10
	runt[jc,day,node] += field("real")/10
	size[jc,day,node] += jsize[job]
	n[jc,day,node]++
	next
}
END	{
	printf("layout %s %s %s %s %s %s %s %s\n",
		"day", "job", "node", "n", "runt", "elapt", "sizeMB", "thruputMB/s")
	for(jc in jobclass) for(day in days) for(node in nodes)
		if(n[jc,day,node] && runt[jc,day,node])
			printf("datum %d %s %s %d %.1f %.1f %.3f %.3f\n",
				day, jc, node,
				n[jc,day,node],
				runt[jc,day,node]/n[jc,day,node],
				elapsedt[jc,day,node]/n[jc,day,node],
				size[jc,day,node]/(1024*n[jc,day,node]),
				size[jc,day,node]/(1024*runt[jc,day,node]))
}' > $tmp

# combine; must be catenating so as to preserve changing layout stuff
out=`ng_spaceman_nm pf-perf-seneschal-$num`
cat $cpt $tmp | awk '
$1=="layout" {
	layout = $0
	next
}
$1=="datum" {
	days[$2] = 1
	jobs[$3] = 1
	nodes[$4] = 1
	k = $5
	n[$2,$3,$4] += k
	for(i = 6; i <= NF; i++)
		t[$2,$3,$4,i] += $i*k
	nfield = NF
	next
}
$1=="average" {
	# skip
	next
}
 {
	# these shouldnt be here!
	printf("skipping %s\n", $0) >"/dev/stderr"
}
END {
	print layout
	for(d in days)
	for(j in jobs)
	for(nd in nodes)
	if(k = n[d,j,nd]){
		printf("datum %d %s %s %d", d, j, nd, k)
		for(i = 6; i <= nfield; i++)
			printf(" %.8g", t[d,j,nd,i]/k)
		printf("\n")
	}
}' | sort --k 1,1r -k 2,2n | awk '
BEGIN {
	theta = .9
}
$1=="layout" {
	print $0
	next
}
$1=="datum" {
	print $0
	jobs[$3] = 1
	nodes[$4] = 1
	if(a[$3,$4] == "")
		a[$3,$4] = $NF
	a[$3,$4] = theta*a[$3,$4] + (1-theta)*$NF
	next
}
END {
	for(j in jobs)
	for(n in nodes)
	if(a[j,n])
		printf("average %s %s %.8g\n", j, n, a[j,n])
}' > $out

wc $out
cur=`ng_spaceman_nm pf-perf-seneschal-cur`
cp $out $cur					# must copy before register to deal with name changes (~ removal)

ng_rm_register  $out
ng_rm_register -e $cur

# we're done
cleanup 0

exit
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_perf_sen:Calculate seneschal performance statistics)

&space
&synop	ng_perf_sen

&space
&desc	&This generates a new version of the &ital(seneschal) performance statistics file
	&cw(pf-perf-seneschal-)&ital(nn).
	It does so by scanning the master log for &ital(seneschal) audit messages.
	The statistics file contains 3 types of lines, distinguished by the first field:
&begterms
&term	layout : This sets out the field layout for the datum lines.
	 
&space
&term	datum : This gives daily statistics for the tuple of (day, node, jobclass).

&space
&term	average : This gives an average value for the throughput metric for
	each tuple of (node, jobclass).
	The value is an exponentially weighted moving average where theta is 0.9.
&endterms	

&space
	The performance statistics file names cycle through a group of (currently) 30,
	and are updated once a day.
	The latest file is also available as the name &cw(pf-perf-seneschal-cur)
	on every node in the cluster.

&space
&see	&ital(ng_senescha) &ital(ng_sreq)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	25 Jul 2003 (ah) : New code.
&term	22 Jun 2005 (sd) : Rearranged copy of out to cur to deal with name change on registeration.
&term	27 Nov 2007 (sd) : Changed register to set to default rather than every. 
&endterms

&scd_end
------------------------------------------------------------------ */
