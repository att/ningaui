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
#  unreg_files.ksh pattern [rm_req options -- like -s host]
# unregister a bunch of related files
# Mnemonic:	unreg_files (likely to become rm_unreg
# Abstract:	Creates a list of files based on a pattern, and optionally
#		unregisters them.
#		
# Date:		14 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# asks a question, reads an answer. if answer is y|Y then $2 is executed (if 
# supplied) and return is true.  if q is entered, then we abort the script
# if prompt is 0,then we always return 0
function ask
{
	typeset ans=""
	typeset rc=0

	if (( $prompt == 0 ))
	then
		ans="Y"
	else
		printf "%s " "$1"
		read ans
	fi

	case $ans in
		y|Y|yes|Yes|YES)
			if [[ -n $2 ]]
			then
				eval $2
			fi
			;;

		q|Q|quit)
			cleanup 1;;

		*)	rc=1;;
	esac

	return $rc
}

# -------------------------------------------------------------------
ustring="regex-pattern"
clist=""
filer=${srv_Filer:-no-filer}
copies=0			# default number of copies -c to change
prompt=1

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	copies="$2"; shift;;
		--force)	prompt=0;;			# UNDOCUMENTED -- no prompt just does; danger major!

		-l)	clist=$2; shift;;

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

if [[ $copies -gt 0 ]]
then
	log_msg "files will NOT be unregistered, their number of copies will be set to: $copies"
fi

if [[ -z $filer ]]
then
	log_msg "could not determine filer"
	cleanup 1
fi

if [[ -n $clist ]]
then
	verbose "submitting command list to replication manager from: $clist"
	grep "^file " $clist |ng_rmreq -s $filer
	cleanup $?
fi

if [[ -z $1 ]]
then
	usage
	cleanup 1
fi

pat=$1

ng_rm_where -s -5 -n -p "$pat" >/tmp/list.$$
wc -l </tmp/list.$$ |read count

if [[ $count -lt 1 ]]
then
	printf "no files matched pattern: $pat\n"
	cleanup 0
fi

awk '
	{ t += $NF; }
	END { printf( "%.2f\n", t/(1024*1024) ); }
'< /tmp/list.$$ |read meg

if (( $prompt > 0 ))
then
	ask "Found $count matches, will free up ${meg}Mb, list them? " "${PAGER:-more} /tmp/list.$$"
	ask "save list? " "cp /tmp/list.$$ /tmp/$LOGNAME.ur; echo \"file list saved in /tmp/$LOGNAME.ur\""
else
	log_msg "$count matches, will free up ${meg}Mb, deleting... "	# we will because if we are here the next ask defaults to true
	
	echo "------- deleted on: `ng_dt -c`" >>$TMP/$LOGNAME.ur
	cat /tmp/list.$$ >>$TMP/$LOGNAME.ur
fi

if ask "unregister them? "
then
	awk -v copies="$copies" ' 
	{
		x = split( $2, a, "/" );
		if( ! seen[a[x]]++ )
		{
			printf( "file %s %s %s %s\n", $3, $2, $4, copies );
		}
	}
	' </tmp/list.$$ |ng_rmreq -s $filer
else
	if ask "nothing unregistered; save repmgr commands? "
	then
		awk -v copies="$copies" ' 
		{
			x = split( $2, a, "/" );
			if( ! seen[a[x]]++ )
			{
				printf( "file %s %s %s %s\n", $3, $2, $4, copies );
			}
		}
		' </tmp/list.$$ >/tmp/$LOGNAME.urc
		printf "rmreq commands have been saved in /tmp/$LOGNAME.urc\n"
	fi
fi

cleanup 0
exit


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_unreg:Unregister files en mass)

&space
&synop	ng_rm_unreg {-l cmd-file | regex-pattern}

&space
&desc	&This will create a list of files currently managed by the 
	replication manager which match the supplied pattern and will 
	optionally unregister them.  
	Once the list of files is created, the user is offered the opportunity
	to view the list at the tty, or to save the list to a file. The user 
	is also prompted as to whether or not the list of files should be 
	given to the replication manager for deletion.  
&space
	If the user opts not to delete the files, &this will prompt the user as to 
	their desire to save the list of replication manager commands that would 
	delete the files. This allows the list to be edited, keeping the commands 
	to delete the desired files, as the edited list can be supplied to &this
	(using the -l option) to cause the files to be deleted.
&space
	This script is meant to be used
	interactively as it can potentially cause the removal of more 
	files than desired depending on the pattern that is supplied.
	It is highly recommended that the list of files which matched the supplied 
	pattern be verified before allowing the files to be removed.

&space
	At any of the prompts made by &this, the response of a 'q' will cause the 
	script to quit processing and exit. The answer to the prompt given the 
	'q' will be assumed to be negative and the operation will not be preformed 
	before quitting.
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-l cmd-file : Supplies the name of a command file that was previously generated
	by &this.  The commands are given to the replication manager for processing.
	This allows the user to prune a list of targeted files before deleting them.
	A minimal amount of validation will be preformed on the command list before giving it 
	to the replication manager.
&endterms

&space
&parms  &This expects that one positional parameter will be supplied  on the command line
	if the &lit(-l) option is not provided.  This parameter is a regular expression 
	pattern that will be used to match files in the replication manager domain. It 
	must be kept in mind that the pattern to match files will NOT be the same as 
	the glob expansion pattern that would be given on a shell command line. 

&space
&exit	Zero if good, something other than zero indicates an error. Error messages should
	be generated.

&files	If the user opts to save either the file list or the command list, &this creates 
	the files in the &lit(/tmp) directory using the login name of the user. The files
	are given suffixes of &lit(^.ur) (unregister) and &lit(^.urc) (unregister commands)
	for the file list and command list respectively. 

&space
&see	ng_rm_register, ng_rmreq

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 May 2003 (sd) : Epoch.
&term	19 Oct 2004 (sd) : removed all ng_leader references.
.**	21 Jan 2005 (sd) : added secret --force option for scripts (danger major, so its
	kept on the QT).
&endterms

&scd_end
------------------------------------------------------------------ */
