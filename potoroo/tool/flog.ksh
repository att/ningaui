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
# Mnemoni: flog.ksh - format things from the log
# Author:  E. Scott Daniels
# Date:    1 December 1997

_NODEBUG=1

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

ustring="[-n] <logfile"
space=1                 # space between lines

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
while [[ ${1%%[!-]*} = "-" ]]    # while "first" parm has an option flag (-)
 do
  shiftcount=1                          # default # to shift at end of loop 
  case $1 in 
    -n) space=0 ;;                      # no space between lines
    -v) verbose=TRUE ;;                 

    -\?) usage                          # standard  help request
         cleanup 1                      # get out - dont execute anything
         ;;
      
    -*)                                 # invalid option - echo usage message 
       error_msg "Unknown option: $1"
       usage
       cleanup 1
       ;; 
   esac                         # end switch on next option character

  shift $shiftcount             # shift off parameters processed during loop
done                            # end for each option character

awk -v space=$space '
{
 date=substr( $1, index( $1, "[" ) + 1, 15 )
 $1 = sprintf( "%s/%s/%s %s:%s:%s", substr( date, 5, 2),
         substr( date, 7, 2 ), substr( date, 3, 2 ),
         substr( date, 9, 2 ), substr( date, 11, 2),
         substr( date, 13, 2) )

 if( match( $4, ".+/" ) )
  x = RSTART + RLENGTH
 else
  x = 1
 $4 = substr( $4, x )
 print
 if( space )
  printf "\n"
}
' <$1

cleanup 0


# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	ng_flog - Format Gecko log messages.
&name &utitle

&space
&synop	/bin/ng_flog [-n] <logfile

&space
&desc	The &ital(ng_flog) script will read the standard input and 
	format the date and time string and the process name found
	on the input records.  Output is written to the standard 
	output device.  The time stamp found in the input records 
	is formatted, and the process name is stripped of its
	leading directory name(s). A blank line is written between 
	records to make reading easier.

&space

&opts
	The following options are supported by this script.
&begterms
&term 	-n : No space option. Blank lines will not be inserted between
	log messages as they are written to the standard output device.
&endterms

&space
&files  /ningaui/site/log/master

&space
&see	ng_glog

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	1 Dec 1997 (sd) : Original code
&endterms
&scd_end
