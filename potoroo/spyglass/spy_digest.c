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
 -----------------------------------------------------------------------------------
	stuff to deal with a digest

	mods:
		03 Jul 2006 (sd) : Added digest name to the subject line.
		20 Jul 2006 (sd) : new_digest now supports a reparsing of the config 
			file by allowing overlay of existing named digests. 
 -----------------------------------------------------------------------------------
*/



Digest_t *new_digest( char *name, char *user, int freq, char *start  )
{
	static char *tmp = NULL;
	Digest_t *new;
	char	buf[NG_BUFFER];
	int	hr;			/* start hour min if supplied */
	int	mn = 0;
	long	sec;			/* tseconds between now and start */
	
	if( ! tmp )
		tmp = ng_env( "TMP" );

	for( new = digests; new; new = new->next )
		if( strcmp( new->name, name ) == 0 )
			break;
	if( new )				/* found existing one */
	{
		ng_free( new->name );		/* ditch these things from the old block */
		ng_free( new->user );
		ng_free( new->fname );
		ng_free( new->toc_fname );
	}
	else
	{
		new = (Digest_t *) ng_malloc( sizeof( *new ), "digest block" );
		memset( new, 0, sizeof( *new ) );
		new->next = digests;
		digests = new;
	}

	new->name = strdup( name );	
	new->user = strdup( user );
	new->freq = freq;

	if( start )
	{
		hr = atoi( start );
		if( (start = strchr( start, ':' )) != NULL )
			mn = atoi( start+1 );
		new->nxt_send = ng_now() +  tsec_until_time( hr, mn );
	}
	else
		new->nxt_send = ng_now( ) + freq;

	sfsprintf( buf, sizeof( buf ), "%s/spy_digest.%s", tmp, name );
	new->fname = ng_strdup( buf );
	sfsprintf( buf, sizeof( buf ), "%s/spy_digest_toc.%s", tmp, name );
	new->toc_fname = ng_strdup( buf );

	ng_stamptime( new->nxt_send, 1, buf );
	ng_bleat( 2, "digest opened for %s freq=%dts fname=%s next-send %s", name, freq, new->fname, buf );

	return new;
}


/* send any digests that have passed their time */
void send_digests( )
{
	char	cmd[NG_BUFFER];			/* ng_mail command */
	char	*send_to = NULL;		/* expanded mailing list */
	ng_timetype now;
	Digest_t *dp;
	Sfio_t	*of;				/* we add a seperator to the toc stuff */

	if( ! digests )
		return;				/* nothing asked for, nothing attempted */

	now = ng_now( );

	for( dp = digests; dp; dp = dp->next )
	{
		if( dp->nxt_send <= now )		/* time to shove it off */
		{

			if( dp->count && ok2mail )		/* dont bother with empty digest */
			{
				if( (of = ng_sfopen( NULL, dp->toc_fname, "a" )) != NULL )
				{
					sfprintf( of, "------------------------------------------------------------------------\n\n" );
					sfclose( of );
				}

				send_to = expand_mlist( dp->user );
				ng_bleat( 2, "sending digest: %s %s -> %s", dp->fname, dp->user, send_to );
				sfsprintf( cmd, sizeof( cmd ), "cat %s %s |ng_mailer -s \"[SPY] %s: %s digest (%d or more messages)\" -T \"%s\" - ", dp->toc_fname, dp->fname, this_node, dp->name, dp->count, send_to );
				ng_bleat( 1, "sending digest: %s", cmd );
				system( cmd );

				unlink( dp->fname );
				unlink( dp->toc_fname );
				ng_free( send_to );
	
				dp->count = 0;
			}
			else
				ng_bleat( 3, "send of digest to %s skipped; count was 0 or ok2mail was off", dp->user );

			dp->nxt_send = now + dp->freq;		/* schedule next send attempt for this digest */
		}
	}
}

/* add info to a digest 
	tag is a description like "state returned to OK"
*/
void add2digests( char *desc, char *tag, char *fname, char *hfname )
{
	Digest_t *dp;
	Sfio_t	*hf;			/* header stuff */
	Sfio_t	*df;			/* data from test */
	Sfio_t	*of;			/* output file */
	long	readl;
	char	buf[NG_BUFFER];
	char	tsbuf[256];		/* timestamp buffer */
	char	toc[NG_BUFFER];		/* entry for toc */

	if( ! digests )
		return;				/* nothing asked for, nothing attempted */

	ng_stamptime( ng_now(), 1, tsbuf );		/* line for the table of contents */
	sfsprintf( toc, sizeof( tsbuf ), "%s %s: %s", tsbuf, desc, tag ? tag : "" );

	df = ng_sfopen( NULL, fname, "r" );
	hf = ng_sfopen( NULL, hfname, "r" );
	for( dp = digests; dp; dp = dp->next )
	{
		ng_bleat( 2, "updating digest: %s  %s", dp->name, fname );
		dp->count++;

		sfseek( df, 0, 0 );
		sfseek( hf, 0, 0 );	/* rewind */

		if( (of = ng_sfopen( NULL, dp->toc_fname, "a" )) != NULL )
		{
			sfprintf( of, "%s\n", toc );
			sfclose( of );
		}

		if( (of = ng_sfopen( NULL, dp->fname, "a" )) != NULL )
		{
			if( tag )
				sfprintf( of, "\n\n%s\n", tag );

			while( (readl = sfread( hf, buf, sizeof( buf ) )) > 0 )
				sfwrite( of, buf, readl );

			sfprintf( of, "\n\n" );
			while( (readl = sfread( df, buf, sizeof( buf ) )) > 0 )
				sfwrite( of, buf, readl );

			sfprintf( of, "\n========================================================================\n" );
			sfclose( of );
		}
		else
			ng_bleat( 0, "unable to open digest file: %s: %s", dp->fname, strerror( errno ) );
	}

	sfclose( df );
	sfclose( hf );
}

/* ditch the named digest -- we trash the files too */
void scratch_digest( char *name )
{
	Digest_t *dp;			/* step through list */
	Digest_t *pp = NULL;		/* pointer at one to prev */

	for( dp = digests; dp; dp = dp->next )
	{
		if( strcmp( dp->name, name ) == 0 ) 	
		{
			if( pp )
				pp->next = dp->next;
			else
				digests = dp->next;
			unlink( dp->fname );
			unlink( dp->toc_fname );
			ng_free( dp->name );
			ng_free( dp->user );
			ng_free( dp->fname );
			ng_free( dp->toc_fname );
			ng_free( dp );
			return;
		}

		pp = dp;
	}

}





