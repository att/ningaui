#!/usr/common/ast/bin/ksh
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


#!/usr/common/ast/bin/ksh
# generate spacehogs files
# run out of root's crontab daily, shortly after midnight
# Mod: 30 Jul 2005 (sd) : Now uses du that is in the path rather than ast (HBD-AKD)
# 	31 Mar 2006 (sd) : now uses amIreally function from stdfun rather than id directly 
#		seems that id -u is not good on darwin/sun/sgi nodes.
#	26 Apr 2006 (sd) : Corrected so that it will run on a Sun (vfstab is organised in 
#		a different manner, and is not in /etc/fstab. 
#	20 Jun 2006 (sd) : sun and darwin seem not to support -m.  Fixed. 
#	12 Oct 2006 (sd) : Now lists the filesystems that it probes to stderr so that the 
#		log file is updated.
#------------------------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# Mnemonic: amIreally
# Abstract: test to see if we are really the user id indicated in $1
# Exit:		returns 0 if we are; 1 if not
# ----------------------------------------------------------------------------

function amIreally
{
	typeset _myuser
	if ! id -u -n 2>/dev/null |read _myuser
	then
		id | read _myuser			# bloody suns
		_myuser=${_myuser%%\)*}
		_myuser=${_myuser#*\(}
	fi

	if [[ $_myuser == "$1" ]]
	then
		return 0
	else
		return 1
	fi
}

if [[ "$*" == *"--man"* || "$*" == *"-?"* ]]
then
	. $NG_ROOT/cartulary		# only source these for --man or -?
	. $NG_ROOT/lib/stdfun
fi

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			exit 1
			;;
	esac

	shift
done


LD_LIBRARY_PATH=/lib:/usr/lib:/usr/common/ast/lib
export LD_LIBRARY_PATH

# only care about directories with at least this many MB in them
THRESHOLD=50

rotatelog() {
	fname=$1
	for ext in 6 5 4 3 2 1 0
	do
		mv -f $fname.$ext $fname.$(($ext + 1)) 2>/dev/null
	done
	true
}

if ! amIreally root
then
	echo "warning: should be run as root" >&2
fi

#if test $(/usr/common/ast/bin/id -u) != 0
#if test $(id -u) != 0
#then
#	echo warning: should be run as root >&2
#fi

case $(uname) in 
	Sun*)	fs_field=3; type_field=4; fstab=/etc/vfstab;;
	*)	fs_field=2; type_field=3; fstab=/etc/fstab;;
esac

DEFAULTFS="/ /usr /tmp /ningaui/tmp /home"
FS=""

# scan list of default filesystems, prune down to include only local ones
for fs in $DEFAULTFS
do
	FS=$FS' '$(awk -v fsf=$fs_field -v tf=$type_field -v fs=$fs '{ if ($1 !~ /^#/ && $(fsf) == fs && $(tf) != "nfs") print fs }' $fstab)
done

echo "probing: $FS" >&2

du_prog=du				# sanity for all but the suns
case $(ng_uname) in
	Sun*)	du_prog=/usr/xpg4/bin/du; du_opts="-x";;
	Darwin) du_opts="-x";;
	*)	du_opts="-m -x";;
esac

for fs in $FS
do
	#(cd $fs && rotatelog $fs/spacehogs && /usr/common/ast/bin/du -m -x . | grep -v '/.*/.*/.*/' | awk -v t=$THRESHOLD '$1 > t' | sort -nr > $fs/spacehogs.0)
	#(cd $fs && rotatelog $fs/spacehogs && du -m -x . | grep -v '/.*/.*/.*/' | awk -v t=$THRESHOLD '$1 > t' | sort -nr > $fs/spacehogs.0)
	(cd $fs && rotatelog $fs/spacehogs && $du_prog $du_opts  . | grep -v '/.*/.*/.*/' | awk -v t=$THRESHOLD '$1 > t' | sort -nr > $fs/spacehogs.0)
done


exit 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_mkspacehogs:Create a list of excessive use by user)

&space
&synop	ng_mk_spacehogs

&space
&desc	&This looks at several spots in the file tree and  builds a list of 
	use.  The information is saved into the file &lit(spacehogs.0)
	on the filesystem that is checked.  This is intended to be run 
	from root's crontable on a regular  basis to track the use
	of filesystems. By default, these filesystems are monitored:
&space
&beglist
&item	
&endlist

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	/ 
&term	/usr 
&term	/tmp 
&term	/ningaui/tmp 
&term	/home 
&endterms

&space
&exit	Exit code of 0 indicates all went well; non-zero inidicates error.

&space
&see	ng_spyglass

&space
&mods
&owner(Mark Plotnick)
&lgterms
&term	18 Dec 2006 (sd) : Added doc.


&scd_end
------------------------------------------------------------------ */
