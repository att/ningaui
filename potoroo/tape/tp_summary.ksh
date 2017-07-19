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
ustring="[-v] [changer]"

while [[ $1 = -* ]]
do
	case $1 in
		-v)	verbose=1; 
			vflag="$vflag -v"
			;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

anal(){
	gzip -d -c tape.*.$1.gz | sort -T $TMP -u | awk -v changer=$1 '
function dofile(f,sz	,x,y,c,rics){
	c = x = asort(vols)
	y = vols[x--]
	for(; x >= 1; x--)
		y = vols[x] "," y
	x = y
	for(y in nv)
		n[x] += nv[y]
	for(y in sv)
		s[x] += sv[y]
	cnt[x] = c
	for(y in seg){
		asegm[y] = 1
		aseg[x,y] = 1
	}
	delete vols
	delete nv; delete sv; delete seg
}
function pretty(key	,i,j,xx,xy){
	for(i in aseg)
		if(index(i, (key "!")) == 1){
			j = i; sub(".*!", "", j)
			xx[j] = 1
			sub(".*,", "", j)
			xy[j] = 1
		}
	# to do: sort xy; for each in xy[i], do etract seg nums, sort and do ranges
	j = asorti(xx)
	for(i = 1; i <= j; i++){
		printf(" %s", xx[i])
	}
	delete xx
	delete xy
}
BEGIN {
	SUBSEP = "!"
}
 {
	nk = $1 "," $2
	if(nk != key){
		if(key)
			dofile(key, siz)
		key = nk
		siz = $3
	}
	vols[$4] = $4
	nv[$4]++
	sv[$4] += $3
	seg[$4 "," $5] = 1
	avols[$4] = $4
	anv[$4]++
	if($2 != "__EXTRA__")
		anve[$4]++
	asv[$4] += $3
}
END {
	if(key)
		dofile(key, siz)
	printf("Report for %s:\n", changer)
	for(x in n){
		printf("%-15s %6d %8.3fGB\n", x, n[x]/cnt[x], s[x]/cnt[x]/1e9)
		printf("%s;", x)
		pretty(x)
		printf("\n")
	}
	for(x in avols)
		printf("%s: %.0f %.3fGB\n", x, anv[x], asv[x]/1e9)
}' #| sort
}

anal1(){
mv tlists! tlists.$$ 2> /dev/null
rm -fr tlists.$$ 2> /dev/null
mkdir tlists! 2> /dev/null

for i in tape*.gz
do
	gzip -d -c $i | gre -v ' __EXTRA__ ' | ng_prcol '1,$2,$3,$4' | uniq | awk '
	function setc(sz	, v,k){
		for(v in arch){
			if((na >= 1) && (np == 0)){
				arch1pp0_n[v]++
				arch1pp0_b[v] += sz
				k = okey; sub(",", " ", k)
				print k > (base v ".p0a1+")
			}
			if(na == 1){
				arch1_n[v]++
				arch1_b[v] += sz
				k = okey; sub(",", " ", k)
				print k > (base v ".a1")
			} else if(na > 1){
				arch2p_n[v]++
				arch2p_b[v] += sz
			}
		}
		for(v in prim){
			if(np == 1){
				if(na >= 1){
					prim1a_n[v]++
					prim1a_b[v] += sz
					k = okey; sub(",", " ", k)
					print k > (base v ".p1a")
				} else {
					prim1_n[v]++
					prim1_b[v] += sz
					k = okey; sub(",", " ", k)
					print k > (base v ".p1")
				}
			} else if(np >= 2){
				if(na >= 1){
					prim2pa_n[v]++
					prim2pa_b[v] += sz
				} else {
					prim2p_n[v]++
					prim2p_b[v] += sz
					k = okey; sub(",", " ", k)
					print k > (base v ".p2+")
				}
			}
		}
	}
	function tape_type(v	, x, y){
		y = substr(v, 2)+0
		x = int(y/10)
		if((x < 40) && (ok[y]!=1))
			return "old"
		else if((x%2) == 0)
			return "p"
		return "a"
	}
	BEGIN {
	base = "tlists!/"
	ok[10]=1; ok[11]=1; ok[12]=1; ok[13]=1; ok[14]=1; ok[15]=1; ok[16]=1; ok[17]=1; ok[18]=1; ok[19]=1
	ok[21]=1; ok[22]=1; ok[23]=1; ok[24]=1; ok[25]=1; ok[26]=1; ok[27]=1; ok[28]=1; ok[35]=1; ok[36]=1
	ok[37]=1; ok[38]=1; ok[39]=1; ok[40]=1; ok[41]=1; ok[42]=1; ok[43]=1; ok[44]=1; ok[45]=1; ok[46]=1
	ok[47]=1; ok[68]=1; ok[110]=1; ok[111]=1; ok[112]=1; ok[113]=1; ok[114]=1; ok[115]=1; ok[116]=1
	ok[117]=1; ok[118]=1; ok[119]=1; ok[121]=1; ok[122]=1; ok[123]=1; ok[124]=1; ok[125]=1; ok[126]=1
	ok[127]=1; ok[128]=1; ok[129]=1; ok[135]=1; ok[137]=1; ok[139]=1; ok[140]=1; ok[141]=1
	ok[142]=1; ok[143]=1; ok[144]=1; ok[145]=1; ok[146]=1; ok[147]=1; ok[262]=1; ok[263]=1; ok[265]=1
	ok[266]=1; ok[268]=1; ok[271]=1; ok[272]=1; ok[273]=1; ok[275]=1; ok[276]=1; ok[277]=1; ok[278]=1
	ok[285]=1; ok[286]=1; ok[287]=1; ok[288]=1; ok[289]=1; ok[297]=1; ok[300]=1; ok[304]=1; ok[305]=1
	ok[369]=1; ok[33]=1; ok[34]=1; ok[52]=1; ok[133]=1; ok[134]=1; ok[152]=1; ok[153]=1; ok[256]=1
	ok[257]=1; ok[258]=1; ok[259]=1; ok[0]=1; ok[6]=1; ok[364]=1; ok[365]=1; ok[366]=1; ok[367]=1
	ok[368]=1; ok[437]=1; ok[048]=1; ok[049]=1; ok[106]=1; ok[148]=1; ok[149]=1; ok[168]=1; ok[169]=1
	ok[370]=1; ok[371]=1; ok[372]=1; ok[373]=1; ok[374]=1; ok[375]=1; ok[376]=1; ok[377]=1; ok[378]=1
	ok[379]=1; ok[396]=1; ok[397]=1; ok[557]=1; ok[207]=1; ok[209]=1; ok[390]=1; ok[391]=1; ok[392]=1
	ok[393]=1; ok[394]=1; ok[395]=1; ok[398]=1; ok[399]=1; ok[173]=1
	# temp; these can contain old names jan 2009
	ok[212]=1; ok[214]=1; ok[238]=1; ok[239]=1; ok[233]=1; ok[230]=1; ok[232]=1; ok[231]=1; ok[235]=1
	ok[216]=1; ok[234]=1; ok[236]=1; ok[210]=1; ok[211]=1; ok[219]=1; ok[218]=1; ok[237]=1; ok[217]=1
	}
	 {
		key = $1 "," $2
		if(key != okey){
			if(okey)
				setc(sz)
			okey = key
			np = na = 0
			sz = $3
			delete arch
			delete prim
		}
		if(tape_type($4) == "a"){
			# archive
			na++
			arch[$4] = 1
		} else {
			np++
			prim[$4] = 1
		}
		allv[$4] = 1
	}
	END {
		if(okey)
			setc()
		gb = 1e-9
		for(v in allv) if(allv[v] == 1){
			printf("%s[%s]: a1+p0=%d,%.3fGB a1=%d,%.3fGB a2+=%d,%.3fGB p1a=%d,%.3fGB p1=%d,%.3fGB p2+a=%d,%.3fGB p2+=%d,%.3fGB\n",
				v, tape_type(v), arch1pp0_n[v], arch1pp0_b[v]*gb,
				arch1_n[v], arch1_b[v]*gb, arch2p_n[v], arch2p_b[v]*gb,
				prim1a_n[v], prim1a_b[v]*gb, prim1_n[v], prim1_b[v]*gb,
				prim2pa_n[v], prim2pa_b[v]*gb, prim2p_n[v], prim2p_b[v]*gb)
		}
	}'
done | sort

mv tlists tlists.$$ 2> /dev/null
mv tlists! tlists
rm -fr tlists.$$ 2> /dev/null
}

cd $NG_ROOT/site/waldo
anal ${1:-STK_SL500_8222ff}
anal1 ${1:-STK_SL500_8222ff}

cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_summary:Summarise vetted tapes)

&space
&synop	ng_tp_summary [changer]

&space
&desc	This generates a summary (on standard output) of tapes that have been vetted.
	It must be run on $srv_FLOCK_WALDO. The changer argument has an obsure format;
	see the source for details.
&space
	There are two types of lines intermixed in teh output.
	Lines with a colon (\&bold(:)) are totals (of file numbers and sizes).
	Other lines refer to sets of files; each set is identified by the group
	of tapes that the files were recorded on. Ordinarily, sets have two tapes
	(primary and archive), but it is unexceptional for single segments (approximately 2GB)
	to be present on three (and rarely four) tapes.
&space
&exit	Always 0.

&space
&see	??

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	April 2006 (ah) : Sunrise.
&term	15 Oct 2007 (sd) : Changed hardcoded /ningaui to NG_ROOT.
&endterms

&scd_end
------------------------------------------------------------------ */
