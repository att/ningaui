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

# -------------------------------------------------------------------------------------
# Mnemonic: 	ng_whist
# Abstract: 	Susses through the woomera log looking for the string passed as $1.
#		prints a history of jobs containing that string and limit changes.
# Date: 	25 Feb 2002
# Author: 	E. Scott Daniels
# -------------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


ustring="search-string [logfilename]"
verbose=0

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
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

what="$1"			# what to search for

if [[ -z $what ]]
then
	usage
	cleanup 1
fi

awk -v verbose=$verbose -v what="$what" '
	function print_stuff( name, 		i )
	{
		if( limits )
			printf( "%s\n", limits );
		limits = "";

		for( i = 0; i < job[name]; i++ )
			print stuff[name, i];
		printf( "\n" );
	}

	BEGIN {
		jidx = 0;
		count = 0;
		done = 0;
	}

	/got EOF/ {
		date = $(NF-3) " " $(NF-2) " " $(NF-1) " " $NF;
		next;
	}

	$1 == "adding" {
		if( index( $0, what ) )
		{
			count++;

			joborder[jidx++] = $3;
			job[$3] = 0;
			#printf( "found job: %s %s\n", what, $3 );
			stuff[$3, job[$3]++] = sprintf( "%s: %s", date, substr( $0, 1, 150 ) );
			next;
		}
	}

	$1 == "queueing" {
		if( job[$3] )
			stuff[$3, job[$3]++] = sprintf( "%s: %s", date, $0 );
		next;
	}

	/resource .* limit/	{
		if( index( $0, "GORP" ) )		# junk stuff we dont care about 
			next;

		$1 = "";
		if( resource[$3] != $NF )		# a change from what it was?
		{					# record all limits in a string that is printed with the next print_stuff
			resource[$3] = $NF;
			limits = sprintf( "%s: LIMITS:", date );
			for( x in resource )
				limits = limits sprintf( " %s=%d ", x, resource[x] );
		}
		next;
	}

	$1 == "job" {
		split( $2, a, "(" );			# inconsistant spacing round job name, we must play some games
		name=a[1];
		if( index( name, ":" ) )
			name = substr( name, 1, length( name ) -1 );

		if( job[name] )
		{
			gsub( "/[^ ),]*", "<fn>" );				# we saw this on the adding line, trash it here 
			stuff[name, job[name]++] = sprintf( "%s: %s", date, $0 );
			if( index( $0, "expiry" ) )
			{
				done++;
				print_stuff( name );
				job[name] = 0;			# dont print at end 
			}
		}

		next;
	}

	END {
		for( x in joborder )				# anything we did not see an end to 
		{
			name = joborder[x]
			if( job[name] )
				print_stuff( name );
		}

		if( verbose )
			printf( "%d jobs, %d not yet done\n", count, count - done );		# some totals
	}
'<${2:-$NG_ROOT/site/log/woomera}

cleanup 0
exit 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_whist:Generate a job history from Woomera log info)

&space
&synop	ng_whist string [woomera-log-name]

&space
&desc	&This reads through the woomera log looking for job addition messages that contain &ital(string.)
	If found all messages related to the job are collected and formatted into a small history 
	which is written as the end of the job is noticed. 

&space
	The output is in the form of &lit(<timestamp>: <information>). Because &ital(woomera) does &stress(not) 
	timestamp each record it writes to the log, the timestamp that &this presents is collected from 
	human readable timestamps in the log. Therefore, the timestamps written to the output are 
	&stress(estimates) and nothing more!

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
	-v : Verbose mode; causes some stats to be printed following the regular output.
&endterms


&space
&parms	&This required one command line parameter, and accepts a second, optional, parameter. 
	The first parameter supplied must be the search string that &this is to look for. 
	The second parameter may be the name of the &ital(woomera) log file to search. If 
	the second parameter is not supplid, &this looks in &lit($NG_ROOT/site/log/woomera)
	for the log information.

&space
&exit	Always exits zero.

&space
&see	&ital(ng_woomera)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	25 Feb 2002 (sd) : Big bang.
&endterms

&scd_end
------------------------------------------------------------------ */
