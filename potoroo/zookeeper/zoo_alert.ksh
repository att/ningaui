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
# Mnemonic:	zoo_alert
# Abstract:	Send a message to be given to objects named alert_msg in any zoo view
#		
# Date:		19 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-c colourtable-value] [-o object-name] [-s parrot-host] msg"
colour=1
parrot_host=$srv_Netif
oname=alert_msg

while [[ $1 = -* ]]
do
	case $1 in 
		-c) 	colour=$2; shift;;
		-o)	oname=$2; shift;;
		-s)	parrot_host=$2; shift;;
		-v)	verbose=1;;

		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -z "$@" ]]		# nothing on command line; read from stdin 
then
	awk '
	{
		oname = $1;
		colour== $2;
		$1 = $2 = "";
		printf( "RECOLOUR %s %d \"%s\"\n", oname, colour, substr( $0, 3 ) ); 
	}
	' |ng_sendudp $parrot_host:$NG_PARROT_PORT
else
	msg="$@"
	printf "RECOLOUR %s %d \"%s\"\n" "$oname" "$colour" "$msg" |ng_sendudp $parrot_host:$NG_PARROT_PORT
fi
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_zoo_alert:Send an alert message to zoo views)

&space
&synop	ng_zoo_alert [-c colour-value] [-o view-obj-name] [-s node] message-text
&break	ng_zoo_alert [-s node]	<message-file

&space
&desc	&This sends the text on the command line to zoo processes. The text will
	be written to active views that have an object named &lit(alert_msg).
&space
	If message text is not supplied on the command line, then &this will read 
	from standard input and send the data to the parrot listening at &ital(node)
	or on the network service node if &ital(node) is not supplied. 
	Messages in the input file are expected to be of the form:
&space
&litblkb
    object-name colour message text
&litblke

&space
	Colour is a digit (0-20) that corresponds to the W1KW colour look up table. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c value : A colour value number from the zoo clolour look-up table (0-20). The 
	message will be displayed in this colour.  (common colours are 0=white, 1=red, 
	2=orange, 6=green.)
&space
&term	-o name : The object name to update in the view. If not supplied &lit(alert_msg) 
	is updated.
&space
&term	-s node : The node to send the mssage to.  Messages are sent to the &lit(ng_parrot) 
	process running for distribution to listening views. 
&endterms


&parms	Any command line paramters passed after the options are sent as the text message
	to the view.
&space
&exit	Always 0.

&space
&see	ng_parrot, ng_zoo

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 May 2003 (sd) : Thin end of the wedge.
&term	31 Aug 2006 (sd) : Mod to accept a bunch of messages on stdin to make things more 
	efficent (one call to sendudp rather than lots).
&endterms

&scd_end
------------------------------------------------------------------ */
