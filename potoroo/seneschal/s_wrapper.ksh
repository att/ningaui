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
#  Mnemonic:	ng_s_wrapper
#  Abstract: 	a generic wrapper that can be invoked from a nawab command line.
#		similar to ng_wrapper, it can redirect output without using the > and >>
#		tokens (to avoid quoting issues).  Unlike ng_wrapper, any occurance of 
#		REPMGR_FS/stuff is assumed to be an output file name which is converted
#		to a real filename using ng_spaceman_nm.  If the command executed is 
#		successful, then all of these converted filenames are registered. 
#		environment and then invokes whatever was placed on the 
#		commandline.
#  Date:	17 November 2006
#  Author: 	E. Scott Daniels
# --------------------------------------------------------------------------

NG_ROOT=${NG_ROOT:-_ROOT_PATH_}				# _ROOT_PATH_ is replaced by site-prep when installed into /usr/bin
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

PROCFUN=${NG_PROCFUN:-$NG_ROOT/lib/procfun}  # get process query oriented functions
. $PROCFUN

# ----------------------------------------------------------------------------------------------
redirect=""
verbose=0
mylog=""
rc=0
keepflag=""
forreal=1
mylog=$TMP/log/s_wrapper.$$	# default to opening a log file; -l turns it off
_cleanup_dirs=$TMP/log

while [[  $1 = -*  ]]
do
	case $1 in 
		-k)	keepflag="-k";;
		-l)	mylog="";;
		-n)	forreal=0;;
		-o*)	redirect="${redirect}${1#??}>$2=!="; shift;;
		-O*)	redirect="${redirect}${1#??}>>$2=!="; shift;;
		-v)	verbose=1;;
	
		--man|-\?) show_man; exit 1;;
		-*)	echo "unrecognised option"; show_man; exit 1;;
	esac

	shift
done

if [[ -n $mylog ]]
then
	exec 2>$mylog
fi
verbose "started: ($@)"

if [[ -z $redirect ]]
then
	if [[ "$*" != *">"* ]]		# if -o not on command line, and user command does not have >, then set default redirect
	then
		cmd_base=${1##*/}
		cmd_base=${cmd_base%%.*}
		def_usr_stdout=$TMP/log/$cmd_base.$$
		redirect=">$def_usr_stdout=!=2>&1"
		verbose "using default redirection set to: $redirect"
	else
		verbose "no stdout/err redirect generated, user command contained its own"
	fi
else
	verbose "redirection supplied via -o/O on the command line: $redirect"
fi

if [[ "$*" == *REPMGR_FS* || "$redirect" == *REPMGR_FS*  ]]		# conversion needed
then
	# convert all REPMGRFS/string to real filenames in repmgr space using ng_spaceman_nm. Must preserve tokenisation which
	# makes this a bit tricky

	for x in "$@"
	do
		ocmd="$ocmd${x}=!="
	done

	rfile=$TMP/reg_cmds.$$  			# commands execute to register output files on success
	rmfile=$TMP/rm_cmds.$$				# commands executed to remove the output files from repmgr space on failure
	awk \
		-v ocmd="${ocmd%=!=}" \
		-v redirect="${redirect%=!=}" \
		-v rfile=$rfile \
		-v rmfile=$rmfile \
		' 
		function cmd( c,	result )
		{
			result="";
			while( (c|getline)> 0 )
			{
				if( ! result )
					result = $0;	
			}
			close( c );

			return result;
		}

		function parse( buf, cmd_flag, 		start, x, a, b, y, p )
		{
			x = split( buf, a, "=!=" );
			if( cmd_flag )
			{
				start = 2;
				printf( "%s ", a[1] );				# command does not get quoted 
			}
			else
				start = 1;
			for( i = start; i <= x; i++ )
			{
				copies = 2;
				if( (p = index( a[i], "REPMGR_FS" )) > 0 )		# token must be converted
				{
						fname = substr( a[i], p+10 );
						y = split( fname, b, "/" );		# it can be REPMGR_FS[/copies]/basename
						if( y > 1 )
							copies = b[1]+0;

						if( p > 1 )				# tack on anything before REPMGR_FS
							a[i] = substr( a[i], 1, p-1 ) cmd( "ng_spaceman_nm " b[y] );
						else
							a[i] = cmd( "ng_spaceman_nm " b[y] );

					ci = split( a[i], c, ">" );
					printf( "ng_rm_register -v -c %d %s\n", copies, c[ci] ) >rfile;
					printf( "rm -f %s\n",  c[ci] ) >rmfile;
				}

				if( index( a[i], ">" ) )		# redirection token must not be quoted
					printf( "%s ", a[i] );
				else
					printf( "\"%s\" ", a[i] );
			}
		}

		BEGIN {
			parse( ocmd, 1 );
			if( redirect 0 )			# if we created redirct from -o junk, then it could have REPMGR_FS
				parse( redirect, 0 );
			printf( "\n" );
		}
	' |read tcmd  #>$TMP/cmd.$$

	verbose "transformed command = ($tcmd) redirect=($redirect)"

	if (( $forreal < 1 ))
	then
		log_msg "reg commands"
		cat $rfile
		log_msg "remove commands"
		cat $rmfile
		log_msg "-n flag set; nothing executed"
		cleanup 1
	fi


set -x
	eval $tcmd				# must eval so that redirection in the command is properly recognised
	rc=$?
set +x
	verbose "user command exited with a return of: $rc"

	if (( $rc == 0 )) 
	then
		if [[ -s $rfile ]]
		then
			verbose "registering files:"
			if (( $verbose > 0 ))
			then
				cat $rfile
			fi
			(
				set -x
				set -e
				. $rfile	
				set +x
			)
			rc=$?
			verbose "registration finished with a return of: $rc"
		else
			verbose "file registration skipped, registration command file empty"
		fi
	else
		if [[ -z $keepflag ]]		# -k not on command line, trash files
		then
			verbose "user command failed, removing output files:"
			set -x
			. $rmfile
			set +x
		else
			log_msg "user command failed, output files kept:"
			awk '{ printf( "\t%s\n", $NF ); }' <$rmfile
		fi
	fi
else
	verbose "no conversion needed for: $@"
	if (( $forreal < 1 ))
	then
		log_msg "-n set, nothing executed"
	fi

	eval "$@" "${redirect//stdout/&1}"
	verbose "user command exited with a return code of: $rc"
	rc=$?
	cleanup $keepflag $rc
fi

if (( $rc > 0 ))
then
	if [[ -n $def_usr_stdout ]]
	then
		mv $def_usr_stdout $def_usr_stdout.failed
		verbose "user command output saved from default to: $def_usr_stdout.failed"
	fi
	if [[ -n $mylog ]]
	then
		mv $mylog $mylog.failed
	fi
fi

rm -f $rmfile
rm -f $rfile

cleanup $keepflag $rc


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_s_wrapper:Seneschal command wrapper)

&space
&synop	ng_s_wrapper [-k] [-l] [-o{1|2} filename] [-v] user-command parms	

&space
&desc	&This is a wrapper similar to &lit(ng_wrapper,) but designed to facilitate 
	an easy way for commands invoked from nawab and/or seneschal to generate output 
	files  in replication manager space, and 
	to register those files upon successful completion of the user command.
	Any filename in the user command portion of the command line that contains
	&lit(REPMGR_FS/filename) will be replaced with a legitimate path in the 
	replication manager file space. It is assumed that the user-command will
	be creating the file(s) listed in this manner. 

&space
	After the user-command has successfully completetd, the file names from the commandline, that 
	were converted, will be registered with replication manager.  By default
	a replication count of 2 is used.  If a different replication count is
	desired it can be added with the &lit(REPMGR_FS) token using the 
	syntax:  &lit(REPMGR_FS/n/filename) where &ital(n) is the replication 
	count supplied with the &lit(-c) opton on the &lit(ng_rm_register)
	command.  
	
&space
&subcat	Standard output/error
	Standard output and standard error of the user command can be supplied on the command line using
	the conventional Kshell redirection symbols &lit(>) and &lit(>>.) Using the 	
	standard redirection symbols is tricky because of quoting issues and it is 
	&stress(STRONGLY) recommended that the -o or -O options be used to specify 
	output redirection. 
&space
	The user command standard output can be redirected to a file in replication manager file space in the 
	same manner as previosly described. 
&space
	When the -o and/or -O options are &stress(NOT) used on the command line, and the 
	user command does &stress(NOT) include standard redirection tokens, &this will automaticly
	redirect the user command stdout and stderr to a file in &lit($TMP/log.) The file name 
	that is used is the basename of the user command and the process id of &this. This file 
	is deleted when the user command completes successfully and is renamed to have a 
	suffix of &lit(.failed) if user command is not successful.  
&space
	The standard output  and error of &this can also be directed via the command 
	line with the standard redirection tokens.  The -l (lowercase ell) option causes
	the standard output and standard error to be redirected to a file in the 
	&lit($TMP/log) directory.  The file will automaticly be removed when the script 
	completes successfully, and will be saved if either &this failes, or the user
	command fails. The &lit(-k) option to &this causes this file to be retained 
	regardless of the final status.

&space
&opts	&This supports the following commandline options. 
&begterms
&term	-k : Keeps standard output and error files for both &this and the user command. 
	If not supplied, the standard output and error files are removed unless the return
	status is not success. This affects only default user command logging (see -O and -o
	later) and the log file for &this when &lit(-l) is used. 
&space
&term	-l : Turns off automatic redirection of the standard output/error for &this.
	If not supplied, &this will automaticly open a file in $TMP/log and redirect its
	standard output and error messages to the file. The file is removed unless the 
	keep (-k) option is also set, or in when the user command fails.
&space
&term	-o[n] file : Set the standard output for the command. If &ital(n) is supplied, 
	it is assumed to be a file descriptor number and is added to the front of the 
	redirect symbol.  For instance, &lit(-o2 foo) redirects  stderr to foo using
	2>foo.  The file descriptor can be omiited. If standard error is to be redirected
	to standard output, then the following option pairs can be used:
&space
&term	-v : Verbose mode. LOTS of information is written to the standard error device 
	when this flag is set. 
&space
&litblkb
  -o /tmp/foo.out -o2 stdout 
&litblke

	The keyword &lit(stdout) is translated to &lit(^&1) just before the command is 
	issued eliminating the need for quotes if &2 were attempted to be submitted 
	as the parameter to -o2.  

&space
&term	-O[n] file : Same as -o except that >> is used instead of > to append to the 
	file. 
&endterms
&space

&parms	All tokens on the command line, beggining with the first token that does not start with a dash, 
	are considered to be a part of the command to execute. 

&space
&exit	The exit code returned by &this is the exit code returned to &this
	by the command that it executed. 
	If the user command completes successfully, the exit code reflects the status returned
	from the attempt(s) to register the file(s).  

&space
&examp
	The following are some examples of how &this might be used. Note that redirection of 
	the output for the user command must be quoted.

&space
	Execute the user command &lit(gen_stats) and which will write its output to a file 
	in replication manager space.  If the user command fails, the standard output and error
	files from both &this and the user command are kept. The output file is also kept.
&space
   	ng_s_wrapper -v -k gen_stats -i input_data -o REPMGR_FS/1/gen_stats.20061128.gz
&space
	Execute the same command, but generate the output as redirected stdout. If the user
	command fails, the standard output and error files from &this are kept, but the 
	output file in replication manager space is delted. 
&space
   	ng_s_wrapper -v gen_stats -i input_data  ">REPMGR_FS/1/gen_stats.20061128.gz"
&space
	Use the more friendly -o1 option to &this to redirect the user command's standard out.
	Quoting issues are avoided.
&space
   	ng_s_wrapper -v gen_stats -i input_data  -o1 REPMGR_FS/1/gen_stats.20061128.gz

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	17 Nov 2006 (sd) : Original.
&endterms

&scd_end
------------------------------------------------------------------ */
