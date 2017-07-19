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
# Mnemonic:	sm_mkpkg - make package
# Abstract:	This accepts a directory name and creates a package in that 
#		directory.  It assumes that the directory is a top level 
#		component (potoroo...) and that global.mk uses GLMK_PKG_ROOT
#		to identify the target directory for nmake install commands.
#		
# Date:		03 December 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
# cannot assume these exist on this node -- could be building on a new node
hold_src=$NG_SRC				# likely a bogus value in the cartulary
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
if [[ -r $CARTULARY ]]
then
	. $CARTULARY
	. $STDFUN
	NG_SRC=$hold_src
else				# some basic functions if running on a non-potoroo box
	argv0=$0

	function verbose
	{
		if [[ $verbose -gt 0 ]]
		then
			echo "$@"
		fi
	}

	function log_msg
	{
		echo "$@"
	}

	function cleanup 
	{
		rm -f /tmp/*.$$ $TMP/*.$$
		exit ${1:-1}
	}

	function usage
	{
		echo "usage: $argv0 $ustring"
		exit 1
	}
fi

function make_dir
{
	while [[ -n $1 ]]
	do
		if [[ ! -d $1 ]]
		then
			if ! $forreal mkdir $1 
			then
				log_msg "cannot make directory: $1"
				cleanup 2
			fi


			verbose "created $1"
		fi

		shift
	done
}

#---------------- bloody suns require us to modifiy the path even more.
case `uname -s` in 
	FreeBSD)	;;
	Darwin)		;;
	Linux)		;;
	Sun*)
			if [[ -d /opt/SUNWspro/bin ]]    # bloody sun os -- these are all to support sun; erk!
			then
        			PATH="$PATH:/opt/SUNWspro/bin"
			fi
			
			if [[ -d /usr/ucb ]]
			then
        			PATH="$PATH:/usr/ucb"
			fi
			
			if [[ -d /usr/ccs/bin ]]                # bloody sun os
			then
        			PATH="$PATH:/usr/ccs/bin"
			fi

			if ! whence gcc >/dev/null 2>&1
			then
				verbose "cannot find gcc, will try with cc"
				export GLMK_CC=cc
			fi
			;;

	*)		;;
esac


# -------------------------------------------------------------------
ustring="[-k] [-n] [-p] [-t type] [pkgname:]directorypath"

#my_uname=`whence uname`
my_uname=ng_uname
pkgtype="rel"                   # package type (eventually default to beta when refsrc is used to generate released pkgs)
forreal=""
purgelog=0
keepthings=0
TMP=${TMP:-/tmp}		# MUST do this if we are mking packages on a non-ningaui node!

while [[ $1 = -* ]]
do
	case $1 in 
		-k)	keepthings=1;;				# keeps the results of the make install after taring
		-n)	forreal="echo would:";;
		-p)	purgelog=1;;
		-t)	pkgtype="$2"; shift;;			# allows for beta or somesuch

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

idir=`pwd`				# invocation directory -- where we will create the package and log
pdir=$TMP/mkpkg.$$
export USER=${USER:-$LOGNAME}			 # bloody suns dont have user
log=$idir/$USER.log.${1##*/}
if [[ $purgelog -gt 0 ]]
then
	>$log
fi

date "+%Y%m%d"|read date			# cannot assume ng_ things are installed on this node
ng_uname --arch | read systype			# consistant on all platforms

while [[ -n $1 ]]
do
	cd $idir				# must be back where we started 

	$forreal rm -fr $pdir			# must ensure it is gone
	make_dir $pdir $pdir/bin $pdir/lib $pdir/lib/pz 		# needed things for install

	component=${1%%:*}			# if potoroo_nawab:potoroo/nawab supplied as name:directory	
	component=${component##*/}			# allow them to give a full path; add sub-pkg name if supplied
	cdir=${1##*:}

	if ! cd ${cdir:-no_such_thing}
	then
		log_msg "cannot switch to: ${cdir:-directory name not supplied on command line}"
		rm -fr $pdir
		cleanup 1
	fi


	if [[ -f ./INSTALL ]]			# special install script to add to package
	then
		verbose "found INSTALL, adding it to the package"
		cp ./INSTALL $pdir		# makefiles will not install this 
	fi

	pname="pkg-$component-$systype-$date-$pkgtype.tgz"
	$forreal rm -f $pname					# dont leave an old one lurking if we should fail


	export GLMK_PKG_ROOT="$pdir"			# causes global.mk to install into our pkg directory rather than into /ningaui/bin
	export GLMK_VERBOSE=1				# make global.mk chatty

	verbose "building package: $pname"
	#if ! $forreal nmake install >>$log 2>&1
	if ! $forreal ng_make install >>$log 2>&1
	then
		log_msg "package creation nmake failed for: $1 (see $log)"
		log_msg "package install target saved in $TMP/$USER.$component.pkg"
		$forreal mv -f $pdir $TMP/$USER.$component.pkg
		cleanup 1
	fi

	verbose "compressing into $idir/$pname"
	cd $pdir
	if [[ -z $forreal ]]
	then
		set -o pipefail				# this is safe as we have only two in our pipe
		if ! tar -cf - . |gzip  >$idir/$pname 
		then
			log_msg "tar/gzip failed (see $log)"
			$forreal mv -f $pdir $TMP/$USER.$component.pkg
			log_msg "package install target saved in $TMP/$USER.$component.pkg"
			cleanup 1
		fi
	else
		echo "would tar -cf - . |gzip  >$idir/$pname "
	fi

	if [[ $keepthings -gt 0 ]]
	then
		$forreal rm -fr $TMP/$USER.$component.pkg		# must trash or mv puts the dir inside rather than renaming
		$forreal mv -f $pdir $TMP/$USER.$component.pkg
	else
		cd $idir			# if we were in pdir, irix will not let us remove it
		rm -fr $pdir
	fi

	shift
done


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_mkpkg:Make a package)

&space
&synop	ng_sm_mkpkg [-k] [-n] [-p] [-t type] [pkgname:]directorypath [pkgname:dir...]

&space
&desc	&This will switch to the directory supplied on the command line and make the package
	using the mkfile present in the directory.  It is assumed that the mkfile in &ital(dir) includes
	&lit(panoptic.mk) and uses the shell variable &lit(GLMK_PKG_ROOT) to define the directory
	where the command &lit(mk install) will install files.  The assumption is that all necessary
	'building' in the directory has been finished, or the &lit(install) rule in the mkfile 
	has the necessary dependancies, as this script does not take into account any precompile/build steps.
&space
&subcat Install Script
	If the package needs a special installation script, it must exist in a file called 
	&lit(INSTALL) in the &ital(directorypath) directory.  This file will be added to 
	the package at the top level of the &lit(tar) file and will be executed by the package 
	installer process.  The install script should expect to be passed one command line parameter:
	the fully quallified pathname of the package &lit(tar) file.  The installation script 
	can expect that &lit(NG_ROOT) pinkpages variable has been exported to the environment.
&space
	Log messages written by &lit(nmake) are collected in a file in the current working 
	directory. By default the log file is appended to, and may be automatically purged by 
	supplying the -p option on the command line. 
	
&space
	Packages are created in the current working directory as gzipped tar files. They 
	are named using the syntax:
&space
	&lit(pkg-)&ital(component-system-date-pkgtype.)&lit(tgz)
&space
&space
	If &lit(pkgname) is supplied on the command line, it is used in the package name in place
	of &ital(component). If the name is not supplied, &ital(component) is derrived using the
	last (only) component in the directory path.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-k : Keep the staging directory used to create the tar file.
&space
&term	-n : No execution mode; just says what it believes it would do.
&space
&term	-p : Purge the log file.
&space
&term	-t type : Supplies the package type. If not specified on the command line, a type of 
	&lit(rel) (release) is used.   Various installation programmes may key on the package 
	type when executing automatic package loading.
&space
&temm	-v : verbose mode.
&endterms


&space
&parms	&This requires that the directory name of a component (e.g. potoroo) be supplied 
	as a command line parameter. It may optionally be prefixed with an alternate name
	if separated from the directory path with a colon (:).  The directory path may 
	be relative or absolute (NG_SRC is not assumed by &this). If multiple targets are 
	supplied on the command line, their packages are created in the order specified.

&space
&exit	A non-zero return code indicates that there was a failure.   A log is written to 
	the current working directory using the user ID in the name.

&space
&see	ng_sm_fetch, ng_sm_build, ng_construct.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 Dec 2004 (sd) : Fresh from the oven.
&term	08 Dec 2004 (sd) : Updated the man page. 
&term	07 Jan 2005 (sd) : Added path extensions to support bloody awful sun environment.
&term	09 Feb 2005 (sd) : Added support for INSTALL script.
&term	03 Apr 2005 (sd) : Now cleans up the scratch directory.
&term	22 Jul 2005 (sd) : Converted to ng_make.
&term	29 Nov 2005 (sd) : Now force the usage of uname via full path because of ksh or sh builtin that gets -p wrong.
&term	06 Dec 2005 (sd) : Ensure we are not in the tmp directory when we try to delete it; irix does not like it.
&term	18 Feb 2005 (sd) : Converted to use --arch option of ng_uname; needed for windows port. 
&term	05 Sep 2006 (sd) : Fixed default of TMP not to assume /ningaui.
&term	29 Sep 2006 (sd) : Fixed man page.
&term	10 Oct 2006 (sd) : We have deprecated the adm directory, so we no longer create it.
&endterms

&scd_end
------------------------------------------------------------------ */
