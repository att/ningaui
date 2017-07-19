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
# Mnemonic:	setbcast.ksh
# Abstract:	Decides what the broadcast interface name is -- should be run at 
#		boot time.  Updates the local cartlulary with the variable that 
#		is supplied with -V option, or NG_LOG_IF if -V not supplied.
#		Care must be taken as the output of ifconfig is different under 
#		linux than other hosts.  Right now we do not do much looking at 
#		the fields past the first so we are not affected, but if we need
#		and/or want to dig the actual bcast address we will need to pay 
#		more attention to such things.
#		
# Date:		27 May 2003 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


ustring="-N network -V variablename"
net=$NG_BROADCAST_NET		# which network do we broadcast on (e.g. 135.127.3)
vname=LOG_IF_NAME		# default to the log pp variable as the one to set
setit=1

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-N)	net=$2; shift;;
		-n)	setit=0;;

		-V)	vname="$2"; shift;;
		-V*)	vname=${1#-?};;

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

if [[ -z $net ]]
then
	log_msg "abort: cannot determine network address for broadcasts; NG_BCAST_NET not set or -N not supplied"
	cleanup 1
fi

case `uname -s` in 
	IRIX*)	ifconfig_cmd=/usr/etc/ifconfig;;		# bloody irix has to be different
	*)	ifconfig_cmd=/sbin/ifconfig;;
esac

$ifconfig_cmd -a |awk -v net="$net" '
	BEGIN { 
		data = 0;
		header = 1;
		type = header;
	}
	/^\t/	{ type = data }

	{
		match( $0, "[a-zA-Z]" )
		if( RSTART <=1 )
			iface = $1;
		else 
		{
			if( index( $0, net "." ) )
			{
				gsub( ":", "", iface );		# some flavours end with colon
				print iface;
				exit;
			}
		}
	}
'| read ifname

if [[ -z $ifname ]]
then
	log_msg "abort: could not determine broadcast interface for net: $net"
	cleanup 1
fi

if [[ $setit -lt 1 ]]
then
	if [[ $verbose -gt 0 ]]
	then
		log_msg "broadcast interface for net ($net) is: $ifname"
	else
		echo $ifname
	fi
	verbose "pinpages not modified, noaction (-n) flag set"
	cleanup 0
fi

verbose "broadcast interface for net ($net) set to: $ifname"
ng_ppset -l -c "broadcast interface name" $vname=$ifname  



cleanup 0
exit

eth0      Link encap:Ethernet  HWaddr 00:02:B3:C7:88:37  
          inet addr:192.168.5.232  Bcast:192.168.5.255  Mask:255.255.255.0
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:55451 errors:0 dropped:0 overruns:0 frame:0
          TX packets:13908 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:100 
          Interrupt:19 Base address:0xd000 Memory:fd000000-fd020000 

eth1      Link encap:Ethernet  HWaddr 00:E0:18:02:6D:24  
          inet addr:135.207.4.232  Bcast:135.207.4.255  Mask:255.255.255.224
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:33952 errors:0 dropped:0 overruns:0 frame:0
          TX packets:21183 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:100 
          Interrupt:20 Base address:0x2000 




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_setbcast:Determine Broadcast Device Name)

&space
&synop	ng_setbcast [-n] [-N network] [-V variablename]

&desc 
&This will determine which interface has been associated with 
the network address provided either by command line or cartulary 
variable (NG_BCAST_NET).  The name of the interface that is associated 
with the network is assigned to the &ital(pinkpages) variable that 
was named on the command line (-V) or to the &lit(LOG_IF_NAME) if 
not supplied on the command line. 

.sp
The primary need for this script is to allow the interface supporting the 
broadcast network to change as is needed allowing the logging software
to be fed the interface name via pinpages information.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-n : No set mode.  The value will be echoed to the stdout device rather than 
	assigning it to a cartulary variable.
&space
&term	-N network : The dot separated network IP address that is searched for.
&space
&term	-V varname : The name of the cartulary variable to have the interface name
	assigned to.
&endterms

&space
&exit	0 if the interface was determined and the variable assigned. 

&space
&see	ng_pinkpages, ng_log, ng_logd

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 May 2003 (sd) : Thin end of the wedge.
&term	02 Jul 2003 (sd) : Added better verbose messages and doc.
&term	16 Jan 2006 (sd) : Added special code to find ifconfig on irix.
&term 	10 Oct 2006 (sd) : Default for net (pinkpage var) corrected: NG_BROADCAST_NET is 
		the right value. 
&endterms

&scd_end
------------------------------------------------------------------ */
