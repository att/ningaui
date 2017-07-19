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
# Mnemonic:	ng_species
# Abstract:	Returns a list of nodes on the current cluster that have the 
#		attributes that were passed in. 
#		
# Date:		06 Oct 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
cluster=$NG_CLUSTER
ustring="[-c cluster] [-v]"

timeout=10

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	allf="-a";;
		-c)	cluster=$2; shift;;
		-t) 	timeout=$2; shift;;
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

need="$@"			# need nodes with these attributes
ng_members -N -c $cluster | read nlist

for n in $nlist
do
	ng_get_nattr  $n |read nattr
	#ng_get_nattr -t $timeout $n |read nattr
	if  ng_test_nattr $allf -l "${nattr:-1014MDP}" $need		# test as though running on node $n (junk tested if node did not respond)
	then
		list="$list$n "
	fi
done

echo $list

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_species:List a set of nodes based on attributes)

&space
&synop	ng_species [-c cluster] [-v] attribue [attribute...]

&space
&desc	&This will list all nodes in the cluster having the attribute(s) indicated on the command line.

&space
&opts	These command line options are recognised:
&begterms
&term	-c cluster : Get a list for members of another cluster.
&space
&term	-v : verbose mode.
&endterms

&space
&parms	One or more attribute names are expected on the command line. 

&space
&exit	Always 0.

&space
&see	ng_get_nattr, ng_get_likeme

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Oct 2003 (sd) : Thin end of the wedge.
&term	27 Jan 2004 (sd) : Corrected a bug with srv_ when executed on leader node
&term	10 Feb 2004 (sd) : Corrected bug where a non-responsive node was being assigned the attributes
	of the local node. 
&term	14 Jan 2005 (sd) : Added -c cluster option, and it now uses narbalek members options.
&endterms

&scd_end
------------------------------------------------------------------ */
