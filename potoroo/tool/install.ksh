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


# DO NOT PUT A # !shellname line here!  It buggers up compilation on systems
#  where potoroo does not exist yet!


# Mnemonic: ng_install
# Author:   Scott Daniels
# Date:		Oct 1997 
# ------------------------------------------------------------------------


if [[ ! -d ${NG_ROOT:-foo}/bin ]]
then
	echo "unable to verify that NG_ROOT (${NG_ROOT:-variable not set}) is correct"
	exit 1
fi

if [[ -f $NG_ROOT/lib/stdfun ]]
then
	. $NG_ROOT/lib/stdfun
else
	if [[ -f $NG_SRC/potoroo/lib/stdfun.ksh ]]
	then 
		. $NG_SRC/potoroo/lib/stdfun.ksh 
	else
		echo "unable to find a usable copy of stdfun -- aborting"
		exit 100
	fi
fi

if [[ -f $NG_ROOT/cartulary ]]
then
	. $NG_ROOT/cartulary 
fi

PATH="/bin:/usr/bin:$PATH"	# use non-ast things so that we can install ast things

# ------------------------------------------------------------------------------------------
# look at a script and fix the #! if it does not seem to reference an existing interpreter
# and we can figure out something to set it to
# do NOT invoke ng_fix_hasbang -- it invokes this script!
# this is invoked only for ksh scripts
function fix_hashbang
{
	typeset t1=""
	typeset t2=""
	typeset t3=""
	typeset path=""
	typeset sname=""
	typeset def_path=""

	if [[ $(file $1) == *"script"* ]]
	then
		head -1 $1 | read t1 t2 t3		# allow #!/path  or #! /path
		path=${t1#??}
		if [[ -z $path ]]
		then
			path=$t2
		else
			t3=$t2				# consistant reference for what might be the script name (#!/usr/bin/env ksh)
		fi

		if [[ ! -x $path ]]
		then
			verbose "#! path in script seems wrong: $path"

			sname=${path##*/}
			if [[ $sname == "env" ]]		#!/usr/bin/env ksh probably used, and path is not right here
			then
				sname=$t3
			fi
			eval typeset -u vname="NG_INSTALL_${sname}_PATH"
			eval def_path=\$$vname
			if [[ -n $def_path ]]
			then
				verbose "added hashbang: #!$def_path"
				echo "#!$def_path" >$1.new
			else
				whence env|read def_path
				verbose "added hashbang: #!$def_path $sname"
				echo "#!$def_path $sname" >$1.new
			fi

			cat $1 >>$1.new
			mv $1.new $1
		fi
	fi
}

# ------------------------------------------------------------------------------------------

add_ng=""
verbose=0
just_kidding=0
mode=775			# default file mode 
ustring="[-g] [-m mode] [-n] [-p] [-v] src-file [destination]"

while [[ $1 == -* ]]
do
	case $1 in 
		-g) 	add_ng="ng_" ;;
		-n) 	just_kidding=1;;
		-p)	cpopts="$cpopts-p ";;
		-m)	mode=$2; shift;;
		-v) 	verbose=1 ;;
	
		-\?)	usage; exit 1;;
		--man)	show_man; exit 1;;
		*) 	usage
			exit 1 
			;;
	esac

	shift
done

if [[ ! -f $1 ]]
then
	echo "$argv0: giving up: cannot find file to install: $1"
	cleanup 1
fi

if [[ -n $3 ]]			# left over from gecko -- must support even tho not documented
then
	mode=$3
fi


dname=${1##*/}
if [[ -n $add_ng && $dname != ng_* ]]
then
	dname="ng_$dname"
fi

if [[ -z $2 ]]   # no dest - default to /NG_ROOT/bin
then
	dest=$NG_ROOT/bin/${dname%.ksh}
else
	if [[ -d $2 ]]         # user supplied directory - copy dname
	then
		dest=$2/${dname%.ksh}
	else
		dest=$2                         # user supplied full name
	fi
fi

if [[ $just_kidding -gt 0 ]]
then
	echo "would install $1 in $dest"
	exit 1
fi

ddir=${dest%/*}
if [[ ! -d $ddir ]] 
then
	if ! mkdir -p $ddir
	then
		echo "could not make directory: $ddir"
	fi
fi

if [[ -e /usr/bin/env ]]
then
	default_ksh_path="/usr/bin/env ksh"
else
	if [[ -e $NG_ROOT/bin/ksh ]]
	then
		default_ksh_path=$NG_ROOT/bin/ksh
	else
		if [[ -e /usr/common/ast/bin/ksh ]]
		then
			default_ksh_path=/usr/common/ast/bin/ksh
		fi
	fi
fi

ksh_path="${NG_INSTALL_KSH_PATH:-$default_ksh_path}"   # if set to where it really lives, use it 

if [[ ! -w $ddir ]]		# if we cannot write to the directory, then we must just overwrite the file and hope for the best
then
	log_msg "directory is not writable, overwriting and hoping for the best"
	if [[ "${1##*.}" = "ksh" ]] 	         # if src file has shell script extension
	then	                                # eliminate the need for /ningaui on other machines
        	echo "#!$ksh_path" >$dest
        	if ! cat $1 >>$dest
		then
			log_msg "cannot install: $1 -> $dest"
			cleanup 1
		fi
	else
		if ! cp $cpopts $1 $dest		  
		then
			log_msg "cannot install: $1 -> $dest"
			cleanup 1
		fi
	fi

	chmod ${mode:-775} $dest 2>/dev/null		# these probably fail, ignore messages
	chgrp $NG_PROD_ID $dest 2>/dev/null
	ls -l $dest

	cleanup 0
fi

if [[ "${1##*.}" == "ksh" ]] 	         # if src file has shell script extension
then	                                # eliminate the need for /ningaui on other machines
        echo "#!$ksh_path" >$dest.xx
        if ! cat $1 >>$dest.xx
	then
		log_msg "cannot install: $1 -> $dest"
		cleanup 1
	fi

	fix_hashbang $dest.xx				# need it to ensure it is right when we build exported stuff
else
	if ! cp $cpopts $1 $dest.xx 2>/tmp/err.$$ 	# remove if scd is ever stripped out (above if uncommented)
	then
		if [[ $cpopts = *-p* ]]			# bloody ast/cp complains if owner/grp cannot be set; if that was all then its success
		then
			md5sum $dest.xx|read mn junk		# if md5s match, then we assume it was successful
			md5sum $1|read mo junk
			if [[ $mo = "$mn" ]] && egrep "owner|group" /tmp/err.$$ >/dev/null
			then	
				log_msg "ignoring cp return code -- likely owner/group issue on preserve"
			else
				cat /tmp/err.$$
				log_msg "cannot install: md5 mismatch after copy $1 -> $dest.xx" 
				cleanup 2
			fi
		else
			log_msg "cannot install: $1 -> $dest"
			cleanup 1
		fi

	fi

	fix_hashbang $dest.xx
fi

# make it executable before we get into critical window
chmod ${mode:-775} $dest.xx
chgrp $NG_PROD_ID $dest.xx

# first part is sneaky; do not disturb current executions
# these two moves must be adjacent (minimise error window)
mv -f $dest $dest.$$ >/dev/null 2>&1		# this might fail becuse $dest is not there; and that is ok
if ! mv $dest.xx $dest
then
	log_msg "could not install: $dest.xx -> $dest"
	mv $dest.$$ $dest				# mv fails on darwin when the system is loaded (exec error it says)
	rm -f $dest.$$					# so we must ensure *.$$ is gone
	rm -f $dest.xx					# and *.xx too
	cleanup 1
fi

rm -f $dest.$$ >/dev/null 2>&1
ls -l $dest

cleanup 0
exit 0

exit  # should never get here, but is required for SCD

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_install:Install A Ningaui File)
&name &utitle

&space
&synop	ng_install [-g] [-p] source-filename [destination]

&space
&desc	&This installs the &ital(source-filename) into 
	the file or directory specified as &ital(desitination.) 
	If &ital(desitnation) is not supplied on the command 
	line, then &lit($NG_ROOT/bin) is assumed. Whenever a directory
	is supplied as the desination, or no destination 
	is supplied, then the basename of the &ital(source-filename) is used
	as the destination filename.
	If the source filename has a .ksh extension, then the extension is 
	automatically removed, and an appropriate &lit(#!) line is added to 
	the top of the script.
&space
	If the file does not have a &lit(.ksh) suffix, and 
	&lit(file) command reports it to be a &ital(script,)
	then the first line of the script is examined for a hash-bang (#!) 
	string. The path and executiable name that follows the hash-bang
	is verified.  If it does not reference an executable file, then 
	the string is replaced with the value of a pinkpages variable 
	that has the form: &lit(NG_INSTALL_)&ital(name)&lit(_PATH)
	where &ital(name) is the uppercase interpreter (e.g. KSH) name. 
&space
	If the pinkpages variable is not defined, an &lit(env) command 
	will be used which will be similar to:
&space
	#!/usr/bin/env ksh

&space
	As a part of the installation the mode of the file will be 
	set to &lit(0755) and the group will be set to &lit($NG_PROD_ID.)
	The &ital(source-filename) does not need to be executable nor 
	does it need to be owned by the &lit($NG_PROD_ID) user id. 
&space
&opts
	These options can be placed on the command line in front of 
	any positional parameters:
&space
&begterms
&term	-p : Preserve. An attempt is made to preserve the date, time 
	and owner of the source file as it is installed.  
&space
&term	-g : Add &lit(ng_) to the basename before installing. This 
	option is recognised only if the second paramter is omitted.
	No check on the basename is made, so if the file already has 
	the &lit(ng_) characters, a second set will be added. 
&endterms

&space
&parms  The following parameters are recognised when entered on 
	the command line. The parameters must be entered in the order
	listed below:
&space
&begterms
&term	source-filename : The name of the source file to be installed.
&space
&term	destination : The name of the destination file or destination 
	directory. If omitted, &lit($NG_ROOT/bin) is assumed.
&endterms

&space
&examp	To install the script ng_aardvark.ksh into the &lit($NG_ROOT/bin)
	directory one of the following commands can be used:
&space
&litblkb
	ng_install ng_aardvark.ksh /ningaui/bin/ng_aardvark
	ng_install ng_aardvark.ksh /ningaui/bin
	ng_install ng_aardvark.ksh
&litblke
&space
	To install .config into the /ningaui directory one of the following 
	commands can be used:
&litblkb
	ng_install .config /ningaui/.config
	ng_install .config /ningaui
&litblke


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Jul 1999 (sd) : Added scd and made destination optional.
&term	09 May 2001 (sd) : converted from Gecko.
&term	10 May 2001 (sd) : Added check to ensure src file exists.
&term	15 Jun 2001 (sd) : now adds #!$NG_ROOT/bin/ksh to the top of each script
&term	27 Jun 2002 (sd) : to allow writing to dir that is not writable but the dest file is.
&term	03 Jul 2002 (sd) : To adjust for <$1 KSH difference
&term	15 Jul 2002 (sd) : To skirt the 'override protection' prompt if what we are installing is running.
&term	27 Jan 2004 (sd) : Added tests to detect and report installation failures.
&term	29 Mar 2004 (sd) : Added -p option to pass -p to cp for lib installs on mac.
&term	30 Mar 2004 (sd) : Added error checking when -p is set. Seems AST/cp complains about owner/group setting
	with -p option where /bin/cp does not.  Erk.
&term	19 Aug 2004 (sd) : Added -m option to set mode; cleaned up internal formatting.
&term	13 Sep 2004 (sd) : Added /bin /usr/bin to path FIRST so that we can use this to install ast things.
&term	07 Dec 2005 (sd) : Allows path of kshell to be supplied at install time via pinkpages NG_INSTALL_KSH_PATH var.
&term	31 Mar 2006 (sd) : verifies NG_ROOT and aborts if not good. 
&term	14 Aug 2006 (sd) : Prevents *.$$ from being left if mv fails (common, it seems, on darwin).
&term	10 Oct 2006 (sd) : Defaults to using $NG_ROOT/bin/ksh as the shell path if it exists.
&term	23 Feb 2007 (sd) : Sets #! in scripts if not correct. 
&term	29 Mar 2007 (sd) : Calls fix_hashbang even when the source has a .ksh; needed for installation on new nodes by 
		the bootstrap_build script.
&term	15 Oct 2007 (sd) : Mod to allow the production user id to come from NG_PROD_ID.
&term	20 Feb 2008 (sd) : Allows /usr/bin/env ksh as a legit hash bang string.
&term	24 Sep 2008 (sd) : Change to default to /usr/bin/env ksh rather than $NG_ROOT/bin/ksh so as to avoid issues on 
		suns with their shortish amount of ps info. 
&endterms
&scd_end
