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
bufgetstr2 (buf, str)
	BUFFER	buf;
	char		**str;
{
	struct buffer	*p;
	int				ret;

	if (!buf || !str) return RERR_PARAM;
	p = (struct buffer*)buf;
	*str = malloc (p->len+1);
	if (!*str) return RERR_NOMEM;
	ret = bufnprints (buf, *str, p->len);
	if (!RERR_ISOK(ret)) {
		free (*str);
		*str = NULL;
		return ret;
	}
	return RERR_OK;
}

char *
bufgetstr (buf)
	BUFFER	buf;
{
	char	*str;
	int	ret;

	ret = bufgetstr2 (buf, &str);
	if (!RERR_ISOK(ret)) return NULL;
	return str;
}





int
bufprints (buf, dest)
	BUFFER	buf;
	char		*dest;
{
	return bufnprints (buf, dest, (uint32_t)-1);
}

int
bufnprints (buf, dest, len)
	BUFFER	buf;
	char		*dest;
	uint32_t	len;
{
	struct buffer			*p;
	struct buf_memlist	*qm;
	struct buf_blocklist	*qb;
	int						c;
	uint32_t					num;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	if (len>p->len) len=p->len;
	switch (p->type) {
	case BUF_TYPE_NULL:
		dest[0] = 0;
		break;
	case BUF_TYPE_MEM:
		qm = p->dat.mem.buffer;
		while (len>0 && qm) {
			num = qm->len;
			if (len<num) num=len;
			memcpy (dest, qm->buf, len);
			dest+=num;
			len-=num;
			qm=qm->next;
		}
		*dest=0;
		break;
	case BUF_TYPE_BLOCK:
		qb = p->dat.block.buffer;
		while (len>0 && qb) {
			num = qb->next ? BUF_PAGE : (p->len % BUF_PAGE);
			if (num==0) num=BUF_PAGE;
			if (len<num) num=len;
			memcpy (dest, qb->buf, len);
			dest+=num;
			len-=num;
			qb=qb->next;
		}
		*dest=0;
		break;
	case BUF_TYPE_FILE:
		if (!p->dat.file.f) {
			*dest = 0;
			return RERR_OK;
		}
		rewind (p->dat.file.f);
		while (len>0 && (c=fgetc(p->dat.file.f))!=EOF) {
			*dest=(unsigned char) c;
			dest++;
			len--;
		}
		fseek (p->dat.file.f, 0, SEEK_END);
		break;
	case BUF_TYPE_CONT:
		if (p->dat.cont.buffer) {
			memcpy (dest, p->dat.cont.buffer, len);
		}
		dest[len]=0;
		break;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}








int
bufprintout (buf, f)
	BUFFER	buf;
	FILE		*f;
{
	return bufnprintout (buf, f, (uint32_t)-1);
}

int
bufnprintout (buf, f, len)
	BUFFER	buf;
	FILE		*f;
	uint32_t	len;
{
	struct buffer			*p;
	struct buf_memlist	*qm;
	struct buf_blocklist	*qb;
	int						c;
	uint32_t					num;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	if (len>p->len) len=p->len;
	
	switch (p->type) {
	case BUF_TYPE_NULL:
		break;
	case BUF_TYPE_MEM:
		for (qm=p->dat.mem.buffer; qm && len>0; qm=qm->next) {
			num = qm->len;
			if (len<num) num=len;
			fwrite (qm->buf, num, 1, f);
			len-=num;
		}
		break;
	case BUF_TYPE_BLOCK:
		for (qb=p->dat.block.buffer; qb && len>0; qb=qb->next) {
			num = qb->next ? BUF_PAGE : (p->len % BUF_PAGE);
			if (num==0) num=BUF_PAGE;
			if (len<num) num=len;
			fwrite (qb->buf, num, 1, f);
			len-=num;
		}
		break;
	case BUF_TYPE_FILE:
		if (!p->dat.file.f) break;
		rewind (p->dat.file.f);
		while ((c=fgetc(p->dat.file.f))!=EOF && len>0) {
			fprintf (f, "%c", (unsigned char) c);
			len--;
		}
		fseek (p->dat.file.f, 0, SEEK_END);
		break;
	case BUF_TYPE_CONT:
		if (!p->dat.cont.buffer) break;
		num=p->len;
		if (len<num) num=len;
		fwrite (p->dat.cont.buffer, num, 1, f);
		break;
	default: 
		return RERR_NOT_SUPPORTED;
	}
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
