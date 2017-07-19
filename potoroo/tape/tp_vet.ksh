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
ustring="[-v] dev"
vtopts=''

while [[ $1 = -* ]]
do
	case $1 in
		-c)	vtopts="$vtopts -c";;
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

dev=$1

# in lieu of proper lookup
case `ng_tp_getid $dev` in
IBM*_4_*)	chg=IBM_03584L32_db49cb1;;
IBM*)		chg=STK_SL500_8222ff;;
SONY*)		chg=QSTAR_412180_995812;;
*)		echo "can't id drive $dev, can't id changer"
		exit 1
		;;
esac

y="pf-tfrag-$chg-`ng_dt -d`-$$"
x=`ng_spaceman_nm $y`
ng_tape_vet $vtopts -v -n $dev > $x
vol=`sed 1q $x | ng_prcol 4`
cp $x $TMP
ng_rm_register -H Steward $x

cleanup 0

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_vet:Vet a tape)

&space
&synop	ng_tp_vet [-v] [-c] dev

&space
&desc	&This script vets the tape on the specified device.
	The output is captured as a tape catalog fragment and registered with repmgr.
	Tapes can be vetted in this way multiple times without ill effect.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Copy the cpios to disk (uses the &bold(-c) option of &ital(ng_tape_vet)).
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
&term	22 aug 2005 (ah) : Birth.
&term	17 May 2006 (sd) : Remove assumption that NG_ROOT is /ningaui if not defined. 
&endterms

&scd_end
------------------------------------------------------------------ */
