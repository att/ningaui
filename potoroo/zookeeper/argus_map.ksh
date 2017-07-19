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
# monitor the argus stuff in the log



CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


trap "jaijen" 1 2 3 15			# make sure our subtask goes away too


function jaijen
{
	if [[ -n $pid ]]
	then
		kill $pid		# the background thing we started
		log_msg "killed $pid; im dying now"
	fi
	cleanup 0;		# now get out for real
}

function doview
{
		
	awk -v targets="$1" -v viewd=$NG_WEB_DIR/views '
	BEGIN {
		nnodes = split( targets, gnames, " " );
		h = 50
		x = 50
		y = 30
		th = 8;
		bars = 80;
		w = bars * 2
	}

	{
		if( last[$3] == $1 )
			next;

		split( $8, a, "=" );
		busy[$3, idx[$3]+0] = 100 - a[2];
		split( $10, a, "=" );
		pageo[$3, idx[$3]+0] = a[2];
		idx[$3]++;
		last[$3] = $1;
	}

	function graphit( name, id, title, data, nvals )
	{
		printf( "TEXT %d %d %d %d \"%s\" \"SansSerf\"\n", x, y-(th+2), th, colour, title )>viewf;
		printf( "%s_%s BARG %d %d %d %d %d %d %d ", name, id, x, y, h, w, bars, 100, -1 * (bars-1) )>viewf;
		if( nvals < 80 )
		{
			for( j= 0; j < 80 - nvals; j++ )
				printf( " 0 " )>viewf;	
			j = 0;
		}
		else
			j = nvals -80;

		for( j; j < nvals; j++ )
			printf( "%d ", data[name, j] )>viewf;
		printf( "\n" )>viewf;
	}

	END {
		for( i = 0; i <= nnodes; i++ )
		{
			if( (name = gnames[i]) != "" )
			{
				viewf = sprintf( "%s/%s_argus.v", viewd, name );
				printf( "view created: %s\n", viewf );
				graphit( name, "busy", "CPU-busy", busy, idx[name] );	
				y += h * 2;
				graphit( name, "pageo", "Page Outs/min", pageo, idx[name] );
				close( viewf );
			}

		}
	}
	
	' <$TMP/log.$$
}
# --------------------------------------------------------------------

if [[ "$@" != *-f* && "$@" != *-\?* && "$@" != *-m* ]]		
then
	PATH=".:$PATH"
	detach_log=$NG_ROOT/site/log/argus_map 
	$detach
	log_msg "detached"
fi

pid=""
makeview=0			# set to 1 (-m) to create a view and exit
parrot_host=localhost

# ------------------------ -------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-f)	;;
		-h)	parrot_host="$2"; shift;;
		-m)	make_view=1;;
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

if [[ $make_view -gt 0 ]]
then
	grep argus $NG_ROOT/site/log/master >$TMP/log.$$
	doview "$@"
	cleanup 0
fi

c=`ng_ps|grep -v grep|grep -v $$|grep -c argus_map`
if [[ $c -gt 0 ]]
then
	log_msg "aready running? c=$c"
	ng_ps |grep -v grep |grep -v $$ |grep  ng_argus_map
	cleanup 1
fi

if [[ $(uname) == SunOS ]]
then
	log_msg "sun does not support -F on tail; we give up"
	cleanup 1
fi

while true
do
	# ast tail does not support -F; neither does suns version
	/usr/bin/tail -F -n 1  $NG_ROOT/site/log/master | grep "argus.*usr=" | awk '
	function getval( what, 		a )
	{
		split( what, a, "=" );
		return a[2];
	}

	{
		now = $1+0;
		node = $3;

		split( $8, a, "=" );
		busy = 100 - a[2];

		usr = getval( $6 );
		sys = getval( $7 );

		busy = sprintf( "%d,%d", usr, sys );			# recolour bargraphs do NOT need quotes round tuples

		split( $11, a, "=" );
		load = int( a[2]  );
		#printf( "RECOLOUR %s_busy %d\n", node, busy )>"/dev/fd/2";
		if( last[$3] != $1 )
		{
			#printf( "RECOLOUR %s_busy %d\n", node, busy );
			printf( "RECOLOUR %s_busy %s\n", node, busy );
			#printf( "RECOLOUR %s_busy %s\n", node, busy )>"/dev/fd/2";
			printf( "RECOLOUR %s_load_ave %d\n", node, load );
		}

		last[$3] = $1;			# save last to remove duplicates

		fflush( );					# must force as stdout is not a tty
	}'  | ng_sendudp $parrot_host:$NG_PARROT_PORT 
done


exit 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_argus_map:Generate views from argus information)

&space
&synop	ng_argus_map [-v]

&space
&desc	&This tails the master log looking for messages from &lit(ng_argus) and generates
	the necessary &lit(w1kw) messages that are broadcast to keep the &lit(ng_argus)
	related views updated.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose mode, though not much is written.
&endterms

&space
&exit	&This runs as a daemon and so the exit code is not relivant.

&space
&see	&ital(ng_zoo) &ital(ng_bat) &ital(ng_argus)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	18 Apr 2002 (sd) : First crumbs off of the brownie.
&endterms

&scd_end
------------------------------------------------------------------ */
