#
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


#  Mnemonic: ng_mailer.ksh
#  Date:     1 February 1997
#  Author:   E. Scott Daniels
# 
# --------------- SCD documentation section at end -------------------------



CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

TMP=${TMP:-/tmp}
if [[ -z $LOGNAME ]]
then
	if [[ -n $USER ]]
	then
		LOGNAME=$USER
	else
		LOGNAME=${HOME##*/}
	fi
fi

# set version specific options (echoed from the function) based on the send mail
# version that is installed. We determine the version installed by connecting to the 
# sendmail daemon on the local host and examining the welcome message (no magic -version
# option in the sendmail command)
function set_mailopts
{
#DEPRECATED -- we assume all installations of sendmail are 8.12 or better
	return



        typeset maj=""
        typeset min=""

        echo "quit" |ng_sendtcp localhost:25 2>/dev/null |awk '
                {
                        if( $4 == "Sendmail" )
                                split( $5, a, "." );
                        print a[1], a[2];                       # just major and minor version
                }
        ' | read maj min

        case $maj.$min in
                8.1[2-9]|8.[2-9]*) ;;                    # no default options from 8.12 out
                *)      echo "-U";;                     # 8.11 and before need -U
        esac
}

# bloody suns..... 
# cannot count on where sendmail is installed, so we search for it 
#
function set_path
{
	if ! whence sendmail >/dev/null
	then
		for x in /usr/sbin /usr/lib /usr/local/bin
		do
			if [[ -x $x/sendmail ]]
			then
				sendmail_path=$x/
				return
			fi
		done
	fi
}

# add one or more addressbook to the list; verify they exist first
function add_pb
{
	while [[ -n $1 ]]
	do
		if [[ -f $1 ]]			# ensure it exists
		then
			phonebook="$phonebook$1 "        
		else
			log_msg "Unable to find phonebook: $book; ignored"
		fi

		shift
	done
}

# convert a list of nicknames into real addresses. If a real address is mixed into the 
# list, then who cares we will let it through
function cvt_names
{
	verbose "cvt_names: $@"

	awk -v verbose=$verbose -v list="$*" '  
	BEGIN { 
		id=1                           		       # field user id is in
		address=2              		               # field that mail addr is in
	} 
	{							# for each phone book record (stdin) map a nickname to an alias name
		if( NF > 0 && (substr( $1, 1,1) != "#") )	# dont do comment/blank line
		{
			if( $(id) == "+" )                                 # mailing list entry (+ listname member-nick member-nick...)
			{
				alias[$(address)] = $3                  	# save first name 
				for( i = 4; i <= NF; i++ )              	# for each additional name 
					if( substr( $(i), 1, 1) !=  "#" )   	# ensure remainder not commentted out
						alias[$(address)]= sprintf( "%s %s", alias[$(address)], $(i) ) 
					else
						i = NF + 1              # comment - ignore rest
			}                       
			else						# regular entry: nickname address
				addr[$(id)] = $(address)                
		}
	}

	# return 1 if address was already passed through this function
	function is_duplicate( a )
	{
		if( !noticed[a]++ )
			return 0;
		
		if( verbose )
			printf( "ng_mailer: duplicate removed from list: %s\n", a ) >"/dev/fd/2";
		return 1;
	}

	END {
		for( idx in alias )                             # for each alias (list), map to individual address(es)
		{ 
			split( alias[idx], alist )              # split names in this alias
			for( a in alist )                       # for each alias name in the list
			{
				if( addr[alist[a]] != 0  )	# maps to an address
				{
					addr[idx] = sprintf( "%s %s", addr[idx], addr[alist[a]] )  # add addr 
				}
				else
				{
					printf( "##NOMAP## %s/%s ", idx, alist[a] ) # show error msg to user
					rc = 1
				}
			}                  # end for each name in alias list
		}                    # end for each alias entry encountered in phonebook
   
		split( list, to, " " )     #  now map names in the list to addresses

		for( idx in to )                   # for each to entry 
		{
			if( index( to[idx], "@" ) )
			{
				if( ! is_duplicate( to[idx] ) )
					printf( "%s ", to[idx] )   # assume a real address entered from the command line
			}
			else
			if( index( to[idx], "!" ) == 1 )	# !daniels -- let it go as daniels (assume local host)
			{
				if( ! is_duplicate( substr( to[idx], 2 ) ) )
					printf( "%s ", substr( to[idx], 2 ) );
			}
			else
			{
				if( addr[to[idx]] != 0 )       # if an address for that user was defined
				{
					if( ! is_duplicate( addr[to[idx]] ) )
						printf( "%s ", addr[to[idx]] )   # print the address for the person
				}
				else
				{
					printf( "##NOTFOUND## %s ", to[idx] )   # show userid not found in book
					rc = 1                                  # bad exit code to force error
				}
			}
		}
    		exit rc                                    # let script know how we did
   	}				# END
'
}

# creates an rfc list of names 
function make_rfc_list
{
	typeset rfc_list=""

	while [[ -n $1 ]]
	do
		if [[ -z $rfc_list ]]
		then
			rfc_list=$1
		else
			rfc_list="$rfc_list, $1"
		fi

		shift
	done

	echo $rfc_list
}

# make an RFC-822 email header
function make_header
{
	typeset x=""
	typeset rfc_alist=""
	typeset tz=""
	typeset domain=""

	case `date +%Z` in		# ast date does not do %z in rfc format
		EST)	tz="-0500";;
		CST)	tz="-0600";;
		MST)	tz="-0700";;
		PST)	tz="-0800";;

		EDT)	tz="-0400";;
		CDT)	tz="-0500";;
		MDT)	tz="-0600";;
		PDT)	tz="-0700";;
		*)	tz="";;
	esac
	typeset rfc_date=`date "+%a, %d %b %Y %H:%M:%S $tz"`

	if [[ -n $NG_DOMAIN ]]
	then
		domain="$(ng_sysname).$NG_DOMAIN"
	else
		domain=$(ng_sysname -f)
	fi

	echo "From: $from_name <${LOGNAME:-$USER}@$domain>"
	echo "To: `make_rfc_list $alist`"

	if [[ -n $clist ]]
	then
		echo "CC: `make_rfc_list $clist`"
  	fi

	if [[ -n $blist ]]
	then
		echo "BCC: `make_rfc_list $blist`"
  	fi

	echo "Date: $rfc_date"
	echo "Subject: $sub"
	echo "Message-ID: KM012977_`ng_dt -i`_$$"		# constant for easy searching, and random rest
	echo "X-Mailer: NINGAUI ng_mailer 3.0/02125"
}

#
# ------------------------------------------------------------------------
# setup the "environment"
# ------------------------------------------------------------------------
#
trap "cleanup 1" 1 2 3 14 15 16   # catch signals to ensure tmp file is removed

ustring="[-B bcc-list] [-C cclist] [-T to-list] [-b phonebook] [-m mime-opt] [-n] [-s subject]  [-v] file [file...]"
sname=$0                            # save script name for error messages
vmsg="Sending"                      # start of verbose message
verbose=0                           # default to quiet mode
execute=1                           # actually send file if true
mime=0                          	# format in MIME format
unset mimeparms
sub="no subject given"
default_phonebook="$NG_ROOT/lib/addressbook"
from_name=""
sm_ver_opts=""				# version specific sendmail options
testing=0					# special testing code
parms="$@"


#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
while [[ $1 != - && $1 = -* ]]    # while "first" parm has an option flag (-)
do
	shiftcount=1                               # # to shift at end of loop 
	case $1 in 
		-B)	bcc_list="$bcc_list$2 "; shift;;
		-C)	cc_list="$cc_list$2 "; shift;;
		-T)	to_list="$to_list$2 "; shift;;

		-b)     add_pb $2; shift;;	# add all supplied in $2 to the list; verify they exist

		-f)	from_name="$2"; shift;;

		-m*)	m="$2"					# mime option of somesort
			
			case $m in 
				-6| 6| -q | q | -c | c) ;;	# legit
				*)	log_msg "illegal mime option: $m"; usage; cleanup 1;;
			esac
			mimeparms="$mimeparms -${m#-}" 

			shift;
			mime=1                          # format input files in MIME
			;;


		-n)	verbose=1  
			execute=0                         # don't actually send file
			forreal="echo would "
			vmsg="Would send"                     # not sending, indicate this 
			;;

		-s)	sub="$2"; shift;;
	
		-v)	verbose=1 
			#smverbose="-v"			# verbose setting for send mail (3/21/07 -- this seems to now trigger a verification report so we drop it -- grumble)
			;;
	
 
		-X)	testing=1;;
		-x)	;;                      # nolonger supported; back compat with old gecko stuff

		--man)	show_man; cleanup 1;;
		-\?)	usage 
			cleanup 1 
			;;
      
		-*)                     # invalid option - echo usage message 
			log_msg "Unknown option: $1"
			usage
			cleanup 1
			;; 
	esac                    # end switch on next option character

  	shift 
done                     # end for each option character

if [[ $1 = "--" ]]
then
	shift
fi


			# we assume remaining parms are file(s) to send
list="${to_list% }"		
if [[ -z $list ]]
then
	log_msg "to list was omitted or empty"
	cleanup 1
fi

# 
# -------------------------------------------------------------------------
# validate files, invoke awk program we generated to create an 
# address list. Check for errors and if there were no errors, mail the 
# file to each address in the list. 
# -------------------------------------------------------------------------
#
if [[ -z "$1" ]]               # file not set - probably no parameters 
then
	log_msg "no file(s) supplied on the command line"
	usage
	cleanup 1
fi

files="$@"

for x in $files
do
	case $x in
		-) ;; 			# allow stdin as a file on cmd line
		*)
  			if [[ ! -r "$x" ]]                        # ensure file given is there 
   			then
    				log_msg "cannot find or read file: $x"
				cleanup 1
			fi
			;;
	esac
done

if [[ -f $default_phonebook ]]		# the ning default exists
then
	phonebook="$phonebook$default_phonebook"
else
	if [[ -z "$phonebook" ]]          # no phonebook supplied on commandline; and no default
	then
		phonebook=/dev/null
		verbose "no phonebook supplied; all addresses expected to be of the form name@domain"
	fi
fi

cvt_status=0
alist=`cat $phonebook | cvt_names ${list//,/ }`		# convert nicknames to addresses (allow list to be comma separated)
cvt_status=$(( $cvt_status + $? ))

blist=`cat $phonebook | cvt_names ${bcc_list//,/ }`
cvt_status=$(( $cvt_status + $? ))

clist=`cat $phonebook | cvt_names ${cc_list//,/ }`
cvt_status=$(( $cvt_status + $? ))


if [[ $cvt_status -gt 0 ]]                                  # handle errors from 
 then                                              # the awk program
  log_msg "errors converting nicknames ($cvt_status)"
	verbose "alist=$alist"
	verbose "blist=$blist"
	verbose "clist=$clist"
  log_msg "Phonebooks used: $phonebook"
	
  if [[ -z $alist ]]
  then
	log_msg "no addresses were produced"
	cleanup 1
  fi

  set $alist $blist $clist                        # look for ##xxx## tokens
  while [[ -n "$1" ]]                              # in the list from awk 
   do
    if [[ "$1" = "##NOTFOUND##" ]]         # indicator that next is a bad name
     then
      log_msg "Could not find name in any phonebook: $2"
      shift
     else
      if [[ "$1" = "##NOMAP##" ]]                   # next token is bad alias
       then
        log_msg "No map for alias: $2"
        shift
       fi
     fi
    shift                                            # move to look at next 
   done
  cleanup 1               # stop processing after all errors were echoed
 fi


#
# ------------------------------------------------------------------------
#  address list was generated without errors, send mail to each address
#  in the list echoing information if in verbose mode
# ------------------------------------------------------------------------
#

if [[ $verbose -gt 0 ]]            # show address and phonebook that we 
then                              # found the address in if verbose mode
	for addr in $alist $blist $clist     # clean address list - run each one
	do
		grep -l $addr $phonebook 2>/dev/null | read first rest		# indicate which phonebook was used 
		log_msg "$vmsg $files to $addr ($first)"
	done
fi

make_header >$TMP/mail.$$

if (( $mime > 0 ))
then
	if [[ $files =  "- "* || $files = *" - "* || $files = *" -" ]]
	then
		verbose "reading stdin"
		while read x 
		do
			printf "$x\n"
		done >$TMP/stdin.$$
		files="${files%%-*} $TMP/stdin.$$ ${files##*-}"		# replace - with our tmp file
		verbose "filelist modified to include stdin, newlist: $files"
	fi
	verbose "converting to mime format: mimeparms= $mimeparms"
	if ! ng_mime $mimeparms -v -s "$sub" $files >>$TMP/mail.$$
	then
		log_msg "Could not convert files to MIME format."
		cp $TMP/mail.$$ $TMP/${USER:-foo}.deadletter
		cleanup 1
	fi
else
	echo "" >>$TMP/mail.$$					# must have a blank line
	if [[ $files =  "- "* || $files = *" - "* || $files = *" -" ]]
	then
		if (( $testing < 1 ))
		then
		cat <&0 >>$TMP/stdin.$$			# problems with /dev/fd/0????, try this extra step rather thans subs
		files="${files%%-*} $TMP/stdin.$$ ${files##*-}"		# replace - with our tmp file
		verbose "copying stdin off to insert in file list: $files"
		else
		files="${files%%-*} /dev/fd/0 ${files##*-}"		# replace - with stdin
		verbose "reading stdin directly: $files"
		fi
	fi
	cat $files >>$TMP/mail.$$			# stdin not burried, we can do it all here
fi

logf=$NG_ROOT/site/log/mailer.$USER
if [[ $execute -gt 0 ]]
then
	exec 2>>$logf
	set_path			# it is not usually in the path, and bloody sun os puts it in an odd place
	if ! ${sendmail_path}sendmail -t <$TMP/mail.$$
	then
		log_msg "sendmail failed: $parms"
		cleanup 1
	else
		log_msg "sendmail ok: $parms"
	fi
else
	log_msg "would execute: sendmail -t <$TMP/mail.$$"
fi

if [[ $verbose -gt 0 || $execute -eq 0 ]]
then
	log_msg "saved copy of formatted mail in: $TMP/$LOGNAME.last.ng_mail"
	cp $TMP/mail.$$ $TMP/$LOGNAME.last.ng_mail
fi



cleanup 0              # delete temp file and get out 
exit    0              # should never happen but allows for scd doc at end

# ------------ SCD documentation section ----------------------------------

&scd_start
&doc_title(ng_mailer:Ningaui mail interface)

&space
&synop	ng_mailer [-B bcc-list] -[C cc-list] [-T to-list] [-b addressbook] [-m mime-opt] [-n] [-s subject] [-v] file [file...]
&space
	&This is a wrapper to the system mail utility (sendmail) which provides 
	RFC822 compliant header generation, and supports the endcoding of files 
	into a MIME formatted message allowing attachments to be easily sent from 
	the command line. 
&space
	&This also supports the concept of a simple address book allowing for 
	nicknames and mailing lists to be expanded and mail sent directly to 
	the targeted address rather than depending on the forwarding of mail 
	via some centralised nick-name processing agent.

&space
&subcat Input Files
	By default, if multiple files names are supplied on the command line they 
	are all combined into a single mail body and sent without MIME encoding.
	If the mime option (-m) is provided on the command line, the first file is 
	added to the generated email as the body text, and the remainder of the 
	files are added as attachments. The dash character (-) can be supplied as 
	any filename on the command line and will be assumed to mean the standard
	input device.  When processing the input files, &this will read all records
	from the standard input and add them to the message in the appropriate location.
&space
	Currently all input files are assumed to be plain ASCII text. If it is necessary 
	to send binary data, then the files should be converted with a tool such as 
	&lit(uuencode) before sending via email.  

&space	
&subcat Recipients
	The generated email message is sent to the list of recipients that are 
	supplied  using the &lit(to), &lit(cabon copy,) or &lit(blind carbon)
	options on the command line.  The information passed to any of these 
	options is expected to be a real address
	(e.g. scooter@cddept.ou.edu), a nickname or list name that is defined 
	in one of the supplied address books (-b), or a local user name.  Local 
	user names are distinguished from mailing list names and nicknames by 
	preceding the name with a bang character (e.g. !root). 
&space
&subcat The Addresbook File
	The addressbook file is used to translate nicknames into email
	addresses that can be passed into the UNIX mail program.
	The addressbook file is also used to define mailing lists: lists 
	of nicknames associated with one mailing list name.
	The script will parse each addressbook file building an internal 
	mapping table and generating errors if nicknames on the command line
	or names in a mailing list cannot be mapped into an email address. 

&space
	Lines in the addressbook file that begin with a hash character (#) 
	are considered to be comments and are ignored (as are blank lines).
	A line that begins with a plus sign (+) is assumed to be a 
	mailing list definition and the script expects the next token
	on the line to be the mailing list name followed by one or
	more nicknames.
	All other lines in the file are assumed to be nickname address
	lines which are used to map nicknames to real mail addresses. 	
	The following illustrates the syntax of the addressbook file. 
&space
&indent
	# comment line &break
	nickname address [# comment] &break
	+ mailinglist_name nickname [nickname nickname...] [# comment] &break
&uindent

&space
	If it is necessary to maintain more information in the file with the 
	nickname and address pairs, the information following the address
	on each line is ignored and thus additional fields can be added as 
	is necessary.

&space
	&This will used the addressbooks supplied on the command line with the 
	-b option in the order that they appear.  The ningaui addressbook,
	&lit(NG_ROOT/lib/addressbook) is used as a last resort if this file 
	exists.  (It is no longer a requirement that a addressbook file 
	exist in order for &this to send the message.)

&space
&opts	The following command line options are supported. 
&begterms
&term	-B list : Blind carbon. &ital(List) is  one or more  addresses and/or nicknames that will be added 
	to the blind  carbon statement in the mail header.  The blind carbon list is removed by &lit(sendmail)
	as the mail is actually sent to the recipients.  
	Multiple addresses can be supplied is provided as a single token of space, or comma, separated items, or
	using multiple &lit(-B) options.
&space
&term	-C list : Carbon copy. &ital(List) is one or more addresses or nicknames added
	to the carbon copy list in the mail header.  This list remains in the mail and will 
	be visible to all recipients.
	Multiple addresses can be supplied is provided as a single token of space, or comma, separated items, or
	using multiple &lit(-C) options.
&space
&term	-T list : To list.  &ital(List) is one or more addresses or nicknames added to the primary 
	list of recipients.
	Multiple addresses can be supplied is provided as a single token of space, or comma,  separated items, or
	using multiple &lit(-T) options.
&space
&term	-b file : Supplies a addressbook file to be used to translate nicknames into real email addresses.
	Mulitple addressbooks can be supplied if quoted and space spearated, or using multiple &lit(-b) options.
&space
&term	-f name : Allows the user to supply a formatted (pretty) name for the 'From:' field in the 
	header. If not supplied, recipiants will likely get username@system.domain.
&space
&term 	-m mime-opt : Send using &ital(mime) format with specialised character 
	encoding of the body based on the option given.  The option are those 
	supported by &lit(ng_mime) and at the time of this writing are:
&space
	&smterms
	&term	q : quoted printable.
	&term	6 : base64 encoding.
	&space
	&endterms

&term	-n : No execution mode.  &This will go through the motions of validating 
	nickname and addresses, but will not actually send the mail message. The 
	mail message will be formatted and saved in the $TMP directory for 
	validation.
&space
&term	-s sub : Supply a subject for the mail. If the subject contains more than one word, 
	it should be quoted on the command line.
&space
&term	-v : verbose mode.
&endterms
 
	
&space
&parms  
	Following all options, &this expects the remaining command line
	parameters to be filenames.  These files are read and added to 
	the mail body in the order that they are encountered.  The special
	file name (-) is assumed to refer to the standard input device
	and when encountered causes &this to read all records to the end 
	of file on standard input and to add these records to the email message
	as is appropriate (as an attachment if the mime option is 
	supplied and the dash character is not the first filename in 
	the list.
&space
	When the MIME option is supplied, the first file is added as 
	the body of the message and all other files are added as attachments.

&exit	Upon normal completion of the script the exit code is set to 0.
	If errors are detected while processing the script will exit with
	a code greater than 0. If &this is not able to resolve one or 
	more of the nicknames from the addressbook(s) then the mail will 
	not be sent to any of the intended recipiants. 

&space
&examp  The following example illustrates how the mailer.ksh script can 
	be used to send the mreport.txt file to two nicknames (poncall 
	and soncall) using two addressbooks (alias.pb and oncall.pb).  
&space
ng_mailer -b alias.pb -b oncall.pb -s "Morn Rpt." -T "poncall soncall" mreport.txt 

&apce
	The following is a sample addressbook file which lists as series of 
	nickname address pairs, and mailing lists.
&indent
.nf
# personal address book used by mailer script
#
#NICKNAME    ADDRESS
#--------    -------------------------------------
mike         mikep@foo.com
jon          jkresov@foo.com
ted          tschmo@foo.com
brenda       bren@foo.com
rick         rickt@foo.com
scott        daniels@research.foo.com

# alias names and mailing lists
#
#NAME        NICKNAME LIST
#--------    -------------------------------------
+ poncall    rick     # primary on call
+ soncall    ted        # secondary on calll
+ status     mike jon ted
.fo
&uindent

&space
&files	
&begterms
&term	$NG_ROOT/lib/addressbook : If this file exists, it is used as the 
	default phone book.  &This will search it last using all 
	addressbook supplied by the user first. (See -b option.)
&endterms

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Feb 1997 : To use Mail/mailx in order to attach subject; &break
	To add multiple name support on alias list; &break
	To remove Mail/mailx as it seemed not to be 
	sending messages out (or notifing user of problems).
&term	09 Apr 1997 : To support Gecko standard cmd line parsing.
&term 	12 Aug 1997 : To make use variables to reference fields from 
	phone book file. 
&term   20 Dec 1997 (sd)  : To support sending MIME messages. 
&term   29 Dec 1997(sd) :  To keep mime tmp file around for post morts. (I68)
&term	10 May 2001 (sd) : To port into Ningaui
&term	17 May 2001 (sd) : To correct problem with mail/mailx and thus convert 
	into a complete UA.
&term	13 May 2003 (sd) : To adjust for sendmail 8.12 not wanting -U option at all.
&term	11 Oct 2004 (sd) : Changed expected location of addrbook to lib.
&term	10 Dec 2004 (sd) : Major overhaul; command line changes, addressbook requirement
	changes, and general imrpovement.  Doc changes.
&term	24 Jan 2005 (sd) : Added -f option.
&term 	12 Feb 2005 (sd) : Corrected bug which required first line of body (in non-mime mode)
		to be non-whitespace.
&term	25 Feb 2005 (sd) : No longer tests sendmail version to determine if -U option is needed.
		we assume that current sendmail does not need it and that we are up to date
		with our flavour of sendmail.
&term	27 Dec 2005 (sd) : Corrected issue with the inability to read /dev/fd/* on some nodes. 
&term	06 Feb 2006 (sd) : Writes a bit of info to log for debuging.
&term	12 Feb 2006 (sd) : changed log file to include $USER name because of bloody nagios. 
&term	21 Jun 2006 (sd) : Doc changes.
&term	29 Jun 2006 (sd) : Fixed check for - in input file list to allow foo-bar filename.
&term	21 Mar 2007 (sd) : Dropped sending sendmail a -v when -v is coded on the command line. 
		This now seems to cause a notification report to be mailed back to the 
		sender rather than just causing sendmail to be chatty about what it is doing.
		Grumble.
&term	02 Jul 2002 (sd) : Now aborts if to list is empty.
&term	14 Aug 2007 (sd) : Uses NG_DOMAIN if set and defaults to ng_sysname -f only if not. On some 
		nodes the O/S does not return the full domain. 
&term	13 Aug 2008 (sd) : To, copy and bcc  lists may now be a comma or space separated list.
		Now removes duplicate names from lists. 
&term	18 Aug 2008 (sd) : Fixed problem with duplicate removal. 
&endterms
&scd_end
# ------------------------ end of scd documentation ----------------------
