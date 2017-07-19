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
# Mnemonic:	ng_uname
# Abstract:	bloody replacement for uname since there seems to be one built into 
#		ksh, and /bin/uname returns some values with spaces or dashes which
#		we just cannot tollerate. 
#		
# Date:		16 December 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# the machine arch and processor types seem to be so inconsistant between platforms
# and even between */bin/uname flavours and the ksh builtin that we need something 
# that will deliver the same thing regardless of the platform.
function do_arch
{
	os_type=`uname -s`		
	case $os_type in 
		Darwin)	parm=-p;;
		*)	parm=-m;;
	esac

	${uname_cmd:-uname} $parm|sed 's/[ -]/_/g'
}

# generate an string of information that indicates the machine arch and os name and revision
function do_arch_string
{
	do_arch | read a
	uname -s | read t
	uname -r|IFS=. read maj min jnk
	
	systype="$t-$maj.${min%%-*}-$a"
	verbose "ostype=$ostype  arch=$arch  systype=$systype"
	echo $systype
}


# -------------------------------------------------------------------
what=pass_through;		# by default pass it to uname
uname_cmd=`whence uname`

while [[ $1 = -* ]]
do
	case $1 in 
		--arch)	what=arch_string;;	

		-m)	what=arch;;
		-p)	what=arch;;

		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	pass_opts="$passopts$1 ";;
	esac

	shift
done


case $what in
	arch)
		do_arch
		;;

	arch_string)
		do_arch_string
		;;

	*)			# pass it onto /bin/uname
		$uname_cmd $pass_opts
		;;
esac
cleanup $?

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_uname:Replacement for uname command)

&space
&synop	ng_uname  [-aimnprsv]
&break	ng_uname --arch

&space
&desc	Mostly because the implementation of -m and -p are inconsistant and sometimes
	yield values with characters that ningaui processes cannot tollerate as 
	parts of file names (blanks and dashes), this function is a wrapper to the 
	system uname command.  For the most part the options on the command line 
	are passed directly to the uname command.  If the -p or -m option is given, then
	the uname command is invoked with the option that will yield a desireable, consistant
	result, and will remove undesired characters from the retult.  The result is 
	written on standard output.

&space
&opts	The options that are legitimate on the system uname command are legitimate here. See
	the manual page for uname for more information. The option &lit(--arch) can be given 
	to &this and will generate a string onto standard output that is a combination of 
	operating system name, o/s version, and machin architecture type.


&space
&exit	The result of this command is the result of the uname that is executed.


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Dec 2005 (sd) : Sunrise.
&term	23 Jan 2006 (sd) : Added --arch.
&endterms

&scd_end
------------------------------------------------------------------ */
