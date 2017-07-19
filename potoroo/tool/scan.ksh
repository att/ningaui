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
#mod 19 Oct 2004 (sd) : remove ng_leader refs 

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

ustring="[-c cluster-name]"
cluster=""
filer=${srv_Filer:-no-filer}

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster=$2; shift;;

		--man)	show_man; cleanup 1;;
		*)	usage; cleanup 1;;
	esac

	shift
done

if [[ -z $cluster ]]			# get members of the cluster; not supplied -- assume our cluster 
then
	ng_members |read sys
else
	ng_rcmd ${cluster:-$filer} ng_members 2>/dev/null|read sys		# we assume that the cluster name is dns lookupable (ningbing)
fi

if [[ -z $sys ]]
then
	echo "no nodes found by ng_members"
	cleanup 1
fi

vars=${1:-RM_FS RM_SEND RM_RCV_FILE RM_DELETE RM_RENAME RM_DEDUP REFILL }

echo "`ng_sysname -s` `uptime`"

for i in $sys
do
	echo sys $i
	ng_wreq -t 5 -s $i explain $vars
done | awk -v rr="$vars" '
function dump(		i){
	printf("%s:", cursys)
	for(i = 1; i <= NCOL; i++){
		printf("\t%s", val[name[i]])
		delete val[name[i]]
	}
	printf("\n")
	if((++ndump)%5 == 0) printf("=\n")
}
BEGIN	{
	spacing = 2
	printf(".TS\nc")
	NCOL = split(rr, xx)
	for(i = 1; i <= NCOL; i++)
		printf("%d c", spacing)
	printf("\nc")
	for(i = 1; i <= NCOL; i++)
		printf("%d n", spacing)
	printf(".\n")
	for(i = 1; i <= NCOL; i++){
		name[i] = xx[i]
		printf("\t%s", xx[i])
	}
	printf("\n")
}
$1=="sys"	{
	if(cursys != "") dump()
	cursys = $2
	next
}
$1=="explaining"	{
	next
}
$1=="resource"	{
	i = $2; sub(":$", "", i)
	lim = $3; sub(".*=", "", lim)
	run = $4; sub(".*=", "", run)
	all = $5; sub(".*=", "", all)
	val[i] = sprintf("%d/%d\\& (%d)", run, lim, all)
	next
}
END	{
	if(cursys != "") dump()
	if((++ndump)%5 != 0) printf("=\n")
	printf(".TE\nend-of-file\n.br")
}' | nroff -t -Tascii 2>/dev/null | sed '/end-of-file/,$d'

exit 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_scan:Scan the cluster's woomeras for various activities)

&space
&synop	ng_scan [resources]

&space
&desc	 &This script scans teh nodes in teh current cluster and reports on various
	resources. By default, it examines RM_FS RM_SEND RM_RCV_FILE RM_DELETE RM_RENAME RM_DEDUP.
	The resources can be set by specifying in a single argument, another list.

&space
	Currently, the node list is set to our current prdouction cluster. Eventually,
	we'll figure out a deterministic way to do this.


&space
&see	ng_woomera(1)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Apr 2003 (sd) : Revamped to support mac and FreeBSD.
&endterms

&scd_end
------------------------------------------------------------------ */
