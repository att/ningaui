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



# Mnemonic: 	mkenv
# Abstract:	creates a build environment consisting of the upper level directories (potoroo, feeds...)
# 		that are supplied on the command line, and then all underlying directories based on 
# 		the matching directory in $NG_SRC. In addition, a pkg directory is created that contains
# 		bin, lib, and other directories that will be the target of installs when creating a 
# 		package. Subdirs are created only if they have a Makefile (otherwise they are not considered
#		needed by the build environment. In addition, directories that begin with a capitalised letter, 
#		or have a dot character embedded in the name are NOT created.   If a directory contains a
#		.prep_env script then it is run to setup the current directory.  The prep script is assumed
#		to do all the needed directory setup INCLUDING the creation of any needed sub directories.
#		If this script should create subdirectories per usual following the execution of the prep
#		script, then the prep script must exit with a code of 99.  Any other non-zero exit code is
#		assumed a failure of the prep script and causes this script to halt. 
#
# Date:		January 2004
# Author:	E. Scott Daniels
# Mod:		11 Nov 2004 :  changes to support new apps/applname/subdir  scheme 
#		25 Mov 2004 : revampped to allow -S and to do some cleanup.
# -------------------------------------------------------------------------------------------------------


if [[ ! -d ${NG_ROOT:-nothing/bin} ]]
then
	echo "NG_ROOT/bin does not exist and must. NG_ROOT=$NG_ROOT"
	exit
fi

if [[ -f $NG_ROOT/lib/stdfun ]]
then
	. $NG_ROOT/lib/stdfun
else
	echo "WARNING: stdfun did not exist in $NG_ROOT/lib; used $NG_SRC/potoroo/lib/stdfun.ksh!"
	. $NG_SRC/potoroo/lib/stdfun.ksh
fi


# input is name of component (potoroo, feeds...)  Directory does not have to exist
# assumed to be in the build directory

# create links to Makefile and data directories
#input is directory path and filename
function link2
{
        if [[ -e $1/$2 ]]
        then
                case ${1##*/} in
                        *.* | [A-Z]*) echo "capitalised or dotted name was skipped: $1";;
                        *)
				if [[ ! -d ${1##*/} ]]
				then
                                	echo "created $1"
                                	newdir ${1##*/}
				fi

				rm -f ${1##*/}/$2
                                echo "created lnk: $1/$2"
                                ln -s $1/$2 ${1##*/}/$2
                                ;;
                esac
	#else
	#	echo "no directory: $1/$2 (this is ok)"
        fi
}


function newdir
{
        typeset p=""

        while [[ -n $1 ]]
        do
                if [[ ! -d $1 ]]
                then
                        p=${1%/*}
                        if [[ $p != $1 && ! -d $p ]]
                        then
                                newdir $p
                        fi
                        if ! mkdir $1
                        then
                                echo "unable to create directory: $1"
                                exit 1
                        fi

                        echo "created dir: $1  (pwd=`pwd`)"
                fi

                shift
        done
}


function build_all
{
	ls $NG_SRC | while read d
	do
		if [[ -d $NG_SRC/$d ]]
		then
			case $d in 
				\~*)		;;
				[A-Z]*)		;;	
				*.*)		;;		# ignore thing.new and the like
				ast)		;;
				*access*) 	;;
				abm_tables)	;;
				aliens)		echo "SKIPPED ALIENS for now";;
				include)	newdir include;;
				stlib)		newdir stlib;;
				*)	
					if [[ ! -e $d ]]
					then
						newdir $d
					fi

					if ! $argv0 $flags $d
					then
						return 1
					fi
			esac
		fi
	done

	return 0
}

#	drop into dir ($1) and do it.  we expect that $1 is a relatvie path of sorts (foo/bar/boff)
#	and that the pwd is the next to the last component (bar in the example). 
#	If .prep_env exists, we run it expecting it to setup the directories in 'boff' and anything 
#	below. When a prep_env script exists we assume it does all needed directory creation and makefile
#	linking unless it exits with a return code of 99.  If the exit code is 99, or if it does not exist, 
#	we create all directories that exist in the source tree that also have Makefiles.
#
function do_dir
{
	(
		src_path="$NG_SRC/$1"
		rel_path="${1##*/}"		# we get passed an expanding path as we are recursively called; must trunc

		echo "establish: $1; rel=$rel_path  src=$src_path"
		newdir $rel_path

		if ! cd $rel_path
		then
			echo "ABORT: cannot cd to $rel_path ($1)"
			exit 1
		fi

		
		if [[ -f $src_path/Makefile ]]
		then
			echo "created link: $src_path/Makefile <- ${mf_prefix}Makefile "
			ln -s $src_path/Makefile ${mf_prefix}Makefile 
		fi
		
		if [[ -e $src_path/.prep_env.ksh ]]			# specific setup script to augment the standard stuff
		then
			echo "running $src_path/.prep_env"
			ksh $src_path/.prep_env.ksh			
			perc=$?
			if [[ $perc -gt 0 && $perc != 99 ]]		# a return of 99 is good, and indicates we are to create directories
			then
				echo "$src_path/.prep_env.ksh failed"
				exit 1
			fi
			echo "finished: $src_path/.prep_env"
		else
			perc=99					# if prepenv returns 99, then we create directories, otherwise we assume it did
		fi
		
		if [[ $perc -eq 99 ]]		
		then
			for sd in $src_path/*
			do
				case ${sd##*/} in 
					[A-Z]*)	;; #echo "skipping $sd";;
					*.*)	;; #echo "skipping $sd";;
					*)
						if [[ -d $sd && -f $sd/Makefile ]]		# a directory with a Makefile
						then
							echo "decending into $1/${sd##*/}"
							do_dir $1/${sd##*/}				# recurse giving it full path
						fi
						;;
				esac
			done
		fi
	)
}

# set up to use reference oriented things
function ref_setup
{
	if [[ ${rs_done:-0} -le 0 ]]		# if -S/-s and path is contains stable we want only one execution of this
	then
		toold=$refd/.bin		 # stable build environment; does not use makefile links
		NG_SRC=$refd  
		mf_prefix=$prefix		# makefiles are created for easier -f building, but not as Makefile
	fi

	rs_done=1
}

function usage
{
	cat <<endKat
usage: ${argv0##*/} [-d build-dir] [-M[x]] [-s|-S path] [component [component...]]

	This script will build the directory structure to mirror a reference tree
	By default, the reference tree is NG_SRC, but can be overridden with -S path.
	Directories in the build tree are created if a directory in the reference tree
	has a Makefile. If the script .prep_env exists, it is executed and assumed to set
	all underlying directories up. If the script exits with a return code of 99, then
	this script will also create subdirectories; any other exit code causes this script
	to skip the directory creation step.

	If the build directory path (. by default) has a component of /stable/, then -s 
	is assumed

	If no component is supplied, then all directories in the reference directory tree
	are duplicated.

	-d path - uses path as the build directory

	-s 	- use ~ningaui/refsrc as the reference.

	-S path - uses path as the reference directory rather than NG_SRC.

	-M creates a Makefile symlink in each directory. This is needed to avoid problems 
		because the development tree is used for compilation. 
endKat
}


# ------------------------------------------------------------------------------------
buildd=.			# assume current directory is build directory (-d overriedes)

force_mf=""			# by default makefiles are not created -M[prefix] will force them to be created
force_d="data"			# on by default; needed if -M is set
force_c="copybook"
refd=~ningaui/refsrc		# reference source. If it exists, then we create xMakefile rather than Makefile so that we can vpath to it
toold=$NG_SRC			# source/construct tools 
prefix="x"			# we create makefile links as xMakefile unless -M is indicated
argv0="$0"

while [[ $1 = -* ]]
do
	case $1 in 
		-d)	buildd=$2; shift ;;	# we will cd here first; do not pass in flags
		-M*)	flags="$1$flags " 
			prefix="${!#-M}" 
			force_mf="Makefile"	# will force the creation of a Makfile link even if ref src exists
			;;

		-s)	flags="-s$flags "; ref_setup;;		# defautl to stable source 

		-S)	refd="$2"; ref_setup; shift;;	# stable setup, but against an alternate directory (do NOT pass in flags)

		-v)	verbose=1;;

		--man)	usage
			exit 1
			;;

		*)	usage
			exit 1
			;;
	esac
	
	shift
done

export NG_SRC		# should be, but lets make sure

if [[ -n $force_mf ]]
then
	force_mf="${prefix}$force_mf"		# must add prefix if we are forcing the makefile
fi

if ! cd $buildd
then
	echo "error switching to build directory: $buildd"
	exit 1
fi

buildd=`pwd`				# convert to absolute path for use later
p=`pwd`
if [[ ${p##*/} != "build" ]]
then
	echo "build directory must have build as final node: $p does not!"
	exit 1
fi

if [[ $p = */stable/* ]]		# we assume that this should be treated as if -s was set
then
	ref_setup
fi


dlinks="$force_mf $force_d $force_c"		# what we need to make links to

if [[ -z $1 ]]			# nothing supplied, do everything
then
	build_all		# create an environment based on every directory in ng_src
	exit $?
fi

if [[ ${d##*/} !=  ${1:-foo} ]]			# at the build level
then
	newdir stlib
	newdir include 2>/dev/null
	newdir include/copybooks 2>/dev/null

	if [[ ! -e construct.ksh ]]				# link the build helper scripts for ease of use
	then
		ln -s  $toold/construct.ksh construct.ksh
		ln -s  $toold/construct2.ksh construct2.ksh
	fi
	if [[ ! -e mkenv.ksh ]]
	then
		ln -s  $toold/mkenv.ksh mkenv.ksh
		ln -s  $toold/mkenv2.ksh mkenv2.ksh
	fi
	if [[ ! -e publish.ksh ]]
	then
		ln -s  $toold/publish.ksh publish.ksh
	fi
	if [[ ! -e shove_pkg.ksh ]]
	then
		ln -s  $toold/shove_pkg.ksh shove_pkg.ksh		# replaces publish
	fi
fi


while [[ -n $1 ]]			# for each package (potoroo, feeds, apps)
do
	(
		if ! cd  $buildd/$1				# create and populate the component directory
		then
			newdir $buildd/$1
		
			if ! cd $buildd/$1
			then
				echo "cannot cd to $1"
				exit
			fi
		fi
	
		case $1 in 
			ast)	;;						

			feeds)	newdir pkg/lib/pz				# special pzip pin directory in the package install area
				newdir pkg/bin pkg/lib pkg/classes pkg/adm   #pkg/site pkg/site/www/public_html/cgi-bin pkg/classes pkg/adm
				;;

			*)	 newdir pkg/bin pkg/lib pkg/classes pkg/adm   
				;;
		esac
	)

	do_dir $1

	if [[ $? -ne 0 ]]
	then
		echo "abort after attempting to initialise $1"
		exit 1
	fi

	shift
done			
