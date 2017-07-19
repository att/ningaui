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

verbose=0

while [[ $1 = -* ]]
do
	case $1 in
		-v) verbose=1;;
	esac

	shift
done

# analyses a dump 4 from seneschal reporting on which nodes a job can run on based on the files
# that are present on the node.
awk -v verbose=$verbose '
	BEGIN {
		jobs_cant = 0;
	}

	function snarf_data( 	a, x )
	{
		for( i = 1; i <= NF; i++ )
			if( index( $(i), "(" ) )
			{
				split( $(i), a, "(" );
				data[substr(a[2], 1, length(a[2])-1)] = a[1];
			}

	}

	function clear_data( 	x )
	{
		for( x in data )
			data[x] = "";
	}

	function spit_stats( 	i, x, ncount, runlist )
	{
		runlist = "";

		for( x in filec )
		{
			if( fidx && filec[x] == fidx )
			{
				ncount++;
				runlist = x " " runlist;
				runon[x]++;
			}

			filec[x] = 0;
		}

		if( verbose || (fidx && !ncount) )
			printf( "%s: %d nodes: %s\n", data["name"], ncount, runlist );

		jobs_total++;
		jobs_nodes[ncount]++;
	}

	/pending:/ {
			if( data["name"] != "" )
				spit_stats( );

			clear_data( );
			snarf_data( );
			pcount++;

			fidx = 0;
	}

	/pending input:/ {
		for( i = 5; i <= NF; i++ )
			filec[$(i)]++;


		fidx++;
	}

	END{
		if( data["name"] != "" )
			spit_stats( );

		for( x in runon )
			printf( "%s could get %d jobs\n", x, runon[x] );

		printf( "%d pending; %d NOT runable; ", jobs_total,  jobs_cant  )
		for( i = 1; i < 15; i++ )
			if( jobs_nodes[i] )
				printf( "%d(%d nodes) ", jobs_nodes[i], i );
		printf( "\n" );
	}
'
