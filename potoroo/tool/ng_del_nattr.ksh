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
# Mnemonic:	ng_deld_nattr
# Abstract:	delete an attribute from the node
#		modifies the NG_NATTR variable and replaces in the local
#		cartulary, and if a permanent flag is set, in the node's 
#		cartulary.
#		NOTE:  
#			with the introduction of narbalek, atrribute management
#			had to change a bit.  We now keep a default list of 
#			node attributes which replaces the list of attributes
#			maintained in the node's cartulary file. So, when deleting
#			an attribute permanently, we must also update NG_DEF_NATTR
#		
# Date:		04 Dec 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# remove parms from the list in cur_list; alters cur_list
function del_from_list
{

	typeset rest=""
	while [[ -n $1 ]]		# for each attr on the command line
	do
		rest=""
		for a in $cur_list	# ditch it if it matches an existing attribute
			do
			if [[ $a != $1 ]]
			then
				rest="$rest$a "
			fi
		done
	
		cur_list="$rest"		# new list without $1
		shift
	done
}

# -------------------------------------------------------------------
ustring="[-b] [-h host] [-p] [-t] [-v] attribute [attribute...]"
me=`ng_sysname -s`
node=$me
for_host="-l"		# default to a local set
perm=1
forreal=1
bcast=0 

while [[ $1 = -* ]]
do
	case $1 in 
		-b)	bcast=1;;
		-h)	for_host="-h $2"; 
			ng_nar_get pinkpages/$2:NG_DEF_NATTR | read NG_DEF_NATTR	# must use their current settings
			ng_nar_get pinkpages/$2:NG_NATTR | read NG_NATTR		# must use their current settings
			shift
			;;
		-p)	perm=1;;			# keep for back compat
		-t)	perm=0;;
		-n)	forreal=0;;

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

cur_list="$NG_NATTR"			# work on the current list
del_from_list "$@"			# remove attrs from the list as given on the command line -- new list is left in $cur_list

comment="modified by ng_del_nattr `ng_dt -c`"
new="export NG_NATTR=\"$cur_list\"	# $comment"	# for immediate insertion in cartulary

if [[ $forreal -gt 0 ]]
then
	echo "$new" >>$CARTULARY
	ng_ppset $for_host NG_NATTR="$cur_list"			# set in pinkpages narbalek namespace
else
	log_msg "no action (-n) flag set; no changes will be made" 	
fi

verbose "new node attribute list: $cur_list"

if [[ $perm -gt 0 ]]		# must remove from the node's NG_DEF_NATTR list
then
	verbose "altering default (permanent) node attribute list"
	cur_list="$NG_DEF_NATTR"				# remove from the default list if there
	del_from_list "$@"
	if [[ $forreal -gt 0 ]]
	then
		ng_ppset  $for_host NG_DEF_NATTR="$cur_list"
	fi
	verbose "attribute(s) deleted permanently; new default list is: $cur_list"
else
	verbose "attribute(s) deleted; if they were defined in NG_DEF_NATTR, then they will revert on restart"
fi

if [[ $forreal -gt 0 && $bcast -gt 0 && -n $srv_Jobber ]]
then
	myhost=`ng_sysname`
	verbose "broadcasting new attributes to seneschal"
	ng_sreq -t 5 -s $srv_Jobber nattr ${myhost:-no-host-name} "`ng_get_nattr`"	# send our attributes to seneschal
fi

ng_get_nattr -P		# publish the new list

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_del_nattr:Delete node attribute)

&space
&synop	ng_del_nattr [-t] [-v] attribute [attribute...]

&space
&desc	&This deletes the attribute(s) supplied as parameters on the 
	command line from the node's static attribute list.  The static attribute
	list includes all attributes that are not computed.
	Computed attributes are those that reflect operating system type, and services; these attributes 
	cannot be affected by this command. 
	If the tempoary option (-t) is given, then the attribute will not be removed from 
	the default list for the node and thus will reappear when the ningaui software on 
	the node restarts. 
&space
	Node attributes, as presented by &lit(ng_get_nattr) are 'computed' based 
	on the current contents of the static attribute list as contained in the 
	 &lit(NG_NATTR) pinkpages variable, and attributes derrived from the current
	operating environment including operating system type. 
	The attribute list is augmented further with 'server attributes' that are based
	on the values of &lit(srv_) pinkpages variables.  A server attribute is added 
	to the list of a node's atribute if the value of the server attribute (srv_Catalogue
	for example) is the name of the node that executed the &lit(ng_get_nattr) command. 
&space
	The attributes that are indicated because of the presence of a &lit(_srv) pinkpages
	variable cannot be maintained using &this.  The primary reason for this is that 
	these variables are generally managed by daemons running in the cluster and 
	removing or resetting the pinkpages variable outside of the daemon's control could
	be disasterous. If these variables mut be reset, the daemon, or its control
	interface, should be used; ng_ppset can be used if absolutely necessary, or in 
	the case where a daemon is not running.

&space
	Some of the attributes listed in &lit(NG_NATTR) are computed and assigned as a 
	part of the Potoroo intiialisation process. These attributes (e.g. Tape, and Cpu_rich)
	can be deleted on a temporary basis, but will reappear with the next restart of 
	the Potoroo software if that attribute still applies to the node. 

&space
&options
	The following options are recognised:
&begterms
&term	-b : Broadcast. Send the new attributes for the node to seneschal immediately.
&space
&term	-h host : Delete the attribute from &ital(host).
&space
&term	-p : Perminant. The change is written to the &lit(cartulary.)&ital(node)
	file in an attempt to remove the attribute forever. 
&space
&term	-t : Remove the attribute only temporarly; the attribute will return when ningaui
	softeare is cycled.
&space
&term	-v : Verbosity is enabled.
&endterms

&space

&parms	Following the options, one or more attributes is expected to be listed. 
	These are removed from the list if present in the list.

&space
&exit	Always zero.

&space
&see	ng_species, ng_get_nattr, ng_add_nattr, ng_sneschal, ng_nawab

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Dec 2003 (sd) : The first.
&term	25 Mar 2004 (sd) : Beefed up the doc.
&term	18 Nov 2004 (sd) : Added -h option.
&term	02 Aug 2005 (sd) : Removed ningaui/adm references (they were not used).
&term	06 Jun 2006 (sd) : Calls get_nattr to publish newly changed list.
&term	04 Dec 2007 (sd) : Corrected man page. 
&term	28 Mar 2008 (sd) : Change to remove 'hard code' of NG_ROOT/cartulary.
&term	26 Sep 2008 (sd) : Added -t option making permanent the default.
&endterms

&scd_end
------------------------------------------------------------------ */
