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
ustring="[-c cluster]"
args=''
tmp=$TMP/tdp.$$

while [[ $1 = -* ]]
do
	case $1 in
		-c)	args="$1 $2"; shift;;
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

if [[ $# -gt 0 ]]
then
	usage
	cleanup 1
fi

cat > $tmp

for p in `$TP_POOLCMD -l`
do
	$TP_POOLCMD $p < $tmp | ng_tp_dump -t $p $args
done

rm $tmp
cleanup 0

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_dump_pool:Dump files to tape by pool)

&space
&synop	ng_tp_dump_pool [-v] [-c cluster]

&space
&desc	&This takes a set of (MD5,name) pairs on standard input, splits them into
	pool-specific lists, and runs a &ital(ng_tp_dump) on each list. The &bold(-c)
	option, if given, is passed through to &ital(ng_tp_dump).
&space
	The splitting into pools is done by the program specified in the pink pages variable
	&bold(TP_POOLCMD).

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms

&term	-c clust : The files to be dumped are on cluster &ital(clust).
&break

&term	-v : Verbose.

&endterms

&space
&exit	&This should always exit with a good return code.

&space
&see	&ital(ng_tp_dump_pool)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	17 Aug 2006 (ah) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
