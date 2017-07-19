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
# Mnemonic:	roll_log
# Abstract:	roll a log file in /ningaui/site/log
#		
# Date:		18 Jul 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function doit
{
	typeset logname=$1
	typeset k=""

	k=$(( $keep - 1 ))			# zero based
	while [[ $k -gt 0 ]]
	do
		old=$logname.$(($k - 1 )).gz
		new=$logname.$k.gz
		cp -p $old $new  2>/dev/null

		k=$(( $k-1 ))
	done
			
	mv $logname $logname.0
	rm -f $logname.0.gz
	ng_wrapper --detach gzip $logname.0 	# move off and do in parallel for faster start
}

# -------------------------------------------------------------------

ustring="[-d dir-name] [-n num-logs] [-v] [path/]logfile [path/logfile...]"
logd=$NG_ROOT/site/log
keep=3

while [[ $1 = -* ]]
do
	case $1 in 
		-d)	logd=$2; shift;;
		-n)	keep=$2; shift;;

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


if cd $logd
then
	while [[ -n $1 ]]
	do
		if [[ -f $1 ]]
		then
			verbose "rolling logs: $1"
			doit $1
		else
			log_msg "log not found, not rolled: $1"
		fi
		shift
	done	
else
	log_msg "unable to switch to: $logd"
	cleanup 1
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_roll_log:Roll off a copy of a log file)

&space
&synop	ng_roll_log [-d directory] [-n num] [-v] logname [logname...]

&space
&desc	&This will 'roll off' the logs named on the command line.  Previously 
	rolled logs are expected to have been compressed and have filenames with the 
	syntax fo &ital(logname.n)&lit(.gz).  The number of logs that are retained 
	during the roll process defaults to three, but can be set by supplying the 
	&lit(-n) process.  
&space
	After the current log file has been moved, it is asynchrounously compressed 
	so that this script can return as quickly as possible to the caller.  


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-d dir : Supplies the name of the directory in which the work will be done. If
	not supplied &lit(NG_ROOT/site/log) is assumed to be the desired working 
	directory.
&space
&term	-n num : Inidcates the number (num) of log files to keep. Archived log files 
	are given a suffex which includes a number in the range of 0 to &lit(n-1).
&space
&term	-v : Verbose mode.
&endterms


&space
&parms &This expects that at least one log name is provided on the command line, and 
	will process all log names that are supplied.  

&space
&exit	If a non-zero exit code is generated, then an error occured during processing.

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	18 Jul 2003 (sd) : Initial slice of the pie.
&term	10 Oct 2008 (sd) : Added usage string. 
&endterms

&scd_end
------------------------------------------------------------------ */
