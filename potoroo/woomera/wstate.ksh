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
# Mnemonic:	wstate - provide the 'state' of the job or resource. 
# Abstract:	Digs through a woomera dump and gives counts for queued, running, finished, and 
#		all (depending on cmd line flags) for jobs that list the resource(s) 
#		supplied on the command line.  If a job name is supplied, then counts for the 
#		job(s) are given
#		
# Date:		23 May 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-a] [-f] [-r] [-q] [-v]  job/resource(s)"
what=
verbose=0

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	what="${what}all ";;
		-r)	what="${what}running ";;
		-f)	what="${what}finished ";;
		-l)	what="${what}limit ";;
		-q)	what="${what}queued ";;

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

if [[ -z $1 ]]
then
	usage
	cleanup 1
fi

pat="$*"			# cannot do replacement on $* it seems
ng_wreq dump | sed 's/cmd.*//; s/after .*,//' | gre "${pat// /: }: | ${pat// / | } " | awk -v verbose=${verbose:-0} -v what="${what:-all}" '
	function get_val( n,		a )
	{
		split( $(n), a, "=" );
		return a[2];
	}

	/^resource/ { 
		limit = get_val( 3 ) + 0;
		next;
	}

	/^job.*status=1/ { 
		queued++;
		all++;
		next;
	}
	/^job.*status=2/ { 
		running++;
		all++;
		next;
	}
	/^job.*status=[34]/ { 
		finished++;
		all++;
		next;
	}
	END {
		x = split( what, a, " " );
		for( i = 1; i <= x; i++ )
		{
			if( i > 1 )
				printf( " " );
			if( a[i] == "running" )
				printf( "%s%d", verbose ? "running=" : "", running );
			else
			if( a[i] == "all" )
				printf( "%s%d", verbose ? "all=" : "", all );
			else
			if( a[i] == "queued" )
				printf( "%s%d", verbose ? "queued=" : "", queued );
			else
			if( a[i] == "finished" )
				printf( "%s%d", verbose ? "finished=" : "", finished );
			else
			if( a[i] == "limit" )
				printf( "%s%d", verbose ? "limit=" : "", limit );
			else
				printf( "%s0\n", verbose ? "unk=" : ""  );
		}
		if( x > 0 )
			printf(  "\n" );
	}
'

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_wstate:Give state counts for jobs/resources known to woomera)

&space
&synop	ng_wstate [-a] [-f] [-r] [-q] [-v] job/resource(s)

&space
&desc	&This will scan the jobs currently known to woomera  for jobs that list the resource 
	name(s) supplied on the command line, or that have a job name that matches the job name(s)
	supplied.  Depending on command line options supplied, the count of finished, pending, queued, and running 
	jobs is then presented on a single line to standard out. 
	The order that the counts are written is the same 
	order that the flags were specified on the command line.  Setting the 
	verbose flag causes the counts to be identified in a &lit(name=v) manner.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Display a total of all jobs found that match the job/resource names.
&space
&term	-f : Display a count of all jobs that have finished.
&space
&term	-l : Show limit for the resource. If job given with this option the value will be 0. 
&space
&term	-r : A count of all jobs that are running is presented. 
&space 
&term	-q : Causes the queued job count to be included in the output. 
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms	All parameters that follow the command line flags are assumed to be either job names
	or resource names to search for. 

&space
&exit
	Zero indicates a good exit. 

&space
&see	ng_woomera, ng_wreq, ng_whist

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	23 May 2006 (sd) : Its beginning of time. 
&term	30 May 2006 (sd) : Added -l option to show limit.


&scd_end
------------------------------------------------------------------ */
