#!/ningaui/bin/ksh
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


#!/ningaui/bin/ksh

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

MINSTATUS=100		# minimum amount back from mtx status                  
PATH=$PATH:/usr/sbin	# to get mtx

slot_cvt(){
	case $1 in
	[0-9]*)
		echo "slot $1"
		;;
	p*)
		echo "portal `awk -v x=$1 'BEGIN { print substr(x, 2)}' < /dev/null`"
		;;
	*)
		echo "bad slot"
		;;
	esac
}

m_slot_cvt(){
	sed 1q $2 > $tmp.2
	read x x x s1 sn p1 pn < $tmp.2
	case $1 in
	[0-9]*)
		echo $1
		;;
	p*)
		echo `awk -v x=$1 -v sn=$sn -v p1=$p1 'BEGIN { print substr(x, 2)-p1+sn+1}' < /dev/null`
		;;
	*)
		echo "bad slot"
		;;
	esac
}

#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#
ustring=''
dev=''
while [[ ${1%%[!-]*} = "-" ]]			# while "first" parm has an option flag (-)
do
	shiftcount=1				# default # to shift at end of loop 
	case $1 in
	-f)	dev=$2; shiftcount=2;;
	-v)	verbose=TRUE; verb=-v;;
	-\?)	usage; cleanup 1;;
	*)		# invalid option - echo usage message 
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac					# end switch on next option character

	shift $shiftcount			# shift off parameters processed during loop
done						# end for each option character

tmp=/tmp/lb_$RANDOM.$$

case $# in
0)	usage;;
*)	;;
esac

me=`ng_sysname`
		
if test -z "$dev"
then
	if ! grep -s library $NG_ROOT/site/config | sed 1q > $tmp
	then
		echo 'no library on this machine'
		exit 1
	fi
	read lib dev < $tmp
fi
if ! grep -s scsi_interface $NG_ROOT/site/config | sed 1q > $tmp
then
	echo 'no scsi interface on this machine'
	exit 1
fi
read xx interface < $tmp

case $interface in
sg_inq)
	if ! mtx -f $dev status > $tmp
	then
		# one more chance... status can fail if done right after doing an inventory
		sleep 5
		if ! mtx -f $dev status > $tmp
		then
			echo "mtx failed"
			exit 2
		fi
	fi
	if [[ `wc -l < $tmp` -lt $MINSTATUS ]]		# just want to know if it worked
	then
		mtx -f $dev inventory > $tmp
		sleep 60
		mtx -f $dev status > $tmp
	fi
	if [[ `wc -l < $tmp` -lt $MINSTATUS ]]		# just want to know if it worked
	then
		echo "mtx status returned short"
		exit 2
	fi
	
	
	cat $tmp | tee $NG_ROOT/site/tape/changer.status | gre -v EXCEP | awk '
	BEGIN	{
		empty = "__empty"
		no_name = "??"
		voltag1 = "VolumeTag = "
		voltag2 = "VolumeTag="
		min_dte = min_slot = min_portal = 999999
		dev = "nodev"
	}
	/Storage Changer/ {
		dev = $3
		sub(":.*", "", dev)
	}
	/Data Transfer Element/	{
		x = $0; sub("^Data Transfer Element", "", x); n = x+0
		x = $0; sub("^[^:]*:", "", x)
		if(x == "Empty")
			dte[n] = empty
		else {
			if(index($0, voltag1)){
				x = $0; sub(".* = ", "", x); sub(" *$", "", x); sub("L[0-9]$", "", x)
				dte[n] = x
			} else
				dte[n] = no_name
		}
		if(n < min_dte)
			min_dte = n
		if(n > max_dte)
			max_dte = n
		next
	}
	/^ *Storage Element/	{
		x = $0; sub("^ *Storage Element", "", x); n = x+0
		x = $0; sub("^[^:]*:", "", x); sub(":.*", "", x)
		if(x == "Empty")
			lab = empty
		else {
			if(index($0, voltag2)){
				x = $0; sub(".*=", "", x); sub(" *$", "", x); sub("L[0-9]$", "", x)
				lab = x
			} else
				lab = no_name
		}
		if(index($0, "IMPORT") > 0){
			portal[n] = lab
			if(n < min_portal)
				min_portal = n
			if(n > max_portal)
				max_portal = n
		} else {
			slot[n] = lab
			if(n < min_slot)
				min_slot = n
			if(n > max_slot)
				max_slot = n
		}
		next
	}
	END	{
		print dev, min_dte, max_dte, min_slot, max_slot, 1, max_portal-min_portal+1
		for(n in dte)
			print "dte", n, dte[n]
		for(n in slot)
			print "slot", n, slot[n]
		for(n in portal)
			print "portal", n-min_portal+1, portal[n]
	}' > $tmp.1

	# needed the catalog before the others so that we can fake out the portal mappings
	case $1 in
	catalog)
		cat $tmp.1
		;;
	load)
		x=`m_slot_cvt $2 $tmp.1`
		echo "info: mtx -f $dev load $x $3" 1>&2
		mtx -f $dev load $x $3
		;;
	unload)
		x=`m_slot_cvt $3 $tmp.1`
		echo "info: mtx -f $dev unload $x $2" 1>&2
		mtx -f $dev unload $x $2
		;;
	transfer)
		x=`m_slot_cvt $2 $tmp.1`
		y=`m_slot_cvt $3 $tmp.1`
		echo "info: mtx -f $dev transfer $x $y" 1>&2
		mtx -f $dev transfer $x $y
		;;
	*)
		echo "$0: unknown command $*" 1>&2
		exit 1
	esac
	
	;;
camcontrol)
	case $1 in
	catalog)
		if ! chio -f $dev status -a > $tmp
		then
			# one more chance... status can fail if done right after doing an inventory
			sleep 5
			if ! chio -f $dev status -a > $tmp
			then
				echo "chio failed"
				exit 2
			fi
		fi
		if [[ `wc -l < $tmp` -lt $MINSTATUS ]]		# just want to know if it worked
		then
			chio -f $dev ielem > $tmp
			sleep 60
			chio -f $dev status -a > $tmp
		fi
		if [[ `wc -l < $tmp` -lt $MINSTATUS ]]		# just want to know if it worked
		then
			echo "chio status returned short"
			exit 2
		fi
		cat $tmp | tee $NG_ROOT/site/tape/changer.status | gre -v EXCEP | awk -v dev=$dev '
		BEGIN	{
			empty = "__empty"
			no_name = "??"
			voltag1 = "VolumeTag = "
			voltag2 = "VolumeTag="
			min_dte = min_slot = min_portal = 999999
		}
		/^drive/	{
			n = $2 + 0
			x = $0; sub(".* voltag: <", "", x); sub(":.*", "", x)
			if(x == "")
				dte[n] = empty
			else {
				dte[n] = x
			}
			if(n < min_dte)
				min_dte = n
			if(n > max_dte)
				max_dte = n
			next
		}
		/^slot/	{
			n = $2 + 0
			x = $0; sub(".* voltag: <", "", x); sub(":.*", "", x)
			if(x == "")
				slot[n] = empty
			else {
				slot[n] = x
			}
			if(n < min_slot)
				min_slot = n
			if(n > max_slot)
				max_slot = n
			next
		}
		/^portal/	{
			n = $2 + 0
			x = $0; sub(".* voltag: <", "", x); sub(":.*", "", x)
			if(x == "")
				portal[n] = empty
			else {
				portal[n] = x
			}
			if(n < min_portal)
				min_portal = n
			if(n > max_portal)
				max_portal = n
			next
		}
		END	{
			print dev, min_dte, max_dte, min_slot, max_slot, min_portal, max_portal
			for(n in dte)
				print "dte", n, dte[n]
			for(n in slot)
				print "slot", n, slot[n]
			for(n in portal)
				print "portal", n, portal[n]
		}'
		;;
	load)
		x=`slot_cvt $2`
		echo "info: chio -f $dev move $x drive $3" 1>&2
		chio -f $dev move $x drive $3
		;;
	unload)
		x=`slot_cvt $3`
		echo "info: chio -f $dev move drive $2 $x" 1>&2
		chio -f $dev move drive $2 $x
		;;
	transfer)
		x=`slot_cvt $2`
		y=`slot_cvt $3`
		echo "info: chio -f $dev move $x $y" 1>&2
		chio -f $dev move $x $y
		;;
	*)
		echo "$0: unknown command $*" 1>&2
		exit 1
	esac
	
	;;
*)
	echo "unknown scsi_interface $interface: exiting"
	exit 1
esac

rm -f $tmp $tmp.[12]

cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&title ng_tp_library - tape library interface
&name &utitle

&space
&synop  ng_tp_library [cmd ...]


&space
&desc   &ital(Ng_tp_library) is the API to the tape library functions on the current machine.
	(Yes, for now we assume there's just one.)
	It performs one of the following commands; by default, it performs the &cw(catalog) command.
&space
&term	catalog:  Return a list of all the elements in the library and what is in that element.
&space
&term	load slot dte: Transfers the medium in &ital(slot) into &ital(dte).
&space
&term	unload dte slot:  Transfers the medium in &ital(dte) into &ital(slot).
&space
&term	transfer slot1 slot2: Transfers the medium from &ital(slot1) to &ital(slot2).
&endterms

	Slots and dte's are all identified by an integer (as designated in the &cw(catalog) output);
	a portal is an integer with a &cw(p) prefix.

&space
	This command operates for a specific model of what a tape library is.
	We observe that libraries are increasingly complicated and that over time,
	the model (and its API) will evolve. The current model is:

	The library is composed of five entities: media (which have labels),
	slots, portals, dte's (DTE = Data Transfer Element, or drive), and a picker.
	Media can only be moved into a slot or dte when that slot or dte is empty.
	Once a medium is moved to a dte, it should be unmounted before it can
	be moved from that dte. (Unmounting is done by an &cw(mt offline) command.)
	Portals are a special kind of slot;
	nearly all libraries offer some quick way to add or take tapes from the library;
	if so, they are done through these portal slots.
	Apart from media (which have labels), all these entities are identified by a
	simple integer. The valid integers need not be sequential. For example,
	the valid dtes might be 0, 1, 38, 39.

	It is important to understand that although libraries vary, the library only
	deals with moving media around the library. The library is fundamentally unaware
	of what is going on with the dte (or even where it is).

	The underlying command used to talk to the library is either &ital(mtx) or &ital(chio),
	chosen by the value of &cw(scsi_interface) in &cw(/ningaui/site/config)
	(&cw(sg_inq) means use &ital(mtx), &cw(camcontrol) means use &ital(chio)).

&exit
	&ital(Ng_tp_library) script will set the system exit code to one
	of the following values in order to indicate to the caller the final
        status of processing when the script exits.
&begterms
&space
&term   0 : The script terminated normally and was able to accomplish the 
	 		caller's request.
&space
&term   1 : The parameter/options supplied on the command line were 
            invalid or missing. 
&endterms

&space
&see    ng_tpreq, ng_tpchanger
&owner(Andrew Hume)
&scd_end
