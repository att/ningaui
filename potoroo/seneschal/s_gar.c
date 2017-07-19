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


/* ----------------------------------------------------------------------------------------------
   Mnemonic:	s_gar.c
   Abstract:	File contains all gar related functions 
		gar is a mk like element that allows seneschal to kick off a command to make 
		a file if the gar command is supplied with the job (via nawab). This is 
		seldom used (was very specific to the needs of 'the cycle' application).
   Date:	9 February 2002
   Author:	E. Scott Daniels

		11 Jul 2002 (sd) - added job id to gar job name to prevent collisions - timestamp
				was not finine enough!
 ---------------------------------------------------------------------------------------------- 
*/

/*	parse a buffer addng/replacing the gar command
	syntax from user: gar=name:command 
	"gar=" is removed before call so we get name:cmd
  ----------------------------------------------------------------------
*/
static void make_gar( char *buf )
{
	Gar_t	*g;
	Syment	*se;
	char	*cmd;		/* pointer at the command */

	if( cmd = strchr( buf, ':' ) )		/* find end of name */
	{
		*cmd = 0;			/* terminate name token */
		cmd++;				/* point at the command */

		if( (se = symlook( symtab, buf, GAR_SPACE,  NULL, SYM_NOFLAGS )) )
		{
			if( (g = (Gar_t *) se->value) )
			{
				if( strlen( cmd ) > strlen( g->cmd ) )
				{
					ng_free( g->cmd );
					g->cmd = ng_strdup( cmd );
				}
				else
					strcpy( g->cmd, cmd );
			}
			else
				ng_bleat( 0, "make_gar: null gar block referenced in symtab: %s", buf );
		}
		else						/* first time to encounter this one */
		{
			g = ng_malloc( sizeof( Gar_t ), "make_gar: gar block" );
			memset( g, 0, sizeof( Gar_t ) );
			g->next = gar_list;
			gar_list = g;							/* add to the list of blocks */
			sfsprintf( g->name, sizeof( g->name ), "%s", buf );
			g->cmd = ng_strdup( cmd );
			symlook( symtab, g->name, GAR_SPACE, g, SYM_NOFLAGS );		/* add the block */
		}

		if( g )
			ng_bleat( 3, "make_gar: added gar command: %s = (%s)", g->name, g->cmd );
	}
}

/* reset file info in a gar block */
static void clear_gar( Gar_t *g )
{
	int	i;

	g->pending = 0;
	
	for( i = 0; i < g->idx; i++ )
		if( g->fstrings[i] )
			*(g->fstrings[i]) = 0;				/* trunc string */
}

/*	add a file name to the string for node in gar name proivded that the max/node
	is not met for the node.
	returns true if we added (not over limit)
*/
static int add_gar( char *name, Node_t *n, char *f )
{
	Syment	*se;
	Gar_t	*g;
	int	i;

	if( (se = symlook( symtab, name, GAR_SPACE,  NULL, SYM_NOFLAGS )) )
	{
		g = (Gar_t *) se->value;
		for( i = 0; i < g->idx && g->nodes[i] != n; i++ );		/* find node */

		if( i >= g->idx )			
		{
			g->idx++;
			g->nodes[i] = n;
			g->nfiles[i] = 0;
			g->fstrings[i] = ng_malloc( NG_BUFFER/2, "add_gar - fstirngs" );
			*(g->fstrings[i]) = 0;
		}

		if( g->nfiles[i] >= MAX_PERGAR )			/* if already plenty queued for this node - then bail */
			return 0;

		ng_bleat( 2, "add_gar: %s(gar) %s(file) %s(node)", name, f,  n->name );

		if( *(g->fstrings[i]) == 0 )			/* first thing queued for this node */
			g->pending++;				/* bump the pending nodes counter up */

		if( ++g->nfiles[i] >= MAX_PERGAR )		/* node reached its max */
			n->flags &= ~NF_GAR_OK;			/* stop assigning things to this node */

		strcat( g->fstrings[i], f );
		strcat( g->fstrings[i], " " );

	}
	else
	{
		ng_bleat( 0, "no gar defined: %s(file) %s(gar)", f, name );
		return 0;
	}

	return 1;
}

/*	create a job block for a gar job and put it on the queue 
	n - pointer to node block
	cmd - pointer to the command string
	fstring - pointer to the string that is passed as arg list (files) to cmd
*/
static void make_gar_job( Node_t *n, char *cmd, char *fstring )
{
	static	int jid = 0;		/* we had multiple jobs for same node at same timestamp, ensure unique */
	Job_t	*j;			/* pointer to the new job */
	char	wrk[NG_BUFFER];		/* work buffer */
	int	i;	

	if( jid++ > 1000 )		/* should not do 1000 in 1/10th of a sec */
		jid = 0;

	sfsprintf( wrk, sizeof( wrk ), "GJ_%s_%I*u_%d", n->name, sizeof( ng_timetype ), ng_now( ), jid );
	j = new_job( wrk );
	symlook( symtab, j->name, JOB_SPACE, j, SYM_NOFLAGS );	

	j->response[0] = 0;			/* prevent status info going to anybody */
	j->size = 5000;				/* just to have something to calc cost on */
	j->max_att = 1;				/* try once, if it fails we will generate a new gar later */
	j->priority = 1500;			/* ensure that run_jobs see these first */
	j->flags |= JF_GAR	;		/* these are scheduled immediatly  with a fixed lease time */
	j->node = n;				/* must run on the node that is passed in */
	j->attempt = -1;			/* make them match again */

	sfsprintf( wrk, sizeof( wrk ), "%s %s", cmd, fstring );
	j->cmd = ng_strdup( wrk );
	
	q_job( j, PENDING );			/* put gar on the pending queue */

	ng_bleat( 2, "make_gar_job: queued gar job: %s %s(target)", j->name, j->node->name );
}


/*	run the gar list and create any jobs that need to be */
static void submit_gar( )
{
	Gar_t	*g;
	int	i;

	for( g = gar_list; g; g = g->next )
	{
		for( i = 0; i < g->idx && g->pending; i++ )
		{
			if( *(g->fstrings[i]) )		/* not an empty string */
			{
				g->pending--;
		/* we may need to leave this set until the job actually completes -- depending on if they take longer than the repmgr cycle */
				g->nfiles[i] = 0;					/* reset number of files pending here */
				make_gar_job( g->nodes[i], g->cmd, g->fstrings[i] );
				ng_bleat( 1, "submit_gar: on %s: %s %s", g->nodes[i]->name, g->cmd, g->fstrings[i] );
			}
		}	

		clear_gar( g );
	}
}


/*
	see if we can gar any of the input files for the indicated job
	if we can, the information is added to the appropriate gar datastrucuture
	which will be used to generate a series of gar jobs on affected nodes
*/

static void gar_files( Job_t *j )
{
	File_t	*f;			/* pointer into the file list */
	Syment	*se;			/* symbol table entry for gar */
	Node_t	*n = NULL;		/* node that we think we need the missing file(s) created on */
	ng_timetype now;
	char	*gar;			/* pointer at the command */
	int 	i;
	int	k;
	int	x;
	int	next = 0;		/* pointer at next node/hits slot to use */
	int	max_hits = 0;		/* max number of hits we saw */
	int	bids = 0;		/* number of bids on the node with max hits (so we give the gar to the node with most bids if tie on max hits) */
	int	hits[MAX_NODES];	/* number of files that each node has */
	Node_t	*nodes[MAX_NODES];	/* nodes that have at least one of the files */

	now = ng_now( );

	nodes[0] = NULL;
	hits[0] = 0;

	if( ! (gflags & GF_ALLOW_GAR) )		/* garring of files is not allowed right now - just leave */
		return;

	if( j->gar_lease > now )		/* too soon to try again */
		return;

	if( j->node )				/* job is targeted, so we need to create files here */
		n = j->node;
	else
	{
		for( i = 0; i < j->ninf; i++ )		/* map the union of input files on each node - hits[i] is count of files on the node */
		{
			if( (f = j->inf[i]) )				/* run the list of input files */
			{
				for( k = 0; k < f->nnodes; k++ )	/* run the list of nodes that currently has a copy */
				{
					if( f->nodes[k] )			/* this file is on node k */
					{
						for( x = 0; x < next && x < MAX_NODES && nodes[x] != f->nodes[k]; x++);
						if( x < MAX_NODES )				
						{
							if( x >= next )				/* first time this node has popped up */
							{
								nodes[x] = f->nodes[k];		/* mark it in our list */
								next++;
								nodes[next] = NULL;		/* initialise as we go */
								hits[next] = 0;
							}

							if( ++hits[x] > max_hits )
							{
								if( max_hits == 0 || (nodes[x]->flags & NF_GAR_OK) )   /* dont stomp on a prev node if this one is stopped */
								{
									max_hits = hits[x];
									n = nodes[x];			/* keep track of the node with the mostest */
									bids = n->bid;			/* track the current bid level for one with the most */
								}
							}
							else
								if( hits[x] == max_hits && (nodes[x]->flags & NF_GAR_OK) )	/* tie- node x wins if its still allowed to get a gar file */
								{								/* as the previous one might be stopped */
									n = nodes[x];					
									bids = n->bid;
								}

							/*ng_bleat( 1, "gar_files: file %s lives on: %s %d(hits) %s(current-winner)", f->name, n->name, hits[x], n? n->name:"node-null" );*/
						}
					}
				}
			}
		}

		if( ! n )				/* its possible that no node had any files */
			n = get_any_node( j, j->notnode, j->nattr );	/* get the node with the most bids - we have to assume that the file can be created there */
	}

	if( !n ||  n->suspended || n->bid < 1 || !(n->flags & NF_GAR_OK) )		/* no node, no bids or node is suspended - then we have to give up here */
	{
		ng_bleat( 3, "gar rejected for job %s on node %s  (suspended=%d bid=%d)",  j->name, n ? n->name:"no-node-available", (n ? n->suspended : -1), n ? n->bid: -100 );
		return;
	}

	j->gar_lease = 0;				/* we use as a flag to dec bids if gar jobs scheduled */

	/* we now have a node to use, see what files do not live there and gar them if we can */

	for( i = 0; n->bid && i < j->ninf; i++ )			/* run the input list */
	{
		if( (f = j->inf[i]) != NULL && f->gar && f->gar_lease < now )	/* file has gar info and lease says ok */
		{
			for( k = 0; k < f->nnodes && f->nodes[k] != n; k++ );   /* will stop with k < nnodes if our node already has this file */
			if( k >= f->nnodes )
			{
				if( add_gar( f->gar, n, f->name ) )				/* queue the need for a gar job */
					j->gar_lease = f->gar_lease = now + GAR_DELAY;
			}
		}
	}
}

/*
	runs the missing queue seeing if jobs that have a priorty greater than target can 
	be garred.
*/
static void calc_gar( int target )
{
	Job_t	*j;				/* pointer into the job queue */
	Node_t	*n;
	int	o_bids = 0;			/* number of outstanding bids */

	if( (gflags & GF_ALLOW_GAR) )			/* gar has not been blocked */
	{
		for( n = nodes; n; n = n->next )
		{
			o_bids += n->bid;		/* count outsitanding bids */
			n->flags |= NF_GAR_OK;	/* ok to gar to this node */
		}

		/* if target >0 then we released jobs; we need to gar anything that had a higher priority */
		/* if o_bids; we did not have enough work to give out - can we gar to make work? */
		/* if pq is empty, then lets see if we can drum up something to do */
		if( pq == NULL || target > 0 || o_bids )
			for( j = mq; j && j->priority > target; j = j->next )
				gar_files( j );
	
		gflags &= ~GF_ALLOW_GAR;		/* turn gar generation off for a bit */
	}
}
