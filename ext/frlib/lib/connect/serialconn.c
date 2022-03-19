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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <signal.h>



#include "errors.h"
#include "slog.h"
#include "serialconn.h"
#include "connection.h"
#include "prtf.h"


struct sercon {
	int				baud;
	int				flags;
	struct termios	otio;
	unsigned			brkcnt;	/* last system counter */
	int				numbrk;	/* pending breaks */
	int				retbrk;	/* received breaks */
};

static struct sercon	**sercon = NULL;
static int				sercon_max = 0;

#define FLAGS_SET 	0x01
#define FLAGS_ADD		0x02
#define FLAGS_UNSET	0x03
#define SET_BAUD		0x04
#define FLAGS_MASK	0x0f
#define FLAGS_INIT	0x10

static int dosetflags (int, int, int);
static int dosetctrterm (int);
static int getnumbrk (int, int);
static int ttygetnumbrk (unsigned *, int);
static int initbrk (int);



int
sercon_open (dev, baud, flags)
	const char	*dev;
	int			baud, flags;
{
	int				fd, ret;
	struct sercon	*p, **ptr;

	if (!dev) return RERR_PARAM;
	if (baud < B50 || baud > B230400) return RERR_NOT_SUPPORTED;
	fd = open (dev, O_RDWR|O_NOCTTY);
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error opening device >>%s<<: %s", dev,
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (fd >= sercon_max) {
		ptr = realloc (sercon, (fd+16)*sizeof (void*));
		if (!ptr) {
			close (fd);
			return RERR_NOMEM;
		}
		sercon = ptr;
		bzero (sercon+sercon_max, (fd+16-sercon_max)*sizeof (void*));
		sercon_max = fd + 16;
	}
	if (!(sercon[fd])) {
		sercon[fd] = malloc (sizeof (struct sercon));
		if (!(sercon[fd])) {
			close (fd);
			return RERR_NOMEM;
		}
	}
	p = sercon[fd];
	bzero (p, sizeof (struct sercon));
	p->baud = baud;
	p->flags = flags;
	tcgetattr (fd, &(p->otio));
	ret = dosetflags (fd, flags, FLAGS_SET | FLAGS_INIT);		
			/* baud will be set implicitely */
	if (!RERR_ISOK(ret)) {
		sercon_close (fd);
		return ret;
	}
	ret = initbrk (fd);
#if 0
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error initializing break counter: %s",
					rerr_getstr3(ret));
	}
#endif
	return fd;
}


int
sercon_close (fd)
	int	fd;
{
	struct sercon	*p;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	tcsetattr (fd, TCSANOW, &(p->otio));
	free (p);
	sercon[fd] = NULL;
	close (fd);
	return RERR_OK;
}

int
sercon_closeArg (fd, arg)
	int	fd;
	void	*arg;
{
	return sercon_close (fd);
}

int
sercon_setflags (fd, flags)
	int	fd, flags;
{
	return dosetflags (fd, flags, FLAGS_SET);
}

int
sercon_unsetflags (fd, flags)
	int	fd, flags;
{
	return dosetflags (fd, flags, FLAGS_UNSET);
}

int
sercon_setbaud (fd, baud)
	int	fd, baud;
{
	if (baud < B50 || baud > B230400) return RERR_NOT_SUPPORTED;
	return dosetflags (fd, baud, SET_BAUD);
}

int
sercon_isserial (fd)
	int	fd;
{
	return ((fd < sercon_max) && sercon[fd]);
}


ssize_t
sercon_send (fd, buf, buflen)
	int			fd;
	const char	*buf;
	size_t		buflen;
{
	ssize_t	ret;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	ret = write (fd, buf, buflen);
	if (ret < 0) {
		FRLOGF (LOG_NOTICE, "error writing to serial connection: %s",
				rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return ret;
}

ssize_t
sercon_sendArg (fd, buf, buflen, flags, arg)
	int			fd, flags;
	const void	*buf;
	size_t		buflen;
	void			*arg;
{
	return sercon_send (fd, buf, buflen);
}

ssize_t
sercon_recv (fd, buf, buflen)
	int		fd;
	char		*buf;
	size_t	buflen;
{
	struct sercon	*p;
	ssize_t			ret;
	char				*s;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	ret = read (fd, buf, buflen-1);
	if (ret < 0) {
		FRLOGF (LOG_NOTICE, "error reading from serial connection: %s",
				rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	buflen = ret;
	for (s=buf; s-buf<(ssize_t)buflen; s++) {
		if (*s == 0 && (getnumbrk (fd, 1) > 0)) {
			/* fdprtf (1, "<break..>\n"); */
			if (s-buf < (ssize_t)buflen-1) {
				memmove (s, s+1, buflen-(s-buf)-1);
			}
			s--;
			buflen--;
			p->retbrk++;
			if (p->flags & SERCON_F_INTBRKFAKE) {
				/* send a sigint to ourself */
				kill (getpid(), SIGINT);
			}
		}
	}
	buf[buflen]=0;
	return buflen;
}

ssize_t
sercon_recvArg (fd, buf, buflen, flags, arg)
	int		fd, flags;
	void		*buf;
	size_t	buflen;
	void		*arg;
{
	return sercon_recv (fd, buf, buflen);
}

int
sercon_getbrk (fd)
	int	fd;
{
	struct sercon	*p;
	int				num;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	num = p->retbrk;
	p->retbrk = 0;
	return num;
}

int
sercon_clear (fd)
	int	fd;
{
	int	ret;

	ret = sercon_getbrk (fd);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
sercon_sendbrk (fd)
	int	fd;
{
	int	ret;

	ret = tcsendbreak (fd, 0);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}


/* **************************
 * static functions
 * **************************/

static
int
dosetflags (fd, flags, what)
	int	fd, flags, what;
{	
	struct sercon	*p;
	struct termios	ntio, ctio;
	int				baud;
	int				dosetterm=0, ret;
	int				dounblock = 0, doblock = 0;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	baud = p->baud;
	switch (what & FLAGS_MASK) {
	case FLAGS_SET:
		if (flags & SERCON_M_CHGMASK) dosetterm=1;
		break;
	case FLAGS_ADD:
		if (flags & SERCON_M_CHGMASK) dosetterm=1;
		flags |= p->flags;
		break;
	case FLAGS_UNSET:
		flags = p->flags & ~flags;
		break;
	case SET_BAUD:
		baud = flags;
		flags = p->flags;
		break;
	default:
		return RERR_PARAM;
	}
	if (what & FLAGS_INIT) {
		bzero (&ntio, sizeof (struct termios));
	} else {
		if (tcgetattr (fd, &ntio) < 0) goto error;
	}
	if ((flags & SERCON_F_NOBLOCK) && !(p->flags & SERCON_F_NOBLOCK)) {
		dounblock = 1;
	} else if (!(flags & SERCON_F_NOBLOCK) && (p->flags & SERCON_F_NOBLOCK)) {
		doblock = 1;
	}
	if (!((flags & SERCON_F_TERMCTRL) && !(p->flags & SERCON_F_TERMCTRL))) {
		dosetterm = 0;
	}

	/* set term flags */
	//ntio.c_cflag = baud | CS8 | CLOCAL | CREAD;
	ntio.c_cflag = baud | CS8 | PARODD | PARENB;
	if (flags & SERCON_F_RTSCTS) ntio.c_cflag |= CRTSCTS;
	//ntio.c_iflag = IGNPAR;
	ntio.c_iflag = 0;
	if (flags & SERCON_F_INTBRK) ntio.c_iflag |= BRKINT;
	if (flags & SERCON_F_NOBRK) ntio.c_iflag |= IGNBRK;
	ntio.c_lflag = 0;
	if (flags & SERCON_F_ECHO) ntio.c_lflag = ECHO;
	ntio.c_oflag = 0;
	//ntio.c_cc[VTIME] = 0;
	//ntio.c_cc[VMIN] = 1;
	if (flags & FLAGS_INIT) {
		tcflush (fd, TCIFLUSH);
	}
	bzero (&ctio, sizeof (struct termios));
	errno=0;
	if (tcsetattr (fd, TCSANOW, &ntio) < 0) goto error;
	if (tcgetattr (fd, &ctio) < 0) goto error;
	if (ntio.c_cflag != ctio.c_cflag || ntio.c_iflag != ctio.c_iflag || \
			ntio.c_oflag != ctio.c_oflag || ntio.c_lflag != ctio.c_lflag) {
error:
		FRLOGF (LOG_WARN, "error configuring device: %s "
					"{cflags: %o - %o, iflags: %o - %o, lflags: %o - %o, oflags: %o - %o}",
					((errno!=0)?rerr_getstr3(RERR_SYSTEM):
					"only some value could be set"),
					ntio.c_cflag, ctio.c_cflag, ntio.c_iflag, ctio.c_iflag,
					ntio.c_lflag, ctio.c_lflag, ntio.c_oflag, ctio.c_oflag);
	}
	p->flags = flags;
	p->baud = baud;

	/* set / unset blocking */
	if (dounblock) {
		ret = conn_setNonBlocking (fd);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error setting fd non blocking: %s",
						rerr_getstr3(ret));
			p->flags &= ~SERCON_F_NOBLOCK;
		}
	} else if (doblock) {
		ret = conn_setBlocking (fd);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error setting fd blocking: %s",
						rerr_getstr3(ret));
			p->flags |= SERCON_F_NOBLOCK;
		}
	}

	/* set controlling terminal */
	if (dosetterm) {
		ret = dosetctrterm (fd);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error setting controlling terminal: %s",
						rerr_getstr3(ret));
			p->flags &= ~SERCON_F_TERMCTRL;
		}
	}
	return RERR_OK;
}



static
int
dosetctrterm (fd)
	int	fd;
{
#if 0
	pid_t	pgrp;
	int	arg;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	pgrp = getpgrp ();
	if (pgrp < 0) {
		FRLOGF (LOG_ERR, "error getting process group: %s",
					rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	setsid ();
	if (fork() > 0) exit (0);
	arg = 0;
	if (ioctl (fd, TIOCSCTTY, arg) < 0) {
		FRLOGF (LOG_WARN, "error setting tty as controlling terminal: %s",
					rerr_getstr3(RERR_SYSTEM));
	}
	if (tcsetpgrp (fd, pgrp) < 0) {
		FRLOGF (LOG_WARN, "error setting process group the foreground process "
					"group of the terminal: %s",
					rerr_getstr3(RERR_SYSTEM));
	}
#endif
	return RERR_OK;
}


static
int
getnumbrk (fd, dec)
	int	fd, dec;
{
	struct sercon	*p;
	unsigned			cnt;
	int				ret, num;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	ret = ttygetnumbrk (&cnt, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (cnt < p->brkcnt) {
		num = (int)(unsigned)(((uint64_t)cnt) + (1ULL<<32) - ((uint64_t)p->brkcnt));
	} else {
		num = (int)(cnt - p->brkcnt);
	}
	p->brkcnt = cnt;
	num = (p->numbrk += num);
	if (dec && p->numbrk) p->numbrk--;
	return num;
}

static
int
initbrk (fd)
	int	fd;
{
	struct sercon	*p;
	unsigned			cnt;
	int				ret;

	if (!((fd < sercon_max) && sercon[fd])) return RERR_INVALID_FILE;
	p = sercon[fd];
	p->brkcnt = 0;
	p->numbrk = 0;
	p->retbrk = 0;
	ret = ttygetnumbrk (&cnt, fd);
	if (!RERR_ISOK(ret)) return ret;
	p->brkcnt = cnt;
	return RERR_OK;
}

static
int
ttygetnumbrk (out, fd)
	int		fd;
	unsigned	*out;
{
	struct serial_icounter_struct	cnt;
	int									ret;

	ret = ioctl (fd, TIOCGICOUNT, &cnt);
	if (ret < 0) return RERR_SYSTEM;
	if (out) *out = (unsigned)cnt.brk;
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
