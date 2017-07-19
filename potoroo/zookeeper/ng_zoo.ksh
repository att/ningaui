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
# Mnemonic: 	ng_zoo
# Abstract: 	Start w1kw and point it at the proper zoo url base.
# Parms:	None at the moment
# Returns: 	Nothing
# Date: 	10 May 2001
# Author: 	E. Scott Daniels
# ----------------------------------------------------------------------

_NODEBUG=1

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
if [[ -f $CARTULARY ]]
then
	. $CARTULARY

	STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
	. $STDFUN
fi


ustring="[-D] [-d domain-name] [-c classpath-dir]  [-s host] [-S scale] [-V view] [--man]"

debugf="/dev/null"
cluster=$NG_CLUSTER
parrot_port=$NG_PARROT_PORT
domain=""
classp="$NG_ROOT/classes"
iview=menu.v			# initial view
scale=""

while [[ $1 = -* ]]
do
	case $1 in
		-c)		classp="$2"; shift;;
		-c*)		classp="${1#-c}";;

		-D)		debugf=$TMP/$LOGNAME.zoo.log
				debugf=/dev/tty
				date >$debugf
				;;

		-d)		domain=".$2"; shift;;
		-d*)		domain=".${1#-d}";;
		-H)		wheight="-H $2"; shift;;

		-s)		host="$host$2 "; shift;;
		-s*)		host="$host${1#-l} ";;

		-p)		parrot_port=$2; shift;;
		-p*)		parrot_port="${1#-p}";;

		-S)		scale="-s $2"; shift;;
		-S*)		scale="-s ${1#-s}";;

		-t)		classp = "/home/ningaui/src/zookeeper:$classp";;	# testing
		-V|-v)		iview="$2"; shift;;
		-V*)		iview="${1#-V}";;
		-v*)		iview="${1#-v}";;	# reserved for verbose mode, but dont suporrt it yet

		-W)		wwidth="-W $2"; shift;;

		--man)		show_man; exit;;
		-\?)		usage
				exit
				;;
		*)		echo "dont understand $1"
				usage
				exit
				;;
	esac

	shift
done

if [[ -z $DISPLAY ]]
then
	echo "DISPLAY varible must be set first"
	exit 1
fi

primary_host=${host%% *}		# first host is primary for url base, others used for parrot connections
for h in $host				# construct list of hosts for w1kw command line
do
	hlist="$hlist-h $h$domain "
done


urlbase="http://$primary_host$domain"'/views/'

java -classpath "$classp/zoo.jar:$classp" w1kw_driver $wwidth $wheight -u $urlbase $hlist $scale -p $parrot_port $iview >>$debugf 2>&1 &

cleanup 0
exit 		# just in case



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_zoo:Start a graphical monitoring process to watch things)

&space
&synop	ng_zoo [-D] [-d domain-name] [-c classpath-dir]  [-H height] [-s host] [-S scale] [-V view] [-W width] [--man]
&space
&desc	This is a wrapper script that starts the W1KW process (Java) which displays
	a graphical representation of various things in the cluster.
	The graphical representations of the cluster are maintained as static views
	accessable via the HTTP daemon running on the leader.  The views are generated
	on the &lit(srv_Netif) node in each cluster, and are supplied to &lit(w1kw)
	using the HTTP daemon on that node.
&space
	Once started, W1KW will attempt to register itself with one or more
	&lit(ng_parrot) processes. The dynamic updates to any displayed view 
	are expected to be delivered in real-time by the &lit(ng_parrot) processes
	that &lit(wk1w) has registered with.  

&space
&opts	The following options may be supplied on the command line:
&begterms
&term	-D : Debug mode. A file (&lit($TMP/$LOGNAME.zoo.log)) is created and all standard 
	output and error messages from the process are directed to this file.
	If not supplied, the standard output and error are directed to &lit(/dev/null).
&space
&term	-d domain-name : Supplies the domainname to be appended to the host name 
	if it is needed to allow the process to address both the &lit(ng_parrot) process
	and access the http daemon that is used to supply the views.
&space
&term	-c classpath-dir : Supplies a directory pathname that is assumed to contain the 
	&lit(zoo.jar) file. This directory is appended to &lit(zoo.jar) when passed to 
	the JVM.
&space
&term	-H height : Specifies the initial height of the window in pixles. 
&space
&term	-s host : Supplies the name of the host(s) that is running an &lit(ng_parrot) 
	(or similar) process which will send dynamic updates to the &lit(w1kw) process that
	&this starts. In most cases, the cluster name can be used as a host name as 
	the cluster name should map in DNS to the srv_Netif for the cluster.
&space
&term	-S scale : Specifies the scale factor that each view will be transformed to before it
	is painted. A value of .5 will cause the view to be rendered at half the size as 
	it would be if this option were not supplied. Supplying a value of 1.25 causes the 
	veiws to be rendered at a scale that is twenty-five percent larger than the default.

&space
&term	-V view : Supplies the name of the initial view to load.
&space
&term	-W width : Specifies the inital window width in pixles. 
&space
&term	--man : Shows this manual page on the current display and causes &this to exit.
&endterms

&space
&see	ng_parrot, ng_zoomap

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 10 May 2001 (sd) : Original code. 
&term	30 Aug 2001 (sd) : Added height and width options.
&term	08 Oct 2004 (sd) : Revampped a bit.
&term	31 Mar 2006 (sd) : Removed setting NG_ROOT from default if not set.
&term	19 Dec 2006 (sd) : Fixed man page so that it does print.
&endterms

&scd_end
------------------------------------------------------------------ */
