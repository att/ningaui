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

if [[ $# -ne 0 ]]
then
	echo "usage: $0"
	exit 1
fi

me=`ng_sysname`
echo "=========$me"

#chown ningaui:ningaui $CONFIG
#chmod 664 $CONFIG

if [[ -r /proc/scsi/scsi ]]		
then
	cat /proc/scsi/scsi | awk '
	/^Host/		{ ndev++ }
	/Type: *Medium Changer/	{ printf("library	/dev/sg%d\n", ndev-1) }
	/Type: *Sequential-Access/	{ printf("tape	/dev/nst%d\n", ntape+0); ntape++ }
	'
else
	#ls /dev/sa[0-9] |awk ' { printf( "tape  %s\n", $0 ); } '	# assume sequential access devices are tapes
	true
fi | while read t d
do
	id=`ng_tp_getid $d`
	case $t in
	library)
		echo "CHANGER_$id=$me,$d"
		ng_ppset -f CHANGER_$id "$me,$d"
		;;
	tape)
		echo "TAPE_$id=$me,$d"
		ng_ppset -f TAPE_$id "$me,$d"
		;;
	esac
done
