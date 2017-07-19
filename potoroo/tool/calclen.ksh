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
# ------------------------------------------------------------------------
#  Mnemonic: calclen
#  Date:     November 1997
#  Author:   E. Scott Daniels
#  This script will read the copy book and generate a listing of each field
#  and the offset and length of each field. 
#  calclen <intput >output
# ------------------------------------------------------------------------


awk -v xref="/tmp/$LOGNAME.xref" '
function blen(type, p1, p2){
old = 0
  if(type == "X") return p1                 # chars
  if(type == "i") return p1                 # numbers         PIC 9999 DISPLAY
  if(type == "is") return p1                # signed numbers  PIC S999 DISPLAY
  if(type == "f") return p1+p2              # fixed point     PIC 9v99 DISPLAY
  if(type == "fs") return p1+p2             # signed fixed pt PIC S9V9 DISPLAY
  if(type == "I") return int((p1+2)/2)      # bcd + sign      PIC 9999 PACKED-
  if(type == "Is") return int((p1+2)/2)     # bcd + sign      PIC S999 PACKED-
  if(type == "F") return int((p1+p2+2)/2)   # signed bcd      PIC S9V9 PACKED-
  if(type == "b" || type == "bs" )          # binary          PIC 9999 BINARY
   {                                                          PIC S9V9 BINARY
    p = (10 ^ (p1 + p2)) -1                 # largest number described by pic
    if( p < 65536 )
     return 2 
    word = 4294967295                       # largest unsigned # in 4 bytes
    for( rv = 4; p > word; p /= word )      # count # words needed for 
     rv += 4                                # the largest number
    return rv                               # and send it back as length
   }
  if( type == "b8" ) return 8               # internal floating pt COMP-1
  if( type == "b4" ) return 4               # internal floating pt COMP-2

  return 0                                  # unrecognized type - error
}

function parse_pic( pic )   # generate lengths and format from pic clause
 {
  fmt_err1 = "\nFIX: *** ERROR: Unable to parse input line %d near: %s\n"

    if( match( pic, "X+" ) )                           # X type clause?
     {
      l = RLENGTH                             # save in case no ( found
      fmt = "X"
      if( match( substr( pic, RSTART ), "\\([0-9]+\\)" ) )
       {
        if( RSTART != 2 )                     # more than one X before (
         {
          printf fmt_err1, NR, pic >stderr
          exit( 1 )
         }
        l = substr( pic, 3, RLENGTH-2 ) + 0   # pull len from in ( ) 
       }

     }
    else
     if( match( pic, "9+" ) )                          # 999. or 99V9 clause 
      {
       l = RLENGTH
       if( index( pic, "S" ) == 1 ) 
        sign = 1
       fmt = "i"

       if( match(  substr( pic, RSTART), "\\([0-9]+\\)" ) )
        {
         if( RSTART != 2 )                     # more than one 9 before (
          {
           if( !(match( pic, "9+V" )) )        # not something like 9999V9(3)
            {                                  # then give up - who knows
             printf fmt_err1, NR, pic >stderr  # what it really is.
             exit( 1 )
            }
          }
         else
          l = substr( pic, sign+3, RLENGTH - 2 ) + 0   # pull length from  ( )
        }
      
       if( match( pic, "V9+" ) )                 # if decimal indicator
        {
         fmt = "f"                               # change to floating point
         l2 = RLENGTH - 1
         if( match(  pic, "V9\\([0-9]+\\)" ) )
          l2 = substr( pic, RSTART+3, RLENGTH - 4 ) + 0  # pull second length
         if( match(  pic, "V99+\\([0-9]+\\)" ) )     # check for V99(x)
          {                                  # then give up - who knows
           printf fmt_err1, NR, pic >stderr  # what it really is.
           exit( 1 )
          }
        }
      }                                      # end not 9
}                                 # end parse pic

function bldstmt( )          # build a statement from input 
{
 do                           # combine input recs into a complete statement
  {
   tbuf = $0                                # save buffer to truncate things
   if( length( tbuf )  > 71 )
    tbuf = substr( tbuf, 1, 71 )            # trash cols 72-80 if there

   tbuf = substr( tbuf, 7 )                 # truncate leading 6 characters

   gsub( " +$", " ", tbuf );                # nix trailing blanks 

   if( index( tbuf, "*" ) != 1 )                     # not comment
    statement = sprintf( "%s %s", statement, tbuf )  # build single statement
  }
 while( (index( statement, "." ) == 0) && getline )

 statement = substr( statement, match( statement, "[^ ]" ) ) #nix lead blanks

 if( !match( statement, "[0-9a-zA-z]" ) )
  tcount = 0
 else
  {
   gsub( ". *$", "", statement )             # dont need dot
   gsub( ":", "", statement )                # trash colons
   statement = toupper( statement )          # ensure uppercase
   tcount = split( statement, toks )         # tokenize  statement to parse
  }
}

 
# ---------------- end functions -------------------------------------------
#

BEGIN { 
 stderr = "/dev/tty"; 
 line = 0 
}

$1 == "88" {                                       # we trash level 88 stuff
 while( (index( $0, "." ) == 0) && getline )  ;    # get all up to end dot (.)
 next                                              
}                                                  # end trash 88 level things 



NF > 0 {                         # parse only non-blank lines
 statement = ""                  # initialize - combine statement into one rec.

 if( index( $0, "*" ) != 7 )         # if not a comment record 
  {
   bldstmt( )                        # build next statement to process 

   if( ! tcount )
     next                            # next record if no tokens in statement

   #gsub( "-", "_", toks[2] )                 # replace dashes in the name token
   #name = toks[2] "_" line                   # make name unique with line #
   #names[name] = 1                           # save name for redefines 
 
   rstring = " "
   sign = 0                                   # no sign yet
   pictok = 0                                 # indicate nothing found yet
   occurs = 1                                 # default occurrences
   l = 0                                      # initialize decimal field length
   l2 = 0                                     # initialize fraction length
   fmt = "none"                               # default to no format

   if( toks[1] <= rlevel )                    # end redefines skip
{
#printf "resetting rlevel from %d tok[1] = %d\n", rlevel, toks[1]
    rlevel = 0
}

   suf = ""
   for( i = 3; i <= tcount; i++ )             # parse for recognized tokens 
    {
     if( toks[i] == "PIC" || toks[i] == "PICTURE" )
       pictok = ++i                          # mark the picture clause value
     else
      if( toks[i] == "OCCURS" )              # save the # of occurrences
        occurs = toks[++i]
      else
       if( toks[i] == "REDEFINES" )          # build redefine string
        {
         if( ! rlevel )                      # if not already in one
          rlevel = toks[1]                    # save redefine level
        }
       else
        suf = suf toks[i]                    # collect suffix stuff

    }
  
   if( (!rlevel) && oidx )                    # if occurs is in progress
    {
     if( toks[1] <= olevel[oidx] )            # need to pop 
      {
       len += olen[oidx] * otimes[oidx] 
       t = sprintf( "Level %d is %d bytes & occurs %d more times (%d bytes)",
             olevel[oidx], olen[oidx], otimes[oidx], otimes[oidx] * olen[oidx] )
       printf "%55s %18s\n\n", t, len 
       otimes[oidx] = 0
       oidx--
      }
    }

   if( pictok )                        # this statement had a pic clause
    {  
     parse_pic( toks[pictok] )         # parse out picture clause
    }


   if( index( suf, "PACKED-DECIMAL" ) ) 
    fmt = toupper( fmt )
   else
    if( index( suf, "BINARY" ) )         
     fmt = "b"
    else
     if( index( suf, "COMP-3" ) )         
      fmt = toupper( fmt )
     else
      if( index( suf, "COMP-4" ) )         
       fmt = "b"
      else
       if( index( suf, "COMP-1" ) )         
        fmt = "B8"
       else
        if( index( suf, "COMP-2" ) )         
         fmt = "B4"
        else
         if( match( suf, "COMP[^-]*" ) )       
          fmt = "b"

   if( fmt != "F" && sign )
    fmt = fmt "s"


   n = toks[1] " " toks[2]                   # print 1st 5 tokens from stmt
   for( i = 1; i < toks[1] / 5; i++ )
     n = " " n 
   n = sprintf( "%-35s %s %s %s", substr( n, 1, 35), toks[3], toks[4], toks[5] )
   printf "%-55s", substr( n, 1, 55 ) 

   if( fmt != "none" )
    {
     slen = blen( fmt, l, l2 )                 # calc statement length

     printf "%3s  %5d %2d ", fmt, slen, occurs

     if( ! rlevel )                              # if not in redefine
      {
       len += slen * occurs                      # add to total length
       if( oidx )                                # in the middle of an occurance
        olen[oidx] += slen                       # add to group length 

       printf "%5d\n", len                       # only show len if increased
      }
     else
      printf "\n"                                # nice termination 
    }
   else
    {
     printf "\n" 
     if( ! rlevel )             # if not in the middle of a redefine
      {
       if( occurs > 1 )        # group occurs multiple times
        {
         oidx++                               # stack onto previous occurs
         olevel[oidx] = toks[1]               # save this level number
         otimes[oidx] = occurs - 1            # less one as 1st recorded alone
         olen[oidx] = 0                       # reset length 
        }
       else
        otimes[toks[1]] = 0 
      }
    }
   
  }           # end not a comment record
}             # end NF > 0

END{ printf "\nLength=%d\n", len }
'

exit $? 




--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&title	calclen - Calculate lengh of copybook defined data area
&name &utitle

&space
&synop	calclen <filename

&space
&desc 	&ital(Calclen) will parse a COBOL copy book and generate 
	a listing of the fields, the field type and the length of 
	each field.  This program is intended to provide an alternate
	means of verification of the &ital(ng_comp_cpy) script output.

&space
&exit	This script exits with the condidion code returned by the 
	&ital(awk) call.


&space
&mods
&owner(Scott Daniels)
&lgterms
&term 1 Nov 1997 (sd) : Original code
&endterms
&scd_end
