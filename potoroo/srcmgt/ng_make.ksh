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


# DO NOT PUT A #! LINE HERE AS WE NEED THIS ON NODES WITHOUT /usr/common/bin/ksh
# Mnemonic:	ng_make
# Abstract:	Wrapper for which ever flavour of make we deem the right one
#		
# Date:		28 April 2005 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------


# must keep some things from being overridden by cartulary if user has them set in the environment
# save now, restore later
osrc=$NG_SRC			
user_DYLD=$DYLD_LIBRARY_PATH
user_LD=$LD_LIBRARY_PATH
user_CC=$GLMK_CC

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions



if [[ $(uname) != "IRIX"* ]]				# bloody irix has issues if we export tons and tons of stuff
then
	if [[ -x ${CARTULARY:-foo} ]]
	then
		. ${CARTULARY:-no.such.beast}
	fi
fi

if [[ -x $STDFUN ]] && whence ng_dt >/dev/null 2>&1
then
	. $STDFUN
else				# some basic functions if running on a non-potoroo box
	function verbose
	{
		if (( $verbose  > 0 ))
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
		rm -f /tmp/PID$$.* /tmp/*.$$ $TMP/*.$$
		exit $1
	}
	
	function ng_dt
	{
		date "+%Y%M%d%H%m%S"
	}

        function ng_tmp_nm
        {
                tmp_nm_seq=$(( ${tmp_nm_seq:-0} + 1 ))
                echo "/tmp/${1:-tmpnm}.$tmp_nm_seq.$$"
        }

fi

#  remove everything but a select few from the environment
#  use this and not wash_env from stdfun; we keep a different set of things.
function scrub_env
{
	typeset rolist=""			# read only list; ksh barfs if we try to unset a readonly var

        readonly | while read vname 			# list of vars that we cannot unset 
        do
                rolist="^${vname%%=*}=|$rolist"
        done

	# we keep only these from being washed away;  
	DRY_CLEAN_ONLY="${rolist}^CVS_RSH=|^DISPLAY=|^HOME=|^LANG=|^DYLD_LIBRARY_PATH|^LD_LIBRARY_PATH=|^LOGNAME=|^MAIL=|^NG_ROOT=|^TMP=|^PATH=|^PS1=|^PWD=|^SHELL=|^TERM=|^USER=|^NG_SRC|^PKG_|^TASK|^GLMK_|^NG_INSTALL_"
	env | egrep -v $DRY_CLEAN_ONLY | while read x
	do
		unset ${x%%=*} 2>/dev/null
	done
}

# --------------------------------------------------------------------------------------------

# restore things that override cartulary from user space
NG_SRC=$osrc
DYLD_LIBRARY_PATH=${user_DYLD:-$DYLD_LIBRARY_PATH}
LD_LIBRARY_PATH=${user_LD:-$LD_LIBRARY_PATH}
GLMK_CC=${user_CC:-$GLMK_CC}



# -------------------------------------------------------------------
if [[ $1 == --man ]]
then
	show_man
	cleanup 1
fi

tool=
use_mk=${NG_USE_MK:-1}		# by default we want mk now
verbose=${GLMK_VERBOSE:-0}

scrub_env		# use ours, not wash_env from stdfun

while [[ $1 == --* ]]
do
	case $1 in 
		--force) 	tool=$2; shift;;
		--verbose)	verbose=1; GLMK_VERBOSE=1;;
		--help)		mk mkfile_help; cleanup 0;;
		--)		shift; break;;
	esac

	shift
done

unset CFLAGS			# dont want these sneaking in from mkfiles that invoke ng_make!
unset IFLAGS

if [[ ! -d ${NG_SRC:-no-such-dir} ]]
then
	echo "NG_SRC is not set or $NG_SRC does not exist"
	cleanup 1
fi

if [[ -d /opt/SUNWspro/bin ]]    # bloody sun os -- these are all to support sun; erk!
then
	PATH="$PATH:/opt/SUNWspro/bin"
fi
if [[ -d /usr/ccs/bin ]]    # bloody sun os -- these are all to support sun; erk!
then
	PATH="$PATH:/usr/ccs/bin"
fi

if [[ -d $NG_SRC/.bin ]]
then
	PATH="$NG_SRC/.bin:$PATH"		# .bin gets stuff during precompile; we want to pick it up first
fi

if [[ -z $tool ]]
then
	if (( use_mk > 0 )) && [[ -f mkfile ]]
	then
		verbose "using: mk in $PWD"
		tool=mk
	else
		verbose "using: nmake in $PWD"
		tool=nmake
	fi
else
	verbose "forced to use: $tool in $PWD"
fi

if [[ $@ == *"$NG_ROOT/bin/"* ]]
then
	aflag="-a"
fi

$tool $aflag "$@"
xc=$?

case $xc in 
	0)	verbose "ng_make: $@ [OK]";;
	*) 	verbose "ng_make: $@ (pwd=$PWD) [FAIL]";;
esac 
cleanup $xc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_make:Wrapper for the make flavour of the month)

&space
&synop	ng_make	[--force tool-name] [--help | --man] [--verbose] args

&space
&desc	&This ensures the correct environment and invokes the preferred
	make tool to build the arguments passed in.   If &ital(--force) 
	is provided, then the next token is assumed to be the make tool
	(make, gmake, nmake, mk) that should be forced. 

&space
&opts	Options and arguments are passed as is to the underlying make that 
	is invoked. If the first argument to &ital(make) starts with &lit(--)
	then a leading &lit(--) must precede it to prevent &this from 
	assuming it is a flag to be processed rather than passed on.  
	All make arguments that begin with 
	a single lead dash (e.g. -M) are safe and do not need the leading 
	double dash.
&space
&lgterms
&term	--force tool : Causes the &ital(tool) to be invoked regardless of whether
	&this thinks it should.  Allows a reversion to an Nmakefile if needed.
&space
&term	--help : Drives mk with the mkfile_help rule.  This produces information 
	with respect to the mk environment and the environment variables that 
	affect it.
&space
&term	--man : Generates the man page for &this.
&space
&term	--verbose : Causes this script to be more verbal than usual.
&endterms

&space
&exit	Whatever make returns.

&space
&see	nmake, gmake, mk

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Apr 2005 (sd) : Sunrise.
&term	16 Jun 2005 (sd) : Added support for --force and mk.
&term	14 Jul 2005 (sd) : Added better diagnostic to verbose message
&term	05 Aug 2005 (sd) : Added --help option and doc.
&term	11 Aug 2005 (sd) : Corrected problem in testing for bad exit code 
		Also to unset variables that trickle in when a mkfile invokes ng_make.
		Added pathing for bloody suns.
&term	18 Oct 2005 (sd) : Added exit to the cleanup function.
&term	28 Dec 2005 (sd) : .bin added to head of the path
&term	30 Dec 2005 (sd) : to test for cartulary and stdfun separately
&term	28 Nov 2006 (sd) : Now the environment is washed and the cartulary is avoided under Irix because 
		of small environmental sizes with that O/S.
&term	29 Mar 2007 (sd) : We now keep NG_INSTALL_ env vars from being washed away.
&term	20 May 2007 (sd) : Now prevents DYLD_LIBRARY_PATH and LD_LIBRARY_PATH from being stompped on by 
		the cartulary settings if user has defined them in their environment.
&term	10 Jul 2007 (sd) : Corrected conflict with stdfun wash_env function. The one here must be 
		different and was renamed.
&term	29 Nov 2007 (sd) : Moved the scrub function below the inclusion of stdfun so as to avoid any 
		problems with duplicate names. 
*&term	13 Mar 2008 (sd) : Forces -a flag on if target is NG_ROOT/bin/*. This is to solve the problem 
		where foo.ksh is checked out, and modified, prior to a build, but the script is not
		installed until after the build has installed. 
&endterms

&scd_end
------------------------------------------------------------------ */
