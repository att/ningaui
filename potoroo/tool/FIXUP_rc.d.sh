#!/bin/sh
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


#!/bin/sh
# Mnemonic:	FIXUP_rc.d.ksh 
#		install as: 
#			/etc/rc.d/ningaui 			-- bsd systems (not recommended)
#			/usr/local/etc/rc.d/ningaui.sh 		-- bsd systems (recommended)
#			/Library/StartupItems/Ningaui/Ningaui 	-- mac osX
#			/etc/init.d/ningaui 			-- linux, then run chkconfig to install in rcx.d directories
#
#		Note: When installing on a MAC, the /etc/hostconfig file must 
#			also be adjusted to include Ningaui=-YES-
#			And, in the /Library/StartupItems/Ningaui directory, these lines
#			need to be put into StartupParameters.plist (no comments allowed):
#				{
#				Description     = "Ningaui Potoroo Services";
#				Provides        = ("Potoroo");
#				Requires        = ("Disks");
#				OrderPreference = "Late";
#				}
#
#		useful doc:
#			man rcorder	(bsd)
#			http://www.usenix.org/events/bsdcon02/full_papers/sanchez/sanchez_html/	(mac SystemStarter)
#			/etc/rc.d/skeleton (linux)
#	NOTE:   we are slowly moving away from MAC OS/X and thus the above information might not be 
#		as percise as it needs to be for current systems. 
#
# Abstract:	This is executed by the system at start time in the order 
#		that the system determines as necessary (using REQUIRE).
# 		This (we hope but have observed cases where it isnt) is also driven at shutdown time to stop things.
#
#		ng_setup and ng_site_prep will fix the _ROOT_PATH_ and _PROD_ID_ strings to be the  
#		proper settings for the node
#
# Date:		14 April 2003
# Author:	E. Scott Daniels
#
# --------------------------------------------------------------------------

# =========== stuff to support startup order in init.d or similar directory =====================

# ------ these are required for BSD startup ordering; space/case sensitive
# PROVIDE: ningaui
# REQUIRE: DAEMON NETWORK cron 
# KEYWORD: FreeBSD
#
# ------ similar, but different for (suse?) linux 2.6 (stupid ### lines are required) ----------------
# ------ see /etc/rc.d/skeleton for details here 
# ------ the should start is needed to drive our bootstrap code first, but only if it is there 
# ------ and does not complain if it is not.
### BEGIN INIT INFO" 
# Provides:       ningaui
# Required-Start: $local_fs $named $cron $network 
# Should-Start: ningaui_bootstrap
# X-UnitedLinux-Required-Start: $ALL
# X-UnitedLinux-Should-Start:
# Required-Stop:  $local_fs $named $network
# Default-Start:  3 5
# Default-Stop:   0 2 1 6
# Short-Description:    Potoroo node initialisation
# Description:    Potoroo node initialisation -- detaches from caller
#	to prevent blocking the rest of initialisation as potoroo init
#	can take some time.
### END INIT INFO

# ----  this is for (generic?) linux startup ordering  99== start late  01== shutdonw early
# chkconfig: 2345 99 01
# --------- end rc.init stuff -------------------

# ======= things that might require manual edit by system admin -- start here ===================
#NG_ROOT=${NG_ROOT:-/ningaui}		# <=== uncomment change this if necessary, comment out next line if you do
NG_ROOT=_ROOT_PATH_			# changed automatically by setup and site prep
export NG_ROOT

NG_PROD_ID=_PROD_ID_			# <=== change if production id is not ningaui 
export NG_PROD_ID

ksh_path="$NG_ROOT/bin/"		# <=== change if needed; site_prep/setup should set a symlink in NG_ROOT. MUST have trailing /
# ========== end manual edit var settings ==================================================

if test $1 = "--man" 
then
	(export XFM_IMBED=$NG_ROOT/lib; $NG_ROOT/bin/tfm $0 stdout .im $XFM_IMBED/scd_start.im 2>/dev/null | ${PAGER:-more})
	exit 1
fi

rm -f /tmp/ningaui.rc
exec >/tmp/ningaui.rc  2>&1		# do NOT use $TMP -- we have not run site-prep to ensure it exists

if [ ! -x $ksh_path/ksh ]
then
	echo "WARNING: path to ksh smells: $ksh_path"
	ksh_path=""			# we will execute ksh and hope for the best!
fi

rm -f /tmp/solo.ng_init 		# these prevent ng_init from starting and can be left if the machine dies.

# on the macs path seems to lack key things; we need just to ensure it is good
PATH="/usr/local/bin:/usr/bin:/bin:/sbin"
export PATH

if [ -f  /etc/rc.subr ]
then
	. /etc/rc.subr		# BSD 5.x
	name=ningaui
	rcvar=`set_rcvar`		# these are mostly for bsd5.x style start -- we go our own way at the moment
	command="$NG_ROOT/bin/ng_init"
	start_cmd="ningaui_start"
	stop_cmd="ningaui_stop"
fi

#
#  the start/stop commands can be executed with us if necessary:
#  NOTE:  su -l ningaui causes FreeBSD 5.1 systems to hang
#  	su ningaui -c "command"
#  ng_init now detaches itself so that the return is instant.
#
case $1 in
	start|faststart)
		echo "starting ningaui"
		#${ksh_path}ksh $NG_ROOT/bin/ng_init		# initialises and then calls ng_node start
		su $NG_PROD_ID -c "${ksh_path}ksh $NG_ROOT/bin/ng_init"
		;;
	
	stop)
		echo "stopping ningaui; messages to $NG_ROOT/site/log/rc.stop"
		#${ksh_path}/ksh $NG_ROOT/bin/ng_node terminate >>$NG_ROOT/site/log/rc.stop 2>&1
		su $NG_PROD_ID -c "${ksh_path}/ksh $NG_ROOT/bin/ng_node terminate >>$NG_ROOT/site/log/rc.stop 2>&1"
		;;

	*restart*)			# likely that things like woomera will not restart because of delayed shutdown; not supported
		echo "restart not supported"
		exit 1
		;;

		

	*)
		echo "dont understand: ${1:-option missing};"
		;;
esac

exit 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rc.d:Ningaui initalisation rc script)

&space
&synop	ng_rc.d {start|faststart|stop}

&space
&desc	&This is invoked during system startup by the O/S procedure that starts various 
	components after reboots, and prior to system shutdown.  The script invokes
	&lit(ng_node) with the proper start or stop option to do the real work. 
&space
	&This should be invoked only by the system process. If it is necessary to 
	stop or start ningaui manually, the ng_node command should be invoked.

&space
&parms
	&This expects one of three keyword tokens on the command line:
&space
&begterms
&term	start : Causes the ningaui environment to be initialised and ningaui daemons 
	to be started. 
&space
&term	faststart : Same as start.
&space
&term	stop : Causes ningaui daemons to be stopped and the ningaui environment prepared for 
	a system halt or reboot.  
&endterms



&space
&exit	Generally exits with a non-zero return code to indiate an error.

&space
&see	ng_node, ng_init

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	14 Apr 2003 (sd) : Original.
&term	07 Aug 2003 (sd) : removed -l from su commands; causes FreeBSD 5.1 to hang
&term	25 Oct 2004 (sd) : added init stuff for the most recent linux (suse?)
&term	08 Jul 2005 (sd) : removed export ksh construct from NG_ROOT.
&term	05 Aug 2005 (sd) : fixed the NG_ROOT fixup that is done by siteprep/setup.
&term	01 Mar 2006 (sd) : the fixup string for ng_root was wrong; fixed it.
&term	26 Oct 2006 (sd) : Pulled ref to AST. Trash ng_init goaway if it is there.
&term	19 Dec 2006 (sd) : Added manual page.
&term	15 Oct 2007 (sd) : Added support for NG_PROD_ID to allow id other than ningaui to be the production id.
&term	25 Mar 2008 (sd) : Now executes ng_init as ningaui.
&term	27 Mar 2008 (sd) : Setup linux (SUSE) ordering to follow ningaui_bootstrap if it is there.
&term	22 Apr 2008 (sd) : Network Time seems not to be a startup item under apple anymore, so I changed 
		the documentation in the flowerbox.  So much for OSX being unix. 
&term	15 Oct 2008 (sd) : We now allow prod-id to be set by setup/siteprep.
&term	13 Jan 2009 (sd) : Source module name chaned from *.ksh to *.sh so that installer will not add the 
		wrong #! line to the top.  Was causing issues on linux nodes. 
&term	01 Jun 2009 (sd) : Changed the node stop command to node terminate so that the node script would not 
		'detach.'  When node was detatching, the system would terminate before ningaui was 100% down.
		
&endterms


&scd_end
------------------------------------------------------------------ */
