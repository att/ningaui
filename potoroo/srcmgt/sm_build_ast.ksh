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
#!/ningaui/bin/ksh
#!/ningaui/bin/ksh
#!/ningaui/bin/ksh
# Mnemonic:	sm_build_ast	
# Abstract:	
#		build the base ast stuff
#		fetch from a known dev node (not from ast developer's home environment)
#		NG_BUILD_AST_SRC is the name of the node and directory (ningd1:/ningaui/tmp)
#		where we expect the ast package files to be. 
#		
# Date:		15 june 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------

# some failures are acceptable (like when xf86 is not installed tk stuff fails)
# check the failures and return bad if something that is not on the acceptable list
# is found
function verify_failure
{
	typeset log
	typeset ok_file="$NG_ROOT/site/ast_ok_build_errors"
	typeset flist
	typeset glist
	typeset failed

	get_tmp_nm failed flist glist | read failed flist glist

	if [[ ! -d $tdir ]]
	then
		tdir=$TMP/${NG_PRODID:-ningaui}/build/ast	# try ningaui
		verbose "could not find dir under ${NG_PRODID:-ningaui}, trying $tdir"
	fi

	find $tdir/arch -name make.out | read log

	if [[ -z $log ]]
	then
		log_msg "cannot find log file from last build in: $tdir/arch/*/make.out l=$l"
		return 1
	fi

	verbose "sussing errors reported in: $log"

	gre "exit code [1-9]" $log >$failed

	if [[ -r $ok_file ]]
	then
		gre -v -f $ok_file $failed >$flist		# failures that are not acceptable
		gre -f $ok_file $failed >$glist		# failures that are ok per the ok file
	else
		verbose "could not find ok-file ($ok_file); all errors are fatal"
		cp $failed $flist			# no ok file - all things are then bad
		touch $glist
	fi

	if [[ -s $glist ]]
	then
		log_msg "[WARN] these bulid errors are deemed ok by $ok_file:"
		cat $glist >&2
	fi

	if [[ -s $flist ]]
	then
		log_msg "[FAIL] these build errors are fatal:"
		cat $flist >&2
		return 1
	fi

	return 0
}


function usage
{
	echo "usage: $argv0 [-d yyyy-mm-dd] [-m usern] [-n] [-p] [-P] [-r] [-s node:dir] [-t tmpdir] [-v]" >&2
	echo "	$argv0 -l  #list mode" >&2
}

function send_mail
{
	for u in $mail2
	do
		ng_mailer -T ${u} -s "$this_host: $1" ${2:-/dev/null}		# mailer can add NG_DOMAIN if needed
	done
}

function map_current2date
{
	ng_rcmd  ${src_node} ls -altr ${src_dir}/'ast.[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9].tgz'|tail -1| awk '
	{
		#-rw-r--r--  1 gsf gsf 23728885 May 29 14:09 /home/gsf/lib/package/tgz/ast.2007-05-29.tgz
		split( $NF, a, "." );
		print( a[2] );
	}'
}


# we do the acutal build in a subshell so we can preserve our environment
# for cleanup and other things once the build is completed
function do_build
{
	(
		# trash vars that give the ast build process heartburn
		unset MAKEFILES			# these gen an error if not set, but that is ok
		unset DISPLAY
		unset VIEWPATH

		set -e				# after here, dont be picky, die on any error
		export NPROC=1
		#export SHELL=/bin/ksh		# dont set this as it can cause ksh to build wrong 
		export CC=${GLMK_AST_CC:-gcc}	
		echo "exported CC = $CC $(whence $CC)"  >&2


		# must pull ningaui and ast things from the path
		# we expect gcc to live in /opt/sfw/bin on a sun; grumble
		# these directories must be added on suns: /opt/SUNWspro/bin /usr/ccs/bin
		old_path=$PATH
		for x in ${PATH//:/ } /opt/sfw/bin /usr/ccs/bin /opt/SUNWspro/bin
		do
			if [[ $x = "\." || $x = *${LOGNAME}*  || $x = *ningaui* || $x = *ast* ]]
			then
				echo "removed from path: $x" >&2
			else
				if [[ -d $x ]]
				then
					echo "kept in path: $x" >&2
					np="$np$x:"
				else
					echo "not found, not put into path: $x" >&2
				fi
			fi
		done
		
		if (( ${allow_ast:-0} > 0 ))
		then
			if [[ -e $ast_path/bin/ksh ]]
			then
				log_msg "allowing /usr/common/ast/bin back into path"
				export PATH=$PWD/bin:$ast_path/bin:$np:.
			else
				log_msg "could not find $ast_path/bin/ksh; not added to path" 
			fi
		else
			export PATH=$PWD/bin:$np:.
				echo "modified path = $PWD/bin:${np}." >&2
				#PATH="${np}:."
		fi
		log_msg "modified path=$PATH"

		export CC=gcc

		whence gcc|read gccp
		if [[ -n $gccp ]]
		then
			echo "found gcc; letting it ride [OK]"
		else
			echo "gcc not found in path, reverting to cc [WARN]" >&2
			export CC=cc
		fi
		
		echo "final path = $PATH" >&2
		
		options=quiet
		action="make"
		case $1 in					# other possibilities, but doubt we will make use of them
			debug)	options="debug"; shift;;
			install) action="install flat - ";;
			test)	action="test";;
			build)	action=make;;
		esac
		
		export LD_LIBRARY_PATH="/usr/local/lib"
		

		if [[ -x bin/package ]]		# unpack of ast tar files should put this here
		then
			pkg_cmd=bin/package
		else
			pkg_cmd=package
		fi
		$pkg_cmd $options $action $2 $3

		rm -fr $TMP/PID$$.d
		exit $?
	)

	return $?
}

# -------------------------------------------------------------------
wash_env "+NG_BUILD_AST_SRC:NG_BUILD_ASTPATH"		#scrub, but keep our important things

LOGNAME=${LOGNAME:-$USER}		# bloody sunos drops USER
LOGNAME=${LOGNAME:-${NG_PROD_ID:-ningaui}} 		# when run from rota these may have been scrubbed from the env
export USER=$LOGNAME			# prevent problems lest someone not realise sun's oddness

src_loc="${NG_BUILD_AST_SRC:-ningd1:/ningaui/tmp}"
date="current"
TMP=${TMP:-/tmp}
list=0			# -l is list only the ast files on the source node
fetch=1			# -n turns off fetch
build=2			# -n -n turns off fetch and build
pause=0			# -p causes a pause after gzip files are read
register=1		# -r turns off to keep us from registering the package
publish=0		# -P turns on
verify_last=0		# -V just verify the last build and exit
verify_patches=1
do_deltas=1		# -D turns this off

ast_path=/usr/common/ast
if [[ -n $NG_BUILD_ASTPATH ]]		# force on for some clusters
then
	allow_ast=1
	ast_path=$NG_BUILD_ASTPATH
else
	allow_ast=0
fi

mail2=""
this_host=$(ng_sysname)


while [[ $1 == -* ]]
do
	case $1 in 
		-A)	allow_ast=1; ast_path=/usr/common/ast_current;;		# same as -a, but with bleeding edge
		-a)	allow_ast=1;;
		-d)	date="$2"; shift;;
		-D)	do_deltas=0;;
		-g)	export CCFLAGS="-g $CCFLAGS";;				# turn on -g in build
		-l)	list=1;;
		-m)	mail2="$mail2$2 "; shift;;
		-n)	fetch=0; build=$(( $build - 1 ));;			# -n == skip fetch; -n -n skip fetch and build
		-p)	pause=1;;						# pause after expansion, before build
		-P)	publish=1;;
		-r)	register=0;;						# dont register the package
		-s)	src_loc="$2"; shift;;
		-t)	TMP="$2"; shift;;

		-v)	vflag="$vflag-v "; verbose=1;;
		-V)	verify_last=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
	esac

	shift
done

src_node=${src_loc%%:*}
src_dir=${src_loc##*:}

if (( $list > 0 ))
then
	ng_rcmd  ${src_node} ls -altr ${src_dir}/'ast.[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9].tgz' | awk '{printf( "PACKG: %s\n", $0 ); }'
	ng_rcmd  ${src_node} ls -altr ${src_dir}/'ast.[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9].20[0-9][0-9]-*.tgz' | awk '{printf( "DELTA: %s\n", $0 ); }'
	
	cleanup 0
fi

if [[ $date == "current" ]]
then
	map_current2date | read date
	verbose "current maps to ${date:-(emtpy string)}"
fi

if [[ -z $date ]]
then
	log_msg "bad date (empty)"
	send_mail "[ERR] ast package creation failed during initialisation"
	cleanup 1
fi

pkg_data="ast release: $date"

ifile="INIT.$date.tgz"
afile="ast.$date.tgz"

tdir=$TMP/$USER/build/ast				# target dir
get_tmp_nm dlist | read dlist		# list of delta files


if (( $verify_last > 0 ))
then
	verify_failure
	cleanup $?
fi

if (( $fetch > 0 ))
then
	rm -fr $tdir-						# trash the old backup
	if [[ -e $tdir ]]
	then
		verbose "moving $tdir to $tdir- as a backup"
		mv $tdir $tdir-
	fi
	
	if ! mkdir -p $tdir
	then
		log_msg "cannot make $tdir [FAIL]"
		send_mail "[ERR] ast package creation failed during initialisation"
		cleanup 1
	fi
	
	if ! mkdir -p $tdir/lib/package/tgz
	then
		log_msg "cannot make $tdir/lib/package/tgz [FAIL]"
		send_mail "[ERR] ast package creation failed during initialisation"
		cleanup 1
	fi
	
	if ! mkdir -p $tdir/lib/bin
	then
		log_msg "cannot make $tdir/lib/bin [FAIL]"
		send_mail "[ERR] ast package creation failed during initialisation"
		cleanup 1
	fi
	
	cd $tdir
	
	log_msg "snarf: $src_node:$src_dir/$ifile --> $tdir/lib/package/tgz"
	if ! ng_rcmd  $src_node ng_ccp -v -d $tdir/lib/package/tgz/ $this_host $src_dir/$ifile  >$TMP/PID$$.err 2>&1
	then
		cat $TMP/PID$$.err
		log_msg "could not snarf file: $ifile [FAIL]"
		send_mail "[ERR] ast package creation failed could not find input $ifile"
		cleanup 1 
	fi
	
	log_msg "snarf: $src_node $src_dir/$afile --> $tdir/lib/package/tgz"
	if ! ng_rcmd  $src_node ng_ccp -v -d $tdir/lib/package/tgz/ $this_host $src_dir/$afile >$TMP/PID$$.err 2>&1
	then
		cat $TMP/PID$$.err
		log_msg "could not snarf file: $afile [FAIL]"
		send_mail "[ERR] ast package creation failed could not find input $afile"
		cleanup 1 	
	fi

	# go fetch all deltas for the package; deltas have .yyyy-mm-dd added between the package date and .tgz
	if (( $do_deltas > 0 ))
	then
		ng_rcmd $src_node ls $src_dir/ast.$date.20??-??-??.tgz $src_dir/INIT.$date.20??-??-??.tgz >$dlist
		delta_data=""
		while read delta
		do
			dn=${delta##*/}
			delta_data="$delta_data ${dn%.tgz}"		# string to add to pkg_data for install file
	
			log_msg "snarf: delta: $src_dir/${delta##*/} --> $tdir/lib/package/tgz"
			if ! ng_rcmd  $src_node ng_ccp -v -d $tdir/lib/package/tgz/ $this_host $src_dir/${delta##*/} >$TMP/PID$$.err 2>&1
			then
				cat $TMP/PID$$.err
				log_msg "could not snarf delta: $src_node:$src_dir/${delta##*/} [FAIL]"
				send_mail "[ERR] ast package creation failed could not snarf delta: $delta"
				cleanup 1 	
			fi
		done <$dlist

		pkg_data="$pkg_data deltas:$delta_data"		# add delta data to the string for install file
	else
		verbose "deltas skipped (-D option on)"
	fi

	verbose "snarf completed [OK]"
else
	log_msg "fetch skipped (-n)"
fi

ls -al  $tdir/lib/package/tgz/*

if [[ -f $ast_path/bin/package ]]
then
	cp $ast_path/bin/package $tdir/lib/bin/
else
	log_msg "cannot find package in $ast_path/bin/package; find it and copy to $tdir/lib/bin/ [WARN]"
	send_mail "[ERR] ast package creation failed could not find package script"
	cleanup 1
fi
log_msg "found copy of package script [OK]"

if (( $allow_ast > 0 ))
then
	ksh_cmd=$ast_path/bin/ksh
else
	ksh_cmd=ksh
fi


if (( $build > 0 ))
then
	if [[ -d ~/.probe ]]
	then
		log_msg "moving ~/.probe out of the way (to ~/.probe-). move back after build if you need it"
		rm -fr ~/.probe-
		mv ~/.probe ~/.probe-
	fi
	
	log_msg "unpacking ast files..."
	if ! $ksh_cmd $tdir/lib/bin/package read
	then
		log_msg "ksh command used: $ksh_cmd"
		log_msg "unpack failed [FAIL]"
		send_mail "[ERR] ast package creation failed to unload"
		cleanup 1
	fi
	
	if (( $pause > 0 ))
	then
		log_msg "PAUSED! Press any key to resume and start build"
		read foo
	fi

	if [[ -f $NG_ROOT/site/patch.ast ]]		# list of date source dest tripples for patching
	then
		cat $NG_ROOT/site/patch.ast | while read apply_date from to junk
		do
			if [[ $apply_date != "-" ]]		# convert date to a format for comparison
			then
				cdate=${date//-/}		# replace - chars in date 
			else
				cdate=$date
			fi

			if [[ $apply_date == "#"* || ! $from ]]		# allow comments and blank lines
			then
				:
			else
				if [[ $cdate == $apply_date  || $apply_date == "all" ]]		# apply date can be a pattern when on rhs of ==
				then
					log_msg "before patch: $(ls -al $to)"
					if ! cp $from $to
					then
						log_msg "cannot apply patch: $from $to [FAIL]"
							cleanup 1
					fi
					log_msg "patch applied: $from $(ls -al $to)"
					patches="$patches${from##*/} "
				else
					log_msg "patch skipped: ad=$apply_date != d=$cdate patch=$from"
				fi
			fi
		done

		pkg_data="$pkg_data patches=$patches"
		log_msg "pkg_data = $pkg_data"
	else
		log_msg "no patches to apply"
	fi

	log_msg "build starts...."
	if ! do_build
	then
		if ! verify_failure
		then
			log_msg "build errors [FAIL]"
			send_mail "[ERR] ast package creation failed to build"
			cleanup 1
		fi
	fi

	log_msg "build completed [OK]"
else
	log_msg "build skipped (-n -n) [OK]"
fi


# create tar files 
log_msg "making pkg files"
log_msg "these binaries disabled in the final pkg-ast- file: who, ps, df"		# per mark's request 10/09/07
log_msg "package data in install script: $pkg_data"

if ! ng_sm_mkpkg_ast -r "who ps df" -D "$pkg_data" $vflag -s $tdir/arch
then
	log_msg "errors building package [FAIL]"
	send_mail "[ERR] ast package creation failed making the ningaui pkg-ast package"
	cleanup 1
fi

if (( $register > 0 ))
then
	log_msg "registering package -- replicate to all nodes on the cluster"
	ls -t pkg-ast-*|head -1 |read pname
	nm=$(ng_spaceman_nm $pname )
	if ! cp $pname $nm 
	then
		log_msg "unable to copy pkg file ($pname) to repmgr space ($nm) [FAIL]"
		cleanup 1
	fi

	if ! ng_rm_register $vflag -e $nm
	then
		log_msg "registration of package file had errors: $nm [FAIL]"
		send_mail "[ERR] ast package creation failed to register"
		cleanup 1
	fi
fi

if (( $publish > 0 ))
then
	log_msg "publishing the package"
	ng_sm_publish $vflag -p -l pkg-ast*
fi

log_msg "ast package creation complete [OK]"
send_mail "[OK] ast package creation complete"
cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_build_ast:Fetch and build the basic AST software)

&space
&synop	ng_sm_build_ast [-d date] [-l] [-m user] [-n [-n]] [-P] [-p] [-r] [-s host:dir] [-t tmp-dir] [-v]

&space
&desc	&This will fetch a copy of the AST source tar file(s) from a development 
	node, unload them, make the software, and then create a ningaui package
	containing the &lit(bin,) &lit(lib,) &lit(include,) and &lit(man) directories.
	The ningaui package can be installed using ng_refurb and will be placed into
	&lit(/usr/common/ast_current.) Unless specifically disabled on the command line,
	the resulting pacakge is registered in replication manager space. The package is 
	pushed out to all clusters with like nodes only when the push option (-P) is
	supplied. 
&space
&subcat Delta Releases
	AST delta source tar files are also looked for in the source directory. These
	files are expected to have a second date, dot separated from the package date,
	as a part of the file name (e.g. ast-2007-11-05.2007-12-15.tgz).  All deltas
	that have the same base date will also be fetched and added to the library directory.
	The ast &ital(package) command understands how to apply the deltas. 
&space
&subcat	Allowable Failures
	In some environments certain build failures might not be considered fatal and the 
	generation of the package allowed to continue even though the overall build failed. 
	Such failures, caused if X-windows is not installed, can be overlooked if the failing 
	component(s) are listed in NG_ROOT/site/ast_ok_build_errors. This file should contain
	a set of regular expressions that are used to match "exit code n" statements generated
	by the AST build process. If this file is not present, all errors listed in the build 
	output are considered fatal. 

&space
&subcat	Patches
	If source files exist that 'patch' specific AST releases, then they can be applied to 
	the soruce tree before it is built.  &This will unload the AST tar files and then look
	for $NG_ROOT/site/patch.ast which should contain three values per non-commented line:
&space
   date patch-path install-path
&space

	Where:
&space
&begterms
	date : Is the date of the AST package that the patch is to be applied to.  It can be a simple 
		pattern that can be matched inside of [[ and ]] of a Kshell evaluation.
		The word "all" can also be supplied to have the patch applied to all AST releases; this
		is NOT recommended. The date may be either yyyymmdd or yyyy-mm-dd format. 

&space
&term	patch-path : Is the path of the file containing the fix. It can be any name such that the patch
	can be dated or otherwise differienciated from similar or duplicate patches. 
&space
&term	install-path : Is the full pathname, including basename, of where the patch should be coppied
	into the AST source tree. 
&endterms

&space
&subcat Pinkpages Variables
	The following pinkpages variables affect how this operates:
&space
&begterms
&term	NG_BUILD_ASTPATH : Defines the path to the ast directory that should be used. If set
	it implies that -a/-A has been placed on the command line and oferrides the default
	path assocuated with -a and -A. If not set, the ast biinaries are not included in the 
	search path unless -a or -A is given on the command line. 
&space
&term 	NG_BUILD_AST_SRC : Defines the node that contains a copy of the most recent
	ast source packaage files. The files are assumed to be in /ningaui/tmp.
&endterms

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-A : Use the bleeding edge of the ast that is installed (/usr/common/ast_current).
	If omitted, the ast binaries are not included in the path.  This may be necessry on 
	some linux flavours to avoid picking up an old, buggy, shell installed in /usr/bin.
&space
&term	-a : Same as -A, except /usr/common/ast is used instead of the most recenlty
	generated ast.  
&space
&term	-d date  : Specifies the date (yyyy-mm-dd) of the ast source to fetch. If not 
	specified, the most recent copy is fetched.
&space
&term	-D : Turn off delta processing; no ast delta files will be coppied to the build area.
	This might be necessary to build on a node that does not have ast (specifically
	pax) installed.
&space
&term	-g : Sets the &lit(-g) filag in the &lit(CFLAGS) environment variable. This 
	&ital(should) trickle into the ast build process and cause binaries to be produced
	with debugging information intact.
&space
&term	-l : List only.  Lists the available source tar files and exits. 
&space
&term	-m user : Send a completion mail message to the user(s) listed. If more than one user
	is to receive mail, then they must be supplied as a space separated list, or with multiple
	-m options.
&space
&term	-n : No fetch mode.  If two -n options are supplied on the command line, then 
	the source is not fetched, and not compiled. 
&space
&term	-p : Pause mode. The script will pause between unloading the tar files and starting
	the build process.  This allows the user to modify or change the source. 
&space
&term	-P : Publish the package to other clusters with like nodes. 
&space
&term	-r : No register mode. If this option is present on the command line, the resulting 
	package file will not be registered in replication manager space. 
&space
&term	-s host:dir : Supplies the host where the script should expect to find the AST 
	source, and the directory.  This host:directory combination must be accessable via 
	ng_rcmd. The pinkpages variable &lit(NG_BUILD_AST_SRC) may also contain the 
	desired node:directory combination. Using the variable is the preferred method. 
&space
&term	-v : Verbose mode.

&endterms


&space
&exit	A non-zero exit code inidcates an error. 

&space
&see	ng_sm_autobuild, ng_sm_mkpkg_ast, ng_sm_publish

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 Jun 2007 (sd) : Blastoff.
&term	28 Jun 2007 (sd) : Fixed bug in registration -- package file was not being copied to repmgr space before
		invoking ng_rm_register.
&term	11 Jul 2007 (sd) : Corrected typo in getting package name for registration.
&term	13 Jul 2007 (sd) : Corrected bug with setting of TMP default.
&term	20 Jul 2007 (sd) : Now registers the package for all nodes on the cluster. 
&term	13 Aug 2007 (sd) : We now capture the ast release date and put into the INSTALL script via the -D option.
&term	21 Aug 2007 (sd) : Added -p option to sm_publish command so that we push the package out to clusters. 
&term	10 Oct 2007 (sd) : The who and ps commands are disabled when the ningaui style ast pkg file is created now. They were 
		causing issues on some nodes.
		.** this disable request made by mp on 10/09/2007.
&term	01 Nov 2007 (sd) : Added -g option.
&term	28 Nov 2007 (sd) : Added support to ignore certain failures. 
&term	30 Nov 2007 (sd) : Added date support for patches and updated man page to include doc on patching. 
&term	03 Jan 2008 (sd) : Added ability to fetch ast deltas.
&term	13 Feb 2008 (sd) : Added -D option.
&term	14 Mar 2008 (sd) : Corrected problem that was causing an empty directory path portion on sun because 
		USER and LOGNAME are not set consistantly. 
&term	07 Apr 2008 (sd) : Had to put more hoops in for sun environment.  Seems if the license server disappears
		we cannot build because ast doesn't always use $CC.  We fake it out by putting a link from cc to gcc in the path. 
&term	26 Jun 2008 (sd0 : Added df to the list of binaries that are disabled in the ast package file. 
&term	08 Jul 2008 (sd) : fully qualified package command to unload ast stuff.
		Also now use GLMK_AST_CC rather than GLMK_CC; on darwin thus must now be cc. 
&term	29 Jul 2009 (sd) : The mail to user value is passed as is now; no attempt to add @$NG_DOMAIN which was buggering things
		if NG_DOMAIN was scrubbed.  Mailer will do the right thing as it should get it from the cartulary even if 
		this script has washed it away.
&term	08 Sep 2009 (sd) : Now accepts NG_BUILD_ALLOWAST from pinkpages to set the ast path and force the inclusion of 
		ast in PATH.  Needed on some flavours of linux to avoid old buggy shells that lurk in /usr/bin.
&term	17 Sep 2009 (sd) : Cleaned up the setting of CC based on discovery of gcc. 
&endterms


&scd_end
------------------------------------------------------------------ */

