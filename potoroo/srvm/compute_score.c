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
##
##NOTE: This code is imbedded into the C source generated when srvm.sc4
##	is parsed to generate C. This code is not compiled by itself.
##
##-----------------------------------------------------------------------
##Mnemonic:	compute_score
##Abstract: 	computes the node's score with regard to starting the 
##		named service. Does this by invoking the score 
##		command and reading the first record written to the 
##		standard output. Optionally the score calculation process
##		may return the scope that we will use when creating the 
##		narbalek variable name.  This allows something like a 
##		DNS service to determine the lan segment and scope itself
##		based on the current lan segment. 
##Date:		11 Feb 2005
##Author: 	E. Scott Daniels
##
##Mods:		25 Feb 2005 (sd) : Added scope ability
##		31 Mar 2005 (sd) : Corrected problem when scope changed from 
##			null to a real value.
##		12 Aug 2005 (sd) : Sets a score of 0 if the computation takes 
##			more than 3 seconds.
##-----------------------------------------------------------------------
##
##	byte sc = 0; is set in the sc4 code that calls us -- we must set
##	it before we return.  it is a byte, rather than int, to keep the 
##	simulation possibilities to a minimum (256).
*/

	char	*buf = NULL;		/* return from command */
	char	*p = NULL;		/* pointer into buffer at scope if returned */
	ng_timetype started = 0;	/* we must have a score returned to us fairly quick to consider it legit */
	ng_timetype ended = 0;
	int	max_wait = 3;		/* max seconds to wait for a score -- after this we toss it out */

	Sfio_t	*f;

	if( !sp || !sp->score )
		return 0;

	max_wait = (sp->election_len / 5 )*10;
	if( max_wait <= 0 )
		max_wait = 30;
	else
		if( max_wait > 600 )
			max_wait = 600;

	started = ng_now( );
	if( (f = sfpopen( NULL, sp->score_cmd, "rw" )) != NULL )
	{
		buf = sfgetr( f, '\n', SF_STRING );
		if( buf )
		{
			sp->score = atoi( buf );		/* must leave sc set for compat with spin */
			if( sp->score > 100 )
				sp->score = 100;		/* not any higher than this */
			sc = sp->score;				/* must leave sc set for spin compatability */

			if( (p = strchr( buf, ':' )) )
			{
				p++;
				if( sp->sscope == NULL || strcmp( p, sp->sscope ) != 0 )		/* scope has changed */
				{
					ng_bleat( 1, " for srv=%s dropping out of election; scope changed %s -> %s", sp->name, sp->sscope ? sp->sscope : "undefined",  p );
					if( sp->sscope )
						ng_free( sp->sscope );
					sp->sscope = ng_strdup( p );
					sc = sp->score = 0;			/* must drop out of current election -- its the wrong scope */
				}
			}
			else
			{
				if( sp->sscope )
				{
					ng_free( sp->sscope );
					sp->sscope = NULL;
				}
			}
		}
		else
		{
			ng_bleat( 1, "compute score: score process failed to open!" );
			sp->score = 0;
		}
		sfclose( f );	

		ended = ng_now();

		if( ended - started > max_wait )		/* must have answer within 3sec */
		{
			ng_bleat( 0, "===WARNING=== computation of score took too long %ds score=%d", (ended-started)/10, sp->score );
			sc = sp->score = 150;		/* bogus -- force a recomp next time */
		}
	}
	else
		ng_bleat( 0, "unable to execute score process for %s: %s", sp->name, sp->score_cmd );

	ng_bleat( 1, " for srv=%s score calculated: %d scope=%s", sp->name, sc, sp->sscope ? sp->sscope : "not supplied" );
