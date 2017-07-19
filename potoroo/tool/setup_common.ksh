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
# Mnemonic:	setup_common.ksh
# Abstract:	build the $NG_ROOT/common directory; required on nodes where we do not 
#		control /usr/common and must limit what we find in /usr/common/ast/bin.
#		
# Date:		16 Jan 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

# do NOT use cartulary or stdfun as this may run before we are installed
# we will pull in cart.min and site/cart.min if they exist; allows us to 
# find an 'alternate' /usr/common. 
#
#CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
#. $CARTULARY

#STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
#. $STDFUN

if [[ -e $NG_ROOT/lib/cartulary.min ]]
then
	. $NG_ROOT/lib/cartulary.min
fi
if [[ -e $NG_ROOT/site/cartulary.min ]]
then
	. $NG_ROOT/site/cartulary.min
fi

function cleanup
{
	rm -f $TMP/*.$$
	exit ${1:-0}
}

function log_msg
{
	echo "$(date) $@" >&2
}

function verbose
{
	if (( $verbose > 0 ))
	then
		echo "$(date) $@" >&2
	fi
}

function switch2
{
	if ! cd $1 
	then
		log_msg "unable to switch to directory: $1"
		cleanup 1
	fi
}

function ensure
{
	while [[ -n $1 ]]
	do
		if [[ ! -d $1 ]]
		then
			log_msg "creating directory: $1"
			if ! mkdir -p $1
			then
				log_msg "unable to make directory: $1"
				cleanup 1
			fi
		fi

		shift
	done
}

# remove the named thing(s) if they are a link.  Allows hard files to be put in 
# as a stopgap or somesuch -- we leave them alone
function rmlink
{
	while [[ -n $1 ]]
	do
		if [[ -h $1 ]]			# if it is a link, remove it to ensure it is pointed at the currently desired spot
		then
			log_msg "removed link: $PWD/$1"
			rm $1
		else
			if [[ -e $1 ]]
			then
				log_msg "not a link, not removed by rmlink: $1"
			fi
		fi

		shift
	done
}

# create links $1=real-directory (/usr/common/ast/bin)  $2-n filenames
#	file names may be either   foo   or foo:bar.  If foo:bar, then 
#	the link is set up such that PWD/bar -> target_dir/foo
#	this allows us to set up things like gawk:awk  
#
#	IMPORTANT: links are created in the current directory 
function link2
{
	typeset tf=""		# target from foo:bar
	typeset lf=""		# link from foo:bar
	typeset tdir="$1"	# target directory
	shift

	while [[ -n $1 ]]
	do	
		lf=${1##*:}			#  from a:b  a is the target file and b is the symlink name
		tf=${1%%:*}			#  target file (a)
		
		if [[ -h $lf ]]			# if it is a link, remove it to ensure it is pointed at the currently desired spot
		then
			rm $lf
		fi

		if [[ ! -e $lf ]]		# dont try if someone put a real file in here
		then
			if [[ ! -e $tdir/$tf ]]
			then
				log_msg "could not set link $PWD/$lf: target ($tdir/$tf) does not exist [ERROR]"
				ng_log $PRI_ERROR $argv0 "could not find target, unable to set $NG_ROOT/common link: $lf -> $tdir/$tf"
			else
				log_msg "creating link: $PWD/$lf -> $tdir/$tf"
				if ! ln -s $tdir/$tf $lf
				then
					log_msg "ln command failed for: $tdir/$tf [FAIL]"
					ng_log $PRI_ERROR $argv0 "hard failure setting link: $lf -> $tdir/$tf"
					cleanup 1
				fi
			fi
		else
			log_msg "could not set link $PWD/$lf -> $tdir/$tf: a file or symlink in $PWD already exists [WARN]"
		fi

		shift
	done
}

#	find $2-$N in the path ($1) and set their links in the current directory
# 	we have the need on sgis to use tools from places like /usr/common/bin but we do not 
#	want the directory in the search path, so we allow links to be set to the first
#	executable copy of $n that we find in the path ($1)
function link2path
{
	typeset d=""
	typeset p=$1;
	typeset created=0
	shift

	while [[ -n $1 ]]
	do
		created=0
		for d in ${p//:/ }
		do
			if [[ -x $d/${1%%:*} ]]
			then
				link2 $d $1
				created=1
				break
			fi
		done

		if (( $created == 0 ))
		then
			log_msg "unable to create link for $1; not found in path: $p"
		fi
		shift
	done
}

# -------------------------------------------------------------------
TMP=/tmp

ast_path=${NG_AST_PATH:-/usr/common/ast}

while [[ $1 == -* ]]
do
	case $1 in 

		--man)
			# this will not work until they install 
			if whence tfm >/dev/null 2>&1
			then
				(export XFM_IMBED=$NG_ROOT/lib; tfm $0 stdout .im $XFM_IMBED/scd_start.im 2>/dev/null | ${PAGER:-more})
			else
				echo "ningaui must be installed for the --man option to function"
			fi
	
			exit 1
			;;

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

if [[ ! -d $NG_ROOT ]]
then
	log_msg "NG_ROOT directory ($NG_ROOT) does not exist!"
	cleanup 1
fi


ensure $NG_ROOT/common
ensure $NG_ROOT/common/bin
ensure $NG_ROOT/common/ast
ensure $NG_ROOT/common/ast/bin

switch2 $NG_ROOT/common/ast/bin

rmlink $(ls)					# blow any symlink away
#rmlink ksh nmake md5sum pax pzip		# if they were links, blow them away


if [[ -d $ast_path/bin ]]		# yes virginia, some systems seem not to have usr/common
then
	log_msg "looking for AST tools in $ast_path/bin"
	link2 $ast_path/bin ksh pax pzip

	switch2 $NG_ROOT/common/ast
	link2 $ast_path lib include
else
	verbose "WARNING: path to AST tools is not valid: $ast_path. Ensure that NG_ROOT/site/cartulary.min has a proper value for NG_AST_PATH."
fi

# get things that are not in a standard place, and from dirs we do not want in the path
# first parm is a set of paths to search for the parms 2-n.
# if the tool named is gawk:awk  we will create awk -> gawk.  


switch2 $NG_ROOT/common/bin
rmlink $(ls)					# blow any symlink away
#rmlink ruby md5sum ng_seq groff			# do removes first

link2 $NG_ROOT/bin ng_md5:md5sum		# for references to old ast version 
link2	$NG_ROOT/bin ng_seq:seq

# nawk does not do it anymore for some scripts, so we refuse to point to it. 
link2path /usr/common/bin:/usr/bin:/usr/local/bin gawk gawk:awk  
link2path /usr/bin:/usr/common/bin:/bin nroff
link2path /usr/common/bin:/usr/bin:/usr/local/bin ruby

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_setup_common:Ensure NG_ROOT/common is setup)

&space
&synop	ng_setup_common

&space
&desc	&This will ensure that the directory &lit($NG_ROOT/common) is 
	created and has the proper AST links.  This allows ningaui 
	to reference a subset of AST tools when the AST pacakge 
	cannot be modified in &lit(/usr/common). Directories other than &lit(ast)
	may well exist in $(NG_ROOT/common), but those are not verified or 
	created by this script.
&space
	The following directories are setup by this script:
&beglist
&item	$NG_ROOT/common
&item	$NG_ROOT/common/ast/bin
&item	$NG_ROOT/common/ast/lib
&item	$NG_ROOT/common/ast/include
&endlist
&space
	The bin directory contains symbolic links to the ast binaries that we do 
	need to use. The lib and include entries are symbolic links to the 
	lib and include directories in &lit(/usr/common/ast.)
&space
	The &lit(PATH) variable for the Ningaui user id should reference 
	the bin directory here rather than in &lit(/usr/common/ast.)

&space
&exit	An exit code that is not zero (0)  indicates a failure. 

&space
&see	ng_init, ng_node, ng_setup

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Jan 2006 (sd) : Sunrise.
&term	18 Jan 2006 (sd) : To adjust for windoze.
&term	17 May 2005 (sd) : Switched path search order for things like awk. Now searches /usr/common/bin first. 
&term	22 Jun 2006 (sd) : Changed groff to nroff.
&term	30 Aug 2006 (sd) : Path to ast directory now can come from site/cartulary.min (NG_AST_PATH) if not the default
		of /usr/common/ast. 
&term	18 Sep 2006 (sd) : Now links md5sum in ast/bin to ng_md5sum.
&term	19 Sep 2006 (sd) : We no longer set a link to nmake.
&term	16 Oct 2006 (sd) : Set link to seq
&term	02 Jan 2007 (sd) : Moved link for md5sum to NG_ROOT/common rather than common/ast.
&term	09 Nov 2007 (sd) : Added links for ruby
&term	26 Nov 2007 (sd) : Removed setting link for python.
&term	30 Jul 2008 (sd) : Added nawk:awk pair so that this will work on suns with bad awk and no gawk.  .** HBD AKD
&term	05 Aug 2008 (sd) : Corrected nawk:awk in the list so that it will work on bad suns. 
&term	22 Sep 2008 (sd) : We now blow all symlinks away before starting work in a directory.  This used to trash
		only links for the things we care about which was leaving old symlinks hanging round. 
		Also, now logs a link failure to the master log.
&term	25 Nov 2008 (sd) : Added verbose function. 
&endterms

&scd_end
------------------------------------------------------------------ */
