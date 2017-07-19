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
#!/usr/common/ast/bin/ksh
# Mnemonic:	mkpkg_ast.ksh
# Abstract:	Make a ningaui package with the few ast things we need. The things 
#		put into the package are all libraries and about 5 binaries.
#		
# Date:		24 July 2005 (HBD D&B)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# This funciton echos out the arch directory name to use.
# we must generate arch names similar to what is used by the ast build process. typically that 
# is  osnameN.hwtype   where osname is the operating system name in lower case, N is the 
# major version number of the O/S, and hwtype is an attempt to identify the hardware (386, 686,
# power pc, etc.  Unfortunately, we cannot predict what will be in the ast build directry, so we 
# generate a bunch of choices and go with the first one we find.  We also notice that ast likes
# ppc rather than powerpc as returned by the system installed uname command. 
#
# this is all avoided if there is only one directory present -- we assume non-nfs builds and should
# only ever find one directory. 
# -----------------------------------------------------------------------------------------------
function find_dir
{
	typeset x=""
	typeset list=""

	if (( $(ls $1 | wc -l ) == 1 ))		# if just one, then use it. 
	then
		x=$( ls $1 )
		echo ${x##*/}
		return
	fi


	# more than one -- try to pick the best one
	(
		ng_uname -psr			# must use ng_uname first, but under linux it does not generate matches 
		uname -psr		
		case $(ng_uname -p) in
			powerpc)	echo "$(ng_uname -sr) ppc";;                    # ast likes this string -- erk!
			x86_64)		echo "$(ng_uname -sr) i386-64";;			# ast likes to assign this string
			i686)		echo "$(ng_uname -sr) i386";;			# grumble.
			sun4*)		echo "sol 9 sun4";;			# bloody ast creates dirs as sol9.sun4u -- double erk
		esac
	) | awk '
		{ 
                        if( $1 == "SunOS" )
                        {
                                split( $2, a, "." );					# ast uses x rather than y from y.x
                                printf( "%s%d.%s ", "sol", a[2]+0, $3 );                # dont know why ast insists on sol
				if( $3 == "sparc" )					# ast also seems to xlate this to sun4, so generate it too
                                	printf( "%s%d.sun4 ", "sol", a[2]+0 );                
                        }      

			printf( "%s%d.%s ", tolower( $1 ), int($2+0), $3 );
			printf( "%s.%s ", tolower( $1 ), $3 );
		}
	'|read list 	# ast package style source arch

	for x in $list
	do
		if [[ -d $1/$x ]]
		then
			verbose "this seems to be the directory that is right: $x" 
			echo $x								# output to calling function
			break;
		fi

		verbose "did not find directory: $1/$x"
	done
}


function make_dir
{
	if ! mkdir $1
	then
		log_msg "unable to make directory: $1"
		cleanup 1
	fi
}

function ensured
{
	if [[ -d $1 ]]
	then
		verbose "verified: $1 is a directory [OK]"
		return
	fi

	log_msg "error: $1 is not a directory [FAIL]"
	cleanup 1
}

# -------------------------------------------------------------------
ustring="[-d] [-g] [-v] -s source-dir"
idir=$PWD
absolute=0
preserve="-p"			# preserve option on cp
install_into_date=0		# -d sets on which causes the directory ast_yyyymmdd to be used as refurb unload dir
pkg_data=""			# -D used to set with date of ast files
disable=""			# -r (remove) adds to this list; these arch/bin files are renamed to x.disable to turn them off 
me=$( ng_sysname )

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	absolute=1;;
		-d)	install_into_date=1;;
		-D)	pkg_data="$2"; shift;;
		-P)	preserve="";;				# do not attempt to preserve date/time on binaries
		-r)	disable="$disable$2 "; shift;;
		-s)	sdir=$2; shift;;			# where to find stuff

		-v)	verbose=1;;
		-t)	find_dir ${2:-.}; cleanup 0;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -z $sdir ]]
then
	log_msg "must enter -s source-dir"
	cleanup 1
fi

need="ksh pax pax-static pzip md5sum nmake"

TMP=${TMP:-$NG_ROOT/tmp}
date "+%Y%m%d"|read date			# cannot assume ng_ things are installed on this node
ng_uname -s |read os
ng_uname -r |IFS="." read maj min junk
ng_uname -p |read arch                             # -m on macs generates a text sentance rather than "ppc" generated by -p
ver=$maj.${min%%-*}
systype="$os-$ver-$arch"
pname="pkg-ast-$systype-$date-rel.tgz"

#  assume something like ~creator/arch/freebsd5.i386  is the directory to use; build the name and test for it
if (( $absolute  == 0 ))
then
	find_dir $sdir | read sarch

	if [[ foo == bar ]]
	then
	ng_uname -psr|awk '{ printf( "%s%d.%s\n", tolower( $1 ), int($2+0), $3 );}'|read sarch 	# ast package style source arch

	if [[ ! -d $sdir/$sarch ]]			# if a directory with version number appended does not exist, look for alternative
	then
		uname -psr|awk '{ printf( "%s%d.%s\n", tolower( $1 ), int($2+0), $3 );}'|read sarch 	# ast package style source arch
		if [[ ! -d $sdir/$sarch ]]			# if a directory with version number appended does not exist, look for alternative
		then
			ng_uname -psr|awk '{ printf( "%s.%s\n", tolower( $1 ), $3 );}'|read sarch 	# ast package style source arch w/o ver number
		fi
	fi
	fi
else
	sarch=""			# assume path provided is into the arch directory
fi

verbose "getting files from source: $sdir/$sarch"
ensured $sdir/$sarch/bin 		# make sure src directories exist
ensured $sdir/$sarch/lib
ensured $sdir/$sarch/include

if [[ -n $disable ]]
then
	for x in $disable
	do
		if [[ -f $sdir/$sarch/bin/$x ]]
		then
			log_msg "$sdir/$sarch/bin/$x being disabled in package [OK]"
			mv $sdir/$sarch/bin/$x $sdir/$sarch/bin/$x.disabled
		fi
	done
fi

wdir=$TMP/wrk.$$

# we have an odd way of doing this.  we add an install script to the mix, so we build a working directory
# of everything that we want in the package file, add the install script, and then create the tar. 
#
make_dir $wdir
make_dir $wdir/bin
make_dir $wdir/lib
make_dir $wdir/include

if ! cd $wdir
then
	log_msg "cannot switch to $wdir"
	rm -fr $wdir
	cleanup 1
fi

if (( $install_into_date > 0 ))		# we want to install into ast_yyyymmdd  directory 
then
cat <<endKat >INSTALL
#!/usr/common/ast/bin/ksh
	# script to install by determining the unload directrory and calling refurb
	# we expect the package filename as $1 (pkg-ast-system-date-rel.tar or somesuch)
	p=\${1%%-rel*}
	p=\${p##*-}		# get just date form the package name
	if [[ ! -d /usr/common/ast_\$p ]]
	then
		echo "INSTALL: making /usr/common/ast_\$p"
		if ! mkdir /usr/common/ast_\$p
		then
			echo "could not make /usr/common/ast_\$p"
			exit 1
		fi
	fi
	echo "INSTALL: invoking refurb for actuall install"
	set -x
	ng_refurb -v -I -u /usr/common/ast_\$p \$1 >/tmp/ast_refurb.err 2>&1
endKat
else				# we will install into usr/common/ast_current
cat <<endKat >INSTALL
#!/usr/common/ast/bin/ksh
	# script to install ast stuff; just reinvokes refurb with the correct unload directory
	# we expect the package filename as $1 (pkg-ast-system-date-rel.tar or somesuch)
	if [[ ! -d /usr/common/ast_current ]]
	then
		echo "INSTALL: FAIL: cannot find directory: /usr/common/ast_current"
		exit 1
	fi
	echo "INSTALL: invoking refurb for actuall install into /usr/common/ast_current"
	echo "INSTALL: ast package data: $pkg_data"
	echo "INSTALL: build node: $me @ $(ng_dt -p)"
	set -x
	ng_refurb -v -I -u /usr/common/ast_current \$1 >/tmp/ast_refurb.err 2>&1
endKat
fi

chmod 775 INSTALL

if ! cd $sdir/$sarch
then
	log_msg "cannot switch to $sdir/$sarch"
	rm -fr $wdir
	cleanup 1
fi

# pax -v -r -w . $wdir/lib    # do NOT use pax -- need to copy linked files rather than links
tar -chf -  bin include lib man share |(cd $wdir; tar -xf -)		# copy lib and include directories


cd $wdir
if ! tar -cf - . |gzip >$idir/$pname
then
	log_msg "could not create: $idir/$pname"
	rm -fr $wdir
	cleanup 1
fi

rm -fr $wdir

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_mkpkg_ast:Make A Ningaui Package Of AST Things)

&space
&synop	ng_sm_mkpkg_ast [-a] [-d] [-D pkg-data] [-r name] [-P] [-v] -s source-dir

&space
&desc	&This creates a standard ningaui packge, that can be installed with
	ng_refurb. Originally the package contained only the parts of AST that
	were needed by ningaui, now it contains all of the bin, lib, include, 
	and man dirctories.  
&space
	The package also includes an INSTALL script that will be executed by ng_refurb
	to do the installation.  The INSTALL script determines the target directory
	(/usr/common/bin/ast_<pkg-date>) and invokes ng_refurb with it specified as the specific 
	unload directory.

&space
	Actual exeution of the refurb might require manual steps if the /usr/common directory
	is owned by root and not writable by the user running the refurb. Folloging the refurb, 
	the symbolic link (/usr/common/ast) will need to be reassigned; the install script
	purposely does NOT do this. 
&space
&subcat The source directory
	The source directory (-s) is assumed to be the path to a directory containing one or more 
	subdirectories named for the type of architecture that is supported by the contents of 
	the directory (e.g. freebsd.1386).  &This will attempt to determine which sub directory to 
	use.  If a full path into the archiceture specific directory is needed (&this cannot determine
	which subdirectory to use), it can be supplied with the -a command line option. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Absolute path.  The source directory is a full path. The directory is expected to contain 
	the bin, lib and include directories that will be placed into the package. 
&space
&term	-d : Set up the install script to install into a date based directory rather than /usr/common/ast_current. The directory is 
	assumed to be /usr/common/ast_yyyymmdd where the date matches the package date. This is 
	the old way, and is not recommended as it requires manual labour on each node to ensure that 
	the directory exists and is writable for each new package. 
&space
&term	-D string : Provides package data that is placed into the INSTALL script. 
&space
&term	-P : Do not attempt to preserve the date/time on files. 
&space
&term	-r name : Causes the &ital(name) binary to be disabled (effectively removed) from the package. 
	Files are expected to be in the arch/bin directory after ast has been built. The named file(s) are
	renamed with a &lit(.disabled) suffix which will prevent casual execution, but keeps them in 
	the package if they need to be enabled after installation.
&space
&term	-s dir : The directory where the 'source' files are to be found.  It is assumed that
	this directory will contain one or more subdirectories based on machine architecture
	and operating system version (e.g. freebsd5.i386).  The directory naming is consistant
	with the AST package and build process, not with the output of uname(1). 
&space
&term	-v : Verbose mode.
&endterms


&space
&exit	A zero exit code indicates success; all other exit codes indicate failure.

&space
&see	ng_refurb, ng_sm_mkpkg

&examp
&litblkb
   ng_sm_mkpkg_ast -v -s ~gsf/arch
&litblke

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Jul 2005 (sd) : Sunrise. (HBD EKD/ESD)
&term	17 Aug 2005 (sd) : Adds include directory too.
&term	22 Dec 2005 (sd) : Corrected uname calls to use ng_uname. 
&term	07 Feb 2006 (sd) : Shifted toward installing into a fixed directory.
&term	14 Jun 2007 (sd) : Now captures all of ast/bin and ast/man. 
&term	18 Jun 2007 (sd) : Had to adjust for odd arch name generated by ast.
&term	13 Jul 2007 (sd) : Added code to support Sun 5.10 on pc.
&term	13 Aug 2007 (sd) : We now capture the ast release date and put into the INSTALL script via the -D option.
&term	10 Oct 2007 (sd) : Added -r option, and fixed manual page.
&term	03 Mar 2007 (sd) : Added build node to the info in the install script.
&term	16 Mar 2009 (sd) : Made changes to allow for build on Sun 5.10.
&endterms

&scd_end
------------------------------------------------------------------ */
