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
# Mnemonic:	auto_build
# Abstract:	builds everthing that I want to build on a weekly basis; run from rota
#		
# Date:		07 January 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

unset CDPATH		# bloody CDPATH from woomera was hosing things

function switch
{
	if ! cd $1
	then
		echo "cannot switch to $1"
		cleanup 1
	fi
}

function send_mail
{
	for u in $mail_user
	do
		ng_mailer -T ${u}@${NG_DOMAIN:-no.ng_domain} -s "$where: $1" ${2:-/dev/null}
	done
}

function chk4errs
{
	if [[ $1 -gt 0 ]] || gre -c "FAILED" $2 >/dev/null 
	then
		log_msg "errors found during build of: $3"
		return 1
	fi

	return 0
}

# turn the pkg install process off/on during build and publication
function set_installer
{
	if (( ${have_inst_orig:-0} == 0 ))
	then
		ng_ppget -l NG_PKGINSTALL| read inst_orig		# get the local copy which is likely blank
		have_inst_orig=1
	fi

	case $1 in 
		on)	ng_ppset -c "set to original value ($inst_orig) by autobuild" -l NG_PKGINSTALL $inst_orig;;	# reset to original value
		off)	ng_ppset -c "off by autobuild (was: $inst_orig)" -l NG_PKGINSTALL=off;;
		*)	log_msg "bad value ($1) to set_installer function"
			cleanup 2
			;;
	esac
}

# -------------------------------------------------------------------
AUTOBLD_TARGETS="${AUTOBLD_TARGETS:-contrib potoroo cuscus feeds}"		# allow env override; sensiable default (-t overrieds this)

fresh=""
export_opt="-e"
keep_dir=0

date="`ng_dt -d`"
date=${date%??????}
date="$date 03:00"		# today at 3am by default
USER=${USER:-$LOGNAME}		# bloody suns dont set one of these
alt_user=""			# set by -u to override USER on some things
mail_user=
pubpkg=1			# by default publish packages too
tree_root=ningaui		# default root of the cvs tree
use_current_time=0
save_libs=0
pre_build=precompile		# what we must mk first (some apps do not need precompile and set this to nuke -- faster)
build_ast=0			# --ast sets this and we just build the ast package
push_pkg="-p"			# option to sm_publish to push the pkg to all clusters; -P turns it off

export NG_CVSHOST=${NG_CVSHOST:-cvs.$NG_DOMAIN}	# try to be sure that this is set

ustring="[--ast source-dir] [-c] [-d date] [-m mail_user] [-N] [-n] [-p prebuild-target] [-P] [-s]  [-T tree-root] [-t target] [-u username]"


while [[ $1 = -* ]]
do
	case $1 in 
		--ast)	build_ast=1; source_dir=$2; shift;;
		-c)	use_current_time=1;;
		-d)	date="$2"; shift;;
		-l)	save_libs=1;;
		-m)	mail_user="$mail_user$2 "; shift;;
		-N)	keep_dir=1; export_opt="-N";;			# prevent reexport of code
		-n)	pubpkg=0;;					# make but dont publish packages
		-p)	pre_build="${2//,/ }"; shift;;			# allow a comma separated list 
		-P)	push_pkg="";;					# register package, but do not push
		-s)	pre_build=nuke;;				# skip precompile (some apps dont need )
		-t)	target="$target$2 "; shift;;
		-T)	tree_root$2; shift;;
		-u)	alt_user=$2; shift;;

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if (( $use_current_time > 0 ))
then
	ng_dt -d|awk '{ printf( "%s %s:%s\n", substr( $1, 1, 8 ), substr( $1, 9, 2 ), substr( $1, 11, 2 ) )}'|read date
else
	export SM_FINGERPRINT_SAVE=1		# if building from 3am src, then allow fingerprints to save
fi

where=`ng_sysname`

TMP=${TMP:-$NG_ROOT/tmp}

verbose "setting up build environment"

udir="$TMP/${alt_user:-$USER}"
mkdir  $udir 2>/dev/null
switch $udir
mkdir build 2>/dev/null
switch build
unset NG_SRC
buildd=`pwd`


if (( $build_ast > 0 ))			# just do ast and exit 
then
	verbose "building ast package in $PWD"

	if ng_sm_mkpkg_ast -v -s $source_dir >$TMP/autobld.err.$$ 2>&1
	then
		log_msg "ast package created ok [OK]"
	else
		msg="error(s) creating ast package [FAIL]"
		log_msg "$msg"
		send_mail "$msg" $TMP/autobld.err.$$
		cleanup 1
	fi

	if (( $pubpkg > 0 ))
	then
		verbose "publishing ast package"
		if ng_sm_publish $push_pkg $vflag pkg-ast-* >>$TMP/autobld.err.$$ 2>&1
		then
			cat $TMP/autobld.err.$$ >&2
			msg="build ok; ast package published [OK]"
		else
			msg="build ok; error publishing ast package [FAIL]"
			log_msg "$msg"
			send_mail "$msg" $TMP/autobld.err.$$
			cleanup 1
		fi
	else
		msg="ast package build ok; not published, -n option supplied [OK]"
	fi

	cat $TMP/autobld.err.$$ >&2
	log_msg "$msg"
	send_mail "$msg" $TMP/autobld.err.$$

	cleanup 0
fi

if [[ $keep_dir -lt 1 ]]
then
	if [[ -d $tree_root ]]
	then
		rm -fr ${tree_root}-
		mv  $tree_root  ${tree_root}-
	fi
	
	if ! mkdir $tree_root
	then
		echo "could not make $tree_root directory in `pwd`"
		send_mail "mkdir of $tree_root failed" /dev/null
		cleanup 1
	fi
fi

verbose "prebuilding with: $pre_build"
ng_sm_build -D $buildd -d "$date" $vflag $export_opt $pre_build >/tmp/out.$$ 2>&1		
if ! chk4errs $? /tmp/out.$$ "prebuild: $pre_build"
then
	send_mail "pre-build ($pre_build) failed" /tmp/out.$$
	cleanup 1
fi


comp_errs=0				# number pkgs not generated because of errors 
for t in ${target:-$AUTOBLD_TARGETS}
do
	if [[ $t = *:* ]]
	then
		bt=$t			# assume user passed in apps/platypus:precompile or something specific
	else
		bt=$t:all
	fi
	verbose "building: $bt"
	ng_sm_build -D $buildd -N $vflag $bt >/tmp/out.$$ 2>&1		# build each target skipping the export
	if chk4errs $? /tmp/out.$$ $t
	then
		verbose "$t build success; adding to pkg target list"
		if [[ $ptarget != *${t%:*}* ]]
		then
			ptarget="$ptarget${t%:*} "		# only unique ones
		fi
	else
		log_msg "build errors for $t; no package will be generated"
		comp_errs=$(( $comp_errs + 1 ))
	fi

	cat /tmp/out.$$ >>/tmp/all.out.$$
	echo "----------------------" >>/tmp/all.out.$$
	cat /tmp/out.$$ >&2
done

if [[ -z $ptarget ]]
then
	log_msg "all builds failed?? -- no package targets in list  [FAILED]"
	send_mail "build failed" /tmp/all.out.$$
	cleanup 1
fi

set_installer off			# disable pkg_inst until we are done

verbose "making packages for: $ptarget"
switch $tree_root
export NG_SRC=`pwd`
if ! ng_sm_mkpkg $vflag $ptarget >/tmp/pout.$$ 2>&1
then
	log_msg "pkg make failed for target(s): $ptarget [FAILED]"
	send_mail "make package failed for: $ptarget" /tmp/pout.$$
	set_installer on
	cleanup 1
fi

cat /tmp/pout.$$ >>/tmp/all.out.$$

if (( ${save_libs:-0} > 0 ))
then
	verbose "installing libs and header files into $NG_ROOT/lib and $NG_ROOT/include"
	mkdir $NG_ROOT/include 2>/dev/null
	(
		cd $NG_SRC/stlib
		for x in *.a
		do
			cp $x $NG_ROOT/lib
			ranlib $NG_ROOT/lib/$x
		done
		cd $NG_SRC/include
		tar -cf - . | (cd $NG_ROOT/include; tar -xf -)	# copybooks is a directory!
		#cp * $NG_ROOT/include
	)
fi

if [[ $pubpkg -lt 1 ]]
then
	verbose "not publishing packages, -n specified"
	if (( $comp_errs > 0 ))
	then
		msg="$comp_errs compile errors; packages not published (-n) [WARN]"
	else
		msg="build successful; packages not published (-n) [OK]"
	fi
	log_msg "$msg"
	send_mail "$msg" /tmp/all.out.$$
	set_installer on
	cleanup 0
fi


verbose "publishing packages: $ptarget"
if ng_sm_publish $push_pkg $vflag pkg*
then
	if (( $comp_errs > 0 ))
	then
		msg="$comp_errs compile errs; some packages not gen/published [WARN]"
	else
		msg="build ok; packages published: $ptarget [OK]"
	fi

	log_msg "$msg"
	send_mail "$msg" /tmp/all.out.$$
else
	if (( $comp_errs > 0 ))
	then
		msg="$comp_errs compilation errs; pkg/publish errors [FAILED]"
	else
		msg="build ok; error publishing one/more packages: $ptarget [FAILED]"
	fi
	
	log_msg "$msg"
	send_mail "$msg" /tmp/all.out.$$
	set_installer on
	cleanup 1
fi


set_installer on
cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_autobuild:Automatic Extract and Build)

&space
&synop	ng_sm_autobuild [--ast src-dir | -c | -d date] [-m user] [-N] [-n] [-p prebild-target] [-P] [-t target] [-T tree_root] [-u username]

&space
&desc	&This extracts a copy of the ningaui source from the CVS repository (ng_sm_fetch)
	into a temporary directory ($TMP/username/build) and compiles the indicated
	targets.  The precompile target is always assumed, and if no targets are 
	supplied on the command line, then AUTOBLD_TARGETS is used as a default list.
	For each target that is successfully compiled, its corresponding package is 
	created and published on the local cluster. 
&space
	Upon termination of the build, the directory structure is left intact. If the 
	command is executed a second time, the directory (ningaui) is moved to an 
	alternate name, and a new directory is created and used as the target for the 
	export (unless the -N option is given).
&space
	When run with the &lit(--ast) option, an ast package is created and optionally
	published.  In this case, no ningaui source is exported. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	--ast dir : Build an ast package using &ital(dir) as the path of the arch 
	directory (e.g. ~gsf/arch).  When building the ast package, $TMP/$USER/build i s
	created, and the pckage is built there.  Unless the -n option is supplied, the
	package is published.
&space
&term	-c : Use the current time for extraction. If not supplied, 3:00am on the current 
	day is used.  Dont use with -d option or who knows what it will do.
&space
&term	-d date : By default, the source extracted is from 3:00am on the current day. This
	allows the extract point to be changed.  Date should be of the form: yyyymmdd hh:mm
	and must be enclosed in quotes.  Dont use with -c option.
&space
&term	-l : Copies libraries and header files from the build environment into $NG_ROOT/lib
	and $NG_ROOT/include respectively.  This option is intended to be used only when 
	&this is invoked for regularly scheduled builds.
&space
&term	-m user : User name(s) to send mail to upon completion of the script. Multiple user
	names are allowed if separated with blanks and quoted. Multiple &lit(-m) options 
	may also be supplied. If omitted, mail is not sent.
&space
&term	-N : No extract mode. The assumption is that $TMP/username/build/ningaui contains 
	a copy of the source to build. Prevents the ningaui directory from being moved away
	to ningaui-.
&space
&term	-n : Do not publish packagges.	If not set, packages are published to all clusters
	which have nodes with the same operating sytem type as that running the script. 
&space
&term	-p target : Defines the prebuild target to use instead of &lit(precompile). If not
	defined, the &lit(precompile) target is made at the &ital(tree_root) level before
	the indicated target(s) is/are built. The target may be a series of comma separated
	targets if more than one target must be made as a prebuild step. Targets are made
	in the order listed. 
&space
&term	-P : Do not push packages. Package(s) created are registered with the local 
	replication manager, but are NOT pushed to other clusters. 
&space
&term	-s : Skip precomile.  Some targets (like apps/sam) do not need to have precompile 
	run and this makes the whole process much faster.  If link errors occur with this 
	option, it is a good bet that what is targeted DOES need to have the precompile 
	step(s) taken. Supplying this option is the same as using &lit(-p nuke).
&space
&term	-t target : supplies one or more, space separated target names (e.g. potoroo, ebub).
	If multiple targets are supplied, then the list must be quoted.
&space
&term	-T tree_root : By default &lit(ningaui) is assumed as the default root of the source 
	tree.  This allows that to be overridden. If this is supplied, the exported 
	soruce tree must support these &lit(mk) targets: precompile, all, install
	and any other make dependancies that &lit(ng_sm_build) expects. 
&space
&term	-u user : Runs the command as if the LOGNAME were set to user. This allows the user's
	directory to be used even if the command is submitted from woomera which normally would 
	execute as ningaui.  In cases such as this, the ningaui user id must have write permission
	to the build directory. This may not always work properly as CVS commands can fail 
	if the real user name (e.g. ningaui if the job is run from woomera) may not have 
	access to the source tree.  If this is the case, then the source must be exported
	manually, and the -N option supplied on the command line.
	
&endterms


&space
&exit	A zero exite code indicates a good state.  Mail is sent to the user indicating the final 
	status.  Log files are left in the build and build/ningaui directories. 

&space
&see	ng_sm_fetch, ng_sm_mkpkg, ng_sm_build, ng_sm_publish, ng_pkg_inst, ng_pkg_list 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Jan 2005 (sd) : Fresh from the oven.
&term	08 Jan 2005 (sd) : Added -n option.
&term	14 Jan 2005 (sd) : Now sends mail when not building pkgs and build is ok.
&term	17 Jan 2005 (sd) : Support for multiple users on -m, or multiple -m options. 
		Cleaned up log and verbose messages.
&term	22 Jan 2005 (sd) : Force the pkg_inst flag off during pkg build and install.
		keeps us from restarting the node until after we have built all packages. 
&term	17 Feb 2005 (sd) : Better error detection and messages.
&term	26 Feb 2005 (sd) : Unset CDPATH as its seting from woomera was hosing things.
&term	11 Mar 2005 (sd) : Added -c option to use the current time. 
&term	22 Mar 2005 (sd) : Added -l option to copy libraries and headers to $NG_ROOT.
&term	28 Mar 2005 (sd) : Corrected manpage and usage to include -c option.
&term	26 Apr 2005 (sd) : Enhanced such that -u will work even if user is not a legit id.
		Must be executed by a legit id in order to extract code from CVS.
&term	01 Jun 2005 (sd) : Default fetch date/time is current day at 3am (mostly for autobuild).
&term	03 Jul 2005 (sd) : Changed the way include is pushed to /ningaui/include.
&term	23 Aug 2005 (asm) : Fixed typo.
&term	08 Feb 2006 (sd) : Put in support to auto build ast package. 
&term	07 Mar 2006 (sd) : Added -s -- skip precompile.
&term	13 Apr 2006 (sd) : Added -p and -P options.
&term	27 Apr 2006 (sd) : Now allows a comma separated list for -p parameter as passing a quoted list 
		via rota/woomera was failing -- too many levels of shell stripping too many escaped 
		quotes to make it simple. 
&term	30 Mar 2007 (sd) : Removed hard coding of domain name. 
&term	09 Jun 2007 (sd) : Corrected man page.
&term	19 Jun 2007 (sd) : Now tries its best to ensure that NG_CVSHOST is set.
&term	04 Apr 2007 (sd) : Added contrib to the default list of packages to build. 
&endterms

&scd_end
------------------------------------------------------------------ */
