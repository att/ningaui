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
 Mnemonic:	s_mpn.c
 Abstract:	This file contains the functions that implement max per node
		(mpn) tracking and testing.  Max per node is based on resources
		that have an mpn value > 0.  For each resource:node pair a count
		of jobs running is maintained. If that count is >= to the mpn 
		value defined in the resource, mpn_check will return false. 

		The counts are maintained as an array of int with a hash table
		used to point rsrc:node pairs to the proper count. 

 Date: 		10 Jun 2004
 Author:	E. Scott Daniels
 Mods:		10 Dec 2004 (sd) : Corrected check; it was treating 0 as unlimited.
		15 Jan 2009 (sd) : Added bleat to assist with understanding why a job is not run.
 ---------------------------------------------------------------------------
*/

static Symtab	*mpn_tab = NULL;	/* private table as we will hit it a lot */
static int	*mpn_counters = NULL;	/* we point symtab things to individual counters */
static int	mpn_next = 0;		/* index into counters */
static int	mpn_alloc = 0;		/* number allocated in counters */

static int mpn_init_tab( )
{
	if( ! mpn_tab )
	{
		if( (mpn_tab = syminit( 1171 ) ) == NULL )
		{
			ng_bleat( 0, "new_mpn_tab: abort: cannot create mpn symbol table" );
			exit( 1 );
		}

		ng_bleat( 1, "mpn table allocated" );
	}
}

static void mpn_more( )
{
		mpn_alloc += 1024;
		mpn_counters = ng_realloc( mpn_counters, sizeof( int ) * mpn_alloc, "mpn_counters" );
		ng_bleat( 1, "mpn counters expanded to: %d", mpn_alloc );
}


/* get the counter for a resource:node pair */
static int *mpn_counter( char *rname, char *nname )
{
	char	wbuf[1024];
	Syment	*se;
	int	*counter;		/* pointer at the counter to put into the symtab */

	sfsprintf( wbuf, sizeof( wbuf ), "%s:%s", rname, nname );
	
	if( (se = symlook( mpn_tab, wbuf, MPN_SPACE, NULL, SYM_NOFLAGS )) != NULL  )	/* its there and has a value */
	{
		if( se->value )
			return (int *) se->value;		/* send back the address inside of counters */
	}

	/* either the combo did not exist, or there was no value (which seems odd), so create one and assume 0 */
	if( mpn_next >= mpn_alloc )
		mpn_more( );				/* alloc more counter space */

	counter = &mpn_counters[mpn_next++];				/* save the address of the spot in couters */
	*counter = 0;
	symlook( mpn_tab, wbuf, MPN_SPACE, counter, SYM_COPYNM );	/* initialise, and have symtab copy the name if needed */
		
	ng_bleat( 1, "mpn_counter: initialised in mpn hash table: %s", wbuf );

	return counter;
}

/* return the current number of jobs running for a resource:node pair */
static int mpn_running( char *rname, char* nname )
{
	int	*counter;

	if( (counter = mpn_counter( rname, nname )) != NULL )		/* should always return something, but dont take chances */
		return *counter;

	ng_bleat( 0, "mpn_running: INTERNAL ERROR: no counter found for %s:%s", rname, nname );
	return 0;
}

/* adust the running counter for resource:node by value (value is pos/neg) */
static void mpn_adjust( char *rname, char *nname, int value )
{
	int	newv;		/* new value */
	int	*counter;	/* the counter for the r:n pair */

	if( (counter = mpn_counter( rname, nname )) != NULL )		/* should always return something, but dont take chances */
	{
		if( (newv = value + *counter) < 0 )
			newv = 0;
		*counter = newv;				/* set adjusted value */
		ng_bleat( 2, "mpn_adjust: %s:%s adjusted by %d; now %d", rname, nname, value, newv );
	}
	else
		ng_bleat( 0, "mpn_adjust: INTERNAL ERROR: no counter found for %s:%s", rname, nname );
}

/*	check to see if the job's resources have a max/node limit set. 
	for each that does, ensure the number of resource jobs 
	running on the node is < max.  
	return 1 if all resources are ok for the node, and 0
	if this job cannot be run on the node because of mpn
*/
static int mpn_check( Job_t *j, Node_t *n )
{
	Resource_t	*r;
	int 	i;

	for( i = 0; i < j->nrsrc; i++ )		/* for each  resource that the job needs */
	{
		if( (r = j->rsrc[i]) != NULL && r->mpn >= 0  )		/* a hit, and it has an mpn value */
		{
			if( mpn_running( r->name, n->name ) >= r->mpn )
			{
				ng_bleat( 5, "mpn_check: reject: node running more than desired jobs for resource: %s %s", n->name, r->name );
				return 0;
			}
		}
	}

	return 1;			/* all seem ok; return good to go */
}

/* adjust all resource:node combinations for a given job:node pair */
static void mpn_adjust_all( Job_t *j, Node_t *n, int value )
{
	Resource_t	*r;
	int 	i;

	for( i = 0; i < j->nrsrc; i++ )		/* for each  resource that the job needed */
	{
		if( (r = j->rsrc[i]) != NULL && r->mpn >= 0  )		/* a hit, and it has an mpn value */
			mpn_adjust( r->name, n->name, value  );		/* adjust it */
	}
}

/* adjust the counter for running jobs that use the named resource if the count is 0 
	must be called when mpn command is received to set a value from none to n
	or when we receive a job that has a resource coded as r=Xn
*/
static void mpn_init_counts( Resource_t *r )
{
	Job_t	*j;
	Resource_t 	*jr;	
	int i;


	for( j = rq; j; j = j->next )		/* for each running job */
	{
		for( i = 0; i < j->nrsrc; i++ )		/* for each  resource that the job needs */
		{
			if( j->rsrc[i] == r  )		/* a hit, and it has an mpn value */
			{
				ng_bleat( 2, "mpn_init_counts: adjusting %s:%s +1", r->name, j->node->name );
				mpn_adjust( r->name, j->node->name, 1 );
			}
		}
	}
}
