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
#!/usr/common/ast/bin/ksh
# Mnemonic:	ng_df - a wrapper round df to produce similar output on all node types
# Abstract:	Generate a consistant output from df across various platforms
#
# Note:		Sun seems to have dropped the /dev/xxxx style of reporting:
#		/l/ng_apps           16384000 2581132 12940314    17%    /l/ng_apps
#
#		rather than the expected
#		/dev/dsk/xxxx        16384000 2581132 12940314    17%    /l/ng_apps
#		
# Date:		7 Nov 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


	# parse df command for inode info
	# on linux it takes two calls to get both used and inode info
	# on the bloody suns we must construct a decent formatted output from their hodgepodge
	# if the sun device name is too long (longer than about 20 characters) the tokens are 
	# bashed together and everything is off by one. Once again we must jump through 
	# hoops to deal with junk from sun.
function inode_parse
{
	awk -v exclude="$exclude" '

		#linux:	Filesystem            Inodes   IUsed   IFree IUse% Mounted on
		#darwin:	Filesystem           1K-blocks      Used     Avail Capacity  iused    ifree %iused  Mounted on
		#bsd:	Filesystem           1K-blocks      Used     Avail Capacity iused    ifree %iused  Mounted on
		#sgi:	Filesystem           Type  kbytes     use     avail  %use   iuse  ifree %iuse  Mounted

		# sun: (multi line output with -g option)
		#/                  (/dev/dsk/c1d0s0   ):         8192 block size          1024 frag size  
		#16533438 total blocks    9248060 free blocks  9082726 available         995904 total files
  		#841690 free files     26738688 filesys id  
     		#ufs fstype       0x00000004 flag             255 filename length


	BEGIN {
		nex = split( exclude, exlist, " " );
		drop = 0;					# set if we will drop the record
	}

	# ------------------- sun stuff -------------------------------------

	
	/block size/ {
		
		if( index( $1, "(" ) )
		{
			split( $1, a, "(" ); 			# mount point is slapped against (devname)
			mt = a[1];
			split( a[2], b, ")" );			# (/dev/foo/bar )		assume space is not there!
			dev = b[1];
			bs = $4 == "block" ? $3 : $2		# depends on whether there is are spaces between dev and )
		}
		else
		{
			mt = $1;

			split( $2, a, " " );			# (/dev/foo/bar )		assume space is not there!
			dev = substr( a[1], 2 );
			bs = $5 == "block" ? $4 : $3		# depends on whether there is are spaces between dev and )
		}

		for( i = 1; i <= nex; i++ )			# see if device is in the exclude list; if it is mark to drop but...
			if( dv == exlist[i] )
				drop = 1;			# we must continue to parse as bloody suns have multiple lines per device


		next;
	}

	/total blocks/ {
		inodes = $(NF-2)+0;
		next;
	}

	/free files/ {
		used_inodes = inodes - $1
		next;
	}

	/fstype/ {
		if( ! drop )
			printf( "inode: %s %s %.0f %.0f %.0f%%\n", mt, dev, inodes, used_inodes, int((used_inodes*100)/inodes) );
		drop = 0;
		next;
	}


	# ------------- bsd, sgi, linux and darwin  -------------------------------
	/Filesystem/ {next;}

	NF > 3 {				# bloody sun, multi line, and has blank lines
		# headers all different, order from right all the same
		printf( "inode: %s %s %.0f %.0f %.0f\n", $NF, $1, $(NF-3) + $(NF-2), $(NF-2)-0, $(NF-1)+0 );
		next;
	}
'
}


# -------------------------------------------------------------------

ustring="[-a] [-h] [-u] [filename]"

local="-l"
pat="^/dev/"
exclude=""
case `uname` in 
	Linux|linux)	local="-T -t xfs -t ext3 -t hfs -t ufs";;
	Sun*|sun*)	local="-l"
			pat="^/"				# new relay nodes dont start /dev/path. 
			exclude="/devices"
			;;
	IRIX*)		local="-l";;
	Darwin|darwin)
			if [[ `uname -v` != *7.9* ]]			# not the old version, assume 10.4 or beyond
			then
				local="-T xfs,ext3,hfs,ufs"
			else
				local="-t xfs,ext3,hfs,ufs"
			fi
			;;

	*) 		local="-t xfs,ext3,hfs,ufs";;
esac

header=1
units=0
inodes=0

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	pat=".*"; local="";;			# report on everything -- dangerous
		-h)	header=0;;
		-i)	inodes=1;;
		-u)	units=1;;

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

if [[ -n $1 ]]		# must turn off if specific filesystem is given
then
	local=""
	pat=".*"
fi

if (( $inodes > 0 ))			# get inode and convert into a common format in df_data.$$
then
	sysn=`uname -s` 
	case $sysn in
		Sun*|sun*)	/bin/df $local -g |inode_parse >$TMP/df_data.$$;;
		*)		/bin/df $local -i |inode_parse >$TMP/df_data.$$;;
	esac
fi

/bin/df $local -k "$@" | gre "Filesystem|$pat" >>$TMP/df_data.$$	# add regular df stuff to df_data.$$ *inode must be first!
awk -v exclude="$exclude" -v units=$units -v header=$header '

	BEGIN {
		cvt_blks = 0;		# dont need to convert blocks to bytes 
		oneg = 1024 * 1024 * 1024;
		onem = 1024 * 1024;
		onek = 1024;

		nex = split( exclude, exlist,  " " );
	}


	/inode:/ {
		saw_inodes = 1;
		if( units )
		{
			inodes[$2] = cvt( $4 );
			iused[$2] = cvt( $5 );
		}
		else
		{
			inodes[$2] =  $4;
			iused[$2] =  $5;
		}
		ipct[$2] = $6;
		next;
	}

	{							# parse inodes will remove these, so this check is after inode: pattern match
		for( i = 1; i <= nex; i++ )			# dont parse if device is in exclude list
			if( $1 == exlist[i] )
				next;
	}

	/Filesystem/ {			# the only thing common is that the header always starts with this
		split( $2, sub_type, "-" );
		if( $2 == "Type" )		# sgi and bsd are the same order with different headers
		{
			typef = 2;
			kbytesf = 3;
			usedf = 4;
			availf = 5;
			capf = 6;
		}
		else
		if( sub_type[2] == "blocks" )		# linux/darwin/bsd	(1k-blocks or 1024-blocks)
		{
			typef = -1;
			kbytesf = 2;
			usedf = 3;
			availf = 4;
			capf = 5;

			cvt_blks=1;
		}
		else
		if( $2 == "kbytes" )		# sun		( same order as linux, but no conversion)
		{
			typef = -1;
			kbytesf = 2;
			usedf = 3;
			availf = 4;
			capf = 5;
		}
		else
		{
			printf( "unrecognised header format: (%s)\n", $0 );
			exit( 0 );
		}

		if( header )
			if( saw_inodes )
				printf( "%-20s\t%8s\t%10s\t%10s\t%10s\t%5s\t%s\n", "Filesystem", "Type", "Kbytes", "Iused", "Iavail", "Icap%", "Mount-pt" );
			else
				printf( "%-20s\t%8s\t%10s\t%10s\t%10s\t%5s\t%s\n", "Filesystem", "Type", "Kbytes", "Used", "Avail", "Cap%", "Mount-pt" );

		next;

	}

	function cvt( value )
	{
		value *= 1024;		# convert to bytes
		if( value > oneg )
			return sprintf( "%.1fG", value / oneg );
		else
		if( value > onem )
			return sprintf( "%.1fM", value / onem );
		else
		if( value > onek )
			return sprintf( "%.1fK", value / onek );
		else
			return sprintf( "%.0fb", value );
	}

	NF == 1 { 
		name = $1;
		two_line = -1;
		next;
	}

	{
		if( ! name )
			name = $1;

		if( units )
		{
			$(kbytesf) = cvt( $(kbytesf) );
			$(availf) = cvt( $(availf) );
			$(usedf) = cvt( $(usedf) );
		}


		if( saw_inodes )
		{
			printf( "%-20s\t%8s\t%10s\t%10s\t%10s\t%4d%%\t%s\n", name, typef < 0 ? "Unavail" : $(typef+two_line), $(kbytesf+two_line), iused[$NF], inodes[$NF], ipct[$NF], $(NF) );
		}
		else
			printf( "%-20s\t%8s\t%10s\t%10s\t%10s\t%5s\t%s\n", name, typef < 0 ? "Unavail" : $(typef+two_line), $(kbytesf+two_line), $(usedf+two_line), $(availf+two_line), $(capf+two_line), $(NF) );
		two_line = 0;
		name = "";
	}

	
'<$TMP/df_data.$$

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_df:Wrapper for df command)

&space
&synop	ng_df [-a] [-h] [-u]  [filename]

&space
&desc	&This provides a wrapper for the &lit(df) command producing the same output
	on all nodes, regardless of the underlying operating system.  The fields 
	produced match those produced with the &lit(AST) verison of df in order to 
	provide an easier conversion to this script.   By default, all numeric values for 
	total, used and availabile bytes are presented in kbytes (-k supplied to the 
	unerlying system &lit(df) command).  When the -u option is supplied, numeric
	values are converted to easier to read values and a unit letter (G, M, K or b) 
	is appended to the value. The following fields are written to standard output
	and are written in the order listed:
&space
&begterms
&term	Filesystem : The name of the filesystem.
&term	Type : The file system type.  Not all operating systems make this information 
	available via the &lit(df) command, so the string &lit(Unavail) is used if 
	the type information is not available.
&term	Kbytes : The total filesystem capacity.
&term	Used : The number of kbytes that are currently used on the filesystem.
&term	Avail : The number of kbytes that are currently available on the filesystem.
&term	Cap% : The current utilisation of the filesystem in terms of percent full.
&term	Mount-pt : The name in the UNIX file tree where the filesystem is mounted. 
&endterms

&space
	By default, &this will list only local filesystems in an effort to prevent
	hangs when systems hosting remote file systems (e.g. NFS) are not available.
	When the all (-a) option is supplied an attempt to list all of the filesystems
	is made, however this can cause issues when one or more remotely hosted filesytems
	are not reachable. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : List all known filesystems; may hang if a remotely mounted filesystem is not available.
&space
&term	-h : Do not show the header line.
&space
&term	-u : Present total, used and available numbers as unit values (e.g. 2.3G) rather 
	than as kbyte values.
&endterms

&parms	If a filename is provided on the command line, then the information about the 
	filesystem that houses that file is listed.  If the file is on a remotely mounted 
	filesystem, the command may block if the filesystem is not mounted. 


&space
&exit	This funciton should always exit with a zero.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Nov 2004 (sd) : Fresh from the oven.
&term	29 Mar 2005 (sd) : Added support for -a and to ignore devices that are not /dev
	by default.
&term	28 Apr 2005 (sd) : Added the -i stuff.
&term	14 Jun 2005 (sd) : Added support to set local stuff to specific disk types.
&term	21 Jun 2005 (sd) : To avoid collsion with tmp data file. 
&term	02 Nov 2005 (sd) : Modified man page.
&term	14 Nov 2005 (sd) : Darwin (Tiger) switched from -t to -T; erk.
&term	31 Mar 2006 (sd) : FIxed local option for SGI.
&term	15 May 2006 (sd) : Changed to support new type under linux, and to fix a sun quirk
		that causes the first two fields of native df to be smashed into one.
&term	26 Nov 2007 (sd) : Fixed for BSD 6.3 change to header 1024-blocks rather than 1k-blocks. 
&term	02 Oct 2008 (sd) : Yet another tweek for sun.  This time we jump through hoops because
		df -k no longer seems to return /dev/xxxx names in column 1. Further we ignore
		the /devices filesystem as it always reports 100% inode usage. 
&endterms

&scd_end
------------------------------------------------------------------ */

