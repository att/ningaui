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

#define	TM_NODE		"NG_TAPE_CHANGER_NODE"
#define	TM_PORT		"NG_TAPE_CHANGER_PORT"
#define	FANNOUNCE	"FLOCK_default_tapedomain"
#define	TM_LOCAL_DOMAIN	"NG_default_tapedomain"
#define	MAX_N		20000

enum { C_notfound, C_dte, C_slot, C_portal, C_end };		/* if you add to this, update tname */
extern char *tname[C_end];

typedef struct
{
	char	*name;
	int	delta;
	char	*vol[NG_BUFFER];
	int	nvol;
} Pool;
extern Pool pool[];
extern int npool;
extern void pool_dump(Sfio_t *fp, char *poolid);
extern void pool_add(Sfio_t *fp, char *poolid, char *volid);
extern void pool_del(Sfio_t *fp, char *poolid, char *volid);

#define	NO_NAME		"??"
#define	EMPTY		"__empty"

typedef struct
{
	char	dev[MAX_N];
	int	min_dte, max_dte;
	char	*dte[MAX_N];
	int	min_slot, max_slot;
	char	*slot[MAX_N];
	int	min_portal, max_portal;
	char	*portal[MAX_N];
} Library;
extern Library changer;
extern void read_library(void);

typedef struct Devmap
{
	int	maxdte;
	struct {
		char	*tapeid;
		char	*sys;
		char	*dev;
	} dte[MAX_N];
} Devmap;
extern Devmap devmap;
extern void read_devmap(void);

extern char *domain;

extern void mtx_load(Sfio_t* fp, int type, int slot, int dte);
extern void mtx_unload(Sfio_t* fp, int dte, int type, int slot);	/* slot < 0 means i don't care */
extern void mtx_transfer(Sfio_t* fp, int src_t, int src_n, int dest_t, int dest_n);
extern void dev_offline(Sfio_t* fp, int dte);
extern int find_empty(Sfio_t* fp, int *val);
extern void heartbeat(void);

extern char *setfields(char *arg);
extern getfields(char *ss, char **sp, int nptrs);
extern getmfields(char *ss, char **sp, int nptrs);
