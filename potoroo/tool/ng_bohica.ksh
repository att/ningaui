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
# Mnemonic:	ng_bohica - cluster file sender
# Abstract:	Accepts a list of files to send and generates a nawab job to 
#		send them. We make use of the nawab jobstep generation from a file
#		which means we generate an input file and send it to the jobber 
#		host, then submit the small programme that references the input file.
#		the last step of the programme causes the list file to be deleted.
#		
# Date:		20 August 2003	
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# ---------------------------------------------------------------------------------
ustring="[-f filelistfile] [-n] [-v] dest-host "
forreal=1
flist=""
col=1			# column that file name is in in the list
vflag=""

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-_)	__=$2; shift;;
		-_*)	__=${1#-?};;

		-c)	col=$2; shift;;
		-c*)	col=${1#-?};;

		-f)	flist="$2"; shift;;
		-f*)	flist=${1#-?};;

		-n)	forreal=0;;

		-v)	verbose=1; vflag="$vflag-v ";;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

desthost=$1

flist=${flist:-/dev/fd/0}		# if not supplied, assume stdin

awk -v col=$col '
	{
		x = split( $(col), a, "/" );		# we need only the basename for nawab
		printf( "%s\n", a[x] );		
	}

'<$flist |sort -u >/tmp/flist.$$


if [[ ! -s /tmp/flist.$$ ]]
then
	log_msg "filelist contained zero files"
	cleanup 1
fi

ng_dt -i |read now
ng_sysname |read myhost
nawab_list=/tmp/nawab_in.$myhost.$now.$$


if [[ $forreal -gt 0 ]]
then
	verbose	"sending list to jobber: $srv_Jobber"

	if ! ng_ccp $vflag -d $nawab_list $srv_Jobber /tmp/flist.$$
	then
		log_msg "unable to send list to jobber node: $srv_Jobber"
		cleanup 3
	fi
fi

# make a nawab programme
prog_name=bohica_${myhost}_${now}_$$
cat <<endKat >/tmp/np.$$
programme $prog_name;

	send_files: [ %f in <contentsf("$nawab_list")> ]
		"cluster file send"
		consumes BOHICA
		priority=20
		attempts 2
		<- %f1
		cmd ng_fsend $vflag %f1 %f1 $desthost >$NG_ROOT/site/log/bohica 2>&1

	cleanup: after send_files "remove the list on the jobber"
		nodes $srv_Jobber
		cmd rm -f $nawab_list
		
endKat

if [[ $verbose -gt 0 ]]
then
	cat /tmp/flist.$$
	verbose "nawab programme:"
	cat /tmp/np.$$
else
	log_msg "nawab programme name: $prog_name"
fi

if [[ $forreal -gt 0 ]]
then
	if ! ng_nreq -s $srv_Jobber -l </tmp/np.$$
	then
		log_msg "job submission failed"
		cleanup 1
	fi
else
	log_msg "nawab job not submitted, -n option set"
fi

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_bohica:Send files from the cluster)

&space
&synop	ng_bohica [-c col] [-f filelistfile] [-n] [-v] dest-host 

&space
&desc 	&This reads a list of files from the standard input or 
	file list file (-f) and creates a &ital(nawab) job that will cause
	the files to be sent to &ital(dest-host).  
	Files are assumed to be registered with replication manager. 
	The transmission is 
	via anonymous &lit(ftp).  The &lit(ng_fsend) script is used to 
	send the files and as such the files will have a &lit(.!) appended
	to them until the transmission is complete.
&space
	The main purpose of this utility is to easily transfer files to a 
	non-cluster host. 

&space
	&This will reduce all filenames in the list to basenames, and will 
	remove duplicates from the list.  Thus, the output from an &lit(ng_rm_where) 
	command can be piped directly into &this and the list will be pruned 
	properly.

&space
&subcat Error Logging
	The &ital(nawab) programme that is submitted is setup such that error 
	messges generated are written to &lit($NG_ROOT/site/log/bohica) on 
	the node that executed the &lit(ng_fsend) command. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c col : Specifies the column of the list that &this should assume contains
	the filename.  If not supplied, the filename is assumed to be the first 
	column of input; the remainder of the columns are ignored.
&space
&term	-f file : Identifies the file that contains the list of files to send.
	If this option is omitted, then &this will read the standard input 
	device for the list of files. 
&space
&term	-n : No execution mode.  Goes through the motions but does not schedule 
	a &ital(nawab) programme.
&space
&term	-v : Verbose mode.  The list of files and the &ital(nawab) programme are displayed
	on the standard error device.
&endterms


&space
&parms	&This expects a single positional parameter on the command line: the destination 
	host name. 

&space
&exit	A non-zero exit code indicates some sort of error during processing. Errors reported
	should be accompanied with a message to standard error. 
&space
&examp
	The following illstrates how &this can be invoked with a list from &lit(ng_rm_where):
&space
&litblkb
  ng_rm_where -p -n pinkpages| ng_bohica -c 2 -v remotehost.att.com
&litblke

&space
&see	&ital(ng_nawab), &ital(ng_fsend), &ital(ng_rm_where)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Aug 2003 (sd) : Fresh from the oven.
&endterms

&scd_end
------------------------------------------------------------------ */
