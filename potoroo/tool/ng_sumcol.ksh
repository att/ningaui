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
# Owner: Gus MacLellan


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
ustring='[-gkmt] col'
unit=1
while [[ $1 == "-"* ]]
do
	case $1 in
	-t)	unit=1e12;;
	-g)	unit=1e9;;
	-k)	unit=1e3;;
	-m)	unit=1e6;;

	-v)	verbose=TRUE;;		# verbose flag 
	--man)	show_man; cleanup 2;;
	-\?)	usage; cleanup 1;;
	*)
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac

	shift 
done	

awk -v unit=$unit '
	{ t += $'$1' }
END { printf("%.0f\n", t/unit) }'

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sumcol:Sum the values in named column)

&space
&synop	ng_sumcol [-{g|t|m|k}] column

&space
&desc	&This reads from standard input and sums the indicated column. The 
	sum is written to the standard output after converting to the 
	desired units based on the supplied option. 

&space
&opts	One of the following flags may be supplied to indicate the units that 
	the result is written in.
&begterms
&term	-t : Terabytes.
&term	-g : Gigabytes.
&term	-m : Megabytes.	
&term	-k : Kllobytes.
&endterms


&space
&parms	The column number to sum must be supplied on the command line. 
	If a record in the input stream does not have the indicated number
	of columns unrpredictable results may occur.

&space
&exit	Always exits 0.

&space
&see	ng_prcol

&space
&mods
&owner(Gus MacLellan)
&lgterms
&term	19 Dec 2006 (sd) : Added man page.
&endterms


&scd_end
------------------------------------------------------------------ */
