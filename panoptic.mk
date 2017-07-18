
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


# ----------------------------------------------------------------------------
# Mnemonic:	panoptic.mk
# Abstract:	Global include file for mkfiles (mk) -- sets the 'environment'
# Author:	Andrew Hume/E. Scott Daniels
# Date:		21 June 2005
# Mods:		08 Jul 2005 (sd) : Adjusted setting of IFLAGS
#		21 Jul 2005 (sd) : Change to the HDEPEND stuff to support sun and lack of gcc.
#		05 Aug 2005 (sd) : Added help and doc.
#		19 Aug 2005 (sd) : Added support for all.html
#		31 Aug 2005 (sd) : Added yacc support for backlevel flavours of yacc.
#		12 Jan 2006 (sd) : Formal conversion to give preference to flex.
#		17 Jan 2006 (sd) : GLMK_SHELL support
#		15 Feb 2006 (sd) : Change to SHELL and MKSHELL to skirt bug in ksh under FreeBSD 6.0
#		18 Feb 2006 (sd) : Change which in gmake var setting; now done in panoptic.ksh because
#			which on sgi/sun returns 0 even if not found.  Need which so we can build with 
#			bash if ksh not available.
#		21 Feb 2006 (sd) : Fixed problem assigning KSH caused on everest; some general revamping to 
#			document things better.
#		15 Mar 2006 (sd) : Added ability to use GLMK_AST to set ast back into the picture (for cve).
#		18 Aug 2006 (sd) : Now supports exporting source for a release. 
#		09 Nov 2007 (sd) : Added install support for ruby source. 
#		17 Jan 2008 (sd) : Added support to generate all.html in lib_ruby
#		21 Jan 2008 (sd) : Fixed lex/yacc fixup for suns.  It got oddly trashed when version 1.48 was checked in.
#		04 Dec 2008 (sd) : We now insert the ast include directory before ningaui if GLMK_AST is supplied. 
#		15 Dec 2008 (sd) : Added support for export with mkfile supplied copyright template (GLMK_CR_TEMPLATE)
#		22 Apr 2009 (sd) : Corrected hardcoded ng_; changed to $PKG_PREFIX.
#		30 Jul 2009 (sd) : Changed '%' to './%' in install meta rules. On some linux nodes mk wasn't doing
#			the right thing; rule would trigger but recipe wouldn't execute. 
# 
# 
# Env vars that affect processing:
#	NG_SRC		The path of the source tree 'root'
#	GLMK_PKG_ROOT	The directory name of the 'root' for installation (NG_ROOT used if not defined)
#	GLMK_AST	The directory path where AST lives
#	GLMK_GFLAG	Set to null to turn it off; default is to us -g on compiles
#	GLMK_SHELL	The shell to use; ksh if not defined (needed for windows and initially bsd 6.0)
#	GLMK_CC		The compiler command to use -- gcc is the default and it might be hard overridden in panoptic.ksh!
#	GLMK_EXPORT_ROOT The directory used to export source to when building a release set. If not 
#			defined, then $TMP/$USER/export/ningaui is used. 
#	GLMK_CR_TEMPLATE The fully qualified path of the copyright template that should be supplied to ng_sm_copyright
#			when exporting source.  (see ng_sm_copyright's man page for what the file must contain)
#
# Vars that mkfiles should set before including this:
#	PKG_PREFIX	The prefix added to the front of scripts and binaries as they are made. If not 
#			supplied, ng_ is assumed.
#	INSTALL		The list of things to be installed into PKG_ROOT/bin. These names must include
#			the package prefix (ng_) if the installed copy is to have it. The names must
#			reference targets, or existing files (e.g. ng_foo.ksh or ng_bar where ng_bar is 
#			made from bar.c).
#	LIB_INSTALL	Same rules as INSTALL, target directory is PKG_ROOT/lib
#	ROOT_INSTALL	Same rules as INSTALL, target directory is PGK_ROOT
#	CLASS_INSTALL	Same rules as INSTALL, target directory is PKG_ROOT/classes
#	IFLAGS		Any -I options to pass to the compiler.
#	SCDSRC		List of files that have self contained documentation that is run through
#			an Xfm parser to generate an html manual page.
#	NOPFX_SCDSRC	Like SDCSRC, a list of files that have self contained doc. These files may NOT
#			have a prefix (e.g. ng_) appended to them when they are installed and thus their
#			manpage name should not either. (mostly for lib_ruby files).
# -----------------------------------------------------------------------------------------------------------------

NPROC=1

# 			the next if is written in the positive as sun's sh does not understand if ! something (geeze)
KSH=`if which ksh 2>/dev/null; then :; else echo ksh; fi`  # must have fully qualified path to skirt windows ksh bug (no which on sgi)
MKSHELL = `echo ${GLMK_SHELL:-$KSH}`
mkshell = $MKSHELL				# MKSHELL has issues in recipes (seems always to be sh so we use this)
SHELL=$MKSHELL					# override user's to skirt bug in ksh under FreeBSD

gflag=`print -- ${GLMK_GFLAG:--g}`	
NG_BUILD=$NG_SRC				# deprecated
PATH=$NG_SRC/.bin:$PATH
#which returns 0 even if not found on sgi/sun -- erk.  panoptic.ksh now has code to set this correctly
#gmake=`if which gmake >/dev/null 2>&1; then echo gmake; else echo make; fi`

# -----  various lib names
libng = ng$gflag
libng_noast = ng_noast$gflag
libng_si = ng_si$gflag

# WARNING:
#	print -- "string"   commands are needed in order to build on tiger and maybe windows because echo cannot 
#		deal with string starting with a dash character.

# ----------- 'system' and AST library/header references; SOCK_LIBS set by execution of panoptic.ksh ----------
#   if user has defined GLMK_AST, then we set AST, AST_LIBS, and alter SFIO_LIBS to not include -lsfio
#   we also alter IFLAGS to include the ast header directory.  By default (no GLMK_AST) we include NG_SRC/include/contrib
#   to pick up sfio definitions and set -lsfio as a library. NG_SRC/include/contrib is split so that the stuff in that 
#   lib can be excluded from the -I search when the user wants to use AST.
#
AST = `print -- ${GLMK_AST}`								# by default no ast, but cve insists on using it so give option	
SFIO_LIBS = `if [[ -z $GLMK_AST ]]; then print -- "-L$NG_SRC/stlib -lsfio -lm"; fi`	# turn off the sfio we build if user included ast directly
#AST_LIBS = `if [[ -n $GLMK_AST ]]; then print -- "-L$AST/lib -last -lm -liconv"; fi`	# off by default, but cve insists on using ast libs
AST_LIBS = `if [[ -n $GLMK_AST ]]; then print -- "-L$AST/lib -last -lm \$ICONV"; fi`	# off by default, but cve insists on using ast libs
# 					last ref likely to be added to or otherwise modified by stuff generated by panoptic.ksh!
last_ref = $SFIO_LIBS $AST_LIBS								# this used to be defined in each mkfile, but easier here

# ----- these next few MUST be prior to including output generated by panoptic.ksh
contrib_headers = `if [[ -z $GLMK_AST ]]; then print -- "-I$NG_SRC/include/contrib"; fi`
ast_headers = `if [[ -n $GLMK_AST ]]; then print -- "-I$AST/include/ast"; fi`
IFLAGS = -I. $IFLAGS $ast_headers -I$NG_SRC/include $contrib_headers
LFLAGS=$LFLAGS -L. -L$NG_BUILD/stlib -l$libng $last_ref

# ------- these defaults may be overridden in output from panopitc.ksh
CC = `echo ${GLMK_CC:-gcc}`
JAVAC = javac

# ---- generate o/s and hardware specific vars (CFLAGS etc.) then suck them in
dummy=`ksh $NG_SRC/panoptic.ksh >__panoptic.mk`
<__panoptic.mk

# ------ pkg and install related vars
PKG_ROOT = `echo ${GLMK_PKG_ROOT:-$NG_ROOT}`
PKG_BIN = $PKG_ROOT/bin
PKG_LIB = $PKG_ROOT/lib
PKG_PREFIX = `echo ${PKG_PREFIX:-ng_}`

# ------ must expand into vars used as dependancies to avoid issues when *INSTALL is null
INSTALL=`for x in ${INSTALL}; do printf "$PKG_ROOT/bin/$x "; done`
ROOT_INSTALL=`for x in ${ROOT_INSTALL}; do printf "$PKG_ROOT/$x "; done`
LIB_INSTALL=`for x in ${LIB_INSTALL}; do printf "$PKG_ROOT/lib/$x "; done`
CLASS_INSTALL=`for x in ${CLASS_INSTALL}; do printf "$PKG_ROOT/classes/$x "; done`

# ------  similar to install, these must be set to NONE if not defined
SCDSRC=`echo ${SCDSRC:-NONE}`

NUKE=`echo ${NUKE:-*.o *.man *.html}`

# ------------------ convenience rules; users may add their own in mkfiles that will APPEND to these
#	to allow user's to add things with the same named rule, these rules have no recipe; thus
#	precompile depends on a local setup rule, and nuke on a local nuke rule.
all:NVQ: HDEPEND $ALL

precompile:QV: hdepend_setup

nuke:V: panoptic_nuke

install:QV: $INSTALL $ROOT_INSTALL $LIB_INSTALL $CLASS_INSTALL

panoptic_nuke:V:
	>HDEPEND
	>HDEPEND_NAMES
	rm -fr D.mk/* || true
	rm -fr $NUKE || true

mkfile_help:VQ:
	cat <<endKat
	CURRENT VARIABLE SETTINGS:
		NG_ROOT		$NG_ROOT
		PKG_ROOT	$PKG_ROOT
		NG_SRC		$NG_SRC
		PKG_PREFIX	$PKG_PREFIX
		CC		$CC
		CFLAGS		$CFLAGS
		IFLAGS		$IFLAGS
		MKSHELL 	$mkshell	
		AST		$AST
		AST_LIBS	$AST_LIBS
		contrib_headers  $contrib_headers
		gflag		$gflag
		gmake		$gmake
	
		GLMK_SHELL	$GLMK_SHELL
		GLMK_VERBOSE	$GLMK_VERBOSE
		GLMK_CC		$GLMK_CC
	
	----------------------------------------------------------------------------
	NOTES:
	 Env vars that affect processing:
		NG_SRC		The path of the source tree 'root'
		GLMK_PKG_ROOT	The directory name of the 'root' for installation (NG_ROOT used if not defined)
		GLMK_AST	The directory path where AST lives
		GLMK_GFLAG	Set to null to turn it off; default is to us -g on compiles
		GLMK_VERBOSE	May cause more information to be generated by scripts like ng_make
		GLMK_SHELL	The shell that is ueed to execute recipies.  Defaults to ksh if not set.
	
	 Vars that mkfiles should set before including panoptic.mk:
		INSTALL		The list of things to be installed into PKG_ROOT/bin. These names must include
				the package prefix (ng_) if the installed copy is to have it. The names must
				reference targets, or existing files (e.g. ng_foo.ksh or ng_bar where ng_bar is 
				made from bar.c).
		LIB_INSTALL	Same rules as INSTALL, target directory is PKG_ROOT/lib
		ROOT_INSTALL	Same rules as INSTALL, target directory is PGK_ROOT
		CLASS_INSTALL	Same rules as INSTALL, target directory is PKG_ROOT/classes
		CFLAGS		Any options (other than -I) to pass to the compiler.
		IFLAGS		Any -I options to pass to the compiler.
		LFLAGS		Any linker options.
	
				building a software release (we do not export the whole CVS tree!)
				Copyright notices are added to each file exported.
				notices. 
				The mk export command will be run in each directory.
				export: rule. 
	
	If *FLAGS are set before including panoptic.mk, the user setting will have the standard 'things'
	added to it.  If the user wants to completely override any of these variables, it should be assigned
	a value AFTER panoptic.mk is included. 
	
	endKat

# ------------------ install metas  -----------------------------------
$PKG_BIN/$PKG_PREFIX%:	%.sh
	ng_install $prereq $target

$PKG_BIN/$PKG_PREFIX%:	%.ksh
	ng_install $prereq $target

$PKG_BIN/$PKG_PREFIX%:	%.rb
	ng_install $prereq $target

$PKG_BIN/%:	%.sh
	ng_install $prereq $target

$PKG_BIN/%:	%.ksh
	ng_install $prereq $target

$PKG_BIN/$PKG_PREFIX%:	%.py
	ng_install $prereq $target

$PKG_BIN/%:	%.py
	ng_install $prereq $target

$PKG_BIN/%.rb:	%.rb
	ng_install $prereq $target

$PKG_BIN/%:	%.rb
	ng_install $prereq $target

#  ---- this causes ambig recipies 
#$PKG_BIN/$PKG_PREFIX%:  %
#	ng_install $prereq $target

# the ./ is needed on some odd versions of linux or the recipe isn't executed
$PKG_BIN/%:  ./%
	ng_install $prereq $target

 $PKG_LIB/$PKG_PREFIX%.rb:     %.rb
	ng_install $prereq $target

$PKG_LIB/$PKG_PREFIX%:	%.sh
	ng_install $prereq $target

$PKG_LIB/$PKG_PREFIX%:	%.ksh
	ng_install $prereq $target

$PKG_LIB/%:	%.sh
	ng_install $prereq $target

$PKG_LIB/%:	%.ksh
	ng_install $prereq $target

$PKG_LIB/%:	./%
	ng_install $prereq $target

# the & meta character does not match / and this IS important!
&:	&.o
	$CC $IFLAGS $CFLAGS -o $target $prereq $lib_ref $last_ref

$PKG_PREFIX&:	&.o
	$CC $IFLAGS $CFLAGS -o $target $prereq $lib_ref $last_ref

&.o:	&.c &.man
	$CC $IFLAGS $CFLAGS -c $stem.c	
	echo $stem.c >>HDEPEND_NAMES

# ---------------- yacc -------------------------------
&.o:D:	&.l
	set -x
	whence $lex
	if [[ $lex == flex ]]
	then
		$lex  -o$stem.c  $prereq
	else
		$lex  $prereq
		mv lex.yy.c $stem.c
	fi
	
	if [[ $(uname) == SunOS ]]				# why sun cannot keep up; it drives me batty
	then
		mv $stem.c $stem.cx
		echo "*** panoptic.mk ***  had to fix $stem.c so that yyin and yyout were set to null because lex and suns compiler dont work together"
		#sed 's/^FILE[ \t].*yyin.*/FILE *yyin=0; FILE *yyout=0;/' <$stem.cx >>$stem.c||rm $stem.c	# fix so suns cc does not barf
		sed 's/FILE[ \t].yyin[ \t]=[ \t]{stdin},[ \t].yyout[ \t]=[ \t]{stdout};/FILE *yyin=0; FILE *yyout=0;/' <$stem.cx >>$stem.c||rm $stem.c	# fix so suns cc does not barf
	fi
	
	$CC $IFLAGS $CFLAGS -c $stem.c||(cp $stem.c $stem.keep.c; rm -f $stem.c; false)	# must remove if we fail or we have ambig rules
	cp $stem.c $stem.keep.c
	rm -f $stem.c

#		for xgram.y it generates xgram.h and xgram.o (xgram.c is an intermed file that is deleted)
&.o:D:	&.y
	if (( $POMK_NEW_YACC > 0 ))
	then
		yacc -d -o$stem.c  $prereq
	else
		yacc -d  $prereq
		mv y.tab.c $stem.c
		if      test -s y.tab.h
		then    ed - y.tab.h <<'!'
	1i
	#ifndef _LANG_H
	#define _LANG_H
	.
	$a
	#endif /* _LANG_H */
	.
	w
	q
	!
			if      cmp -s y.tab.h $stem.h
			then    
				rm -f y.tab.h
			else    
				mv y.tab.h $stem.h
			fi
		fi
		if      test -f y.output
		then    
			mv y.output $stem.grammar
	
		fi
	fi
	
	$CC  $IFLAGS $CFLAGS -c $stem.c||(cp $stem.c $stem.keep.c; rm -f $stem.c; false)	# fail but after removing output
	cp $stem.c $stem.keep.c
	rm -f $stem.c						# must remove or a subsequent make has an ambig rule

# ------- man page stuff -------------------------------

HTML_DIR = $NG_SRC/manuals/html
TXT_DIR = $NG_SRC/manuals/text
LIST_DIR = $NG_SRC/manuals/list

XFM_IMBED = $NG_SRC/include			# this is used by scd_start.im to find other imbed files
hpath = HFM_PATH=.:$XFM_IMBED; export HFM_PATH

X = ${SCDSRC:$PKG_PREFIX%=%} 			# strip leading ng_ type of things
HTML_ALL_DEPEND = ${X:%=$PKG_PREFIX%.html} ${NOPFX_SCDSRC:%=%.html}	# add ng_ (so we get just one, but skip no prefix list) and .html
TXT_ALL_DEPEND = ${X:%=$PKG_PREFIX%.txt}	# add ng_ (so we get just one) and .txt

ng_NONE.list:Q: 
	echo 	"no defined files to list ... this is ok" >/dev/null

ng_NONE.html:Q: 
	echo 	"no defined sdcsrc files so we will not generate any .html files" >/dev/null

# this happens if NOPFX_SDCSRC is empty; it is ok, but issue the message 
NOPFX_SCDSRC:Q: 
	echo 	"NOPFX_SDCSRC empty/undefined [OK]" >/dev/null

%.html:	%.xfm
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.html:	%.im
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.html:	%.sh
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.html:	%.ksh
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.html:	%.rb
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.html:	%.c
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.xfm
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.im
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.sh
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.ksh
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.rb
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.html:	%.c
	 XFM_IMBED=$XFM_IMBED hfm $prereq   $target  .im $XFM_IMBED/scd_start.im

ng_NONE.txt:Q: 
	echo 	"no defined sdcsrc files so we will not generate any .txt files" >/dev/null

%.txt:	%.xfm
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.im
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.sh
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.ksh
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.sh
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.rb
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

%.txt:	%.c
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.xfm
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.im
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.sh
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.ksh
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.rb
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

$PKG_PREFIX%.txt:	%.c
	 XFM_IMBED=$XFM_IMBED tfm $prereq   $target  .im $XFM_IMBED/scd_start.im

# build html man pages and install into the web directory
all.html:QV:	$HTML_ALL_DEPEND
	if [[ ! -d $HTML_DIR ]]
	then
		mkdir -p $HTML_DIR
	fi
	if [[ -d $HTML_DIR ]]
	then
		for f in $HTML_ALL_DEPEND
		do
			# this is triggered because we have to have something assigned to SCDSRC= and NOPFX_SCDSRC=  (mk feature?)
			if [[ $f != ng_NONE.html && $f != *SCDSRC* ]]		
			then
				echo mv $f $HTML_DIR/$htmlcat.$f
				mv $f $HTML_DIR/$htmlcat.$f 
			fi
		done
	fi

all.txt:QV:	$TXT_ALL_DEPEND
	if [[ ! -d $TXT_DIR ]]
	then
		mkdir -p $TXT_DIR
	fi
	if [[ -d $TXT_DIR ]]
	then
		for f in $TXT_ALL_DEPEND
		do
			if [[ $f != ng_NONE.txt ]]		# this is triggered because we have to have something on SCDSRC= (mk feature?)
			then
				echo mv $f $TXT_DIR/$htmlcat.$f
				mv $f $TXT_DIR/$htmlcat.$f 
			fi
		done
	fi

# -------------- generating x.man from x.c --------------------------------
# in order for this to work properly, x.man x.o  is the prereq order that must be defined.
# we MUST reference from the tool directory or we cannot run on a non-standard (no common/ast/bin/ksh) node.
makeman = $NG_SRC/potoroo/tool/make_man.ksh
&.man : &.c
	$makeman $stem.c $target

# ----------------- header file dependency things  -----------------------------
__dummy=`ksh -c "if [[ ! -f HDEPEND ]]; then touch HDEPEND; touch HDEPEND_NAMES; fi"`
__dummy=`ksh -c "if [[ ! -d D.mk ]]; then mkdir D.mk 2>/dev/null; fi"`
hdepend_setup:QV:
	>HDEPEND
	sleep 1
	>HDEPEND_NAMES
	mkdir -p D.mk 2>/dev/null
	rm -f D.mk/*

HNAMES=`sort -u HDEPEND_NAMES`
HNAMES=${HNAMES:%=D.mk/%}
HDEPEND:Q:	$HNAMES HDEPEND_NAMES
	if [[ -n $HNAMES  && $CC == gcc ]]
	then
		echo "HNAMES=${HNAMES%D.mk/}"
		cat ${HNAMES%D.mk/} /dev/null | sed 's/^#.*//' > $target
	else
		touch $target
	fi

D.mk/stest_%: %

D.mk/%:DQ:	%
	if [[ $CC != gcc ]]		# gcc not on our sun
	then
		exit 0
	else
		$CC $IFLAGS $CFLAGS  -M $stem > $target
	fi

<HDEPEND

# --------------------------------------------------------------------------
