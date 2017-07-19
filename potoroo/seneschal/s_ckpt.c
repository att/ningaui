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
# Mnemonic:	ckpt.c 
# Abstract:	these functions support the limited checkpointing that seneschal does.
#		Seneschal keeps a tripple of information for each node that it is aware 
#		of: nodename:load:{R|S} where load is the number of jobs seneschal 
#		should try to keep running on the node, and R/S indicates whether 
#		the node was running or suspended at the time.  The first token of
#		the string is the overall running or suspended state of seneschal.
#		All of this is kept in a single narbalek variable: sene/<cluster>/nodeinfo.
#
#		Sene also keeps the n and t values for repmgr data (dump1) fetches
#		where n is the number to fetch and t is the time (sec) to delay
#		between fetches. narbalek variable is sene/cluster/rmif
#
#		resource information is kept in sene/cluster/rsrc:rname variables.
#		the var is created for each resource that has jobs that reference it.
#		if the refcount goes to 0, then the nar var is dropped (the data still
#		lives in seneschal).
#
#		There might be other checkpoint data kept further down the road in 
#		different sene/<cluster/* variables. 
#		
# Date:		16 Mar 2006
# Author:	E. Scott Daniels
# Mods:		17 May 2006 (sd) : sets node lease if the desired load read is < 0 
#		21 Mar 2007 (sd) : fix memory leak -- free value after getting nar stuff
#		10 Dec 2007 (sd) : Added queue processing and work limits to chkpointed data
# ---------------------------------------------------------------------------------
*/

static ng_timetype	next_ckpt = 0;	
static	char	*cname = NULL;

schedule_ckpt( )
{
	next_ckpt = 0;			/* force a checkpoint */
}


/* we will be sneaky and keep our node limit checkpoint in narbalek! */
write_ckpt( ng_timetype now )
{
	Node_t	*n;
	Resource_t *rp;
	char	value[NG_BUFFER];
	char	vname[250];
	char	wbuf[100];
	int	vlen = 0;		/* len of stuff in value */

	if( now < next_ckpt )
		return;

	next_ckpt = now + CKPT_FREQ;

	if( ! cname )
		cname = ng_env( "NG_CLUSTER" ); 

	if( ! cname )
		return;

	if( nodes )					/* only if there are nodes */
	{
		sfsprintf( vname,  sizeof( vname ), "sene/%s/nodeinfo", cname );
	
		value[0] = 0;
		vlen = sfsprintf( value, sizeof( value ), "%s ", suspended ? "SUSPENDED" : "RUN" );

		for( n = nodes; n; n = n->next )
		{
			if( !(n->flags & NF_RELEASED) )
			{
				vlen += sfsprintf( wbuf, sizeof( wbuf ), "%s:%d:%s ", n->name, n->desired, n->suspended ? "S" : "R" );
				if( vlen >= NG_BUFFER )
					break;					/* little chance, and little harm if we cut it off */
				strcat( value, wbuf );
			}
		}

		ng_bleat( 2, "write_ckpt: %s=%s", vname, value );
		ng_nar_set( vname, value );
	}

	value[0] = 0;							/* repmgr message request limits -- moot when d1agent is running */
	sfsprintf( value, sizeof( value ), "%d %d  (max_rm_req, freq_rm_req)", max_rm_req, freq_rm_req );
	sfsprintf( vname,  sizeof( vname ), "sene/%s/rmif", cname );
	ng_nar_set( vname, value );
	ng_bleat( 2, "write_ckpt: %s=%s", vname, value );

	value[0] = 0;							/* queue processing limits */
	sfsprintf( value, sizeof( value ), "%d %d (max_work, max_resort)", max_work, max_resort );
	sfsprintf( vname,  sizeof( vname ), "sene/%s/qplimits", cname );
	ng_nar_set( vname, value );
	ng_bleat( 2, "write_ckpt: %s=%s", vname, value );

	for( rp = resources; rp; rp = rp->next )
	{
		sfsprintf( vname, sizeof( vname ), "sene/%s/rsrc:%s", cname, rp->name );
		if( rp->ref_count > 0 )
			sfsprintf( value, sizeof( value ), "%d%s %d", rp->limit, rp->flags & RF_HARD ? "h" : "s", rp->mpn );
		else
			*value = 0;					/* no references, drop the checkpoint data */
		ng_bleat( 2, "write_ckpt: %s=%s", vname, value );
		ng_nar_set( vname, value );
	}
}

read_ckpt( )
{
	Resource_t	*rp;	/* pointer at a resource block */
	char	*tok;
	char	*p;		/* generic pointer into token */
	char	*np;		/* pointer into the string at a name */
	char	*value;		/* value from narbalek */
	char	vname[250];
	char	*nvlist[1024];	/* variable list from narbalek */
	char	*strtok_p;
	int	i;
	int	nvcount = 0;	/* variable count from narbalek */
	Node_t	*n;

	if( ! cname )
		if( (cname = ng_env( "NG_CLUSTER" )) == NULL  )
			return;

	sfsprintf( vname,  sizeof( vname ), "sene/%s/nodeinfo", cname );

	if( (value = ng_nar_get( vname )) != NULL )
	{
		tok = strtok_r( value, " ", &strtok_p );
		suspended = *tok == 'S' ? 1 : 0;			/* overall suspendedness of seneschal is frist token */
		while( tok = strtok_r( NULL, " ", &strtok_p ) )
		{
			
			if( p = strchr( tok, ':' ) )
			{
				*p = 0;
				if( (n = get_node( tok, CREATE )) )
				{
					n->desired = atoi( p+1 );
					if( n->desired < 0 )			/* node was suspended because of bad womera */
						n->lease = ng_now( ) + 3000;	/* set a lease of 5m and then open the node back up */

					if( (p = strchr( p+1, ':' )) )
						n->suspended = *(p+1) == 'S' ? 1 : 0;

					ng_bleat( 1, "ckpt_read: added/updated node: %s %d(load) %s", n->name, n->desired, n->suspended ? "SUSPENDED" : "RUNNING" );
				}
			}
		}
		ng_free( value );
	}



	sfsprintf( vname,  sizeof( vname ), "sene/%s/rmif", cname );		/* repmgr interface info */
	if( (value = ng_nar_get( vname )) != NULL )
	{
		ng_bleat( 1, "ckpt_read: setting max_rm_req/freq_rm_req: %s", value );

		if( (tok = strtok_r( value, " ", &strtok_p )) != NULL ) 		/* at num per requst */
		{
			max_rm_req = atoi( tok );			
			if( (tok = strtok_r( NULL, " ", &strtok_p )) != NULL ) 		/* at freq  */
				freq_rm_req = atoi( tok );
		}
	
		ng_free( value );
	}

	sfsprintf( vname,  sizeof( vname ), "sene/%s/qplimits", cname );		/* queue processing limits */
	if( (value = ng_nar_get( vname )) != NULL )
	{
		int i;
		long v;

		ng_bleat( 1, "ckpt_read: setting variables from value: %s", value );

		tok = strtok_r( value, " ", &strtok_p );
		i = 0;
		while( tok )
		{
			v = atol( tok );			
			switch( i )
			{
				case 0:		max_work = v; break;
				case 1: 	max_resort = v; break;
				default:	break;
			}

			i++;
			tok = strtok_r( NULL, " ", &strtok_p );
		}
	
		ng_free( value );
	}

	/* pick up resource settings; expect:  sene/cluster/rsrc:rname=limit[h]  mpn    */
	sfsprintf( vname, sizeof( vname ), "sene/%s/rsrc:", cname );
	nvcount = ng_nar_namespace( &nvlist, 1024, vname, '=' );			/* passing = causes name=value to be returned rather than just name */
	ng_bleat( 1, "ckpt_read: %d resource settings found in narbalek", nvcount );
	for( i = 0; i < nvcount; i++ )
	{
		if( (np = strchr( nvlist[i], ':' )) != NULL && (p = strchr( nvlist[i], '=' )) != NULL )
		{
			np++;

			ng_bleat( 1, "ckpt_read: setting resource from value: %s", np );
			*p = 0;
			p++;
			while( *p && (isspace( *p ) || *p == '"')  )			/* nar likes to quote things */
				p++;
			if( (rp = get_resource( np, CREATE )) != NULL )
			{
				rp->limit = atoi( p );			/* snarf the limit value */
				p++;
				while( *p && !isspace( *p ) )
					p++;
				if( *(p-1) == 'h' )
					rp->flags |= RF_HARD;		/* its a hard limit not a soft one */
				if( *p )
					rp->mpn = atoi( p );		/* snarf the max per node value */
			}
		}

		ng_free( nvlist[i] );
	}

}

