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
***************************************************************************
*
* Mnemonic: sistruct.h
* Abstract: This file contains the structure definitions ncessary to support
*           the SI (Socket interface) routines.
* Date:     26 March 1995
* Author:   E. Scott Daniels
*
******************************************************************************
*/

struct ioq_blk              /* block to queue on session when i/o waiting */
 {
  struct ioq_blk *next;     /* next block in the queue */
  char *data;               /* pointer to the data buffer */
  char *sdata;			/* pointer into databuffer where send should start (do NOT free)*/
  unsigned int dlen;        /* bytes remaining to send */
  void *addr;               /* address to send to (udp only) */
 };

struct callback_blk         /* defines a callback routine */
 {
  void *cbdata;            /* pointer to be passed to the call back routine */
  int ((*cbrtn)( ));       /* pointer to the callback routine */
 };

struct tp_blk               /* transmission provider info block */
 {
  struct tp_blk *next;      /* chain pointer */
  struct tp_blk *prev;      /* back pointer */
  int fd;                   /* open file descriptor */
  int flags;                /* flags about the session / fd */
  int type;                 /* connection type SOCK_DGRAM/SOCK_STREAM */
  struct sockaddr_in *addr; /* connection local address */
  struct sockaddr_in *paddr; /* session partner's address */
  struct ioq_blk *squeue;   /* queue to send to partner when it wont block */
  struct ioq_blk *stail;     /* tail of the send queue */
 };

struct ginfo_blk            /* global - general info block (global pointer maintained to this info) */
 { 
  unsigned int magicnum;     /* magic number that ids a valid block */
  struct tp_blk *tplist;     /* pointer at tp block list */
  fd_set readfds;            /* set of flags indicating sids we want to test */
  fd_set writefds;           /* by the signal call for read or write */
  fd_set execpfds;           /* by the signal call for read or write */
  char *rbuf;                /* read buffer */
  struct callback_blk *cbtab; /* pointer at the callback table */
  int fdcount;               /* largest fd to select on in siwait */
  int flags;                 /* status flags */
  int childnum;              /* number assigned to next child */
  int kbfile;                /* AFI handle on stdin */
  int rbuflen;               /* read buffer length */
  int primary_listen_fd;     /* the first fd opened to listen for tcp conn requests */
 };

