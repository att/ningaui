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
	mnemonic: 	rm_pickn
	Abstract:	acepts a list of node, total-space, avail-space tripples on stdin. 
			generates 1 to n selected node names in a manner similar to the way 
			repmgr selects them.  Input is the first line of each filelist 
			file of nodes to select from. The node name and available amount
			are the only two bits of info used from each line.  

			We then compute a set of weighted values, and for 0 to count-1 we
			select a random node.  Selected nodes are written, one per line, to 
			the standard output. 

			Verbose modes (-v, -v -v, -v -v -v) generate increasing bits of 
			information about weights and selection.

	Date:		25 Apr 2005 
	Author:		E. Scott Daniels/Andrew Hume (choose_node snarfed from munge.c)

	cmd line: 	rm_pickn [n]   # n is number of node names to generate
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <sfio.h>
#include <ningaui.h>

#include <rm_pickn.man>

#define MAX_NODES	256

typedef struct {			/* used to map nodes that we see an flist entry for */
	ng_int64 avail;			/* avail space on the node */
	ng_int64 total;			/* total space -- ignored for this */
	float	weight;			/* weight assigned to the node */
	char 	*name;		
} Node;

Node *nodes[MAX_NODES];
int nnodes = 0;
int uniform = 1;			/* uniform dist rather than weighted toward free node */

#define	PFAC	1.5
void choose_node( int count )
{
	int j; 
	int kk;
	int nnn;			/* number of nn node references */
	double tot, m;
	struct { Node *n; long sel; int i; ng_uint32 avail; } nn[MAX_NODES];	/* 2 lists allow us to prune nodes with no space */

	nnn = 0;
	for(j = 0; j < nnodes; j++)			/* pluck off only nodes with viable space available */
	{
		if(nodes[j]->avail > 0)
		{
			nn[nnn].avail = nodes[j]->avail;
			nn[nnn].n = nodes[j];
			nn[nnn].i = j;
			nn[nnn].sel = 0;
			nnn++;
		}
	}
	if(nnn == 0)
		return;
	

	
	
	m = 0;				/* compute average available per node */
	for(j = 0; j < nnn; j++){
		 m += nn[j].avail;
	}

	m /= nnn; 				/* average per node */
	if(m < .001) 
		m = 0.001;

	if( uniform )				/* uniform mode all get a weight of 1 and we randomise across them */
		for( j = 0; j < nnn; j++ )
			nn[j].n->weight = 1;
	else
	{
		for(j = 0; j < nnn; j++)
			nn[j].n->weight = pow((nn[j].avail)/m, PFAC);	/* weight  based on relationship to average avail per node */
	}

	for(j = 1; j < nnn; j++)
		nn[j].n->weight += nn[j-1].n->weight;
	
	
	if( verbose ){
		sfprintf(sfstderr, "weights: " );

		for(j = 0; j < nnn; j++)
			sfprintf(sfstderr, " %s:%.1f", nn[j].n->name, nn[j].n->weight);
		sfprintf(sfstderr, "\n");
	}

	for( kk = 0; kk < count; kk++ )
	{
		tot = nn[nnn-1].n->weight * drand48();		/* pick a value between 0 and max weight */
	
		for(j = 0; j < nnn; j++)
		{
			if(tot < nn[j].n->weight){		/* find node based on random total */
				if( verbose > 2 )
					sfprintf(sfstderr, "rand=%.1f select %d(%s)\n", tot, nn[j].i, nn[j].n->name);
				
				j++;
				break;
			}
		}					/* on exit j will be 1 past the selected node */

		printf( "%s\n", nn[j-1].n->name );
		nn[j-1].sel++;
	}

	if( verbose > 1 )
	{
		for( kk = 0; kk < nnn; kk++ )
			sfprintf( sfstderr, "%10s %I*d %d\n", nn[kk].n->name, Ii(nn[kk].n->avail), nn[kk].sel );
	}
}

void usage( )
{
	sfprintf( sfstderr, "usage: %s [count] <flist-info\n", argv0 );
}

main( int argc, char **argv )
{
	char	*buf;
	char	*p;
	int 	nn;
	int 	count = 1;
	unsigned short	seed[3];
	struct timeval	tv;

	ARGBEGIN
	{
		case 'b':	uniform = 0; break;
		case 'v':	verbose++; break;
usage:
		default:
			usage( );
			exit( 1);
			break;
	}
	ARGEND

	gettimeofday( &tv, NULL );		/* must seed with high-res timer to get different answers on back to back calls */
	seed[0] = tv.tv_usec;
	seed[1] = tv.tv_sec;
	seed[2] = seed[0] ^ ((int) &main);	/* should help concurrent executions get different answers */
	seed48( seed );

	nnodes = 0;
	while( (buf = sfgetr( sfstdin, '\n', SF_STRING )) != NULL  )		/* expect node-name, total space, avail space */
	{
		nodes[nnodes] = (Node *) ng_malloc( sizeof( Node), "node" );

		if( (p = strtok( buf, " " )) )
			nodes[nnodes]->name = strdup( buf );
		else
			nodes[nnodes]->name = NULL;

		if( (p = strtok( NULL, " " )) )
			nodes[nnodes]->total = strtol( p, NULL, 10 );
		else
			nodes[nnodes]->total = 0;

		if( (p = strtok( NULL, " " )) )
			nodes[nnodes]->avail  = strtol( p, NULL, 10 );
		else
			nodes[nnodes]->avail  = 0;

		nodes[nnodes]->weight = 0;			/* choose sets, but be polite and init */
		
		if( ++nnodes > MAX_NODES )
			exit( 1 );		/* crude but in a hurry */
	}

	if( nnodes <= 0 )
	{
		ng_bleat( 1, "no nodes found in list" );
		exit( 1 );
	}
	
	if( argc > 0 )
		count = atoi( argv[0] );

	choose_node( count  );
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rm_pickn:Pick node using repmgr algorithm)

&space
&synop	ng_rm_pickn [count] <flist-info

&space
&desc	&This reads from standard input a list of &ital(freespace) information 
	from the various replication manager filelist files.
	From this list the desired number (count) of node names are selected and written 
	to the standard output device as a newline separated list.

&space
	By default, the seleciton process is random and should be fairly uniform across
	all of the nodes in the list.  If the balance option (-b) is provided on the command
	line, then the nodes are selected using an algorithm similar to the replicaiton manager weighted algorithm;
	preference is given  to  the nodes with the most available space.

&space
&opts	These options are recognised when placed on the command line ahead of the parameters:
&begterms
&term	-b : Balance.  Select nodes with an effort to balance the load across the nodes. When 
	run with this option, &this will select nodes giving preference to the nodes that have 
	the least utilisation.  
&space
&term	 -v : Verbose mode.  Using two or three &lit(-v) options causes more information to be 
	generated. All verbose output is written to the standard error device. 
&endterms

&space
&parms	An optional node count may be supplied; if omitted, one (1) is assumed. The count is the 
	number of node names written to the standard output device. 

&space
&exit	Non-zero to inidcate error

&space
&see	ng_repmgr.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	25 Apr 2005 (sd) : Sunrise.
&term	09 May 2005 (sd) : Converted such that uniform distribution is now the default. 
&term	28 Oct 2005 (sd) : Now puts out nothing if no records are read. 
&term	04 Apr 2006 (sd) : Fixed a debugging message that was being put out even if not in 
		verbose mode.
&term	22 Apr 2007 (sd) : Changed weighting to give the expected results when -b flag used. 
		Added better doc. 
&endterms

&scd_end
------------------------------------------------------------------ */
