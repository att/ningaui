# do NOT place a #! line here as the standard ningaui one will probably not work on a virgin node. 
# as a result, it might be necessary to fully qualify the ksh path when executing this command.
#
#
# Mnemonic:	bootstrap_build -- allow us to build on a non-cluster node
# Abstract:	This should set things up enough on a new node to build our source
#		This is a chicken and egg script. You must have it to start, but 
#		once you have it it will check out the sourece from cvs and get you
#		started. Run with 'ksh bootstrap_build.ksh'  and when it is done 
#		it will print the steps needed to build on a virgin node.  If you 
#		need to see the steps again, run with -h or -? options.
# options:	
#		-a		-- do everything (including make)
#		-c host		-- host where cvs repository lives (if fetching)
#		-d directory	-- the directory  in cvs tree (if fetching source)
#		-D directory	-- the directory that the source is in ($TMP/$USER/dev)
#		-h		-- show help info
#		-i directory    -- install only; pkg files in current directory are installed into dir listed
#		-k		-- keep tmp files and such after build
#		-n		-- do not fetch source from cvs (fetch if script thinks necessary)
#		-N		-- nuke first
#		-p		-- generate packages (implies -a)
#		-R directory	-- directory to use as temporary NG_ROOT
#		-t direcotry	-- Directory to use as $TMP
#		
#		
# Date:		30 Dec 2005
# Author:	E. Scott Daniels
# Mod:		16 Jan 2006 (sd) : Added setup_common call
#		30 May 2006 (sd) : Added /usr/common/ast/bin to end of path.  Needed for 
#			ksh on a virgin node now that we reference NG_ROOT/common/bin for 
#			things. 
#		26 Nov 2007 (sd) : Added ability to create a mk Makefile helper file if it does 
#			not exist. 
# ---------------------------------------------------------------------------------

# install pkg files that are here; we unload them into the directory that was given on 
# the -i option. we DO NOT try to create this directory and fail if it is missing
function do_install
{
	if [[ ! -d ${install_into:-nosuchthing} ]]
	then
		log_msg "unable to install: cannot find directory: ${install_into:-dir name not supplied}"
		cleanup 1
	fi

	for x in pkg-*
	do
		set -o pipefail
		gzip -dc $x | (set -e; cd $install_into; tar -xf -)
		if (( $? > 0 ))
		then
			log_msg "install failed trying to unload $x [FAIL]"
			cleanup 1
		fi
		log_msg "installed: $x  [OK]"
	done

	cleanup 0
}


# the machine arch and processor types seem to be so inconsistant between platforms
# and even between */bin/uname flavours and the ksh builtin that we need something 
# that will deliver the same thing regardless of the platform.
function get_arch
{
	os_type=$(uname -s)		
	case $os_type in 
		Darwin)	parm=-p;;
		*)	parm=-m;;
	esac

	${uname_cmd:-uname} $parm|sed 's/[ -]/_/g'
}

# the mk stuff requires makefile include files that are named using the O/S and arch type.
# if we think the one is missing for this type of hardware O/S combo, we will try to make it 
# so that the build will not fail. 
function fix_mk
{
	typeset arch=""
	typeset os=""
	typeset model=""
	typeset d=""

	arch="$( get_arch )"
	os="$( uname )"

	(
		if cd $NG_SRC/contrib/mk
		then
			if [[ ! -f Make.${os}-$arch ]]
			then
				model="$( ls Make.${os}-*|head -1 )"
				if [[ -z $model ]]
				then
					model="Make.FreeBSD-1386"	
				fi
				
				cd ..
				for d in mk libbio libfmt libregexp libutf
				do
					log_msg "creating: $d/Make.${os}-$arch"
					(
						if cd $d
						then
							cp $model Make.${os}-$arch
						fi
					)
				done
			fi
		fi
	)
}

function instruct
{
cat <<endKat
==========================================================================
Ningaui source should be set up in $d 
$r should have been created as a temporary NG_ROOT directory. 
The source can now be compiled.

You may execute ng_build.ksh -a [-p] to build and optionally
generate package files, or you may manually go through the 
steps to build:
	1) source in the file: $TMP/build_setup.$USER

	2) cd $d/ningaui/contrib

	3) execute these commands to build mk: 
		ksh build precompile; 
		ksh build all; 
		ksh build install 

	4) cd $d/ningaui

	5) execute these commands to build: 
		mk precompile; 	
		mk all; 
		mk Install
	
	6) add $r to your PATH

	7) execute these to build man pages (in ./manuals)
	   and to create index.html:
		mk manpages

	8) generate packages with these commands:
		ng_sm_mkpkg contrib
		ng_sm_mkpkg potoroo

After packages are built, you can manually install them by uncompressing
and passing the data to tar, or you can execute:
        ksh ng_build.ksh -v -i ${NG_ROOT:-NG_ROOT}

which will install the packages into the ${NG_ROOT:-NG_ROOT} directory. 

NOTE: On the mk Install command the 'I' is capitalised as this install
	will place ningaui into the bld_root directory in the current 
	directory, in preparation to generate packages, and does NOT do 
	an install on the node.

==========================================================================
endKat
exit 1
}

function cleanup 
{
	rm -f $TMP/*.$$
	exit ${1:-0}
}

function verbose
{
	if (( $verbose > 0 ))
	then
		log_msg "$@"
	fi
}

function log_msg
{
	printf "%s %s\n" "$(date)" "$@"
}

function ensured
{
	while [[ -n $1 ]]
	do
		if [[ ! -d $1 ]]
		then
			if ! mkdir -p $1 
			then
				log_msg "unable to make directory: $1"
				cleanup 1
			fi

			log_msg "created directory: $1"
		else
			verbose "directory existed: $1"
		fi

		chmod 775 $1

		shift
	done
}

# create a path string using directories passed in, but only if they exist
function build_path
{
	typeset p=""

	while [[ -n $1 ]]
	do
		if [[ -d $1 ]]
		then
			p="$p$1:"
		fi
	
		shift
	done

	echo "${p%:}"
}

function usage 
{
	ustring="[-a | -p] [-c cvs_host] [-d cvsroot] [-D local_build_dir] [-f] [-k] [-n] [-r build_root_dir] [-t tmp_dir] [-v]"
	(
	echo "usage: $argv0 $ustring" 
	cat <<endKat
	where:		(defaults, used if option not supplied, are in parens)
		-a 	build everything (set environment; build nothing)
		-p	build everything and generate packages; implies -a (does not create package files)
		-c host supply cvs host name	(value of NG_CVSHOST)
		-d dir	cvs root directory on -c host (/cvsroot)
		-D dir  dir where ningaui src tree is, or will be after fetch (see note)
		-f	force source fetch (fetch done only if script thinks it is needed)
		-i dir  Install pacakges from this directory into dir and exit (all other actions/options ignored)
		-k 	keep temp files	(temp files are removed)
		-r dir	directory used as NG_ROOT during the build (see note)
		-s path path to Kshell to use with #! in .ksh scripts (first ksh found in search of obvious places)
		-t dir	directory used as working space (/ningaui/tmp if present, otherwise /tmp)
		-v	verbose mode on
		
	For most, executing this script with the -a and -p options should be all that is needed 
	to build ningaui and create two package files (one for contrib and one for potoroo). The
	package files can then be 'installed' on like nodes in the cluster. 

	By default an attempt to fetch source is made only if the script believes
	that the source is not already available. Supplying the -c or -f options force 
	an attempt to fetch the source. To fetch source, it is assumed that the -c option 
	was used to supply the name of the machine that houses the CVS repository containing
	Ningaui source; the -d option needs to be used if the root of CVS repository is 
	not /cvsroot.  

	When not fetching the source from a CVS repository, the script assumes that the current 
	working directory ends with the final directory name of ningaui (e.g. /tmp/scooter/build/ningaui) 	
	and that all of the ningaui source exists in subdirectories to the current. Please see 
	one of the README files for more details on files, directories that are created and/or used during 
	the build processes, in addition to a broader discussion on how to build ningaui.

	TROUBLESHOOTING
	If this script dies with a shell error (probably something like 'bad substitution'), then the 
	version of Kshell that was used to execute the script is not recent enough.  Get the most recent
	Kshell version, or use the full path of the most recent version when invoking this script. At 
	the time of this writing, bash is not capable of executing this script because of its limitations.
		
endKat
	) >&2
}

# -------------------------------------------------------------------
export CVS_RSH=${CVS_RSH:-ssh}

verbose=0
check_out=1
gen_pkgs=0
keep=0
cvs_host=$NG_CVSHOST		# use -c to override
cvs_dir=/cvsroot 		# use -d to override
d=""
r=""
do_all=0
nuke_first=0

if [[ -n $USER ]]		# bloody sunos -- old shells, bad awk, uncapable vi, and no var
then
	USER=$LOGIN
fi
if [[ -n $USER ]]		# bloody sunos -- old shells, bad awk, uncapable vi, and no var
then
	log_msg "cannot figure out user name USER and LOGIN seem unset; export one and restart"
	cleanup 1
fi


if [[ -d /ningaui/tmp ]]	# yes, this MUST be /ningaui and not $NG_ROOT!
then
	TMP=/ningaui/tmp	# preferred
else
	TMP=/tmp		# will probably do
fi

if ! which make >/dev/null 2>&1
then
	log_msg "cannot find make; ensure that the directory containing make's binary is in your PATH"
	log_msg "PATH=$PATH"
	cleanup 1
fi

# we CANNOT use bash as it does not (currently) support:
#	all of the ${vname} expansions that kshell does 
# 	floating point in $(( expression ))  
#	$(cmd) | read var-name work 
# (there are probably others too)
for x in /usr/common/ast/bin/ksh /usr/local/bin/ksh /usr/bin/ksh /bin/ksh #/usr/local/bin/bash /usr/bin/bash /bin/bash 
do
	if [[ -x $x ]]
	then
		ksh_path=$x
		break;
	fi
done

if [[ ${PWD##*/} == ningaui ]]		# if we are sitting in the ningaui directory; assume pwd is where src is
then
	d=${PWD%/*}			# parent directory is the base 
	r=${PWD}/bld_root		# temp root is right here too
	if [[ -d contrib ]]
	then
		check_out=0		# we assume that source is already here if contrib exists (-f or -c will override)
	fi
fi

awk -v foo=bar 'BEGIN { print foo }' >/dev/null 2>&1
if (( $? != 0 ))
then
	log_msg "version of awk $(whence awk) is bad; must have gawk 3.1 or later, and be able to reference it as awk"
	cleanup 1
fi

if ! whence gcc >/dev/null 2>&1
then
	export GLMK_CC=cc
	log_msg "cannot find gcc; using cc instead [WARN]"
fi

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	do_all=1;;		# drive the whole show until an error 

		-c)	check_out=1; cvs_host=$2; shift;;

		-d)	cvs_dir=$2; shift;;

		-D)	d="$2"; shift;;

		-f)	check_out=1;;

		-h|-help|--help|-\?)	do_help=1;;	# dont do now as we need -t if they assign it !instruct; cleanup 1;;

		-i)	install_into=$2; shift;;

		-k)	keep=1;;

		-n)	check_out=0;;		# now the default, but kept for back compat
		-N)	nuke_first=1;;

		-p)	do_all=1; gen_pkgs=1;;

		-R)	r="$2"; shift;;

		-s)	ksh_path=$2; shift;;

		-t)	TMP=$2; shift;;

		-v)	verbose=1;;

		--man)	if whence tfm >/dev/null 2>&1
			then
				(export XFM_IMBED=$NG_ROOT/lib; tfm $0 stdout .im $XFM_IMBED/scd_start.im 2>/dev/null | ${PAGER:-more})
			else
				usage
			fi
			cleanup 1
			;;
				
		-*)	log_msg "unrecognised option: $1"
			usage
			cleanup 1
			;;
	esac

	shift
done

if [[ -z $ksh_path ]]
then
	log_msg "WARNING: no shell path for .ksh scripts!"
fi

if [[ ! -x $ksh_path ]]
then
	log_msg "WARNING: path to default shell for .ksh scripts seems bad: $ksh_path"
else
	log_msg "kshell path = $ksh_path"
fi

PATH="$PATH:${ksh_path%/ksh}"		# in case they did not read the readme and did not put it in the path

shell=${ksh##*/}			# use that shell here

if [[ -z $d || -z $r ]]			# not defined above, or on command line 
then
	d=$TMP/$USER/dev
	r=$TMP/$USER/bld_root	# a temporary root for initial install (must wait until opts processed incase -t supplied)
fi

if (( ${do_help:-0} > 0 ))
then
	usage
	if (( $verbose > 0 ))
	then
		if [[ -f $TMP/build_setup.$USER ]]
		then
			instruct			# give them instructions if theyve run this script 
		fi
	fi
	cleanup 1
fi

if [[ -n $install_into ]]
then
	do_install
	cleanup $?
fi


verbose "ensuring needed support directories exist"
ensured $d
ensured $d/ningaui
ensured $r

NG_SRC=$d/ningaui
ensured $NG_SRC/.bin  
ensured $NG_SRC/include
ensured $NG_SRC/include/copybooks
ensured $NG_SRC/stlib
ensured $r/bin




if ! cd $d
then
	log_msg "unable to swtich to: $d"
	cleanup 1
fi

# check out the source 
if (( $check_out > 0 ))
then
	verbose "fetching source from cvs"
	if ! cvs -d :ext:$USER@$cvs_host:$cvs_dir checkout ningaui >$TMP/checkout_log.$$ 2>&1
	then
		log_msg "source checkout failed. checkout log moved to $TMP/checkout_log.$USER"
		mv $TMP/checkout_log.$$ $TMP/checkout_log.$USER
		cleanup 1
	fi

	if (( $keep > 0 ))
	then
		verbose "checkout log kept in  $TMP/checkout_log.$USER"
		mv $TMP/checkout_log.$$ $TMP/checkout_log.$USER
	fi
else
	verbose "skipping fetch of code from cvs"
fi

export NG_ROOT=$r
if [[ -f $NG_SRC/potoroo/tool/setup_common.ksh ]]
then
	if ! $shell $NG_SRC/potoroo/tool/setup_common.ksh  >setup_common.out 2>&1
	then
		log_msg "could not setup $NG_ROOT/common"
	fi
fi

p="$(build_path . $NG_SRC/.bin $r/bin $r/common/ast/bin ${PATH//:/ } /usr/common/ast/bin)"
log_msg "creating a setup file that can be sourced: $TMP/build_setup.$USER"
# =================== build setup file -- sucked in later if -a was supplied ==================
cat <<endKat >$TMP/build_setup.$USER
	export NG_ROOT=$r
	export PKG_ROOT=$r
	export NG_SRC=$d/ningaui
	export TMP=$TMP
	#export NG_INSTALL_KSH_PATH=$ksh_path
	export NG_INSTALL_KSH_PATH="${NG_INSTALL_KSH_PATH:-/usr/bin/env ksh}"

	export PATH="$p"
endKat

echo "PATH=$p" >$r/cartulary

ensured $r/bin $r/lib

if [[ -f $NG_SRC/potoroo/srcmgt/sm_fingerprint.ksh ]]		# precompile in contrib needs this
then
	cp $NG_SRC/potoroo/srcmgt/sm_fingerprint.ksh $NG_SRC/.bin/ng_sm_fingerprint
	cp $NG_SRC/potoroo/lib/stdfun.ksh $r/lib/stdfun
fi

fix_mk			# fix the helper make files in mk and mk lib directories for the os/arch combo

if [[ -n $GLMK_AST ]]
then
	if [[ $PATH != *$GLMK_AST/bin* ]]
	then
		log_msg "WARNING: GLMK_AST is set but the directory is not in your path: $GLMK_AST/bin"
	fi
fi

if (( $do_all > 0 ))
then
	set -e
	cd $NG_SRC
	. $TMP/build_setup.$USER

        if  ! whence sh >/dev/null 2>&1 
        then            
                log_msg "cannot find sh in path; mk needs this"
                exit 1
        fi            

	log_msg "writing build stdout/err messages to $NG_SRC/build_log"

	log_msg "building mk"
	(cd contrib && ksh build precompile && ksh build all && ksh build install) >$NG_SRC/build_log 2>&1
	if (( $? > 0 ))
	then
		log_msg "failed to build mk; see $NG_SRC/build_log"
		exit 1
	fi

	if (( $nuke_first > 0 ))
	then
		log_msg "nuke first set; nuking... log in $NG_SRC/nuke_log"
		mk nuke >>$NG_SRC/nuke_log 2>&1
	fi

	log_msg "precompile begins"
	if ! mk precompile  >>$NG_SRC/build_log 2>&1
	then
		log_msg "precompile failed; see $NG_SRC/build_log"
		exit 1
	fi

	log_msg "mk all begins"
 	if ! mk all >>$NG_SRC/build_log 2>&1
	then
		log_msg "failed to mk all; see $NG_SRC/build_log"
		exit 1
	fi

	log_msg	"mk Install begins"
	if ! mk Install	 >>$NG_SRC/build_log 2>&1
	then
		log_msg "failed to mk install; see $NG_SRC/build_log"
		exit 1
	fi

	log_msg "generating man pages into ./manuals"
	if ! mk manpages >>$NG_SRC/build_log 2>&1
	then
		log_msg "failed to mk manpages; see $NG_SRC/build_log"
		exit 1
	fi

	if (( $gen_pkgs > 0 ))
	then
		PATH=$NG_SRC/bld_root/bin:$PATH
		log_msg "creating packages: potoroo contrib"
		ng_sm_mkpkg contrib
		ng_sm_mkpkg potoroo
	fi

	log_msg "build complete!"
	exit 0
fi


instruct
echo "execute $0 -v -h to see this message again."

cleanup 0

exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_build.ksh:Bootstrap build on a non-ningaui node)

&space
&synop	ng_build.ksh [-a] [-c cvs-host] [-D build-dir] [-d cvs-dir] [-f] 
&break	[-h] [-k] [-R root-dir] [-t tmp-dir] [-v]
&space
	ng_build.ksh [-v] -i ng-root-directory

&space
&desc	&This script creates a build environment in which ningaui software
	can be compiled.  If the source is not already in the current working 
	directory, and the CVS host with the source is available, the source is
	fetched.  If the all (-a) option is supplied on the command line, &this
	will attempt to compile all of the source. 
&space
	In general, &this is used to compile ningaui source and place it into 
	a 'build root' directory structure from which ningaui packages can be
	created.  It is possible to supply a real 'root' directory on the 	
	command line (-R) which will cause the newly built software to be installed
	into the named directory, rather than it being placed into a subdirectory
	to the current working directory.

&space
	&This will attempt to fetch the ningaui source from the CVS host if 
	it believes the source does not already exist in the current directory
	tree. The force (-f) option can be used to force a fetch. If the CVS 
	host that holds the ningaui source is not available using the -f option 
	will cause the script to fail.

&space
	At a minimum, this script sets up the following directories in the current
	working directory:
&space
&begterms
&term	.bin : A small set of tools and scripts that are needed by the build 
	process.
&space
&term	bld_root : A directory used as NG_ROOT (PKG_ROOT) during the build process. 
	The installation step of the build will install scripts, binaries and 
	other files into the directories under this directory.  The -R option 
	causes the script to use an alternate 'root' directory allowing the 
	newly built software to be directly installed. 
&space
&term	include : Ningaui header files are placed here.
&space
&term	stlib : Ningaui library files are placed here. 
&space
&term	manuals : HTML man pages are placed here when `mk manpages` is executed. 
&endterms
&space
	&This also creates an environment setup script fragment file in 
	build_setup.$USER in the $TMP directory (see -t).  This file should
	be sourced into the environment before running any &lit(mk) commands.
	If &this is used to compile the source after establishing the 
	environment, it is not necessary to source the file.  
&space
&subcat Building Packages
	After the software is built, even if not installed outside of the build 
	environment, it is possible to create ningaui packages. To do this
	the environmental setup file &stress(MUST) be sourced in, and then 
	the &lit(ng_sm_mkpkg) command can be executed.  The &lit(ng_sm_mkpkg)
	command will create packages for the components named on the command 
	line which can be sent and installed on other ningaui hosts. 


&opts	The following command line options are available:
&begterms
&term	-a : Build all. In addition to creating the environment the 	
	script will build the software. If this option is not supplied, 
	the build environment is intialised and the user must follow
	the instructions written to the standard error device to 
	compile the software. 
&space
&term	-c host : Supplied the name of the CVS host to fetch a copy 
	of the software from.   This option implies &lit(-f) and will 
	force a fetch of the source even if it exists in the current 
	directory.
&space
&term	-D dir : Causes the script to use &ital(dir) as the build directory.
	When omitted from the command line, the current directory is used. 
&space
&term	-d dir : Supplies the directory within the CVS environment (cvs root)
	to use when fetching source.  If not supplied, /cvsroot is assumed.
&space
&term	-f : Force a source fetch from CVS.  This script only fetches the source
	if the source appears not to reside in the current working directory (or 
	the directory supplied with the -D option). 
&space
&term	-h : Displays a short help message, and build instuctions if the environment
	has already been setup by this script. 
&space
&term	-i dir : Assumes package files (pkg-*) exist in the current directory. These 
	packages are installed into the &ital(dir) named after the option.  If this 
	option is used, all other options and actions are ignored. 
&space
&term	-k : Keep temporary files. 
&space
&term	-R dir : Supplies the directory used as NG_ROOT during the build process.
	If not supplied the directory &lit(bld_root) is created in the build 
	directory.  It is necessary to supply this directory only if the software
	is to be installed directly (say into /usr/local/ningaui) rather than 
	build with the intension of creating packages. 
&space
&term	-s path : Supplies the full path of the Korn shell (including the /ksh). This 
	string is used in all #! lines when .ksh script are installed.  If this is 
	not supplied, a search for kshell is made and the first one found is used. 
	The search order is: 
&space
&beglist
&item	/usr/common/ast/bin
&item	/usr/local/bin
&item	/usr/bin
&item	/bin
&endlist
&space
&term	-t dir : Uses &ital(dir) as a temporary directory.  If not supplied 
	&this will use &lit(/ningaui/tmp) or &lit(/tmp) if &lit(/ningaui/tmp) 
	does not exist. 
&space
&term	-v : Verbose mode.  
&endterms

&space
&exit	 Any non-zerio return code indicates an error. 

&space
&see	ng_sm_build, ng_sm_mkpkg

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Feb 2006 (sd) : Sunrise.
&term	18 Aug 2006 (sd) : Some changes to support release of software. 
&term	30 Aug 2006 (sd) : Added man page. 
&term	09 Oct 2006 (sd) : Added -p option to build packages with -a. 
&term	29 Mar 2007 (sd) : Fixed the cartulary that is created by this.
&term	20 Apr 2007 (sd) : Now pulls fingerprint stuff into .bin. 
&term	27 Jun 2007 (sd) : This now sets CVS_RSH if not set; we assume ssh.
&term	26 Nov 2007 (sd) : Added an install option to make installation easier.
&term	28 Jan 2008 (sd) : Ensures that Kshell is in the path. 
&term	09 Mar 2009 (sd) : Added some checking for make in the path and usable version of gawk/awk. 
&term	05 Jun 2009 (sd) : Now use the existing value of NG_INSTALL_KSH_PATH if set -- so we can 
		use this script to build generic binaries for the release site. 
&term	30 Jul 2009 (sd) : Added a bit to the usage message on trouble shooting. 
&endterms

&scd_end
------------------------------------------------------------------ */
