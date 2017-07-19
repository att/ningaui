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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN

# -------------------------------------------------------------------
ustring="[-v] [-d domain] [req ...]"
vtopts=''
domain=$FLOCK_default_tapedomain

while [[ $1 = -* ]]
do
	case $1 in
		-d)	domain=$2; shift;;
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

x=`echo FLOCK_tapedomain_$domain`
node=`ng_ppget $x`
if [[ -z "$node" ]]
then
	echo "can't find node for tape domain $domain" 1>&2
	exit 1
fi

if [[ -z "$@" ]]
then
	cat
else
	echo "$@"
fi | ng_sendtcp -t 300 -e '~done' -w ${node}:TAPE_CHANGER

cleanup 0



exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tpreq:Give commands to ng_tp_changer)

&synop  ng_tpreq [-v] [-d domain] [req ...]

&desc	&This passes requests for action to &ital(ng_tp_changer)
	and writes any output to its standard output. Requests are either
	supplied on the command line or on standard input. The commands are
	described in &ital(ng_tp_changer)'s man page. Multiple commands
	may be entered by simply catenating them (with no seperator).
	
&opts   &This understands the following options:

&begterms
&term 	-d domain : specify the tape changer domain. By default, it uses &cw($FLOCK_default_tapedomain).

&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	The program will return with a code of zero, unless errors are detected
	(in which case it exits with a code of one).

&see	ng_tp_changer

&mods	
&owner(Andrew Hume)
&lgterms
&term	jan 2006 (ah) : Birth.
&endterms

&scd_end
------------------------------------------------------------------ */
