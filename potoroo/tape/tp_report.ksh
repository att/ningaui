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


tmp=/tmp/tr.$$
> $tmp

cd /home/andrew/tapes

if [[ $# -gt 0 ]]
then
(
	ng_tpreq pooldump all
	sed 's/^/empty /' < list.empty
	sed 's/^/archive /' < list.archive
	sed 's/^/gone /' < list.gone
	ng_tpreq status all | awk '
	$1=="dte" { print $3; next }
	/^ *[0-9]+:/ { print $2; next }
	{ next }' | gre '^[BC]' | sed 's/^/library /'
	gre ' pf-tapes-[^.]*.cpio .* bilby:s4 ' /ningaui/site/waldo/waldo.canon |
		sed 's/.*pf-tapes-\(B.....\).*/lseg \1/'
	gre : /ningaui/site/waldo/tape.summary | sed 's/^/vet /; s/:/ /'
	(
		echo 'BEGIN {'
		sed '1,/SUBSEP/d' < /ningaui/bin/ng_waldo_gen | sed '/}/,$d'
		cat <<'EOF'
			for(i in ok) printf("modern B%05d\n", i)
			exit(0)
		}
EOF
	) > $tmp
	awk -f $tmp < /dev/null
	sed 's/^/desk /' < list.desk
	gzip -d -c /ningaui/site/waldo/tape*STK*.gz | awk '
	 {
		if(s[$4] < $5) s[$4] = $5
	}
	END {
		for(i in s) printf("qut %s TAPE cmd /home/andrew/qut %s %d\n", i, i, s[i]+1)
	}'
) > temp.in
	echo generated temp.in 1>&2
	cat temp.in
else
	cat temp.in
fi | awk '
function pr_1(){
	printf("%d total tapes, by type:\n", nu+nc)
	printf("\t%4d bought\n", nu+ngone+nc)
	printf("\t%4d tapes gone\n", -ngone)
	printf("\t====\n\t%4d physical tapes:\n", nu+nc)
	printf("\t\t%4d older series tapes\n", na1)
	printf("\t\t%4d modern series tapes\n", nb1)
	printf("\t\t%4d cleaning tapes\n", nc)
	print "--------------"
}
function gen_2(){
	nc = 0
	for(i in clean)
		if(clean[i] == 1)
			nc++
	nu = 0
	for(i in universe)
		if(universe[i] == 1){
			nu++
			leftover[i] = 1
		}
	for(i in library)
		if(library[i] == 1){
			nlib++
			leftover[i] = 0
			if((universe[i] != 1) && (clean[i] != 1)){
				noddlib++
				soddlib = soddlib "," i
			}
		}
	for(i in archive)
		if(archive[i] == 1){
			na++
			leftover[i] = 0
			if((universe[i] != 1) && (clean[i] != 1)){
				noddarch++
				soddarchb = soddarch "," i
			}
		}
	for(i in gone)
		if(gone[i] == 1){
			ngone++
		}
	for(i in desk)
		if(desk[i] == 1){
			leftover[i] = 0
			ndesk++
		}
	for(i in empty)
		if(empty[i] == 1){
			leftover[i] = 0
			nempty++
		}

	for(i in leftover)
		if(leftover[i] == 1){
			sleft = sleft "," i
		}
}
function pr_2(){
	printf("\n%d total tapes, by location:\n", nu+nc)
	printf("\t%d in library\n", nlib)
	printf("\t%d in archive store\n", na)
	printf("\t%d on desk:\n", ndesk+nempty)
	printf("\t\t%d lurking\n", ndesk)
	printf("\t\t%d empty\n", nempty)
	printf("\t%d unknown at this point %s\n", (nu+nc)-nlib-na-ndesk-nempty, substr(sleft, 2))
	print "--------------"
}
function gen_a(){
	a2file = "temp.a2"
	a3file = "temp.a3"
	a4file = "temp.a4"
	for(i in universe){
		if(universe[i] != 1)
			continue
		if(index(i, "CLN") == 1)
			continue
		if(modern[i] == 1)
			continue
		if((substr(i, 2)+0) >= 400)
			continue
		na1++
		if(vetted[i] == 1){
			if(lseg[i] == 1){
				na2++
				sa2 = sa2 "," i
			} else {
				na3++
				sa3 = sa3 "," i
				print qut[i] > a3file
			}
		} else {
			na4++
			sa4 = sa4 "," i
			print i > a4file
		}
	}
}
function pr_a(){
	printf("\n%d older series tapes:\n", na1)
	printf("\t%d vetted:\n", na2+na3)
	x = sa2; gsub(",", " ", x)
	printf("ng_tpreq decant%s\n", x) > a2file
	if(length(sa2) > MAXSTR)
		sa2 = substr(sa2, 2, MAXSTR+1) "..."
	else
		sa2 = substr(sa2, 2)
	printf("\t\t%d with lseg %s (%s)\n", na2, sa2, a2file)
	if(length(sa3) > MAXSTR)
		sa3 = substr(sa3, 2, MAXSTR+1) "..."
	else
		sa3 = substr(sa3, 2)
	printf("\t\t%d without lseg %s (%s)\n", na3, sa3, a3file)
	if(length(sa4) > MAXSTR)
		sa4 = substr(sa4, 2, MAXSTR+1) "..."
	else
		sa4 = substr(sa4, 2)
	printf("\t%d not vetted %s (%s)\n", na4, sa4, a4file)
	print "--------------"
}
function gen_c(){
	c2file = "temp.c2"
	c3file = "temp.c3"
	c4file = "temp.c4"
	for(i in universe){
		if(universe[i] != 1)
			continue
		if(index(i, "CLN") == 1)
			continue
		if(((substr(i, 2)+0) < 400) && (modern[i] != 1))
			continue
		nb1++
		if(pools[i] == 1){
			nb5a++
		} else if(apools[i] == 1){
			nb5b++
		} else if(unalloc[i] == 1){
			nb5c++
		} else if(empty[i] == 1){
			nb5d++
		} else if(vetted[i] == 1){
			if(lseg[i] == 1){
				nb2++
				sb2 = sb2 "," i
				print i > c2file
			} else {
				nb3++
				sb3 = sb3 "," i
				print qut[i] > c3file
			}
		} else {
			nb4++
			sb4 = sb4 "," i
			print i > c4file
		}
	}
}
function pr_c(){
	printf("\n%d modern series tapes:\n", nb1)
	printf("\t%d pools etc:\n", nb5a+nb5b+nb5c+nb5d)
	printf("\t\t%d in feed pools\n", nb5a);
	printf("\t\t%d in ARCHIVE\n", nb5b);
	printf("\t\t%d in UNALLOCATED\n", nb5c);
	printf("\t\t%d empty\n", nb5d);
	printf("\t%d vetted:\n", nb2+nb3)
	if(length(sb2) > MAXSTR)
		sb2 = substr(sb2, 2, MAXSTR+1) "..."
	else
		sb2 = substr(sb2, 2)
	printf("\t\t%d with lseg %s (%s)\n", nb2, sb2, c2file)
	if(length(sb3) > MAXSTR)
		sb3 = substr(sb3, 2, MAXSTR+1) "..."
	else
		sb3 = substr(sb3, 2)
	printf("\t\t%d without lseg %s (%s)\n", nb3, sb3, c3file)
	if(length(sb4) > MAXSTR)
		sb4 = substr(sb4, 2, MAXSTR+1) "..."
	else
		sb4 = substr(sb4, 2)
	printf("\t%d not vetted %s (%s)\n", nb4, sb4, c4file)
	print "--------------"
}
function gen_b(){
	narchive = 0
	svet2 = ""
	nvet2 = 0
	b3file = "temp.b3"
	b4file = "temp.b4"
	b5file = "temp.b5"
	for(i in universe){
		if(universe[i] != 1)
			continue
		if(pools[i] == 1)
			continue
		n = substr(i, 2)+0
		if((n < 400) && (modern[i] != 1))
			continue
		if((int(n/10)%2) == 0)
			continue
		narchive++
		if(vetted[i] == 1){
			nvet++
			svet = svet "," i
			if(vsize[i] >= FULL){
				if(lseg[i] == 1){
					nvet1a++
					svet1a = svet1a "," i
				} else {
					nvet1b++
					svet1b = svet1b "," i
					print qut[i] > b4file
				}
			} else {
				nvet2++
				svet2 = svet2 "," i
			}
		} else {
			if(apools[i] == 1){
				nunvet1++
				sunvet1 = sunvet1 "," i
			} else if(empty[i] == 1){
				nunvet3++
				sunvet3 = sunvet3 "," i
			} else {
				nunvet2++
				sunvet2 = sunvet2 "," i
				print i > b5file
			}
		}
		sarchive = sarchive "," i
	}
}
function pr_b(){
	printf("\n%d archive tapes (subset of modern series):\n", narchive)
	printf("\t%d vetted tapes < %sGB %s\n", nvet2, FULL, substr(svet2, 2))
	printf("\t%d vetted tapes >= %sGB:\n", nvet1a+nvet1b, FULL)
	x = svet1a; gsub(",", " ", x)
	printf("ng_tpreq decant%s\n", x) > b3file
	if(length(sunvet1) > MAXSTR)
		sunvet1 = substr(sunvet1, 2, MAXSTR+1) "..."
	else
		sunvet1 = substr(sunvet1, 2)
	if(length(svet1a) > MAXSTR)
		svet1a = substr(svet1a, 2, MAXSTR+1) "..."
	else
		svet1a = substr(svet1a, 2)
	printf("\t\t%d tapes with lseg %s (%s)\n", nvet1a, svet1a, b3file)
	if(length(svet1b) > MAXSTR)
		svet1b = substr(svet1b, 2, MAXSTR+1) "..."
	else
		svet1b = substr(svet1b, 2)
	printf("\t\t%d tapes without lseg %s (%s)\n", nvet1b, svet1b, b4file)
	if(length(sunvet2) > MAXSTR)
		sunvet2 = substr(sunvet2, 2, MAXSTR+1) "..."
	else
		sunvet2 = substr(sunvet2, 2)
	if(length(sunvet3) > MAXSTR)
		sunvet3 = substr(sunvet3, 2, MAXSTR+1) "..."
	else
		sunvet3 = substr(sunvet3, 2)
	printf("\t%d unvetted tapes:\n", nunvet1+nunvet2+nunvet3)
	printf("\t\t%d ARCHIVE tapes %s\n", nunvet1, sunvet1)
	printf("\t\t%d empty(new) tapes %s\n", nunvet3, sunvet3)
	printf("\t\t%d unvetted tapes %s (%s)\n", nunvet2, sunvet2, b5file)
	print "--------------"
}
BEGIN {
	last_id = "B00639"
	for(i = 0; i <= (substr(last_id, 2)+0); i++)
		universe[sprintf("B%05d", i)] = 1
}
$1~/FEEDS/ {
	for(i = 2; i <= NF; i++)
		pools[$i] = 1
	next
}
$1~/TEST/ {
	for(i = 2; i <= NF; i++)
		pools[$i] = 1
	next
}
$1~/COMB/ {
	for(i = 2; i <= NF; i++)
		pools[$i] = 1
	next
}
$1~/ARCHIVE/ {
	for(i = 2; i <= NF; i++)
		apools[$i] = 1
	next
}
$1~/TEMP/ {
	for(i = 2; i <= NF; i++)
		unalloc[$i] = 1
	next
}
$1~/UNALLOCATED/ {
	for(i = 2; i <= NF; i++)
		unalloc[$i] = 1
	next
}
$1=="empty" {
	for(i = 2; i <= NF; i++)
		empty[$i] = 1
	next
}
$1=="desk" {
	for(i = 2; i <= NF; i++)
		desk[$i] = 1
	next
}
$1=="library" {
	for(i = 2; i <= NF; i++){
		library[$i] = 1
		if(index($i, "CLN") == 1)
			clean[$i] = 1
	}
	next
}
$1=="vet" {
	vetted[$2] = 1
	vsize[$2] = $4+0
	next
}
$1=="gone" {
	for(i = 2; i <= NF; i++){
		gone[$i] = 1
		universe[$i] = 0
	}
	next
}
$1=="lseg" {
	for(i = 2; i <= NF; i++)
		lseg[$i] = 1
	next
}
$1=="archive" {
	for(i = 2; i <= NF; i++)
		archive[$i] = 1
	next
}
$1=="modern" {
	for(i = 2; i <= NF; i++)
		modern[$i] = 1
	next
}
$1=="qut" {
	x = $2
	$1 = ""
	$2 = "job:"
	qut[x] = $0
	next
}
 {
	next
}
END {
	MAXSTR = 20
	FULL = 206
	print "Report on StorageTek tapes\n"
	gen_2()
	gen_a()
	gen_b()
	gen_c()
	pr_1()
	pr_2()
	pr_a()
	pr_c()
	pr_b()
}'

rm $tmp

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_tp_report - produce report on the StorageTek tapes
&name &utitle

&space
&synop  ng_tp_report


&space
&desc   &ital(Ng_tp_report) produces a detailed report on teh state of the StorageTek tapes.

&space

&exit
	&ital(Ng_tp_library) script will set the system exit code to one
	of the following values in order to indicate to the caller the final
        status of processing when the script exits.
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&endterms

&space
&see    [cross references]
&owner(Andrew Hume)
&scd_end

