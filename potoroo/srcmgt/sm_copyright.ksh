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
# Mnemonic:	ng_sm_copyright
# Abstract:	adds a copyright box to the top of files passed in on stdin if the 
#		filename(s) have recognised extensions.
#		
# Date:		03 May 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


function gen_template
{
	cat <<endKat >$template
_BEGIN_COMMENT_BLOCK
_LINE_COMMENT_SYM ======================================================================== $id_tag 
_LINE_COMMENT_SYM                                                         
_LINE_COMMENT_SYM       This software is part of the AT&T Ningaui distribution 
_LINE_COMMENT_SYM	
_LINE_COMMENT_SYM       Copyright (c) $start_year-$end_year by AT&T Intellectual Property. All rights reserved
_LINE_COMMENT_SYM	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
_LINE_COMMENT_SYM	
_LINE_COMMENT_SYM       Ningaui software is licensed under the Common Public
_LINE_COMMENT_SYM       License, Version 1.0  by AT&T Intellectual Property.
_LINE_COMMENT_SYM                                                                      
_LINE_COMMENT_SYM       A copy of the License is available at    
_LINE_COMMENT_SYM       http://www.opensource.org/licenses/cpl1.0.txt             
_LINE_COMMENT_SYM                                                                      
_LINE_COMMENT_SYM       Information and Software Systems Research               
_LINE_COMMENT_SYM       AT&T Labs 
_LINE_COMMENT_SYM       Florham Park, NJ                            
_LINE_COMMENT_SYM	
_LINE_COMMENT_SYM	Send questions, comments via email to $mail_address@$mail_domain.
_LINE_COMMENT_SYM	
_LINE_COMMENT_SYM                                                                      
_LINE_COMMENT_SYM ======================================================================== $trail_tag
_END_COMMENT_BLOCK

endKat
}

# add the special things round the user supplied template
function gen_usr_template
{
	awk '
		BEGIN	{ printf( "_BEGIN_COMMENT_BLOCK\n" ); }
		END	{ printf( "_END_COMMENT_BLOCK\n" ); }
			{ printf( "_LINE_COMMENT_SYM %s\n", $0 ); }
	'<$1 >$template
}


# add the header to the file  
#	$1 == begin comment block
#	$2 == end comment block
#	$3 == line comment sym
function add_header
{
	typeset fn="$1"
	typeset bb="$2"
	typeset eb="$3"
	typeset ls="$4"

	if gre "==.*$id_tag" $fn >/dev/null 
	then
		log_msg "file has header; skipped: $fn"
		return
	fi

	if cp $fn $fn-		# this keeps the permissions the same as were on the original file; if we move we loose them
	then
		sed "s:_BEGIN_COMMENT_BLOCK:$bb:; s:_LINE_COMMENT_SYM:$ls:; s:_END_COMMENT_BLOCK:$eb:"  $template >$fn
		rc1=$?
		cat $fn- >>$fn
		rc2=$?

		if (( ($rc2 + $rc1) != 0 ))
		then
			log_msg "abort: error adding header to $fn"
			mv $fn- $fn
			cleanup 2
		fi

		rm -f $fn-
		verbose "header added: $fn"
		
	else
		log_msg "abort: unable to move $fn to $fn-"
		cleanup 1
	fi
		
}

# -----------------------------------------------------------------------------------
ustring=" [-p pattern | -f filelist-flie] [file1 [file2...]]"

date "+%m %d %y" | read m d y junk

template=$TMP/template.$$

start_year=2001			# these are put into the template when we cat it to file
end_year="$(date +%Y)"

mail_domain=research.att.com
mail_address=ningaui_support

id_tag="v1.0/0a157"		# we put this into each header that we generate 
printf "%1d%x%02d%d" $(($y/10)) $m $d $(($y%10)) | read  trail_tag

fpat=""				# if supplied we do a find for files in the current directory
flist=""
force=0

while [[ $1 = -* ]]
do
	case $1 in 
		-F)	force=1;;
		-f)	flist=$2; shift;;
		-p) 	fpat="$2"; shift;;
		-t)	usr_template=$2; shift;;

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

if [[ -d CVS ]]
then
	if (( $force < 1 ))
	then
		log_msg "abort: this should not be executed in a CVS checkout directory!"
		cleanup 5
	fi
fi

if [[ -n $usr_template ]]
then
	verbose "using user template: $usr_template"
	if [[ ! -f $usr_template ]]
	then
		log_msg "abort: cannot find your template: $usr_template"
		cleanup 1
	fi
	gen_usr_template $usr_template 
else
	gen_template
fi

tflist=$TMP/flist.$$
if [[ -n $fpat ]]
then
	ls | gre "$fpat" >$tflist
	
else
	if [[ -n $flist ]]
	then
		cat $flist >$tflist
	else
		if [[ -n $1 ]]				# files on the command line
		then
			for f in $*
			do
				if [[ -f $f ]]
				then
					echo "$f" >>$tflist
				fi
			done
		else
			while read x
			do
				if [[ -f $x ]]
				then
					echo $x
				fi
			done >$tflist
		fi
		
	fi
fi

if [[ ! -s $tflist ]]
then
	log_msg "no files found to add header to"
	cleanup 1
fi

cat $tflist | while read fname 
do
	case ${fname##*.} in 
		awk)	
			add_header $fname "" "" "#" 
			;;
		c)  
			add_header $fname "/*" "*/" "*"
			;;
		cx) 
			#add_header $fname
			;;
		db) 
			#add_header $fname
			;;
		gch)  
			#add_header $fname
			;;
		h)   
			add_header $fname "/*" "*/" "*"
			;;
		im) 					# scd imbed files
			add_header $fname "" "" ".**"
			;;
		java)
			add_header $fname "" "" "//"
			;;
		sh|bash|ksh|rb)
			head -1 $fname | read fl	# snarf first line and see if its #!*
			if [[ $fl == "#!"* ]]
			then
				add_header $fname "$fl" "" "#" 	# use their first #! line as the block start
			else
				add_header $fname "#" "" "#" 	# just use comment sym
			fi
			;;
		l)						# lex (like c)
			add_header $fname "/*" "*/" "*"
			;;
		m4)					# our spin stuff
			add_header $fname "/*" "*/" "" 
			;;
		man)   
			add_header $fname "/*" "*/" ""	# man stuff generated by scd from c files
			;;
		mk)   					# mk files and includes
			add_header $fname "" "" "#" 
			;;
		np)  					# nawab programmes 
			add_header $fname "" "" "#" 
			;;
		py) 
			head -1 $fname | read fl				# snarf first line and see if its #!*
			if [[ $fl == "#!"* ]]
			then
				add_header $fname "$fl" "" "#" 			# use their first #! line as the block start
			else
				add_header $fname "#!/usr/bin/env python" "" "#" 
			fi
			;;
		rota)  
			add_header $fname "" "" "#" 
			;;
		sc4)  						# stuff written in our m4 macro lang for spin
			add_header $fname  "/*" "*/" ""
			;;
		scd) 						# stand alone self contained doc
			add_header $fname "" "" ".** " 
			;;

		xfm)						# xfm source document
			add_header $fname "" "" ".** "
			;;
		y)						# yacc grammars (like c)
			add_header $fname "/*" "*/" "*"
			;;
	
		*)		
			case ${fname##*/} in				# special case whole file names
				spyglass.*) 	add_header $fname "" "" "#" ;;
				mkfile) 	add_header $fname "" "" "#" ;;
				crontab*) 	add_header $fname "" "" "#" ;;
				cartulary.*) 	add_header $fname "" "" "#" ;;

				*)		 # unknown just leave it as is
					verbose "filename has unknown type; skipped: $fname"
					;;
			esac
			;;
	esac
done
	

rm -f /tmp/*.$$





cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_copyright:Add copyright statment to source files)

&space
&synop	ng_sm_copyright [-f filelist-file | -p find-pattern | file1 [file2...]]

&space
&desc	&This reads filenames from the command line, from a file containing a list of 
	newline separated filenames, or from standard input, and inserts a copyright header
	box into the file if the filename suffix (all characters after the last dot)
	is a known type.  The header box is put in as comments and thus the suffix 
	is assumed to be meaningful (.c for C programme files, .ksh for Korn Shell 
	script files, etc.)
&space
	Each header box is given an id string and if &this finds the id string in a 
	file, an additioinal header box is not added; it is safe to give the same list
	to &this twice. 
&space
&subcat	WARNING:
	It is NOT advised to execute this command in a CVS checkout directory.  The 
	header boxes are designed to be added as the source is exported and prepared
	for external use.  This allows the copyright statement to be maintained and 
	adjusted as a part of the pacakaging process and not as a part of the source itself.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f filelist : Supplies a file that contains the list of files to add a header box 
	to. 
	If both the -f and -p options are given on the command line, it is impossible 
	to predict what this script will do. 
&space
&term	-p pattern : Is a pattern that will be used to match files in the current directory.
	The pattern is given to &ital(gre) and thus it may be any expression that &ital(gre) 
	supports. 
	If both the -f and -p options are given on the command line, it is impossible 
	to predict what this script will do. 

&space
&term	-t user-template : The filename of a user template that should be used instead of 
	the default template supplied by this script.  
&space
	The constants are replaced with the appropriate comment symbols given the type
	of source file being augmented. 
	
&endterms


&space
&parms
	If neither the -f or the -p options are used, then a list of file names may be 
	supplied on the command line.  If no files are supplied on the command line
	then &this assumes the list of files will be supplied on the standard input 
	device. 

&space
&exit
	An exit code that is not 0 indicates an error.  Error codes are accompanied
	with a textual message to the standard error device descirbing the issue. 

&space
&see	ng_sm_fetch, ng_sm_replace, ng_sm_new, ng_sm_history

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	03 May 2006 (sd) : Its beginning of time. 
&term	18 Sep 2006 (sd) : Added trailer tag to flowerbox; adjusted mail addresses.
&term	31 oct 2007 (sd) : Added recognisition of ruby (.rb) files. 
&term	13 Nov 2008 (sd) : Updated man page to include -t option.


&scd_end
------------------------------------------------------------------ */
