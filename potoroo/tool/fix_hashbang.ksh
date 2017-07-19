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
# Mnemonic:	fix_hashbang
# Abstract:	suss through the directory given and attempt to make the #!
#		lead line correct
#		
# Date:		20 February 2008 (broken out of refurb as setup needs it too)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
if [[ -f $CARTULARY ]]
then
	. $CARTULARY
else
	touch $CARTULARY		# allows us to run on a system that has not been installed
fi

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# we must set this up as setup_common might not have been run on the system when this is invoked
if whence gawk >/dev/null 2>&1			# preferred awk version
then
	awk=gawk
else
	if whence nawk >/dev/null 2>&1		# allows us to run on old/backwards systems before potoroo installed & started
	then
		awk=nawk
	else
		awk=awk
	fi
fi

# -------------------------------------------------------------------
ustring="[-n] [-v] directory [dir...]"
forreal=1

while [[ $1 == -* ]]
do
	case $1 in 
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

if [[ -z $1 ]]
then
	usage
	cleanup 1
fi


if [[ ! -f $NG_ROOT/bin/ksh ]]
then
	if [[ -e  /usr/common/ast/bin/ksh ]]
	then
		ksh=/usr/common/ast/bin/ksh
	else
		echo "WARNING: NG_ROOT/bin/ksh and /usr/common/ast/bin/ksh don't seem to exist; errors might occurr if /usr/bin/ksh is old!"
		ksh=ksh
	fi	
else
	ksh=$NG_ROOT/bin/ksh
fi

# suss out files in the listed directories that are probably scripts (according to file)
# from that list we print a set of tripples (script-name, #! string, shell name) for 
# the files that appear to have an invalid #! path.  If the script type is ksh, then 
# we copy the script off and let ng_install reinstall it assuming that it will get the 
# #! business right on the install node as opposed to the node where packages were created.
#
# we now allow for `#!/usr/bin/env ruby` style hash-bang lines. We only verify that the 
# "path/shell" part is right -- if ruby is not installed we will not catch it.
while [[ -n $1 ]]
do
	(
	verbose "searching for scripts in: $1" 
	cd $1

	# WARNING: file returns different verbage on different systems. script seems the only common token
	ls | xargs file | $awk '/script/ { gsub( ":", "", $1 );  print $1 }' |while read f
	do
							# watch out; bloody suns barf if trailing whitespace after 1q!
		sed 's/\#!//; 1q' <$f | read what junk 	# pluck path/shell from #!/path/shell junk
		if [[ -n $what && ! -e $what ]]
		then
			echo $f $what ${what##*/}
		fi
	done >/tmp/vhb.$$				# DONT use TMP, it might not be set
	
	count=0

	while read script path shell
	do
		count=$(( $count + 1 ))

		case $shell in 
			ksh)				
				if (( $forreal > 0 ))
				then
					verbose "re-installed $script to fix $path"
					gre -v "^#!" $script >/tmp/$script.$shell				# toss out the original one
					(cd /tmp; $ksh $NG_ROOT/bin/ng_install $script.$shell $PWD >/dev/null)	# use ksh cmd; ng_install might have bad #! too
					rm /tmp/$script.$shell 
				else
					log_msg "would reinstall $script to fix $path"
				fi
				;;

			*)	
				log_msg "WARNING: the #! path or shell type seems wrong or is not recognised in $script shell=$shell path=$path; it was left unchanged"
				;;
		esac
	done </tmp/vhb.$$

	if (( forreal < 1 ))
	then
		log_msg "${count:-no} scripts required change in $1"
	fi
	
	)

	shift
done


rm -f /tmp/vhb.$$



cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_fix_hashbang:Fixup #! lines in scripts)

&space
&synop	ng_fix_hashbang directory [directory..]

&space
&desc	&This will suss out all scripts (as identified by the file command)
	from the named directories and will validate the #! statement in each.
	If the statement appears wrong, the script is reinstalled.

&space
&parms	One or more directory names must be supplied.

&space
&exit	A non-zero return code indicates that an error occurred. 

&space
&see	&seelink(tool:ng_refurb)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Feb 2008 (sd) : Its beginning of time. 
&term	30 Jul 2008 (sd) : Changes to support execution on a node that has had a package unloaded but needs to 
		be fixed (not much else can run other than this). .** HBD AKD
&term	24 Sep 2008 (sd) : Change to allow . to be supplied as directory to search (passing . to ng_install was causing odd issues).
&endterms


&scd_end
------------------------------------------------------------------ */

