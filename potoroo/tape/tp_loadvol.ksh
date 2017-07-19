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
ustring="[-v] volid:dev"
vtopts=''
tpopts=''

while [[ $1 = -* ]]
do
	case $1 in
		-d)	tpopts="$tpopts -d $2"; shift;;
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
1)	;;
*)
	usage
	cleanup 1
	;;
esac

echo "$1" | IFS=: read volid dev

if [[ -z "$dev" ]]
then
	dev=1		# egregious, but occam says don'y multiply narbarlek variables
fi

ng_tpreq $tpopts loadvol $volid $dev | while read status x
do
	case $status in
	info:)
		echo "$status $x" 1>&2
		;;
	okay:)
		echo "$x" | read vol dte tid s d
		echo $s $d $tid
		exit 0
		;;
	error:)
		echo "mapdev error: $x" 1>&2
		exit 1
		;;
	*)
		echo "ng_tpreq error; exiting" 1>&2
		exit 1
		;;
	esac
done

exit 2

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_loadvol:Load a tape)

&space
&synop	ng_tp_loadvol [-v] volid[:drive]

&space
&desc	&This causes the specifed tape to be mounted on the specifed drive.
	If no drive is specified, then &cw(1) is used.
	The definition of drive is given in the &cw(loadvol) command in &ital(ng_tp_changer),
	but includes a DTE number (typically 0-3), or the drive id
	(the output of &ital(ng_tp_getid), typically something like &cw(IBM_3580_822312).
&space
	The name of the system and the device the volid is mounted on is printed on stdout.
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose.
&endterms

&space
&exit	&This normally exits with a good return code, but otherwise there will be diagnostics
	and nothing printed on stdout.

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
