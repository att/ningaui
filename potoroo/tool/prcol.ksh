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
ustring='[column-number]'

while [[ $1 == -* ]]		
do
	shiftcount=1			# default amount to shift at end of loop 
	case $1 in
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)				# invalid option - echo usage message 
			error_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac				# end switch on next option character

	shift $shiftcount		# shift off parameters processed during loop
done					# end for each option character

awk '{ print $'"${1:-1}"' }'

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_prcol:Print Column)

&space
&synop	ng_prcol column-number

&space
&desc	&This reads from standard input and writes to the standard output 
	only the column specified on the command line.

&space
&space
&parms	The column number to print is expected on the command line. If
	not supplied the default is the first column.

&space
&exit	Always 0.

&space
&mods
&owner(Gus MacLellan)
&lgterms
&term	18 Dec 2006 (sd) : Added doc.


&scd_end
------------------------------------------------------------------ */
