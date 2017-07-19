#!/ningaui/common/ast/bin/ksh
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


#!/ningaui/common/ast/bin/ksh
# Mnemonic:	ng_proc_info
# Abstract:	Replacement for the guts of the isrunning/isalive functions 
#		in the procfun.ksh library functions.  The vast differences 
#		in ps output were causing the gre pattern to be too complex.
#		easier to maintain this way.
#		
# Date:		20 Sep 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-c)	count=1;;
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


	# assume input is from ng_ps:
	# USER       PID  PPID     TIME  STARTED CMD
awk 	-v cmd="$1" \
	-v parm="$2" \
	-v verbose=${verbose:-0} \
	-v count=${count:-0} \
	'	
		{
			ccol = 6;			# col we assume command starts in 
			if( $(ccol)+0 > 0 )		# assume date is $5 and $6 where $6 is day number and no comand starts [0-9]
				ccol++;			# e.g. ningaui  19684 18398 00:00:16   Sep 19 ng_rm_dbd

			if( $(ccol) == "ksh" || index( $(ccol), "bin/ksh" ) )		# allow for [bin/]ksh command name 
				this = $(ccol+1);					# skip the ksh stuff if there
			else
				this = $(ccol);

			x = split( this, a, "/" );					# just the basename in a[x]

			if( a[x] == cmd )
			{
				if( ! parm || index( $0, parm ) )			# only if we find the parm string too
				{
					if( count )
						found++;
					else
						if( verbose )
							printf( "%s \n", $0 );
						else
						{
							need_nl++;
							printf( "%s ", $2 );
						}
				}
			}
		}

		END {
			if( count )
				printf( "%d\n", found );
			else
				if( need_nl )			# only if we actually found something
					printf( "\n" );
		}
	'

cleanup $?



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_proc_info:List ps info for named commands)

&space
&synop	ng_proc_info [-c] [-v] command [unique-arg] <ng_ps-listing

&space
&desc	&This pokes through the information on standard input (assumed to 
	be output from ng_ps) and reports on the the &ital(command) 
	occurances found in the data.  By default, a list of process IDs
	for each process started with &ital(command) is written to 
	standard output. If &ital(unique-arg) is provided, only the 
	&ital(command) that contains the &ital(unique-arg) string 
	in its command line will be reported on. 
&space
	In verbose mode, the full record(s) matched are written to 
	standard output.  If the count (-c) option is given, only 
	a count of the number of records matched is given. 
&space
	&This does NOT execute an &lit(ng_ps) command as obtaining a 
	process list is expensive and if the caller has several processes
	to check (ng_node for example), it is more efficent to get one
	listing and parse it several times. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	 -c : Count. Simply emits the number of matches found.
&space
&term	-v : Verbose.  The full record of each match is written.
&endterms


&space
&parms	&This expects the name of the command to search for in the data. 
	Optionally, a second string may be supplied. Typically this is 
	a set of options that should be found on the command line in the ng_ps 
	listing that make the command unique.

&space
&exit
	Exits with the return code from the underlying awk.

&examp
&litblkb
   ng_ps | ng_proc_info ng_parrot   # list pids for all parrot procs

   ng_ps | ng_proc_info -c ng_parrot   # just the count

   ng_ps | ng_proc_info ng_endless "ng_parrot"  # endless processes wrapping parrot

   ng_ps | ng_proc_info ng_endless     # all endless processes 
&litblke



&space
&see
	ng_ps

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Sep 2006 (sd) : Its beginning of time. 
&endterms


&scd_end
------------------------------------------------------------------ */
