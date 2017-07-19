#!/usr/common/ast/bin/ksh
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


#!/usr/common/ast/bin/ksh
# Mnemonic:	spy_wrapper
# Abstract:	wrapper script that spyglass invokes to drive a test. It executes the 
#		suplied script and captures its output and status. The output filename
#		and status are returned to spyglass via a tcp/ip message.
#		
# Date:		26 June 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


if ! [[ "$@" = *--foreground* || "$@" =  *-\?* || "$@" = *--man* ]]
then
	$detach
	exec >$NG_ROOT/site/log/spy_wrapper 2>&1
fi

# -------------------------------------------------------------------
ustring="-i id [-p port] command tokens"

port=$NG_SPYG_PORT
while [[ $1 = -* ]]
do
	case $1 in 
		--foreground)	;;				# ignore -- forground for testing
		-i)	id="$2"; shift;;		# id we send back to associate status with the test 
		-p)	port=$2; shift;;		# mostly for testing

		-v)	verbose=1
			vopt="$vopt-v "
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

if [[ -z $id ]]
then
	log_msg "abort: no id supplied"
	cleanup 2
fi

if [[ -z $port ]]
then
	log_msg "abort: no port for spyglass"
	cleanup 2
fi

if [[ ! -d $TMP/spyglass ]]
then
	mkdir $TMP/spyglass
fi

log_msg "running: for $@"



output="$TMP/spyglass/spy_out.$id.$(ng_dt -i)"
last_output="$TMP/spyglass/spy_out.$id"			# keep most recent output 

"$@" >$output 2>&1
status=$?


#awk ' /^subject_data:/ { $1 = ""; print ":" $0; exit( 0 )} ' <$output | read sdata
awk ' 
	BEGIN { sdata = " "; }
	/^subject_data:/ { $1 = ""; gsub( ":", " ", $0 ); sdata = sprintf( "%s", $0 ); next; }
	/^key:/ { key = $2; next }
	END { printf( "%s:%s\n", sdata, key ) }
' <$output | read sdata key

echo "status:$id:$status:$output:$sdata$key  (spy_wrapper: returned to spyglass)" >>$output
cp $output $last_output			# must copy before sending status to spyglass

echo "status:$id:$status:$output:$sdata$key" | ng_sendtcp $vopt -e "#end#" localhost:$port

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_wrapper:Command wrapper for ng_spyglass)

&space
&synop	ng_spy_wrapper -i id-string [-p port] command tokens

&space
&desc	&This is invoked by ng_spyglass to execute a command. The output of the command
	(stdout and stderr) are captured along with the command status.  The filename
	containing the output, and the status, are passed back to spyglass via a TCP/IP
	message.

&space
	The command output is searched for two patterns: "^^subject_data:"  and "^^key:".  
	If either of the patterns are found in the output the remaining information is passed to 
	spyglass. The &lit(subject_data) information is pass in its entirety (all tokens 
	on the line) and used as the subject for any alert messages that are sent.  This 
	allows the probe to customise the subject and override the description that
	was defined in the spyglass conifguration file.   If the &lit(key:) pattern exists in 
	the file, the second token on the line is passed as an alarm key. 

&space
	The alarm key is used by spyglass to potentially squelch alert messages until the 
	key changes.   If a probe is defined with 'squelch change,' then after the initial 
	alarm message, another alert message is not sent until the returned key is different, 
	or until the state has been reset to 'normal.'  One use of this feature is when checking
	for old files.  The check probe can create a key based on the current set of old files. 
	As long as the set of old files does not change, the key should remain the same and 
	there should not be a continued set of alarms for the same set of old files.  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	--foreground  : Prevents the script from detaching from the process that started it; mostly
	for interactive testing.
&space
&term	-i string : The id string that spyglass will use to associate the status and filename to the 
	test that was executed.
&space
&term	-p port : The port that spyglass is listening on for TCP/IP messages. If not defined, 
	the pinkpages variable NG_SPYG_PORT is assumed to hold the value. 
&endterms


&space
&parms	All parameters are assumed to be the command tokens to execute.  There should not be any 
	redirection symbols in the command.

&space
&exit	Always exits good.

&space
&see	ng_spyglass, ng_spy_ckfage, ng_spy_ckfsfull, ng_spy_ckdaemon

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Jun 2006 (sd) : Its beginning of time. 
&term	21 Aug 2006 (sd) : Supports a key in the probe output. 
&term	22 Aug 2006 (sd) : Fixed where we preserve the last output data as it was being 
		trashed by spyglass before we could save it.
&term	12 Dec 2008 (sd) : Corrected man page. 


&scd_end
------------------------------------------------------------------ */
