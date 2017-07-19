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
# Mnemonic: ng_mime.ksh
# Author:   E. Scott Daniels
# Date:     29 November 1997
#
# ----------------------- SCD AT END -----------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

d=`ng_dt -d`

# ----------------------------------------------------------------------
# Mime header/trailer things
# ----------------------------------------------------------------------
m_ver="MIME-Version: 1.0"
m_bound="--804306kad-802199ekd"                        # boundary string
m_sbound="--$m_bound"                                  # starting bound
m_ebound="$m_sbound--"                                 # ending bound
m_mixedtype="Content-Type: multipart/mixed; boundary=$m_bound"
m_texttype="Content-Type: text/plain; charset=US-ASCII"
m_namedtype="Content-Type: text/plain; charset=US-ASCII; name="

#m_id="Message-ID: NINGaui_$d_$$"		# depricated -- caller must setup header

ustring="[-6|q] -[c]  [-v] filename [filename...]"
verbose=0
crlf=false
unset sub           # must be unset, not ""

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
while [[ $1 != -- && $1 = -* ]]    # while "first" parm has an option flag (-)
do
	case $1 in 
		-6)	cteparms="$cteparms -6" ;;     # base64 option for cte
		-c)	crlf=true               ;;     # convert file to 0x0d0x0a lines
		-q) 	;;				# allow -q from ng_mailer (default if this is not supplied)
		-s)	shift;;			# ignored -- not supported any longer
		-v)	verbose=1               ;;

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

if [[ $1 = "--" ]]
then
	shift
fi

# at this point positional parms are left in $1, $2 ... $n


if [[ -z "$1" ]]
 then
  usage
  cleanup 1 
 else
  if ! [[ -r $1 ]]
   then
    error_msg "Cannot read file: $1"
    cleanup 1
   fi
 fi


# these added by ng_mailer now
#echo "Subject: ${sub:-No Subject}"
#echo $m_id

# --------------setup "header" for the MIME body -------------------------
echo "$m_ver"
printf "$m_mixedtype\n\n"
cat <<endcat
This message is in MIME Version 1.0 format.  Your mail reader seems not
to be able to understand it and thus some, or all, of this message
may be unreadable.
endcat

printf "\n$m_sbound\n"          # boundary for the first/main message
printf "$m_texttype\n" 	        # indicate the content type of this section

if ! ng_cte $cteparms $1              # encode main message 
 then
  cleanup $?
 fi

shift

while [[ -n "$1" ]]           # for each attachment file
 do
	if [[ $1 = "-" ]]
	then
		file=/dev/fd/0		# stdin
	else
  		if ! [[ -r $1 ]]            # ensure it is there and good
   		then
    			error_msg "Cannot read file: $1"
    			cleanup 1
   		fi

		file="$1"
	fi

 	printf "\n$m_sbound\n" 		# add boundary for next attachment
 	aname=${file##*/}			# make attachment name
 	printf "$m_namedtype\"$aname\"\n"	# add type of next section and name it

	if [[ "$crlf" = "true" ]]             # convert file to crlf before encoding
	then 
		cat $file | sed /$/s/// | ng_cte $cteparms   # encode the file 
	else
		ng_cte $cteparms $file
	fi

	if [[  $? -ne 0 ]]                     # cte failed
	then
		error_msg "Could not encode file: $file "
		cleanup $?
	fi
  
	shift
done

printf "\n$m_ebound\n"           # terminate the body of the message 

cleanup 0                  # trash temp files etc, then get out

# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_mime:Format a file and attachments for mailing)

&space
&synop	ng_mime [-c] [-6|q] filename1 [filename2... filenamen]

&space
&desc	&ital(Ng_mime) will create a document body in MIME format such that 
	the first file supplied on the command line is sent as the main 
	message, and the remainder of the files are sent as attachments
	to the main message. The contents of the files are encoded using 
	the quoted-printable algorithm in the &ital(ng_cte) program. 
	Attachments are encoded using the &ital(ng_cte) &lit(-c) option
	if it is passed in on the command line.
	The script writes the resulting message body to the standard 
	output device. 
&space
	Any RFC822, or other mailer specifications for mail headers, 
	are &stress(not) generated by &this.  The calling programme
	(like ng_mailer) is expected to 'wrap' the outpuf from &this 
	with the appropriate mail headers and trailers
&space
&space
&opts	This script will accept the following options if supplied on 
	the command line.
&begterms
&term   -6 : Causes &ital(ng_cte) to be called with the base64 encoding 
	option set for both the main message and attachments. 
&space
&term 	-c : Causes records in the attachment files to be converted to 
	carriage return/line feed terminated (CRLF) rather than terminated
	with just a newline character (0x0a). This conversion is done 
	prior to content transfer encoding by &ital(ng_cte).
&space
&term	-q : Use quoted printable encoding.  This is the default and 
	the option is supported to allow calling programmes to always
	be able to supply one of the encoding options.
&endterms

&space
&parms	The script accepts an unlimited number of parameters on the 
	command line. It expects that each of the parameters is a file
	name. The first name will be used as the main body of the message,
	and the contents of the other files will be added to the message
	as attachments. 

&space
&exit	The script will exit with a zero exit code if all processing 
	was successful and a non-zero return code if errors were 
	encountered. 

&notes
	Currently this script provides only for the shipment of plain, 
	ASCII, text files.  If it is necessary to send binary data, 
	the script must be changed to provide the necessary MIME headers
	and trailers to accomplish this.  Unpredictable results will 
	occur if a non-text file is submitted to this script. Binary 
	files can be sent if the sending programme first encodes them 
	using something like &lit(uuencode) which will convert the binary
	into an ASCII format.

&space
&see	ng_cte, RFC2045, RFC822.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Dec 1997 (sd) : Original code. 
&term	28 Jun 1999 (gus) : Changed to keep the txt extension on the attached filename.  Windows is so smart!
&term	10 May 2001 (sd) : Ported to Ningaui.
&term	10 Dec 2004 (sd) : Added fixed bugs associated with echo not working the same in all 
	environments (most notibly the support, or lack there of, for \n to generate a second newline in 
	the message.
&endterms
&scd_end
