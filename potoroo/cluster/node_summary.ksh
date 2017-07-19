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
# Mnemonic:	ng_node_summary
# Abstract:	generate a summary of node stats
#		
# Date:		2001 sometime.
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
while [[ $1 == -* ]]
do
	case $1 in 

		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

cleanup 0



x=`awk '/kernel: Linux version/ { n = NR; if(syslogd) n = syslogd }
/syslogd.*: restart/	{ syslogd = NR }
END	{ if(n == "") n = 1; print n }' < messages`
sed -n "$x,\$p" < messages > messages.recent

awk '
function scrub(x,	y){
	y = x
	sub("[:,]$", "", y)
	return y
}
/kernel: Memory:/	{ split($7, y, "/"); printf("mem_total %d\nmem_avail %d\n", y[2], y[1]); }
/kernel: hd/		{
			if(index($0, "probe failed")) next
			match($0, "kernel: hd")
			i = "hd" substr($0, RSTART+RLENGTH, 1)
			x = substr($0, RSTART+RLENGTH+2)
			sub("^ *", "", x); sub(" *$", "", x)
			gsub(" ", "_", x)
			ide[i] = x
			}
/kernel: Detected scsi/	{
			printf("scsi_name %s %s %s_%s_%s_%s\n", $9, $8,
				scrub($11), scrub($13), scrub($15), scrub($17))
			}
/kernel:.*hdwr sector/	{ printf("scsi_size %s %s %s\n", scrub($8), $14, $11) }
/init: Entering runlevel: 3/	{ printf("runlevel3 %s\n", substr($0, 1, 15)) }
/kernel: Floppy drive/	{ printf("floppy %s %s\n", $8, $10) }

END	{
	for(i in ide) printf("ide_desc %s %s\n", i, ide[i])
}' < messages.recent > midden

awk -v ngroot=${NG_ROOT:-/foo} '
function ul(x,	y){
	y = x
	gsub(" ", "_", y)
	return y
}
function trim(x,	y){
	y = x
	sub("^0*", "", y)
	if(y == "") y = "0"
	return y
}
/^= scsi/	{
		while(getline){
			if($1 == "=") break
			if($1 == "Host:") id = $2 "_" trim($4) "_" trim($6) "_" trim($8)
			if($1 == "Vendor:") desc = $2 " " $4
			if($1 == "Type:"){
				what = $0
				sub("^.*Type: *", "", what)
				sub(" *ANSI.*", "", what)
				printf("scsi_desc %s %s %s\n", id, ul(desc), ul(what))
			}
		}
	}
/^= ide/	{
		while(getline){
			if($1 == "=") break
			printf("ide_prod %s %s %s\n", $1, $2, $3)
		}
	}
/^= cpu/	{
		ncpu = 0
		while(getline){
			if($1 == "=") break
			if(index($0, "model name")){
				name = $0
				sub("[^:]*: *", "", name)
			}
			if(index($0, "cpu MHz")){
				mhz = $0
				sub("[^:]*: *", "", mhz)
			}
			if(index($0, "cache size")){
				cache = $0
				sub("[^:]*: *", "", cache)
			}
			if(index($0, "bogomips")){
				cpu[mhz "MHz " name ", " cache " cache"]++
				ncpu++
			}
		}
		printf("cpu_n %d\n", ncpu)
		for(i in cpu) printf("cpu_type %d %s\n", cpu[i], ul(i))
	}
/^= df/	{
		while(getline){
			if($1 == "=") break
			if(index($1, "/dev/sd") == 1){
				dev = $1; sub(".*/", "", dev)
				printf("mount %s %s %s\n", dev, $2, $7)
			}
			if(k = index($7, ngroot "/fs")){
				dev = $1; sub(".*/fs", "", dev)
				printf("datastore %s %s %s\n", dev, $3, $4)
			}
		}
	}
/^= fstab/	{
		while(getline){
			if($1 == "=") break
			if($4 == "noauto"){
				dev = $1; sub(".*/", "", dev)
				printf("mount %s %s %s\n", dev, $3, $2)
			}
		}
	}
/^= partition/	{
		while(getline){
			if($1 == "=") break
			if($1 == "Disk"){
				dev = $2; sub(".*/", "", dev); sub(":", "", dev)
				ncyl[dev] = $7;
			}
			if($1 == "Units") printf("geom %s %.0f\n", dev, $5 * $7)
			if(substr($1, 1, 1) == "/"){
				dev = $1; sub("/dev/", "", dev)
				printf("partition %s %d %d %d %d\n", dev, $2, $3+1-$2, $4, $5)
			}
		}
	}
/^= networking/	{
		while(getline){
			if($1 == "=") break
			if(match($0, "HWaddr ")){
				mac = substr($0, RSTART+RLENGTH, 17)
			}
			if(match($0, "inet addr:")){
				ip = substr($0, RSTART+RLENGTH)
				sub(" .*", "", ip)
			}
		}
		printf("ether %s %s\n", ip, mac)
	}
/^= clock/	{
		while(getline){
			if($1 == "=") break
			if(match($0, "^server ")){
				server = substr($0, RSTART+RLENGTH)
				sub(",.*", "", server)
			}
			if(match($0, "offset ")){
				offset = substr($0, RSTART+RLENGTH)+0
			}
		}
		printf("clock %s %.4f\n", server, offset)
	}
/^= install/	{
		cinstall = "-"
		binstall = cinstall
		while(getline){
			if($1 == "=") break
			if(match($0, "current ")){
				cinstall = ul(substr($0, RSTART+RLENGTH))
			}
			if(match($0, "backup ")){
				binstall = ul(substr($0, RSTART+RLENGTH))
			}
		}
		printf("install %s %s\n", cinstall, binstall)
	}
END	{
}' < devdetails >> midden

awk '
function space(x,	y){
	y = x
	gsub("_", " ", y)
	return y
}
$1=="cpu_n"	{ ncpu = $2 }
$1=="cpu_type"	{ if($2 != ncpu) printf("BAD!\n"); cpu = $3 }
$1=="mem_total"	{ mem = int($2/1024+.5) }
$1=="scsi_name"	{ type[$2] = $3; dmap[$4] = $2 }
$1=="scsi_size"	{ size[$2] = $3*$4 }
$1=="scsi_desc"	{ dev = dmap[$2]; vend[dev] = $3; what[dev] = $4 }
$1=="ide_desc"	{ ide3[$2] = $3 }
$1=="ide_prod"	{ ide1[$2] = $3; ide2[$2] = $4; nide++ }
$1=="datastore"	{ td += $3; tu += $4 }
$1=="clock"	{ tol = 0.00005; if(($3 > tol) || ($3 < -tol)) clockerror = $3+0 }
$1=="install"	{ instc = $2; instb = $3 }
END	{
	for(i in ide1) ide[space(ide1[i] " (" ide2[i] ": " ide3[i] ")")]++
	for(i in type){
		t = type[i]
		tt = sprintf("%s %s %s", vend[i], what[i], type[i])
		if(t == "disk") tt = sprintf("%.1fgB disk", size[i]*1e-9)
		if(t == "tape") tt = sprintf("%s tape", vend[i])
		if(t == "generic") tt = sprintf("%s %s", vend[i], what[i])
		scsi[space(tt)]++
		nscsi++
	}
	printf("%d %s CPU%s; %.0fgB memory\n", ncpu, space(cpu), (ncpu==1)?"":"s", mem)
	if(nide > 0) for(i in ide) printf("%d %s\n", ide[i], i)
	if(nscsi > 0) for(i in scsi) printf("%d %s\n", scsi[i], i)
	printf("datastore: %.1f/%.1fGB", tu*1e-6, td*1e-6)
	if(td) printf(" (%.1f%%)", tu*100/td)
	printf(" used\n")
	if(clockerror) printf("clock incorrect: delta %.4fs (tol=%.4fs)\n", clockerror, tol)
	printf("installs: %s(cur) %s(backup)\n", space(instc), space(instb))
}' < midden > summary





/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_node_summary:Generate Hardware Summary of Node)

&space
&synop	ng_node_summary

&space
&desc	&This will generate a summary of hardware and operating system information 
	to the &lit(summary) file in the current directory.


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	01 Oct 2007 (sd) : Added what might pass as the man page.
&endterms


&scd_end
------------------------------------------------------------------ */

