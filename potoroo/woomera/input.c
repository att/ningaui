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
	mod:	02 Apr 2007 (sd) : Fixed check in limit to error if second token 
			is omitted. 
*/

#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<time.h>
#include	<ctype.h>
#include	<string.h>
#include	"ningaui.h"
#include	"woomera.h"
#include	<errno.h>

static int	jid = 1;			/* job naming scheme */
static Syment	*force_resource(char *name, FILE *out);

int
scan_req(int serv_fd)
{
	FILE *in, *out;
	int fd;
	Channel *c;
extern int errno;

errno = 0;
	fd = ng_serv_probe_wait(serv_fd, 1);		/* block for at most 1 second while waiting for connect */
	if(fd < 0)
		return 1;
	if(fd == 0)
		return 0;
	out = fdopen(fd, "w");
	/* need to close out just once, so use the channel stuff */
	c = channel(out);
	inc_channel(c);

	in = fdopen(dup(fd), "r");
	grok("socket", in, out);
	if(ferror(in) && (errno != EAGAIN)){
		fprintf(stderr, "input read failed: %s\n", ng_perrorstr());
		ng_log(LOG_ERR, "input read failed: %s\n", ng_perrorstr());
	}
	fclose(in);
	dec_channel(c);
	if(verbose > 1)
		fprintf(stderr, "after grok: c=%d fp=%d n=%d fd=%d\n", c, c->fp, c->count, fd);
	return 0;
}

typedef enum {
	Tok_after, Tok_list, Tok_lists, Tok_listr, Tok_pause, Tok_resume,
	Tok_pmin, Tok_pmax, Tok_pload, Tok_run, Tok_anystatus, Tok_limit,
	Tok_explain, Tok_blocked, Tok_purge, Tok_verbose, Tok_dump, Tok_save,
	Tok_ready, Tok_status, Tok_expires, Tok_push,
	Tok_times,
	Tok_exit, Tok_job, Tok_sjob, Tok_cmd, Tok_colon, Tok_comma, Tok_pri, Tok_unknown,
	Tok_EOF
} Token_Type;
typedef struct
{
	Token_Type	type;
	char		*val;
} Token;
static char	*linebuf = 0;
static int	linelen = 0;
static char	*errbuf = 0;
static int	errlen = 0;
static char	*tokbuf = 0;
static int	tokbuflen = 0;
static int	lineno;
static Token	*tokens;
static int	atokens;
static int	readline(FILE *);
static void	tokenize(void);
static Token_Type	predefined(char *);
static int	after(Node *n, int toki, FILE *out);
static int	eoline(int toki, FILE *out);
static Channel	*orphan;

#define	ERRPREP		if(linelen+NG_BUFFER > errlen) errbuf = ng_realloc(errbuf, errlen=linelen+NG_BUFFER, "error buffer")
#define	ERRPRINT(err)	fprintf(stderr, "ERROR-> %s\n", err), fprintf(out, "%c%s\n", ERRCHAR, err), err[NG_BUFFER-16] = 0, ng_log(WLOG, "%s\n", err)

int
grok(char *file, FILE *fp, FILE *out)
{
	Node *n;
	char buf[NG_BUFFER];
	char *s;
	int i;
	Syment *sym;

	if(verbose > 1)
		fprintf(stderr, "grok(%s): out=%d\n", file, out);
	if(orphan == 0){
		orphan = channel(stdout);
		/* make sure it never closes */
		inc_channel(orphan);
		inc_channel(orphan);
	}
	lineno = 1;
	while(readline(fp)){
		tokenize();
		switch(tokens[0].type)
		{
		case Tok_exit:
			n = new_node(N_op);
			n->op = O_exit;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_blocked:
			n = new_node(N_op);
			n->op = O_blocked;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_explain:
			n = new_node(N_op);
			n->op = O_explain;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			for(; tokens[i].type == Tok_unknown; i++)
				add_arg(n, ng_strdup(tokens[i].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_pause:
			n = new_node(N_op);
			n->op = O_pause;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			for(; tokens[i].type == Tok_unknown; i++)
				add_arg(n, ng_strdup(tokens[i].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_resume:
			n = new_node(N_op);
			n->op = O_resume;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			for(; tokens[i].type == Tok_unknown; i++)
				add_arg(n, ng_strdup(tokens[i].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_purge:
			n = new_node(N_op);
			n->op = O_purge;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(tokens[i].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected an identifier at %s: src=%s", tokens[i].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			for(; tokens[i].type == Tok_unknown; i++)
				add_arg(n, ng_strdup(tokens[i].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_push:
		case Tok_run:
			n = new_node(N_op);
			n->op = (tokens[0].type == Tok_run)? O_run:O_push;
			n->out = channel(out);
			i = after(n, 1, out);
			if(tokens[i].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected an identifier at %s: src=%s", tokens[i].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			for(; tokens[i].type == Tok_unknown; i++)
				add_arg(n, ng_strdup(tokens[i].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_sjob:
		case Tok_job:
			if(tokens[1].type == Tok_colon){
				sprintf(buf, "j%d_%d", getpid(), jid++);
				i = 2;
			} else {
				strcpy(buf, tokens[1].val);
				if(tokens[2].type != Tok_colon){
					ERRPREP;
					sprintf(errbuf, "syntax error: expected a colon, got %s: src=%s", tokens[2].val, linebuf);
					ERRPRINT(errbuf);
					return 1;
				}
				i = 3;
			}
			if(sym = symlook(syms, buf, S_JOB, 0, SYM_NOFLAGS)){
				n = (Node *)sym->value;
				if(verbose > 2)
					fprintf(stderr, "\t\treusing node %s for %s\n", n->name, buf);
				if(n->status == J_unknown)
					n->status = J_not_ready;
				if(n->cmd){
					ng_free(n->cmd);
					n->cmd = 0;
				}
			} else {
				if(verbose > 2)
					fprintf(stderr, "\t\tnew node %s\n", buf);
				n = new_node(N_job);
				n->name = ng_strdup(buf);
			}
			n->out = (tokens[0].type == Tok_sjob)? channel(out):orphan;
			inc_channel(n->out);
			n->cmd = 0;
			n->pri = DEFAULT_PRI;
			while(tokens[i].type != Tok_EOF){
				switch(tokens[i].type)
				{
				case Tok_pri:
					i++;
					if(tokens[i].type == Tok_EOF){
						ERRPREP;
						sprintf(errbuf, "syntax error: expected a priority: src=%s", linebuf);
						ERRPRINT(errbuf);
						return 1;
					}
					n->pri = strtol(tokens[i].val, &s, 10);
					if(n->pri > MAX_PRI)
						n->pri = MAX_PRI;
					if(*s){
						ERRPREP;
						sprintf(errbuf, "syntax error: bad format in priority (at %s): src=%s", s, linebuf);
						ERRPRINT(errbuf);
						return 1;
					}
					i++;
					break;
				case Tok_status:
				case Tok_ready:
				case Tok_expires:
				case Tok_after:
					i = after(n, i, out);
					break;
				case Tok_cmd:
					n->cmd = ng_strdup(tokens[i].val);
					i++;
					break;
				case Tok_unknown:
					add_rsrc(n, resource(tokens[i].val));
					i++;
					break;
				default:
					ERRPREP;
					sprintf(errbuf, "syntax error: in job spec at %s: src=%s", tokens[i].val, linebuf);
					ERRPRINT(errbuf);
					return 1;
				}
			}
			if(n->cmd == 0){
				ERRPREP;
				sprintf(errbuf, "syntax error: cmd missing in job: src=%s", linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			jadd(n);
			break;
		case Tok_pmax:
			n = new_node(N_op);
			n->op = O_pmax;
			n->out = orphan;
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a number, got %s: src=%s", tokens[1].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_pmin:
			n = new_node(N_op);
			n->op = O_pmin;
			n->out = orphan;
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a number, got %s: src=%s", tokens[1].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_pload:
			n = new_node(N_op);
			n->op = O_pload;
			n->out = orphan;
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a number, got %s: src=%s", tokens[1].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_verbose:
			n = new_node(N_op);
			n->op = O_verbose;
			n->out = orphan;
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a number, got %s: src=%s", tokens[1].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_limit:
			n = new_node(N_op);
			n->op = O_limit;
			n->out = orphan;
			inc_channel(n->out);
			i = after(n, 1, out);
			if( !( (tokens[i].type == Tok_unknown) && (tokens[i+1].type == Tok_unknown))  ){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a resourceid and number: src=%s", linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			force_resource(tokens[i].val, out);
			add_arg(n, ng_strdup(tokens[i++].val));
			add_arg(n, ng_strdup(tokens[i++].val));
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_times:
			n = new_node(N_op);
			n->op = O_times;
			n->out = channel(out);
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a comment word, got %s", linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_list:
			n = new_node(N_op);
			n->op = O_list;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_lists:
			n = new_node(N_op);
			n->op = O_lists;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_listr:
			n = new_node(N_op);
			n->op = O_listr;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_dump:
			n = new_node(N_op);
			n->op = O_dump;
			n->out = channel(out);
			inc_channel(n->out);
			i = after(n, 1, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_save:
			n = new_node(N_op);
			n->op = O_save;
			n->out = orphan;
			inc_channel(n->out);
			if(tokens[1].type != Tok_unknown){
				ERRPREP;
				sprintf(errbuf, "syntax error: expected a filename, got %s: src=%s", tokens[1].val, linebuf);
				ERRPRINT(errbuf);
				return 1;
			}
			add_arg(n, ng_strdup(tokens[1].val));
			i = after(n, 2, out);
			if(!eoline(i, out))
				goto skip;
			add_prereq(node_r, n, 1);
			break;
		case Tok_EOF:
			/* blank line */
			break;
		case Tok_unknown:
			ERRPREP;
			sprintf(errbuf, "syntax error: unknown woomera directive at %s: src=%s", tokens[0].val, linebuf);
			ERRPRINT(errbuf);
			return 1;
			break;
		default:
			ERRPREP;
			sprintf(errbuf, "internal error: unimplemented woomera directive: src=%s", linebuf);
			ERRPRINT(errbuf);
			return 1;
			break;
		}
skip:;
	}
	return 0;
}

static int
eoline(int toki, FILE *out)
{
	if(tokens[toki].type == Tok_EOF)
		return 1;
	ERRPREP;
	sprintf(errbuf, "syntax error: expected end-of-line, got '%s': src=%s", tokens[toki].val, linebuf);
	ERRPRINT(errbuf);
	return 0;
}

static int
readline(FILE *fp)
{
	int n, c;

	n = 0;

	alarm( SOCKET_READ_TO );		/* max seconds we will wait -- we cannot gracefully stop if this pops */
	while((c = getc(fp)) >= 0){
		if(n >= linelen){
			linelen += 1024;
			linebuf = ng_realloc(linebuf, linelen, "input line buffer");
		}
		linebuf[n++] = c;
		if(c == '\n'){
			lineno++;
			if((n > 2) && (linebuf[n-2] == '\\'))	/* escaped newline */
				n -= 2;
			else {
				linebuf[n-1] = 0;
				if(linebuf[0] == ERRCHAR)
					goto eof;
				alarm( 0 );			/* cancel */
				return 1;
			}
		}
	}
eof:
	alarm( 0 );		/* cancel */
	if(verbose){
		time_t t;

		time(&t);
		fprintf(stderr, "got EOF; last line was '%s'; received at %s", linebuf? linebuf:"<none>", ctime(&t));
	}
	return 0;	/* EOF */
}

static void
tokenize(void)
{
	int n;
	char *s, *q;

#define	CID(c)	(isalnum(c) || ((c) == '-')  || ((c) == '_') || ((c) == '.') || ((c) == '#') || ((c) == '/') || ((c) == '@'))

	n = 0;
	if(tokbuflen < linelen)
		tokbuf = ng_realloc(tokbuf, tokbuflen = linelen, "token buffer");
	q = tokbuf;
	for(s = linebuf; *s; n++){
		while(isspace(*s))
			s++;
		if(*s == 0)
			break;
		if(n >= atokens){
			atokens += 64;
			tokens = ng_realloc(tokens, atokens*sizeof(tokens[0]), "token buffer");
		}
		tokens[n].val = q;
		if((n > 1) && (tokens[n-1].type == Tok_cmd)){
			/* special case for cmd */
			while(*s)
				*q++ = *s++;
			*q = 0;
			tokens[n-1].val = tokens[n].val;
			n--;
		} else if(CID(*s)){
			for(*q++ = *s++; CID(*s);)
				*q++ = *s++;
			*q++ = 0;
			tokens[n].type = predefined(tokens[n].val);
		} else {
			*q++ = *s++;
			*q++ = 0;
			tokens[n].type = predefined(tokens[n].val);
		}
	}
	tokens[n].type = Tok_EOF;
	if(verbose > 1){
		int i;

		fprintf(stderr, "tokenise returns");
		for(i = 0; i < n; i++)
			fprintf(stderr, " %d='%s'", tokens[i].type, tokens[i].val);
		fprintf(stderr, "\n");
	}
}

Token predef[] = {
	{ Tok_comma, "," },
	{ Tok_colon, ":" },
	{ Tok_after, "after" },
	{ Tok_anystatus, "anystatus" },
	{ Tok_blocked, "blocked" },
	{ Tok_cmd, "cmd" },
	{ Tok_dump, "dump" },
	{ Tok_exit, "exit" },
	{ Tok_expires, "expires" },
	{ Tok_explain, "explain" },
	{ Tok_job, "job" },
	{ Tok_limit, "limit" },
	{ Tok_list, "list" },
	{ Tok_listr, "listr" },
	{ Tok_lists, "lists" },
	{ Tok_pause, "pause" },
	{ Tok_pload, "pload" },
	{ Tok_pmax, "pmax" },
	{ Tok_pmin, "pmin" },
	{ Tok_pri, "pri" },
	{ Tok_purge, "purge" },
	{ Tok_push, "push" },
	{ Tok_ready, "ready" },
	{ Tok_resume, "resume" },
	{ Tok_run, "run" },
	{ Tok_save, "save" },
	{ Tok_sjob, "sjob" },
	{ Tok_status, "status" },
	{ Tok_times, "times" },
	{ Tok_verbose, "verbose" },
	{ 0, 0 }
};

static Token_Type
predefined(char *s)
{
	int i;

	for(i = 0; predef[i].val; i++)
		if(strcmp(s, predef[i].val) == 0)
			return predef[i].type;
	return Tok_unknown;
}

static int
nspec(Node *n, int toki)
{
	for(;;){
		switch(tokens[toki++].type)
		{
		case Tok_expires:
			n->expiry = strtol(tokens[toki++].val, 0, 10);
			break;
		case Tok_status:
			n->status = strtol(tokens[toki++].val, 0, 10);
			break;
		case Tok_ready:
			n->ready = strtol(tokens[toki++].val, 0, 10);
			break;
		default:
			return toki-1;
		}
	}
}

static int
after(Node *n, int toki, FILE *out)
{
	Node *nn;
	char *s;
	Syment *sym;
	int any, i;

	any = 0;
	if(tokens[toki].type == Tok_EOF)
		return toki;
	while((i = nspec(n, toki)) != toki)
		toki = i;
	if(tokens[toki].type != Tok_after)
		return toki;
	toki++;
	if(tokens[toki].type == Tok_anystatus){
		any = 1;
		toki++;
	}
	for(; tokens[toki].type != Tok_EOF; toki++){
		if(tokens[toki].type == Tok_comma){
			toki++;
			for(;;){
				i = nspec(n, toki);
				if(i == toki)
					break;
				toki = i;
			}
			break;
		}
		if(tokens[toki].type != Tok_unknown){
			ERRPREP;
			sprintf(errbuf, "syntax error: expected identifier, got '%s': src=%s", tokens[toki].val, linebuf);
			ERRPRINT(errbuf);
			break;
		}
		s = tokens[toki].val;
		if(isupper(s[0])){
			sym = force_resource(s, out);
			nn = new_node(N_rsrc);
			nn->name = ng_strdup(s);
			nn->rsrc = sym->value;
		} else {
			if((sym = symlook(syms, s, S_JOB, 0, SYM_NOFLAGS)) == 0){
				nn = new_node(N_job);
				nn->name = ng_strdup(s);
				nn->status = J_unknown;
				sym = symlook(syms, nn->name, S_JOB, nn, SYM_NOFLAGS);
			}
			nn = sym->value;
		}
		add_prereq(n, nn, any);
	}
	return toki;
}

static Syment *
force_resource(char *name, FILE *out)
{
	Syment *sym;
	Resource *r;

	if(isupper(name[0])){
		if((sym = symlook(syms, name, S_RESOURCE, 0, SYM_NOFLAGS)) == 0){
			r = new_resource(name);
			sym = symlook(syms, r->name, S_RESOURCE, r, SYM_NOFLAGS);
		}
	} else {
		ERRPREP;
		sprintf(errbuf, "syntax error: expected resource id, got '%s': src=%s", name, linebuf);
		ERRPRINT(errbuf);
		sym = 0;
	}
	return sym;
}
