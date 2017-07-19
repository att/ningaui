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
# do power management things like up/down/cycle

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# parms are <powerstrip-ip> <nodename>
function bld_script
{
	case $oper in
		cycle)	cmd="/Boot $who";;
		down)	cmd="/Off $who";;
		*)	cmd="/On $who";;
	esac

	cat <<endkat 
spawn telnet $1

expect "NPS> "
set timeout 70
send "$cmd\r"

expect_before {
        Invalid         { exit 1 }
	"Sure? (Y/N):"	{ send "y\r" }
	eof		{ send_user "end of file received\n"; exit 1 }
	timeout		{ send_user "\n*** got a timeout\n"; exit 2 }
	"Processing"	{ send_user "command is processing\n" }
}

expect "NPS> "		{ send "/x\r"; send_user "\nsent exit\n"; exit 0 }
expect "NPS> "		{ send "/x\r"; send_user "\nsent exit\n"; exit 0 }
expect "NPS> "		{ send "/x\r"; send_user "\nsent exit\n"; exit 0 }
expect "NPS> "		{ send "/x\r"; send_user "\nsent exit\n"; exit 0 }

endkat
}

# search for $1 in the map and print the power strip id that owns it
function lookup
{
	grep $1 ${map:-nosuchfile} | awk '{print $2; exit( 0 )}'
}

# -------------------------------------------------------------------------

map=$NG_ROOT/site/power_map 	# where map to the stars is
oper=cycle			# by default cycle things
force=0

while [[ $1 = -* ]]
do
	case $1 in 
		-c) 	oper=cycle;;		# even though its the default
		-d)	oper=down;;
		-f) 	force=1;;		# allow for non interactive use
		-u)	oper=up
			force=1			# this one is ok to not ask their intensions
			;;

		-\?)	usage; cleanup 1;;
		--man) show_man; cleanup 1;;
		*)	echo unrecognised $1 in "$@"; usage; cleanup 1 ;;
	esac

	shift
done

who=${1%%.*}			# which node to zap (allow things like ning15.research.att.com )
if [[ -z $who ]]
then
	error_msg "missing nodename from command line"
	cleanup 1
fi

ps=`lookup $who`		# map who to a power strip
if [[ -z $ps ]]
then
	error_msg "cannot map $who to a powerstrip" 
	cleanup 1
fi

if [[ $force -lt 1 ]]
then
	echo "this will SHUTDOWN $who. Are you sure? [n] \c"
	read ans
	if [[ $ans != y ]]
	then
		echo "aborted, no action taken."
		cleanup 0
	fi
fi

bld_script $ps $who | expect - 
cleanup 0

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_power_mgt:Network powerstrip management interface)

&space
&synop	ng_power_mgr [-c] [-d] [-f] [-u] nodename

&space
&desc	&This builds a "chat" script that is used to communicate 
	with a power strip via &lit(telnet) in order to control 
	the power to one of the plugs on the strip. The power
	can be stopped, started, or cycled depending on command line
	options supplied. If the  &ital(nodename) can be mapped 
	to a power supply IP address then the chat script is 
	executed and the desired power management action is taken.
&space
	&This assumes that the power strip has been configured such that
	it recognises the name of the node that is to have its power 
	altered. 

&opts	The following options are recognised:
&begterms
&term	-c : Cycle the power to the node. (default)
&space
&term	-d : Take the power down to the node. 
&space
&term	-f : Force; do not prompt user to ensure this is what they want to do.
&space
&term	-u : Bring the power to an up state for the node. 
&endterms
&space

&parms	The only required parameter that is recognised by &this is the
	node name. If this parameter is not supplied, the scrip 
	will exit with a bad return code. 

&space
&see	ng_power_map

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 23 Jul 2001 (sd) : New stuff.
&term 23 Aug 2001 (sd) : To allow full host.domain names as node name.
&endterms

&scd_end
------------------------------------------------------------------ */
