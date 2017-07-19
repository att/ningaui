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
# Mnemonic:	rm_fstat 
# Abstract:	get repmgr stats about a file
#		
# Date:		
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-5] [-i] [-m] [-r] [-s] [-S] [-v] filename [filename...]"
what=""				# what stats to get
iwhat=""			# we force -i to print last if its supplied
host=${srv_Filer:-no_filer}
default_what="5 s i"	

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	what="5 s r m S i";;		# print all, in the order that they appear from repmgr; i must be last
		-5)	what="${what}5 ";;		# md5
		-i)	iwhat="i "; default_what="";;	# instances; separate to keep last in output
		-m)	what="${what}m ";;		# matched
		-r)	what="${what}r ";;		# rep count
		-s)	what="${what}s ";;		# size
		-S)	what="${what}S ";;		# stable

		-h)	host=$2; shift;;

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

# must use cat in the pipe to deal with the handshake character, else awk cannot deal with the first record
while [[ -n $1 ]]
do
	echo dump1 ${1##*/}
	shift
done | ng_rmreq -s $host -a  |cat -v  | awk -F "#" -v what="${what:-$default_what} $iwhat" '
	BEGIN {
		nneed = split( what, need, " " );
	}

	NF > 1 {					# allow blank lines, and possible conversion of handshake to newline
		split( $1, a, " " );
		printf( "%s", substr( a[2], 1, length( a[2] ) - 1 ) );

		for( i = 1; i <= nneed; i++ )
		{

			if( need[i] == "5" )
				printf( " %s", substr( a[3], 5 ) );
			else
			if( need[i] == "s" )
				printf( " %s", substr( a[4], 6 ) );
			else
			if( need[i] == "r" )
				printf( " %s", substr( a[5], 8 ) );
			else
			if( need[i] == "m" )
				printf( " %smatched", substr( a[7], 9 )+0 ? "" : "!" );
			else
			if( need[i] == "S" )
				printf( " %sstable", substr( a[8], 8 )+0 ? "" : "!" );
			else
			if( need[i] == "i" )				# instances
			{
				for( j = 2; j <= NF; j++ )
				{
					split( $(j), a, " " ) 
					if( a[1] != "attempts" && a[1] != "hint" )
						printf( "\n\t%s", $j );			# these we put on a new line
				}
			}
		}

		printf( "\n" );
	}
'
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_fstat:Generate filestats for a repmgr managed file)

&space
&synop	ng_rm_fstat [-5] [-a] [-h host] [-i] [-m] [-r] [-s] [-S] [-v]  filename [filename...]

&space
&desc	&This will generate the requeted information about a file that is managed
	by repliation manager. The information displayed is written on a single 
	line for each file. The first token written is the filename followed by 
	the information requested by command line options.  The information is 
	presented in the order that the flags appear on the command line, with 
	the exception of instances. If -i is supplied, the instances are always
	printed last and one per line with a leading tab character.
&space
	The information is obtained by a direct query to replication manager and 
	is the most efficent method of determining information on a per file basis. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-5 : Display the md5 checksum value.
&space
&term	-a : Causes all of the information for the file(s) to be displayed.  The order 
	displayed is: md5, size, replication count, matched status, and stabliity flag. The 
	instances are printed one per line following the initial line for the file.
&space
&term	-i : Display all instances.
&space
&term	-m : Display current matched value as "matched" or "!matched".
&space
&term	-r : Display the replication count.
&space
&term	-s : Display the file's size
&space
&term	-S : Display the current stable value as "stable" or "!stable".
&space
&term	-v : verbose mode.
&endterms


&space
&parms	At least one filename is expected. If multiple files are listed, they are processed
	in the order that they appear on the command line. 

&space
&see	&ital(ng_rmreq) &ital(ng_repmgr)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 Jul 2004 (sd) : Fresh from the oven.
&term   18 Aug 2004 (sd) : Converted srv_Leader references to corect srv_ things.
&term	27 Jul 2007 (sd) : Fixed bug in printing all information. 
&endterms

&scd_end
------------------------------------------------------------------ */
