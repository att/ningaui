#
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


#  do NOT put a #! line here -- let export/install do it!
# Mnemonic: 	genindex
# Abstract: 	Generates an index page for a directory of html files. File 
#		names are expected to have the following syntax:
#			[<catigory>.]<name>.html
#		(<name> may not contain any dot (.) characters)
#		The title from each document is extracted and used as the 
#		text for the link. If no title is present in the doc
#		then the file name is used.
# Date:		3 April 2001 (HBD me!)
# Author: 	E. Scott Daniels
# --------------------------------------------------------------------------


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

export XFM_IMBED=${XFM_IMBED:-NG_ROOT/lib}
 
# --------------------------------------------------------------------------------------
im=0					# set to 1 to generate an imbed file rather than html
output="index.html"
ustring="[-h header-file] [-l len] [-t title]"
headers=""
title="Ningaui Manual Pages"
len=0				# max len - zero says dont chop
workd=.


while [[ $1 = -* ]]
do
	case $1 in 
		-d)	workd=$2; shift;;
		-h)	headers="$2"; shift;;	# name of file with column header mappings
		-h*)	headers="${1#-?}";;
		-o)	output="$2"; shift;;

		-i)	im=1;; 			# generate imbed file, not html file

		-l) 	len=$2; shift;;		# max length of titles in display
		-l*)	len=${1#-?};;
		-t)	title="$2"; shift;;
		-t*)	title="${1#-?}";;

		--man)	show_man; exit;;
		-\?)	usage; exit;;
		*)	usage; exit 1;;
	esac

	shift
done

if [[ -n $headers && $headers != /* ]]
then
	headers=`pwd`"/$headers"
fi

if ! cd ${workd:-.}
then
	echo "unable to change to ${workd:-.}"
	exit 1
fi

if [[ -f list.html ]]
then
	rm -f list.html
fi

grep -H "<title>" *html >/tmp/titles.$$
cp /tmp/titles.$$ titles

ls *html *pdf 2>/dev/null| gre -v index.html | awk \
	-v headf="$headers" \
	-v tf=/tmp/titles.$$ \
	-v im=$im \
	-v h2="$title" \
	-v maxlen=$len \
	'
	BEGIN {
		while( (getline <tf) >0 )				# pull in the titles from html files
		{
			#filename:possible-junk<title>the title will be here[</title> junk]
			match( $0, "<title>" );
			tstart = RSTART + RLENGTH;
			if( match( $0, "</title>" ) )
				t = substr( $0, tstart, RSTART - tstart );
			else
				t = substr( $0, tstart )     # assume it goes to end of line
			split( $0, a, ":" );
			gsub( "<[^<]*>", "", t );
			if( maxlen && length( t ) > maxlen )
				t = substr( t, 1, maxlen ) "^...";
			gsub( "\\(", "^(", t );
			gsub( "\\)", "^)", t );
		 	title[a[1]] = t;
		}
		close( tf );

		coidx = 1;            # cat order index (MUST be intialised)
		#cat_order[0] = "misc";
		cat_idx["misc"] = 0;

		if( headf )
		{
			hpoi = 0;
			found_misc = 0;

			while( (getline <headf) >0 )
			{
				if( substr( $1, 1, 1 ) != "#" )		# allow comments
				{
					hpo[hpoi++] = $1;			# display in order received 
					if( $1 == "misc" )
						found_misc++;

					for( i = 2; i <= NF; i++ )
						header[$1] = header[$1] $(i) " ";
				}
			}
	
			close( headf );

			if( ! found_misc )			# we will add this for them if not supplied
			{
				hpo[hpoi++] = "misc";
				header["misc"] = "Uncatigorised Documents";
			}
				
		}
		else	# no file, these are simple defaults
		{
			hpoi = 0;
			hpo[hpoi++] = "cn";		header["tcm"] = "Collection Node";
			hpo[hpoi++] = "lib";		header["lib"] = "C Library Functions (libng-g.a)";
			hpo[hpoi++] = "silib";		header["silib"] = "C Socket Callback Library (libng_si-g.a)";
			hpo[hpoi++] = "jobmgt";		header["jobmgt"] = "Job Management";
			hpo[hpoi++] = "repmgr";		header["repmgr"] = "Replecation Manager"
			hpo[hpoi++] = "cycle";		header["cycle"] = "Cycle Management";
			hpo[hpoi++] = "zoo";		header["zoo"] = "Zookeeper Things";
			hpo[hpoi++] = "pinkpages";	header["pinkpages"] = "Pinkpages and Narbalek";
			hpo[hpoi++] = "tool";		header["tool"] = "Tools and Utilities";
			hpo[hpoi++] = "feeds";		header["feeds"] = "Feeds: Parsers ";
			hpo[hpoi++] = "feedtool";	header["feeds"] = "Feeds: tools";
			hpo[hpoi++] = "feedlib";	header["tcm"] = "Feeds: Library";
			hpo[hpoi++] = "tape";		header["tape"] = "Tape"; 
			hpo[hpoi++] = "lib_ruby";	header["lib_ruby"] = "Ruby Modules"; 
			hpo[hpoi++] = "misc";		header["misc"] = "Uncatigorised Documents";

		}

		if( ! im )
		{
			printf( ".dv doc_title %s\n", h2 ? h2 : "HTML Document Index" );
			printf( ".dv back_color ^#000000\n" );
			printf( ".dv text_color ^#ffffff\n" );
			printf( ".dv link_color link=#c9c900 vlink=#a0a000\n" );
			printf( ".gv e XFM_IMBED imbed\n.im &{imbed}/hfmstart.im\n" );
 			printf( ".dv textsize 10\n" );
			printf( ".st &textsize\n" );
			printf( ".hn off\n" );
			printf( ".h2 %s\n", h2 ? h2 : "HTML Document Index" );
		}
	}

	{
		total++;

		n = split( $1, a, "." );
		if( a[2] == "html" || a[2] == "pdf" )		# xxx.pdf we assume that xxx is not the catigory
		{
			cat = "misc";
			ref = a[1] ".html";				# reference assuming linked pdf files will be 	ref.pdf for a ref.html
		}
		else
		{
			cat = a[1];				# pull catigory from the first node 
			ref = a[1] "." a[2] ".html";				# xxx.ref.pdf should get xxx.ref.htmls title if possible
		}

		if( !cat_seen[cat]++ )
		{
			cat_idx[cat] = 0;
			if( ! header[cat] )
			{
				header[cat] = cat;
				hpo[hpoi++] = cat;
			}
		}

		if( a[n] == "pdf" )
		{
			pdf[cat,cat_idx[cat]] = $1;
			key[cat,cat_idx[cat]] = "pdf";
			txt[cat,cat_idx[cat]] = title[ref] ? title[ref] : "{" $1 "}";
		}
		else
		{
			if( (x = index( title[$1], "- " )) )       # ? title[$1] : "<" $1 ">";
			{
				
				key[cat,cat_idx[cat]] = substr( title[$1], 1, x-1 );
				txt[cat,cat_idx[cat]] = substr( title[$1], x+1 );
				gsub( "- *", "", key[cat,cat_idx[cat]] );
			}
			else
			{
				if( title[$1] == "" )
				{
					key[cat,cat_idx[cat]] = "<notitle>";
					txt[cat,cat_idx[cat]] = $1;
				}
				else
				{
					x = split( $1, zz, "." );
					key[cat,cat_idx[cat]] = " ";
					txt[cat,cat_idx[cat]] = zz[x-1];
				}
			}
			file[cat,cat_idx[cat]] = $1;
		}
		cat_idx[cat]++;

	}

	function showcat( cat )
	{
		#cat = cat_order[c];

		if( cat_idx[cat] <= 0 )
			return;
	
		#printf( "&uindent\n.sp 1 &stress(%s) .br &indent\n", header[cat] ? header[cat] : cat );
		printf( ".ed\n%s\n.sp 1\n&stress(%s) .br \n.bd 1.2i\n", need_line ? ".ln" : " ", header[cat] ? header[cat] : cat  );
		need_line++;
		for( j = 0; j < cat_idx[cat]; j++ )
		{
			if( file[cat, j] )
				printf( ".di &llink(%s:%s) : &llink(%s:%s)\n", file[cat,j], key[cat,j], file[cat,j],  txt[cat,j] );
			if( pdf[cat,j] )
				printf( ".di &llink(%s:%s): &llink(%s:%s)\n", pdf[cat,j],  key[cat,j], file[cat,j],txt[cat,j] );		# if we have a pdf file that goes with it 
		}
	}

	END {
		need_line = 0;
		ncol1 = total * .65;			# where we break left column

		printf( ".ta 3i 3i\n" );

		#for( i = 0; i < coidx; i++ )
		for( i = 0; i < hpoi; i++ )
		{
			if( col_tot && col_tot + cat_idx[hpo[i]] > ncol1 )		# time to bump to next col?
			{
				need_line = 0;
				printf( ".ed\n" );
				col_tot = -total;
				printf( ".cl\n" );
			}

			showcat( hpo[i] );
			col_tot += cat_idx[hpo[i]];
		}

		printf( ".ed\n.et\n" );

		if( !im )
			printf( ".gv Date\n.gv Time\n.ln\nPage updated &_date @ &_time\n" );
	}
' >/tmp/list.$$

cp /tmp/list.$$ index.xfm

if [[ $im -gt 0 ]]
then
	mv /tmp/list.$$ ${output%.*}.im
else
	hfm /tmp/list.$$ $output
fi

rm /tmp/*$$
exit 0			# needed for scd

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(genindex - Generate Index Of HTML Documents)

&space
&synop	genindex [-d dir] [-h header-list]  [-i] [-l len] [-o output-fname] [-t page-title]

&space
&desc	&ital(Genindex) changes to either &ital(dir-path) if specified
	or the ningaui HTML directory (&lit(~/html)) and creates an 
	HTML file names &lit(list.html). The file contains a link to 
	each HTML document in the directory. The text that is formatted
	to be displayed by a browswer is the text contained in the 
	&ital(title) tags of the document. If the document is untitled, 
	then the file name is presented in angle braces (&lit(<) and &lit(>)).
	
&space
	Documentation titles are formatted into two columns with 
	an attempt to keep the columns equal in length. Catorgories 
	are not split between columns. The attempt to equally divide
	the data is foiled if the last catigory listed has more than half 
	of the total items to display is in the last catigory to be printed. 
	
&space
&subcat	File Cagigorisation
	Files in the directory will be grouped in the output list 
	if their file names have the following syntax:
&space
	&lit(<catigory>.<name>.html)
&space
	The &ital(catigory) portion of the file name is used to 
	look up a header string that is placed at the top of the 	
	column in the output HTML document. If the catigory name 
	does not translate to a defined header string, the catigory
	portion of the filename is used. 
	Users may define catigory headers by supplying
	a file with the &lit(-h) option, where the file contains 
	records with the following syntax:
&space
	&lit(<catigory><whitespace><header-text>)
&space
	Comment lines (lines whose first token begins with a 
	hash character) are permitted. Catigories are placed on the page
	in the order that they are listed in this file and catigories
	that are defined, but have no HTML files, are not printed.

&space
&opts
	The following options are supported by &ital(genindex).
&space
&begterms
&term	-d dir : Directory where the html files reside. If not supplied
	the current directory is used. 
&space
&term	-h file : Specifies the file name that defines the header 
	strings that should be used when interpreting the first node
	of the file name. If this parameter is not supplied, then 
	the default header strings will be used. 
&space
&term	-i : Imbed file for XFM is generated rather than a complete 
	HTML file. This allows the index to be incorporated inside of 
	another XFM generated document. The file generated is 
	&lit(index.im) unless -o is used. If -o is used, then the 
	extension (if given) is removed and &lit(.im) is added.
&space
&term	-l n : Defines the maximum length, in characters, that each 
	document title will be truncated to. 
&space
&term	-t string : Supplies the string that is used as the document
	title for the generated index. 
&endterms

&space
&parms	&ital(Genindex) accepts the name of the directory to search 
	for HTML files as the only parameter on the command line. If 
	this directory path is not given, then the current working directory
	is used. 
	The output file, &lit(index.html) is left in the current directory.

&space
&exit	A zero return code indicates that processing completed normally.
	Any exit code that is not zero indicates that &ital(genindex)
	was not able to complete its task as requested. 

&subcat Environment Variables
	The following environment variables must be set:
&begterms
&term	NG_ROOT : Set to the ningaui root, or the build root (NG_SRC/bld_root).
space
&term	TMP : Set to a temporary directory to use (NG_ROOT/tmp, or /tmp).
&space
&term	XFM_IMBED : Generally set to $NG_ROOT/lib, this directory contains
	the XFM setup macro files.
&endterms

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Apr 2001 (sd) : Original Code. (HBD2me)
&term	10 Jan 2005 (sd) : Adjusted column swtich to better even the to columns.
&term	31 Aug 2006 (sd) : Defaults now to working in the current directory. 
&term	29 Mar 2007 (sd) : Beefed up the man page a bit. 
&term	22 Jan 2007 (sd) : Changed the unvistied link colour. 
&term	19 Aug 2009 (sd) : Handles titles that dont have module name - description syntax.
&endterms

&scd_end
------------------------------------------------------------------ */
