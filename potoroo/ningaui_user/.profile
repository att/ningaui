# WARNING:  
#	This file is maintained as a part of the ningaui src tree.
#	it is installed into the ningaui user home directory with each 
#	restart of ningaui/potoroo processes; changes made to this file 
#	will be lost with the next restart!
# ---------------------------------------------------------------------------------------------

# Modifications: 
#	19 Jun 2004 (bsr) : (Unknown changes)
#	02 Aug 2004 (sd) : Added stuff to support compilation on sun.
#	26 Jul 2005 (bsr) : Added . to CDPATH
#	06 Sep 2006 (sd) : prep for release.
#	19 Nov 2008 (sd) : now can handle environment where no ksh is in initial path
# --------------------------------------------------------------------------------------------

# we must ensure that ksh is in the path in order for ng_wrapper to work. 
# some systems seem not to have ksh installed in any of the usual places and
# our bootstrap into the ningaui world is buggered if we cannot execute ng_wrapper.
for d in /usr/common/ast/bin /usr/local/ast/bin /usr/local/bin/ /usr/bin /bin
do
        if [[ -e $d/ksh ]]
        then
                PATH="$PATH:$d"
                break;
        fi
done

if [[ -x ~/ng_wrapper ]]
then
	NG_ROOT="$( ~/ng_wrapper -r)"
else
	NG_ROOT="$( /ningaui/ng_wrapper -r)"
fi

if [[ ! -d ${NG_ROOT:-dummy} ]]
then
	echo "****** WARNING *******  NG_ROOT is not valid, or could not be determined ($NG_ROOT)"
fi

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}   # get standard configuration file
. $CARTULARY

# Do NOT use the standard functions in a login profile
# STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}        # get standard functions
# . $STDFUN

# --------------------------- history ----------------
who -m|read user tty junk			# assuming they have su'd to this user
if [[ $user != "ningaui" ]]
then
	hf=$TMP/.sh_history.$user
else
	hf=$TMP/.sh_history.${tty##*/}		# linux is a directory/n bsd, is ttypn
fi
export HISTFILE=$hf				# keep concurrent ningaui logins from stomping
export HISTSIZE=1000
trap "/bin/rm -f $HISTFILE" 1 0

# --------------------------- Term -------------------
if test "xx$TERM" = "xx"
then
	# Leave TERM alone if set, otherwise set to dumb
	export TERM=dumb
fi

# --------------------------- Editor -----------------
set -o vi
export EDITOR=vi
export FCEDIT=vi
export CSHEDIT=vi

# --------------------------- Miscellaneous ----------
umask 002
stty erase '^h' kill '^u' intr '^C'
export RUBYOPT=rubygems
export PAGER=cat
export CDPATH=.:$NG_ROOT/site:$HOME
export PS1='${HOST}_${PWD##*/}=; '
export PS2='	'
export MANPATH=/usr/common/man:/usr/common/ast/man:$MANPATH
unalias ls			# turn off colour crap

