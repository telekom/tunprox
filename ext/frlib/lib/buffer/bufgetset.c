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
bufsetflags (buf, flags)
	BUFFER	buf;
	uint32_t	flags;
{
	struct buffer	*p;
	int				tid;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->flags |= flags;
	if ((flags & BUF_F_MAXSIZE) && p->max_size && (p->max_size < p->len) ) {
		buftrunc (buf, p->max_size);
		p->overflow++;
	}
	if ((flags & BUF_F_AUTOCONVERT) && p->type != BUF_TYPE_FILE
			&& p->max_mem_size && (p->max_mem_size < p->len) ) {
		tid = p->type;
		bufconvert (buf, BUF_TYPE_FILE);
		p->otype = tid;
	}
	return RERR_OK;
}



int
bufsetmaxmemsize (buf, max_mem_size)
	BUFFER	buf;
	uint32_t	max_mem_size;
{
	struct buffer	*p;
	int				tid;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->max_mem_size = max_mem_size;
	if (max_mem_size && (p->flags & BUF_F_AUTOCONVERT) &&
			(p->type != BUF_TYPE_FILE) && (p->len > max_mem_size)) {
		tid = p->type;
		bufconvert (buf, BUF_TYPE_FILE);
		p->type = tid;
	}
	return RERR_OK;
}



int
bufsetmaxsize (buf, max_size)
	BUFFER	buf;
	uint32_t	max_size;
{
	struct buffer	*p;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->max_size = max_size;
	if (max_size && (p->flags & BUF_F_MAXSIZE) && (p->len > max_size)) {
		buftrunc (buf, max_size);
		p->overflow++;
	}
	return RERR_OK;
}




int
bufunsetflags (buf, flags)
	BUFFER	buf;
	uint32_t	flags;
{
	struct buffer	*p;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->flags &= ~flags;
	if ((flags & BUF_F_AUTOCONVERT) && !(p->flags & BUF_F_NO_RECONVERT)
			&& (p->type == BUF_TYPE_FILE) && (p->otype != BUF_TYPE_FILE)) {
		bufconvert (buf, p->otype);
	}
	return RERR_OK;
}


int
bufsetfilename (buf, filename)
	BUFFER	buf;
	char		*filename;
{
	struct buffer	*p;

	if (!buf || !filename || !*filename) return RERR_PARAM;
	p = (struct buffer*)buf;
	if (p->filename) return RERR_FORBIDDEN;
	p->filename = strdup (filename);
	if (!p->filename) return RERR_NOMEM;
	return RERR_OK;
}

int
bufresetoverflow (buf)
	BUFFER	buf;
{
	if (!buf) return RERR_PARAM;
	((struct buffer*)buf)->overflow = 0;
	return RERR_OK;
}




uint32_t
bufgetflags (buf)
	BUFFER	buf;
{
	if (!buf) return 0;
	return ((struct buffer*)buf)->flags;
}


uint32_t
bufgetmaxmemsize (buf)
	BUFFER	buf;
{
	if (!buf) return 0;
	return ((struct buffer*)buf)->max_mem_size;
}

uint32_t
bufgetmaxsize (buf)
	BUFFER	buf;
{
	if (!buf) return 0;
	return ((struct buffer*)buf)->max_size;
}


int
bufgettype (buf)
	BUFFER	buf;
{
	if (!buf) return BUF_TYPE_NONE;
	return ((struct buffer*)buf)->type;
}


int
bufgetorigtype (buf)
	BUFFER	buf;
{
	if (!buf) return BUF_TYPE_NONE;
	return ((struct buffer*)buf)->otype;
}


char *
bufgetfilename (buf)
	BUFFER	buf;
{
	if (!buf) return NULL;
	return ((struct buffer*)buf)->filename;
}


int
bufoverflow (buf)
	BUFFER	buf;
{
	if (!buf) return -RERR_PARAM;
	return ((struct buffer*)buf)->overflow;
}

uint32_t
buflen (buf)
	BUFFER	buf;
{
	if (!buf) return 0;
	return ((struct buffer*)buf)->len;
}





uint32_t
bufcount (buf)
	BUFFER	buf;
{
	struct buffer	*p;

	if (!buf) return 0;
	p = (struct buffer*) buf;
	switch (p->type) {
	case BUF_TYPE_NULL:
	case BUF_TYPE_FILE:
		return 1;
	case BUF_TYPE_MEM:
		return p->dat.mem.count;
	case BUF_TYPE_BLOCK:
		return p->dat.block.count;
	case BUF_TYPE_CONT:
		return p->dat.cont.bufferlen;
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
