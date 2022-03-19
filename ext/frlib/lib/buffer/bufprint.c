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
bufprint (buf, str)
	BUFFER		buf;
	const char	*str;
{
	if (!buf) return RERR_PARAM;
	if (!str || !*str) return RERR_OK;
	return bufnprint (buf, str, strlen (str));
}

int
bufnprint (buf, str, len)
	BUFFER		buf;
	const char	*str;
	uint32_t		len;
{
	struct buffer			*p;
	struct buf_blocklist	*q;
	uint32_t					buflen, num, num2;
	int32_t					x;
	int						ret, tid;
	char						*buffer, *s;

	if (!buf) return RERR_PARAM;
	if (!str || len==0) return RERR_OK;
	p = (struct buffer*)buf;

	if ((p->flags & BUF_F_MAXSIZE) && (p->max_size < p->len+len)) {
		len = p->max_size-p->len;
		p->overflow++;
	}
	if (len <= 0) return RERR_OK;
	if ((p->flags & BUF_F_AUTOCONVERT) && p->max_mem_size < p->len+len) {
		tid = p->type;
		ret = bufconvert (buf, BUF_TYPE_FILE);
		if (!RERR_ISOK(ret)) return ret;
		p->otype = tid;
	}
	switch (p->type) {
	case BUF_TYPE_NULL:
		break;
	case BUF_TYPE_MEM:
		s = malloc (len+1);
		if (!s) return RERR_NOMEM;
		memcpy (s, str, len);
		s[len]=0;
		ret = bufputstr (buf, s);
		if (!RERR_ISOK(ret)) {
			free (s);
			return ret;
		}
		break;
	case BUF_TYPE_FILE:
		if (!p->dat.file.f) {
			ret = buffileopen (buf);
			if (!RERR_ISOK(ret)) return ret;
		}
		num = x = fwrite (str, len, 1, p->dat.file.f);
		buffileclose (buf, 0);
		if (x < 0) return RERR_SYSTEM;
		p->len += num;
		if (num < len) return RERR_SYSTEM;
		break;
	case BUF_TYPE_CONT:
		buflen = p->dat.cont.bufferlen;
		if ((p->len + len + 1) > buflen) {
		    if (p->flags & BUF_F_GROW_FAST) {
				if (buflen <= 0) buflen = BUF_PAGE;
				while (buflen < (p->len + len + 1)) {
					buflen *= 2;
				}
		    } else {
				buflen = ((p->len + len + 1) / BUF_PAGE) * BUF_PAGE + BUF_PAGE;
			}
		    buffer = realloc (p->dat.cont.buffer, buflen);
			if (!buffer) return RERR_NOMEM;
			p->dat.cont.buffer = buffer;
			p->dat.cont.bufferlen = buflen;
		}
		memcpy (p->dat.cont.buffer+p->len, str, len);
		p->len += len;
		p->dat.cont.buffer[p->len]=0;
		break;
	case BUF_TYPE_BLOCK:
		if (!p->dat.block.buffer) {
			p->dat.block.buffer = malloc (sizeof (struct buf_blocklist));
			if (!p->dat.block.buffer) return RERR_NOMEM;
			p->dat.block.bufferend = p->dat.block.buffer;
			p->dat.block.count = 1;
			p->dat.block.buffer->buf[0] = 0;
			p->dat.block.buffer->next = NULL;
		}
		q = p->dat.block.bufferend;
		/* write first segment */
		num = (p->len) % BUF_PAGE;
		num2 = BUF_PAGE - num;
		if (num2 > len) num2 = len;
		memcpy (q->buf+num, str, num2);
		q->buf[num+num2]=0;
		p->len+=num2;
		len-=num2;
		str+=num2;
		/* write middle segments */
		while (len > BUF_PAGE) {
			q = malloc (sizeof (struct buf_blocklist));
			if (!q) return RERR_NOMEM;
			q->buf[0] = 0;
			q->next = NULL;
			p->dat.block.bufferend->next = q;
			p->dat.block.bufferend = q;
			p->dat.block.count++;
			memcpy (q->buf, str, BUF_PAGE);
			q->buf[BUF_PAGE] = 0;
			len-=BUF_PAGE;
			str+=BUF_PAGE;
			p->len += BUF_PAGE;
		}
		if (len <= 0) break;
		/* write final segment */
		q = malloc (sizeof (struct buf_blocklist));
		if (!q) return RERR_NOMEM;
		q->buf[0] = 0;
		q->next = NULL;
		p->dat.block.bufferend->next = q;
		p->dat.block.bufferend = q;
		p->dat.block.count++;
		memcpy (q->buf, str, len);
		q->buf[len]=0;
		p->len += len;
		break;
	}
	return RERR_OK;
}





int
bufputstr (buf, str)
	BUFFER	buf;
	char		*str;
{
	if (!buf) return RERR_PARAM;
	if (!str || !*str) return RERR_OK;
	return bufnputstr (buf, str, strlen (str));
}

int
bufnputstr (buf, str, len)
	BUFFER	buf;
	char		*str;
	uint32_t	len;
{
	struct buffer			*p;
	struct buf_memlist	*q;
	int						ret, tid;

	if (!buf) return RERR_PARAM;
	if (!str || len==0) return RERR_OK;
	p = (struct buffer*)buf;

	if ((p->flags & BUF_F_MAXSIZE) && (p->max_size < p->len+len)) {
		len = p->max_size-p->len;
		p->overflow++;
	}
	if (len <= 0) {
		free (str);
		return RERR_OK;
	}
	str[len]=0;
	if ((p->flags & BUF_F_AUTOCONVERT) && p->max_mem_size < p->len+len) {
		tid = p->type;
		ret = bufconvert (buf, BUF_TYPE_FILE);
		if (!RERR_ISOK(ret)) return ret;
		p->otype = tid;
	}

	/* for all buffers apart of mem buffers call bufprint */
	if (p->type != BUF_TYPE_MEM) {
		ret = bufnprint (buf, str, len);
		if (RERR_ISOK(ret)) free (str);
		return ret;
	}

	/* it is a mem buffer */
	if (!p->dat.mem.buffer) {
		q = p->dat.mem.buffer = malloc(sizeof(struct buf_memlist));
	} else {
		q = p->dat.mem.bufferend;
		q->next = malloc(sizeof(struct buf_memlist));
		q = q->next;
	}
	q->next = NULL;
	q->buf = str;
	q->len = len;
	p->len += len;
	p->dat.mem.bufferend = q;

	return RERR_OK;
}	







int
bufputc (buf, c)
	BUFFER		buf;
	const char	c;
{
	char	str[2];

	if (!buf) return RERR_PARAM;
	str[0] = c;
	str[1] = 0;
	return bufnprint (buf, str, 1);
}

int
bufputunic (buf, c)
	BUFFER	buf;
	uint32_t	c;
{
	char	str[8], *s;
	int	ret;

	if (!buf) return RERR_PARAM;
	s = str;
	ret = ucs4utf8_char (&s, c);
	if (!RERR_ISOK(ret)) return ret;
	*s = 0;
	return bufnprint (buf, str, s-str);
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
