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

/* socket interface routines */

#include <ng_tokenise.h>
#include <ng_socket.h>

static int cb_newsess( void *data, int sid, char *from )
{
	ng_bleat( 3, "new session attached" );
	return SI_RET_OK;		/* accept and move on; we do nothing special with the session */
}

static int cb_disc( void *data, int sid )
{
	ng_bleat( 3, "session detached" );
	return SI_RET_OK;		
}

/* invoked for each bunch of data received on tcp session */
static int cb_tcpdata( void *data, int sid, char *buf, int len )
{
	static char **tokens = NULL;
	static void *flow = NULL;
	int	ntoks;
	char	*rec;			/* pointer to next record that was in the buffer */
	

	if( ! ok2run )
		return SI_RET_OK;

	if( ! flow )
		flow = ng_flow_open( NG_BUFFER );		/* establish a flow input buffer for the session */

	ng_flow_ref( flow, buf, len );              		/* regisger new buffer with the flow manager */

	while( rec = ng_flow_get( flow, '\n' ) )		/* get all complete (new line terminated) records */
	{
		tokens = ng_tokenise( rec, ':', '\\', tokens, &ntoks );
		ng_bleat( 4, "cooked data: ntoks=%d  %s %s", ntoks, tokens[0],  rec );
		switch( *tokens[0] )
		{
			case 'd':			/* delete:{test|digest}:name */
				if( ntoks > 2 )
				{
					if( strcmp( tokens[1], "digest" ) ==  0 )
					{
						scratch_digest( tokens[2] );
						ng_siprintf( sid, "digest deleted: %s\n", tokens[2] );
					}
					else
					if( strcmp( tokens[1], "test" ) == 0 )
					{
						if( ! scratch_test( (Test_t *) st_find( SP_TEST, tokens[2] ) ) )
							ng_siprintf( sid, "could not delete test: %s (see NG_ROOT/site/log/spyglass)\n", tokens[2] );
						else
							ng_siprintf( sid, "test deleted: %s\n", tokens[2] );
					}
					else
						ng_siprintf( sid, "delete request syntax invalid: %s is not delete:{test|digest}:name\n", buf );
				}
				else
					ng_siprintf( sid, "delete request syntax invalid: %s is not delete:{test|digest}:name\n", buf );
				break;

			case 'e':			/* explain[:testname] or explain:digest[:digest-name] */
				if( ntoks > 1 )
				{
					if( strcmp( tokens[1], "digest" ) == 0 )
						explain_digest( sid, tokens[2] );
					else
						explain_named_test( sid, tokens[1] );
				}
				else
				{
					dump_all( sid );
					explain_digest( sid, NULL );		
				}
				break;

			case 'l':				/* list */
				list_test_chain( sid, tlist, 0 );
				break;

			case 'p':			
				if( ntoks > 1 )
				{
					if( *(tokens[0]+2) == 'r' )				 /* parse:config-file-name */
					{
						if( parse_config( tokens[1] ) > 0 )
							ng_siprintf( sid, "config file parsed: %s\n", tokens[1] );
						else
							ng_siprintf( sid, "config file failed to parse: %s\n", tokens[1] );
					}
					else							/* assume pause:n */
					{
						pause_until = ng_now() + (atoi( tokens[1] ) * 10);
						ng_bleat( 0, "pause command received; pausing %s seconds", tokens[1] );
						ng_siprintf( sid, "pause command accepted\n" );
					}
				}
				else
					ng_siprintf( sid, "parse request did not contain the config file name\n" );
				break;

			case 'S':			/* S[tate]:value */
				if( ntoks > 1 )
					cur_sst = atoi( tokens[1] );
				else
					ng_bleat( 0, "bad state string, expected State:value, got: %s", rec );
				break;
	
			case 's':		/* status:testname:state:filename:[subject-data]:[key]   or   squelch:testname:[seconds|on|off|reset] */
				if( *(tokens[0]+1) == 'q' )			/* squelch */
				{
					process_squelch( tokens[1], tokens[2] ? tokens[2] : "on" );	/* default to setting on */
				}
				else
				{
					ng_bleat( 3, "status received: %s", rec );
					if( ntoks >= 4 )
						process_status( tokens[1], tokens[2], tokens[3], tokens[4], tokens[5] );
					else
						ng_siprintf( sid, "bad status string, expected status:test-id:value:filename:subject-data:key, got: %s", rec );
				}
				break;

			case 'v':
				switch( ntoks )
				{
					case 2:
						verbose = atoi( tokens[1] );
						ng_bleat( 0, "%s verbose now changed to %d", version, verbose );
						break;

					case 3:			/* verbose:testname:value */
						set_test_verbose( tokens[1], tokens[2] );
						break;

					default: 
						break;
				}
				break;
			
			case 'x':
				if( strcmp( tokens[0], "xit050705" ) == 0 )
				{
					ng_bleat( 0, "valid exit string received; shutting down" );
					ok2run = 0;
				}
				break;

			default:
				ng_bleat( 0, "unrecognised command received via TCP/IP: (%s)", rec );
				break;
		}
	}

	ng_siprintf( sid, "#end#\n" );		/* nice termination string */

	return ok2run ? SI_RET_OK : SI_RET_QUIT;
}

static int cb_udpdata( void *data, char *buf, int len, char *from, int sid )
{
	buf[len-1] = 0;
	ng_bleat( 3, "udp message ignored: %s", buf );
	return SI_RET_OK;
}

void init_si( char *port )
{
	int tries = 10;

	while( tries--  )
	{
		if( ng_siinit( SI_OPT_FG, atoi( port ), atoi( port ) ) == 0 )		/* initialise the network interface */
		{
			ng_sicbreg( SI_CB_CONN, &cb_newsess, NULL );   		/* called when connection is received */
			ng_sicbreg( SI_CB_DISC, &cb_disc, NULL );    		/* disconnection */
			ng_sicbreg( SI_CB_CDATA, &cb_tcpdata, (void *) NULL );	/* cooked (tcp) data recieved */
			ng_sicbreg( SI_CB_RDATA, &cb_udpdata, (void *) NULL );	/* raw (udp) data received */

			ng_bleat( 1, "callbacks registered; listening on port %s", port );
			return;
		}
		else
		{
			if( tries && errno == EADDRINUSE )
			{
				ng_bleat( 0, "network interface port in use; waiting a bit to see if it frees up" );
				sleep( 15 );
			}
			else
			{
				ng_bleat( 0, "abort: unable to initialise network interface for port: %s: %s", port, strerror( errno ) );
				exit( 1 );
			}
		}
	}

	ng_bleat( 0, "abort: unable to initialise network interface for port: %s: %s", port, strerror( errno ) );
	exit( 1 );

}
