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
# Mnemonic:	list_packages
# Abstract:	lists packages that are currently in repmgr space on the node and
#		indicates the state of each (installed/not installed) as well as
#		identifying each that is desired. 
#
# Date:		26 Mar 2004 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# show a table of installation settings cluster/package
function show_settings
{

	ng_nar_get|gre "PKG_[a-z]|NG_PKG_INSTALL"| awk -v what="$1" '
	BEGIN {
		if( what )
		{
			x = split( what, a, " " );
			for(  i = 1; i <= x; i++ )
				need[a[i]]++;
		}
		else
			need_all = 1;
	}

	{
		split( $1, a, "=" ); 
		split( a[1], b, "/" );
		split( b[2], e, ":" );

		split( e[2], d, "_" );

		pkg = d[2];
		cluster = e[1];
		

		seen_cluster[cluster]++;
		seen_pkg[pkg]++;

		gsub( "\"", "", a[2] );
		data[cluster,pkg] = a[2];
	}

	END {
		printf( "%12s ", "" );
		for( p in seen_pkg )
		{
			if( need_all || need[p] )
				printf( "%10s    ", p )
		}
		printf( "\n" );

		f = "flock";
		for( c in seen_cluster )
		{
			if( c == "flock" )
				continue;

			printf( "%12s ", c );
				for( p in seen_pkg )
				{
					if( need_all || need[p] )
					{
						if( data[c,p] )
							printf( "%10s(c) ", data[c,p] );
						else
							printf( "%10s(f) ", data[f,p] );
					}
				}

			printf( "\n" );
		}

	}
	'

	cat <<endKat
	---------------------------------------------------------------------------------	
	(c) value comes from pinkpages/cluster setting
	(f) value comes from pinkpages/flock setting

	deprecated - package is no longer created/distributed
	noonstall  - package is not targed for install on the cluster
	current	   - Currenlty installed package is desired
 	latest	   - The most recently available package will be installed automatically
    	<date>	   - The package with this specific date is targeted for the cluster
endKat
}

# -------------------------------------------------------------------
ustring="[-a] [-c] [-d] [-l] [-M min] [-p prefix] [-P ppvar_prefix] [-t type] [-u dir] [package-name]"
command_only=0
verbose=0
all=1
type=rel		# package type (last -xxx)
prefix=pkg		# prefix on package files registered with repmgr
pp_prefix=PKG		# prefix on variables in pinkpages
root=$NG_ROOT
min_override=""
ng_sysname|read me
node="$me"		# look round the cluster for all installed things
deletes=0
vlevel=0
nkeep=2			# number to keep from a delete pespective
show_install=0
rm_state=""		# -s sets to -a to have rm-where show even if not stable
stability_flag=""	# -s sets to pass a -s on the refurb command
list_keep_only=0	# -K sets and we list only th epackages we'd keep

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	all=1;;
		-c)	command_only=1;;
		-d)	command_only=1; deletes=1;;			# list delete info
		-i)	show_install=1;;			# show table of install settings
		-K)	list_keep_only=1;;
		-k)	nkeep=$2; shift;;
		-l)	all=0;;				# list only the latest of each package
		-M)	min_override=$2; shift;;	# so we can subs 4.8 for 4.9 and the like
		-p)	prefix="$2"; shift;;
		-P)	pp_prefix=$2; shift;;		# so we can support PKG_NOBOOT_name  variables
		-s)	rm_state="-a"; stability_flag="-s";;			# show even if not stable
		-t)	type=$2; shift;;
		-u)	root=$2; shift;;				# unload directory

		-v)	vlevel=$(( $vlevel + 1 )); verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

verbose "sf=$stability_flag"

if (( $show_install > 0 ))
then
	x="$@"
	show_settings "$x"
	cleanup $?
fi


pkg_hist=${NG_PKG_HIST:-$NG_ROOT/site/pkg_hist}		# package installation history
#refurb_hist=$NG_ROOT/site/refurb 2>/dev/null		# history of refurb installs (we care not if it is missing)

	# this is what keeps us from doing it for another node -- we dont know their stuff
#my_uname=`whence uname`				# buggered up built-in in sh or ksh under suse
my_uname=ng_uname
$my_uname -s|read ostype			# figure out o/s type to look at
$my_uname -p|read arch
$my_uname -r|IFS=. read maj min jnk

if [[ $arch == sun4* ]]
then
	arch="${arch%?}"			# ditch u/v from sun4u sun4v as we dont care
fi

if [[ -n $min_override ]]
then
	min="$min_override"
else
	if [[ -n $NG_PKG_MINVER ]]
	then
		verbose "override of minor version: NG_PKG_MINVER = $NG_PKG_MINVER"
		min=$NG_PKG_MINVER
	fi
fi

systype="$ostype-$maj.${min%%-*}-$arch"
verbose "ostype=$ostype  arch=$arch  systype=$systype"


	# gather data: from narbalek, refurb install info, rm_where stuff; pump into awk to process all of it
(
	ng_nar_get -P "flock.*:${pp_prefix}_[a-z].*" | sed 's/[ 	]*#.*$//; s/"//g'		# order is important so node specifics override flock/cluster
	ng_nar_get -P "$NG_CLUSTER.*:${pp_prefix}_[a-z].*" |sed 's/[ 	]*#.*$//; s/"//g'
	ng_nar_get -P "$node.*:${pp_prefix}_[a-z].*" |sed 's/[ 	]*#.*$//; s/"//g'

	grep  "$systype.*$type" $refurb_hist $pkg_hist |sort -k 7,7 -u 2>/dev/null	# the installation history channel

		# MUST be last as we expect to read install data before output from rm-where
	if [[ -n $1 ]]
	then
		while [[ -n $1 ]]
		do
			ng_rm_where $rm_state -p $node "${prefix}-$1" | grep "$systype.*$type" | sed 's!/.*/!!' |sort -k 2,2 
			shift
		done
	else
		ng_rm_where $rm_state -p $node "${prefix}-" | grep "$systype.*$type" | sed 's!/.*/!!' |sort -k 2,2 
	fi

) | sed ' s/.tgz//; s/.tar//; s/.tar.gz//' | awk \
	-v pp_prefix=$pp_prefix \
	-v min_over=$min_override \
	-v root=$root \
	-v command_only=$command_only  \
	-v all=$all \
	-v verbose=$vlevel \
	-v deletes=$deletes \
	-v nkeep=$nkeep \
	-v list_keep_only=$list_keep_only \
	-v stability_flag=$stability_flag \
	'
	BEGIN {
		if( command_only )
			all = 1;			
	}

	/pinkpages/ {					# settings from narbalek; pinkpages/<scope>:PKG_pkgname=<desried>
		split( $0, a, ":" );
		split( a[2], b, "=" );
		x = split( b[1], a, "_" );
		desired[a[x]] = b[2];			# date on desired package, or the word latest
		next;
	}

		#installed: Mon Oct 18 10:33:17 2004 /ningaui/tmp/pkg-feeds-FreeBSD-4.8-i386-20041016-rel.tar
	/installed: / {							# from the installation history
		y = split( $NF, b, "/" );				# b[y] gets the basename
		got[b[y]] = $2 " " $3 " " $4 " " $5 " " $6;		# installation info for the package
		split( b[y], c, "-" );					# c[2] gets the pkg name, c[6] gets the date

		last_installed[c[2]] = c[6];				# date off of last installed package (for numeric comp)
		last_inst_info[c[2]] = got[b[y]];			# date the pkg was installed
		last_inst_file[c[2]] = b[y];				# file used for last install 

		next;
	}

				# assume crap from rm-where:	node filename junk
				# expected to be sorted by filename which should leave them in date order
	{
		x = split( $2, a, "/" );	# a[x] gets basename of file that was in repmgr space
		seen[a[x]]++;
		
		split( a[x], b, "-" );		# b[2] gets pkg name, b[6] pkg date
		pseen[b[2]] = a[x]; 		# by package; track the last (most recent)
		newest[b[2]] = b[6];		# the date stamp on the most recent

		if( got[a[x]] )				# this file was at one point installed 
		{
			iorder[a[x]] = ioidx[b[2]]+0;		# save pkg name in order so we can suggest deletes if lots of them
			ioidx[b[2]]++;
		}
		else
		{
			uorder[a[x]] = uoidx[b[2]]+0;		# save unordered
			uoidx[b[2]]++;
		}
		next;
	}

	# set want string, and return 1 if it is desired 
	function set_want( des, this )
	{
		if( des == "latest" || des == this )	 
		{
			want_str = "DESIRED>|";
			return 1;
		}
		else
			want_str = "        |";
		return 0;
	}

	END {
		if( !all )				# show only the most recent of each pkg found in repmgr space
		{
			for( y in pseen )		# for latest of  each pkg type
			{
				x = pseen[y];
				if( verbose )
					printf( "desired pkg for %s is: %s\n", y, desired[y] ) >"/dev/fd/2";
				split( x, a, "-" );


				if( last_installed[y]+0 > newest[y]+0 )	# last installed package is not in repmgr space
				{
					printf( "WARNING: last installed pkg is not in repmgr space: %s %s %s\n", y, last_inst_file[y], last_inst_info[y] )>"/dev/fd/2";
				}
				else
				{
					set_want( desired[y], a[6] );
					installed = got[x] ? got[x] : "not installed";

					printf( "%s %-45s %25s", want_str, x, installed );
					if( verbose )
						printf( "\tng_refurb %s %s -u %s -v -r %s:%s:%s\n", stability_flag, min_over ? "-M " min_over : "", root, a[2], a[6], a[1] );
					else
						printf( "\n" );
				}
			}

			exit( 0 );
		}

					# figure out which are candidates for deletes -- keep[x] if we should keep it
		for( x in seen )			# x is the full pkg name w/o tgz extension
		{
			split( x, a, "-" );
			if( got[x] ) 		# this file was installed
			{
				nseen = ioidx[a[2]]; 		# number of this pkg seen 
				rank = iorder[x];
			}
			else					# track uninstalled
			{
				nseen = uoidx[a[2]]; 		# number of this pkg seen 
				rank = uorder[x];
			}

			line = nseen - (nkeep+1);
			keep[x] = rank <= line ? 0 : 1;
		}

		if( all )
		{
			for( x in seen )		# show all pkgs found in repmgr space
			{
				split( x, a, "-" );
				this_date = a[6];
				pname = a[2];

				if( desired[pname] == "" && ! warned[pname]++ && verbose )
					printf( "WARNING: no pinkpages var %s_%s for pkg in repmgr space; ignored\n", pp_prefix, pname ) >"/dev/fd/2";

				if( verbose && desired[pname] != "latest" && !dinfo[pname]++ && !warned[pname]++ )
					printf( "NOTE: %s_%s is !latest: %s\n", pp_prefix, pname, desired[pname] ) >"/dev/fd/2";

				if( newest[pname]+0 >= last_installed[pname]+0  )		# newest can be the deisred if latest coded
					latest = newest[pname]  
				else							# most recently installed is newer than newest in repmgr
				{
					latest = last_installed[pname];		# the latest is what was installed even if not in repmgr space
					keep[x] = 1;				# and we want to keep this one too
					if( ! command_only && ! warned_notfound[pname]++ )
					{
						set_want( desired[pname] == "latest" ? latest : desired[pname], last_installed[pname] );  
						printf( "%s %-45s %25s\t(not in repmgr space!)\n", want_str, last_inst_file[pname],  got[last_inst_file[pname]] );
					}
				}
				wanted = set_want( desired[pname] == "latest" ? latest : desired[pname], this_date );  

				if( wanted )
					keep[x] = 1;

				installed = got[x] ? 1 : 0;

				if( command_only )
				{
					if( deletes )
					{
						if( ! keep[x] )
							printf( "ng_rm_unreg --force -v %s\n", x );
					}
					else
						if( ! installed  && wanted )
							printf( "ng_refurb %s %s -u %s -v -r %s:%s:%s\n",  stability_flag, min_over ? "-M " min_over : "", root, pname, this_date, a[1] );
				}
				else
				{
					if( ! list_keep_only || keep[x] )
					{
						printf( "%s %-45s %25s", want_str, x, installed ? got[x] : "not installed" );

					if( verbose )
					{
						if( keep[x] )
						{
							if( verbose > 1 )
								printf( "\tng_refurb %s -u %s -v -r %s:%s:%s",  min_over ? "-M " min_over : "", root, pname, this_date, a[1] );
						}
						else
							printf( "\t<<<candidate for unregister" );
					}
					printf( "\n" );
					}
				}
			}

		}
	}
	' |sort -k 2,2 


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_pkg_list:List pkg- files in replication manager space)

&space
&synop	ng_pkg_list [-a] [-c] [-d] [-l] [-M min-override] [-p prefix]  [-P ppvar_prefix] [-t pkg-type] [pkg-name]

&space
&desc
	&this will list the packages that are registered in replication manager 
	space on the current node.  The package name, and installation time, are 
	written for each pakcage file that is found in replication manager space. 
	If the verbose option, the 
	installation command is also listed.  An attempt is made to identify the 
	package that is currently the desired package to have installed. If the 
	desired package can be determined, then it is flagged as it is listed 
	on the standard output device. 
&space
&subcat Candidates for Unregister
	In verbose mode, package files that are candidiates for unregistration 
	from replicaiton manger space are indicated.  By default, the last
	two installed packages, and the last two uninstalled packages (if there
	are any) are not flagged for deletion. 

&space
&subcat Generating Commands
	If the command only (-c) flag is given on the commmand line, Then only 
	the commands that need to be executed to install the desired packages 	
	are written to the standard output device.  &This is silent about all 
	other packages, installed or not, and thus when running in command list 
	mode, no output indicates that there are no packages to be installed. 
&space
	If the &lit(-d) option is listed on the command line, then unregister 
	commands are generated rather than refurb commands.

&space
&subcat The Desired Package
	A package is determined to be the desired package by examining the 
	pinkpages variable &lit(PKG_)&ital(pkg-name).  The value is expeted
	to be either the word &lit(latest), or the datestamp (yyyymmdd) of 
	the package that is desired.  The variable may be set at the 
	flock, cluster and node levels and will be properly interpreted
	as this script is executed on each node. If &this is executed for 
	a package that does not have at least a flock wide variable assigned
	for it, no desired package will be determined. 
&space
	If the variable name prefix &lit(PKG) is not desired, it can be changed
	with the -P option on the command line.  

&subcat Pinkpages Variables
	These variables, if set in pinkpages, cause this script to behave
	differently:
&space
&begterms
&term	NG_PKG_HIST : Is used as the name of the installation history file. If
	not defined, then NG_ROOT/site/pkg_hist is assumed to be the file.
&space
&term	NG_PKG_MINVER : If set overrides the minimum version that is returned
	as the operating system version by &lit(uname).  This allows packages
	generated for 7.6 to be recognised on a system where uname reports 
	the version to be 7.7.  If the &lit(-M) option is supplied on 
	the command line, this variable is ignored.  If this variable is 
	not set, then packages are recognised only if the version in the 
	package name matches the operating system version exactly.

&endterms
&space


&opts	These options are recognised when placed on the command line ahead of 
	any positional parameters.
&begterms
&term	-a : All. List all packages for all O/S types that are currently 
	registered with replication manager. If not supplied only the most recently 
	created package, for the local operating system type is listed. 
	(based on the date in the package name). 
&space
&term	-c : Command only. List only the  command needed to install  the package.
	By default, the whole package name, installation date (if installed),
	and the refurb command are written for each package.
&pace
&term	-d : Generate the commands that can be issued to unregister (delete) 
	package files that are condisdered to be no longer necessary. 
&space
&term	-i : Information list.  Generates a table of installation settings by package and 
	cluster.  Indicates what package is currently desired for each cluster (deprecated,
	current, latest, specific date, not to be installed). When this option is supplied
	all other options are ignored. 
&space
&term	-k n : Keep the last n registered, and n unregistered files when generating
	or flagging deletes.  By default the number is two.
&space
&term	-K : List only the packages that are not scheduled for delete.  This is generally the 
	last 2 of each type (installed/not installed).  Primarly this is for cluster summaries.
&space
&term	-l : 	Latest mode.  Displays information about just the most recent 
	package(s) found on the node. 
&space
&term	-M n : Minimum operating syatem relase (e.g. 8 in 4.8) override.  Allows 
	a 4.8 package to be installed on a 4.9 system when -M 9 is supplied.
	This value will override the value set in the pinkpages variable
	NG_PKG_MINVER.
&space
&term	-p prefix : Allows the package name prefix (usually pkg) to be 
	specified. 
&space
&term	-P prefix : Supplies the pinkpages variable prefix to use. If not supplied
	&lit(PKG) is assumed.  
&space
&term	-s : Set rm file search state to "any." Normally files that are not stable in 
	repmgr are not listed. If this is set, then all files are listed.
&space
&term	-t type : By default the package type is &lit(rel). This allows 
	a beta, or other named type, to be listed. 
&space
&term	-u dir : Supply the unload directory that is placed into the 
	refurb command.  This is needed only if the package is NOT unloaded
	into $NG_ROOT.  (The AST packages need to be unloaded into /).

&endterms

&parms
	&This will accept a package name (e.g. potoroo)  name as a positional parameter. When 
	supplied, only information about this package type is presented.

&space
&exit	Always 0.

&space
&files
&lgterms
&term	NG_ROOT/site/pkg_hist : A history of package installation. Expected to contain one record
	per package with: installation date and the package filename that was used to do the install.
	If NG_PKG_HIST is defined in the cartulary, it is uded as the history file rather than 
	this pathname.
&endterms

&space
&see
	ng_sm_build, ng_sm_publish, ng_sm_mkpkg
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Mar 2004 (sd) : Just off the press. 
&term	06 Apr 2004 (sd) : changed name to ng_pkg_list.
&term	05 Jan 2005 (sd) : Added support for managing desired package info in narbalek.
&term	11 Jan 2005 (sd) : Added use of NG_PKG_HIST to name the history file.
&term	18 Jan 2005 (sd) : Now handles a package installed, but removed from repmgr space.
&term	20 Jan 2005 (sd) : Added support for NG_PKG_MINVER.
&term	25 Jan 2005 (sd) : Added delete flag and delete command generation.
&term	28 Jan 2005 (sd) : Updated documentation for -d.  Now treats installed and uninstalled
	package files differntly when calculating delete candidates. The last two installed and 
	the last two uninstalled (if any) packages are NOT flagged for delete.  
&term	03 Mar 2005 (sd) : Added --force to the unreg command.
&term	06 Apr 2005 (sd) : Fixed sort to sort on the filename as key -- doh.
&term	19 Jun 2005 (sd) : Removed reference to the old refurb history file. 
&term	04 Aug 2005 (sd) : Had to change \t to straight tabs in sed regular expressions to deal with 
		AST and /usr/bin differences as we switch to ast-lite.
&term	08 Sep 2005 (sd) : Added -i option.
&term	16 Dec 2005 (sd) : Changed uname call to ng_uname.
&term	01 Feb 2006 (sd) : Added -s option.
&term	18 Feb 2006 (sd) : Added -K option to list only the packages that are not scheduled for delete.
&term	21 Feb 2006 (sd) : Passes the stability option onto refurb when set. 
&term	27 Apr 2006 (sd) : Allows the pinkpages variable prefix to be altered with the -P option. 
&term	07 Oct 2008 (sd) : Tweek to pick up any sun4 package as sun has shifted to sun4v from sun4u. 
&endterms

&scd_end
------------------------------------------------------------------ */
