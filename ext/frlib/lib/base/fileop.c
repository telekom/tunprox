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

#ifdef Linux
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifdef Linux
#include <linux/fs.h>
#endif
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdarg.h>


#include "prtf.h"
#include "errors.h"
#include "fileop.h"
#include "slog.h"
#include "textop.h"
#include "frfinish.h"
#include "uid.h"


static int fop_resolvepath2 (char**, const char*);


ssize_t
fop_fdread (fd, obuf)
	int	fd;
	char	**obuf;
{
	char		*buf=NULL, *p;
	size_t	buflen=0;
	ssize_t	nl, len=0;
	int		eno;

	if (fd < 0) return RERR_PARAM;
	while (1) {
		if ((size_t)len+1024 > buflen) {
			buflen += 1024;
			p = realloc (buf, buflen+1);
			if (!p) {
				if (buf) free (buf);
				return RERR_NOMEM;
			}
			buf = p;
		}
		nl = read (fd, buf+len, 1024);
		if (nl == 0) break;
		if (nl < 0) {
			eno = errno;
			free (buf);
			errno = eno;
			return RERR_SYSTEM;
		}
		len+=nl;
		if (len < 0) {
			/* integer overflow */
			free (buf);
			return RERR_TOO_LONG;
		}
	}
	if ((size_t)len < buflen) {
		p = realloc (buf, len+1);
		if (p) buf = p;
	}
	buf[len] = 0;
	if (obuf) {
		*obuf = buf;
	} else {
		free (buf);
	}

	return len;
}

ssize_t
fop_fnread (fname, obuf)
	const char	*fname;
	char			**obuf;
{
	int		fd;
	ssize_t	ret;

	if (!strcmp (fname, "-")) {
		fd = 0;
	} else {
		fd = open (fname, O_RDONLY);
		if (fd < 0) return RERR_SYSTEM;
	}
	ret = fop_fdread (fd, obuf);
	if (fd > 2) close (fd);
	return ret;
}

ssize_t
fop_fileread (f, obuf)
	FILE	*f;
	char	**obuf;
{
	if (!f) return RERR_PARAM;
	return fop_fdread (fileno(f), obuf);
}



char *
fop_read_fn (fname)
	const char	*fname;
{
	return fop_read_fn2 (fname, NULL);
}

char *
fop_read_fn2 (fname, flen)
	const char	*fname;
	int			*flen;
{
	FILE	*f;
	char	*buf;

	if (!fname) return NULL;
	if (!strcmp (fname, "-")) {
		f = stdin;
	} else {
		f = fopen (fname, "r");
	}
	if (!f) return NULL;
	buf = fop_read_file2 (f, flen);
	if (f != stdin) fclose (f);
	return buf;
}

char *
fop_read_file (f)
	FILE	*f;
{
	return fop_read_file2 (f, NULL);
}

char *
fop_read_file2 (f, flen)
	FILE	*f;
	int	*flen;
{
	char	* buf;
	size_t	buflen, len, nl;

	buflen = len = 0;
	buf = NULL;

	if (!f) return NULL;
	buflen = 1024;
	buf = malloc (buflen+1);
	if (!buf) return NULL;
	while (1) {
		if (len+1024 > buflen) {
			buflen += 1024;
			buf = realloc (buf, buflen+1);
			if (!buf) return NULL;
		}
		nl = fread (buf+len, 1, 1024, f);
		len+=nl;
		if (nl != 1024) {
			if (feof (f)) break;
			if (ferror (f)) {
				free (buf);
				return NULL;
			}
		}
	}
	if (len < buflen) {
		buf = realloc (buf, len+1);
		if (!buf) return NULL;
	}
	buf[len] = 0;
	if (flen) *flen = len;

	return buf;
}


char *
fop_read_fd (fd) 
	int	fd;
{
	return fop_read_fd2 (fd, NULL);
}

char *
fop_read_fd2 (fd, flen)
	int	fd;
	int	*flen;
{
	char		*buf;
	ssize_t	buflen, len, nl;

	buflen = len = 0;
	buf = NULL;

	if (fd < 0) return NULL;
	buflen = 1024;
	buf = malloc (buflen+1);
	if (!buf) return NULL;
	while (1) {
		if (len+1024 > buflen) {
			buflen += 1024;
			buf = realloc (buf, buflen+1);
			if (!buf) return NULL;
		}
		nl = read (fd, buf+len, 1024);
		len+=nl;
		if (nl != 1024) {
			if (nl == -1) {
				free (buf);
				return NULL;
			} else {
				break;
			}
		}
	}
	if (len < buflen) {
		buf = realloc (buf, len+1);
		if (!buf) return NULL;
	}
	buf[len] = 0;
	if (flen) *flen = len;

	return buf;
}

int
fop_mkbasedir (file)
	const char	*file;
{
	char	_buf[256];
	char	*buf = _buf;
	int	ret;
	char	*s;

	if (!file) return RERR_PARAM;
	buf = top_strcpdup (buf, sizeof (_buf), file);
	if (!buf) return RERR_NOMEM;
	s = rindex (buf, '/');
	if (s && s>buf) {
		*s = 0;
		ret = fop_mkdir_rec (buf);
	}
	if (buf != _buf) free (buf);
	return ret;
}

int
fop_mkdir_rec2 (dir)
	const char	*dir;
{
	char	_buf[256];
	char	*buf = _buf;
	int	ret;

	if (!dir) return RERR_PARAM;
	buf = top_strcpdup (buf, sizeof (_buf), dir);
	if (!buf) return RERR_NOMEM;
	ret = fop_mkdir_rec (buf);
	if (buf != _buf) free (buf);
	return ret;
}

int
fop_mkdir_rec (dir)
	char	*dir;
{
	int			ret;
	char			*s;
	struct stat	sbuf;

	if (!dir) return RERR_PARAM;
	if (!*dir || !strcmp (dir, "/")) return RERR_OK;
	/* check wether it already exists */
	if (stat (dir, &sbuf) == 0) {
		if (S_ISDIR (sbuf.st_mode)) return RERR_OK;
		return RERR_NODIR;
	}
	/* do create what's missing */
	s=rindex (dir, '/');
	if (s) {
		*s=0;
		ret = fop_mkdir_rec (dir);
		*s='/';
		if (!RERR_ISOK(ret)) return ret;
	}
	ret = mkdir (dir, 0777);
	if (ret < 0 && errno != EEXIST) {
#if 0
		FRLOGF (LOG_ERR, "error creating directory %s: %d: %s", 
								dir, errno, rerr_getstr3(RERR_SYSTEM));
#endif
		return RERR_SYSTEM;
	}
	return RERR_OK;
}



int
fop_rmdir_rec (dir)
	const char	*dir;
{
	DIR				*dd;
	struct dirent	*dirent;
	char				*dname;
	struct stat		sbuf;

	if (!dir) return RERR_PARAM;
	dd = opendir (dir);
	if (!dd) {
		FRLOGF (LOG_ERR, "error opening dir %s: %s", dir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dirent=readdir(dd))) {
		if (!strcmp (dirent->d_name, ".") || !strcmp (dirent->d_name, "..")) 
			continue;
		dname=malloc (strlen (dir)+strlen (dirent->d_name)+2);
		if (!dname) {
			closedir (dd);
			return RERR_NOMEM;
		}
		sprintf (dname, "%s/%s", dir, dirent->d_name);
		lstat (dname, &sbuf);
		if (S_ISDIR (sbuf.st_mode)) {
			fop_rmdir_rec (dname);
		} else {
			if (unlink (dname) != 0) {
				FRLOGF (LOG_WARN, "error unlinking file %s: %s", dname,
								rerr_getstr3(RERR_SYSTEM));
			}
		}
		free (dname);
	}
	closedir (dd);
	if (rmdir (dir) != 0) {
		FRLOGF (LOG_ERR, "error removing dir %s: %s", dir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return RERR_OK;
}



int
fop_getfilemode (dir, file)
	const char	*dir, *file;
{
	char			buf[256];
	char			*dname;
	struct stat	sbuf;
	int			ret;
	int			needfree = 0;

	if (!dir && !file) return RERR_PARAM;
	if (!dir) {
		dname = (char*)file;
	} else if (!file) {
		dname  = (char*)dir;
	} else {
		dname = a2sprtf (buf, sizeof (buf), "%s/%s", dir, file);
		if (buf != dname) needfree = 1;
	}
	ret = stat (dname, &sbuf);
	if (needfree) free (dname);
	if (ret < 0) return RERR_SYSTEM;
	return (int) sbuf.st_mode;
}

int
fop_lgetfilemode (dir, file)
	const char	*dir, *file;
{
	char			buf[256];
	char			*dname;
	struct stat	sbuf;
	int			ret;
	int			needfree = 0;

	if (!dir && !file) return RERR_PARAM;
	if (!dir) {
		dname = (char*)file;
	} else if (!file) {
		dname  = (char*)dir;
	} else {
		dname = a2sprtf (buf, sizeof (buf), "%s/%s", dir, file);
		if (buf != dname) needfree = 1;
	}
	ret = lstat (dname, &sbuf);
	if (needfree) free (dname);
	if (ret < 0) return RERR_SYSTEM;
	return (int) sbuf.st_mode;
}



int
fop_exist (file)
	const char	*file;
{
	return fop_exist2 (NULL, file);
}

int
fop_issymlink (file)
	const char	*file;
{
	return fop_issymlink2 (NULL, file);
}

int
fop_isdir (file)
	const char	*file;
{
	return fop_isdir2 (NULL, file);
}

int
fop_isdirlink (file)
	const char	*file;
{
	return fop_isdirlink2 (NULL, file);
}

int
fop_exist2 (dir, file)
	const char	*dir, *file;
{
	int	ret;

	ret = fop_getfilemode (dir, file);
	if (!RERR_ISOK(ret)) return 0;
	return 1;
}

int
fop_issymlink2 (dir, file)
	const char	*dir, *file;
{
	int	ret;

	ret = fop_lgetfilemode (dir, file);
	if (!RERR_ISOK(ret)) return 0;
	return (S_ISLNK (ret));
}

int
fop_isdir2 (dir, file)
	const char	*dir, *file;
{
	int	ret;

	ret = fop_lgetfilemode (dir, file);
	if (!RERR_ISOK(ret)) return 0;
	return (S_ISDIR (ret));
}

int
fop_isdirlink2 (dir, file)
	const char	*dir, *file;
{
	int	ret;

	ret = fop_getfilemode (dir, file);
	if (!RERR_ISOK(ret)) return 0;
	return (S_ISDIR (ret));
}

int
fop_resolvepath (obuf, file)
	char			**obuf;
	const char	*file;
{
	char	*buf1, *buf2;
	int	i, ret;

	if (!file || !obuf) return RERR_PARAM;
	for (i=0,buf1=(char*)file; i<255; i++) {
		buf2 = NULL;
		ret = fop_resolvepath2 (&buf2, buf1);
		if (!RERR_ISOK(ret)) {
			if (buf1 != file) free (buf1);
			return ret;
		}
		ret = !strcmp (buf1, buf2);
		if (buf1 != file) free (buf1);
		if (ret) {
			*obuf = buf2;
			return RERR_OK;
		}
		buf1 = buf2;
	}
	return RERR_TOO_MANY_HOPS;
}

static
int
fop_resolvepath2 (obuf, file)
	char			**obuf;
	const char	*file;
{
	char				_buf[1024];
	char				_ibuf[1024];
	char				_cwd[1024];
	char				lbuf[1024];
	char				*buf=_buf, *ibuf=_ibuf, *cwd=_cwd, *p;
	char				*s, *str, *so;
	int				ret, len, num, bufsz, islink;
	char				c;
	struct passwd	pass, *ppass;

	if (!file || !obuf) return RERR_PARAM;
	*obuf = NULL;
	len = strlen (file);
	if (*file == '~') {
		for (s=(char*)file; *s && *s != '/'; s++);
		if (s-file > (ssize_t)sizeof (_buf)) return RERR_TOO_LONG;
		file++;
		if (s == file) {
			str = getenv ("HOME");
			strncpy (cwd, str, sizeof(_cwd));
			cwd[sizeof(_cwd)-1]=0;
		} else {
			strncpy (buf, file, s-file);
			buf[s-file]=0;
			ret = getpwnam_r (buf, &pass, ibuf, sizeof(_ibuf), &ppass);
			if (ret < 0) return RERR_SYSTEM;
			strcpy (cwd, pass.pw_dir);	/* is guaranteed to fit, because cwd 
													is equal in size to ibuf */
		}
		for (file=s; *file == '/'; file++);
	} else if (*file != '/') {
		if (!getcwd (cwd, sizeof(_cwd))) {
			cwd = getcwd (NULL, 0);
			if (!cwd) return RERR_SYSTEM;
		}
		len += strlen (cwd) + 1;
	} else {
		cwd = NULL;
	}
	if (len + 1 > (ssize_t)sizeof(_ibuf)) {
		ibuf = malloc (len+1);
		if (!ibuf) {
			if (cwd && cwd != _cwd) free (cwd);
			return RERR_NOMEM;
		}
	}
	if (cwd) {
		sprintf (ibuf, "%s/%s", cwd, file);
		if (cwd != _cwd) free (cwd);
	} else {
		strcpy (ibuf, file);
	}
	bufsz = sizeof(_buf);
	s = ibuf;
	so = buf;
	len = sizeof(_buf) - 1;
	while (1) {
begloop:
		islink=0;
		for (str = s; *str == '/'; str++);
		if (!*str) break;
		for (s=str; *s && *s != '/'; s++);
		c = *s;
		*s = 0;
		if (!strcmp (str, ".")) {
			*s = c;
			continue;
		}
		if (!strcmp (str, "..")) {
			if (so > buf) {
				for (so--, len++; *so != '/' && so > buf; so--, len++);
			}
			*s = c;
			continue;
		}
		islink = fop_issymlink (ibuf);
		if (islink) {
			num = readlink (ibuf, lbuf, sizeof(lbuf)-1);
			if (num < 0) {
				if (ibuf != _ibuf) free (ibuf);
				if (buf != _buf) free (buf);
				return RERR_SYSTEM;
			}
			lbuf[num]=0;
		} else {
			num = s - str;
		}
cpylink:
		if (islink) {
			if (!strcmp (lbuf, ".")) {
				*s = c;
				goto begloop;
			}
			if (!strcmp (lbuf, "..")) {
				if (so > buf) {
					for (so--, len++; *so != '/' && so > buf; so--, len++);
				}
				*s = c;
				goto begloop;
			}
			if (*lbuf == '/') {
				so = buf;
				len = bufsz;
			}
		}
		if (num+1 > len) {
			bufsz += 1024;
			if (buf == _buf) {
				buf = malloc (bufsz);
				if (!buf) {
					if (ibuf != _ibuf) free (ibuf);
					return RERR_NOMEM;
				}
				*so = 0;
				strcpy (buf, _buf);
				so = buf + (so - _buf);
			} else {
				p = realloc (buf, bufsz);
				if (!p) {
					free (buf);
					if (ibuf != _ibuf) free (ibuf);
					return RERR_NOMEM;
				}
				so = p + (so - buf);
				buf = p;
			}
			len += 1024;
		}
		if (!islink || *lbuf != '/') {
			*so = '/';
			so++; len--;
		}
		if (islink) {
			strncpy (so, lbuf, num);
		} else {
			strncpy (so, str, num);
		}
		so += num;
		len -= num;
		*s = c;
		*so = 0;
		if (islink && fop_issymlink (buf)) {
			islink++;
			if (islink > 256) {
				if (ibuf != _ibuf) free (ibuf);
				if (buf != _buf) free (buf);
				return RERR_INVALID_FILE;
			}
			num = readlink (buf, lbuf, sizeof(lbuf)-1);
			if (num < 0) {
				if (ibuf != _ibuf) free (ibuf);
				if (buf != _buf) free (buf);
				return RERR_SYSTEM;
			}
			lbuf[num]=0;
			if (so > buf) {
				for (so--, len++; *so != '/' && so > buf; so--, len++);
			}
			goto cpylink;
		}
	}
	*so = 0;
	if (ibuf != _ibuf) free (ibuf);
	if (buf == _buf) {
		buf = strdup (_buf);
		if (!buf) return RERR_NOMEM;
	}
	*obuf = buf;
	return RERR_OK;
}


int
fop_getfilelen (size, fname)
	off_t			*size;
	const char	*fname;
{
	struct stat	sbuf;

	if (!fname || !size) return RERR_PARAM;
	if (stat (fname, &sbuf) < 0) {
		if (errno == EPERM) return RERR_FORBIDDEN;
		return RERR_SYSTEM;
	}
	*size = sbuf.st_size;
	return RERR_OK;
}


int
fop_copyfile (src, dest, flags)
	const char	*dest, *src;
	int			flags;
{
	off_t	len, off;
	int	fdi, fdo, ret;
	char	*ptr;

	if (fop_exist (dest)) {
		if (flags & FOP_F_FORCE) {
			unlink (dest);
		} else {
			return RERR_ALREADY_EXIST;
		}
	}
	ret = fop_getfilelen (&len, src);
	if (!RERR_ISOK(ret)) return ret;
	fdi = open (src, O_RDONLY);
	if (fdi < 0) return RERR_SYSTEM;
	fdo = open (dest, O_RDWR|O_EXCL|O_CREAT, 0666);
	if (fdo < 0) {
		close (fdi);
		return RERR_SYSTEM;
	}
	off = 0;
	while (len > 65536) {
		ptr = mmap (NULL, 65536, PROT_READ, MAP_PRIVATE, fdi, off);
		if (!ptr) {
			close (fdi);
			close (fdo);
			return RERR_SYSTEM;
		}
		ret = write (fdo, ptr, 65536);
		munmap (ptr, 65536);
		if (ret < 0) {
			close (fdi);
			close (fdo);
			return RERR_SYSTEM;
		}
		len -= 65536;
		off += 65536;
	}
	ptr = mmap (NULL, len, PROT_READ, MAP_PRIVATE, fdi, off);
	close (fdi);
	if (!ptr) {
		close (fdo);
		return RERR_SYSTEM;
	}
	ret = write (fdo, ptr, len);
	munmap (ptr, len);
	close (fdo);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}




int
fop_creatfile (path, size, flags)
	const char	*path;
	off_t			size;
	int			flags;
{
	int	ret, mode, fd, num, xflags;
	char	_buf[256], *buf, *s;

	if (!path) return RERR_PARAM;
	if (fop_exist (path)) {
		if (!(flags & FOP_CREAT_F_OVERWRITE)) return RERR_ALREADY_EXIST;
		if (unlink (path) < 0) {
			if (errno == EPERM) {
				return RERR_FORBIDDEN;
			} else {
				return RERR_SYSTEM;
			}
		}
	} else {
		s = rindex (path, '/');
		if (s && s > path) {
			buf = top_strcpdup2 (_buf, sizeof (_buf), path, s-path-1);
			if (!buf) return RERR_NOMEM;
			s = buf + (s-path);
			*s = 0;
			if (fop_exist (buf)) {
				if (!fop_isdirlink (buf)) {
					if (buf != _buf) free (buf);
					return RERR_INVALID_PATH;
				}
			} else {
				ret = fop_mkdir_rec (buf);
				if (!RERR_ISOK(ret)) {
					if (buf != _buf) free (buf);
					return ret;
				}
			}
			if (buf != _buf) free (buf);
		}
	}
	if (flags & FOP_CREAT_F_SETMODE) {
		mode = flags & FOP_CREAT_M_MODE;
	} else {
		mode = 0666;
	}
	fd = open (path, O_RDWR | O_CREAT | O_EXCL 
#ifdef Linux
									| O_LARGEFILE
#endif
									, mode);
	if (fd < 0) {
		if (errno == EPERM) {
			return RERR_FORBIDDEN;
		} else {
			return RERR_SYSTEM;
		}
	}
	if (size > 0) {
		if (flags & FOP_CREAT_F_FILL) {
			char	buf2[1024*1024];
			memset (buf2, 0, 1024*1024);
			while (size > 1024*1024) {
				if ((num = write (fd, buf2, 1024*1024)) <= 0) {
					close (fd);
					unlink (path);
					return RERR_SYSTEM;
				}
				size -= num;
			}
			while (size > 0 && (num = write (fd, buf2, size)) > 0) {
				size -= num;
			}
			if (num <= 0) {
				close (fd);
				unlink (path);
				return RERR_SYSTEM;
			}
		} else {
			if (lseek (fd, size-1, SEEK_SET) < size) {
				close (fd);
				unlink (path);
				return RERR_SYSTEM;
			}
			if (write (fd, "\0", 1) < 1) {
				close (fd);
				unlink (path);
				return RERR_SYSTEM;
			}
		}
	}
	close (fd);
	xflags = 0;
	if (flags & FOP_CREAT_F_RMONDIE) xflags = FRFINISH_F_ONDIE;
	if (flags & FOP_CREAT_F_RMONEXIT) xflags |= FRFINISH_F_ONFINISH;
	if (xflags) {
		ret = frregfile (path, xflags);
		if (!RERR_ISOK(ret)) {
			unlink (path);
			return ret;
		}
	}
	return RERR_OK;
}


int64_t
fop_filesize (file)
	const char	*file;
{
	struct stat		sbuf;
	int				fd;
	uint64_t			size;
	unsigned long	nblock;
	off_t				off;
	int				ischar, isdev, isstream;

	if (!file) return RERR_PARAM;
	if (!*file) return RERR_INVALID_FILE;
	if (stat (file, &sbuf) < 0) {
		if (errno == EPERM) return RERR_FORBIDDEN;
		return RERR_SYSTEM;
	}
	ischar = S_ISCHR (sbuf.st_mode);
	isstream = S_ISFIFO (sbuf.st_mode) || S_ISSOCK (sbuf.st_mode);
	isdev = S_ISBLK (sbuf.st_mode);
#ifdef Linux
	/* on linux for some char devices (e.g. raw devices) it is possible
	 *	to get a size - so try it
	 */
	isdev = isdev || ischar;
#else
	isstream = isstream || ischar;
#endif
	if (isstream) {
		/* the size cannot be determined - return -1 */
		return (int64_t) -1;
	} else if (!isdev) {
		/* we have a regular file or a dir - return the inode size */
		return (int64_t) sbuf.st_size;
	}

	/* we have a device */
	fd = open (file, O_RDONLY);
	if (fd < 0) {
		if (errno == EPERM) return RERR_FORBIDDEN;
		return RERR_SYSTEM;
	}
#ifdef Linux
	if (ioctl (fd, BLKGETSIZE64, &size) < 0) {
		if (ioctl (fd, BLKGETSIZE, &nblock) < 0) {
			goto error;
		}
		size = (uint64_t)nblock * 512LL;
	}
	close (fd);
	if (size == 0 && ischar) return -1;
	return (int64_t) size;
error:
	if (ischar) {
		/* a seek always will return a 0 */
		close (fd);
		return -1;
	}
#endif /* Linux */
	if ((off = lseek (fd, 0, SEEK_END)) < 0) {
		close (fd);
		return RERR_SYSTEM;
	}
	close (fd);
	return (int64_t) off;
}

int
fop_getcwd (res, buf, blen)
	char		**res, *buf;
	size_t	blen;
{
	char		_xbuf[256], *xbuf, *s;
	size_t	xblen;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 2) {
		xbuf = _xbuf;
		xblen = sizeof (_xbuf);
	} else {
		xbuf = buf;
		xblen = blen;
	}
	while (1) {
		if (getcwd (xbuf, xblen)) break;
		if (errno != ERANGE) {
			if (xbuf != buf && xbuf != _xbuf) free (xbuf);
			return RERR_SYSTEM;
		}
		if (xbuf != buf && xbuf != _xbuf) {
			xblen += 256;
			s = realloc (xbuf, xblen);
			if (!s) {
				free (xbuf);
				return RERR_NOMEM;
			}
			xbuf = s;
		} else if (xbuf != _xbuf && blen < sizeof (_xbuf)) {
			xbuf = _xbuf;
			xblen = sizeof (_xbuf);
		} else {
			xblen = 512;
			xbuf = malloc (xblen);
			if (!xbuf) return RERR_NOMEM;
		}
	}
	*res = xbuf;
	return RERR_OK;
}

int
fop_sanitize (res, buf, blen, fname, flags)
	char			**res, *buf;
	size_t		blen;
	const char	*fname;
	int			flags;
{
	char			*s2;
	const char	*s;
	size_t		len, lenr;
	int			ret;

	if (!res || !fname) return RERR_PARAM;
	if (*fname == '/' || (*fname == '~' && (flags & FOP_F_NOHOME)) || \
								(*fname != '~' && (flags & FOP_F_NOLOCAL))) {
		*res = (char*)fname;
		return RERR_OK;
	}
	if (*fname == '~') {
		s = index (fname, '/');
		ret = fpw_gethometilde (res, buf, blen, fname);
		if (ret == RERR_NOT_FOUND) {
			*res = (char*)fname;
			return RERR_OK;
		}
		if (!RERR_ISOK(ret)) return ret;
		if (!*res) return RERR_INTERNAL;
		if (!s) return RERR_OK;
		lenr = strlen (*res);
		len = strlen (s) + lenr + 1;
	} else {
		ret = fop_getcwd (res, buf, blen);
		if (!RERR_ISOK(ret)) return ret;
		if (!*res) return RERR_INTERNAL;
		lenr = strlen (*res);
		len = strlen (fname) + lenr + 2;
	}
	if (*res != buf || len > blen) {
		if (*res != buf) {
			s2 = realloc (*res, len);
			if (!s2) {
				free (*res);
				return RERR_NOMEM;
			}
			*res = s2;
		} else {
			*res = s2 = malloc (len);
			if (!s2) return RERR_NOMEM;
			strcpy (s2, buf);
		}
	}
	s2 += lenr;
	if (*fname != '~') {
		*s2 = '/';
		s2++;
	}
	strcpy (s2, s);
	return RERR_OK;
}



int
fop_foreachsubdir (dir, func, arg)
	const char	*dir;
	int 			(*func) (void *, const char *);
	void			*arg;
{
	DIR				*dd;
	struct dirent	*dirent;
	char				*dname;
	char				buf[256];
	int				ret;

	if (!dir || !func) return RERR_PARAM;
	dd = opendir (dir);
	if (!dd) {
		FRLOGF (LOG_ERR, "error opening dir %s: %s", dir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dirent=readdir(dd))) {
		if (!strcmp (dirent->d_name, ".") || !strcmp (dirent->d_name, "..")) 
			continue;
		if (!fop_isdir2 (dir, dirent->d_name)) continue;
		dname = a2sprtf (buf, sizeof (buf), "%s/%s", dir, dirent->d_name);
		if (!dname) {
			closedir (dd);
			return RERR_NOMEM;
		}
		ret = func (arg, dname);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error elaborating subdir (%s): %s", dname,
						rerr_getstr3(ret));
			if (dname != buf) free (dname);
			closedir (dd);
			return ret;
		}
		if (dname != buf) free (dname);
	}
	closedir (dd);
	return RERR_OK;
}

int
fop_foreachsubdir_tout (dir, func, arg, tout)
	const char	*dir;
	int 			(*func) (void *, const char *, tmo_t);
	void			*arg;
	tmo_t			tout;
{
	DIR				*dd;
	struct dirent	*dirent;
	char				*dname;
	char				buf[256];
	int				ret;
	tmo_t				start, now;

	if (!dir || !func) return RERR_PARAM;
	TMO_START(start,tout);
	dd = opendir (dir);
	if (!dd) {
		FRLOGF (LOG_ERR, "error opening dir %s: %s", dir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dirent=readdir(dd))) {
		if (!TMO_CHECK(start,tout)) {
			closedir (dd);
			return RERR_TIMEDOUT;
		}
		if (!strcmp (dirent->d_name, ".") || !strcmp (dirent->d_name, "..")) 
			continue;
		if (!fop_isdir2 (dir, dirent->d_name)) continue;
		dname = a2sprtf (buf, sizeof (buf), "%s/%s", dir, dirent->d_name);
		if (!dname) {
			closedir (dd);
			return RERR_NOMEM;
		}
		ret = func (arg, dname, TMO_GETTIMEOUT(start,tout,now));
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error elaborating subdir (%s): %s", dname,
						rerr_getstr3(ret));
			if (dname != buf) free (dname);
			closedir (dd);
			return ret;
		}
		if (dname != buf) free (dname);
	}
	closedir (dd);
	return RERR_OK;
}


mode_t
fop_getumask ()
{
	char		*buf, *s, *line, *var, *val;
	mode_t	mod;
	int		ret;

	ret = fop_fnread ("/proc/self/status", &buf);
	if (!RERR_ISOK(ret)) goto trad;
	s = buf;
	while ((line=top_getline (&s, 0))) {
		var = top_getfield (&line, ":", 0);
		if (strcasecmp (var, "umask") != 0) continue;
		val = top_getfield (&line, NULL, 0);
		mod = strtol (val, NULL, 8);
		free (buf);
		return mod;
	}
trad:
	mod = umask (0022);
	umask (mod);
	return mod;
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
