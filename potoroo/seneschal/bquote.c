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

/*	expand the first set of back quoted things.  
	no mechanism to escape bk quotes ( e.g. ` ...\`....\`....` not supported)
*/
char *bquote( char *buf );
{
	char *rb = NULL;		/* return buffer */
	char *b1;			/* first back quote */
	char *b2;			/* last back quote */
	char *eb;			/* buffer back from ng_cmd */
	int l;				/* len of new buffer after expansion */

	if( b1 = strchr( buf, '`' ) )	
	{
		if( b2 = strchr( b1+1, '`' ) )
		{
			*b2 = 0;
			eb = ng_cmd( b1+1 );		/* run the command */
			*b2 = '`';			
			*b1 = 0;
			l = strlen( buf ) + strlen( eb ) + strlen( b2+1 ) + 1;			
			rb = ng_malloc( l, "bquote buffer"
			sfsprintf( rb, l, "%s%s%s", buf, eb, b2+1 );
			*b1 = '`';
			ng_free( eb );
		}
		else
			ng_bleat( 0, "bquote: closing back quote not found: %s", buf );
	}

	return rb;
}
