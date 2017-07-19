#!/ningaui/bin/ksh
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


#!/ningaui/bin/ksh
# Mnemonic:	ng_refurb
# Abstract:	Packages and then distributes things to other machines in the 
#		cluster. Also can be run to unload the received file
# Date:		05 March 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

my_uname=ng_uname

# suss out files in the listed directories that are probably scripts (according to file)
# from that list we print a set of tripples (script-name, #! string, shell name) for 
# the files that appear to have an invalid #! path.  If the script type is ksh, then 
# we copy the script off and let ng_install reinstall it assuming that it will get the 
# #! business right on the install node as opposed to the node where packages were created.
#
# we now allow for `#!/usr/bin/env ruby` style hash-bang lines. We only verify that the 
# "path/shell" part is right -- if ruby is not installed we will not catch it.
function fix_hashbang
{
	while [[ -n $1 ]]
	do
		(
		cd $1

		# WARNING: file returns different verbage on different systems. script seems the only common token
		ls | xargs file | awk '/script/ { gsub( ":", "", $1 );  print $1 }' |while read f
		do
								# watch out; bloody suns barf if trailing whitespace after 1q!
			sed 's/\#!//; 1q' <$f | read what junk 	# pluck path/shell from #!/path/shell junk
			if [[ -n $what && ! -e $what ]]
			then
				echo $f $what ${what##*/}
			fi
		done >/tmp/vhb.$$				# DONT use TMP, it might not be set
		
		while read script path shell
		do
			case $shell in 
				ksh)				
					verbose "re-installed $script to fix $path"
					gre -v "^#!" $script >/tmp/$script.$shell			# toss out the original one
					(cd /tmp; ksh $NG_ROOT/bin/ng_install $script.$shell $1/)	# use ksh cmd; ng_install might have bad #! too
					rm /tmp/$script.$shell 
					;;
	
				*)	
					log_msg "WARNING: the #! path seems wrong in $script shell=$shell path=$path; it was left unchanged"
					;;
			esac
		done </tmp/vhb.$$
		)

		shift
	done

	rm -f /tmp/vhb.$$
}


# create goaway files for each thing passed in
function pause_daemons
{
	typeset flag=$1
	shift
	while [[ -n $1 ]]
	do
		verbose "create/remove ($flag) goaway file for: $1"
		ng_goaway $flag $1
		shift
	done
}

# make a complete package name from pkg_info which can be a three token, colon
# spearated string component:date:rel-type (e.g. potoroo:20040403[:pkg])
# pkg-<component>-<sysinfo>-<arch>-<date>-rel.tgz
function mk_pname 
{
	typeset pkg_info="$1"
	typeset os="`uname -s`"		# os type 
	typeset ver="`uname -r`"	# os ver  5.2 from 5.2.1...
	typeset arch=""			# i386
	$my_uname -p | read arch
	case $arch in
		sun4*)	arch="sun4.";;			# treat sun4u and sun4v the same; must use . to match pattern when given to rm-where
	esac

	typeset maj=""
	typeset min=""
	typeset date=""
	typeset unit=""
	typeset pkg_type="$2"

	if [[ -z $pkg_info ]]
	then
		echo "pkg-no-pkg_info-supplied"		# somthing bogus for them to fail on
		return
	fi

	echo $pkg_info |IFS=":" read unit date pkg_type junk
	echo $ver| IFS='.-' read maj min junk
	min=${min%%-*}

	if [[ -n $min_override ]]
	then
		verbose "overriding $maj.$min with $maj.$min_override"
		min=$min_override				# allow 4.9 to change to 4.8
	fi
	echo "${pkg_type:-pkg}-${unit}-$os-$maj.$min-$arch-${date:-.*}-${rel_type:-rel}.tgz"
}

# send a file off 
# parms expected to be:  node file remote-filename
function push_file
{
	if [[ $via_ftp -gt 0 ]]
	then
		verbose "put $2 $3"
		echo "put $2 $3" >/tmp/ftp.$$
		echo "quit" >>/tmp/ftp.$$
		ftp </tmp/ftp.$$ $1				# we force the login/pw 
		return $?
	else
		verbose ng_ccp $vflag -d $3 $1 $2
		ng_ccp $vflag -d $3 $1 $2
		return $?
	fi
}

# update the history file ($1 is the tar filename)
function update_hist
{
	verbose "$argv0 on $me: unload successful ($1)"
	#echo "installed: $now $1" >>$NG_ROOT/site/refurb	# old spot
	echo "installed: $now $1" >>$NG_ROOT/site/pkg_hist	# new official spot
}

# if the package tar file has a ./INSTALL file, then we will extract it and run it
# rather than loading the package ourself.  If we find it, we pass it just the name
# of the tar file.  
# returns from this function:
#	0 == unload/install successful; package was installed
#	1 == no INSTALL file existed; regular processing to install should continue
#	>1 == error; script probably should abort.
# --------------------------------------------------------------------------------------
function try_install 
{
	typeset rc=0

	if (( $install_allowed < 1 ))
	then
		verbose "not testing for INSTALL; -I supplied on command line"
		return 1
	fi

	if ! cd $TMP
	then
		return 2
	fi

	if ! mkdir wrk.$$
	then
		return 2
	fi
	verbose "work directory created successfully"

					# blooday tar does not always put ./ on the filename so we must extract for both cases
	tar -xf $1 INSTALL ./INSTALL	# AND tar returns good even when neither file is in the archive -- erk!
	if [[ ! -f ./INSTALL ]]
	then
		verbose "did not find ./INSTALL in package file"
		rm -fr wrk.$$
		return 1
	fi

	mv INSTALL $TMP/INSTALL.$$
	chmod 755 $TMP/INSTALL.$$
	cd $TMP
	rm -fr wrk.$$

	verbose "found INSTALL in package; invoking it"
	verbose "======================================"
	if (( $forreal > 0 ))
	then
		$TMP/INSTALL.$$ $1
		rc=$?
	else
		log_msg "would run: ./INSTALL.$$ $1"
	fi

	verbose "======================================"
	if (( $rc == 0 ))
	then
		verbose "INSTALL script completed successfully"
		return 0
	fi

	mv ./INSTALL.$$ ./INSTALL.$$.failed
	log_msg "INSTALL script failed.  left in $TMP/INSTALL.$$.failed for debugging"
	return 2			#must return higher than 1 so that we do not attempt to refurb directly from the tar file
}

# receive and unpack a distribution
function suckin
{
	rc=0
	ng_sysname |read me
	tarf=$1

	ng_dt -c |read now
	if [[ -r $tarf ]]
	then
		echo "$now: installing from: $tarf" >>$logf

		if [[ $tarf = *gz ]]		# allow for .gz or .tgz
		then
			zfile=$tarf
			if [[ $tarf = *.tgz ]]
			then
				tarf=${tarf%.*}.tar		# convert to .tar
			else
				tarf=${tarf%.gz}		# just yank .gz
			fi

			if ! gzip -dc <$zfile >$tarf 
			then
				log_msg "abort: cannot uncompress: $zfile"
				cleanup 1
			fi
		fi

		try_install $tarf				# if pkg has an INSTALL script
		case $? in
			0)	update_hist $tarf		# worked; we are done here
				return 0
				;;

			1)	;;				# try_install rc==1 means no install script; continue

			*)	log_msg "abort: fatal error searching for, or executing, INSTALL in tar file: $tarf"
				cleanup 1
		esac

		if [[ ! -d ${unloadd:-/nounload_supplied} ]]
		then
			verbose "unload directory not found; making: ${unloadd:-nounload_dir_supplied}"
			mkdir -p ${unloadd:-/nounload_supplied}			
		fi

		if  cd ${unloadd:-nounload_supplied}
		then
			tar -tvf $tarf | awk '{ print $1, $NF }' | while read p fn
			do
				if [[ $p = -* && $p = *x*  && -e ${fn:-nosuchfile} ]]		#regular file and executable
				then
					echo "moved executable: $fn --> $fn-" >>$logf
					mv $fn $fn-			# sneak the new one in if executable 
					if [[ ${fn##*/} != ng_refurb ]]	# bad things might happen if we rm ourself
					then				
						/bin/rm $fn-		# full path because we dont want to use ningaui/bin/rm
					fi
				fi
			done 

			if tar -xvf $tarf >$TMP/refurb.lst		# unload and save the last list
			then
				update_hist $tarf
			else
				log_msg "$argv0 on $me: ABORT: unload of tar FAILED!"
			fi
		else
			log_msg "$argv0 on $me: ABORT: unable to switch to unload directory: $unloadd"
			rc=1
		fi
	else
		log_msg "$argv0 on $me: ABORT: cannot find or read tar file: $tarf"
		ls -al ${tarf:-no-file-name-found}
		rc=1
	fi

	return $rc
}

# ------------------------------------------------------------------

ustring="[-b] [-f dir] [-I] [-M min-override] [-n] [-N] [-s] [-u unload-rootdir] [-t rel-type] [-v] {pkg_filename | pkg_info}"

mynode=`ng_sysname`
logf=$NG_ROOT/site/log/refurb		# a private idaho for scribbling things
wrkd=$TMP				# big spot to build our distribution file
unloadd="/"				# where we will unpack
verbose=1			# decided its nice to have chat by default
forreal=1
install_allowed=1		# allowed to look for INSTALL in the pkg (allows INSTALL script to use refurb to unload pkg)
keep_tar=1			# assume user supplies a tar -- we may leave it uncompressed, but undeleted too
rel_type=rel			# -t overrides; appended to end of package name -$type
min_override=$NG_PKG_MINVER	# if set use it by default; -M still overrides
stability_flag=""		# -s sets to -a for rm_where call
fix_dir=""			# directory list that we will fix ksh #! paths if they seem wrong (set by -f and -u opts); defaults to NG_ROOT/bin
allow_fix=1;			# -N turns this off and no #! fixup will be done; mostly for installing things like ast

echo "`ng_dt -i` `date` started" >>$logf

uname -s|read mytype		# os type of this node -- can distribute only to similar types

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	rel_type=beta;; 
	
		-f)	fix_dir="$fix_dir$2 "; shift;;

		-I)	install_allowed=0;;			# must ignore INSTALL if it exists in the package file

		-M)	min_override=$2; shift;;		# min version override -- allows 4.8 to be installed on 4.9 systems
		-n)	forreal=0;;
		-N)	allow_fix=0;;

			# -r is deprecated; here for backwards compat
		-r)	tar_file="$2"; shift;;		 	# can be something like potoroo:20040403:rel
		-r*)	tar_file="${1#-?} ";;

		-s) 	stability_flag="-a";; 

		-u)	unloadd="$2"; shift
			fix_dir="$fix_dir$unloaddd "
			;;

		-u*)	unloadd="${1#-?} "
			fix_dir="$fix_dir$unloaddd "
			;;

		-t)	rel_type=$2; shift;;

		-v)	vflag="$vflag-v "; verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac


	shift
done


tar_file=${tar_file:-$1}		# support old -r syntax; default to command line parm 
if [[ -n $tar_file ]]			# from command line or $1
then
	if [[ $tar_file = *:* ]]			# package info, not absolute tar name -- search repmgr space for the real thing
	then
		pkg_name=`mk_pname $tar_file $pkg_type`				# make package name  from potoroo:20050229:pkg
		verbose "searching for package: $pkg_name"
		if pkg_path=`ng_rm_where $stability_flag -p $mynode $pkg_name|sort |head -1`
		then
			pkg_path="${pkg_path#* }"
			if [[ -n $pkg_path && $pkg_path != "MISSING" ]]		# this is likely parnoia
			then
				verbose "copy $pkg_path to $TMP/${pkg_path##*/} for unload"
				if [[ $forreal -gt 0 ]]
				then
					if ! cp ${pkg_path:-unset-pkg_path} $TMP/${pkg_path##*/}		# move to scratch spot to unzip
					then
						log_msg "cp $pkg_path to \$TMP ($TMP) failed"
						cleanup 1
					fi
				fi
				pkg_path=$TMP/${pkg_path##*/}
			else
				log_msg "invalid or no path returned by rm_where: ${pkg_path:-null-string}"
				cleanup 1
			fi
		else
			log_msg "cannot find a package on this node: ${pkg_name:-null-string}"
			cleanup 1
		fi

		keep_tar=0			# this is a tmp copy, trash it
		tar_file=$pkg_path		# suck in will be passed this
	else
		if [[ $tar_file != /* ]]	# relative path, must be fully qualified
		then
			tar_file="$PWD/$tar_file"
		fi
	fi

	rc=0
	if [[ $forreal -gt 0 ]]
	then
		pause_daemons -s $NG_REFURB_PAUSE	# pause/stop these things during a reload
		suckin $tar_file			# go get the named file and install it here
		rc=$?

		if (( $allow_fix > 0 ))
		then
			verbose "fixing #! paths in ${fix_dir:-$NG_ROOT/bin}"
			fix_hashbang ${fix_dir:-$NG_ROOT/bin}
		fi

		ng_fixup -v 				# fix flock specific links in lib/bin directories
		pause_daemons -r $NG_REFURB_PAUSE	# remove the goaway files we set

		if (( ${keep_tar:-1} < 1 ))
		then
			verbose "removing tar file: $tar_file"
			rm -fr $tar_file
		fi
	else
		log_msg "would unload ${tar_file:no-name-found}"
	fi

	cleanup $rc
fi

log_msg "pkg filename, or pkg info, not supplied on command line"
usage
cleanup 1
exit 

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_refurb:Unload a package file to refurbish a node)

&space
&synop	
	ng_refurb [-b] [-f] [-I] [-M minor] [-n] [-N] [-s] [-u unload-rootdir] [-t rel-type] [-v] pkg_name | pkg_info

&space
&desc
	&This accepts a package name or packge information (pkg_name:date:type)
	on on the command line and causes the package to be unloaded on the 
	current node.  The package file is expected to be a tar, or gizpped 
	tar, file. If package information is given, the filename is calculated 
	and the file is expected to be found in replication manager file space. 

&space
	Package information consists of three colon separated tokens:
&space
&begterms
&term	name : The software package name (e.g. potoroo, feeds, mlink).
&space
&term	date : The date that the package filename contains (yyyymmdd).
&space
&term	prefix : The file prefix. This defaults to pkg and is the lead portion of the real package file name.
&endterms

&space	
	A full package name (e.g. pkg-name-system_type-date-release_type) is created 
	from the package information and replication manager file space
	on the node is searched for the actual file.  When found, the file 
	is coppied to the &lit($TMP) directory where it is uncompressed 
	if needed and then used. 

&space
	Unloading consists of the following steps:
&space
&begterms
&term	1) Changing to the unload directory supplied with the &lit(-u) command 
	line parameter, or '/' if no directory is supplied.
&space
&term	2) Creating go-away files for all ningaui processes listed in the 
	pinkpages variable &lit(NG_REFURB_PAUSE) to prevent things like
	ng_flist_send from running during a refurb.
&space
&term	3) Unloading the package file.  
&space
&term	4) Removing all go-away files that were created.
&space
&term	5) Removing the working package file if it was coppied from 
	replication manager file space. 
&endterms

&space
&subcat The INSTALL script
	If the package &lit(tar) file contains a file named &lit(./INIT) it is 
	extracted and executed.  It is assumed that this script can be passed the 
	name of the &lit(tar) file and will do the necessary things to install
	the package. The INIT script may invoke &this as long as it passes the 
	&lit(-I) command line option. (This allows the init script to set the 
	unload root directory to something other than &lit(/), and then
	to use &this to do the installation.) 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b : Assume release type is 'beta' and thus -beta will be assumed as the final 
	component of the package name. If not supplied, "-rel" is assumed unless the -t 
	option is used. 
&space
&item 	-f dir : Supplied the bin directory that should be searched for hash-bang (#!)
	fixup.  If not supplied, NG_ROOT/bin is assumed.
&space
&item	-F : Disable hash-bang fixup. 
&space
&term	-I : Ignore INSTALL script.  If the package(s) that are to be used for refurbing
	have an INSTALL script it will be ignored.  This allows the INSTALL script 
	to invoke &this to install a package (generally in this case the invocation of &this 
	by INSTALL will be to simply adjust the unload directory path).
&space
&term	-M minor : Allows the minor version of the operating system to be ignored and packages
	created with &ital(minor) found. This allows a 6.2 package to be installed on a 6.3
	system when &lit(-M 2) is supplied. 
&space
&term	-n : No execution mode. Some information about what would be done is written to 
	the standard error device, and &this stop. (see -s)
&space
&term	-s : Ignore stability.  The package will be located and used without regard to 
	the stability of the file in replication manager. 
&space
&term	-t type : The package type. The type defautls to "rel" and is the last component
	of the package pathname.  Other types of packages might be beta, alpha, or patch.
&space
&term	-u dir : Supplies an alternate &ital(root) directory where the package should be 
	unloaded after being transferred to each target node. If this option is not 
	supplied, the package is unloaded into the root directory (/) thus overlaying 
	the 'live' files. 
&space
&term	-v : Be chatty. 
&endterms

&space
&parms	&This expects that the package filename, or package information, is supplied as the 
	only command line parameter.  If package information is supplied, then a full 
	package name is created and replication manager space is searched for the package.

&examp
&litblkb
    # from a known location
    ng_refurb -v -u $NG_ROOT /ningaui/tmp/pkg-potoroo-FreeBSD5.3-i386-20050229-rel

    # from somewhere in repmgr space: pkg-potoroo-<machinetype/ver>-20050229-rel
    ng_refurb -v -u $NG_ROOT potoroo:20050229:pkg

    # from an INSTALL script that does nothing fancy
    ng_refurb -v -I -u $NG_ROOT/apps/mlink  mlink:20050229:pkg  
&litblke

&space
&see
	&seelink(tool:ng_pkg_inst), &seelink(tool:ng_pkg_list), &seelink(srcmgt:ng_smmkpkg)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	05 Mar 2003 (sd) : First crack.
&term	27 May 2003 (sd) : To do a nice install of executing binaries.
&term	02 Jun 2003 (sd) : to allow exclusion of things from an included directory
&term	21 Jun 2003 (sd) : To add -m (allow missing) option, and correct usage string.
&term	01 Jul 2003 (sd) : Added some doc on -s and -n.
			Added ability to glob in the package list on exclusions
			Added remote os type check.
&term	08 Jul 2003 (sd) : Removed rcp and added ftp ability.
&term	12 Sep 2003 (sd) : Added support to pause daemons with the contents of NG_REFURB_PAUSE.
&term	17 Mar 2004 (sd) : Now can pull a rfb- package from repmgr space and install it.
&term	14 Jul 2004 (sd) : Converted many of the log_msg calls to verbose so it is much quieter.
&term	11 Oct 2004 (sd) : added -b (beta) option to pick up beta package type
&term	09 Feb 2005 (sd) : added support for running the INSTALL script it is included in the package.
			Complete revamp to eliminate all of the original 'package and push' 
			functionality.  Updated man page.
&term	17 Feb 2005 (sd) : Corrected problem in making pkg name.
&term	24 Feb 2005 (sd) : Fixed problem with tar not exiting bad when requested file not in archive.
&term	28 Feb 2005 (sd) : Adjusted extract for INSTALL to support tar flavours that do not put ./ on the 
		filename.
&term	11 Jul 2005 (sd) : Now uses $TMP.
&term	29 Nov 2005 (sd) : Fixed uname to force full path usage as ksh or sh has a built-in that does not
		work under SUSE.
&term	16 Feb 2006 (sd) : Uses NG_PKG_MINVER as the default min version number; -M will still override 
		the pinkpages variable. 
&term	21 Fev 2006 (sd) : Accepts a -s option to indicate that stability should NOT be considered when searching
	for a package file in repmgr space. 
&term	27 Apr 2006 (sd) : Corrected error message to make a bit more sense. 
&term	17 Oct 2006 (sd) : Added fix_hashbang to do fixup on #! ksh paths that seem wrong.  Allows packages 
		generated with one NG_ROOT value to be installed on a node that has a different value.
&term	13 Feb 2006 (sd) : Added change to support the output of the file() on the sun. 
&term	20 Feb 2007 (sd) : Fixed sed that was barfing on the sun (trailing whitespace after 1q). 
&term	15 May 2007 (sd) : Added check to verify success of decompress.
&term	20 Jul 2007 (sd) : Changed diagnostic if a scripts #! path seems bad.
&term	16 Aug 2007 (sd) : Updated man page to be more accurate.
&term	30 Jan 2008 (sd) : Added -N option to allow install function to prevent fixup alltogether.
	Corrected the man page.
&term	07 Oct 2008 (sd) : Tweek to allow to work on sun4v.
&term	14 Oct 2009 (sd) : Corrected the directory refrence for fix_hashbang when -u is given on the command line. 
		Added call to ng_fixup to fix flock pointers in lib and bin directories.
&endterms

&scd_end
	
	

