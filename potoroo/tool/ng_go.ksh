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
# Mnemonic:	ng_go
# Abstract:	ssh to a host named by a service name and optional cluster name
#		
# Date:		22 March 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="service-name [cluster]"

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

case $1 in 
	[A-Z]*) x=`ng_ppget -C ${2:-$NG_CLUSTER} srv_$1` ;;
	[a-z]*)	x=$1;;
	*) 	
		log_msg "$1 seems invalid"
		usage; exit 1;;
esac

if [[ -z $x ]]
then
	log_msg "$1 did not translate into a name for cluster: ${2:-$NG_CLUSTER}"
	cleanup 1
fi

ssh $x

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_go:Initate an SSH session based on service name)

&space
&synop	ng_go service-name [cluster-name]

&space
&desc	&This will translate the service name given on the command line (assuming that 
	Filer translatest to srv_Filer) into a host name and will start an ssh session 
	to that host.  If the cluster name is given as a second positional parameter
	the translation is done based on the pinkpages definition of the service for the 
	indicated cluster.  If cluster is ommited, the value of NG_CLUSTER is used. 

&space
&exit	Will exit with the value that sssh exited with.  If an error occurs before the 
	call to ssh, then a non-zero exit code is issued to indicate the error


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Mar 2005 (sd) : Sunrise.
&endterms

&scd_end
------------------------------------------------------------------ */
