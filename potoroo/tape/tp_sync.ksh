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
ustring="[-v]"
vtopts=''

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

for i in FEEDS_OTHER FEEDS_RICS FEEDS_CVE FEEDS_PLATYPUS
do
	ng_tpreq pooldump $i | read x y z
	case $where in
	1)	where=2;;
	*)	where=1;;
	esac
	ng_tpreq pooladd TOVET $y:$where
done

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_sync:Synchronise the tape catalog and the current tapes)

&space
&synop	ng_tp_sync [-v]

&space
&desc	&This script arranges for the scheduling tape vets for the current
	working volid's for each feed bucket, by adding to the &bold(TOVET) pool.
	Regrettable, where they are vetted is hardcoded.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose.
&endterms

&space
&exit	&This should always exit with a good return code.

&space
&see	&ital(ng_tape_vet)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	15 aug 2006 (ah) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
