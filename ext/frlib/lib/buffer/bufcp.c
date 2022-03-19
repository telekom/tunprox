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

static int buf_convempty (struct buffer*, int);


int
bufcp (dest, src)
	BUFFER	dest, src;
{
	struct buffer			*pd, *ps;
	struct buf_memlist	*qm;
	struct buf_blocklist	*qb;
	BUFFER 					help;
	int						ret;
	int						tid;
	u_int32_t				len, num;
	char						*str;

	if (!dest || !src) return RERR_PARAM;
	if (dest == src) {
		ret = bufdup (&help, src);
		if (!help) return ret;
		ret = bufmv (dest, help);
		bufclose (help);
		return ret;
	}
	pd = (struct buffer*)dest;
	ps = (struct buffer*)src;

	if (ps->type == BUF_TYPE_NULL) return RERR_OK;
	if (pd->type == BUF_TYPE_NULL) return RERR_OK;
	len = pd->len + ps->len;
	num = ps->len;
	if ((pd->flags & BUF_F_MAXSIZE) && (len > pd->max_size)) {
		num = pd->max_size - pd->len;
		if (pd->len > pd->max_size) num=0;
		len = ps->max_size;
		pd->overflow++;
	}
	if (num == 0) return RERR_OK;
	if (len > ps->max_mem_size) {
		tid = pd->type;
		bufconvert (dest, BUF_TYPE_FILE);
		pd->otype = tid;
	}
	switch (pd->type) {
	case BUF_TYPE_BLOCK:
#if 0	/* to be done - to be more performant and less memory consuming */
		if (ps->type == BUF_TYPE_FILE) {
			if (!ps->dat.file
			if (ps->len % BUF_PAGE) {
			}
			break;
		}
#endif
		/* else fall thru */
	case BUF_TYPE_MEM:
	    switch (ps->type) {
	    case BUF_TYPE_MEM:
			for (qm=ps->dat.mem.buffer; qm && num>0; qm=qm->next) {
				ret = bufnprint (dest, qm->buf, qm->len<num?qm->len:num);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_ERR, "error copying into dest buffer: %s", 
									rerr_getstr3 (ret));
					return ret;
				}
				num-=qm->len;
			}
			break;
	    case BUF_TYPE_FILE:
			str = malloc (num+1);
			if (!str) return RERR_NOMEM;
			ret = bufnprints (src, str, num);
			if (!RERR_ISOK(ret)) {
				free (str);
				return ret;
			}
			ret = bufnputstr (dest, str, num);
			if (!RERR_ISOK(ret)) return ret;
			break;
	    case BUF_TYPE_CONT:
			if (!ps->dat.cont.buffer) return RERR_OK;
			ret = bufnprint (dest, ps->dat.cont.buffer, num);
			break;
		case BUF_TYPE_BLOCK:
			for (qb=ps->dat.block.buffer; qb && num>0; qb=qb->next) {
				len = qb->next ? BUF_PAGE : ps->len%BUF_PAGE;
				if (len == 0 && ps->len != 0) len = BUF_PAGE;
				ret = bufnprint (dest, qb->buf, len<num?len:num);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_ERR, "error copying into dest buffer: %s",
									rerr_getstr3 (ret));
					return ret;
				}
				num-=len;
			}
	    }
	    break;
	case BUF_TYPE_FILE:
		if (!pd->dat.file.f) {
			ret = buffileopen (dest);
			if (!RERR_ISOK(ret)) return ret;
		}
		bufnprintout (src, pd->dat.file.f, num);
		bufevallen (dest);
		buffileclose (src, 0);
		break;
	case BUF_TYPE_CONT:
		len = pd->len + num + 1;
		if (pd->dat.cont.bufferlen > len) {
			len = pd->dat.cont.bufferlen;
		} else if (len%BUF_PAGE) {
			len += BUF_PAGE - len % BUF_PAGE;
		}
		str = realloc (pd->dat.cont.buffer, len);
		if (!str) return RERR_NOMEM;
		pd->dat.cont.buffer = str;
		pd->dat.cont.bufferlen = len;
		bufnprints (src, str+pd->len, num);
		pd->len+=num;
		break;
	}
	return RERR_OK;
}





int
bufmv (dest, src)
	BUFFER	dest, src;
{
	struct buffer	*pd, *ps;
	int				otype, ret, tid;
	u_int32_t		len, num;

	if (!dest || !src) return RERR_PARAM;
	if (dest == src) return RERR_PARAM;
	pd = (struct buffer*)dest;
	ps = (struct buffer*)src;
	if (ps->type == BUF_TYPE_NULL) return RERR_OK;
	len = pd->len + ps->len;
	if ((pd->flags & BUF_F_MAXSIZE) && (len > pd->max_size)) {
		num = pd->max_size - pd->len;
		if (pd->len > pd->max_size) num=0;
		buftrunc (src, num);
		ps->overflow = 0;
		len = ps->max_size;
		pd->overflow++;
	}
	if (ps->len == 0) return RERR_OK;
	if ((pd->flags & BUF_F_AUTOCONVERT) && (len > pd->max_mem_size)) {
		tid = pd->type;
		bufconvert (dest, BUF_TYPE_FILE);
		pd->otype = tid;
	}
	if (!((pd->type == BUF_TYPE_MEM) || (pd->type == BUF_TYPE_BLOCK 
				&& ps->type == BUF_TYPE_BLOCK && (pd->len % BUF_PAGE == 0)))) {
		ret = bufcp (dest, src);
		bufclean (src);
		return ret;
	}

	switch (pd->type) {
	case BUF_TYPE_MEM:
		otype = ps->otype;
		if (ps->type != BUF_TYPE_MEM) {
			ret = bufconvert (src, BUF_TYPE_MEM);
			if (!RERR_ISOK(ret)) {
				ret = bufcp (dest, src);
				bufclean (src);
				bufconvert (src, otype);
				return ret;
			}
		}
		if (!ps->dat.mem.buffer) {
			bufconvert (src, otype);
			return RERR_OK;
		}
		if (!pd->dat.mem.buffer) {
			pd->dat.mem.buffer = ps->dat.mem.buffer;
		} else {
			pd->dat.mem.bufferend->next = ps->dat.mem.buffer;
		}
		pd->dat.mem.bufferend = ps->dat.mem.bufferend;
		pd->len += ps->len;
		pd->dat.mem.count += ps->dat.mem.count;
		ps->dat.mem.buffer = NULL; 
		ps->dat.mem.bufferend = NULL;
		ps->dat.mem.count = 0;
		ps->len = 0;
		ps->overflow = 0;
		bufconvert (src, otype);
		break;
	case BUF_TYPE_BLOCK:
		if (!ps->dat.block.buffer) break;
		if (pd->len == 0 && pd->dat.block.buffer) {
			free (pd->dat.block.buffer);
			pd->dat.block.buffer = NULL;
			pd->dat.block.bufferend = NULL;
			pd->dat.block.count = 0;
		}
		if (pd->dat.block.buffer) {
			pd->dat.block.bufferend->next = ps->dat.block.buffer;
		} else {
			pd->dat.block.buffer = ps->dat.block.buffer;
		}
		pd->dat.block.bufferend = ps->dat.block.bufferend;
		pd->dat.block.count += ps->dat.block.count;
		ps->dat.block.buffer = NULL;
		ps->dat.block.bufferend = NULL;
		ps->dat.block.count = 0;
		pd->len+=ps->len;
		ps->len = 0;
		ps->overflow = 0;
		break;
	}
	return RERR_OK;
}




int
bufconvert (buf, type)
	BUFFER	buf;
	int		type;
{
	struct buffer			*p;
	struct buf_memlist	*qm, *qmnext;
	struct buf_blocklist	*qb, *qbnext;
	int						ret, ret2;
	u_int32_t				len, origlen;
	char						*str;

	if (!buf || type < BUF_TYPE_MIN || type > BUF_TYPE_MAX) return RERR_PARAM;
	p = (struct buffer*) buf;
	if (type == p->type) return RERR_OK;
	if (type == BUF_TYPE_NULL) {
		ret = bufclean (buf);
		if (!RERR_ISOK(ret)) return ret;
		return buf_convempty (buf, type);
	}
	if (p->type == BUF_TYPE_NULL || p->len == 0) {
		return buf_convempty (buf, type);
	}
	if (p->type == BUF_TYPE_FILE && p->len > p->max_mem_size &&
			p->max_mem_size>0 && (p->flags & BUF_F_AUTOCONVERT)) {
		p->otype = type;
		return RERR_OK;
	}
	origlen = p->len;
	p->len = 0;
	switch (p->type) {
	case BUF_TYPE_MEM:
		qm = p->dat.mem.buffer;
		p->dat.mem.buffer = NULL;
		p->dat.mem.bufferend = NULL;
		ret = buf_convempty (buf, type);
		if (!RERR_ISOK(ret)) {
			p->dat.mem.buffer = qm;
			p->type = BUF_TYPE_MEM;
			bufclean (buf);
			return ret;
		}
		for (ret=RERR_OK; qm; qm=qmnext) {
			qmnext = qm->next;
			ret2 = bufnputstr (buf, qm->buf, qm->len);
			if (ret2 != RERR_OK) {
				free (qm->buf);
				ret = ret2;
			}
			free (qm);
		}
		qm = qmnext = NULL;
		if (!RERR_ISOK(ret)) return ret;
		break;
	case BUF_TYPE_BLOCK:
		qb = p->dat.block.buffer;
		p->dat.block.buffer = NULL;
		p->dat.block.bufferend = NULL;
		ret = buf_convempty (buf, type);
		if (!RERR_ISOK(ret)) {
			p->dat.block.buffer = qb;
			p->type = BUF_TYPE_BLOCK;
			bufclean (buf);
			return ret;
		}
		for (ret=RERR_OK; qb; qb=qbnext) {
			len = qb->next?BUF_PAGE:origlen%BUF_PAGE;
			if (len==0 && origlen != 0) len = origlen;
			/* the following is a dirty hack, but the fact, that buf is the
				first var in struct buf_blocklist, does cause the address
				of buf to be alloc'ed, so it can be freed. however, we
				must not free it in here, if we don't have an error cond. 
			 */
			qbnext = qb->next;
			ret2 = bufnputstr (buf, qb->buf, len);
			if (ret2 != RERR_OK) {
				free (qb);
				ret = ret2;
			}
		}
		qb = qbnext = NULL;
		if (!RERR_ISOK(ret)) return ret;
		break;
	case BUF_TYPE_CONT:
		str = p->dat.cont.buffer;
		p->dat.cont.buffer = NULL;
		p->dat.cont.bufferlen = 0;
		ret = buf_convempty (buf, type);
		if (!RERR_ISOK(ret)) {
			free (str);
			return ret;
		}
		ret = bufnputstr (buf, str, origlen);
		if (!RERR_ISOK(ret)) {
			free (str);
			return ret;
		}
		break;
	case BUF_TYPE_FILE:
#if 0
		if (type == BUF_TYPE_BLOCK) {
			if (!p->dat.file.f) {
				ret = buffileopen (buf);
				if (!RERR_ISOK(ret)) return ret;
			}
			f = p->dat.file.f;
			p->dat.file.f = NULL;
			p->dat.file.wasopen = 0;
			ret = buf_convempty (buf, type);
			if (!RERR_ISOK(ret)) {
				fclose (f);
				if (!(p->flags & BUF_F_NO_DELETE)) unlink (p->filename);
				return ret;
			}
			/* to be continued ... */
		}
#endif
		str = malloc (origlen+1);
		if (!str) return RERR_NOMEM;
		ret = bufnprints (buf, str, origlen);
		if (!RERR_ISOK(ret)) {
			free (str);
			return ret;
		}
		ret = buf_convempty (buf, type);	/* it is not empty, but this is ok 
												for file buffers */
		if (!RERR_ISOK(ret)) {
			free (str);
			return ret;
		}
		ret = bufnputstr (buf, str, origlen);
		if (!RERR_ISOK(ret)) {
			free (str);
			return ret;
		}
	    break;
	}
	return RERR_OK;
}




static
int
buf_convempty (buf, type)
	struct buffer	*buf;
	int				type;
{
	u_int32_t	len;

	if (!buf || type < BUF_TYPE_MIN || type > BUF_TYPE_MAX) return RERR_PARAM;
	if (buf->type == type) return RERR_OK;
	switch (buf->type) {
	case BUF_TYPE_NULL:
	case BUF_TYPE_MEM:
		break;
	case BUF_TYPE_BLOCK:
		if (buf->dat.block.buffer) {
			free (buf->dat.block.buffer);
		}
		buf->dat.block.buffer = buf->dat.block.bufferend = NULL;
		break;
	case BUF_TYPE_FILE:
		if (buf->dat.file.f) {
			fclose (buf->dat.file.f);
		}
		if (buf->dat.file.wasopen && buf->filename) {
			if (!(buf->flags & BUF_F_NO_DELETE)) {
				unlink (buf->filename);
			} else {
				buf->flags |= BUF_F_APPEND;
			}
		}
		break;
	case BUF_TYPE_CONT:
		if (buf->dat.cont.buffer) {
			free (buf->dat.cont.buffer);
		}
		break;
	}
	/* hack - but faster */
	len = (u_int32_t)(size_t)&(((struct buffer*)0)->next) - 
				(u_int32_t)(size_t)&(((struct buffer*)0)->dat);
	bzero (&(buf->dat), len);
	buf->type = buf->otype = type;
	return RERR_OK;
}




int
bufdup (dest, src)
	BUFFER	*dest, src;
{
	struct buffer	*ps, *pd;
	int				ret;

	if (!dest || !src) return RERR_PARAM;
	ps = (struct buffer*)src;
	*dest = bufopen (ps->type, ps->flags);
	if (!*dest) return RERR_INTERNAL;
	pd = (struct buffer*)*dest;
	pd->max_size = ps->max_size;
	pd->max_mem_size = ps->max_mem_size;
	ret = bufcp (*dest, src);
	if (!RERR_ISOK(ret)) {
		bufclose (*dest);
		return ret;
	}
	pd->overflow = ps->overflow;
	pd->otype = ps->otype;
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
