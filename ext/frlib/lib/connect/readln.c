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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <unistd.h>
#include <sys/poll.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>


#include "errors.h"
#include "readln.h"
#include "slog.h"
#include "prtf.h"
#include "textop.h"
#include "connection.h"
#include "config.h"
#include "frinit.h"
#include "fdready.h"


#define READBUFSIZE 1024
struct readbuf {
	char	buf[READBUFSIZE];
	char	*ptr;
};
static int getreadbuf (struct readbuf**, int fd);

static int dopoll (int fd, int event);
static int read_config ();

static int config_read=0;
static int gtimeout=-1;
static int c_timeout=5;


/* fdcom_readln reads a line terminated by \n or \r\n from the filedescriptor fd
	a \r\n is converted to a \n
	other then that, the line is not modified - the \n is contained in the
	output
	with FDCOM_NOWAIT does not wait for input, but returns imediately
	if there are no data available returning RERR_NODATA
 */
int
fdcom_readln (fd, line, flags)
	int	fd, flags;
	char	**line;
{
	char				*buf;
	char				*ptr = NULL;
	int				num, outlen=0, newlen, ret;
	char				*s=NULL, *out=NULL;
	int				met_eot=0;
	off_t				off, off2;
	struct readbuf	*rb=NULL;
	int				timeout=-1;

	if (fd<0 || !line) return RERR_PARAM;
	if (!config_read) read_config ();
	timeout = (flags & FDCOM_NOWAIT) ? 50 : ((gtimeout > 0) ? 
								(gtimeout * 1000) : gtimeout);
	ret = getreadbuf (&rb, fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting read buffer: %s\n", 
								rerr_getstr3 (ret));
		return ret;
	}
	buf = rb->buf;
	ptr = rb->ptr;

	/* check wether there are still data in buffer */
	if (ptr && *ptr) {
		for (s=ptr; *s && *s!='\n' && (!(flags&FDCOM_EOT) || *s!=FDCOM_EOTCHAR); s++);
		if (*s==FDCOM_EOTCHAR) met_eot=1;
		outlen = (*s=='\n')?(s-ptr+1):(s-ptr);
		out = malloc (outlen+1);
		if (!out) return RERR_NOMEM;
		memcpy (out, ptr, outlen);
		out[outlen]=0;
		rb->ptr = (ptr+=outlen+met_eot);
	}
	/* read new data till we found a newline */
	while ((!s || *s!='\n') && !met_eot) {
#if 0
		if (flags & FDCOM_NOWAIT) {
			ret = fdcom_canread (fd);
			if (!RERR_ISOK(ret)) return ret;
		}
#endif
		if (!(flags & FDCOM_BLOCK)) {
			ret = fd_isready (fd, (tmo_t)timeout*1000000LL);
			if (ret == RERR_TIMEDOUT) return RERR_NODATA;
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error waiting for data: %s", rerr_getstr3(ret));
				return ret;
			}
		}
		num = read (fd, buf, READBUFSIZE-1);
		if (num<0) {
			FRLOGF (LOG_ERR, "error reading from fd (%d): %s", fd,
									rerr_getstr3(RERR_SYSTEM));
			free (out);
			return RERR_SYSTEM;
		}
		buf[num]=0;
		rb->ptr = (ptr=buf);
		if (num==0) {
			/* check wether we are at an end of file */
			off = lseek (fd, 0, SEEK_CUR);
			if (off < 0) {
				if (errno == ESPIPE) continue;	/* we read from a pipe, 
												 *	not a file 
												 */
				FRLOGF (LOG_WARN, "error in lseek: %s", rerr_getstr3(RERR_SYSTEM));
				return RERR_NODATA;
			}
			off2 = lseek (fd, 0, SEEK_END);
			if (off2 < 0) {
				FRLOGF (LOG_ERR, "error in lseek: %s", rerr_getstr3(RERR_SYSTEM));
				return RERR_SYSTEM;
			}
			if (off2 == off) return RERR_NODATA;
			off2 = lseek (fd, off, SEEK_SET);
			if (off2 < 0) {
				FRLOGF (LOG_ERR, "error in lseek: %s", rerr_getstr3(RERR_SYSTEM));
				return RERR_SYSTEM;
			}
			continue;
		}
		for (s=ptr; *s && *s!='\n' && (!(flags&FDCOM_EOT) || *s!=FDCOM_EOTCHAR); s++);
		if (*s==FDCOM_EOTCHAR) met_eot=1;
		newlen = (*s=='\n')?(s-ptr+1):(s-ptr);
		out = realloc (out, outlen+newlen+1);
		if (!out) return RERR_NOMEM;
		memcpy (out+outlen, ptr, newlen);
		outlen+=newlen;
		out[outlen]=0;
		rb->ptr = (ptr+=newlen+met_eot);
	}
	/* delete evtl. \r at end of line */
	if (outlen>=2 && out[outlen-2]=='\r') {
		out[outlen-2]='\n';
		out[outlen-1]=0;
		outlen--;
	}
	*line = out;
	if (met_eot) return RERR_EOT;
	return RERR_OK;
}




/* fdcom_readln2 is like fdcom_readln, but strips whitespaces at begin and end of
	line with FDCOM_STRIPMIDDLE or FDCOM_STRIPALLSPACE even in the middle of
	the line. empty lines and lines only containing whitespaces are
	not returned except when FDCOM_WITHBLANKLN is given.
	with FDCOM_LOGLN, the line (before stripping is logged using
	myprtf (PRT_PROTIN, ...). 
 */
int
fdcom_readln2 (fd, line, flags)
	int	fd, flags;
	char	**line;
{
	int	ret;

	if (!line || fd < 0) return RERR_PARAM;
	ret = fdcom_readln (fd, line, flags);
	if (!RERR_ISOK(ret)) return ret;
#if 0
	if (flags & FDCOM_LOGLN) {
		myprtf (PRT_PROTIN, "< %s", *line);
	}
#endif
	ret = fdcom_stripln (*line, flags);
	if (!RERR_ISOK(ret)) {
		free (*line);
		return ret;
	}
	if (!**line && !(flags & FDCOM_WITHBLANKLN)) {
		free (*line);
		return fdcom_readln2 (fd, line, flags);
	}
	if ((**line == '#') && (flags & FDCOM_SKIPCOMMENT)) {
		free (*line);
		return fdcom_readln2 (fd, line, flags);
	}
	return RERR_OK;
}



#define myisspace(c)	(isspace(c)||((c)=='\n')||((c)=='\r'))
/* fdcom_stripln strips whitespaces at beginning and end of string including newlines 
   with the flag FDCOM_STRIPMIDDLE whitespaces in the middle of the string are
   reduced to a single one.
   with FDCOM_STRIPALLSPACE set, all whitespaces are deleted
 */
int
fdcom_stripln (line, flags)
	char	*line;
	int	flags;
{
	char	*s, *s2;
	int	all;

	if (!line) return RERR_PARAM;
	s=top_skipwhite (line);
	if (s && s!=line) {
		for (s2=line; *s; s++, s2++) *s2=*s;
		*s2=0;
	} else {
		s2=line+strlen(line);
	}
	for (s=s2-1; s>line && myisspace(*s); s--) {}; s++;
	*s=0;
	if ((flags & FDCOM_STRIPMIDDLE) || (flags & FDCOM_STRIPALLSPACE)) {
		all = (flags & FDCOM_STRIPALLSPACE) ? 1 : 0;
		for (s=s2=line; *s; s++) {
			if (myisspace (*s)) {
				if (!all) {
					*s2=' ';
					s2++;
				}
				s=top_skipwhite (s+1);
				if (!s || !*s) break;
			}
			*s2=*s;
			s2++;
		}
		if (s2>line && myisspace (*(s2-1))) s2--;
		*s2=0;
	}
	return RERR_OK;
}








int
fdcom_canread (fd)
	int	fd;
{
	return dopoll (fd, POLLIN);
}

int
fdcom_canwrite (fd)
	int	fd;
{
	return dopoll (fd, POLLOUT);
}


static
int
dopoll (fd, event)
	int	fd, event;
{
	struct pollfd	ufd;
	int				ret;

	if (fd<0) return RERR_PARAM;
	ufd.fd = fd;
	ufd.events = event;
	ufd.revents = 0;
	ret = poll (&ufd, 1, 10);	/* wait max 10 miliseconds */
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error in poll (on fd: %d): %s", fd,
								rerr_getstr3 (RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (ret == 0 || !(ufd.revents & event)) return RERR_NODATA;
	return RERR_OK;
}




static struct readbuf	*readbuf_list=NULL;
static int 					readbuf_num=0;


static
int
getreadbuf (readbuf, fd)
	int				fd;
	struct readbuf	**readbuf;
{
	int	newnum, i;

	if (fd<0 || !readbuf) return RERR_PARAM;
	if (fd>=readbuf_num || !readbuf_list) {
		newnum = fd+1;
		readbuf_list = realloc (readbuf_list, newnum*sizeof (struct readbuf));
		if (!readbuf_list) {
			readbuf_num=0;
			return RERR_NOMEM;
		}
		for (i=readbuf_num; i<newnum; i++) {
			readbuf_list[i].buf[0] = 0;
			readbuf_list[i].ptr = NULL;
		}
		readbuf_num = newnum;
	}
	*readbuf = &(readbuf_list[fd]);
	return RERR_OK;
}



int
fdcom_setreadtimeout (timeout)
	int	timeout;
{
	CF_MAYREAD
	if (timeout < 0) {
		timeout = c_timeout;
	}
	gtimeout = timeout;
	return RERR_OK;
}


static
int
read_config ()
{
	cf_begin_read ();
	c_timeout = cf_atoi (cf_getarr2 ("readln_timeout", fr_getprog(), "5"));
	if (gtimeout < 0) {
		gtimeout = c_timeout;
	}
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
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
