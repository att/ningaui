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
	Mnemonic: 	add2mrad
	Abstract: 	add data to the most recently added descriptor
	Date:		23 march 2006
	Author: 	E. Scott Daniels

	Mods:		04 May 2007 (sd) : Fixed return if dp is null on entry; It was 
				not returning NULL, but was coded without a value so junk
				was going back. 
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sfio.h>

#include <ningaui.h>
#include <ng_lib.h>
#include <nawab.h>

Desc *add2mrad( Desc *dlist, void *data, Datatype ot )
{
	extern	Desc *mrad;
	extern	int yyerrors;
	
	Iter	*itp;
	Range	*rap;
	Rlist	*rlp;
	Value	*vp;
	Io	*iop;
	Desc	*dp;
	Flist	*fp;
	Job	*j;
	Troupe	*tp;
	char	*cp;		/* pointer at string  */

	dp = mrad;		/* point at most recently added one */
	if( dp == NULL )
		return NULL;

	if( dp->type != T_djob )
	{
		ng_bleat( 0, "ad2mrad: missing job(troupe) label" );
		return dlist;
	}

	switch( ot )
	{
		case DT_value:			/* we assume its a comment if string, otherwise we throw and err */
			vp = (Value *) data;
			if( vp->type != T_vstring )
			{
				if( dp->val )
					purge_value( dp->val );
				dp->val = (Value *) data;
				ng_bleat( 2, "add2mrad: type=%d  value", vp->type );
				break;
			}
			else
			{
				if( dp->desc )
					ng_free( dp->desc );
				dp->desc = strdup( vp->sval );
				ng_bleat( 2, "add2mrad: type=%d value treated as comment (%s)", vp->type, dp->desc );
				purge_value( vp );			/* weve kept all that we need from this */
			}
			break;

	 	case DT_comment:
			if( dp->desc )
				free( dp->desc );
			dp->desc = (char *) data; 
			ng_bleat( 2, "add2mrad: type=%d  comment (%s)", ot, dp->desc );
			break;

		case DT_progid:
			ng_bleat( 2, "add2mrad: type=%d progid", ot );
			dp->progid = *((long *) data); 
			break;

	 	case DT_type:
			ng_bleat( 2, "add2mrad: type=%d type: WARNING -- this is NOT expected!!! no data saved", ot );
			break;

	 	case DT_name:
			ng_bleat( 2, "add2mrad: type=%d name", ot );
			if( dp->name )
				free( dp->name );
			dp->name = (char *) data; 
			break;

	 	case DT_after:
			ng_bleat( 2, "add2mrad: type=%d after", ot );
			if( dp->after )
				free( dp->after );
			dp->after = (char *) data; 
			break;

	 	case DT_nodes:
			ng_bleat( 2, "add2mrad: type=%d nodes", ot );
			if( dp->nodes )
				free( dp->nodes );
			dp->nodes = (char *) data; 
			break;

 		case DT_cmd:
			ng_bleat( 2, "add2mrad: type=%d cmd", ot );
			if( dp->cmd )
				free( dp->cmd );
			dp->cmd = (char *) data; 
			break;

		 case DT_consumes:
			ng_bleat( 2, "add2mrad: type=%d rscs", ot );
			if( dp->rsrcs )
				free( dp->rsrcs );
			dp->rsrcs = (char *) data; 
			break;

 		case DT_attempts:
			ng_bleat( 2, "add2mrad: type=%d attempts", ot );
			if( dp->attempts )
				purge_value( dp->attempts );
			dp->attempts = (Value *) data;
			break;

		case DT_priority:
			ng_bleat( 2, "add2mrad: type=%d priority", ot );
			if( dp->priority )
				purge_value( dp->priority );
			dp->priority = (Value *) data;
			break;

		case DT_keep:					/* number of seconds to keep prog info after it has completed */
			if( dp->keep )
				purge_value( dp->keep );
			dp->keep = (Value *) data;
			break;

		case DT_reqin:
			ng_bleat( 2, "add2mrad: type=%d reqin", ot );
			if( dp->reqdinputs )
				purge_value( dp->reqdinputs );
			dp->reqdinputs = (Value *) data;
			break;

		case DT_range:
			ng_bleat( 2, "add2mrad: type=%d range", ot );
			if( dp->range )
				purge_range( dp->range );
			dp->range = (Range *) data; 
			break;

		case DT_input:
			ng_bleat( 2, "add2mrad: type=%d input prevlist=%x", ot, dp->inputs );
			iop = (Io *) data;
			iop->next = dp->inputs;
			dp->inputs = iop;
			break;
	
		 case DT_output:
			ng_bleat( 2, "add2mrad: type=%d output prevlist=%x", ot, dp->outputs );
			iop = (Io *) data;
			iop->next = dp->outputs;
			dp->outputs = iop;
			break;

		case DT_woomera:
			vp = (Value *) data;
			if( dp->woomera )
				ng_free( dp->woomera );
			dp->woomera = (char *) data;
			break;

		
		default:
			ng_bleat( 0, "add2mrad: type=%d unrecognised", ot );
			break;
	}

	return dlist;			/* the way andrew wrote the yacc, it must go back */
}

