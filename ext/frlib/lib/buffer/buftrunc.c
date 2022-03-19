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
 * Portions created by the Initial Developer are Copyright (C) 1999-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include "buffer.h"
#include "buffer_types.h"


int
buftrunc (buf, len)
	BUFFER	buf;
	uint32_t	len;
{
	struct buffer			*p;
	struct buf_memlist	*qm, *qmnext;
	struct buf_blocklist	*qb, *qbnext;
	char						*str;
	uint32_t					len2;
	int						ret;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	if (p->len <= len) return RERR_OK;
	switch (p->type) {
	case BUF_TYPE_NULL:
		break;
	case BUF_TYPE_MEM:
		p->dat.mem.count=0;
		for (qm = p->dat.mem.buffer; qm && len>qm->len; qm=qm->next) {
			p->dat.mem.count++;
			len-=qm->len;
		}
		if (!qm) break;
		p->dat.mem.count++;
		str = realloc (qm->buf, len + 1);
		if (str) qm->buf = str;
		qm->buf [len] = 0;
		qm->len = len;
		p->dat.mem.bufferend = qm;
		for (qm=qm->next; qm; qm=qmnext) {
			qmnext=qm->next;
			if (qm->buf) free (qm->buf);
			free (qm);
		}
		if (p->dat.mem.bufferend) {
			p->dat.mem.bufferend->next = NULL;
		}
		p->len = len;
		break;
	case BUF_TYPE_BLOCK:
		p->dat.block.count=0;
		for (qb = p->dat.block.buffer; qb; qb=qb->next) {
			len2 = qb->next ? BUF_PAGE: p->len%BUF_PAGE;
			if (len2 == 0 && p->len != 0) len2 = p->len;
			if (len<=len2) break;
			p->dat.block.count++;
			len-=len2;
		}
		if (!qb) break;
		p->dat.block.count++;
		qb->buf [len] = 0;
		p->dat.block.bufferend = qb;
		for (qb=qb->next; qb; qb=qbnext) {
			qbnext=qb->next;
			free (qb);
		}
		if (p->dat.block.bufferend) {
			p->dat.block.bufferend->next = NULL;
		}
		p->len = len;
		break;
	case BUF_TYPE_FILE: 
		if (!p->dat.file.f) {
			ret = buffileopen (buf);
			if (!RERR_ISOK(ret)) return ret;
		}
		rewind (p->dat.file.f);
		ftruncate (fileno(p->dat.file.f), len);
		fseek (p->dat.file.f, 0, SEEK_END);
		buffileclose (buf, 0);
		p->len = len;
		if (!(p->flags & BUF_F_NO_RECONVERT) && (p->otype != BUF_TYPE_FILE)
						&& (p->max_mem_size >= p->len))  {
			bufconvert (buf, p->otype);
		}
		break;
	case BUF_TYPE_CONT: 
		if ((len == 0) && (p->dat.cont.bufferlen != BUF_PAGE)) {
			len2 = BUF_PAGE;
		} else if (p->flags & BUF_F_GROW_FAST) {
			for (len2 =p->dat.cont.bufferlen; (len2>>1) > (len+1); len>>=1);
			if (len2 < BUF_PAGE) len2 = BUF_PAGE;
		} else {
			len2 = ((len + 1) / BUF_PAGE) * BUF_PAGE + BUF_PAGE;
		}
		str = realloc (p->dat.cont.buffer, len2);
		if (str) {
			p->dat.cont.buffer = str;
			p->dat.cont.bufferlen = len2;
		}
		p->dat.cont.buffer [len] = 0;
		p->len = len;
		break;
	}
	return RERR_OK;
}


int
bufclean (buf)
	BUFFER	buf;
{
	struct buffer	*p;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->overflow = 0;
	return buftrunc (buf, 0);
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
