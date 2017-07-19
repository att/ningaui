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
# Mnemonic:	ng_job_nodes
# Abstract:	List the nodes that are currently bidding for and/or running jobs
#		
# Date:		16 Sep 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



ustring="[-s job-host] [-v]"
#ng_ppget srv_Jobber|read jobber
jobber=${srv_Jobber:-no_jobber}

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-s)	jobber=$2; shift;;

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

ng_sreq -s $jobber dump 30 |egrep -v "summ|SUSPEND"|awk '
	{ 
		if( ($3+$5) > 0 ) 		# bids or running
		{
			split( $2, a, "(" );
			printf( "%s\n", a[1] );
		}
	}
'| sort -u | awk '{ printf( "%s ", $1 ); } END { printf( "\n" ); }'

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_job_nodes:List nodes bidding for jobs)

&space
&synop	ng_job_nodes [-s host] [-v]

&space
&desc	&This lists all of the nodes that are currently bidding on or 
	running jobs under the control of &ital(seneschal).  
	Nodes that are suspended are not included in the list even if 
	they are currently running jobs. The list is created by 
	sending a query to &ital(seneschal) for a list of all known nodes
	and the number of bids and running jobs associated with each. Any 
	node that has a positive bid/running count is listed. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-s host : Supplies the host running &ital(seneschal). If this option is not 
	supplied, the host named by the &lit(srv_Jobber) cartulary variable is 
	assumed to be running seneschal.
&space
&term	-v : Verbose mode.
&endterms

&space
&exit	This script always returns a zero return code to indicate no errors 
	during processing. Any non-zero exit code indicates an error condition.

&space
&see	ng_seneschal, ng_nawab, ng_woomera.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Sep 2003 (sd) : Thin end of the wedge.
&term	17 May 2004 (sd) : Avoids use of ppget.
&endterms

&scd_end
------------------------------------------------------------------ */
