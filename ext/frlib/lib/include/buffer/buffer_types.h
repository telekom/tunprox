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


#ifndef _R__FRLIB_LIB_BUFFER_BUFFER_TYPES_H
#define _R__FRLIB_LIB_BUFFER_BUFFER_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif

#include "errors.h"
#include "slog.h"
#include "textop.h"


#define BUF_PAGE 1024
#define TMP_FILE "/tmp/buffer.XXXXXX"



struct buf_null {
	int		dummy;
};

struct buf_memlist {
	char						*buf;
	uint32_t					len;
	struct buf_memlist	*next;
};

struct buf_mem {
	struct buf_memlist	*buffer,
								*bufferend;
	uint32_t					count;
};

struct buf_file {
	FILE	*f;
	int	wasopen;
};

struct buf_cont {
	char		*buffer;
	uint32_t	bufferlen;
};

struct buf_blocklist {
	char						buf[BUF_PAGE+1];
	struct buf_blocklist	*next;
};

struct buf_block {
	struct buf_blocklist	*buffer,
								*bufferend;
	uint32_t					count;
};




struct buffer{
	int 				type,
						otype;
	uint32_t			flags,
						max_size,
						len,
						max_mem_size;
	int 				overflow;
	char				*filename;
	union {
		struct buf_null	null;
		struct buf_mem		mem;
		struct buf_file	file;
		struct buf_cont	cont;
		struct buf_block	block;
	} 					dat;
	struct buffer	*next,
						*prev;
};




















#endif	/* _R__FRLIB_LIB_BUFFER_BUFFER_TYPES_H */



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
