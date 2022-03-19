/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <float.h>

#include <fr/base.h>

#include "parseevent.h"
#include "evfilter.h"
#include "evpool.h"


//#define EV_WHAT_NONE		0
#define EV_WHAT_NAME		1
#define EV_WHAT_ARG		2
#define EV_WHAT_ATTR		3
#define EV_WHAT_VAR		4

//#define EV_OP_NONE		0
#define EV_OP_EXIST		1
#define EV_OP_NEXIST		2
#define EV_OP_EQ			3
#define EV_OP_NEQ			4
#define EV_OP_GT			5
#define EV_OP_GE			6
#define EV_OP_LT			7
#define EV_OP_LE			8
#define EV_OP_CEQ			9
#define EV_OP_CNEQ		10
#define EV_OP_CGT			11
#define EV_OP_CGE			12
#define EV_OP_CLT			13
#define EV_OP_CLE			14
#define EV_OP_MATCH		15
#define EV_OP_NMATCH		16
#define EV_OP_CMATCH		17
#define EV_OP_CNMATCH	18


struct ev_srul {
	int					what;
	union {
		int				arg;
		const char		*attr;
		struct ev_var	var;
	};
	int					op;
	struct ev_val		val;
	int					hasregex;
	regex_t				regex;
};

static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
#define G_LOCK	do { pthread_mutex_lock (&mutex); } while (0)
#define G_UNLOCK do { pthread_mutex_unlock (&mutex); } while (0)


#define EV_NODE_LEAF		0
#define EV_NODE_LOP		1
#define EV_NODE_NOT		2
#define EV_NODE_DEFAULT	3
#define EV_NODE_PREV		4
#define EV_NODE_LAST		5

struct ev_lop;
struct ev_node {
	int	what;
	union {
		struct ev_lop	*lop;
		struct ev_srul	*srul;
		struct ev_node	*nod;
	};
};

//#define EV_LOP_NONE	0
#define EV_LOP_AND	1
#define EV_LOP_OR		2
#define EV_LOP_XOR	3

struct ev_lop {
	int				lop;
	struct ev_node	left,
						right;
};

#define EV_ACT_NONE		0x00
#define EV_ACT_TARGET	0x10
#define EV_ACT_ACCEPT	0x11
#define EV_ACT_REJECT	0x12
#define EV_ACT_DROP		0x20
#define EV_ACT_GOTO		0x30
#define EV_ACT_GOSUB		0x31
#define EV_ACT_RETURN	0x32
#define EV_ACT_BRANCH	0x33
#define EV_ACT_NAME		0x40
#define EV_ACT_ARG		0x41
#define EV_ACT_UNARG		0x42
#define EV_ACT_ATTR		0x43
#define EV_ACT_UNATTR	0x44
#define EV_ACT_VAR		0x45
#define EV_ACT_UNVAR		0x46
#define EV_ACT_LOG		0x50

#define EV_ACGR_TARGET	0x10
#define EV_ACGR_DROP		0x20
#define EV_ACGR_JUMP		0x30
#define EV_ACGR_SET		0x40
#define EV_ACGR_LOG		0x50
#define EV_ACGR_MASK		0xf0

struct ev_action {
	int						typ;
	union {
		struct {
			char				*target;
			struct tlst		targ;
		};
		char					*label;
		char					*name;
		struct {
			int				argnum;
			struct ev_val	argval;
		};
		struct ev_attr		attr;
		struct ev_var		var;
		struct {
			int				level;
			char				*msg;
		};
	};
	struct ev_action		*next;
};

struct ev_rule {
	struct ev_node		*node;
	struct ev_action	action;
};

struct ev_label {
	char	*name;
	int	lineno;
};

struct ev_filter {
	int			refcnt;
	int			lineno;
	char			*buf;
	int			flags;
	struct tlst	rules;
	struct tlst labels;
};

struct evfilters {
	int					self;
	struct ev_filter	*in, *out, *any;
	int					refcnt, protect;
};


static int sevf_lopfree (struct ev_lop*);
static int sevf_srulfree (struct ev_srul*);
static int sevf_nodfree (struct ev_node*);
static int sevf_actionfree (struct ev_action*);
static int sevf_rulfree (struct ev_rule*);
static int sevf_shiftreduce (int, struct ev_val*, int, int);
static int sevf_getnode (struct ev_node**, int, int);
static int sevf_cleanstack ();
static int sevf_dofree (struct ev_filter*);
static int sevf_free (struct ev_filter*);
static int sevf_init (struct ev_filter*);
static int sevf_addbuf (struct evfilters*, char*, int);
static int sevf_addfile (struct evfilters*, const char*, int);
static int sevf_addfilter (struct evfilters*, const char*, int);
static int sevf_unprotect (struct evfilters*);
static int sevf_protect (struct evfilters*);
static int sevf_reference (struct evfilters*);
static int sevf_release (struct evfilters*);
static int sevf_listrm (int);
static int sevf_insert (struct evfilters*);
static int sevf_find (struct evfilters**, int);
static int parserules (struct ev_filter*, char*, int);
static char *sevf_getline (char**);
static int sevf_apply (struct evlist*, struct evfilters*, struct event*, int);
static int float_almost_eq (double, double);


int
evf_new ()
{
	struct evfilters	*filter;
	int					id;

	filter = malloc (sizeof (struct evfilters));
	if (!filter) return RERR_NOMEM;
	bzero (filter, sizeof (struct evfilters));
	filter->refcnt = 1;
	id = sevf_insert (filter);
	if (id < 0) {
		free (filter);
		return id;
	}
	filter->self = id;
	return id;
}

int
evf_release (id)
	int	id;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_release (filter);
}

int
evf_protect (id)
	int	id;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_protect (filter);
}

int
evf_unprotect (id)
	int	id;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_unprotect (filter);
}

int
evf_reference (id)
	int	id;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_reference (filter);
}

int
evf_addfilter (id, name, flags)
	int			id, flags;
	const char	*name;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_addfilter (filter, name, flags);
}

int
evf_addfile (id, fname, flags)
	int			id, flags;
	const char	*fname;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_addfile (filter, fname, flags);
}

int
evf_addbuf (id, buf, flags)
	int	id, flags;
	char	*buf;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_addbuf (filter, buf, flags);
}

int
evf_addconstbuf (id, buf, flags)
	int			id, flags;
	const char	*buf;
{
	flags &= ~EVF_F_NOCPY;
	flags |= EVF_F_COPY | EVF_F_FREE;

	return evf_addbuf (id, (char*)(void*)buf, flags);
}

int
evf_apply (evlist, id, ev, flags)
	struct evlist	*evlist;
	struct event	*ev;
	int				id, flags;
{
	int					ret;
	struct evfilters	*filter;

	ret = sevf_find (&filter, id);
	if (!RERR_ISOK(ret)) return ret;
	return sevf_apply (evlist, filter, ev, flags);
}

int
evf_evlistfree (evlist, flags)
	struct evlist	*evlist;
	int				flags;
{
	int	ret, ret2 = RERR_OK;

	if (!evlist) return RERR_PARAM;
	if (evlist->next) {
		ret = evf_evlistfree (evlist->next, flags);
		if (!RERR_ISOK(ret)) ret2 = ret;
		free (evlist->next);
	}
	if (evlist->event && (evlist->flags & EVF_F_FREE) && 
										!(flags & EVF_F_NOEVFREE)) {
		ret = evpool_release (evlist->event);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	bzero (evlist, sizeof (struct evlist));
	return ret2;
}


/* ********************
 * apply functions
 * ********************/
static int doapply (struct evlist*, struct ev_filter*, struct event*, int, int, int);
static int copyapply (struct evlist*, struct ev_filter*, struct event*, int, int, int);
static int branch (struct evlist*, struct ev_filter*, struct event*, int, int, int);
static int findlabel (struct tlst*, char*);
static int applylog (struct event*, struct ev_action*, int);
static int applyset (struct event*, struct ev_action*, int);
static int applytarget (struct event*, struct ev_action*, int);
static int applyaccept (struct event*, int);
static int getnewev (struct event**, struct event*, int);
static int checkrule (struct ev_node*, struct event*, int, int, int);
static int checklop (struct ev_lop*, struct event*, int, int, int);
static int checkleaf (struct ev_srul*, struct event*, int);


static
int
sevf_apply (evlist, filter, ev, flags)
	struct evlist		*evlist;
	struct evfilters	*filter;
	struct event		*ev;
	int					flags;
{
	struct ev_filter	*evf;

	if (!filter || !ev) return RERR_PARAM;
	if (!evlist && (flags & EVF_F_COPY)) return RERR_PARAM;
	if (!evlist) flags |= EVF_F_NOBRANCH;
	flags &= ~EVF_F_FREE;
	if ((flags & EVF_F_BOTH) == EVF_F_BOTH) {
		FRLOGF (LOG_ERR, "cannot apply in and out filter, decide what you want");
		return RERR_FORBIDDEN;
	}
	if (flags & EVF_F_IN) {
		evf = filter->in ? filter->in : filter->any;
	} else if (flags & EVF_F_OUT) {
		evf = filter->out ? filter->out : filter->any;
	} else {
		evf = filter->any ? filter->any : filter->in;
	}
	if (!evf && (filter->self != 0)) {
		FRLOGF (LOG_VERBOSE, "empty filter rule");
	}
	return copyapply (evlist, evf, ev, 0, 0, flags);
}

static
int
copyapply (evlist, filter, ev, start, last, flags)
	struct evlist		*evlist;
	struct ev_filter	*filter;
	struct event		*ev;
	int					start, last, flags;
{
	int	ret;

	flags &= ~EVF_F_FREE;
	if (flags & EVF_F_COPY) {
		struct event	*nev;

		if (!evlist) return RERR_PARAM;
		ret = getnewev (&nev, ev, flags);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_copy (nev, ev);
		if (!RERR_ISOK(ret)) {
			evpool_release (nev);
			return ret;
		}
		flags |= EVF_F_FREE;
		ev = nev;
	}
	if (evlist) {
		bzero (evlist, sizeof (struct evlist));
		evlist->event = ev;
		evlist->flags = flags;
	}
	ret = doapply (evlist, filter, ev, start, last, flags);
	if (!RERR_ISOK(ret)) {
		if (evlist) evf_evlistfree (evlist, 0);
		return ret;
	}
	if (evlist) evlist->target = ret;
	return ret;		/* ret is one of EVF_T_... */
}


static
int
getnewev (nev, ev, flags)
	struct event	**nev, *ev;
	int				flags;
{
	int	ret, usepool;

	if (!ev || !nev) return RERR_PARAM;
	if (flags & EVF_F_USEPOOL) {
		usepool = 1;
	} else if (flags & EVF_F_NOPOOL) {
		usepool = 0;
	} else if (evpool_isin (ev) == RERR_OK) {
		usepool = 1;
	} else {
		usepool = 0;
	}
	if (usepool) {
		ret = evpool_acquire (nev);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		*nev = malloc (sizeof (struct event));
		if (!*nev) return RERR_NOMEM;
		ret = ev_new (*nev);
		if (!RERR_ISOK(ret)) {
			free (*nev);
			return ret;
		}
	}
	return RERR_OK;
}

#define APPLYSTACKSZ	256
struct applystackframe {
	int	ret;
	int	prev;
};
struct applystack {
	struct applystackframe	stack[APPLYSTACKSZ];
	int							stackptr;
};

static
int
doapply (evlist, filter, ev, sp, last, flags)
	struct evlist		*evlist;
	struct ev_filter	*filter;
	struct event		*ev;
	int					sp, last, flags;
{
	struct applystack	stack;
	int					ret;
	struct ev_rule		*rul;
	struct ev_action	*act;
	int					prev;

	if (!ev || sp < 0) return RERR_PARAM;
	if (!filter) {
		return applyaccept (ev, flags);
	}
	stack.stackptr=0;
	prev = 0;
	for (; sp<filter->lineno; sp++) {
		ret = TLST_GETPTR (rul, filter->rules, sp);
		if (!RERR_ISOK(ret)) return ret;
		ret = checkrule (rul->node, ev, prev, last, flags);
		if (ret == RERR_FAIL) {
			last = prev = 0;
			continue;
		}
		if (!RERR_ISOK(ret)) return ret;
		/* ok - rule matched - apply action */
		if (rul->node->what != EV_NODE_DEFAULT) prev = last = 1;
		for (act=&(rul->action); act; act=act->next) {
			switch (act->typ & EV_ACGR_MASK) {
			case EV_ACGR_SET:
				ret = applyset (ev, act, flags);
				if (!RERR_ISOK(ret)) return ret;
				break;
			case EV_ACGR_LOG:
				ret = applylog (ev, act, flags);
				if (!RERR_ISOK(ret)) return ret;
				break;
			case EV_ACGR_DROP:
				return EVF_T_DROP;
			case EV_ACGR_TARGET:
				return applytarget (ev, act, flags);
			case EV_ACGR_JUMP:
				switch (act->typ) {
				case EV_ACT_GOSUB:
					if (stack.stackptr >= APPLYSTACKSZ) return RERR_STACK_OVERFLOW;
					stack.stack[stack.stackptr].ret = sp;
					stack.stack[stack.stackptr].prev = prev;
					stack.stackptr++;
					/* fall thru */
				case EV_ACT_GOTO:
					prev = 0;
					ret = findlabel (&(filter->labels), act->label);
					if (!RERR_ISOK(ret)) return ret;
					sp = ret-1;		/* will be incremented by loop */
					break;
				case EV_ACT_RETURN:
					if (stack.stackptr == 0) {
						FRLOGF (LOG_DEBUG, "return statement on empty stack is "
													"ignored");
						break;
					}
					stack.stackptr--;
					sp = stack.stack[stack.stackptr].ret;
					prev = stack.stack[stack.stackptr].prev;
					break;
				case EV_ACT_BRANCH:
					ret = findlabel (&(filter->labels), act->label);
					if (!RERR_ISOK(ret)) return ret;
					ret = branch (evlist, filter, ev, ret, last, flags);
					if (!RERR_ISOK(ret)) return ret;
					break;
				default:
					FRLOGF (LOG_ERR, "invalid action (%d)", act->typ);
					return RERR_NOT_SUPPORTED;
				}
				goto contloop;
				break;
			default:
				FRLOGF (LOG_ERR, "invalid action (%d)", act->typ);
				return RERR_NOT_SUPPORTED;
			}
		}
contloop:
		while (0);	/* noop */
	}
	/* when we do reach here, no target has been found, so the default 
	 * target "accept" does apply 
	 */
	return EVF_T_ACCEPT;
}


static
int
checkrule (nod, ev, prev, last, flags)
	struct ev_node	*nod;
	struct event	*ev;
	int				prev, last, flags;
{
	int	ret;

	if (!ev) return RERR_PARAM;
	if (!nod) return RERR_FAIL;
	switch (nod->what) {
	case EV_NODE_DEFAULT:
		return RERR_OK;
	case EV_NODE_PREV:
		return prev ? RERR_OK : RERR_FAIL;
	case EV_NODE_LAST:
		return last ? RERR_OK : RERR_FAIL;
	case EV_NODE_NOT:
		ret = checkrule (nod->nod, ev, prev, last, flags);
		if (ret == RERR_FAIL) return RERR_OK;
		if (!RERR_ISOK(ret)) return ret;
		return RERR_FAIL;
	case EV_NODE_LOP:
		return checklop (nod->lop, ev, prev, last, flags);
	case EV_NODE_LEAF:
		return checkleaf (nod->srul, ev, flags);
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;	/* should never reach here */
}

static
int
checklop (lop, ev, prev, last, flags)
	struct ev_lop	*lop;
	struct event	*ev;
	int				prev, last, flags;
{
	int	ret, ret2;

	if (!ev || !lop) return RERR_PARAM;
	ret = checkrule (&(lop->left), ev, prev, last, flags);
	if (!RERR_ISOK(ret) && ret != RERR_FAIL) return ret;
	switch (lop->lop) {
	case EV_LOP_AND:
		if (ret == RERR_FAIL) return ret;
		return checkrule (&(lop->right), ev, prev, last, flags);
	case EV_LOP_OR:
		if (ret != RERR_FAIL) return ret;
		return checkrule (&(lop->right), ev, prev, last, flags);
	case EV_LOP_XOR:
		ret2 = checkrule (&(lop->right), ev, prev, last, flags);
		if (!RERR_ISOK(ret2) && ret2 != RERR_FAIL) return ret;
		if (ret == RERR_FAIL && ret2 != RERR_FAIL) return RERR_OK;
		if (ret != RERR_FAIL && ret2 == RERR_FAIL) return RERR_OK;
		return RERR_FAIL;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;	/* should never reach here */
}

static
int
checkleaf (srul, ev, flags)
	struct ev_srul	*srul;
	struct event	*ev;
	int				flags;
{
	struct ev_val	eval, sval;
	int				exist, ret;

	if (!ev || !srul) return RERR_PARAM;
	switch (srul->what) {
	case EV_WHAT_NAME:
		eval.typ = EVP_T_STR;
		ret = ev_getname ((const char**)&(eval.s), ev);
		if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) return ret;
		exist = (ret == RERR_NOT_FOUND) ? 0 : 1;
		break;
	case EV_WHAT_ARG:
		ret = ev_getarg_v (&eval, ev, srul->arg);
		if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) return ret;
		exist = (ret == RERR_NOT_FOUND) ? 0 : 1;
		if (exist) {
			switch (eval.typ) {
			case EVP_T_VOID:
				exist = 0;
				break;
			case EVP_T_STR:
				/* empty arguments are treated as non existent */
				if (!eval.s || !*(eval.s)) exist = 0;
				break;
			}
		}
		break;
	case EV_WHAT_ATTR:
		ret = ev_getattr_v (&eval, ev, srul->attr);
		if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) return ret;
		exist = (ret == RERR_NOT_FOUND) ? 0 : 1;
		break;
	case EV_WHAT_VAR:
		ret = ev_getvar_v (	&eval, ev, srul->var.var, srul->var.idx1,
									srul->var.idx2);
		if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) return ret;
		exist = (ret == RERR_NOT_FOUND) ? 0 : 1;
		break;
	default:
		return RERR_INTERNAL;	/* should not happen */
	}
	/* check for existance */
	if (srul->op == EV_OP_EXIST) {
		return exist ? RERR_OK : RERR_FAIL;
	} else if (srul->op == EV_OP_NEXIST) {
		return (!exist) ? RERR_OK : RERR_FAIL;
	} else if (!exist) {
		return RERR_FAIL;
	}
	/* check for type */
	sval = srul->val;
	if (sval.typ != eval.typ) return RERR_FAIL;
	/* check ops by type */
	switch (eval.typ) {
	case EVP_T_INT:
		switch (srul->op) {
		case EV_OP_EQ:
		case EV_OP_CEQ:
			return eval.i == sval.i ? RERR_OK : RERR_FAIL;
		case EV_OP_NEQ:
		case EV_OP_CNEQ:
			return eval.i != sval.i ? RERR_OK : RERR_FAIL;
		case EV_OP_GT:
		case EV_OP_CGT:
			return eval.i > sval.i ? RERR_OK : RERR_FAIL;
		case EV_OP_GE:
		case EV_OP_CGE:
			return eval.i >= sval.i ? RERR_OK : RERR_FAIL;
		case EV_OP_LT:
		case EV_OP_CLT:
			return eval.i < sval.i ? RERR_OK : RERR_FAIL;
		case EV_OP_LE:
		case EV_OP_CLE:
			return eval.i <= sval.i ? RERR_OK : RERR_FAIL;
		default:
			return RERR_NOT_SUPPORTED;
		}
		break;
	case EVP_T_FLOAT:
		switch (srul->op) {
		case EV_OP_EQ:
		case EV_OP_CEQ:
			return float_almost_eq (eval.f, sval.f) ? RERR_OK : RERR_FAIL;
		case EV_OP_NEQ:
		case EV_OP_CNEQ:
			return float_almost_eq (eval.f, sval.f) ? RERR_FAIL : RERR_OK;
		case EV_OP_GT:
		case EV_OP_CGT:
			return eval.f > sval.f ? RERR_OK : RERR_FAIL;
		case EV_OP_GE:
		case EV_OP_CGE:
			return eval.f >= sval.f ? RERR_OK : RERR_FAIL;
		case EV_OP_LT:
		case EV_OP_CLT:
			return eval.f < sval.f ? RERR_OK : RERR_FAIL;
		case EV_OP_LE:
		case EV_OP_CLE:
			return eval.f <= sval.f ? RERR_OK : RERR_FAIL;
		default:
			return RERR_NOT_SUPPORTED;
		}
		break;
	case EVP_T_STR:
		if (!eval.s) return RERR_FAIL;
		switch (srul->op) {
		case EV_OP_EQ:
			return strcasecmp (eval.s, sval.s) == 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_NEQ:
			return strcasecmp (eval.s, sval.s) != 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_GT:
			return strcasecmp (eval.s, sval.s) > 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_GE:
			return strcasecmp (eval.s, sval.s) >= 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_LT:
			return strcasecmp (eval.s, sval.s) < 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_LE:
			return strcasecmp (eval.s, sval.s) <= 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CEQ:
			return strcmp (eval.s, sval.s) == 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CNEQ:
			return strcmp (eval.s, sval.s) != 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CGT:
			return strcmp (eval.s, sval.s) > 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CGE:
			return strcmp (eval.s, sval.s) >= 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CLT:
			return strcmp (eval.s, sval.s) < 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_CLE:
			return strcmp (eval.s, sval.s) <= 0 ? RERR_OK : RERR_FAIL;
		case EV_OP_MATCH:
		case EV_OP_CMATCH:
			if (!srul->hasregex) return RERR_INTERNAL;
			ret = regexec (&(srul->regex), eval.s, 0, NULL, 0);
			if (ret == 0) return RERR_OK;
			if (ret == REG_NOMATCH) return RERR_FAIL;
			return RERR_SYSTEM;
		case EV_OP_NMATCH:
		case EV_OP_CNMATCH:
			if (!srul->hasregex) return RERR_INTERNAL;
			ret = regexec (&(srul->regex), eval.s, 0, NULL, 0);
			if (ret == 0) return RERR_FAIL;
			if (ret == REG_NOMATCH) return RERR_OK;
			return RERR_SYSTEM;
		default:
			return RERR_NOT_SUPPORTED;
		}
		break;
	default:
		return RERR_INTERNAL;
	}
}


static
int
findlabel (labellist, label)
	struct tlst	*labellist;
	char			*label;
{
	int					ret;
	struct ev_label	*ptr;

	if (!labellist || !label) return RERR_PARAM;
	ret = TLST_SEARCH (*labellist, label, tlst_cmpistr);
	if (ret == RERR_NOT_FOUND) {
		FRLOGF (LOG_DEBUG, "jump label >>%s<< does not exist", label);
	}
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (ptr, *labellist, ret);
	if (!RERR_ISOK(ret)) return ret;
	return ptr->lineno;
}

static
int
applytarget (ev, act, flags)
	struct event		*ev;
	struct ev_action	*act;
	int					flags;
{
	int		eflags=0;
	int		ret, tret;
	unsigned	i;
	char		*name, *arg;

	if (!ev || !act) return RERR_PARAM;
	if (!(flags & EVF_F_NOCPY)) {
		eflags |= EVP_F_CPY;
	}
	switch (act->typ) {
	case EV_ACT_ACCEPT:
		tret = EVF_T_ACCEPT;
		name = (char*)"ACCEPT";
		break;
	case EV_ACT_REJECT:
		tret = EVF_T_REJECT;
		name = (char*)"REJECT";
		break;
	case EV_ACT_DROP:
		return EVF_T_DROP;
	case EV_ACT_TARGET:
		tret = EVF_T_ACCEPT;
		name = act->target;
		if (!name || !*name) return RERR_INVALID_NAME;
		break;
	default:
		FRLOGF (LOG_ERR, "invalid action (%d)", act->typ);
		return RERR_NOT_SUPPORTED;
	}
	if ((flags & EVF_F_OUTTARGET) && (tret == EVF_T_ACCEPT)) {
		if (act->typ == EV_ACT_TARGET) {
			ret = ev_setname (ev, name, eflags);
			if (!RERR_ISOK(ret)) return ret;
		}
		return tret;
	}
	TLST_FOREACH2 (arg, act->targ, i) {
		if (!arg) arg=(char*)"";
		ret = ev_settarg_s (ev, arg, i, eflags);
		if (!RERR_ISOK(ret)) return ret;
	}
	ret = ev_settname (ev, name, eflags);
	if (!RERR_ISOK(ret)) return ret;
	return tret;
}

static
int
applyaccept (ev, flags)
	struct event	*ev;
	int				flags;
{
	int	ret;

	if (!ev) return RERR_PARAM;
	if (!(flags & EVF_F_OUTTARGET)) {
		ret = ev_settname (ev, "ACCEPT", 0);
		if (!RERR_ISOK(ret)) return ret;
	}
	return EVF_T_ACCEPT;
}

static
int
applyset (ev, act, flags)
	struct event		*ev;
	struct ev_action	*act;
	int					flags;
{
	int	eflags=0;
	int	ret;

	if (!ev || !act) return RERR_PARAM;
	if (!(flags & EVF_F_NOCPY)) {
		eflags |= EVP_F_CPY;
	}
	switch (act->typ) {
	case EV_ACT_NAME:
		ret = ev_setname (ev, act->name, eflags);
		break;
	case EV_ACT_ARG:
		ret = ev_setarg_v (ev, act->argval, act->argnum, eflags);
		break;
	case EV_ACT_UNARG:
		ret = ev_rmarg (ev, act->argnum);
		break;
	case EV_ACT_ATTR:
		ret = ev_addattr_v (ev, act->attr.var, act->attr.val, eflags);
		break;
	case EV_ACT_UNATTR:
		ret = ev_rmattr (ev, act->attr.var);
		break;
	case EV_ACT_VAR:
		ret = ev_addvar_v (	ev, act->var.var, act->var.idx1, act->var.idx2,
									act->var.val, eflags);
		break;
	case EV_ACT_UNVAR:
		ret = ev_rmvar (ev, act->var.var, act->var.idx1, act->var.idx2);
		break;
	default:
		FRLOGF (LOG_ERR, "invalid action (%d)", act->typ);
		return RERR_NOT_SUPPORTED;
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_DEBUG, "error setting data: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
applylog (ev, act, flags)
	struct event		*ev;
	struct ev_action	*act;
	int					flags;
{
	if (!ev || !act) return RERR_PARAM;
	if (act->typ != EV_ACT_LOG) return RERR_NOT_SUPPORTED;
	if (!(flags & EVF_F_QUIET)) {
		SMLOGF ("evfilter", act->level, "%s", act->msg);
	}
	return RERR_OK;
}


static
int
branch (evlist, filter, ev, start, last, flags)
	struct evlist		*evlist;
	struct ev_filter	*filter;
	struct event		*ev;
	int					flags, last, start;
{
	if (flags & EVF_F_NOBRANCH) return RERR_OK;
	if (!evlist) return RERR_PARAM;
	while (evlist->next) evlist=evlist->next;
	evlist->next = malloc (sizeof (struct evlist));
	if (!evlist->next) return RERR_NOMEM;
	bzero (evlist->next, sizeof (struct evlist));
	/* note: cleanup will be done by upper copyapply() */
	return copyapply (evlist->next, filter, ev, start, last, flags | EVF_F_COPY);
}


/* ***************************
 * id management
 * ***************************/

static struct tlst	evflist;
static int				evflist_isinit=0;
static int sevf_evflistinit ();

static
int
sevf_find (ofilter, id)
	struct evfilters	**ofilter;
	int					id;
{
	int					ret;
	struct evfilters	*ptr;

	if (!evflist_isinit) {
		ret = sevf_evflistinit ();
		if (!RERR_ISOK(ret)) return ret;
	}
	G_LOCK;
	ret = TLST_GET (ptr, evflist, id);
	G_UNLOCK;
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_NOT_FOUND;
	if (ofilter) *ofilter = ptr;
	return RERR_OK;
}


static
int
sevf_insert (filter)
	struct evfilters	*filter;
{
	int					ret;

	if (!filter) return RERR_PARAM;
	if (!evflist_isinit) {
		ret = sevf_evflistinit ();
		if (!RERR_ISOK(ret)) return ret;
	}
	G_LOCK;
	ret = TLST_ADDINSERT (evflist, filter);
	G_UNLOCK;
	return ret;
}

static
int
sevf_evflistinit ()
{
	struct evfilters	*zero = NULL;
	int					ret;

	if (evflist_isinit) return RERR_OK;
	G_LOCK;
	if (evflist_isinit) {
		G_UNLOCK;
		return RERR_OK;
	}
	zero = malloc (sizeof(struct evfilters));
	if (!zero) {
		ret = RERR_NOMEM;
		goto out;
	}
	bzero (zero, sizeof (struct evfilters));
	zero->protect = 1;
	zero->self = 0;
	ret = TLST_NEW (evflist, struct evfilters*);
	if (!RERR_ISOK(ret))  goto out;
	ret = TLST_SET (evflist, 0, zero);
	if (!RERR_ISOK(ret)) {
		TLST_FREE (evflist);
		goto out;
	}
	evflist.num = 1;
	evflist_isinit = 1;
out:
	G_UNLOCK;
	if (!RERR_ISOK(ret)) {
		if (zero) free (zero);
		FRLOGF (LOG_ERR, "error initializing filter list: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
sevf_listrm (id)
	int	id;
{
	int	ret;

	if (!evflist_isinit) return RERR_OK;
	G_LOCK;
	ret = TLST_REMOVE (evflist, id, 0);
	G_UNLOCK;
	return ret;
}


/* ****************************
 * main parser functions
 * ****************************/

static
int
sevf_release (filter)
	struct evfilters	*filter;
{
	int	ret;

	if (!filter) return RERR_PARAM;
	if (filter->refcnt <= 0) return RERR_OK;
	filter->refcnt--;
	if (filter->refcnt == 0 && !filter->protect) {
		/* unlink filter */
		ret = sevf_listrm (filter->self);
		if (!RERR_ISOK(ret)) {
			filter->refcnt++;
			return ret;
		}
		/* free the filter - ignore errors */
		if (filter->in) sevf_free (filter->in);
		if (filter->out) sevf_free (filter->out);
		if (filter->any) sevf_free (filter->any);
		bzero (filter, sizeof (struct evfilters));
	}
	return RERR_OK;
}

static
int
sevf_reference (filter)
	struct evfilters	*filter;
{
	if (!filter) return RERR_PARAM;
	filter->refcnt++;
	return RERR_OK;
}

static
int
sevf_protect (filter)
	struct evfilters	*filter;
{
	if (!filter) return RERR_PARAM;
	filter->protect = 1;
	return RERR_OK;
}

static
int
sevf_unprotect (filter)
	struct evfilters	*filter;
{
	if (!filter) return RERR_PARAM;
	if (filter->self == 0) return RERR_FORBIDDEN;
	filter->protect = 0;
	return RERR_OK;
}

static
int
sevf_addfilter (filter, id, flags)
	struct evfilters	*filter;
	const char			*id;
	int					flags;
{
	const char	*in, *inrules, *out, *outrules, *any, *anyrules;
	int			ret, pflags, ret2 = RERR_OK;
	int			hasany = 0;

	if (!filter) return RERR_PARAM;
	if (filter->protect) return RERR_FORBIDDEN;
	cf_begin_read ();
	if (id && *id) {
		in = cf_getvarf ("evfilter[%s]/in", id);
		inrules = cf_getvarf ("evfilter[%s]/inrules", id);
		out = cf_getvarf ("evfilter[%s]/out", id);
		outrules = cf_getvarf ("evfilter[%s]/outrules", id);
		any = cf_getvarf ("evfilter[%s]/any", id);
		anyrules = cf_getvarf ("evfilter[%s]/anyrules", id);
	} else {
		in = cf_getval ("evfilter/in");
		inrules = cf_getval ("evfilter/inrules");
		out = cf_getval ("evfilter/out");
		outrules = cf_getval ("evfilter/outrules");
		any = cf_getval ("evfilter/any");
		anyrules = cf_getval ("evfilter/anyrules");
	}
	cf_end_read ();
	pflags = (flags & EVF_F_NOCPY) ? flags | EVF_F_COPY | EVF_F_FREE : \
												flags & ~EVF_F_FREE;
	if (flags & EVF_F_IN) {
		ret = RERR_OK;
		if (in) {
			ret = sevf_addfile (filter, in, flags & ~EVF_F_OUT);
		} else if (inrules) {
			ret = sevf_addbuf (filter, (char*)inrules, (pflags | EVF_F_COPY) & ~EVF_F_OUT);
		} else if (any) {
			ret = sevf_addfile (filter, any, flags & ~EVF_F_OUT);
			hasany = 1;
		} else if (anyrules) {
			ret = sevf_addbuf (filter, (char*)anyrules, (pflags | EVF_F_COPY) & ~EVF_F_OUT);
			hasany = 1;
		}
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error adding in-filter (%s): %s", (id?id:""),
										rerr_getstr3 (ret));
			}
			ret2 = ret;
			hasany = 0;
		}
	}
	if (flags & EVF_F_OUT) {
		ret = RERR_OK;
		if (out) {
			ret = sevf_addfile (filter, out, flags & ~EVF_F_IN);
		} else if (outrules) {
			ret = sevf_addbuf (filter, (char*)outrules, (pflags | EVF_F_COPY) & ~EVF_F_IN);
		} else if (hasany && filter->in) {
			filter->out = filter->in;
			filter->out->refcnt++;
			ret = RERR_OK;
		} else if (any) {
			ret = sevf_addfile (filter, any, flags & ~EVF_F_IN);
		} else if (anyrules) {
			ret = sevf_addbuf (filter, (char*)anyrules, (pflags | EVF_F_COPY) & ~EVF_F_IN);
		}
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error adding out-filter (%s): %s", (id?id:""),
										rerr_getstr3 (ret));
			}
			ret2 = ret;
		}
	}
	if (!(flags & (EVF_F_IN | EVF_F_OUT))) {
		ret = RERR_OK;
		if (in) {
			ret = sevf_addfile (filter, in, flags | EVF_F_IN);
		} else if (inrules) {
			ret = sevf_addbuf (filter, (char*)inrules, pflags | EVF_F_IN | EVF_F_COPY);
		}
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error adding in-filter (%s): %s", (id?id:""),
										rerr_getstr3 (ret));
			}
			ret2 = ret;
			ret = RERR_OK;
		}
		if (out) {
			ret = sevf_addfile (filter, out, flags | EVF_F_OUT);
		} else if (outrules) {
			ret = sevf_addbuf (filter, (char*)outrules, pflags | EVF_F_OUT | EVF_F_COPY);
		}
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error adding out-filter (%s): %s", (id?id:""),
										rerr_getstr3 (ret));
			}
			ret2 = ret;
			ret = RERR_OK;
		}
		if (any) {
			ret = sevf_addfile (filter, any, flags);
		} else if (anyrules) {
			ret = sevf_addbuf (filter, (char*)anyrules, pflags | EVF_F_COPY);
		}
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error adding any-filter (%s): %s", (id?id:""),
										rerr_getstr3 (ret));
			}
			ret2 = ret;
			ret = RERR_OK;
		}
	}
	if (!RERR_ISOK(ret2)) return ret2;
	return RERR_OK;
}

static
int
sevf_addfile (filter, fname, flags)
	struct evfilters	*filter;
	const char			*fname;
	int					flags;
{
	int	ret;
	char	*buf;

	if (!filter || !fname) return RERR_PARAM;
	if (filter->protect) return RERR_FORBIDDEN;
	if (!*fname) return RERR_OK;
	buf = fop_read_fn (fname);
	if (!buf) {
		FRLOGF (LOG_ERR, "error reading file >>%s<<: %s", fname, 
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	flags &= ~EVF_F_COPY;
	flags |= EVF_F_FREE;
	ret = sevf_addbuf (filter, buf, flags);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	return RERR_OK;
}

static
int
sevf_addbuf (filter, buf, flags)
	struct evfilters	*filter;
	char					*buf;
	int					flags;
{
	struct ev_filter	*xfilter;
	int					ret;

	if (!filter) return RERR_PARAM;
	if (filter->protect) return RERR_FORBIDDEN;
	if (buf) {
		xfilter = malloc (sizeof (struct ev_filter));
		if (!xfilter) return RERR_NOMEM;
		ret = sevf_init (xfilter);
		if (!RERR_ISOK(ret)) {
			free (xfilter);
			return ret;
		}
		if (flags & EVF_F_COPY) {
			buf = strdup (buf);
			if (!buf) {
				sevf_free (xfilter);
				return RERR_NOMEM;
			}
		}
		ret = parserules (xfilter, buf, flags);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				FRLOGF (LOG_ERR, "error parsing rules: %s", rerr_getstr3(ret));
			} else {
				FRLOGF (LOG_DEBUG, "failed parsing rules");
			}
			sevf_free (xfilter);
			if (flags & EVF_F_COPY) free (buf);
			return ret;
		}
	} else {
		xfilter = NULL;
	}
	if (flags & EVF_F_IN) {
		if (filter->in) sevf_free (filter->in);
		filter->in = xfilter;
		if (xfilter) xfilter->refcnt++;
	}
	if (flags & EVF_F_OUT) {
		if (filter->out) sevf_free (filter->out);
		filter->out = xfilter;
		if (xfilter) xfilter->refcnt++;
	}
	if (!(flags & (EVF_F_IN | EVF_F_OUT))) {
		if (filter->any) sevf_free (filter->any);
		filter->any = xfilter;
		if (xfilter) xfilter->refcnt++;
	}
	return RERR_OK;
}


static
int
sevf_init (filter)
	struct ev_filter	*filter;
{
	if (!filter) return RERR_PARAM;
	bzero (filter, sizeof (struct ev_filter));
	TLST_NEW (filter->rules, struct ev_rule);
	TLST_NEW (filter->labels, struct ev_label);
	return RERR_OK;
}


static
int
sevf_free (filter)
	struct ev_filter	*filter;
{
	int	ret;

	if (!filter) return RERR_PARAM;
	filter->refcnt--;
	if (filter->refcnt <= 0) {
		ret = sevf_dofree (filter);
		free (filter);
	}
	return ret;
}


static
int
sevf_dofree (filter)
	struct ev_filter	*filter;
{
	unsigned			i;
	struct ev_rule	*ptr;

	if (!filter) return RERR_PARAM;
	TLST_FOREACHPTR2 (ptr, filter->rules, i) {
		sevf_rulfree (ptr);
	}
	if ((filter->flags & EVF_F_FREE) && filter->buf) {
		free (filter->buf);
	}
	TLST_FREE (filter->labels);
	TLST_FREE (filter->rules);
	bzero (filter, sizeof (struct ev_filter));
	return RERR_OK;
}



/* ********************
 * parser
 * ********************/
static int parserules2 (struct ev_filter*, char*, int);
static int sevf_parsecondition (char**, struct ev_node**, int, int);
static int sevf_gettok (char**, struct ev_val*);
static int sevf_parselabel (char*, struct ev_label*, int, int, int);
static int parseset (char*, struct ev_action*, int, int);
static int parsejump (char*, struct ev_action*, int, int);
static int parselog (char*, struct ev_action*, int, int);
static int parsetarget (char*, struct ev_action*, int, int);
static int parseaction (char*, struct ev_action*, int, int);
static int sevf_parseline (char*, struct ev_rule*, int, int);
static int sevf_parseaction (char**, struct ev_action*, int, int);

static
int
parserules (filter, buf, flags)
	struct ev_filter	*filter;
	char					*buf;
	int					flags;
{
	int	ret;

	ret = parserules2 (filter, buf, flags);
	if (ret == RERR_FAIL) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "failed parsing filter rules");
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Failed parsing filter rules\n");
		}
		return RERR_FAIL;
	} else if (!RERR_ISOK(ret)) {
		sevf_dofree (filter);
		FRLOGF (LOG_ERR, "error parsing filter rules: %s", 
						rerr_getstr3 (ret));
		return ret;
	}
	filter->buf = buf;
	filter->flags = flags;
	return RERR_OK;
}


static
int
parserules2 (filter, buf, flags)
	struct ev_filter	*filter;
	char					*buf;
	int					flags;
{
	char					*line;
	int					lineno, plineno=0, ret;
	struct ev_rule		rule;
	struct ev_label	label;

	if (!buf || !filter) return RERR_PARAM;
	lineno = filter->lineno;
	/* parse buffer */
	while (1) {
		line = sevf_getline (&buf);
		if (!line) break;
		line = top_skipwhite (line);
		if (!line || !*line) continue;
		plineno++;
		if (*line == '@') {
			ret = sevf_parselabel (line, &label, flags, lineno, plineno);
			if (ret == RERR_FAIL && !(flags & EVF_F_STRICT)) continue;
			if (!RERR_ISOK(ret)) return ret;
			ret = TLST_INSERT (filter->labels, label, tlst_cmpistr);
			if (!RERR_ISOK(ret)) return ret;
			continue;
		} 
		ret = sevf_parseline (line, &rule, flags, plineno);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_ADD (filter->rules, rule);
		if (!RERR_ISOK(ret)) return ret;
		filter->lineno = ++lineno;
	}
	sevf_cleanstack ();
	return RERR_OK;
}

static
char *
sevf_getline (next)
	char	**next;
{
	return top_getline2 (next, TOP_F_LINESPLICE, "//");
}

static
int
sevf_parseline (line, rule, flags, lineno)
	char					*line;
	struct ev_rule		*rule;
	int					flags, lineno;
{
	int	ret;

	if (!line || !rule) return RERR_PARAM;
	bzero (rule, sizeof (struct ev_rule));
	ret = sevf_parsecondition (&line, &(rule->node), flags, lineno);
	if (!RERR_ISOK(ret)) return ret;
	ret = sevf_parseaction (&line, &(rule->action), flags, lineno);
	if (!RERR_ISOK(ret)) {
		sevf_nodfree (rule->node);
		bzero (rule, sizeof (struct ev_rule));
		return ret;
	}
	return RERR_OK;
}

static
int
sevf_parseaction (ptr, action, flags, line)
	char					**ptr;
	struct ev_action	*action;
	int					flags, line;
{
	char	*str;
	int	ret, first=1;

	if (!ptr || !*ptr || !action) return RERR_PARAM;
	while ((str = top_getfield (ptr, ";", TOP_F_DQUOTE|TOP_F_NOSKIPCOMMENT))) {
		if (first) {
			first = 0;
			ret = parseaction (str, action, flags, line);
			if (!RERR_ISOK(ret)) return ret;
		} else {
			action->next = malloc (sizeof (struct ev_action));
			if (!action->next) return RERR_NOMEM;
			ret = parseaction (str, action->next, flags, line);
			if (!RERR_ISOK(ret)) {
				free (action->next);
				action->next = NULL;
				return ret;
			}
			action = action->next;
		}
	}
	return RERR_OK;
}

static
int
parseaction (str, action, flags, line)
	char					*str;
	struct ev_action	*action;
	int					flags, line;
{
	bzero (action, sizeof (struct ev_action));
	sswitch (str) {
	sncase ("#")
		str++;
		/* fall thru */
	sincase ("accept")
	sincase ("reject")
		return parsetarget (str, action, flags, line);
		break;
	sincase ("drop")
		action->typ = EV_ACT_DROP;
		break;
	sincase ("name")
	sincase ("arg")
	sincase ("attr")
	sincase ("var")
		return parseset (str, action, flags, line);
		break;
	sincase ("debug")
	sincase ("info")
	sincase ("notice")
	sincase ("warn")
	sincase ("error")
	sincase ("alert")
	sincase ("panic")
		return parselog (str, action, flags, line);
		break;
	sincase ("goto")
	sincase ("gosub")
	sincase ("return")
	sincase ("branch")
		return parsejump (str, action, flags, line);
	sdefault
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "invalid action >>%s<< in line %d", str, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Invalid action >>%s<< in line %d\n", str, line);
		}
		return RERR_FAIL;
	} esac;
	return RERR_OK;
}

static
int
parsetarget (str, action, flags, line)
	char					*str;
	struct ev_action	*action;
	int					flags, line;
{
	char	*arg;
	int	first=1, ret;

	if (!str || !action) return RERR_PARAM;
	str = top_skipwhiteplus (str, "#");
	ret = TLST_NEW (action->targ, char*);
	if (!RERR_ISOK(ret)) return ret;
	while ((arg = top_getfield (&str, ":", TOP_F_DQUOTE|TOP_F_NOSKIPBLANK))) {
		if (first) {
			sswitch (arg) {
			sicase ("accept")
				action->typ = EV_ACT_ACCEPT;
				action->target = (char*)"ACCEPT";
				break;
			sicase ("reject")
				action->typ = EV_ACT_REJECT;
				action->target = (char*)"REJECT";
				break;
			sicase ("drop")
				action->typ = EV_ACT_DROP;
				TLST_FREE (action->targ);
				return RERR_OK;
			scase ("")
				TLST_FREE (action->targ);
				if (!(flags & EVF_F_QUIET)) {
					FRLOGF (LOG_DEBUG, "empty target in line %d", line);
				}
				if (flags & EVF_F_VERBOSE) {
					fprintf (stderr, "Empty target in line %d\n", line);
				}
				return RERR_FAIL;
			sdefault
				action->typ = EV_ACT_TARGET;
				action->target = arg;
				break;
			} esac;
		} else {
			ret = TLST_ADD (action->targ, arg);
			if (!RERR_ISOK(ret)) {
				TLST_FREE (action->targ);
				return ret;
			}
		}
	}
	return RERR_OK;
}

static
int
parselog (str, action, flags, line)
	char					*str;
	struct ev_action	*action;
	int					flags, line;
{
	char	*level, *msg, *next;

	if (!str || !action) return RERR_PARAM;
	level = top_getfield (&str, "(", TOP_F_NOSKIPBLANK);
	msg = top_getfield (&str, ")", TOP_F_NOSKIPBLANK | TOP_F_DQUOTE);
	if (!level || !*level) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "missing log function in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Missing log function in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (!msg) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "missing log message in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Missing log message in line %d\n", line);
		}
		return RERR_FAIL;
	}
	next = msg;
	msg = top_getquotedfield (&next, "", 0);
	if (!msg) msg=(char*)"";
	action->typ = EV_ACT_LOG;
	action->msg = msg;
	sswitch (level) {
	sicase ("debug")
		action->level = LOG_DEBUG;
		break;
	sicase ("info")
		action->level = LOG_INFO;
		break;
	sicase ("notice")
	sicase ("note")
		action->level = LOG_NOTICE;
		break;
	sincase ("warn")
		action->level = LOG_WARN;
		break;
	sicase ("error")
		action->level = LOG_ERR;
		break;
	sicase ("alert")
		action->level = LOG_CRIT;
		break;
	sicase ("panic")
		action->level = LOG_PANIC;
		break;
	sdefault
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "unknown log function >>%s<< in line %d", 
										str, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Unknown log function >>%s<< in line %d\n", 
										str, line);
		}
		return RERR_FAIL;
	} esac;
	return RERR_OK;
}

static
int
parsejump (str, action, flags, line)
	char					*str;
	struct ev_action	*action;
	int					flags, line;
{
	char	*jump, *label;

	if (!str || !action) return RERR_PARAM;
	jump = top_getfield (&str, " \t", 0);
	label = top_getfield (&str, " \t", 0);
	if (!jump) return RERR_INTERNAL;
	sswitch (jump) {
	sicase ("goto")
		action->typ = EV_ACT_GOTO;
		break;
	sicase ("gosub")
		action->typ = EV_ACT_GOSUB;
		break;
	sicase ("return")
		action->typ = EV_ACT_RETURN;
		return RERR_OK;
	sicase ("branch")
		action->typ = EV_ACT_BRANCH;
		break;
	sdefault
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "unknown jump action >>%s<< in line %d", str, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Unknown jump action >>%s<< in line %d\n", str, line);
		}
		return RERR_FAIL;
	} esac;
	if (!label) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "missing label in jump action in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Missing label in jump action in line %d\n", line);
		}
		return RERR_FAIL;
	}
	action->label = label;
	return RERR_OK;
}


static
int
parseset (str, action, flags, line)
	char					*str;
	struct ev_action	*action;
	int					flags, line;
{
	char				*ref, *val, *s;
	int				unset=0;
	int				isvar=1;
	struct ev_val	eval;

	if (!str || !action) return RERR_OK;
	ref = top_getfield (&str, "=", TOP_F_NOSKIPBLANK);
	val = top_stripwhite (str, 0);
	if (!ref) return RERR_FAIL;
	if (!val || !*val) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "missing value in set action in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Missing value in set action in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (!strcasecmp (ref, "name")) {
		if (!strcmp (val, "!")) {
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "cannot unset event name in line %d", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Cannot unset event name in line %d\n", line);
			}
			return RERR_FAIL;
		}
		val = top_skipwhiteplus (val, "\"");
		for (s=val; isalnum(*s) || *s=='_' || *s=='/'; s++);
		if (s) *s=0;
		if (!*val) {
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "missing value in set action in line %d", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Missing value in set action in line %d\n", line);
			}
			return RERR_FAIL;
		}
		action->typ = EV_ACT_NAME;
		action->name = val;
		return RERR_OK;
	}
	if (!strcmp (val, "!")) {
		unset = 1;
	} else {
		if (*val == '"') {
			eval.typ = EVP_T_STR;
			eval.s = top_getquotedfield (&val, NULL, 0);
		} else if (*val == '&' && val[1] =='i') {
			eval.typ = EVP_T_INT;
			eval.i = (int64_t)strtoull (val+2, NULL, 16);
		} else if (*val == '&' && val[1] =='f') {
			uint64_t	ival;
			eval.typ = EVP_T_FLOAT;
			ival = (uint64_t)strtoull (val+2, NULL, 16);
			eval.f = *(double*)(void*)&ival;
		} else {
			for (s=val; isdigit (*s) || *s=='-' || *s=='+'; s++);
			if (*s == '.' || *s == 'e' || *s == 'E') {
				eval.typ = EVP_T_FLOAT;
				eval.f = strtod (val, NULL);
			} else {
				eval.typ = EVP_T_INT;
				eval.i = strtoll (val, NULL, 10);
			}
		}
	}
	sswitch (ref) {
	sincase ("arg")
		if (!unset) {
			action->typ = EV_ACT_ARG;
		} else {
			action->typ = EV_ACT_UNARG;
		}
		s = index (ref, '[');
		if (s) {
			s = top_skipwhite (s+1);
			action->argnum = atoi (s);
		} else {
			action->argnum = 0;
		}
		if (!unset) {
			action->argval = eval;
		}
		break;
	sincase ("attr")
		isvar = 0;
		/* fall thru */
	sincase ("var")
		if (isvar) {
			action->typ = unset ? EV_ACT_UNVAR : EV_ACT_VAR;
		} else {
			action->typ = unset ? EV_ACT_UNATTR : EV_ACT_ATTR;
		}
		top_getfield (&ref, "{", TOP_F_NOSKIPBLANK);
		s = top_getfield (&ref, "}", TOP_F_NOSKIPBLANK);
		if (!s || !*s) {
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "missing %s name in line %d",
										isvar?"variable":"attribute", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Missing %s name in line %d\n",
										isvar?"variable":"attribute", line);
			}
			return RERR_FAIL;
		}
		if (isvar) {
			char	c;
			action->var.var = s = top_skipwhite (s);
			action->var.idx1 = 1;
			action->var.idx2 = 0;
			for (; isalnum(*s) || *s==':' || *s=='/' || *s=='_'; s++);
			c = *s;
			*s = 0;
			if (!(c==0 || c=='.' || c=='[')) {
				for (s++; *s && !(*s=='.' || *s=='['); s++);
				c=*s;
			}
			if (c=='.') {
				s = top_skipwhite(s+1);
				action->var.idx1 = strtol (s, &s, 10);
				if (s && *s) {
					s = top_skipwhite (s);
					c = *s;
				} else {
					c = 0;
				}
			}
			if (c=='[') {
				s = top_skipwhite (s+1);
				action->var.idx2 = atoi (s);
			}
			if (!unset) action->var.val = eval;
		} else {
			action->attr.var = s;
			if (!unset) action->attr.val = eval;
		}
		break;
	sdefault
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "unknown reference >>%s<< in set action in line %d",
									str, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Unknown reference >>%s<< in set action in line %d\n",
									str, line);
		}
		return RERR_FAIL;
	} esac;
	return RERR_OK;
}


static
int
sevf_parselabel (str, label, flags, line, pline)
	char					*str;
	struct ev_label	*label;
	int					flags, line, pline;
{
	char	*s;

	if (!str || !label) return RERR_PARAM;
	str = top_skipwhiteplus (str, "@");
	for (s=str; isalnum (*s) || *s == '_'; s++);
	*s = 0;
	if (!*str) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "invalid label name in line %d", pline);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Invalid label name in line %d\n", pline);
		}
		return RERR_FAIL;
	}
	bzero (label, sizeof (struct ev_label));
	label->name = str;
	label->lineno = line;
	return RERR_OK;
}


#define TOK_NONE		0
#define TOK_UNKNOWN	1
#define TOK_NAME		2
#define TOK_ARG		3
#define TOK_ATTR		4
#define TOK_VAR		5
#define TOK_AND		6
#define TOK_OR			7
#define TOK_XOR		8
#define TOK_NOT		9
#define TOK_LPAR		10
#define TOK_RPAR		11
#define TOK_CMP		12
#define TOK_VAL		13
#define TOK_LAST		14
#define TOK_PREV		15
#define TOK_ACTION	16

#define TOK_REF		30
#define TOK_LOP		31
#define TOK_NODE		32


static
int
sevf_parsecondition (ptr, node, flags, line)
	char				**ptr;
	struct ev_node	**node;
	int				flags, line;
{
	int				tok, ret;
	struct ev_val	val;

	if (!node) return RERR_PARAM;
	sevf_cleanstack ();
	while (1) {
		tok = sevf_gettok (ptr, &val);
		if (tok < 0) return tok;
		switch (tok) {
		case TOK_UNKNOWN:
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "syntax error in line %d", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Syntax error in line %d\n", line);
			}
			if (flags & EVF_F_STRICT) {
				return RERR_FAIL;
			}
			break;
		case TOK_NONE:
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "missing action in line %d", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Missing action in line %d\n", line);
			}
			return RERR_FAIL;
		case TOK_ACTION:
			return sevf_getnode (node, flags, line);
		default:
			ret = sevf_shiftreduce (tok, &val, flags, line);
			if (!RERR_ISOK(ret)) return ret;
			break;
		}
	}
	return RERR_INTERNAL;	/* shall never reach here */
}


static
int
sevf_gettok (ptr, val)
	char				**ptr;
	struct ev_val	*val;
{
	char		*str;
	int		tok;

	if (!ptr || !*ptr) return 0;
	if (!val) return RERR_PARAM;
	str = *ptr;
	str = top_skipwhite (str);
	if (!str || !*str) return TOK_NONE;
	sswitch (str) {
	sincase ("name") 
		tok = TOK_NAME;
		str += 4;
		for (; isalnum(*str); str++);
		break;
	sincase ("arg")
		tok = TOK_ARG;
		str += 3;
		for (; isalnum(*str); str++);
		str = top_skipwhite (str);
		if (*str == '[') {
			str = top_skipwhite (str+1);
			val->i = atoi (str);
			for (; *str && *str != ']'; str++);
			if (*str == ']') str++;
		} else {
			val->i = 0;
		}
		val->typ = EVP_T_INT;
		break;
	sincase ("attr")
	sincase ("var")
		if (*str == 'v' || *str == 'V') {
			tok = TOK_VAR;
			str += 3;
		} else {
			tok = TOK_ATTR;
			str += 4;
		}
		for (; isalnum(*str); str++);
		str = top_skipwhite (str);
		if (*str == '{') {
			val->s = ++str;
			for (; *str && *str != '}'; str++);
			if (*str == '}') {
				*str = 0;
				str++;
			}
			val->s = top_stripwhite ((char*)val->s, 0);
			val->typ = EVP_T_STR;
		} else {
			val->s = (char*)"";
			val->typ = EVP_T_VOID;
		}
		break;
	sincase ("last")
		tok = TOK_LAST;
		str += 4;
		break;
	sincase ("prev")
		tok = TOK_PREV;
		str += 4;
		break;
	sncase ("(")
		tok = TOK_LPAR;
		str++;
		break;
	sncase (")")
		tok = TOK_RPAR;
		str++;
		break;
	sncase ("&&")
		tok = TOK_AND;
		str += 2;
		break;
	sncase ("||")
		tok = TOK_OR;
		str += 2;
		break;
	sncase ("^^")
		tok = TOK_XOR;
		str += 2;
		break;
	sncase ("==")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_EQ;
		str += 2;
		break;
	sncase ("!=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_NEQ;
		str += 2;
		break;
	sncase (">=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_GE;
		str += 2;
		break;
	sncase (">")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_GT;
		str += 1;
		break;
	sncase ("<=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_LE;
		str += 2;
		break;
	sncase ("<")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_LT;
		str += 1;
		break;
	sncase ("#")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_MATCH;
		str += 1;
		break;
	sncase ("!#")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_NMATCH;
		str += 2;
		break;
	sncase ("^==")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CEQ;
		str += 3;
		break;
	sncase ("^!=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CNEQ;
		str += 3;
		break;
	sncase ("^>=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CGE;
		str += 3;
		break;
	sncase ("^>")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CGT;
		str += 2;
		break;
	sncase ("^<=")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CLE;
		str += 3;
		break;
	sncase ("^<")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CLT;
		str += 2;
		break;
	sncase ("^#")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CMATCH;
		str += 2;
		break;
	sncase ("^!#")
		tok = TOK_CMP;
		val->typ = EVP_T_INT;
		val->i = EV_OP_CNMATCH;
		str += 3;
		break;
	sncase ("!")
		tok = TOK_NOT;
		str++;
		break;
	sncase ("=>")
		tok = TOK_ACTION;
		str+=2;
		break;
	sncase ("\"")
		tok = TOK_VAL;
		val->typ = EVP_T_STR;
		val->s = ++str;
		for (; *str && *str != '"'; str++) {
			if (*str == '\\') {
				str++;
				continue;
			}
		}
		if (*str) {
			*str = 0;
			str++;
		}
		val->s = top_unquote ((char*)val->s, TOP_F_UNQUOTE_DOUBLE);
		break;
	sncase ("&i")
		tok = TOK_VAL;
		val->typ = EVP_T_INT;
		val->i = (int64_t)(uint64_t)strtoull(str+2, &str, 16);
		break;
	sncase ("&f")
		tok = TOK_VAL;
		val->typ = EVP_T_FLOAT;
		assert (sizeof(int64_t)==sizeof(double));
		{
			int64_t	ival;
			ival = (int64_t)(uint64_t)strtoull(str+2, &str, 16);
			val->f = *(double*)(void*)&ival;
		}
		break;
	sdefault
#define NUMINICHAR(s) ((((s) >= '0') && ((s) <= '9')) || ((s) == '+') \
								|| ((s) == '-') || ((s) == '.'))
#define NUMCHAR(s) (NUMINICHAR(s) || ((s) == 'e') || ((s) == 'E'))
#define NUMFLOATCHAR(s) (((s) == 'e') || ((s) == 'E') || ((s) == '.'))
		if (!NUMINICHAR(*str)) {
			/* error - eat up till whitespace */
			for (; *str && !iswhite (*str); str++);
			tok = TOK_UNKNOWN;
			break;
		}
		{
			char	*s;
			val->typ = EVP_T_INT;
			for (s=str; *s && NUMCHAR(*s); s++) {
				if (NUMFLOATCHAR(*s)) {
					val->typ = EVP_T_FLOAT;
					break;
				}
			}
		}
		if (val->typ == EVP_T_INT) {
			val->i = (int64_t)strtoll (str, &str, 10);
		} else {
			val->f = strtod (str, &str);
		}
		tok = TOK_VAL;
		break;
	} esac;
	*ptr = str;
	return tok;
}


/* *********************
 * shift reduce parser
 * *********************/

struct srelement {
	int				grp;
	int				tok;
	struct ev_val	val;
	struct ev_node	*node;
};
#define STACKSZ	256
struct srelement	stack[STACKSZ];
int					stackptr=0;

static int mksrul (struct ev_node**, int, struct ev_val*, int, struct ev_val*, int, int);
static int pushnode (struct ev_node*, int, int);
static int pushnode2 (struct ev_node*, int, int);
static int pushref (int, struct ev_val*, int, int);
static int pushlop (int, int, int);
static int pushnot (int, int);
static int pushlpar (int, int);
static int pushrpar (int, int);
static int pushcmp (struct ev_val*, int, int);
static int pushval (struct ev_val*, int, int);
static int pushlast (int, int, int);
static int shiftreduce (int, struct ev_val*, int, int);
static int error_reduce (int, int);

static
int
sevf_cleanstack ()
{
	int	i;

	if (stackptr > STACKSZ) stackptr = STACKSZ;
	for (i=0; i<stackptr; i++) {
		if (stack[i].tok == TOK_NODE) {
			if (stack[i].node) {
				sevf_nodfree (stack[i].node);
				free (stack[i].node);
				stack[i].node = NULL;
			}
		}
	}
	stackptr = 0;
	bzero (stack, STACKSZ * sizeof (struct srelement));
	return RERR_OK;
}

static
int
sevf_getnode (node, flags, line)
	struct ev_node	**node;
	int				flags, line;
{
	struct ev_node	*nod;
	int				ret;

	if (!node) return RERR_PARAM;
	if (stackptr < 1) {
		*node = nod = malloc (sizeof (struct ev_node));
		if (!nod) return RERR_NOMEM;
		bzero (nod, sizeof (struct ev_node));
		nod->what = EV_NODE_DEFAULT;
		return RERR_OK;
	}
	if (stack[stackptr-1].grp == TOK_REF) {
		ret = mksrul (	&nod, stack[stackptr-1].tok, &(stack[stackptr-1].val), 
							EV_OP_EXIST, NULL, flags, line);
		if (!RERR_ISOK(ret)) return ret;
		stackptr--;
		ret = pushnode (nod, flags, line);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (stackptr < 1) return RERR_INTERNAL;	/* should never happen */
	if (stackptr > 1 || stack[0].grp != TOK_NODE) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error in line %d\n", line);
		}
		if (flags & EVF_F_STRICT) {
			return RERR_FAIL;
		}
		ret = error_reduce (flags, line);
		if (ret == RERR_FAIL) {
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "unresolvable syntax error in line %d - skip "
										"rule", line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Unresolvable syntax error in line %d - skip "
										"rule\n", line);
			}
			return ret;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		}
	}
	if (stackptr != 1 || stack[0].grp != TOK_NODE) return RERR_INTERNAL;
	*node = stack[0].node;
	stackptr = 0;
	return RERR_OK;
}

static
int
sevf_shiftreduce (tok, val, flags, line)
	int				tok;
	struct ev_val	*val;
	int				flags, line;
{
	int	ret;

	ret = shiftreduce (tok, val, flags, line);
	if ((ret == RERR_FAIL) && !(flags & EVF_F_STRICT)) {
		return RERR_OK;
	}
	return ret;
}

static
int
shiftreduce (tok, val, flags, line)
	int				tok;
	struct ev_val	*val;
	int				flags, line;
{
	struct ev_node	*nod;
	int				ret;

	if (stackptr > 0 && tok != TOK_CMP && stack[stackptr-1].grp == TOK_REF) {
		/* wee need to reduce stack first */
		ret = mksrul (	&nod, stack[stackptr-1].tok, &(stack[stackptr-1].val),
							EV_OP_EXIST, NULL, flags, line);
		if (!RERR_ISOK(ret)) return ret;
		stackptr--;
		ret = pushnode (nod, flags, line);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (stackptr > 0 && tok != TOK_VAL && stack[stackptr-1].tok == TOK_CMP) {
		/* error, which we will resolve here */
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (a compare must be followed "
							"by a value) in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (a compare must be followed "
							"by a value) in line %d", line);
		}
		if (flags & EVF_F_STRICT) {
			return RERR_FAIL;
		}
		stackptr--;
		return shiftreduce (tok, val, flags, line);
	}
	switch (tok) {
	case TOK_NAME:
	case TOK_ARG:
	case TOK_ATTR:
	case TOK_VAR:
		return pushref (tok, val, flags, line);
	case TOK_AND:
	case TOK_OR:
	case TOK_XOR:
		return pushlop (tok, flags, line);
	case TOK_NOT:
		return pushnot (flags, line);
	case TOK_LPAR:
		return pushlpar (flags, line);
	case TOK_RPAR:
		return pushrpar (flags, line);
	case TOK_CMP:
		return pushcmp (val, flags, line);
	case TOK_VAL:
		return pushval (val, flags, line);
	case TOK_LAST:
	case TOK_PREV:
		return pushlast (tok, flags, line);
	default:
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "invalid token (%d) in line %d", 
							stack[stackptr-1].tok, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Invalid token in line %d\n", line);
		}
		return RERR_FAIL;
	}
	/* should never reach here */
	return RERR_INTERNAL;
}

static
int
pushlast (tok, flags, line)
	int	tok, flags, line;
{
	struct ev_node	*nod;

	nod = malloc (sizeof (struct ev_node));
	if (!nod) return RERR_NOMEM;
	bzero (nod, sizeof (struct ev_node));
	switch (tok) {
	case TOK_LAST:
		nod->what = EV_NODE_LAST;
		break;
	case TOK_PREV:
		nod->what = EV_NODE_PREV;
		break;
	default:
		return RERR_INTERNAL;
	}
	return pushnode (nod, flags, line);
}

static
int
pushval (val, flags, line)
	struct ev_val	*val;
	int				flags, line;
{
	struct ev_node	*nod;
	int				ret;

	if (stackptr < 2 || !(stack[stackptr-1].tok == TOK_CMP 
						&& stack[stackptr-2].grp == TOK_REF)) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (value not after compare) in line "
										"%d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (value not after compare) in line "
										"%d\n", line);
		}
		return RERR_FAIL;
	}
	/* reduce */
	ret = mksrul (&nod, stack[stackptr-2].tok, &(stack[stackptr-2].val), 
						stack[stackptr-1].val.i, val, flags, line);
	if (!RERR_ISOK(ret)) return ret;
	stackptr-=2;
	return pushnode (nod, flags, line);
}

static
int
pushcmp (val, flags, line)
	struct ev_val	*val;
	int				flags, line;
{
	if (!val) return RERR_PARAM;

	if (stackptr < 1 || stack[stackptr-1].grp != TOK_REF) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (invalid compare operator) in line "
										"%d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (invalid compare operator) in line "
										"%d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].tok = TOK_CMP;
	stack[stackptr].grp = TOK_CMP;
	stack[stackptr].val = *val;
	stack[stackptr].node = NULL;
	stackptr++;
	return RERR_OK;
}


static
int
pushrpar (flags, line)
	int	flags, line;
{
	struct ev_node	*nod;

	if (stackptr < 2 || !(stack[stackptr-1].tok == TOK_NODE 
							&& stack[stackptr-2].tok == TOK_LPAR)) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (invalid right parenthesis) in line "
										"%d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (invalid right parenthesis) in line "
										"%d\n", line);
		}
		return RERR_FAIL;
	}
	/* reduce */
	nod = stack[stackptr-1].node;
	stackptr-=2;
	return pushnode (nod, flags, line);
}

static
int
pushlpar (flags, line)
	int	flags, line;
{
	if (stackptr > 0 && stack[stackptr-1].tok == TOK_NODE) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (invalid left parenthesis) in line "
										"%d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (invalid left parenthesis) in line "
										"%d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].tok = TOK_LPAR;
	stack[stackptr].grp = TOK_LPAR;
	stackptr++;
	return RERR_OK;
}

static
int
pushnot (flags, line)
	int	flags, line;
{
	if (stackptr > 0 && stack[stackptr-1].tok == TOK_NOT) {
		/* not eliminates not */
		stackptr--;
		return RERR_OK;
	}
	if (stackptr > 0 && stack[stackptr-1].tok == TOK_NODE) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error (invalid not) in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error (invalid not) in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].tok = TOK_NOT;
	stack[stackptr].grp = TOK_NOT;
	stackptr++;
	return RERR_OK;
}

static
int
pushlop (tok, flags, line)
	int	tok;
	int	flags, line;
{
	if (stackptr < 1 || stack[stackptr-1].tok != TOK_NODE) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].tok = tok;
	stack[stackptr].grp = TOK_LOP;
	stackptr++;
	return RERR_OK;
}


static
int
pushref (tok, val, flags, line)
	int				tok;
	struct ev_val	*val;
	int				flags, line;
{
	struct ev_node	*nod;
	int				ret;

	if (stackptr > 0 && stack[stackptr-1].tok == TOK_NOT) {
		ret = mksrul (&nod, tok, val, EV_OP_NEXIST, NULL, flags, line);
		if (!RERR_ISOK(ret)) return ret;
		stackptr--;
		return pushnode (nod, flags, line);
	}
	if (stackptr > 0 && !(stack[stackptr-1].tok == TOK_LPAR 
								|| stack[stackptr-1].grp == TOK_LOP)) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].tok = tok;
	stack[stackptr].grp = TOK_REF;
	stack[stackptr].val = *val;
	stackptr++;
	return RERR_OK;
}
	


static
int
pushnode (node, flags, line)
	struct ev_node	*node;
	int				flags, line;
{
	int	ret;

	ret = pushnode2 (node, flags, line);
	if (!RERR_ISOK(ret)) {
		sevf_nodfree (node);
		free (node);
		if (ret == RERR_FAIL) {
			if (flags & EVF_F_STRICT) {
				return RERR_FAIL;
			} else {
				return RERR_OK;
			}
		}
		return ret;
	}
	return RERR_OK;
}

static
int
pushnode2 (node, flags, line)
	struct ev_node	*node;
	int				flags, line;
{
	struct ev_node	*newnod;

	if (!node) return RERR_PARAM;
	if (stackptr >= 1 && stack[stackptr-1].tok == TOK_NOT) {
		newnod = malloc (sizeof (struct ev_node));
		if (!newnod) {
			return RERR_NOMEM;
		}
		bzero (newnod, sizeof (struct ev_node));
		newnod->what = EV_NODE_NOT;
		newnod->nod = node;
		stackptr--;
		return pushnode (newnod, flags, line);
	}
	if (stackptr >= 2 && stack[stackptr-1].grp == TOK_LOP && stack[stackptr-2].tok == TOK_NODE) {
		newnod = malloc (sizeof (struct ev_node));
		if (!newnod) {
			return RERR_NOMEM;
		}
		bzero (newnod, sizeof (struct ev_node));
		newnod->what = EV_NODE_LOP;
		newnod->lop = malloc (sizeof (struct ev_lop));
		if (!newnod->lop) {
			free (newnod);
			return RERR_NOMEM;
		}
		bzero (newnod->lop, sizeof (struct ev_lop));
		switch (stack[stackptr-1].tok) {
		case TOK_AND:
			newnod->lop->lop = EV_LOP_AND;
			break;
		case TOK_OR:
			newnod->lop->lop = EV_LOP_OR;
			break;
		case TOK_XOR:
			newnod->lop->lop = EV_LOP_XOR;
			break;
		default:
			free (newnod->lop);
			free (newnod);
			if (!(flags & EVF_F_QUIET)) {
				FRLOGF (LOG_DEBUG, "invalid token (%d) in line %d", 
								stack[stackptr-1].tok, line);
			}
			if (flags & EVF_F_VERBOSE) {
				fprintf (stderr, "Invalid token in line %d\n", line);
			}
			return RERR_FAIL;
		}
		if (!stack[stackptr-2].node) {
			free (newnod->lop);
			free (newnod);
			return RERR_INTERNAL;
		}
		newnod->lop->left = *(stack[stackptr-2].node);
		free (stack[stackptr-2].node);
		stack[stackptr-2].node = NULL;
		stackptr-=2;
		newnod->lop->right = *node;
		free (node);
		return pushnode (newnod, flags, line);
	}
	if (stackptr > 0 && stack[stackptr-1].tok != TOK_LPAR) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "syntax error in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Syntax error in line %d\n", line);
		}
		return RERR_FAIL;
	}
	if (stackptr >= STACKSZ) return RERR_STACK_OVERFLOW;
	stack[stackptr].node = node;
	stack[stackptr].grp = TOK_NODE;
	stack[stackptr].tok = TOK_NODE;
	stackptr++;

	return RERR_OK;
}


static
int
mksrul (node, tok, arg, op, val, flags, line)
	struct ev_node	**node;
	int				tok, op, flags, line;
	struct ev_val	*arg, *val;
{
	struct ev_node	*nod;
	int				rflags, ret;
	char				errstr[128];

	if (!node || !arg) return RERR_PARAM;
	if (!val && !(op == EV_OP_EXIST || op == EV_OP_NEXIST)) return RERR_PARAM;
	nod = malloc (sizeof (struct ev_node));
	if (!nod) return RERR_NOMEM;
	*node = nod;
	bzero (nod, sizeof (struct ev_node));
	nod->what = EV_NODE_LEAF;
	nod->srul = malloc (sizeof (struct ev_srul));
	if (!nod->srul) {
		free (nod);
		return RERR_NOMEM;
	}
	bzero (nod->srul, sizeof (struct ev_srul));
	nod->srul->op = op;
	if (!(op == EV_OP_EXIST || op == EV_OP_NEXIST)) {
		nod->srul->val = *val;
	}
	switch (tok) {
	case TOK_NAME:
		nod->srul->what = EV_WHAT_NAME;
		break;
	case TOK_ARG:
		nod->srul->what = EV_WHAT_ARG;
		nod->srul->arg = arg->i;
		break;
	case TOK_ATTR:
		nod->srul->what = EV_WHAT_ATTR;
		nod->srul->attr = arg->s ? arg->s : "";
		break;
	case TOK_VAR:
		nod->srul->what = EV_WHAT_VAR;
		{
			char	*s, c;
			nod->srul->var.var = s = top_skipwhite (arg->s);
			nod->srul->var.idx1 = 1;
			nod->srul->var.idx2 = 0;
			for (; isalnum(*s) || *s==':' || *s=='/' || *s=='_'; s++);
			c = *s;
			*s = 0;
			if (!(c==0 || c=='.' || c=='[')) {
				for (s++; *s && !(*s=='.' || *s=='['); s++);
				c=*s;
			}
			if (c=='.') {
				s = top_skipwhite(s+1);
				nod->srul->var.idx1 = strtol (s, &s, 10);
				if (s && *s) {
					s = top_skipwhite (s);
					c = *s;
				} else {
					c = 0;
				}
			}
			if (c=='[') {
				s = top_skipwhite (s+1);
				nod->srul->var.idx2 = atoi (s);
			}
		}
		break;
	default:
		free (nod->srul);
		free (nod);
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "invalid token (%d) in line %d", tok, line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Invalid token in line %d\n", line);
		}
		return RERR_FAIL;
	}
	rflags = REG_EXTENDED | REG_NOSUB;
	switch (nod->srul->op) {
	case EV_OP_CMATCH:
	case EV_OP_CNMATCH:
		rflags |= REG_ICASE;
		/* fall thru */
	case EV_OP_MATCH:
	case EV_OP_NMATCH:
		ret = regcomp (&(nod->srul->regex), nod->srul->val.s, rflags);
		if (ret != 0) {
			regerror (ret, &(nod->srul->regex), errstr, sizeof (errstr));
			FRLOGF (LOG_DEBUG, "error compiling regex >>%s<<: %s", 
									nod->srul->val.s, errstr);
			return RERR_FAIL;
		}
		nod->srul->hasregex = 1;
		break;
	}
	return RERR_OK;
}

static
int
error_reduce (flags, line)
	int	flags, line;
{
	struct ev_node	*nod;
	int				ret;

	if (stackptr < 1) return RERR_FAIL;
	if (stackptr == 1 && stack[0].tok == TOK_NODE) return RERR_OK;
	if (stack[stackptr-1].grp == TOK_REF) {
		ret = mksrul (	&nod, stack[stackptr-1].tok, &(stack[stackptr-1].val),
							EV_OP_EXIST, NULL, flags, line);
		if (!RERR_ISOK(ret)) return ret;
		stackptr--;
		ret = pushnode (nod, flags, line);
		if (!RERR_ISOK(ret)) return ret;
		return error_reduce (flags, line);
	}
	if (stack[stackptr-1].tok == TOK_NODE && stack[stackptr-2].tok == TOK_LPAR) {
		if (!(flags & EVF_F_QUIET)) {
			FRLOGF (LOG_DEBUG, "unbalanced left parenthesis in line %d", line);
		}
		if (flags & EVF_F_VERBOSE) {
			fprintf (stderr, "Unbalanced left parenthesis in line %d\n", line);
		}
		ret = pushrpar (flags, line);
		if (ret != RERR_FAIL) {
			if (!RERR_ISOK(ret)) return ret;
			return error_reduce (flags, line);
		}
	}
	if (stackptr < 1) return RERR_INTERNAL;
	/* reduce */
	if (stack[stackptr-1].tok == TOK_NODE) {
		sevf_nodfree (stack[stackptr-1].node);
	}
	stackptr--;
	return error_reduce (flags, line);
}


/* **********************
 * free functions 
 * **********************/

static
int
sevf_rulfree (rule)
	struct ev_rule	*rule;
{
	int	ret, ret2=RERR_OK;

	if (!rule) return RERR_PARAM;
	ret = sevf_actionfree (&(rule->action));
	if (!RERR_ISOK(ret)) ret2 = ret;
	if (rule->node) {
		ret = sevf_nodfree (rule->node);
		if (!RERR_ISOK(ret)) ret2 = ret;
		free (rule->node);
	}
	return ret2;
}


static
int
sevf_actionfree (action)
	struct ev_action	*action;
{
	if (!action) return RERR_PARAM;
	if (action->typ & EV_ACGR_TARGET) {
		TLST_FREE (action->targ);
	}
	action->typ = EV_ACT_NONE;
	if (action->next) {
		sevf_actionfree (action->next);
		free (action->next);
	}
	bzero (action, sizeof (struct ev_action));
	return RERR_OK;
}

static
int
sevf_nodfree (node)
	struct ev_node	*node;
{
	int	ret, ret2=RERR_OK;

	if (!node) return RERR_PARAM;
	switch (node->what) {
	case EV_NODE_LEAF:
		if (node->srul) {
			ret = sevf_srulfree (node->srul);
			if (!RERR_ISOK(ret)) ret2 = ret;
			free (node->srul);
		}
		break;
	case EV_NODE_LOP:
		if (node->lop) {
			ret = sevf_lopfree (node->lop);
			if (!RERR_ISOK(ret)) ret2 = ret;
			free (node->lop);
		}
		break;
	case EV_NODE_NOT:
		if (node->nod) {
			ret = sevf_nodfree (node->nod);
			if (!RERR_ISOK(ret)) ret2 = ret;
			free (node->nod);
		}
		break;
	}
	bzero (node, sizeof (struct ev_node));
	return ret2;
}

static
int
sevf_srulfree (srul)
	struct ev_srul	*srul;
{
	if (!srul) return RERR_PARAM;
	if (srul->hasregex) {
		regfree (&(srul->regex));
	}
	bzero (srul, sizeof (struct ev_srul));
	return RERR_OK;
}

static
int
sevf_lopfree (lop)
	struct ev_lop	*lop;
{
	int	ret, ret2 = RERR_OK;

	if (!lop) return RERR_PARAM;
	switch (lop->lop) {
	case EV_LOP_AND:
	case EV_LOP_OR:
	case EV_LOP_XOR:
		ret = sevf_nodfree (&(lop->left));
		if (!RERR_ISOK(ret)) ret2 = ret;
		ret = sevf_nodfree (&(lop->right));
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}


static
int
float_almost_eq (a, b)
	double	a, b;
{
	double	diff, l;

	diff = fabs (a - b);

	a = fabs (a);
	b = fabs (b); 
	l = (b > a) ? b : a;
	if (diff <= l * DBL_EPSILON) {
		return 1;
	}
	return 0;
}












/*
 * Overrides for XEmacs and vim so that we get a uniform tabbing style.
 * XEmacs/vim will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 3
 * c-basic-offset: 3
 * tab-width: 3
 * End:
 * vim:tw=0:ts=3:wm=0:
 */
