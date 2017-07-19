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

#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"ningaui.h"
#include	"ng_config.h"


#define 	CATMAN_MSG_VERSION "03"		/* message version needs to be put in front of all op codes (e.g. 03map) */
int	cat_bypass = 0;

typedef struct
{
	char	*name;
	int	dsn;
} Dsn;
Dsn	*dsns;
Dsn	*edsns;
static	void	ng_init_dsns(void);

static	int cat_in = -1;		/* file des for comm with catalogue -- in and out */
static	int cat_out = -1;
static	Sfio_t *cat_sfin;

/* ----------- These routines obsolete the original mappnig routines that read the real catalogue ------------- */
/* connect to catman on catalaogue host -- returns 1 for ok */
static int catconn( )
{
	char *port = NULL;
	char *host = NULL;
	int rc;			/* return from rx call */

	if( cat_in >= 0 )
		return 1;

	port = ng_env( "NG_CATMAN_PORT" );
	host = ng_env( "srv_Catalogue" );

        if( !port || !host)
        {
                ng_bleat( 0, "unable to get catman port (NG_CATMAN_PORT) and/or host (srv_Catalogue) from env" );
                return 0;
        }
 
        ng_bleat( 3, "attempting session to catman: %s:%s", host, port  );

        rc = ng_rx( host, port, &cat_in, &cat_out );                  /* returns 0 for all ok */
	cat_sfin = sfnew( NULL, NULL, 0, cat_in, SF_READ );
	ng_bleat( 3, "connect status to %s:%s = %d", host, port, rc );
	free( host );
	free( port );

	return ! rc;
}
        
int ng_cat_name2dsn( char *name )
{
	char req[2048];
	char *buf;
	int n;

	if( ! catconn( ) )
	{
		ng_bleat( 0, "name2dsn: abort: connection could not be established n=%d", n );
		return 0;
	}

	sfsprintf( req, sizeof( req ), "%smap::%s::::::::::\n", CATMAN_MSG_VERSION, name );
	write( cat_out, req, strlen( req ) );
	if( (buf = sfgetr( cat_sfin, '\n', SF_STRING)) != NULL )
	{
		n = atoi( buf );

		while( buf && strcmp( buf, "~done" ) != 0 )
			buf = sfgetr( cat_sfin, '\n', SF_STRING);
	
		return n;
	}
	else
		return 0;
}

char *ng_cat_dsn2name( int dsn )
{
	char *buf;
	char	req[2048];
	int n;
	int done = 0;
	char	*name;

	if( ! catconn( ) )
	{
		ng_bleat( 0, "name2dsn: abort: connection could not be established" );
		return 0;
	}

	sfsprintf( req, sizeof( req ), "%smap:%d:::::::::::\n", CATMAN_MSG_VERSION, dsn );	/* number of :s must match expected field count */
	write( cat_out, req, strlen( req ) );

	if( (buf = sfgetr( cat_sfin, '\n', SF_STRING)) != NULL )
	{
		name = strdup( buf );

		while( buf && strcmp( buf, "~done" ) != 0 )
			buf = sfgetr( cat_sfin, '\n', SF_STRING);
	
		return name;
	}
	else
		return NULL;
}

void ng_cat_close( )
{
	close( cat_out );
	sfclose( cat_sfin );
	cat_out = cat_in = -1;
}


int ng_name_dsn(char *name)
{
	return -1;
	/*return ng_cat_name2dsn( name ); */
}

char * ng_dsn_name(int dsn)
{
	return strdup( "unavailable" );
	/*return ng_cat_dsn2name( dsn );*/
}

/* ------------------------- original routines ---------------------------------------- */
int
xng_name_dsn(char *name)
{
	Dsn *d;

	if( cat_bypass )
		return 0;

	if(dsns == 0)
		ng_init_dsns();
	if(edsns == 0)			/* no dsns */
		return 0;
	if(name == 0)
		return 0;
	for(d = dsns; d < edsns; d++)
		if(strcmp(name, d->name) == 0)
			return d->dsn;
	return 0;
}

char *
xng_dsn_name(int dsn)
{
	static char buf[64];
	int h, l, m;

	if(cat_bypass)
		goto unknown;
	if(dsns == 0)
		ng_init_dsns();
	if(edsns){
		l = 0; h = edsns-dsns;
		/* sleazy hack: estimate start positions */
		if(dsn >= h)
			goto unknown;
		if((dsn > 0) && (dsns[dsn-1].dsn == dsn))
			return dsns[dsn-1].name;
		while(l < h-1){
			m = (h+l)/2;
			if(dsns[m].dsn > dsn)
				h = m;
			else
				l = m;
		}
		if(dsns[l].dsn == dsn)
			return dsns[l].name;
	}
unknown:
	sprintf(buf, "<unknown dsn %d>", dsn);
	return buf;
}

static int
cmp(const void *a, const void *b)
{
	Dsn *aa = (Dsn *)a, *bb = (Dsn *)b;

	return aa->dsn - bb->dsn;
}

static void
ng_init_dsns(void)
{
	FILE *fp;
	char buf[NG_BUFFER];
	char fname[1024];
	char	*cluster = NULL;
	int n, i;
	Dsn *d;
	char 	*np;

	edsns = 0;

	if( (cluster = ng_env( "NG_CLUSTER" )) == NULL )
	{
		sfprintf(sfstderr, "%s: can't dig cluster name from cartulary ", argv0);
		exit( 1 );
	}

	np = ng_rootnm( DSNPATH );
	sfsprintf( fname, sizeof( fname ), "%s/%s_catalogue", np, cluster );
	free( np );

	if((fp = fopen(fname, "r")) == NULL){
		sfprintf(sfstderr, "%s: can't open dsn file; bypassing dsn to name translation ", argv0);
		perror(fname);
		/*exit(1);
		*/
		cat_bypass = 1;
		return;
	}
	i = 1000;
	dsns = ng_malloc(i*sizeof(dsns[0]), "dsn map");
	d = dsns;
	while(fgets(buf, sizeof buf, fp)){
		char *s, *r;

		s = strchr(buf, '|');
		if(s == 0)
			continue;
		*s++ = 0;
		r = strchr(s, '|');
		if(r == 0)
			continue;
		*r = 0;
		d->name = ng_strdup(s);
		d->dsn = strtol(buf, 0, 10);
		d++;
		if(d >= dsns+i){
			n = i;
			i *= 2;
			dsns = ng_realloc(dsns, i*sizeof(dsns[0]), "dsn map");
			d = dsns+n;
		}
	}
	edsns = d;
	qsort(dsns, edsns-dsns, sizeof(*dsns), cmp);
	if(verbose > 1)
		sfprintf(sfstderr, "%s: read %d dsn maps\n", argv0, edsns-dsns);
	fclose(fp);
}

#ifdef SELF_TEST

int main( )
{
	extern int verbose;

	char *buf;
	char *name;
	int	i;

	verbose=2;

	printf( "ng_dsn: enter fid numbers, one per line; -1 to end.\n> " );
	while( (buf = sfgetr( sfstdin, '\n', SF_STRING )) != NULL )
	{
		if( atoi( buf ) < 0 )
		{
			ng_cat_close( );
			return 0;
		}

		name = ng_cat_dsn2name( atoi( buf ) );
		i = ng_cat_name2dsn( name );
		printf( "%d --> %s -->\n> ", atoi( buf ), name, i );
		free( name );
	}
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
#ifdef NEVER_DEFINED
&scd_start
&doc_title(ng_dsn:Catalogue name and file id translation)

&space
&synop	char *ng_cat_dsn2name( int dsn ) &break
	int   ng_cat_name2dsn( char *name );  &break
	void  ng_cat_close( );

&space
&desc
	Thees routines accept either a catalogue file name or file id number (dsn) 
	and query the catalogue to determine the appropriate reverse mapping. 
	The catalogue manager, &ital(catman), is assumed to be running on the 
	node that is listed in the cartulary as &lit(srv_Catalogue) and listening 
	on the port &lit(NG_CATMAN_PORT). 
&space
	On the first translation call, a TCP/IP session to the catalogue manager is 
	established.  This session is maintained until the process terminates or 
	calls &lit(ng_cat_close). Thus for long running programmes, that do not need
	to make frequent calls, the catalogue session should be closed when not needed.

&space
&exit	The two translation functions return either a poitner to the filename, or the 
	integer file number.  If a NULL pointer or the file id 0 is returned the name
	or filenumber was not found in the catalogue, or a connection could not be established.
	Diagnostics are written to stderr if problems occur. 

&space
&see	ng_catalogue, ng_catman

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	14 Oct 2003 (sd) : Added cat_ functions.
&term	12 May 2004 (sd) : Added catman msg version stuff.
&endterms

&scd_end
#endif
-------------------------------------------------------------------*/
