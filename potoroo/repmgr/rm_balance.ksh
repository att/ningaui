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
                                   
#
# ------------------------------------------------------------------------

perform=true
ustring='[-n]'
while [[ $1 == -* ]]
do
	case $1 in
		-n)	perform=false;;

		--man)	show_man; cleanup 1;;
		-v)	verbose=1; verb=-v;;
		-\?)	usage; cleanup 1;;
       		*)
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac				# end switch on next option character

	shift 
done					# end for each option character

reqf=/tmp/breq
dump=$NG_ROOT/site/rm/dump

> $reqf


filer="${srv_Filer:-no-filer}"

egrep -v 'instance|attempt' < $dump | awk -v ldr=$filer '
/^file /	{ size = substr($4, 6)*1e-6; md5 = substr($3, 5); next }
/^nodes:/	{
		while(getline){
			if(NF == 0) break
			printf("node %s %d %d\n", $1, $3, $4)
		}
	}
NF > 1		{ if(NF!=2){
			size = $4*1e-6
			md5 = $3
		}
		printf("file %s %s %.3f %s\n", $2, $1, size, md5)
	}
	{ ; }
' | sort +3nr | tee /tmp/q | awk -v ldr=$filer -v reqf=$reqf -v lease=$((`date -f%#` + 3600)) '
function pr(		i){
	for(i in dused) printf(" %s=%.1f", substr(i, 5), 100*dused[i]/total[i])
	printf("\n")
}
function sort(		going, a, t){
	for(a in dused) pused[a] = dused[a]/total[a]
	for(going = 1; going;){
		going = 0
		for(a = 0; a < n-1; a++){
			if(pused[ref[idx[a]]] > pused[ref[idx[a+1]]]){
				t = idx[a]; idx[a] = idx[a+1]; idx[a+1] = t
				going = 1
			}
		}
	}
}
$1=="file"	{ used[$3] += $4; file[$3,++fn[$3]] = $2; size[$3,fn[$3]] = $4; md5[$3,fn[$3]] = $5 }
$1=="node"	{ total[$2] = $3+0; dused[$2] = $3-$4 }
END	{
		tol = 10	# 10% difference between nodes
		for(i in dused) used[i] += 0
		for(i in dused) printf("%s: used %.3fGB, total %.3fGB, %.1f%% full\n", i, dused[i]*1e-3, total[i]*1e-3, 100*dused[i]/total[i])
		total[ldr] *= .7	# give leader an extra 30% buffer
		pr()
		n = 0
		for(i in dused){
			idx[n] = n
			ref[n] = i
			fi[i] = 1
			n++
		}
		for(i = 0; i < 500; i++){
			sort()
			lo = ref[idx[0]]
			hi = ref[idx[n-1]]
			if(fi[hi] > fn[hi]) break	# no more files to move
			if((dused[hi]/total[hi]) < (dused[lo]/total[lo]+tol/100)) break
			m = size[hi,fi[hi]]
			if(m < 100) break		# too small a file (100MB)
			if((i%50) == 0) printf("lo=%.1f%%=%.0f(%s) hi=%.1f%%=%.0f(%s): move %.3f\n", 100*dused[lo]/total[lo], dused[lo]*1e-3, lo, 100*dused[hi]/total[hi], dused[hi]*1e-3, hi, m)
			dused[lo] += m
			dused[hi] -= m
			printf("hint %s %s %d %s\n", md5[hi,fi[hi]], file[hi,fi[hi]], -2, lo) >reqf
			printf("hint %s %s %d !%s\n", md5[hi,fi[hi]], file[hi,fi[hi]], -2, hi) >reqf
			fi[hi]++
		}
		printf("after %d moves:\n", i)
		pr()
}'

if [[ $perform == true ]]
then
	ng_rmreq < $reqf
fi

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_balance:Balance Replication manager files between nodes)

&space
&synop  ng_rm_balance [-n] [-v]


&space
&desc   &This balances files under the control of replication manager beween nodes.
	The script generates a series of hints that are passed to replication manager. 
	These hints suggest where replication manager should move files. 
&space

&options	These options are supported:
&begterms
&term	-n : No execute mode.  A list of files that would be moved is created, but 
	no action is taken to move the files.
&endterms

&space
&exit
	An exit code that is not zero indicates that the files could not be added to the 
	database. 

&space
&see    ng_flist_add, ng_flist_del, ng_flist_send, ng_repmgr, ng_repmgrbt
&space
&owner(Andrew Hume)
&lgterms
&term	19 Oct 2004 (sd) : removed ng_ldr refs. 
&term	01 Aug 2006 (sd) : Added manual page and beefed up options processing.
&endterms
&scd_end

