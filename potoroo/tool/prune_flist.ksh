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
# Mnemonic:	prune_flist
# Abstract:	Runs a list of node file pairs and prunes all files that 
#		are associated with any node in the exclude list.  If the 
#		'must have 1' flag is set on the command line (-m), then 
# 		every file listed in the input must appear at least once
#		in the resulting set or the script exits with an error.
#		expects the list on stdin and writes the pruned list onto 
#		stdout (if there are no errors).
#		
# Date:		16 September 2003 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------

musthave1=0
keep=0

while [[ $1 = -* ]]
do
	case $1 in 
		-k)	keep=1;;
		-m)	musthave1=1;;
		-n)	nodes="$nodes$2 "; shift;;

		
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

awk \
	-v nodes=" $nodes" \
	-v musthave1=$musthave1 \
	-v keep=$keep \
'
	function base( f,		a, x )
	{
		x = split( f, a, "/" );
		return a[x];
	}

	{
		node = $1;
		bname = base( $2 );

		count[bname]++;
		if( index( nodes, " " node " " ) )	# its in the list
		{
			if( keep )			# keep things in the list
				printf( "%s\n", $0 );		
			else
				count[bname]--;		
		}
		else					# not in the list
		{
			if( !keep )			# keep things not in the list
				printf( "%s\n", $0 );		
			else
				count[bname]--;		
		}
	}
	
	END {
		if( ! musthave1 )
			exit( 0 );		# no need to check -- all is well by default 

		for( f in count )
			if( count[f] == 0 )
			{
				printf( "0 instances remained: %s\n", f ) >"/dev/fd/2";
				errors++;
			}

		if( errors )
			printf( "%d files had no instances after prune.\n", errors ) >"/dev/fd/2";

		exit( errors ? 1 : 0 );
	}
'
cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(name:short description

&space
&synop

&space
&desc

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&endterms


&space
&parms

&space
&exit

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2002 (sd) : Thin end of the wedge.
&endterms

&scd_end
------------------------------------------------------------------ */
