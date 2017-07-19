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
#
# Mnemonic: trimlog.ksh
# Author:   E. Scott Daniels
# Date:     23 January 1997
#
# ----------------------- SCD AT END -----------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



ng_dt -c |read dow junk         # get current day of week
case $dow in                    # determine number of days to keep in the log
 Sun) days=1 ;;
 Mon) days=2 ;;
 Tue) days=3 ;;
 Wed) days=4 ;;
 Thu) days=5 ;;
 Fri) days=6 ;;
 Sat) days=7 ;;
esac
days=$(($days+7))

lname=$gecko/log/master                    # default log to work on 
ustring="[-d days] [-l logname]"

verbose=1
compress=false

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
while [[  $1 == "-"* ]]
 do
  case $1 in 
    -c)  compress=true ;;               # compress trimmings
    -d*) getarg days "$1" "$2" ;;           # allow override from default value

    -l*) getarg lname "$1" "$2" ;;

    -v) verbose=1 ;;

    -\?) usage     
         cleanup 1 
         ;;

	--man) show_man; cleanup 1;;
      
    -*)           
       error_msg "Unknown option: $1"
       usage
       cleanup 1
       ;; 
   esac 

  shift 
done   

exec >$NG_ROOT/site/log/trimlog.log  2>&1   # we keep our own log - dont use ng_log!!!

# at this point positional parms are left in $1, $2 ... $n

d=`ng_dt -d`                       # id for save file
hold=$lname.hold.$$.               # prevent cleanup from deleting it
sname=$lname.old.$d                # spot to save trimmings in
tlog=$lname.trimmed.$$             # what will become the new log

verbose "trimming $lname into $sname"
 
if [[ "$days" -gt 15 || "$days" -lt 2 ]]
 then
  log_msg "enter a more realistic days to keep value (between 2 and 16)"
  log_msg "log not trimmed, bad days to keep: $days"
  cleanup 1
 fi

if [[ ! -w $lname ]]
 then
  log_msg "cannot write to log: $lname"
  cleanup 1
 fi

if ! mv $lname $hold
 then
  log_msg "cannot move log to holdfile: $hold"
  cleanup 1 
 fi

days=$(( $days * 60 * 60 * 24 *10 ))    # convert days to tenths of seconds
today=`ng_dt -i`
tz=`ng_dt -i 19940101000000`                    # time zone correction 
today=$(( ($today - ($today % 864000) + $tz) )) # midnight today

stime=$(( $today - $days ))                     # split time for trim
verbose "split point in log will be: `ng_dt -p $stime` ($stime)"

touch $lname
touch $sname      # must touch to ensure that size check works later

if ! awk -v top=$sname -v bot=$tlog -v stime=$stime '
  {
   if( $0 + 0 < stime )    # modified to not barf on too many fields
    print >>top                   # trim upto split time
   else
    print >>bot                   # keep last n days
  }
  ' <$hold 
 then
  log_msg "awk failed, unable to trim log: $lname"
  mv $hold $lname    
  cleanup 1
 fi

verbose "gathering file sizes..."
nsize=`wc -l <$tlog`         # capture sizes to check for message loss
tsize=`wc -l <$sname` 
osize=`wc -l <$hold` 

verbose "validating file sizes..."

if [[ $(( $nsize + $tsize )) -eq $osize ]]           # ensure no messages lost 
 then
  verbose "sizes ok, deleting hold file. $nsize + $tsize = $osize"
  rm -f $hold >/dev/null              # trash the hold log cuz all went well!
  mv $tlog $lname                     # ok to move trimmed log to right place
 else
  log_msg "sizes dont balance, hold file ($hold) kept: $nsize + $tsize != $osize"
  mv $hold $lname                     # put the old log back the way it was
  cleanup -k 1
 fi

if [[ "$compress" = "true" ]]       
 then
  verbose "compressing trimmings"
  gzip $sname
 fi

cleanup 0                           # trash temp files etc, then get out

# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_trimlog:Trim the Ningaui Master Log.)

&space
&synop	ng_trimlog [-c] [-d days] [-l logname] [-v]

&space
&desc   &ital(Ng_trimlog) will trim a log file such that a specified number 
	of days prior to the current day remain in the log file. 
	The defaults are to trim the Ningaui master log and to keep 
  	the information in the active log from the previous Saturday at
	mignight through the current day.
	The trimmed material will be 
	saved using the same base name as the log, with an extension 
	of &lit(^.old.<date>). This command must be executed only when 
	no other processes are writing to the log, or unspecified 
	results will occur. 

&space
&opts	The following options are accepted by this script.
&begterms
&term   -c : Causes the trimmed material to be compressed.
&space
&term	-d days : Specifies the number of days to keep in the log. 
&space
&term 	-l logname : Supplies an alternate log file to work on. 
&space
&term   -v : Execute while blabbering on about things (verbose mode).
&endterms

&space
&exit   &ital(Ng_trimlog) will exit with a return code of 0 if all goes 
	well, and a non-zero return code if it cannot successfully 
	trim the specified log.

&errors Error messages generated by this script are written to a special 
	log file in the &lit(/gecko/log) directory.  Because the script 
	is trimming the master log the &ital(ng_log) script cannot be 
	used to record any messages that it must generate.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term   23 Jan 1998 (sd) : Original code. 
&term   02 Apr 1998 (sd) : Added default "keep" to hold back to the previous saturday.
&term   13 May 1998 (sd) : Touch new files to ensure they exist for wc command.
&term   18 Jan 1999 (sd) : So that it will not barf on too many fields on a log record.
&term	10 May 2001 (sd) : Converted from Gecko.
&term	19 Dec 2006 (sd) : Fixed so that man page shows.
&endterms
&scd_end
