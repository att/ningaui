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
**************************************************************************
*  Mnemonic: ng_siinit
*  Abstract: This routine is responsible for initializing the Socket
*            Interface runtime environment. It will allocate the master
*            global information block and, based on the flags passed in,
*            will establish listen ports, and various flags that will
*            determine how the library functions later.
*            This routine also owns the global definitons.
*  Parms:    opts - Options:
*              SI_OPT_FORK - Fork a new process for each session connected
*              SI_OPT_FG   - Keep task in foreground.
*              SI_OPT_TTY  - Open and poll the stdin device
*              SI_OPT_ALRM - Setu to trap the alarm signal
*            tport - Port number to set listen TCP fd on (if > 0)
*            uport - Port number to open UDP listen port; if 0 any port is
*                    requested.
*  Returns:  1 if all is ok, 0 if there was a problem
*            it is referred to as a handle to keep the user from having to
*            know what it is.
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*  Mods:
*		03 May 2005 (sd) - added support for primary listen fd
*		08 Oct 2008 (sd) - added support for no delay flag (version set to 2.1). 
*
**************************************************************************
*/
#include  "sisetup.h"     /* get the setup stuff */

char *lib_ver = "ngsi_version 2.1/0a088";		/* make it easy to see in strings which lib we picked up */


/* this routine "owns" the global variables */
int deaths = 0;              /* deaths that the parent is not aware of */
int sigflags = 0;            /* signal processing flags SF_ constants */
int ng_sierrno;                 /* external error number for caller to use */

				/* these flags are set using setsockopt() and must be individual integers not bit flags */
int si_tcp_reuse_flag = 0;		/* siestablish will set the reuse flag on the fd before bind */
int si_udp_reuse_flag = 0;		/* siestablish will set the reuse flag on the fd before bind */
int si_no_delay_flag = 0;		/* with caution - set nodelay which will disable delayed ack messages */
struct ginfo_blk *gptr = NULL;

void ng_sireuse( int t, int u )
{
	si_tcp_reuse_flag = t;
	si_udp_reuse_flag = u;
}

/* set nodelay flag for all future established sessions */
void ng_sinodelay( int v )
{
	si_no_delay_flag = !(v == 0);
}

int ng_siinit( int opts, int tport, int uport )
{
	extern int errno; 
 int status = OK;              /* status of internal processing */
 struct tp_blk *tpptr;         /* pointer at tp stuff */
 struct sigaction sact;        /* signal action block */
 int i;                        /* loop index */
 int signals = SI_DEF_SIGS;    /* signals to be set in ng_sisetsig */

 ng_sierrno = SI_ERR_NOMEM;       /* setup incase alloc fails */

 gptr = ng_sinew( GI_BLK );                /* get a gi block */
 if( gptr != NULL )                     /* was successful getting storage */
  {
   gptr->kbfile = -1;                   /* default to no keyboard */
   gptr->rbuf = (char *) ng_malloc( MAX_RBUF, "siinit:rbuf" );   /* get rcv buffer*/
   gptr->rbuflen = MAX_RBUF;

   if( opts & SI_OPT_ALRM )     /* turn on alarm signal too? */
    signals |= SI_SIG_ALRM;     /* add it to the list */
   ng_sisetsig( signals );         /* set up signals that we want to catch */

   if( opts & SI_OPT_FORK )    /* does caller want to fork for sessions? */
    {
     gptr->flags |= GIF_FORK;            /* yes - set flag to cause this */

     memset( &sact, 0, sizeof( sact ) );
     sact.sa_handler = &ng_sisignal;         /* setup our signal trap */
     sigaction( SIGCHLD, &sact, NULL );    /* also get signals on child deaths*/
    }

   ng_sierrno = SI_ERR_TPORT;

   if( tport > 0 )                      /* if user wants a TCP listen socket */
    {
	tpptr = ng_siestablish( TCP_DEVICE, tport ); /* bind to tp */

     if( tpptr != NULL )                          /* got right port */
      {                                           /* enable connection reqs */
       if( (status = listen( tpptr->fd, 1 )) >= OK )
        {
         gptr->tplist = tpptr;                 /* add blk to list */
         tpptr->flags |= TPF_LISTENFD;         /* this is where we listen */
	 	gptr->primary_listen_fd = tpptr->fd;		/* makes closing listener while draining easy */
        }
      }                                        /* end if tcp port ok */
     else
      status = ERROR;                          /* didnt get requested port */
    }                                          /* end if tcp port > 0 */

   if( uport >= 0 && status != ERROR )       /* does user want a UDP port? */
    {
	errno = 0;
     ng_sierrno = SI_ERR_UPORT;
     tpptr = ng_siestablish( UDP_DEVICE, uport ); /* estab with udp */
     if( tpptr != NULL )
      {                                /* got our requestd port */
       tpptr->next = gptr->tplist;
       if( tpptr->next != NULL )       /* if not at the head of the list */
        tpptr->next->prev = tpptr;
       gptr->tplist = tpptr;           /* add to list */
      }
     else
      status = ERROR;            /* could not allocate requested port */
    }                            /* end if specific udp port numer */

   if( status == ERROR )         /* if an establish failed or wrong port */
    {
     ng_sishutdown( gptr );           /* clean up the list */
     ng_free( gptr );
     return 1;             /* get out now with an error to user */
    }

   if( ! (opts & SI_OPT_FG) )    /* user has not selected fground option */
    {                            /* so detach us from control terminal */
     ng_sierrno = SI_ERR_FORK;

     chdir( "/" );                    /* prevent causing unmounts to fail */
     status = fork( );                /* fork to ensure not process grp lead */
     if( status > 0 )                  /* if parent */
      exit( 0 );                         /* then just get out */
     if( status < 0 )                  /* fork failed */
      {
       ng_free( gptr );                   /* so trash the block and */
       return 1;                 /* get out while the gettn's good */
      }
                                       /* we are the 1st child */
#ifdef OS_LINUX
     setpgrp( );                       /* set us as the process group leader */
#endif
     close( 0 );                       /* close the stdxxx files */
     close( 1 );
     close( 2 );
    }
   else                         /* if in foreground we can have a keyboard */
    if( opts & SI_OPT_TTY )                     /* does user want it ? */
     gptr->kbfile = 0;                   /* "open" the file */

   gptr->cbtab = (struct callback_blk *) ng_malloc( (sizeof( struct callback_blk ) * MAX_CBS ), "siinit:cbtab" );
   if( gptr->cbtab != NULL )
    {
     for( i = 0; i < MAX_CBS; i++ )     /* initialize call back table */
      {
       gptr->cbtab[i].cbdata = NULL;    /* no data and no functions */
       gptr->cbtab[i].cbrtn = NULL;
      }
    }                   /* end if gptr->ctab ok */
   else                 /* if call back table allocation failed - error off */
    {
     ng_sishutdown( gptr );  /* clean up any open fds */
     ng_free( gptr );
     gptr = NULL;       /* dont allow them to continue */
    }
  }                     /* end if gen infor block allocated successfully */

 return 0;		/* all ok */
}                       /* ng_siinit */

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_siinit:Socket Interface Initialisation)

&space
&synop	int ng_siinit( int opts, int tport, int uport )

&space
&desc	This function initialises the socket interface which can then be used 
	by an application to send and receive TCP and UDP messages over IP. It 
	provides for multiple concurrent sessions, and active listening for 
	connection requests.  The interface to the user programme is provided 
	via the registration of one or more callback functions, and then the 
	invocation of &lit(ng_siwait)) or &lit(ng_sipoll()). 

&space
&parms
&begterms
&term	opts : Options that control the behaviour of the socket interface. This may be
	any of these constants included from ng_socket.h:
&space
&begterms
&term	SI_OPT_FORK : Fork a new process for each session connected.
&term	SI_OPT_TTY  : Open and poll the stdin device (assuming it is the keyboard).
&term	SI_OPT_ALRM : Set to trap the alarm signal.
&endterms

&endterms

&space
&exit	A zero return code indicates success. A non-zero return code indicates
	failure.  The global variables &lit(errno) and &lit(ng_sierrno) will
	likely contain useful debugging information. 

&space
&see	

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	26 Mar 1995 (sd) : Its beginning of time. 


&scd_end
------------------------------------------------------------------ */
