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

#include <string.h>
#include	<signal.h>
#include	<stdlib.h>
#include <sfio.h>
#include "ningaui.h"
#include "ng_ext.h"
#include "repmgr.h"

#define	MAX_IDLE	10		/* max time idle before flush */
#define	MAX_FLUSH	60		/* max time between flushes */

struct {
	char	*b;
	int	n;
	int	a;
	time_t	t;
} buf;
void flushbuf(char *port);

int
main(int argc, char **argv)
{
	char *port = ng_env("NG_REPMGR_PORT");
	char *rport = ng_env("NG_REPMGRS_PORT");
	int serv_fd, fd;
	int i, k;
	int iport;
	int going;

	ARGBEGIN{
	case 'p':	SARG(port); break;
	case 'r':	SARG(rport); break;
	case 'v':	OPT_INC(verbose); break;
	default:	goto usage;
	}ARGEND
	if(argc != 0){
usage:
		sfprintf(sfstderr, "Usage: %s [-p port] [-r port] [-v]\n", argv0);
		exit(1);
	}

	if(verbose > 1)
		sfprintf(sfstderr, "%s: attempting ipc from %s\n", argv0, port);
	if((serv_fd = ng_serv_announce(port)) < 0){
		if(verbose)
			sfprintf(sfstderr, "%s: announce failed: using port %s\n", argv0, port);
		exit(1);
	}

	buf.a = 250000;
	buf.n = 0;
	buf.b = ng_malloc(buf.a, "buffer");
	buf.t = time(0);

	ng_bleat(2, "entering main loop");
	for(;;){
		ng_bleat(2, "probing: ");
		fd = ng_serv_probe_wait(serv_fd, MAX_IDLE);
		ng_bleat(2, "probe returns %d", fd);
		if(fd < 0)
			return 0;
		if(fd){
			while((k = read(fd, buf.b+buf.n, buf.a-buf.n)) > 0){
				for(i = 0; i < k; i++)
					if((0xFF&buf.b[buf.n+i]) == EOF_CHAR){
						buf.n += i;
						goto done;
					}
				buf.n += k;
				if((buf.a-buf.n) < 1024){
					buf.a *= 2;
					buf.b = ng_realloc(buf.b, buf.a, "buffer");
				}
			}
		done:
			close(fd);
		} else
			flushbuf(rport);
		if(buf.t+MAX_FLUSH <= time(0))
			flushbuf(rport);
	}

	/* cleanup */
	ng_serv_unannounce(serv_fd);
	ng_bleat(1, "exiting normally");

	exit(0);
}
