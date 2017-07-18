
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


#
# 	Top level mkfile for ningaui.  To build on a virgin system (no 
#	ningaui software installed) best to execute ng_build.ksh with 
#	the -a and -v options (create development environment and build
#	everything). 
#	
#	If you want to mk things by hand then run ng_build.ksh without the 
#	-a option to set up the build environment.  Then follow the directions
#	that ng_build.ksh writes to stderr when it has finished. 
#
#
#	Mod:	02 Dec 2004 (sd) : First cut.
#		07 Jul 2005 (sd) : Converted to mkfile
#		14 Nov 1005 (sd) : we no longer hit the apps directory as Tiger cannot 
#			handle nmake and most of the apps stuff is still not converted to mk
#		28 Dec 2005 (sd0 : Removed mlink/ebub from main stream.
#		16 Dec 2006 (sd) : Added Install directive for building on non-potoroo node
#		26 Dec 2006 (sd) : Inlcuded the .im files in include/ for nuking.
#		22 Aug 2006 (sd) : Added export stuff for sofware release
#

# must be set in EVERY mkfile AND in panoptic.mk 
MKSHELL = `echo ${GLMK_SHELL:-ksh}`		

< $NG_SRC/panoptic.mk

# contrib should be early as things in feeds (at least) need to build binaries at precompile time 
# and those seem to need the sfstdio stuff.   This is mandated with the shift to windows compatability.
dirs = contrib potoroo 
dist_dirs = contrib potoroo 

# some contrib things are manditory for building everyting, this avoids a complete precompile
# and can be used as a '-p target' on an ng_sm_autobuild command
contrib:V:			
	(
		cd contrib
		mk precompile
	)

precompile:V:
	cp potoroo/srcmgt/ng_make.ksh .bin/ng_make		# must have these in order to do anything
	chmod 775 .bin/ng_make
	
	export TASK=precompile
	mk do_task
	cd stlib
	for x in *.a
	do
		ranlib $x  2>/dev/null ||true
	done

all:V:
	export TASK=all
	mk do_task

# for initial build on a node where we do not have potoroo yet
Install:V:
	export TASK=install
	mk do_task
	cp $NG_ROOT/lib/cartulary.min $NG_ROOT/cartulary

nuke:V:
	( cd stlib; rm -f *.a )
	( cd include; rm -f *.h  *.im copybooks/*.h )
	
	export TASK=nuke
	mk do_task
	(cd .bin; rm -f * )

install:V:	package

manpages:V:
	(cd manuals; mk precompile)
	export TASK=manpages
	mk do_task
	(cd manuals; mk index)
	
package: precompile all 
	ng_sm_mkpkg -v $dist_dirs

README:V:
	#XFM_IMBED=$XFM_IMBED 
	tfm README.xfm README
	XFM_IMBED=$XFM_IMBED hfm README.xfm README.html
	(ksh bootstrap_build.ksh --man >README.ng_build||true)

# logic for doing the subdirectories is the same regardless of the task; this reduces the duplication of code
do_task:VQ:
	for d in $dirs
	do
		if [[ -d $d ]]		# the release does not contain all dirs; do just what we have
		then
		(
			if cd $d
			then
				echo "===== making  $TASK in $d ========="
				ng_make $TASK
				x=$?
				printf "mk_status: "
				case $x in 
					0)	printf "$TASK dir=$d [OK]\n";;
					*)	printf "$TASK dir=$d rc=$x [FAIL]\n";
						exit 1;
						;;
				esac
			fi
		)
		fi
	done

