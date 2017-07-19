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
-------------------------------------------------------------------------------------
  Mnemonic:	s_debug.c
  Abstract:	Simple debug routine for seneschal testing. Optionally included by 
		seneschal.c.
  Author:	E. Scott Daniels
-------------------------------------------------------------------------------------
*/
void debug( Job_t *j, File_t *f, Node_t *n, char *tstring, int recurse )
{
	Node_t	*nn;
	int	ov;
	static	int level;
	int 	i;
	char	*sstring = "not set";

	if( level > 25 )
		return;

	level++;
	if( tstring == NULL )
		tstring = "";

	ov = verbose;
	verbose = 4;

	if( n )
		ng_bleat( 0, "debug:%snode @%x %s next=%x prev=%x bid=%d", tstring, n, n->name, n->next, n->prev, n->bid );

	if( f )
	{
		ng_bleat( 0, "debug:%sfile @%x %s next=%x prev=%x nnodes=%d nidx=%d ref=%d", tstring, f->name, f->next, f->prev, f->nnodes, f->nidx, f->ref_count );
		for( i = 0; i < f->nidx; i++ )
			if( f->nodes[i] )
				ng_bleat( 0, "debug:\t%spath [%d] %s (%s)", tstring, i, f->nodes[i]->name, f->paths[i] );
			else
				ng_bleat( 0, "debug:\t%spath [%d] no pointer", tstring, i  );
		if( recurse)
		{
			for( i = 0; i < f->nref; i++ )
				if( f->reflist[i] )
					debug( f->reflist[i], NULL, NULL, "\t", 0 );
				else
					ng_bleat( 0, "debug:\tjob [%d] no pointer", i );
		}
	}

	if( j )
	{
		switch( j->state )
		{
			case PENDING: sstring = "pending"; break;
			case MISSING: sstring = "missing"; break;
			case RUNNING: sstring = "running"; break;
			default:	sstring = "complete"; break;
		}

		ng_bleat( 0, "debug:%sjob @%x %s next=%x prev=%x q=%s node=%s att=%d, flag=%x", 
				tstring, j, j->name, j->next, j->prev, sstring, j->node, j->attempt, j->flags );
		if( check_resources( j, 1 ) )		/* resources for job are ok */ /* change 1 to job cost when allocating this way */
			ng_bleat( 0, "debug:%sjob is runnable from a resource perspective", tstring );
		else
			ng_bleat( 0, "debug:%sjob is blocked because of resources", tstring );

		if( j->ninf )
		{
			int reason;

			if( (nn = map_f2node( j, &reason )) != NULL )
			{
				ng_bleat( 0, "debug:%snode %s has all input files and bids", tstring, nn->name );
				if( recurse )
					debug( NULL, NULL, nn, "\t", 0 );
			}
			else
				ng_bleat( 0, "debug:%scannot find a node with all input files and bids: reason=0%02x", tstring, reason );
		}

		if( recurse )
		{
			for( i = 0; i  < j->ninf; i++ )
				if( j->inf[i] )
					debug( NULL, j->inf[i], NULL, "\tinput-", 0 );
			for( i = 0; i  < j->noutf; i++ )
				if( j->outf[i] )
					ng_bleat( 0, "\toutput-file: %s", j->outf[i] );
		}
	}


	level--;
	verbose = ov;
}
