/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

/*
---------------------------------------------------------------------------
  Mnemonic: 	ng_ckpt.c
  Abstract: 	Routines that assist in the creation of checkpoint files
		using a two tumbler naming system. Files are created with 
		a lead character of a or b followed by a three digit number.
		'a' files correspond to the 'high-tumbler' and 'b' files
		to the 'low tumbler'. Tumblers roll over based on the 
		max values assigned during the init call, or at 10 and 
		300 (high/low) if no max values are supplied. 
 Date: 		28 February 2002 (ported over from stream depot manager)
 Author: 	E. Scott Daniels
---------------------------------------------------------------------------
*/
#include	<unistd.h>
#include	<stdarg.h>     /* required for variable arg processing -- MUST be included EARLY to avoid ast stdio.h issues */
#include	<string.h>
#include	<time.h>
#include 	<inttypes.h>
#include	<syslog.h>
#include	<stdio.h>
#include 	<sfio.h>
#include	<stdlib.h>
#include	<errno.h>

#include	<sys/socket.h>         /* network stuff */
#include	<net/if.h>
#include 	<netinet/in.h>
#include	<netdb.h>

#include	"ningaui.h"
#include	<ng_lib.h>

static int tumbler_low = 0;
static int tumbler_high = 0;
static int tl_max = 300;
static int th_max = 10;

/* 	seed the tumblers and set maximums if provided 
	seed is low[,high] and may be null if user just wants to set maxes
*/
void ng_ckpt_seed( char *seed, int mtl, int mth )
{
        char *c;

	if( seed )
	{
        	tumbler_low = strtol( seed, 0, 0 );
        	if( (c = strchr( seed, ',' )) )
                	tumbler_high = strtol( c+1, 0, 0 );
	}

	if( mtl > 0 )
		tl_max = mtl;
	if( mth > 0 )
		th_max = mth;
	if(verbose)
		sfprintf(sfstderr, "set seed to %d,%d lim=%d,%d\n", tumbler_low, tumbler_high, tl_max, th_max);
}

/* DEPRECATED with new spaceman interface */
static char *get_spaceman_nm( char *name )
{

	Sfio_t	*f;
	char	cmd[1024];
	char	*buf;
	char	*rs = NULL;			/* return string */

        sfsprintf( cmd, sizeof( cmd ), "ng_spaceman_nm %s", name );
        if( (f = sfpopen( NULL, cmd, "r" )) != NULL )
        {
                buf = sfgetr( f, '\n', SF_STRING );

                if( buf && *buf )
                        rs = strdup( buf );
		sfclose( f );
        }
	else
		ng_bleat( 0, "ng_ckpt_name/spaceman_nm: could not create pipe to spaceman_nm command: %s", strerror( errno ) );

	return rs;
}

/*	create the next name based on current tumblers 
	basename is the user defined base name to which .annn.cpt or .bnnn.cpt is added
	if managed is ! 0, then the filename generated is in a spaceman managed environment
*/
char *ng_ckpt_name( char *basename, int managed )
{
        char    name[NG_BUFFER];
        char    *buf;                   /* string read from the command */

        if( ++tumbler_low >= tl_max )
        {
                tumbler_low = 0;
                if( ++tumbler_high >= th_max )
                        tumbler_high = 0;
                sfsprintf( name, sizeof( name ), "%s.a%03d.cpt", basename, tumbler_high );
        }
        else
                sfsprintf( name, sizeof( name ), "%s.b%03d.cpt", basename, tumbler_low );

        if( ! managed )
                return strdup( name );          /* holding back a bit, dont stick into repmgr playground */

	buf = ng_spaceman_nm( name );

	if( ! buf )
		ng_bleat( LOG_ERR * (-1), "ng_ckpt_name: did not get a name from spaceman for: %s", name  );

	return buf;
}

/*	create the next name based on current tumblers but dont update tumblers
	good to use when appl wants to delete the next ckpt file early.
	basename is the user defined base name to which .annn.cpt or .bnnn.cpt is added
	if managed is ! 0, then the filename generated is in a spaceman managed environment
*/
char *ng_ckpt_peek( char *basename, int managed )
{
        char    name[NG_BUFFER];
        char    *buf;                   /* string read from the command */
	int	tlow;
	int	thi;

	tlow = tumbler_low + 1;
	thi = tumbler_high + 1;	

        if( tlow >= tl_max )
        {
                tlow = 0;
                if( thi >= th_max )
                        thi = 0;
                sfsprintf( name, sizeof( name ), "%s.a%03d.cpt", basename, thi );
        }
        else
                sfsprintf( name, sizeof( name ), "%s.b%03d.cpt", basename, tlow );

        if( ! managed )
                return strdup( name );          /* holding back a bit, dont stick into repmgr playground */

	buf = ng_spaceman_nm( name );

	if( ! buf )
		ng_bleat( LOG_ERR * (-1), "ng_ckpt_name: did not get a name from spaceman for: %s", name  );

	return buf;
}


#ifdef SELF_TEST
main( int argc, char **argv )
{
	int i;
	char	*buf;
	char	*seed = "2,3";

	argv0 = "ng_ckpt_test";

	if( argc > 1 )		/* assume seed passed in */
		seed = argv[1];

	fprintf( stderr, "setting seed to: %s\n", seed );
	ng_ckpt_seed( seed, 10, 5 );

	for( i = 0; i < 20; i++ )
	{
		fprintf( stderr, "[%d]\tPEEK 1>>%s\n", i, ng_ckpt_peek( "scooter", 1 ) );
		fprintf( stderr, "\t PEEK 2>>%s\n",  ng_ckpt_peek( "scooter", 1 ) );
		fprintf( stderr, "\t %s\n",  ng_ckpt_name( "scooter", 1 ) );
	}
}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_ckpt:Checkpoint File Name Support)

&space
&synop	void ng_ckpt_seed( char *seed, int max_tumbler_low, int max_tumbler_hi );
&break	char *ng_ckpt_name( char *basename, int managed );
&break	char *ng_ckpt_peek( char *basename, int managed );

&space
&desc
	These routines provide a consistant method of creating a Ningaui checkpoint 
	file name with a given base component.  The file names are generated on 
	a two "tumbler" system. The basename supplied to &lit(ng_ckpt_name) is 
	appended with either &lit(annn.cpt) or &lit(bnnn.cpt) depending on the 
	state of the tumblers. If the low tumbler value is not at its maximuim, 
	then the second form of the file name &lit(bnnn.cpt) is generated with 
	&ital(nnn) replaced with the tumbler value. If the low tumbler is at its
	maximum value, then the first format of the file name is generated and 
	the low tumbler is reset to 0.  The high tumbler value is reset to zero
	when it reaches its maximum.
&space
	&lit(Ng_ckpg_seed) allows the user application to set the tumbler values
	and expects that the seed values are passed in as a string in the form 
	of &lit(lowv,highv.) The user applicaiton may also set the maximum 
	tumbler values on the call to &lit(ng_ckpt_seed) if it passes in 
	values for each maximum that are greater than zero. If &lit(ng_ckpt_seed)
	is not invoked, or values not supplied, the following defaults are used:

&space
&begterms
&term	low tumbler : Set to a value of 0. Maximum set to 300 (0-299).
&term	hi tumbler : Set to a value of 0. Maximum set to 10 (0-9).
&endterms

&space
	&lit(Ng_ckpt_name) accepts a pointer to a NULL terminated, ASCII, string
	that it uses as the basename for the complete file name. The current 
	values of the tumblers are examined and the tumbler influenced portion 
	of the name is appended to the basename.  If the &ital(managed) parameter 
	is not zero, then the function requests a directory name from &lit(ng_spaceman)
	and appends the filename to that directory name. A pointer to the buffer 
	(user must free) with the new filename is returned on success, and NULL
	is returned on error.

&space
	&lit(Ng_ckpt_peek) accepts the same parameters and returns a name in the same format
	as &lit(ng_ckpt_name) but does not update the tumblers.  This is typically 
	used when the application wants to delete the next ckpt file without having 
	to remember the filename until its time to create the next file.

&space
&see	&ital(ng_spaceman) &ital(ng_spaceman_nm)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	01 Mar 2002 (sd) : Broken out from stream depot manager code.
&term	14 May 2002 (sd) : Added peek routine
&term 	13 Sep 2004 (sd) : To ensure stdarg.h is included before stdio.h.
&term	31 Jul 2005 (sd) : changes to avoid gcc warnings.
&endterms

&scd_end
------------------------------------------------------------------ */
