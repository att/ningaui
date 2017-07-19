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
# generates a map of power "ports" to cluster node names

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function bld_script
{
	cat <<endkat 
spawn telnet $1

expect "NPS> "

send "/s\r"
expect_before {
        Invalid         { exit 1 }
        default         { send_user expect_out(buffer) }
	"Sure? (Y/N):"	{ send "y\r"; }
	"NPS> "		{ send "/x\r"; send_user "\n"; exit 0 }
}

expect 
expect
expect

endkat
}

# -------------------------------------------------------------------------

ustring="$agv0 [-a] [-l]"
log=0					# write config to the log
archive=$NG_ROOT/site/power_map
stuff=/tmp/stuff.$$			# tmp output 
verbose=0

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	doarchive=1;;
		-l)	log=1;;
		-v)	verbose=1;;
		--man) show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		*)	echo "unknown flag: $1"
			usage;
			cleanup;;
	esac

	shift
done


#addrs=${NG_POWER_STRIPS:-"135.207.4.229 135.207.4.228"}
addrs=${NG_POWER_STRIPS:-"ningnps1 ningnps2 ningnps3"}
for x in $addrs
do
	bld_script $x | /usr/bin/expect -  | grep '^ *[0-9]' | sort -u | awk -F '|' -v verbose=$verbose -v logit=$log -v strip=$x '
		 {	if( $1 + 0 > 0  && ! index( $2, "undef" ) )
			{
				if( verbose > 0 )
					printf( "%s\n", $0 );
				else
					printf( "%s %s %d\n", $2, strip, $1 );		# stuff for map
				junk = junk sprintf( "%s", substr( $0, 1, length( $0 ) - 1 ) );
			}
		 }

		END {
			if( logit )
			{
				gsub( " +", "", junk );
				gsub( "\\(undefined\\)\\|", "", junk );
				#printf( "strip=%s junk=%s\n", strip, junk );
				system( "ng_log 6 ng_power_map " "\"" strip " " junk"\"" );
			}
		}
	 '
done |sort -u >$stuff			# -u used incase confirmaion is turned on on the strip causing double info

if [[ $doarchive -gt 0 ]]
then
	if [[ -s $stuff ]]
	then
		mv  $archive.0 $archive.1
		mv  $archive $archive.0
		cat $stuff >$archive
	else
		err_msg "data not archived; zero length"
	fi
else
	cat $stuff
fi

cleanup 0

exit 0
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_power_map:Generate a map of nodes to power strips)

&space
&synop	ng_power_map [-a] [-l]

&space
&desc	&This will make inquires to each known power strip about the 
	nodes that are attached to the strip. 
	Power strip IP addresses are expected to be defined by the 
	cartulary variable &lit(NG_POWER_STRIPS), and if the variable 
	references more than one strip, the addreesses should be 
	separated with whitespace.
	The information gathered
	is formatted and written to the standard output device. Each 
	record written by &this will have the following syntax:
&space
&litblkb
	<nodename> <powerstrip> <plug>
&litblke
&space
	Where:
&begterms
&term	nodename : Is the name of the node as known to the powerstrip.
&space
&term	powerstrip : Is the IP address of the powerstrip.
&space
&term	plug : is the plug number that the node is plugged into on the strip.
&endterms

&opts	The following options are recognised if entered from the command line:
&space
&smterms
&term	-a : Archive. Causes the power map data to be archived in &lit($NG_ROOT/site/power_map)
	with two historical copies of the map maintained as &lit(power_map.0) (most recent) and
	&lit(power_map.1) in the same site directory.
&space
&term	-l : Log the configuration for each powerstrip queried in the master log.
&endterms

&files
	The file &lit(/ningaui/site/power_map) is created when the &lit(-a) (archive) option 
	is supplied on the command line. Historical copies are made when an archive is created
	and are placed into files with the same basename and either a &lit(.0) or &lit(.1) 
	extension.
&space
&mods
&owner(Scott Daniels)
&lgterms
&term 23 Jul 2001 (sd) : New stuff.
&term 26 Jul 2001 (sd) : Added ability to log config to master log.
&term	24 Aug 2001 (sd) : Added archive option.
&term	15 Oct 2007 (sd) : Correction to man page.
&endterms

&scd_end
------------------------------------------------------------------ */
