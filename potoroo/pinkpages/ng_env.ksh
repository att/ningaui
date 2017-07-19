#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
# Mnemonic:	ng_env
# Abstract:	print information about the ningaui environment/cartulary.
#		
# Date:		19 May 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-h hostname] [-c clustername]"
hosts=""

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	hosts="$hosts`ng_rcmd $2 ng_members -R` "; shift;;
		-h)	hosts="$hosts$2 "; shift;;

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

vals="$@"

if [[ -z $hosts ]]
then
	hosts=`ng_sysname`
fi

for h in $hosts
do
	if [[ -z $vals ]]
	then
			ng_rcmd $h 'ng_wrapper env' |awk -v host=$h '{ sub( "=", "=\"" ); printf( "%s: %s\"\n", host, $0 );}'


	else
		for v in $vals
		do
			ng_rcmd $h 'ng_wrapper eval echo \$'"$v" |read val
			echo "$h: $v=\"$val\""
		done
	fi
done
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_env:Display ningaui environment/cartulary settings)

&space
&synop	ng_env [-c cluster] [-h host] [variable-name...]

&space
&desc	&This will look up the variable name(s) supplied on the command line on the 
	host(s) indicated by either the hosts supplied using &lit(-h) or all hosts
	for any clusters supplied on the command line.  The result is one record written 
	to standard out for each host/variable combination. Each record will have the 
	format:
&space
&litblkb
   <hostname>: <varname>="<value>"
&litblke

&space
	If no variable name is supplied on the command line, then a 'dump' of the 
	environment as it exists after sourcing (dotting) in the cartulary is written 
	to the standard output device. The dump occurs for each host indicated on 
	the command line, or for all hosts indicated by each cluster name supplied.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c cluster : Supplies the name of a cluster.  All hosts will be querried for the 
	value of the supplied variable(s). Multiple clusters may be supplied with 
	multiple -c options, or as a space separated string (-c "clust1 clust2).
&space
&term	-h host : Supplies the name of a host to querry. Multiple hosts may be supplied with 
	multiple -h options or as a space separated string (-h "host1 host2").
&endterms

	If no host, or cluster option is supplied, then the local host is querried.

&space
&parms
	Following any options the variable names (e.g. NG_ROOT) may be supplied. Each variable
	is looked up on the respective host and its value is presented.  If a variable name is not 
	set then it is returned as having a value of a null string.  
&space
	If no variable names are supplied on the command line, then a dump of the environment on 
	each host is requested and presented on standard out using the syntax described earlier.
&space
&exit
	Always exits with a zero return code. 

&space
&see	ng_ppget ng_ppset ng_pp_build

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 May 2004 (sd) : Fresh from the oven.
&term	17 May 2008 (sd) : Now executes an ng_members command using -R (registered) to avoid 
		issues if there is not a filer on the cluster. 
&endterms

&scd_end
------------------------------------------------------------------ */
