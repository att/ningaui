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
# Mnemonic:	sg_start
# Abstract:	start spyglass. This is NOT a wrapper script like most other 
#		ningaui startup scripts.  This script is necessary to create a list
#		of configuration files that will be passed to spyglass on the commandline. 
#		The standard ng_endless is used to keep spyglass running.
#		
# Date:		19 Aug 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


if [[ "$@" != *"-e"* && "$@" != *"--man"* && "$@" != *"-?"* ]]
then
	ng_sgreq -s 2 -s localhost  explain nosuchprobe 2>/dev/null | read ans
	if [[ $ans == "explain: unknown test:"* ]]
	then
		log_msg "spyglass is already running (responded to explain)"
		cleanup 1
	fi

	exec >>$NG_ROOT/site/log/sg_start 2>&1
	$detach
fi

# -------------------------------------------------------------------
ustring="[-p port] [-v]"
vflag=""
port=""
host=""
stop=0

while [[ $1 == -* ]]
do
	case $1 in 
		-e)	stop=1;;
		-p)	port="-p $2"; shift;;

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if (( $stop > 0 ))
then 
	ng_goaway ng_sg_start		# cause managing start process to stop
	ng_sgreq -e			# off spyglass
	verbose "spyglass and controlling wrapper signaled to stop"
	cleanup 0
fi


ng_goaway -r ng_sg_start
while true					# we cannot run in an endless or we dont pickup/drop config files with each cycle
do
	wash_env
	ng_ppget -r NG_SPYG_CONFIG | read NG_SPYG_CONFIG		# must get fresh value -- washed from cartulary supplied stuff
	clist="${NG_SPYG_CONFIG:-$NG_ROOT/lib/spyglass.cfg} "		# set the default config file
	(								# add application and site specific files
		ls $NG_ROOT/apps/*/lib/spyglass*.cfg 2>/dev/null
		ls $NG_ROOT/site/spyglass*.cfg 2>/dev/null | gre -v spyglass.cfg
	) | while read c
	do
		clist="$clist$c "
	done

	verbose "spyglass config files: $clist"
	ng_roll_log spyglass
	verbose "starting spyglass...."
	ng_wrapper  -O1 $NG_ROOT/site/log/spyglass -o2 stdout ng_spyglass $vflag $port -c "'$clist'" 

	verbose "spyglass has ended"
	if ng_goaway -c ng_sg_start
	then
		log_msg "found goaway file; exiting"
		ng_goaway -r ng_sg_start
		cleanup 0
	fi

	sleep 3
done


cleanup 0			# shouldn't get here, but lets be safe

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sg_start:Start Spyglass)

&space
&synop	ng_sg_start [-e] [-p port] [-v]

&space
&desc	&This will detach from the controlling tty and then start &lit(ng_spyglass) 
	giving it the correct set of configuration files. 
	All files that exist in &lit(NG_ROOT/lib,)  &lit(NG)ROOT/site,) or &lit(NG_ROOT/appl/*/lib) and 
	have the filename syntax of &lit(spyglass*.cfg) will be passed to 
	&lit(ng_spyglass) to read as configuration files.  The order of the 
	files passed cannot be predicted other than &lit(NG_ROOT/lib/spyglass.cfg) 
	will be parsed first.  It is strongly suggested that all application probe names have the form
	&lit(<appl>-<probename>) (e.g. aether-ssh) to ensure that application probes do not overlay
	each other.

&space
	Once &lit(ng_spyglass) has been started, &this waits for spyglass to exit and will restart spyglass unless
	a goaway file for &this has been set. 
	When &lit(ng_spyglass) is restarted, the list of configuration files is 
	reconstructed such that new ones are noticed, and ones that have been deleted
	are not looked for. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-e : Cause &this to send an exit command to &lit(ng_spyglass) and to set a 
	goaway file for the controlling &this such that when &lit(ng_spyglass) stopps
	it is not restarted.   If the user's intent is to cycle just the &lit(ng_spyglass)
	process, the command &lit(ng_sgreq -e) should be used. 

&space
&term	-p port : Specify a port for spyglass to listen on. If not supplied, the default
	of &lit(NG_SPYGLASS_PORT) is used. 

&space
&term	-v : Verbose mode. The script might chat more about what it is doing if this is on and 
	the flag is passed to &lit(ng_spyglass). 
&endterms


&space
&exit	An exit code of zero is good; all others indicate an error. 

&space
&see	&seelink(spyglass:ng_spyglass) 
	&seelink(spyglass:ng_spy_ckdaemon)
	&seelink(spyglass:ng_spy_ckfage) 
	&seelink(spyglass:ng_spy_ckmisc) 
	&seelink(spyglass:ng_spy_wrapper)
	&seelink(spyglass:ng_spy_ckdisk) 
	&seelink(spyglass:ng_spy_ckfsfull) 
	&seelink(spyglass:ng_spy_cknet)


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 Aug 2007 (sd) : Its beginning of time. 
&term	21 Apr 2009 (sd) : This script now sustains spyglass rather than depending on an ng_endless
		process to do so.  Using endless, we could not pass new config files on restart.
		Beefed up the man page.
&endterms


&scd_end
------------------------------------------------------------------ */

