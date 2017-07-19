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
ustring="[-l] [-s stem] [pool]"
stem=''
list=0
allpool='COMB FEEDS_CVE FEEDS_OTHER FEEDS_PLATYPUS FEEDS_RICS'
tmp=$TMP/tpc.$$

while [[ $1 = -* ]]
do
	case $1 in
		-l)	list=1;;
		-s)	stem=$2; shift;;
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

case $# in
0)	pool="$allpool";;
1)	pool=$1;;
*)	usage; cleanup 1;;
esac

cat > $tmp

if [[ $list -gt 0 ]]
then
	echo $allpool
	cleanup 0
fi

for p in $pool
do
	case $p in
	COMB)	cmd="gre '^pf-comb-'";;
	*)	cmd="ng_cuscus -S -M $tmp -a $p -n 2";;
	esac
	if [[ -z "$stem" ]]
	then
		eval "$cmd" < $tmp
	else
		eval "$cmd" < $tmp > $stem.$p
	fi
done

cleanup 0

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_pool_cuscus:Seperate into pools using cuscus)

&space
&synop	ng_tp_pool_cuscus [-v] [-l] [-s stem] [pool]

&space
&desc	&This is an implementation of the pool filter needed by some tape commands,
	based on &ital(ng_cuscus). It either lists the supported pools (using the &bold(-l)),
	or it filters filenames on its standard input. If a pool is specified,
	then only filenames belonging to that pool are output. If no pool is specified,
	then only filenames that belong to any pool are output.
&space
	The output appears on standard output, unless the &bold(-s) option is specified.
	In this case, the output for pool &ital(p) will go into the file &ital(stem)&cw(.)&ital(p).
&space
	&This preserves the order of filenames from input to output, except in the case
	of both no pool is given and no &bold(-s) option is specified; in this case,
	the order is not preserved.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms

&term	-l : List. List on standard output the supported pools.
&break

&term	-s stem : Split the input file names into pool-specific lists, each in a file
	whose name is &ital(stem) with the pool name as a suffix.
&break

&term	-v : Verbose.

&endterms

&space
&exit	&This should always exit with a good return code.

&space
&see	&ital(ng_cuscus), &ital(ng_tp_dump_pool)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	17 Aug 2006 (ah) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
