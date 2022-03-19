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
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <fr/base.h>

#include "lnqueue.h"
#include "multistream.h"


struct entrybuf {
	char	*buf;
	int	refcnt;
};

struct squeue {
	void			*exist;
	int			stream;
	struct tlst	queue;
	int			first, next;
	struct tlst	frag;
};




static int combinefrags (struct lnqueue*, struct squeue*, char*, int,int);
static int doaddmsg2 (struct lnqueue*, struct squeue*, char*, int, int, int*, int);
static int doaddmsg (struct lnqueue*, int, char*, int, char*, int, int);




int
lnq_new (lnq)
	struct lnqueue	*lnq;
{
	int	ret;

	if (!lnq) return RERR_PARAM;
	bzero (lnq, sizeof (struct lnqueue));
	ret = TLST_NEW (lnq->squeue, struct squeue);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lnq->bufs, struct entrybuf);
	if (!RERR_ISOK(ret)) return ret;
	return ret;
}

int
lnq_setmstream (lnq, prot)
	struct lnqueue	*lnq;
	int				prot;
{
	if (!lnq || prot < 0) return RERR_PARAM;
	if (!MSTR_PROT_VALID(prot)) return RERR_NOT_SUPPORTED;
	lnq->mstream = 1;
	lnq->prot = prot;
	return RERR_OK;
}

int
lnq_unsetmstream (lnq)
	struct lnqueue	*lnq;
{
	if (!lnq) return RERR_PARAM;
	lnq->mstream = 0;
	return RERR_OK;
}


int
lnq_free (lnq)
	struct lnqueue	*lnq;
{
	struct squeue		*sq;
	struct entrybuf	*ebuf;
	unsigned				i;
	int					ret, ret2=RERR_OK;

	if (!lnq) return RERR_PARAM;
	for (i=0; i<=lnq->squeue.max; i++) {
		ret = TLST_GETPTR (sq, lnq->squeue, i);
		if (!RERR_ISOK(ret) || !sq->exist) continue;
		ret = TLST_FREE (sq->queue);
		if (!RERR_ISOK(ret)) ret2 = ret;
		ret = TLST_FREE (sq->frag);
		if (!RERR_ISOK(ret)) ret2 = ret;
		bzero (sq, sizeof (struct squeue));
	}
	for (i=0; i<=lnq->bufs.max; i++) {
		ret = TLST_GETPTR (ebuf, lnq->bufs, i);
		if (!RERR_ISOK(ret)) continue;
		if (ebuf->buf) free (ebuf->buf);
	}
	ret = TLST_FREE (lnq->squeue);
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = TLST_FREE (lnq->bufs);
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = lnq_new (lnq);
	if (!RERR_ISOK(ret)) ret2 = ret;
	return ret2;
}



int
lnq_addmsg (lnq, stream, msg, msglen, flags)
	struct lnqueue	*lnq;
	int				stream, flags, msglen;
	char				*msg;
{
	int		ret, mstream, buflen;
	char		*buf;

	if (!lnq || !msg || msglen < 0) return RERR_PARAM;
	mstream = lnq->mstream;
	if (flags & LNQ_F_DIRECT) mstream = 0;
	buf = msg;
	buflen = msglen;
	if (mstream) {
		if (!(flags & LNQ_F_NOCHECK)) {
			ret = mstr_check (msg, msglen, lnq->prot);
			if (!RERR_ISOK(ret)) return ret;
		}
		stream = mstr_getchan (msg, lnq->prot);
		if (!RERR_ISOK(stream)) return stream;
		msglen = mstr_getmsglen (msg, lnq->prot);
		if (!RERR_ISOK(msglen)) return msglen;
		msg = mstr_getmsg (msg, lnq->prot);
		if (!msg) return RERR_PROTOCOL;
	} else {
		if (stream < 0) return RERR_PARAM;
	}
	if (flags & LNQ_F_CPY) {
		buf = malloc (msglen + 1);
		if (!buf) return RERR_NOMEM;
		memcpy (buf, msg, msglen);
		buf[msglen] = 0;
		msg = buf;
		buflen = msglen;
		flags |= LNQ_F_FREE;
	}
	ret = doaddmsg (lnq, stream, msg, msglen, buf, buflen, flags);
	if (!RERR_ISOK(ret)) {
		if (flags & LNQ_F_CPY) {
			free (buf);
		}
		FRLOGF (LOG_ERR, "error adding message to queue: %s",
								rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


int
lnq_release (lnq, refid)
	struct lnqueue	*lnq;
	int				refid;
{
	struct entrybuf	*ebuf;
	int					ret;

	if (!lnq) return RERR_PARAM;
	if (refid < 0) return RERR_OK;
	ret = TLST_GETPTR (ebuf, lnq->bufs, refid);
	if (!RERR_ISOK(ret)) return ret;
	ebuf->refcnt--;
	if (ebuf->refcnt <= 0) {
		if (ebuf->buf) free (ebuf->buf);
		bzero (ebuf, sizeof (struct entrybuf));
	}
	return RERR_OK;
}


int
lnq_popmsg (qen, lnq, stream)
	struct lnqentry	*qen;
	struct lnqueue		*lnq;
	int					stream;
{
	struct squeue		*sq;
	struct lnqentry	*ptr;
	int					ret, i, j;

	if (!qen || !lnq || stream < 0) return RERR_PARAM;
	ret = TLST_GETPTR (sq, lnq->squeue, stream);
	if (!RERR_ISOK(ret)) return ret;
	if (!sq->exist) return RERR_NOT_FOUND;
	if (sq->first >= sq->next) return RERR_NOT_FOUND;
	ret = TLST_GET (*qen, sq->queue, sq->first);
	if (!RERR_ISOK(ret)) return ret;
	sq->first ++;
	if (sq->first >= sq->next) {
		ret = TLST_RESET (sq->queue);
		if (RERR_ISOK(ret)) {
			sq->first = sq->next = 0;
		} else {
			sq->first = sq->next;
		}
	} else if (sq->first > 640) {	/* number is arbitrary */
		for (i=sq->first, j=0; i<sq->next; i++,j++) {
			ret = TLST_GETPTR (ptr, sq->queue, i);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "cannot get queue entry for shifting, might "
							"cause hazzard in future: %s", rerr_getstr3(ret));
				break;
			}
			ret = TLST_SET (sq->queue, j, *ptr);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "cannot set queue entry for shifting, might "
							"cause hazzard in future: %s", rerr_getstr3(ret));
				break;
			}
		}
		sq->next = j;
		sq->first = 0;
		sq->queue.num = j;	/* hack */
	}
	return RERR_OK;
}

int
lnq_popmsg2 (msg, msglen, refid, lnq, stream)
	char				**msg;
	int				*msglen, *refid, stream;
	struct lnqueue	*lnq;
{
	struct lnqentry	qen;
	int					ret;

	if (!msg || !refid) return RERR_PARAM;
	ret = lnq_popmsg (&qen, lnq, stream);
	if (!RERR_ISOK(ret)) return ret;
	*msg = qen.data;
	if (msglen) *msglen = qen.dlen;
	*refid = qen.refid;
	return RERR_OK;
}


int
lnq_popcpy (msg, msglen, lnq, stream)
	char				**msg;
	int				*msglen, stream;
	struct lnqueue	*lnq;
{
	struct lnqentry	qen;
	int					ret, needcpy;
	struct entrybuf	*ebuf;
	char					*buf;

	if (!msg) return RERR_PARAM;
	ret = lnq_popmsg (&qen, lnq, stream);
	if (!RERR_ISOK(ret)) return ret;
	needcpy = 1;
	if (qen.refid > 0) once {
		ret = TLST_GETPTR (ebuf, lnq->bufs, qen.refid);
		if (!RERR_ISOK(ret)) break;
		if (ebuf->refcnt != 1) break;
		if (qen.data != ebuf->buf) {
			memmove (ebuf->buf, qen.data, qen.dlen);
			qen.data  = ebuf->buf;
			qen.data[qen.dlen] = 0;
			buf = realloc (qen.data, qen.dlen+1);
			if (buf) qen.data = buf;
		}
		bzero (ebuf, sizeof (struct entrybuf));
		lnq->bufs.num --;	/* hack */
		needcpy = 0;
	}
	if (needcpy) {
		buf = malloc (qen.dlen+1);
		memcpy (buf, qen.data, qen.dlen);
		buf[qen.dlen] = 0;
		qen.data = buf;
		lnq_release (lnq, qen.refid);
	}
	*msg = qen.data;
	if (msglen) *msglen = qen.dlen;
	return RERR_OK;
}


int
lnq_has (lnq, stream)
	struct lnqueue	*lnq;
	int				stream;
{
	struct squeue	*sq;
	int				ret;

	if (!lnq || stream < 0) return RERR_PARAM;
	ret = TLST_GETPTR (sq, lnq->squeue, stream);
	if (!RERR_ISOK(ret)) return ret;
	if (!sq->exist) return RERR_NOT_FOUND;
	if (sq->first < sq->next) return 1;
	return 0;
}

int
lnq_hasany (lnq)
	struct lnqueue	*lnq;
{
	struct squeue	*sq;
	int				i, ret;

	if (!lnq) return RERR_PARAM;
	for (i=0; i<=(ssize_t)lnq->squeue.max; i++) {
		ret = TLST_GETPTR (sq, lnq->squeue, i);
		if (!RERR_ISOK(ret)) continue;
		if (!sq->exist) continue;
		if (sq->first < sq->next) return i;
	}
	return RERR_FAIL;
}







/* *******************
 * static functions
 * *******************/


static
int
doaddmsg (lnq, stream, msg, msglen, buf, buflen, flags)
	struct lnqueue	*lnq;
	int				stream, msglen, buflen, flags;
	char				*msg, *buf;
{
	struct squeue		*sqp, sq;
	int					ret, refid, refcnt;
	struct entrybuf	ebuf, *ebufptr;

	if (!lnq || !msg || !buf || msglen < 0 || buflen < 0) return RERR_PARAM;
	/* get squeue struct */
	ret = TLST_GETPTR (sqp, lnq->squeue, stream);
	if (!RERR_ISOK(ret) && (ret != RERR_NOT_FOUND)) return ret;
	if (ret == RERR_NOT_FOUND) {
		bzero (&sq, sizeof (struct squeue));
		ret = TLST_SET (lnq->squeue, stream, sq);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_GETPTR (sqp, lnq->squeue, stream);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (!sqp) return RERR_INTERNAL;
	if (!sqp->exist) {
		bzero (sqp, sizeof (struct squeue));
		sqp->stream = stream;
		ret = TLST_NEW (sqp->queue, struct lnqentry);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_NEW (sqp->frag, struct lnqentry);
		if (!RERR_ISOK(ret)) return ret;
		sqp->exist = sqp;	/* any but NULL */
	}

	/* first add buf */
	if (flags & LNQ_F_FREE) {
		bzero (&ebuf, sizeof (struct entrybuf));
		ebuf.buf = buf;
		ebuf.refcnt = 1;
		ret = refid = TLST_ADDINSERT (lnq->bufs, ebuf);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_GETPTR (ebufptr, lnq->bufs, refid);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		ebufptr = NULL;
		refid = -1;
	}

	/* add message */
	refcnt = 0;
	ret = doaddmsg2 (lnq, sqp, msg, msglen, refid, &refcnt, flags);
	if (ebufptr) {
		if (refcnt == 0) {
			bzero (ebufptr, sizeof (struct entrybuf));
		} else {
			ebufptr->refcnt = refcnt;
		}
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
doaddmsg2 (lnq, sq, msg, msglen, refid, refcnt, flags)
	struct lnqueue	*lnq;
	struct squeue	*sq;
	char				*msg;
	int				msglen, refid, flags, *refcnt;
{
	int					i, ret;
	char					*s;
	struct lnqentry	qen;

	if (!lnq || !sq || !msg || !refcnt || msglen < 0) return RERR_PARAM;
	*refcnt = 0;
	for (s=msg, i=0; i<msglen && *s!='\n'; s++, i++);
	if (i >= msglen) {
		/* we have a fragment */
		bzero (&qen, sizeof (struct lnqentry));
		qen.data = msg;
		qen.dlen = msglen;
		qen.refid = refid;
		ret = TLST_ADD (sq->frag, qen);
		if (!RERR_ISOK(ret)) return ret;
		*refcnt = 1;
		return RERR_OK;
	}

	msglen -= i + 1;
	if (i > 0 && s[-1] == '\r') {
		s[-1] = 0;
		i--;
	} else {
		*s = 0;
	}
	s++;

	/* add first entry */
	if (TLST_GETNUM(sq->frag) > 0) {
		ret = combinefrags (lnq, sq, msg, i, flags);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		/* insert first message */
		qen.data = msg;
		qen.dlen = i;
		qen.refid = refid;
		ret = TLST_ADD (sq->queue, qen);
		if (!RERR_ISOK(ret)) return ret;
		*refcnt = 1;
		sq->next++;
	}
	msg = s;
	while (msglen > 0) {
		for (s=msg, i=0; i<msglen && *s!='\n'; s++, i++);
		bzero (&qen, sizeof (struct lnqentry));
		qen.data = msg;
		qen.dlen = i;
		qen.refid = refid;
		if (i >= msglen) {
			/* we have a fragment */
			ret = TLST_ADD (sq->frag, qen);
			if (!RERR_ISOK(ret)) return ret;
			(*refcnt) ++;
			return RERR_OK;
		}
		msglen -= i + 1;
		if (i > 0 && s[-1] == '\r') {
			s[-1] = 0;
			qen.dlen--;
		} else {
			*s = 0;
		}
		msg = s+1;
		ret = TLST_ADD (sq->queue, qen);
		if (!RERR_ISOK(ret)) return ret;
		(*refcnt)++;
		sq->next++;
	}
	return RERR_OK;
}


static
int
combinefrags (lnq, sq, msg, msglen, flags)
	struct lnqueue	*lnq;
	struct squeue	*sq;
	char				*msg;
	int				msglen, flags;
{
	unsigned				i;
	struct lnqentry	qen, *qep;
	size_t				sz;
	char					*buf, *s;
	struct entrybuf	ebuf;
	int					refid, ret;

	if (!lnq || !sq || !msg || msglen < 0) return RERR_PARAM;
	sz = msglen;
	TLST_FOREACHPTR2 (qep, sq->frag, i) {
		sz += qep->dlen;
	}
	buf = malloc (sz+1);
	if (!buf) return RERR_NOMEM;
	bzero (&ebuf, sizeof (struct entrybuf));
	ebuf.buf = buf;
	ebuf.refcnt = 1;
	ret = refid = TLST_ADDINSERT (lnq->bufs, ebuf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	s = buf;
	TLST_FOREACHPTR2 (qep, sq->frag, i) {
		memcpy (s, qep->data, qep->dlen);
		s += qep->dlen;
	}
	memcpy (s, msg, msglen);
	s += msglen;
	*s = 0;
	bzero (&qen, sizeof (struct lnqentry));
	qen.data = buf;
	qen.dlen = sz;
	qen.refid = refid;
	ret = TLST_ADD (sq->queue, qen);
	if (!RERR_ISOK(ret)) {
		lnq_release (lnq, refid);
		return ret;
	}
	sq->next++;
	TLST_FOREACHPTR2 (qep, sq->frag, i) {
		lnq_release (lnq, qep->refid);
		bzero (qep, sizeof (struct lnqentry));
	}
	TLST_RESET (sq->frag);
	return RERR_OK;
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
