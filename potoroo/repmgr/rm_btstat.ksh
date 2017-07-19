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
# Mnemonic:	rm_btstat
# Abstract:	Try to present info about the queue of data in rmbt
#		
# Date:		16 Jan 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function parse_dump
{
	awk '
	/exit/ { 
		if( ! ex )
			ex = tot+1; 		# location of the first exit
		exc++;				# count them
	}

	NF <= 0 { next; }
	{
		tot++;
		type[$1]++;
	}

	END {
		soup = "thstndrdththththththth";		# say that 10 times fast!

		if( tot )
			printf( "\n%-10s %5s\n", "Command", "Count" );
		for( x in type )
			printf( "%-10s %5d\n", x, type[x] );

		if( ex )
		{
			x = ((ex % 10) * 2 ) + 1;
			suffix = substr( soup, x, 2 );
			str = sprintf(  " first exit command is %d%s in the queue\n", ex, suffix );
		}
		else
			str = "";
		printf( "%-10s %5d %s\n", "Total:", tot, str );
	}
	'<$fname
}


# -------------------------------------------------------------------
fname=""
oldrmbt=0
keep=

ng_sysname|read my_node
filer=${srv_Filer:-nofiler}
args="$@"
cluster=""

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster=$2; shift;;
		-f) 	fname=$2; shift;;
		-o)	oldrmbt=1;;
		-k)	keep="-k";;
		-s)	filer=$2; shift;;

		-v)	verbose=1; vflag="-v";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -n $fname ]]		# just parse supplied file 
then
	parse_dump 
	cleanup 0
fi

if [[ -n $cluster ]]
then
	ng_ppget -C $cluster srv_Filer|read filer
fi

if [[ -z $filer ]]
then
	log_msg "unable to determine filer for ${cluster:-$NG_CLUSTER}"
	cleanup 1
fi

if [[ $filer != $my_node ]]
then
	ng_rcmd $filer $argv0 $vflag
	cleanup $?
fi

fname=$TMP/rmbt_tail.$$

verbose "dumping info from rmbt into $fname"
if ! ng_rmreq -s $filer "~dump~$fname"
then
	log_msg "unable to communicate with filer on $filer"
	cleanup 1
fi

verbose "waiting for dump confirmation"

i=0
while sleep 1
do
	if [[ -f $fname ]]			# must wait for the confo msg in the log to say it is ready
	then
		if tail -1000 $NG_ROOT/site/log/repmgrbt|grep "dump.*seconds.*$fname"|sed 's/^.*dump_innards://; s/written to.*//'
		then
			parse_dump
			cleanup $keep 0
		fi
	fi

	i=$(( $i + 1 ))
	if (( $oldrmbt > 0 ))
	then
		if (( $i > 10 ))
		then
			parse_dump 
			cleanup $keep 0
		fi
	else
		if (( i > 100 ))
		then
			log_msg "wait for dump file expired"
			cleanup $keep 1
		fi
	fi
done

log_msg "internal error!"
cleanup 1			# should not get here



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_btstat:Show statistics about ng_repmgrbt)

&space
&synop	ng_rm_btstat [-v]

&space
&desc	&This causes &lit(ng_repmgrbt) to dump the contents of its 
	send queue and then analysis the output to generate some
	stats about what is queued.   The output is a tally of each
	type of command that is queued, with a total of the number of
	commands, buffers and the location of the first exit command
	in the queue (if any).

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose mode.
&endterms


&space
&exit	Any non-zero exit code indicates that &this was not able to communicate with 
	&lit(ng_repmgrbt), or there was a problem interpreting the information received.

&space
&see	ng_repmgr, ng_repmgrbt

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Jan 2005 (sd) :  New stuff.
&endterms

&scd_end
------------------------------------------------------------------ */
