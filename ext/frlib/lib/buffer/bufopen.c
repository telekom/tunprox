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


static struct buffer	*buf_head = NULL;
static int buf_link (struct buffer*);
static int buf_unlink (struct buffer*);



BUFFER
bufopen (tid, flags)
	int		tid;
	uint32_t	flags;
{
	struct buffer	*buf;
	int				ret;

	if (tid < BUF_TYPE_MIN || tid > BUF_TYPE_MAX) return NULL;
	buf = malloc (sizeof (struct buffer));
	if (!buf) return NULL;
	bzero (buf, sizeof(struct buffer));
	buf->type = tid;
	buf->otype = tid;
	buf->flags = flags;
	ret = buf_link (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return NULL;
	}
	return (void*)buf;
}





int
bufclose (buf)
	BUFFER	buf;
{
	struct buffer			*p;
	struct buf_memlist	*qm;
	struct buf_blocklist	*qb;
	int						ret;

	if (!buf) return RERR_PARAM;
	p = (struct buffer *) buf;
	/* unlink it first */
	ret = buf_unlink (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	switch (p->type) {
	case BUF_TYPE_NULL:
		break;
	case BUF_TYPE_MEM:
		while (p->dat.mem.buffer) {
			qm=p->dat.mem.buffer->next;
			free (p->dat.mem.buffer->buf);
			free (p->dat.mem.buffer);
			p->dat.mem.buffer = qm;
		}
		p->dat.mem.bufferend = NULL;
		p->dat.mem.count = 0;
		break;
	case BUF_TYPE_BLOCK:
		while (p->dat.block.buffer) {
			qb=p->dat.block.buffer->next;
			free (p->dat.block.buffer);
			p->dat.block.buffer = qb;
		}
		p->dat.block.bufferend = NULL;
		p->dat.block.count = 0;
		break;
	case BUF_TYPE_FILE:
		if (p->dat.file.f) fclose (p->dat.file.f);
		if (p->filename) {
			if (!(p->flags & BUF_F_NO_DELETE)) {
				unlink (p->filename);
			}
			free (p->filename);
		}
		p->len = 0;
		break;
	case BUF_TYPE_CONT:
		if (p->dat.cont.buffer && !(p->flags & BUF_F_NO_DELETE)) {
			free (p->dat.cont.buffer);
		}
		p->dat.cont.bufferlen = 0;
		p->len = 0;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	bzero (p, sizeof (struct buffer));
	free (buf);
	return RERR_OK;
}






int
bufcloseall ()
{
	int	ret;

	while (buf_head) {
		ret = bufclose (buf_head);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}




static
int
buf_link (buf)
	struct buffer	*buf;
{
	if (!buf) return RERR_PARAM;
	if (buf->flags & BUF_F_NOLINK) return RERR_PARAM;
	
	buf->next = buf_head;
	if (buf_head) {
		buf->prev = buf_head->prev;
		buf_head->prev = buf;
	} else {
		buf->prev = buf;
	}
	buf_head = buf;
	return RERR_OK;
}


static
int
buf_unlink (buf)
	struct buffer	*buf;
{
	if (!buf) return RERR_PARAM;

	if (!buf->prev) return RERR_PARAM;	/* it is not linked */
	if (buf->next) {
		buf->next->prev = buf->prev;
	}
	if (buf != buf_head) {
		if (buf->prev) buf->prev->next = buf->next;
	} else {
		buf_head = buf->next;
	}
	buf->next = buf->prev = NULL;
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
