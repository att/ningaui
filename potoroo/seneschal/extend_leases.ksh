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
# Mnemonic:	extend_leases
# Abstract:	
# 		extend lesases by amount ($1) for running jobs that match pattern $2
# 		if -m val is given, then this is applied only to jobs whose leases are 
# 		currently less than val. 
#		
#		With the implementation of ng_s_sherlock, this script should not be 
#		necessary. This is not quite deprecated, but close, so using this 
#		script is not advised. 
#		
# Date:		11 Mar 2005 (formalised)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


ustring="[-f floor-sec] [-l [+]lease-sec] [-m max_elapsed] [-n] [-N node] [-v] [jobname-pattern]"
floor=400				# we let leases get to this point before extending; 0 == always extend 
node=""
forreal=1
amt=5000				# seconds to extend lease to (+n is additive)
max=$(( 24 * 60 * 60 ))			# max seconds of execution time; if job shows greater elapsed than this we do not extend
while [[ $1 = -* ]]
do
	case $1 in 
		-f)	floor=$2; shift;;
		-l)	amt=$2; shift;;		
		-m)	max=$2; shift;;
		-n)	forreal=0;;
		-N)	node=$2; shift;;

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

tfile=$TMP/extend_leases.$$

ng_ppget srv_Jobber|read jobber
ng_sreq -s $jobber explain running|grep "${1:-.*}" |awk \
	-v node="$node" \
	-v floor=${floor:-0} \
	-v jobber=$jobber \
	-v amt=$amt \
	-v max=$max \
	'
	{
		if( node )
		{
			for( i = 1; i < NF; i++ )
				if( index( $(i), "(node)" ) )
				{
					split( $(i), a, "(" );	
					if( node != a[1] )
						next;			# skip, not running on this node
				}
		}


		if( !floor || $(NF-1)+0 < floor )			# nothing supplied, or amount remaining is less than threshold
		{
			if( max && $(NF)+0 > max )			# job elapsed is just tooooooo much
			{
				printf( "lease for job NOT extended -- too much elapsed time: %s %s %s\n", $3, $(NF-1), $NF ) >"/dev/fd/2"
				next;
			}

			if( substr( amt, 1, 1 ) == "+" )
				printf( "ng_sreq -s %s extend %s %d\n", jobber, $3, amt+$(NF-1) );
			else
				printf( "ng_sreq -s %s extend %s %d\n", jobber, $3, amt );
		}
	}
' >$tfile

if [[ -s $tfile ]]
then
	case $forreal in
		0)
			log_msg "would extend leases with these commands:"
			cat $tfile 
			;;

		1)
			log_msg "extending leases with these commands:"
			ksh -x $tfile
			;;
	esac

else
	verbose "no jobs needed extensions"
fi

cleanup 0




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_extend_leases:Extend Seneschal leases for jobs)

&space
&synop ng_extend_leases [-f seconds] [-l [+]seconds] [-m seconds] [-n] [-N node] [-v] [jobname-pattern]

&space
&desc	&This will examine the list of jobs that seneschal indicates are currently running
	and will extend their leases if the current lease is less than the floor value (-f) 
	supplied on the command line.  Leases are reset to the value supplied with the -l 
	parameter.  If the lease value has a leading plus sign (+), then that number of 
	seconds are added to the current lease. 
&space
	By default, all executing jobs are considered for extension.  The set of jobs can 
	be limited by supplying a node name with the node (-N) option, or by supplying 
	a pattern to use to select only a subset of the running jobs. 

&space
	To prevent the extension of a list forever, &this implements a maximum execution 
	time which defaults to one day.  If a running job shows more than this amount of 
	elapsed time, it will not be considered for a lease extension. The maximum time 
	can be set to a value other than the default with the -m option.
&space
&subcat NOTE!
	With the implementation of ng_s_sherlock, this script should not be 
	necessary. This is not quite deprecated, but close, so using this 
	script is not advised. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f sec : The floor.  Jobs whose leases are less than this value will have their 
	leases extended.  The value supplied should be slightly more than the periodic 
	execution time of &this. The default is 400 seconds if not supplied which should 
	work well if &this is executed every five minutes (300 seconds).
&space
&term	-l sec : The lease extension.  Supplies the number of seconds to reset the lease to 
	for any job needing reset.   If &ital(sec) is supplied with a leading plus sign (+), 
	the current value of the lease is added to &ital(sec) and the lease is set to 
	this value. 

&space
&term	-m sec : The max elapsed time a job is allowd to be extended.  If seneschal reports
	a job has more elapsed time than &ital(sec), the job is no longer considered for 
	extension.

&space
&term	-n : No execution mode.  &This will go through the motions and only list what it would 
	like to do.

&space
&term	-N node : Matches only jobs running on &ital(node).
&space
&term	-v : Chatty mode. 
&endterms


&space
&parms	A single, optional, command line parameter may be supplied on the command line.  This is 
	a jobname pattern which is used to filter jobs from the list presented by seneschal. 
	The pattern is a regular expression pattern, and should be quoted on the command line.

&space
&exit	Always 0, unless somthing was not supplied correctly on the command line.
	

&examp	The following can be used to extend the leases for collate jobs that have 
	not been running longer than 10 hours
&litblkb
   ng_extend_leases -l 5000 -m 36000 collate
&litblke

&space
&see	ng_seneschal

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	11 Mar 2005 (sd) : Made an offical ningaui tool.
&term	14 Sep 2006 (sd) : Added note to man page.
&endterms

&scd_end
------------------------------------------------------------------ */
