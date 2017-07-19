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
 --------------------------------------------------------------------------------------
  Mnemonic:	n_uif.c
  Abstract: 	This contains the user interface routines invoked when a udp message
		is received
  Date:		18 January 2002
  Author: 	E. Scott Daniels

  mods:		23 Feb 2004 (sd) - added submitted timestamp to explain
		12 Aug 2004 (sd) - fixed purge -- bad test for null message
		09 Aug 2005 (sd) - corrected data passing to explain all -- oppteron issues
 --------------------------------------------------------------------------------------
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"ng_tokenise.h"
#include	"nawab.h"

/* ------------------ support stuff ----------------------- */
void explain_troupe( Programme *p, Troupe *tp )
{
	Troupe	*bp;			/* pointer at troupe that blocks this one */
	int	i;

	dump_troupe( tp );
	for( bp = p->troupes; bp; bp = bp->next )
	{
		
		for( i = 0; i < bp->bidx; i++ )
			if( bp->blocked[i] == tp )		/* bp is blocking tp */
			{
				if( bp->njobs )
					ng_bleat( 0, "\tblocked by: %s  %d(%%done) %s(state) %d(errs)", 
						bp->desc->name,  ((bp->cjobs+bp->ejobs)*100)/(bp->njobs ? bp->njobs:1), tstate_str( bp->state ), bp->ejobs );
				else
					ng_bleat( 0, "\tblocked by: %s pending(state)", bp->desc->name );
			}
	}
}

/* must be a pointer to how_much because of explain all interface */
void explain_programme( Programme *p, int *how_much )
{
	Troupe	*t;
	int	jobs = 0;
	int	cjobs = 0;
	int	ejobs = 0;
	int 	i;
	int	ext = 0;		/* we explained the troupe */
	int	nout = 0;		/* we send via udp -- must pace ourselves */
	char	tbuf[100];		/* pretty time buffer */



	for( t = p->troupes; t; t = t->next )
	{
		jobs += t->njobs;	
		cjobs += t->cjobs;
		ejobs += t->ejobs;
	}

	ng_stamptime( p->subtime, 1, tbuf );
	ng_bleat( 0, "programme: %s(name) %d(jobs) %d(ok) %d(err) %d(%%done) %ld(sec2purge) %s(submitted) %s(user)", 
				p->name, jobs, cjobs, ejobs, (cjobs*100)/(jobs ? jobs : 1), (long) (p->purge_time ? (p->purge_time - ng_now())/10:-1) , tbuf, p->subuser ? p->subuser : "nobody" );

	if( *how_much == EX_PROG )
		return;					/* list just programmes */

	for( t = p->troupes; t; t = t->next )
	{
		ext = 0;
		if( ++nout > 100 )
		{
			nout = 0;
			siesta( 4 );		/* sleep for a bit of a second to let udp catch up */
		}

		if( *how_much == EX_ALL || t->njobs > 10 )
		{
			explain_troupe( p, t );
			ext++;
		}
		if( t->njobs )
		{
			for( i = 0; i < t->njobs; i++ )
				if( t->jobs[i].status == FIN_BAD )
				{
					if( ! ext++ )
						explain_troupe( p, t );			/* must explain troupe if explaining failure */
					ng_bleat( 0, "\tfailure: %d(job#) %s(info) cmd=%s", i, t->jobs[i].info, t->jobs[i].cmd );
				}
		}
	}
}


/* 	causes all programmes to be explained. called with a null symentry causes it to invoke itself 
	indirectly via symtraverse
*/
void explain_all( Syment *se, void *data )	/* protoype defined by symtraverse */
{
	if( ! se )
	{
		symtraverse( symtab, PROG_SPACE, explain_all, (void *) data );
		return;
	}

	explain_programme( (Programme *) se->value, (int *) data );	/* generates warning and that is ok */
}



/* ----------------- things called from the udp routine --------------------*/

/* 	expect: <programme-name> <troupename> {job#|all|bad|pending} */
void uif_resubmit( char *msg )
{
	Programme *p;
	Troupe	*tp;
	char	*tok;
	char	*strtokp;

	if( ! msg )
		return;

	tok = strtok_r( msg, " ", &strtokp );

	if( p = (Programme *) lookup( tok, PROG_SPACE ) )
	{
		if( p->purge_time )				/* all jobs were previously finished */
		{
			p->purge_time = 0;		
		}

		tok = strtok_r( NULL, " ", &strtokp );
		if( tok && (tp = (Troupe *) lookup( tok, p->progid + TROUPE_SPACE )) )
		{
			if( (tok = strtok_r( NULL, " ", &strtokp )) == NULL )		/* allow user to leave off */
				tok = "bad";						/* default to resubmit errors */
			
			if( strcmp( tok, "all" ) == 0 )
				resubmit_jobs( p, tp, RE_SUBMIT_ALL, 1 );
			else
			if( strcmp( tok, "bad" ) == 0 || strncmp( tok, "fail", 4 ) == 0 )
				resubmit_jobs( p, tp, RE_SUBMIT_BAD, 1 );
			else
			if( strncmp( tok, "pend", 4 ) == 0 )
				resubmit_jobs( p, tp, RE_SUBMIT_RUNNING, 1 );		/* jobs we think are running */
			else
				resubmit_jobs( p, tp, atoi( tok ), 1 );			/* restart the job number */
		}
		else
			ng_bleat( 0, "resubmit: troupe (%s) not found in programme (%s)", tok, p->name );
	}
	else
		ng_bleat( 0, "resubmit: programme not found: (%s)", tok );
}

/*	called for purge message
	assume multiple tokens on the line, each a programme name to purge
*/
void uif_purge( char *msg )
{
	Syment	*se;
	Programme *p;
	char	*tok;
	char	*strtokp;

	if( ! msg )			/* was coredumping when no parms passed in */
		return;

	while( (tok = strtok_r( msg, " ", &strtokp )) != NULL )
	{
		msg = NULL;
		if( (se = symlook( symtab, tok, PROG_SPACE, 0 , SYM_NOFLAGS )) )
			purge_programme( (Programme *) se->value );
		else
			ng_bleat( 0, "purge: programme not found: (%s)", tok );
	}
}

/* 	called when explain [all|prog] [programme] [programme]... is received 
*/
void uif_explain( char *msg )
{
	Syment	*se;
	Programme *p;
	char	*tok = NULL;
	char	*strtokp;
	void	*data = NULL;				/* explain all uses this to determine how much to explain */
	int	how_much = EX_FAIL;			/* how much explain_programme will dump depends on this -- default to prog & failures */

	data = (void *) EX_FAIL;
	if( msg )
	{
		tok = strtok_r( msg, " ", &strtokp );

		if( strcmp( tok, "all" ) == 0 )
		{
			tok = strtok_r( NULL, " ", &strtokp );
			data = (void *) EX_ALL;
			how_much = EX_ALL;				/* explain only programme info */
		}
		else
		if( strcmp( tok, "prog" ) == 0 )	
		{
			data = (void *) EX_PROG;
			how_much = EX_PROG;				/* explain only programme info */
			tok = strtok_r( NULL, " ", &strtokp );
		}
		else
		if( strcmp( tok, "fail" ) == 0 )	
			tok = strtok_r( NULL, " ", &strtokp );		/* just skip over fail -- its the default */
	}

	if( ! tok )			/* no programme name(s) given - explain for all programmes */
		explain_all( NULL, &data );
	else						/* explain for just the programmes given */
	{
		while( tok )
		{
			if( (se = symlook( symtab, tok, PROG_SPACE, 0 , SYM_NOFLAGS )) )
				explain_programme( (Programme *) se->value, &how_much );
			else
				ng_bleat( 0, "explain: programme not found: (%s)", tok );

			tok = strtok_r( NULL, " ", &strtokp );
		}
	}
}

static void dump_help( )
{
	ng_bleat( 0, "valid nawab dump commands:" );
	ng_bleat( 0, "dump c[oredump] [u=username] [programme-name]" );
	ng_bleat( 0, "dump p[rog] [programme-name]" );
	ng_bleat( 0, "dump t[roupe] programme-name troupe-name" );
	ng_bleat( 0, "dump v[ariables] programme-name" );
}

/*	figures out what the user wants dumped and does it 
	expect: 	dump p[rog] [name]
			dump t[roupe] progname troupname
			dump v[ariables] prog-name
			dump h[elp]
			dump c[ore] [u=username] [progname]
	though the dump is removed by the caller 
*/
void uif_dump( char *msg )
{
	Syment	*se;
	Programme *p;
	char	*tok;
	char	*strtokp;
	char	fname[256];

	if( ! msg )
	{
		dump_help( );
		return;
	}

	tok = strtok_r( msg, " ", &strtokp );
	switch( *tok )
	{
		case 'c':			/* 'core' dump */
			if( (tok = strtok_r( NULL, " ", &strtokp )) == NULL )
				sfsprintf( fname, sizeof( fname ), "/tmp/nawab_dump", tok );
			else
			{
				if( *(tok+1) == '=' )
				{
					sfsprintf( fname, sizeof( fname ), "/tmp/%s.nawab_dump", tok+2 );
					tok = strtok_r( NULL, " ", &strtokp );
				}
			}

			core_dump( fname, tok );				/* dump the stuff */
			break;

		case 'p':
			if( (tok = strtok_r( NULL, " ", &strtokp )) != NULL )
			{
				if( (se =symlook( symtab, tok, PROG_SPACE, 0, SYM_NOFLAGS )) )
					dump_programme( se->value );
				else
					ng_bleat( 0, "uif_dump: programme not defined: %s", tok );
			}
			else
				dump_programmes( NULL, NULL );
			break;

		case 't':
			if( (tok = strtok_r( NULL, " ", &strtokp )) != NULL )
			{
				if( (se =symlook( symtab, tok, PROG_SPACE, 0, SYM_NOFLAGS )) )
				{
					p = (Programme *) se->value;
					if( (tok = strtok_r( NULL, " ", &strtokp)) )
					{
						if( (se = symlook( symtab, tok, TROUPE_SPACE + p->progid, 0, SYM_NOFLAGS )) )
						{
							dump_troupe( se->value );
						}
						else
							ng_bleat( 0, "uif_dump: troupe (%s) not defined for prog (%s)", tok, p->name );
					}
					else
						ng_bleat( 0, "uif_dump: sytax error: dump troupe <progname> <troupename>" );
				}
				else
					ng_bleat( 0, "uif_dump: programme not defined: %s", tok );
			}
			else
				ng_bleat( 0, "uif_dump: sytax error: dump troupe <progname> <troupename>" );
			break;

		case 'v':
			if( (tok = strtok_r( NULL, " ", &strtokp )) != NULL )
			{
				if( (se =symlook( symtab, tok, PROG_SPACE, 0, SYM_NOFLAGS )) )
				{
					p = (Programme *) se->value;
					symtraverse( symtab, p->progid+ VAR_SPACE, showvar, NULL );
					symtraverse( symtab, p->progid+ IONUM_SPACE, showvar, NULL );
				}
				else
					ng_bleat( 0, "uif_dump/vars: programme not defined: %s", tok );
			}
			else
				ng_bleat( 0, "uif_dump: sytax error: dump v[ariables] <progname>" );
				
			break;

		default:
			dump_help( );
			break;
	}
}
