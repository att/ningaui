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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN
                                   
#
# ------------------------------------------------------------------------
#  parse the command line for options and required parameters
# ------------------------------------------------------------------------
#

ustring='file [file...]'
while [[ $1 = -* ]]		# while "first" parm has an option flag (-)
do
	case $1 in 
	--man)	show_man; cleanup 1;;
	-v)	verbose=TRUE; verb=-v;;
	-\?)	usage; cleanup 1;;
	*)		# invalid option - echo usage message 
		error_msg "Unknown option: $1"
		usage
		cleanup 1
		;; 
	esac				# end switch on next option character

	shift 
done					# end for each option character

if [[ -z $1 ]]
then 
	cleanup 1
fi

md5sum $* | ng_flist_add
cleanup $?

# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_rm_rcv_file : Add files to the local rm_db database)

&space
&synop  ng_rm_rcv_file file [file...]


&space
&desc   &ital(Ng_rm_rcv_file) adds files to the local rm_db database.  It does so 
	for each file supplied on the command line. 

&space
	The filenames supplied on the command line MUST be fully qualified.  The
	files must reside in a directory tree listed inside one of the replication 
	manager arenas.  If not, an  error message will be generated. 

&space
	More or less this is a convenient wrapper to ng_flist_add; refer to that 
	manual page for more details.

&exit
	An exit code that is non-zero indicates an error.  
&space
&examp  ng_rm_rcv_file /ningaui/fs03/00/12/file 

&space
&see    ng_flist_send, ng_repmgr, ng_flist_add

&mods
&owner(Andrew Hume)
&lgterms
&term	11 Jul 2005 (sd) : Added check to ensure commandline parms passed in, and error code returned from flist_add. 
&term	01 Aug 2006 (sd) : Fixed usage message. Updated options loop.
&endterms

&scd_end
------------------------------------------------------------------ */
