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
#include <sys/stat.h>


void*
bufgetref (buf)
	BUFFER	buf;
{
	struct buffer	*p;

	if (!buf) return NULL;
	p = (struct buffer*)buf;
	switch (p->type) {
	case BUF_TYPE_CONT:
		return (void*) &(p->dat.cont.buffer);
	case BUF_TYPE_FILE:
		return (void*) &(p->dat.file.f);
	}
	return NULL;
}


int
bufsetlen (buf, len)
	BUFFER	buf;
	uint32_t	len;
{
	struct buffer	*p;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	p->len = len;
	if (p->type == BUF_TYPE_CONT) {
		p->dat.cont.bufferlen = len+1;
	}
	return RERR_OK;
}


int
bufevallen (buf)
	BUFFER	buf;
{
	struct buffer			*p;
	struct buf_memlist	*qm;
	struct buf_blocklist	*qb;
	uint32_t					sz;
	int						ret, tid;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	switch (p->type) {
	case BUF_TYPE_NULL:
		p->len = 0;
		break;
	case BUF_TYPE_MEM:
		for (sz=0, qm=p->dat.mem.buffer; qm; qm=qm->next) {
			sz += qm->len;
		}
		p->len = sz;
		break;
	case BUF_TYPE_BLOCK:
		for (sz=0, qb=p->dat.block.buffer; qb; qb=qb->next) {
			sz += strlen (qb->buf);
		}
		p->len = sz;
		break;
	case BUF_TYPE_CONT:
		p->len = strlen (p->dat.cont.buffer);
		break;
	case BUF_TYPE_FILE:
		if (!p->dat.file.f) {
			ret = buffileopen (buf);
			if (!RERR_ISOK(ret)) return ret;
		}
		p->len = ftell (p->dat.file.f);
		buffileclose (buf, 0);
		break;
	}
	if ((p->flags & BUF_F_MAXSIZE) && (p->len > p->max_size)) {
		buftrunc (buf, p->max_size);
		p->overflow++;
	}
	if ((p->flags & BUF_F_AUTOCONVERT) &&
			(p->len > p->max_mem_size) && (p->type != BUF_TYPE_FILE)) {
		tid = p->type;
		bufconvert (buf, BUF_TYPE_FILE);
		p->otype = tid;
	} else if ((p->type == BUF_TYPE_FILE) &&
			!(p->flags & BUF_F_NO_RECONVERT) &&
			(p->len <= p->max_mem_size)) {
		bufconvert (buf, p->otype);
	}
	return RERR_OK;
}




int
buffileopen (buf)
	BUFFER	buf;
{
	struct buffer	*p;
	char				*str;
	int				fd, oldmask;
	FILE				*f;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;

	if (p->type != BUF_TYPE_FILE) return RERR_OK;
	if (p->dat.file.f) return RERR_OK;
	if (!p->filename) {
		str = strdup (TMP_FILE);
		if (!str) return RERR_NOMEM;
		oldmask = umask (0077);
		fd = mkstemp (str);
		umask (oldmask);
		if (fd < 0) {
			FRLOGF (LOG_ERR, "error creating temp file >>%s<<: %s",
									str, rerr_getstr3 (RERR_SYSTEM));
			free (str);
			return RERR_SYSTEM;
		}
		f = fdopen (fd, "w+");
		if (!f) {
			FRLOGF (LOG_ERR, "error opening temp file >>%s<<: %s",
									str, rerr_getstr3 (RERR_SYSTEM));
			close (fd);
			free (str);
			return RERR_SYSTEM;
		}
		p->filename = str;
	} else {
		if ((p->flags & BUF_F_APPEND) || p->dat.file.wasopen) {
			f = fopen (p->filename, "a+");
			if (!p->dat.file.wasopen) {
				bufevallen (buf);
			}
		} else {
			f = fopen (p->filename, "w+");
		}
		if (!f) {
			FRLOGF (LOG_ERR, "error opening file >>%s<<: %s",
									p->filename, rerr_getstr3 (RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	}
	p->dat.file.f = f;
	p->dat.file.wasopen = 1;
	return RERR_OK;
}



int
buffileclose (buf, force)
	BUFFER	buf;
	int		force;
{
	struct buffer	*p;
	int				ret;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;

	if (p->type != BUF_TYPE_FILE) return RERR_OK;
	if (!p->dat.file.f) return RERR_OK;
	if (!force && !(p->flags & BUF_F_AUTOCLOSE)) return RERR_OK;
	ret = fclose (p->dat.file.f);
	if (ret != 0) return RERR_SYSTEM;
	p->dat.file.f = NULL;
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
