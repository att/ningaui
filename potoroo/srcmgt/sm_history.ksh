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
# Mnemonic:	sm_history
# Abstract:	show some history things
#		
# Date:		20 April 2005
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# ------------------------------------------------------------------
ustring="[-a] [d- yyyymmdd hh:mm] [-r] {package|username}"
NG_CVSHOST=${NG_CVSHOST:-cvs-host-not-defined}
export USER=${USER:-$LOGNAME}			 # bloody suns dont have user
cvsroot=${CVSROOT:-:ext:$USER@$NG_CVSHOST:/cvsroot}
export CVS_RSH=${CVS_RSH:-ssh}

ng_dt -d | read date
date="${date%??????} 00:00"
forreal=""
verbose=0
all=""
breakf=1		# primary break field
sbreakf=0		# secondary break field 	0 == no second break
raw=0
blabber=0

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	all="-a";;
		-A)	breakf=2; blabber=1;;
		-d)	date="$2"; shift;;
		-D)	date="20000101 00:00";;
		-n)	forreal="echo would execute:";;
		-r)	raw=1;;
		-s)	sbreakf=2;;
		
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



what=$1
if (( $raw > 0 ))
then
	$forreal cvs -d $cvsroot history $all -c -D "$date"  | gre ${what:-ningaui/} 
	cleanup $?
fi

$forreal cvs -d $cvsroot history $all -c -D "$date"  | gre ${what:-ningaui/} | awk -v blabber=$blabber '
	{
		id = $8 "/" $7;
		x = split( $8, a, "/" );

		if( $7 == "Makefile" )
			fname[id] = a[x] "/" $7;
		else
			fname[id] = $7;

			fname[id] = a[x] "/" $7;


		last_mod[id] = $2 " " $3;

		all_mods[id,amidx[id]+0] = last_mod[id];
		am_dev[id,amidx[id]+0] = $5;
		amidx[id]++;

		if( $1 == "A" )
			added[id] = $2;
		if( $1 == "R" )
			added[id] = "deleted";


		if( ! index( dev_list[id], $5 ) )
			dev_list[id] = dev_list[id] $5 " ";
		dev_list_count[id,$5]++;				# number of mods made by each person
		updates[id]++;
		if ( a[2] == "apps" )
			pkg[id] = a[3];
		else
			if( (pkg[id] = a[2]) == "" )
				pkg[id] = ".";
	}

	END {
		if( blabber )
		{
			for( x in last_mod )
			{
				#printf( "%-10s %-35s %18s %12s %3d %s\n", pkg[x], fname[x], "", added[x] ? added[x] : "?", updates[x], "" );
				for( i = 0; i < amidx[x]; i++ )
				{
					printf( "%-10s %-35s %18s %12s %3s %s\n", pkg[x], fname[x], all_mods[x,i], added[x], "-", am_dev[x,i] );
					added[x] = "";			# only on the first
				}
			}
		}
		else
			for( x in last_mod )
			{
				#printf( "%-10s %-35s %18s %12s %3d %s\n", pkg[x], fname[x], last_mod[x], added[x] ? added[x] : "?", updates[x], dev_list[x] );
				printf( "%-10s %-35s %18s %12s %3d ", pkg[x], fname[x], last_mod[x], added[x] ? added[x] : "?", updates[x] );
				y = split( dev_list[x], sdl, " " )		
				for( z = 1; z <= y; z++ )
					printf( "%s(%d) ", sdl[z], dev_list_count[x,sdl[z]] );
				printf( "\n" );
			}
	}
' | sort $key |awk -v breakf=$breakf -v sbreakf=$sbreakf '
	{
		if( sbreakf )
		{
			split( $(sbreakf), a, "/" );
			sb_data = a[1];
		}

		if( last != $(breakf) )
		{
			if( last1 != $1 )
				n = 100;

			if( breakf != 2 || n > 10 )
			{
				printf( "\n%-10s %-35s %18s %12s %3s %s\n", "Package", "File", "Last Mod", "Added", "Chg", "Developer(s)" );
				n = 0;
			}
			else
			{
				printf( "\n" );
			}
		}
		else
			if( sbreakf && last_sb_data != sb_data )
				printf( "\n" );

		last_sb_data = sb_data;
		last1 = $1;
		last = $(breakf);
		n++;
		print;
	}
'

exit
M 2005-04-05 13:21 +0000 daniels 1.12 narbalek.c           ningaui/potoroo/pinkpages        == <remote>
M 2005-04-05 13:44 +0000 daniels 1.4  Makefile             ningaui/potoroo/srvm             == <remote>
M 2005-04-07 01:32 +0000 daniels 1.10 pkg_list.ksh         ningaui/potoroo/tool             == <remote>
A 2005-04-07 12:38 +0000 daniels 1.1  ebub_parse.ksh       ningaui/ebub/tools               == <remote>
M 2005-04-07 15:05 +0000 daniels 1.4  srv_logger.ksh       ningaui/potoroo/tool             == <remote>
M 2005-04-08 14:08 +0000 daniels 1.3  rm_where.ksh         ningaui/potoroo/repmgr           == <remote>


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sm_history:Source history)

&space
&synop	ng_sm_history [-A] [-r] [-d yyyymmdd hh:mm] [-a [package|username]]

&space
&desc	&This will extract some history information from the source repository 
	and present it on standard output.  Information such as package, module name,
	last mod date, date added (if known) and a list of developers is given.  All information 
	is as of the current day, or the date supplied with the -d option.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : List information for all users. This option is needed when the command line 
	option is a username, and causes more informtion to be written when a package name 
	is given as the command line parameter.
&space
&term	-A : List all changes for each file since the date supplied. 
&space
&term	-d date : The date that should be used when searching for things in the repository.
	If not supplied, all changes since midnight on the current date are listed. 
&space
&term	-D : Use the 'beginning of time' as the date.  We consider January 1, 2000 to be 
	the beginning of time as far as source is concerned. 
&space
&term	-r : Raw mode.  The raw information spit out by the underlying source management tool 
	is written to the standard output device and not interpreted/formatted.  This 
	will give more granularity, but be more difficult to read. 
&endterms


&space
&parms	
	&This accepts one command line parameter which is used as a search key when pruning the information 
	from the repository.  It may be a user name or package name.  If it is a user name, then the -a option 
	is likely a manditory option in order to cause the repository to generate all of the data that it can.

&space
&exit	Always zero.

&space
&see	ng_sm_fetch, ng_sm_replace, ng_sm_new, ng_sm_tag

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	29 April 2005 (sd) : Sunrise.
&term	17 Mar 2006 (sd) : Fixed doc.
&term	21 Mar 2006 (sd) : added -A option to display all changes. 
&term	21 Jun 2007 (sd) : Added -D option.
&term	29 Nov 2007 (sd) : Added mod count to each user in brief listing.
&endterms

&scd_end
------------------------------------------------------------------ */
