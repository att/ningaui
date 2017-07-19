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
******************************************************************************
*
*  Mnemonic: ng_socket.h
*  Abstract: 	constants supporting the socket library ng_sixxxxxx routines.
*		This file contains the definitions that a "user" application
*            will include. It is also included by the siconst.h file.
*  Date:     26 March 1995
*  Author:   E. Scott Daniels
*  Mods: 	07 Oct 2001 : ported in for ningaui 
*		15 Apr 2008 (sd) : Added proto for sipoll().
*
*****************************************************************************
*/

#define TCP_DEVICE     "TCP"     /* device driver type requested on init/conn*/
#define UDP_DEVICE     "UDP"

#define SI_OPT_FORK    0x01      /* initialization options - fork sessions */
#define SI_OPT_FG      0x02      /* keep process in the "foreground" */
#define SI_OPT_TTY     0x04      /* processes keyboard interrupts if fg */
#define SI_OPT_ALRM    0x08      /* cause setsig to be called with alarm flg */

                                 /* callback function type used when registering a callback routine */
                                 /* used to indentify cb in ng_sicbreg */
#define SI_CB_SIGNAL   0         /* usr signal/alarm received */
#define SI_CB_UDATA    1         /* handles udp data arrival */
#define SI_CB_TDATA    2         /* handles tcp data arrival */
#define SI_CB_KDATA    3         /* handles data arrival from keyboard */
#define SI_CB_SECURITY 4         /* authorizes acceptance of a conect req */
#define SI_CB_CONN     5         /* called when a session is accepted */
#define SI_CB_DISC     6         /* called when a session is lost */
#define SI_CB_POLL     7

#define SI_CB_RDATA    SI_CB_UDATA         /* old -- "raw" data; use UDATA */
#define SI_CB_CDATA    SI_CB_TDATA         /* old -- 'cooked' data; use CDATA */

                                 /* return values from callback */
#define SI_RET_OK      0         /* processing ok */
#define SI_RET_ERROR   (-1)      /* processing not ok */
#define SI_RET_UNREG   1         /* unregester (never call again) the cb */
#define SI_RET_QUIT    2         /* set the shutdown flag */
#define SI_RET_CEDE    3	/* user wants control back w/o shutting down */

                                 /* values returned to user by SI routines */
#define SI_ERROR       (-1)      /* unable to process */
#define SI_OK          0         /* processing completed successfully */
#define SI_QUEUED      1         /* send message was queued */

                                  /* flags passed to signal callback */
#define SI_SF_QUIT     0x01      /* program should terminate */
#define SI_SF_USR1     0x02      /* user 1 signal received */
#define SI_SF_USR2     0x04      /* user 2 signal received */
#define SI_SF_ALRM     0x08      /* alarm clock signal received */

                                 /* signal bitmasks for the setsig routine */
#define SI_SIG_QUIT    0x01      /* please catch quit signal */
#define SI_SIG_HUP     0x02      /* catch hangup signal */
#define SI_SIG_TERM    0x04      /* catch the term signal */
#define SI_SIG_USR1    0x08      /* catch user signals */
#define SI_SIG_USR2    0x10
#define SI_SIG_ALRM    0x20      /* catch alarm signals */
#define SI_QHT_SIGS    0x07      /* all three 'terminate' signals quit, term, hup */
#define SI_DEF_SIGS    0x1F      /* default signals to catch (all but alarm)*/

                                 /* ng_sierrno values set in public rtns */
#define SI_ERR_NONE     0        /* no error as far as we can tell */
#define SI_ERR_NOMEM    1        /* could not allocate needed memory */
#define SI_ERR_TPORT    2        /* could not bind to requested tcp port */
#define SI_ERR_UPORT    3        /* could not bind to requested udp port */
#define SI_ERR_FORK     4        /* could not fork to become daemon */
#define SI_ERR_HANDLE   5        /* SIHANDLE passed in was bad */
#define SI_ERR_SESSID   6        /* invalid session id */
#define SI_ERR_TP       7        /* error occured in transport provider */
#define SI_ERR_SHUTD    8        /* cannot process because in shutdown mode */
#define SI_ERR_NOFDS    9        /* no file descriptors are open */
#define SI_ERR_SIGUSR1  10       /* signal received data not read */
#define SI_ERR_SIGUSR2  11       /* signal received data not read */
#define SI_ERR_DISC     12       /* session disconnected */
#define SI_ERR_TIMEOUT  13       /* poll attempt timed out - no data */
#define SI_ERR_ORDREL   14       /* orderly release received */
#define SI_ERR_SIGALRM  15       /* alarm signal received */
#define SI_ERR_QUEUED	16	 /* send had to queue the message */


/* prototypes */
#ifdef SI_RTN
void ng_siaddress( void *, void* , int );
void ng_sibldpoll( );					/* build information to do a poll with */
void ng_sicbstat( int, int );				/* test status returned by a callback */
struct tp_blk *ng_siestablish( char *, int );		/* establish a session after a connect request is received */
void *ng_sinew( int );					/* create a new datastructure */
int ng_sinewsess( struct tp_blk * );			/* setup a new session */
int ng_sisend( struct tp_blk * );			/* send queued data */
void ng_sisignal( int );				/* deal with a received signal */
void ng_siterm( struct tp_blk * );			/* terminate a session */
#endif

/* 'public' routines */
int ng_siblocked( int handle );				/* return true if send/sendt would queue the write in user space */
void ng_sicbreg( int, int ((*)()),  void * );		/* callback routine registration */
int ng_siclose( int );					/* close a session */
int ng_siclose_listen();				/* close first tcp listen port on the list */
int ng_siclose_udp();     				/* close first udp port on the list */
int ng_siconnect( char * );				/* connect to a remote process */
void ng_siforcequeue( int sid, int val );		/* set force2queue flag for session if value is not zero */
char * ng_sigetname( int );     				/* get name (address.port) that cooresponds to this side of the session id */
int ng_sigetlfd( );					/* get fd for the listen socket opened with siint() call */
int ng_siinit( int, int, int );				/* initialise the interface */
int ng_listen( int );					/* open a new tcp listen port -- port of 0 gets random, returns sid */
int ng_siopen( int );					/* open a udp port */
int ng_sipoll( int delay );				/* poll for activity on a session; block up to delay in 100s of a second */
int ng_simcast_join(  char *group, int iface, int ttl );	/* join/leave a multi-cast group.  group is the net: e.g. 236.0.0.7 */
int ng_simcast_leave( char *group, int iface );
int ng_siprintf( int sid, char *fmt, ...);
int ng_sircv( int, char *, int, char*, int );		/* receive data on a particular session */
int ng_sireinit( int );					/* reinitialise - sometimes necessary when linking in db apis */
void ng_sireuse( int tcp, int udp );			/* set/clear reuse flag */
int ng_sisendt( int, char *, int );			/* send a tcp datagram */
int ng_sisendu( char *, char *, int );			/* send a udp datagram */
int ng_siclear( int fd );				/* clear any queued buffers on the sssion */
void ng_sishutdown( );					/* close the interface */
int ng_siwait( );					/* poll all open sessions and drive callbacks as needed */


extern int ng_sierrno;					/* socket's private err reporting via SI_ERR constants */
