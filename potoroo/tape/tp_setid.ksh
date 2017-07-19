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
# Mnemonic:	ng_tp_setid
# Abstract:	set pinkpages variable(s) based on device names passed in.
#		
# Date:		8 Aug 2006
# Author:	E. Scott Daniels (based on Andrew's original getid script)
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function set_var
{
	typeset value=""
	typeset vname="TAPE_DEVID_${1##*/}"		# just sg0 from /dev/sg0
	typeset dname="$1"				# keep the /dev for dname

	case ${sys_type:-no-type} in
	linux|Linux)
		(
			$NG_ROOT/bin/ng_dosu sg_inq83 $1 | od --width=1000 -t x1 | sed 's/^/binid /; 2d'	# scsi query to get id from page 83
			$NG_ROOT/bin/ng_dosu sg_inq $1							# scsi query to get general info
		) | awk -v dev=$1 '
		BEGIN { kind = "devtype?"; vend = "vendor?"; prod = "prod?"; binid = "binid?" }
		/Device_type=8/	{ kind = "changer" }
		/Device_type=1/	{ kind = "tape" }
		/Vendor identification:/ { vend = $3 }
		/Product identification:/ { prod = $3 }
		/^binid / {
			for(i = 5; i < NF; i++)
				if($i == "08"){
					binid = $(i+6) $(i+7) $(i+8)
					next
				}
	
			binid = "dunno"
		}
			{ ; }
		END { print kind, vend, prod, binid }'
		;;

	freebsd|FreeBSD)
			#pass0: <IBM ULT3580-TD4 7A31> Removable Sequential Access SCSI-3 device 
			#pass0: Serial Number 0007854656
			#pass0: 100.000MB/s transfers , Tagged Queueing Enabled
			#------------------------------------------------------------
			#pass1: <IBM 03584L32 7360> Removable Changer SCSI-3 device 
			#pass1: Serial Number 0000000229940401
			#pass1: 100.000MB/s transfers , Tagged Queueing Enabled

		$NG_ROOT/bin/ng_dosu cc_inq ${1##*/} | awk '
			BEGIN { kind = "devtype?"; vend = "vendor?"; prod = "prod?"; binid = "binid?" }
			NR==1 { vend = substr($2, 2); prod = $3; }
			NR==2 { x = $4; sub("^0*", "", x); binid = sprintf("%x", x) }
			/ Changer / { kind = "changer" }
			/ Sequential Access / { kind = "tape" }
		 	{ ; }
			END { print kind, vend, prod, binid }'
		;;
	esac | awk '
	 { kind = $1; vend = $2; prod = $3; binid = $4 }
	END {
		pmap["IBM_ULTRIUM-TD2"] = "IBM_3580_2"
		pmap["IBM_ULT3580-TD4"] = "IBM_3580_4"
		pmap["SONY_SDX-500C"] = "SONY_SDX_500C"
		pmap["QUALSTAR_TLS-412180"] = "QSTAR_412180"
		pmap["STK_SL500"] = "STK_SL500"

		x = vend "_" prod
		if(pmap[x])
			x = pmap[x]
		else
			printf("tp_setid: warning: no nickname for %s\n", x) >"/dev/stderr"

		print x "_" binid
	}' | read value

	case $value in 
		*dunno*)	
				log_msg "value ($value) seems wrong; deleting variable $vname"
				value=""
				;;

		__*)		
				log_msg "value ($value) seems wrong; deleting variable $vname"
				value=""
				;;	

		*)		verbose "tp_setid: variable set: $vname=$value"
				rc=0
				;;
	esac

	if (( $forreal > 0 ))
	then
		eval ng_ppset -l $vname=\"$value\"
		if (( $echo_value > 0 ))
		then
			echo $value
		fi

		
		if [[ -n $value ]]
		then
			verbose "tp_setid: variable set: TAPE_$value=$mynode,$dname"
			eval ng_ppset -f TAPE_$value=\"$mynode,$dname\"
		fi
	else
		eval log_msg \"no execution mode:  ng_ppset -l $vname=\'$value\'\"
	fi
}


function usage
{
	cat <<endKat
usage:	$argv0 -a
	$argv0 [-e] device-name [device-name...]
endKat
}

# -------------------------------------------------------------------
echo_value=0
mynode="$(ng_sysname)"
reset_all=0
sys_type=$( ng_uname -s)
forreal=1

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	reset_all=1;;
		-e)	echo_value=1;;
		-n)	forreal=0;;
		-t)	sys_type=$2; shift;;		# mostly for testing

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

if (( $reset_all > 0 ))
then
	while read type dev
	do
		case $type in 
			tape)		set_var $dev;;
			library)	set_var $dev;;
			*)		;;
		esac
	done <$NG_ROOT/site/config

	cleanup 0
fi


while [[ -n $1 ]]
do
	if [[ $1 == /dev/* ]]
	then
		if [[ -e $1 ]]
		then
			set_var $1
		else
			log_msg "tp_setid: bad device name: $1"
		fi
	else
		log_msg "tp_setid: bad device name: $1"
	fi

	shift
done

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(nt_tp_setid:Set tape device pinkpages variable)

&space
&synop	ng_tp_setid [-e] dev-name [dev-name...]
&break	ng_tp_setid -a

&space
&desc	For each device on the commandline (e.g. /dev/nst0) we query the device 
	for the hardware ID and set a local pinkpages variable that maps the 
	device to the ID.  The pinkpages variable then should be used by all 
	other processes (ng_tp_getid) to map the device name to the ID in order 
	to prevent scsi collisions and thus a failure to retrieve the information.
&space
	The pinkpages variable generated will have the syntax: TAPE_<device-name>. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Attempt to determine all tape related devices on the node and set all 
	pinkpages variables.  Designed to be run with this option at node start.
&space
&term	-e : In addition to setting the information in pinkpages, the value is echoed 
	to the standard output device. 
&endterms


&space
&parms	&This expects one or more device names to be listed on the command line. 

&space
&exit
	If an error is detected, or &this cannot perform the desired operation, an 
	error code greater than zero is returned. 

&space
&see	ng_tp_changer, ng_tp_getid

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 Aug 2006 (sd) : Its beginning of time. 
&term	03 Jul 2007 (sd) : Modified to use ng_dosu to get info from sg_inq.
&term	09 Jul 2008 (sd) : Tweeked to make work with camcontrol.
&term	26 Sep 2008 (sd) : Fixed call to dosu (added path).
&endterms

&scd_end
------------------------------------------------------------------ */
