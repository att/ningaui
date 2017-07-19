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

#define	NCON	10

typedef struct Con {
	int		fd;
	struct Con	*next;
} Con;

typedef struct {
	int		serv_fd;
	pthread_t	thread;
	pthread_t	con[NCON];
	pthread_mutex_t	lock;
	Chunk		*top;
	Chunk		*last;
	Con		*cons;
} Input_block;
extern Chunk *getchunk(Input_block *);
extern void putchunk(Input_block *, char *, int);
extern void putcon(Input_block *, int);
extern int getcon(Input_block *);

#define	P()	pthread_mutex_lock(&i->lock)
#define	V()	pthread_mutex_unlock(&i->lock)

extern void *con_thread(void *);
