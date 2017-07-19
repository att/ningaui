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
# Mnemonic:	ng_add_nattr
# Abstract:	add an attribute to the node (temporary or permanent)
#		Adds to the NG_NATTR variable and replaces in the local
#		cartulary, and if a permanent flag is set, in the node's 
#		cartulary.
#		NOTE:  
#			with the introduction of narbalek, atrribute management
#			had to change a bit.  We now keep a default list of 
#			node attributes which replaces the list of attributes
#			maintained in the node's cartulary file. So, when adding
#			an attribute permanently, we must also update NG_DEF_NATTR
#			in pinkpages (local).
# parms:	
#		
# Date:		04 Dec 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# based on input list, set the list of attributes that need to be added
function set_list
{
	list=""
	while [[ -n $1 ]]
	do
		if [[ " $cur_attrs " != *" $1 "* ]]		# spaces are important
		then
			list="$list$1 "
		fi
		shift
	done
}  


# trash extra blanks
function pretty
{
	echo $1 | sed 's/^ * //; s/ * $//;'		
}

# -------------------------------------------------------------------
ustring="[-b] [-h host] [-t] [-v] attribute"
me=`ng_sysname -s`
node=$me
perm=1
vflag=""
bcast=0
for_host="-l"		# default, set nattr for me (local set ppset call)

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	bcast=1;;		# send to the jobber NOW!
		-h)	for_host="-h $2"; 
			ng_nar_get pinkpages/$2:NG_DEF_NATTR | read NG_DEF_NATTR	# must use their current settings
			ng_nar_get pinkpages/$2:NG_NATTR | read NG_NATTR	# must use their current settings
			shift
			;;
		-p)	perm=1;;
		-t)	perm=0;;

		-v)	verbose=1; vflag="-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ -z $1 ]]
then
	usage
	cleanup
fi

	# old stuff before narbalek
#admd=$NG_ROOT/adm		# where cartulary files live
#lcart=$admd/cartulary.local
#ncart=$admd/cartulary.$me

list=""
new=""
cur_attrs="$NG_NATTR"			# DONT use ng_get_natter, as we only reset what is in NG_NATTR and not srv_ things
set_list "$@"				# create a list of attrs that do not currently exist in NG_NATTR

if [[ -n $list ]]			# something was not set, prep to add it 
then
	verbose "these attributes will be added: $list"
	comment="node attributes; modified by ng_add_nattr `ng_dt -c`"

	pretty "$NG_NATTR $list" | read new		# new attributes with extra white space deleted
	verbose "static node attrs now: $new"
	#echo "export NG_NATTR=\"$new\"		# $comment " >>$CARTULARY	# make it happen now

	ng_ppset $vflag $for_host -c "$comment" NG_NATTR="$new"
else
	if [[ $perm -lt 1 ]]
	then
		verbose "all attributes given were already set"
	fi
fi


if [[ $perm -gt 0 ]]		# ensure that the attribute lasts between boots
then
	cur_attrs=" $NG_DEF_NATTR "		# get the default nattr list  (spaces are important) 
	set_list "$@"				# add all from the command line that were not there
	if [[ -n $list ]]
	then
		ng_ppset $vflag $for_host -c "$comment" NG_DEF_NATTR="$NG_DEF_NATTR $list"
		verbose "these attributes now set permanently: $NG_DEF_NATTR $list"
	else
		verbose "these attributes now set permanently: $NG_DEF_NATTR"	# all on command line were already in perm list
	fi
else
	verbose "attribute(s) added; will revert with next potoroo software reset"
fi

if [[ $bcast -gt 0 && -n $srv_Jobber ]]
then
	myhost=`ng_sysname`
	verbose "broadcasting new attributes to seneschal"
	ng_sreq -t 5 -s $srv_Jobber nattr ${myhost:-no-host-name} "`ng_get_nattr`"	# send our attributes to seneschal
fi

ng_get_nattr -P		# publish the attributes in narbalek space.
cleanup 0


# old code that had to jack with the cartulary.<node> file

#if [[ $perm -gt 0 ]]		# ensure that the attribute lasts between boots
#then
#	NG_NATTR=""			# clear
#	list=""
#
#	. $ncart			# pick up just what attributes are set perminately
#
#	cur_attrs="$NG_NATTR"		# just these for set list call
#	set_list "$@"			# set list to what was not set perm (some could have been temp deleted)
#
#
#	if [[ -n $list ]]		# some need to be set permanently in the nodes cartulary
#	then
#		cp $ncart $ncart-			# save backup
#		grep -v NG_NATTR $ncart- >$ncart	# clear previous settings from file
#
#		comment="node attributes; modified by ng_add_nattr `ng_dt -c`"
#		new="export NG_NATTR=\"$NG_NATTR $list\"	# $comment"
#		echo "$new" >>$ncart
#		verbose "attribute(s) added permanently: $list"
#	fi
#else
#	verbose "attribute(s) added; will revert with next potoroo software reset"
#fi
#


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_add_nattr:Add node attributes)

&space
&synop	ng_add_nattr [-b] [-h host] [-t] [-v] attribute 

&space
&desc	&This adds one or more attributes to the node's current attribute list.
	The list is maintained using a local cartulary variable &lit(NG_NATTR).
	By default, the attribute is permanently added; the &lit(-t) option should
	be used to add the attribute on a temporary (until the next node start) basis.
	Permanent apptributes are maintained using a special pinkpages (locally scoped)
	variable: &lit(NG_DEF_NATTR.) The pinkpages variable &lit(NG_NATTR) is a combination
	of permanent, and temporary attributes in addition to attributes that are derrived
	from services hosted on the node. 
&space
	This command affects the attributes for the node that the command is 
	run on.  The only way to set the attributes for another node is to 
	remotely run the command using ng_rcmd on the target node. 

&space
&opts	The following command line options are recognised:
&begterms
&term	-b : Broadcast new attributes to seneschal.  As soon as the attributes are 
	changed, they are broadcast to seneschal.
&space
&term	-h host : Set the attribute for the named host. If omitted, the attribute 
	is set for the node running the command. 
&space
&term	-p : Perminant. An effort is made to keep the change between software 
	resets. (This is unnecessary, but remains to quietly support the original 
	command syntax.)
&space
&term	-t : Declare the attribute as a temporary one; it will not survive a node 
	stop/start cycle.
&space
&term	-v : Verbose mode. 
&endterms

&space
&parms	One or more attributes are expected on the command line.  Attributes are 
	single word tokens, which generally begin with an uppercase letter. 

&space
&examp
&litblkb
   ng_add_nattr New_attribute
   ng_add_nattr -p New_attr       # keep after reboot too
&litblke

&space
&exit	Always zero.

&space
&see	ng_species

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Dec 2003 (sd) : The first.
&term	03 Sep 2004 (sd) : Added support for narbalek oriented variable storage.
&term	11 Oct 2004 (sd) : Added -b option
&term	28 Oct 2004 (sd) : Updated man page.
&term	12 Nov 2004 (sd) : Added -h host support.
&term	03 May 2005 (sd) : Cleanup to dreference adm dir.
&term	06 Jun 2005 (sd) : Calls get_nattr to publish newly changed list.
&term	26 Sep 2008 (sd) : Added the -t option and changed the default to be adding a perm attr.
&endterms

&scd_end
------------------------------------------------------------------ */
