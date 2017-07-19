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
# Mnemonic: breaklock.ksh
# Author:   E. Scott Daniels
# Date:     26 December 1997
#
# ----------------------- SCD AT END -----------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

ustring="[-q] [-v] filename"
verbose=0
execute=true             # set to false for query only mode

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
while [[ $1 = -* ]] 
 do
  shiftcount=1                          # default # to shift at end of loop 
  case $1 in 
    -q) execute=false ;;                # query mode
    -v) verbose=1 ;;                 
	--man)	show_man; cleanup 1;;

    -\?) usage                          # standard help request
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

# at this point positional parms are left in $1, $2 ... $n



 if [[ -z "$1" ]]
  then
   usage
   cleanup 2 
  fi

if [[ "$execute" = "true" ]]
 then
	if ! amIreally ${NG_PROD_ID}
   	then
    		log_msg "Must be logged in as $NG_PROD_ID to execute."
	fi
    	cleanup 2 
   fi
 fi

 if [[ -d $1.lck ]]                      # ensure directory is there 
  then
    x=`ls $1.lck/lock*`                        
    opid=${x##*.}                        # get the owning PID 
    if [[ -n "$opid" ]]
     then
      y=`ps -ef | grep $opid | awk -v opid=$opid '{
          if( opid == $2 )
           {
            if( index( $5, ":" ) )
             printf( "%s %s %s %s", $1, $5, $7, $8 )
            else
             printf( "%s %s %s %s %s", $1, $5, $6, $8, $9 )  # old process
            found=1 
           }
         }
         END { 
           if( !found )
            printf( "Owning process not found in system" );
         }
         '`

      msg="$1; owned by $opid ($y)"
     else
      msg="$1; No lock file in directory, cannot determine owner"
     fi

    if [[ "$execute" = "true" ]]
     then
      ng_log $PRI_NOTICE $0 "Breaking lock on: $msg"
      rm -rf $1.lck                        # remove the lock
      error_msg "Lock broken: $msg"
     else
      error_msg "Current time: `ng_dt -p`"
      error_msg "Lock on $msg"
      error_msg "lock info file stats follow: "
      if ls -al $1.lck/* 2>/dev/null
       then
        for f in `ls $1.lck/*`
         do
          error_msg "Owning process: " `cat $f`
         done
       else
        error_msg "Info file does not exist, lock directory stats:"
        ls -ald $1.lck
       fi
     fi

  else                                   # no lock file existed
   error_msg "Lock not found for: $1" 
  fi

cleanup 0                  # trash temp files etc, then get out

# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_breaklock:Break a lock on a file.)

&space
&synop	ng_breaklock filename

&space
&desc	&ital(Ng_breaklock) will force a lock on a file to be released
	and report statistics on the lock owner to the Ningaui master 
	log. The user must be logged in under the ningaui user id in 
	order to use this command, unless the command is being executed
	in query (-q) mode. 

&space
&parms  The script accepts only one parameter which is the name of 
	the file to be unlocked. 

&space
&opts
&begterms
&term	-q : Query mode. The file listed on the command line will be
	checkecked for locks, and if locked details about the lock will 
	be written to the standard error device.
&endterms

&space
&exit	The script exits with a 0 if the lock was successfully broken
	and a non-zero exit code is issued when the lock could not 
	be found, parameter(s) were bad, or the user was not logged in 
	under the Ningaui user id. 

&space
&see	/ningaui/lib/stdfun

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Dec 1997 (sd) : Original code.
&term   06 May 1997 (sd) : To add query mode code.
&term	10 May 2001 (sd) : Ported to Ningaui.
&term	01 Oct 2007 (sd) : Spruced up manual page.
&endterms
&scd_end
