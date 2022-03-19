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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>

#include "bufref.h"
#include "tlst.h"
#include "errors.h"
#include "slog.h"

struct xbuf {
	char		*buf;
	char		*origbuf;	/* for copy only */
	size_t	buflen;
	int		refcnt;
};


#define BUFREF_DOINIT	((struct bufref) { .isinit = 1, .lst = TLST_SINIT_T (struct xbuf) })

static int _bufref_cmp (void*, void*);
static int _bufref_origcmp (void*, void*);

void
bufref_free (bufref)
	struct bufref	*bufref;
{
	struct xbuf	*p;
	unsigned		i;

	if (!bufref) return;
	if (!bufref->isinit) return;
	TLST_FOREACHPTR2 (p, bufref->lst, i) {
		if (!p) continue;
		if (!p->buf) continue;
		free (p->buf);
		p->buf = NULL;
	}
	TLST_FREE (bufref->lst);
	*bufref = BUFREF_DOINIT;
}

void
bufref_reset (bufref)
	struct bufref	*bufref;
{
	struct xbuf	*p;
	unsigned		i;

	if (!bufref) return;
	if (!bufref->isinit) return;
	TLST_FOREACHPTR2 (p, bufref->lst, i) {
		if (!p) continue;
		if (!p->buf) continue;
		free (p->buf);
		p->buf = NULL;
	}
	TLST_RESET (bufref->lst);
}

void
bufref_clear (bufref)
	struct bufref	*bufref;
{
	if (!bufref) return;
	if (!bufref->isinit) return;
	TLST_FREE (bufref->lst);
	*bufref = BUFREF_DOINIT;
}

int
bufref_add (bufref, buf, blen)
	struct bufref	*bufref;
	char				*buf;
	size_t			blen;
{
	struct xbuf		xbuf = { .buf = buf, .buflen = blen, .refcnt = 0 };
	int				ret;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) {
		*bufref = BUFREF_DOINIT;
	}
	ret = TLST_INSERT (bufref->lst, xbuf, _bufref_cmp);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
bufref_add2 (bufref, buf)
	struct bufref	*bufref;
	char				*buf;
{
	return bufref_add (bufref, buf, 0);
}

int
bufref_rmbuf (bufref, buf)
	struct bufref	*bufref;
	char				*buf;
{
	struct xbuf		xbuf = { .buf = (char*)buf }, *p;
	int				ret, id;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) return RERR_OK;
	ret = id = TLST_SEARCH (bufref->lst, xbuf, _bufref_cmp);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (p, bufref->lst, id);
	if (!RERR_ISOK(ret)) return ret;
	free (p->buf);
	p->buf = NULL;
	ret = TLST_REMOVE (bufref->lst, id, TLST_F_SHIFT);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
bufref_addref (bufref, buf)
	struct bufref	*bufref;
	char				*buf;
{
	struct xbuf		xbuf = { .buf = buf, .refcnt = 1 };
	int				ret;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) {
		*bufref = BUFREF_DOINIT;
	}
	ret = TLST_INSERT (bufref->lst, xbuf, _bufref_cmp);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


int
bufref_ref (bufref, buf)
	struct bufref	*bufref;
	const char		*buf;
{
	struct xbuf		xbuf = { .buf = (char*)buf }, *p;
	int				ret, id;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) return RERR_OK;
	ret = id = TLST_SEARCH (bufref->lst, xbuf, _bufref_cmp);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (p, bufref->lst, id);
	if (!RERR_ISOK(ret)) return ret;
	p->refcnt ++;
	return RERR_OK;
}

int
bufref_unref (bufref, buf)
	struct bufref	*bufref;
	const char		*buf;
{
	struct xbuf		xbuf = { .buf = (char*)buf }, *p;
	int				ret, id;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) return RERR_OK;
	ret = id = TLST_SEARCH (bufref->lst, xbuf, _bufref_cmp);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (p, bufref->lst, id);
	if (!RERR_ISOK(ret)) return ret;
	p->refcnt--;
	if (p->refcnt <= 0) {
		free (p->buf);
		p->buf = NULL;
		ret = TLST_REMOVE (bufref->lst, id, TLST_F_SHIFT);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

int
bufref_cpy (dst, src)
	struct bufref			*dst;
	const struct bufref	*src;
{
	struct xbuf	*p, xb;
	int			ret;
	unsigned		i;

	if (!dst || !src) return RERR_PARAM;
	if (!dst->isinit) {
		*dst = BUFREF_DOINIT;
	}
	if (!src->isinit) return RERR_OK;
	TLST_FOREACHPTR2 (p, ((struct bufref*)src)->lst, i) {
		if (!p->buf) continue;
		xb = (struct xbuf) {
			.origbuf = p->buf,
			.buflen = p->buflen,
			.refcnt = p->refcnt
		};
		if (p->buflen) {
			xb.buf = malloc (p->buflen);
			if (!xb.buf) {
				return RERR_NOMEM;
			}
			memcpy (xb.buf, p->buf, p->buflen);
		}
		ret = TLST_INSERT (dst->lst, xb, _bufref_origcmp);
		if (!RERR_ISOK(ret)) {
			if (xb.buf) free (xb.buf);
			return ret;
		}
	}
	return RERR_OK;
}

const char *
bufref_refcpy (dst, buf)
	struct bufref	*dst;
	const char		*buf;
{
	struct xbuf	xb = { .origbuf = (char*)buf }, *p;
	int			ret, id;

	if (!dst || !buf) return NULL;
	ret = id = TLST_SEARCH (dst->lst, xb, _bufref_origcmp);
	if (ret == RERR_NOT_FOUND) return buf;
	if (!RERR_ISOK(ret)) return NULL;
	ret = TLST_GETPTR (p, dst->lst, id);
	if (!RERR_ISOK(ret)) return NULL;
	if (!p->buf) return NULL;
	return (const char*) p->buf + (buf - (const char *)p->origbuf);
}

int
bufref_cpyfinish (dst, dsttmp)
	struct bufref	*dst, *dsttmp;
{
	struct xbuf	*p;
	int			ret;
	unsigned		i;

	if (!dst || !dsttmp) return RERR_PARAM;
	if (!dst->isinit) *dst = BUFREF_DOINIT;
	if (!dsttmp->isinit) return RERR_OK;
	TLST_FOREACHPTR2 (p, dsttmp->lst, i) {
		if (!p || !p->buf) continue;
		p->origbuf = NULL;
		ret = TLST_INSERT (dst->lst, *p, _bufref_cmp);
		if (!RERR_ISOK(ret)) return ret;
	}
	TLST_FREE (dsttmp->lst);
	*dsttmp = BUFREF_DOINIT;
	return RERR_OK;
}


static
int
_bufref_cmp (ptr, val)
	void	*ptr, *val;
{
	struct xbuf	*p, *v;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	v = ptr;	/* yes it is exchanged ! */
	p = val;
	if (v->buf == p->buf) return 0;
	if (v->buf > p->buf && v->buf < p->buf + p->buflen) return 0;
	if (v->buf < p->buf) return -1;
	return 1;
}

static
int
_bufref_origcmp (ptr, val)
	void	*ptr, *val;
{
	struct xbuf	*p, *v;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	v = ptr;	/* yes it is exchanged ! */
	p = val;
	if (v->origbuf == p->origbuf) return 0;
	if (v->origbuf > p->origbuf && v->origbuf < p->origbuf + p->buflen) return 0;
	if (v->origbuf < p->origbuf) return -1;
	return 1;
}



int
bufref_addcpy (bufref, obuf, buf, blen)
	struct bufref	*bufref;
	char				**obuf;
	const char		*buf;
	size_t			blen;
{
	char	*xbuf;
	int	ret;

	if (!bufref || !buf) return RERR_PARAM;
	if (blen < 1) blen = strlen (buf)+1;
	xbuf = malloc (blen);
	if (!xbuf) return RERR_NOMEM;
	memcpy (xbuf, buf, blen);
	ret = bufref_addref (bufref, xbuf);
	if (!RERR_ISOK(ret)) {
		free (xbuf);
		return ret;
	}
	if (obuf) *obuf = xbuf;
	return RERR_OK;
}


char *
bufref_strcpy (bufref, str)
	struct bufref	*bufref;
	const char		*str;
{
	char	*xbuf = NULL;
	int	ret;

	ret = bufref_addcpy (bufref, &xbuf, str, 0);
	if (!RERR_ISOK(ret)) return NULL;
	return xbuf;
}


int
bufref_numref (bufref, buf)
	struct bufref	*bufref;
	const char		*buf;
{
	struct xbuf		xbuf = { .buf = (char*)buf }, *p;
	int				ret, id;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) return 0;
	ret = id = TLST_SEARCH (bufref->lst, xbuf, _bufref_cmp);
	if (ret == RERR_NOT_FOUND) return 0;
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (p, bufref->lst, id);
	if (!RERR_ISOK(ret)) return ret;
	return p->refcnt;
}


int
bufref_reforcpy (bufref, obuf, buf, blen)
	struct bufref	*bufref;
	char				**obuf;
	const char		*buf;
	size_t			blen;
{
	struct xbuf		xbuf = { .buf = (char*)buf }, *p;
	int				ret, id;

	if (!bufref || !buf) return RERR_PARAM;
	if (!bufref->isinit) {
		return bufref_addcpy (bufref, obuf, buf, blen);
	}
	ret = id = TLST_SEARCH (bufref->lst, xbuf, _bufref_cmp);
	if (ret == RERR_NOT_FOUND) {
		return bufref_addcpy (bufref, obuf, buf, blen);
	}
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (p, bufref->lst, id);
	if (!RERR_ISOK(ret)) return ret;
	p->refcnt ++;
	if (obuf) *obuf = (char*)buf;
	return RERR_OK;
}

const char *
bufref_strreforcpy (bufref, str)
	struct bufref	*bufref;
	const char		*str;
{
	char	*obuf;
	int	ret;

	ret = bufref_reforcpy (bufref, &obuf, str, 0);
	if (!RERR_ISOK(ret)) return NULL;
	return obuf;
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
