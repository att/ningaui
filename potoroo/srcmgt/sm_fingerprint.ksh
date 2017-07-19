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


#   do NOT put a #! in the source file -- the build process on a non-ningaui node will choke!
# Mnemonic:	sm_fingerprint
# Abstract:	This peeks at the cvs history for files in the current directory and 
#		generates a fingerprint based on what it sees. If -l is used, a list
#		is generated rather than the fingerprint.  If -C is used, then a C 
#		char * string is generated into the named file in addition to printing
#		the fingerprint to stdout. 
#		
# Date:		15 Feb 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# ----------------------------------------------------------------------------------
ustring="[-C srcfile] [-c] [-i id] [-l] [-r src-root] [-s] [-v]"

date="20000101 00:00"				# how far to go back for history in source manager
all_mode=1					# -c turns off and generates just a list of changes
list=0						# -l turns on and generates a list for the user

save_list=${SM_FINGERPRINT_SAVE:-0}		# autobuild will set these so we dont have to hard code options in mk file
cfile="$SM_FINGERPRINT_CFILE"			
src_root=ningaui

while [[ $1 = -* ]]
do 
	case $1 in 
		-C)	cfile=$2; shift;;
		-c)	all_mode=0;;
		-i)	id=$2;;
		-l)	list=1;;	# list mode
		-r)	src_root=$2; shift;;
		-s)	save_list=1;;		# save the list in repmgr space

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

tfile=$TMP/xx.$$
d=${PWD##*/}
if [[ -z $id ]]
then
	id=$d
fi

eval dname=\${PWD##*$src_root/}
now=$(ng_dt -p)
echo "fingerprint of $src_root/$dname on ${now%% *}" >$tfile		# dont save time stamp -- we want all autobuilds to have same fingerprint

(
	find . -type f -print | sed 's/^/LS: /; s![.]/!ningaui/'"$dname"'/!' 
	ng_sm_history -r -a -d "$date" 
) | gre "${1:-$d}" | awk -v pdir=$d -v all_mode=$all_mode '
	/^LS: /	{			# record all that live in the dirctory tree
		if( $2 == "." || $2 == ".." )
			next;

		exists[$2]++;
		next;
		
	}

	/^M / || /^A / {		# suss through raw cvs output
		date=$2;
		time=$3;
		user = $5;
		v=$6
		bn = $7
		x = split( $8, a, "/" );
		pn = ""
		for( i=1; i <= x; i++ )
			pn = pn a[i] "/";
		pn = pn bn; 
	
		if( a[1] != "ningaui" )
			next;
	
		i = idx[pn]++;
		update[pn,i] = date " " time;
		person[pn,i] = user;
		version[pn,i] = v;
	}

	END {
		for( x in exists )		# for each file in the tree, show what history we found
		{				# output as filename#stuff#stuff  allowing us to sort later then format
			if( all_mode || idx[x]+0 > 0 )
			{
				printf( "%s %s", x, idx[x] ? "" : "#original" );
				for( i = 0; i < idx[x]; i++ )
					printf( "#%s %4s %10s", update[x,i], version[x,i], person[x,i] );
				printf( "\n" );
			}
		}
	}
' | sort | awk '
	/CVS/ { next; }

	{
		x = split( $0, a, "#" );
		printf( "%s\n", a[1] );
		for( i = 2; i <= x; i++ )
			printf( "\t%s\n", a[i] );
		printf( "\n" );
	}' >>$tfile


ng_md5 $tfile | read m5 junk
if (( $save_list > 0 ))
then
	fn="pf-fingerprint-$id-$m5-$(ng_dt -d)"
	ng_rm_where -n $fn|read junk nodes
	if [[ -z $nodes ]]			# does not exist
	then
		path=$(ng_spaceman_nm $fn)
		verbose "registering fingerprint file: ${path%\~}"
		cp $tfile $path
		ng_rm_register -v $path
	fi
fi

fp_string="[$id] [$(ng_dt -d)] [$m5]"
if [[ -n $cfile ]]
then
	verbose "added fingerprint to $cfile"
	echo "char *__ng_fingerprint_${id} = \"source fingerprint: $fp_string\";" >$cfile
fi
if (( $list > 0 ))		# chat to the tty
then
	cat $tfile
fi

echo "$fp_string"

cleanup 

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_fingerprint:Generate a source dir fingerprint

&space
&synop	ng_sm_fingerprint [-C csrc-file] [-c] [-i id-string] [-l] [-r src-root] [-s] [-v]

&space
&desc	&This gets a current set of history information from CVS and generates a list
	of revision information that serves as input to generate a fingerprint. The fingerprint, 
	a C &lit(char *) string, can be included by a programme as means for verifying 
	the programmes version based on revision state. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Changes only. If not supplied, all files in the current directory are 
	inlucded in the fingerprint. Using this option causes only files that have 
	a revision history (changes to the original file checked in) will be included.
&space
&term	-C filename : Supplies a file name into which the C constant string (char *) is 
	placed.  The variable name given is &lit(__ng_fingerprint_<id>.)
&space
&term	-i string : An ID string that is used in the C variable name and in the filename
	if &lit(-s) is used.  If not supplied, the last portion of the current path
	is used. 
&space
&term	-l : List mode. Causes a list of information to be generated rather than the 
	fingerprint.  
&space
&term	-r src-root : The name of the dirctory that is the root of the source tree. If 
	not supplied &lit(ningaui) is assumed. When computing a fingerprint value, &this
	will discard all of the directory path prior to the source root name. 
&space
&term	-s : Save mode.  In addition to generating the fingerprint, the history data
	that was extracted is saved and registered in replication manager space. It is 
	intended that this be used only when publishing the package. A new file is saved
	only if it does not exist; if a package is created multiple times from the same 
	source multiple files are not generated.  The filename will have the syntax:
	&lit(pf-fingerprint-<id>-<fingerprint>-<date>.)
&endterms


&subcat	Environment Variables
	These environment variables are generally set by the autobuild process to control
	operation desired when automatically generating packages. This allows the 
	invocations of &this to occur in the mkfile, without the corresponding commandline
	options needing to be set. 
&space
&lgterms
&term	SM_FINGERPRINT_SAVE : If set to 1 is the same as having added the &lit(-s) option. 
&term	SM_FINGERPRINT_CFILE : Sets the default name for the generated C fingerprint string.
&endterms

&space
&exit
	An exit code of 0 indicates success; all others failure.

&space
&see	ng_sm_history, ng_sm_stat, ng_sm_fetch, ng_sm_replace, ng_sm_add

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 Feb 2007 (sd) : The starting block.
&term	19 Apr 2007 (sd) : Corrected putting of date and path into the output file so that 
	fingerprints generated in directories that are not the same as the autobuild 
	directory yield the same result. 
&endterms


&scd_end
------------------------------------------------------------------ */
