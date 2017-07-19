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


# do not put a shell reference here or this cannot be used on a non-installed node!
#
# Mnemonic:	make_man.ksh
# Abstract:	Makes an include file that contains the necessary information 
#		for a C programme to generate an on the fly man page. The man
#		page is preformatted, and included during the compilation of the 
#		programme. The function show_man( ) is generated in the include
#	   	file and will dump the text to stdout.
#		This tool is NOT distrubited as a part of the bin directory.
# Date:		25 March 2001
# Author:	E. Scott Daniels
#
# --------------------------------------------------------------------------

function cleanup 	# overridden by stdfun if its there 
{
	rm -f /tmp/*.$$
	exit $1
}

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
if [[ -x $CARTULARY ]]
then
	. $CARTULARY
fi

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
if [[ -x $STDFUN ]]
then
	. $STDFUN
fi

if [[ -f $NG_SRC/include/scd_start.im ]]		# latest installed during precompile
then
		imbed=$NG_SRC/include
else
	if [[ -f $NG_SRC/.bin/scd_start.im ]]		# used to be installed here; should not be, but this does not hurt 
	then
		imbed=$NG_SRC/.bin
	else
		if [[ -f $NG_ROOT/lib/scd_start.im ]]	# fall back on installed flavour
		then
			imbed=$NG_ROOT/lib
		else
			imbed="${XFM_IMBED:-$HOME/doc/imbed}"		# probalby will not work, but try
		fi
	fi
fi

argv0=$0

function dump_man
{
	export XFM_PATH=".:$imbed"
	if [[ tfm_exists -gt 0 ]]
	then
 		tfm $argv0 stdout .im $imbed/scd_start.im |${PAGER:-more}
	else
		echo "unable to format manual page; tfm is not installed"
	fi
}

# ----------------------------------------------------------------------
#if [[ -x $NG_ROOT/bin/tfm ]]
if whence tfm >/dev/null  2>&1
then
	tfm_exists=1
else
	tfm_exists=0
fi

while [[ $1 = -* ]]
	do
		case $1 in
			--man) dump_man; cleanup 0;;
		esac

		shift
	done

base="${1%.*}"
suf=${1##*.}

if [[ $tfm_exists -gt 0 ]]
then
	secondary=`grep -c "\&secondary" $base.$suf 2>/dev/null`

	tfm $base.$suf stdout .im $imbed/scd_start.im | awk -v secondary=$secondary -v fname=${base##*/}.man '
	BEGIN {
		if( secondary )
			printf( "char *_man2_text[] = {\n" )>fname;
		else
			printf( "char *_man_text[] = {\n" )>fname;
 	} 

	{ 
		gsub( "\"", "10KAT2376", $0 );         # hide the " for now
		buf = sprintf( "\"%s\",", $0 );
		gsub( "\" *\"", "\" \"", buf );        # reduce blank lines to just one space
		gsub( "10KAT2376", "\\\"", buf );    # make " visible again 
 		printf( "%s\n", buf )>fname; 
	}

	END {
		printf( "(char *) 0};\n" )>>fname;      # terminate the list w/ null pointer
		close( fname );
	}
	' 
else
	cat <<endcat >${base##*/}.man
	char *_man_text[] = {
		"No formatted manual page available; tfm not installed during build",
		(char *) 0
	};
endcat
fi

cleanup 0

exit 0
# ------------------- SCD: Self Contained Documentation ---------------------

&scd_start
&doc_title(ng_make_man:Makes manual page include file for C programme.)

&space
&synop ng_make_man base_filename

&space
&desc	&This parses &ital(base_filename)&lit(.c) to generate the
	manual page for the programme. The manual page is converted into 
	a &lit(C) &ital(include) file and given the name 
	&ital(base_filename)&lit(.man).  The &lit(C) programme is expected
	to include the &lit(.man) file and to invoke the function 
	&lit(show_man()) when appropriate. 
	The &lit(show_man()) function is expected to reside in the Ningaui
	library and when invoked will
	cause the manual page contents to be displayed on the standard
	output device using the pager programme referenced by the
	&lit(PAGER) environment variable.
&space
	It is generally assumed that this script will not be invoked 
	directly by users, but placed in a &lit(Makefile) or other such
	build script.

&space
&parms	A single positional parameter is expected by &{this}: The basename
	of the &lit(C) file that is to have its &lit(.man) include file 
	generated. 

&space
&files	
&begterms
&term	base_filename.c : Assumed to exist and be the &lit(C) programme
	file that contains the self contained documentation section.
&space
&term	base_filename.man : File generated. Will contain the manual page 
	(generated from SCD in the &lit(^.c) file), and a function that can 
	be invoked by the programme to display it.
&endterms

&space
&examp	The following example illustrates how the &lit(.man) file can be
	generated for the &lit(scooter.c) programme.
&space
&beglitb
	ng_make_man scooter
&endlitb

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	25 Mar 2001 (sd) : Original Code
&term	01 Nov 2001 (sd) : Made such that it can be distributed.
&term	25 Jun 2003 (sd) : To write output to current directory rather than directory
	that the source is coming from -- viewpathing support
&term	16 Nov 2004 (sd) : Added -f option when cleaning up tmp files.
&term	06 Aug 2005 (sd) : Changed hardcoded assumption of .c. 
&term	15 Dec 2005 (sd) : Changed invocation when --man is entered.
&term	10 Oct 2006 (sd) : Sets tfm present flag based on whence and not hard path.
&term	01 Oct 2007 (sd) : Corrected man page.
&endterms

&scd_end
