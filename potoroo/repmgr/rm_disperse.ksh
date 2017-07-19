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
# Mnemonic:	rm_disperse
# Abstract:	given a filename, compute hood information based on data in 
#		pinkpages or in a disperse table in site. 
#		output is of the form  <prefix> <sufix> such that the <prefix> can 
#		be used to find all of the variables known to repmgr. Mulitple
#		One or more file names can be given on the command line, or a list of filenames
#		can be supplied on stdin.  In the case of a stdin list, the output
#		is prefix, sufix, filename so that the caller can more easily match up 
#		the filename to the generated hood information.  
#		
# Date:		03 November 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# read filenames from stdin and generate a prefix suffix [filename] for each
function generate_hash
{
	get_tmp_nm data | read data	

	env | gre RM_DISP_ | awk '					# data from pinkpages
		{
			gsub( ".*=", "" )
			print;
		}
	' > $data

	if [[ -e $NG_ROOT/site/rm_disperse.table ]]			# load from table if there
	then
		cat $NG_ROOT/site/rm_disperse.table >>$data
	fi

	awk -v table=$data -v data2=$include_data -v extended=$extended -v prefixonly=$prefixonly '
		BEGIN {
			oidx = 0;

			while( (getline<table) > 0 )
			{
				if( ! pat[$1] )			# take only the first one we see
				{
					order[oidx++] = $1;
					pat[$1] = $1;
					sep[$1] = $2;
					const[$1] = $3;
					fields[$1] = $4;
					ext[$1] = $5;
				}
			}

			close( table );
		}

		function prt( p, s, data  )
		{
			if( prefixonly )
				printf( "%s\n", p );
			else
				printf( "%s %s%s\n", p, s, data2 ? " " data : "" );
		}

		# accept a: an array of fields (from a filename, but it doesnt matter), alen is the length
		#        f: an array of fielnum[:start:len]|format strings flen is the number in this array
		function gen_str( a, alen, f, flen,		i, fn, b, c, outstr )
		{
			for( i = 1; i <= flen; i++ )				# add each needed component from the string 
			{
				split( f[i], b, "|" );				# split fieldn|format into usable bits
				fn = b[1] + 0;					# make numeric in case its 3:start,len
				if( fn < 0 )
					fn = alen + fn + 1;			# -1 is last field, -2 is second to last... (ruby style)

				if( index( b[1], ":" ) )			# field:[start:len]
				{
					split( b[1], c, ":" );			# c[1] == field, c[2] == start, c[3] == len 
					value = substr( a[fn], c[2], c[3] );	# get start,len substring
				}
				else
					value = a[fn];

				outstr = sprintf( "%s%s" b[2], outstr, outstr == "" ? "" : "_",  value );
			}

			return outstr;
		}

		# generate the hash based on the filename.  data should be the filename and is printed as the third token 
		# if the include data flag is set (only when filenames read from stdin).  
		#
		function gen_hash( str, key, data, 		x, i, outstr, f, b )
		{
			x = split( str, a, sep[key] );				# split the string into components (e.g. pf aether 1001 1 junk)
			nfields = split( fields[key], f, "," );			# get field-num/format pairs to snag from components (order listed is order used)

			outstr = "";

			outstr = gen_str( a, x, f, nfields );			# generate the main string

			#estr = "";						# this isnt needed any longer; when patterns were hardcoded it was
			#if( extended && ext[key] != "" )
			#{
			#	nfields = split( ext[key], f, "," );
			#	estr = "_" gen_str( a, x, f, nfields );			# generate extension string
			#}

			prt( const[key] "_", outstr, data );
		}

		{ 
			gsub( "/.*/", "", $0 );  		# path is discarded

			for( i = 0; i < oidx; i++ )
			{
				p = order[i];

				if( match( $1, pat[p] ) > 0 )
				{
					gen_hash(  $1, pat[p], $0 );
					
					break;
				}
			}

			next;
		}
	'
}


# -------------------------------------------------------------------
ustring="[-e] [-p]"
include_data=0
extended=0
prefixonly=0

while [[ $1 == -* ]]
do
	case $1 in 
		-e)	extended=1;;
		-p)	prefixonly=1;;

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

if [[ $1 ]]			# one or more names given on the command line
then
	while [[ -n $1 ]]
	do
		echo $1 | generate_hash
		shift
	done
else
	include_data=1
	generate_hash
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_disperse:Generate a hood name based on a filename)

&space
&synop	ng_rm_disperse [-e] [-p] [-v] [filename]

&space
&desc
	&This accepts one or more file names on the command line and generates a hood prefix and 
	suffix to stdout for each.  
	If multiple filenames are given, the data is written in the same order. 
	The filename may be  omitted from the command line, and in this case &this assumes
	that a list of file names, one per line, is to be read from standard input. The list may be 
	any number of space separated tokens, but only  first token is treated as the filename.
	The output consists of a prefix and sufix where the prefix can be used to 
	find all of the related hood values in replication manger.  Together, they form the 
	hood value computed for the filename. 

&space
&subcat	Translations
	The filename  to  hood info translations is based on a specification defined in a table 
	file, &lit(NG_ROOT/site/rm_disprese.table) and/or pinkpages variables.  Pinkpages variable
	names have the form &lit(RM_DISP_XXXX) where &lit(XXXX) can be any desired string. The content
	of a variable, or line in the table file, is a set of space separated tokens. The following 
	lists these tokens in the order expected:

&space
&begterms
&term	pattern : This is the pattern used to match a file name.  If a filename matches the pattern
	then the remainder of the data is used to generate the hood information for the filename. 
&space
&term	seperator : This is the seperator character used to split the filename. 
&space
&term	prefix : This is the prefix constant string that is used for the file.  A trailing underbar
	(_) character is automatically added. 
&space
&term	field|fmt : A comma separated list of field and format specifications.  These are used in 
	order and define the field number (1 based) of the filename that are used to 
	construct the variable (suffix) portion of the hood.  The format portion is a C printf style
	format allowing the user to pad with leading zeros and the like.  
&space
	The field portion may be a single number (e.g. 3), or may be a colon separated tripple 
	indicating field number, start, and lengh (e.g. 3:1:8) such that only the substring of 
	the field is used. This can be useful if just the year, month and day of a human readable 
	date field is desired. 
&space
	The field number may be negative indicating fields from the end of the string (ruby style)
	such that -1 is the last field, -2 is the second to last, and so forth.
&endterms
&space

&subcat Search Order and Conflicts
	If the same pattern is defined both in a pinkpages variable and the table, the value in 
	pinkpages is used.  If the same pattern is defined multiple times in the disk table, 
	the first definition is used.   The order that a pattern is tested is the order that 
	it appears in the table file.  Patterns defined by pinkpages variables are tested first, 
	but the order that pinkpages variables are used is not defined.  If the order that patterns
	are applied to filenames is important, use only a definition table. 
	
&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-p : Prefix only.  The sufix is not generated and all that is needed on the command line 
	is enough of a filename to recognise the file type (e.g. pf-aether-). This allows a prefix to 
	be determined wihthout having to have an actual filename.
&space
&term	-v : Verbose. 
&endterms


&space
&parms
	Optionally the filename, or file prefix, can be supplied on the command line. If omitted, 
	&This assumes that the list of filenames is to be read from stdandard input.  When a list is 
	processed, the input record is echoed to standard output along with the prefix and suffix tokens.

&space
&exit
	A zero return code indicates a good status.

&examp
	Assuming the pinkpages variable RM_DISP_PFAETHER is defined as
&space
&litblkb
   ^pf-aether- - TACHYON_CHAN 3|%04d,4|%02d,-1:1:8|%s 
&litblke
&space
	then the filename
&space
&litblkb
   pf-aether-1001-3-2009090403123000-20090403124000.mpg 
&litblke
&space
	would yield the output: &lit(TACHYON_CHAN_ 1001_03_20090403)

&space
&see
	&seelink(repmgr:ng_repmgr)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Nov 2008 (sd) : Its beginning of time. 
&term	01 Jun 2009 (sd) : Rewrote so as to be table/pinkpages based. Made the man page more clear.
&endterms


&scd_end
------------------------------------------------------------------ */

