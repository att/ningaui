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
# Mnemonic:	scsi_map
# Abstract:	map scsi devices as best we can. gives meaning to device names like /dev/sg0 or /dev/nst0
#		
# Date:		25 Jul 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring=""
all=0
raw=0
announce_interface=0
allow_busy=0
strip=0				# for camcontrol we must strip /dev/ from name
search=""

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	all=1;;
		-b)	allow_busy=1;;
		-r)	raw=1;;
		-i)	announce_interface=1;;
		-s)	search="$2"; shift;;
		-t)	type="$type$2 "; shift;;

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

if [[ -n $type ]]
then
	type=${type% }
fi

# uncomment once camcontrol is in dosu and we can interpret its output 
# at the moment, camcontrol does not give a nice type indication like 
# sg_inq does.  we will have to provide a map (da->disk etc) when/if
# we implement camcontrol support to map scsi info on freebsd.
#
for c in camcontrol sg_inq 
do
	if whence $c >/dev/null		# go with first we find, so order is important
	then
		case $c in 
			sg_inq) 	
				if (( $announce_interface > 0 ))
				then
					echo scsi_interface sg_inq		# method for communicating with library
				fi
				dosu_cmd=sg_inq
				;;

			camcontrol) 	
				if (( $announce_interface > 0 ))
				then
					echo scsi_interface camcontrol
				fi
				dosu_cmd=cc_inq; 
				strip=1
				if [[ $type == "all" && -z search ]]
				then
						#<IBM ULT3580-TD4 7A31>             at scbus0 target 0 lun 0 (sa0,pass0)
						#<IBM 03584L32 7360>                at scbus0 target 0 lun 1 (ch0,pass1)
						#<SEAGATE ST3146807LW 0005>         at scbus2 target 0 lun 0 (da0,pass2)
					ng_dosu cc_devlist | awk '
						{ 
							split( $NF, a, "," ); 
							list = list " " substr( a[1], 2 ); 
						}
						END {
							gsub( " ", " /dev/", list );
							print list;
						}
					'| read search

					verbose "search list populated: $search"
				fi
				;;
		esac
		break
	fi
done

if [[ -z $search ]]			# allow to be blank for camcontrol to nicely populate
then
	search="/dev/sg[0-9]*"		# old Linux default to search for changer
fi

if [[ -z $dosu_cmd ]]
then
	verbose "unable to find a scsi inquiry command"
	cleanup 0
fi


for d in $(ls $search 2>/dev/null )
do
	if (( $strip > 0 ))
	then	
		sd=${d##*/}
	else
		sd=$d
	fi

	echo "device: $d"
	$NG_ROOT/bin/ng_dosu $dosu_cmd $sd 2>&1
done | awk \
	-v type="${type:-changer}" \
	-v allow_busy=$allow_busy \
	-v raw=$raw \
	-v all=$all \
	-v verbose=$verbose \
	'
	BEGIN {
		name_map["changer"] = "library";	# type names converted (from = to); if not supplied type name reported is used
	}

	raw > 0 { print; next; }

	/device: / { 
		device = $2; 
		next; 
	}

	# --------------- camcontrol based output ------------------------------
		#pass0: <SEAGATE ST3146807LW 0002> Fixed Direct Access SCSI-3 device 
		#pass0: Serial Number 3HY04DEJ0000730496PL
		#pass0: 320.000MB/s transfers (160.000MHz, offset 63, 16bit), Tagged Queueing Enabled

		#pass0: <IBM ULT3580-TD4 7A31> Removable Sequential Access SCSI-3 device 
		#pass0: Serial Number 0007854656
		#pass0: 100.000MB/s transfers , Tagged Queueing Enabled

		#pass1: <IBM 03584L32 7360> Removable Changer SCSI-3 device 
		#pass1: Serial Number 0000000229940401
		#pass1: 100.000MB/s transfers , Tagged Queueing Enabled


	/^pass.*Sequential.Access/ {
		if( type == "Tape" || type == "tape" || type == "all" )
		{
			this_type = "tape";
			snarf = 1;
			next;
		}
		
	}

	/^pass.*Changer/ {
		if( type == "changer" || type == "Changer" || type == "all" )
		{
			snarf = 1;
			this_type = name_map["changer"] ? name_map["changer"] : "changer";
		}
	}

	/^pass.*Direct/ {
		if( type == "disk" || type == "Disk" || type == "all" )
		{
			snarf = 1;
			this_type = name_map["disk"] ? name_map["disk"] : "disk";
		}
	}

	/^pass.*Serial Number/ { 		# we assume that all is not specified on bsd; no type info from camcontrol so we must assume
		if( snarf )
		{
			sn = $NF; 	 		# the user is single typing the command which makes this script less than useful.
			dname[sn] = device;
			kind[sn] = this_type;
		}

		snarf = 0;
	}

	# --------------- sg_inq based output ------------------------------
		#standard INQUIRY:
  		#PQual=0  Device_type=0  RMB=0  version=0x05  [SPC-3]
  		#[AERC=0]  [TrmTsk=0]  NormACA=0  HiSUP=0  Resp_data_format=2
  		#SCCS=0  ACC=0  TGPS=0  3PC=0  Protect=0  BQue=0
  		#EncServ=0  MultiP=0  [MChngr=0]  [ACKREQQ=0]  Addr16=0
  		#[RelAdr=0]  WBus16=0  Sync=0  Linked=0  [TranDis=0]  CmdQue=0
  		#Clocking=0x0  QAS=0  IUS=0
    		#length=96 (0x60)   Peripheral device type: disk
 		#Vendor identification: ATA     
 		#Product identification: Maxtor 6Y080M0  
 		#Product revision level: YAR5
 		#Unit serial number: Y24E154C            
		#

	/busy/ { 
		if( allow_busy )
		{
			sn = device "-busy";
			dname[sn] = device;
			kind[sn] = type;
		}
		else
			if( verbose )
				printf( "device busy: %s\n", device ) >"/dev/fd/2"; 
		next;
	 }

	/Unit serial number:/ { 
		sn = $NF; 
		if( !dname[sn] || all )
		{
			dname[sn] = dname[sn] device " "; 
			kind[sn] = kind[device];
		}
		next; 
	}

	/Peripheral device type:/ {
		if( $NF == type  || type == "all" )
			kind[device] = name_map[$NF] ? name_map[$NF] : $NF;
	}
	# --------------------------------------------------------------------

	END {
		for( s in dname )
		{
			if( kind[s] )
				printf( "%s\t%s%s\n", kind[s], dname[s], all ? " " s : "" );
		}
		
	}
'

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_scsi_map:Map SCSI devices)

&space
&synop	ng_scsi_map [-a] [-r] [-s pattern] [-t type] [-v]

&space
&desc	&This will search for scsi device files (by default sg*) in the device (/dev) 
	portion of the file tree, and will gather information from each device such that 
	the device and type are written to the standard output.  By default, only tape
	changer devices (media changes, or media libraries) are searched for.  The &lit(-t)
	option can be used to specify the desired device type, or the word "all" can be used
	to print information on all devices found.  

&space
	If a search pattern is supplied, using the &lit(-s) option (e.g. /dev/nst[0-9]*), 
	it is possible that the device will report busy and may not be listed.  An indication
	that a device reported busy is made only when in verbose mode, and is made to the 
	standard error device. The &lit(-b) flag allows for a different treatment of busy devices. 

&space
&subcat Limitations
	This script was origianlly written to help the &lit(ng_site_prep) script suss out the 
	device name of the media library changer during the population of  the 
	$NG_ROOT/site/config file. 
	On a Linux node the &lit(sg_inq)  is expected to exist and is used when collecting data.
	On FreeBSD machines, the &lit(camcontrol) command is used.
	If the script cannot find either of these commands it exits silently with a good return code.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : All information is listed. When this option is selected, all devices that 
	indicate the same serial number are presented (as opposed to just the first), and 
	the serial number is written following all of the device names.  This is possible if 
	more than one pattern is given (e.g. /dev/nst* /dev/sg*).
	Do not confuse with specifying &lit(-t all) to indicate all recognised types.
&space
&term	-b : Allow busy. If a device reports busy it is usually ignored, however this flag can 
	cause it to be assumed to be of the type being searched for and will present it 
	as though the inquery was successful and the device type matched the specified type. 
&space
&term	-i : Announce interface.  Causes the scsi_interface (camcontrol or sg_inq) to be 
	written to stdout. 
&space
&term	-r : Raw mode.  The information from the device inquery command is written and not 
	interpreted. 
&space
&term	-s pattern : Supplies one or more filename patterns (not regular expressions) to search 
	for.  This is needed on Linux machines, but not on FreeBSD machines when the &lit(-t all) 
	option is used. 
&space
&term	-t type : Inidciates an alternate type to look for.  By default, media changer devices 
	are sussed out (as if -t changer were specified).  
	The type pattern should match the last token of the device type 
	string on Linux machines, and must be disk, tape, or changer when run on FreeBSD. 
	The special type "all" can be used.  This causes all devices that match the search 
	pattern (-s) given on the command line to be presented and not any specific type. 
	When searching for changer devices, the type output is converted to the 
	word &lit(library.)
&space
	Further, if &lit(-t all) is specified on a FreeBSD system, and the search list is not
	specified, then &this will collect a list of attached SCSI devices using the &lit(devlist)
	option of &lit(camcontrol) and use that as the search pattern. 
&space
&term	-v : Verbose mode. 
&endterms


&space
&exit	An exit code of zero indicates success; all others failure.

&examp
	A pretty generic command, on linux the -s string is required, but on FreeBSD it can be omitted:
&space
	ng_scsi_map -t all -s "/dev/sg0 /dev/nst[0-9] /dev/st[0-9] /dev/ch[0-9] /dev/sa[0-9] /dev/da[0-9]"

&space
	might yield this output: 
&space
&litblkb
tape    /dev/sa0
disk    /dev/da0
library /dev/ch0
&litblke 

&space
&see	camcontrol(8), sg_inq, &seelink(tool:ng_dosu)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Jul 2007 (sd) : Its beginning of time. 
&term	09 Jul 2008 (sd) : Added camcontrol support for FreeBSD systems.
&endterms


&scd_end
------------------------------------------------------------------ */

