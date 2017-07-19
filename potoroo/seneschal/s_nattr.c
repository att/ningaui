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
 Mnemonic:	s_nattr.c
 Abstract:	contains node attribute manipulation/testing functions

 Date: 		05 November 2003
 Author:	E. Scott Daniels
 Mods:		27 Mar 2004 (sd) : converted free() to ng_free()
 ---------------------------------------------------------------------------
*/

static void set_nattr( char *nname, char *ats )
{
	Node_t	*node;
	char buf[NG_BUFFER];
	char *old;

	if( ! nname )
		return;

	if( (node = get_node( nname, CREATE )) == NULL )
		return;

	if( ats )
		sfsprintf( buf, sizeof( buf ), " %s ", ats );		/* save with lead and  trail sep for easy compare */

	if( node->nattr )
		free( node->nattr );

	node->nattr =  ats ? strdup( buf ) : NULL;
}

/*	test to see if the desired attributes all exist in the 
	current attributes known for a node.
	attributes are expected to be space separated. The desired attributes
	are also space separted and may be prefixed with a not (!) character -- 
	true returned if !attr is passed and attr is not in the list.  
	Returns 1 for matches all, and 0 for does not match all.
	
	if the current list is null, no attributes known, then we return 0 to 
	avoid running on a node that we really do not want to.  If they are not
	broadcasting their attributes, they dont get jobs.
*/
static int test_nattr( char *cur, char *desired )
{
	char	**tok;
	char	*t;			/* pointer at the token we are working with */
	char	b[1024];
	int	ntoks = 0;
	int	matched = 0;		/* number of matches */
	int	i;
	int	not = 0;
	int	need;			/* number we must match */

	if( ! cur )			/* if a node does not have any attrs defined, we cannot use it */
		return 0;

	if( ! desired )
		return 1;				/* nothing required then its true all the time */

	if( (gflags && GF_NEED_NATTR) && !cur )		/* if we must have node attributes to consider, return false if no attrs are there */
		return 0;

	tok = ng_tokenise( desired, ' ', '\\', NULL, &ntoks );		/* tokenise the attributes we are looking for */
	
	need = ntoks;				/* number that must match */
	for( i = 0; i < ntoks; i++ )
	{
		t = tok[i];	
		if( *t == 0 )			/* multiple blanks tokenise as nulls, we skip but must dec number needed */
			need--;				
		else
		{
			if( (not = *t == '!' ? 1 : 0) )
				t++;					/* advance past ! and set not flag */
	
			sfsprintf( b, sizeof( b ), " %s ",  t );		/* build string to match in current attributes */
	
			if( strstr( cur, b ) )				/* matched ,string, */
			{
				if( not )
					break;			/* short circuit on first failure */
			}
			else
			{
				if( ! not )
					break;
			}
	
			matched++;
		}
	}
	
	ng_free( tok );

	return matched == need;			/* true if we matched all attributes */
}


