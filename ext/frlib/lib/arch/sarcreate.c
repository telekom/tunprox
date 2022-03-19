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
 * Portions created by the Initial Developer are Copyright (C) 2003-2021
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#define _FILE_OFFSET_BITS 64

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

#include <fr/base.h>
#include <fr/mail.h>
#include <fr/cal.h>
#include <fr/thread.h>
#include <fr/buffer.h>

#include "sar.h"


union myhash {
	struct md5sum	md5hash;
};
static int myhash_init (union myhash*, int);
static int myhash_calc (union myhash*,int, const char *, int);
static int myhash_get (union myhash*,int, char *);
static int _sar_zipopen (pid_t*, int, int);
static int _sar_wrfile (struct sar*, struct sar_entry*);
static const char *sar_maycpy (struct sar*, const char*, int);

static int _sar_wrfinish (BUFFER, BUFFER, const char*, size_t);
static int _sar_wrcat (BUFFER, struct sar*);
static int _sar_wrdesc (BUFFER, struct sar*);
static int _sar_wrsig (BUFFER, struct sar*);
static int _sar_wrpad (BUFFER, size_t);
static int _sar_wrentry (BUFFER, struct sar_entry*);
static int mybufdnprtf (BUFFER, int, const char*, const char*, ...);
static int _sar_wrhead (struct sar*);
static int _sar_movearch (struct sar*, size_t);



int
sar_new (sar)
	struct sar	*sar;
{
	if (!sar) return RERR_PARAM;
	*sar = SAR_INIT;
	return RERR_OK;
}

int
sar_bufref (sar, buf)
	struct sar	*sar;
	char			*buf;
{
	if (!sar || !buf) return RERR_PARAM;
	return bufref_addref (&sar->bufref, buf);
}

static
const char *
sar_maycpy (sar, buf, cpy)
	struct sar	*sar;
	const char	*buf;
	int			cpy;
{
	char	*s;
	int	ret;

	if (!sar || !buf) return NULL;
	if (!cpy) return buf;
	s = strdup (buf);
	if (!s) return NULL;
	ret = sar_bufref (sar, s);
	if (!RERR_ISOK(ret)) {
		free (s);
		return NULL;
	}
	return (const char*)s;
}


int
sar_setarch (sar, ar, cpy)
	struct sar	*sar;
	const char	*ar;
	int			cpy;
{
	if (!sar || !ar) return RERR_PARAM;
	sar->ar = sar_maycpy (sar, ar, cpy);
	return sar->ar ? RERR_OK : RERR_NOMEM;
}

int
sar_desc_set_fmagic (sar, fmagic, cpy)
	struct sar	*sar;
	const char	*fmagic;
	int			cpy;
{
	if (!sar || !fmagic) return RERR_PARAM;
	sar->desc.fmagic = sar_maycpy (sar, fmagic, cpy);
	return sar->desc.fmagic ? RERR_OK : RERR_NOMEM;
}

int
sar_desc_set_fver (sar, fver, cpy)
	struct sar	*sar;
	const char	*fver;
	int			cpy;
{
	if (!sar || !fver) return RERR_PARAM;
	sar->desc.fver = sar_maycpy (sar, fver, cpy);
	return sar->desc.fver ? RERR_OK : RERR_NOMEM;
}

int
sar_desc_set_fdesc (sar, fdesc, cpy)
	struct sar	*sar;
	const char	*fdesc;
	int			cpy;
{
	if (!sar || !fdesc) return RERR_PARAM;
	sar->desc.fdesc = sar_maycpy (sar, fdesc, cpy);
	return sar->desc.fdesc ? RERR_OK : RERR_NOMEM;
}



int
sar_desc_add (sar, key, val, cpykey, cpyval)
	struct sar	*sar;
	const char	*key, *val;
	int			cpykey, cpyval;
{
	struct mailhdr_line	*p;

	if (!sar || !key || !val) return RERR_PARAM;
	key = sar_maycpy (sar, key, cpykey);
	val = sar_maycpy (sar, val, cpyval);
	if (!key || !val) return RERR_NOMEM;
	p = realloc (sar->desc.desc.lines, sizeof (struct mailhdr_line) 
							* (sar->desc.desc.numlines+1));
	if (!p) return RERR_NOMEM;
	sar->desc.desc.lines = p;
	p += sar->desc.desc.numlines;
	sar->desc.desc.numlines++;
	p->field = (char*)key;
	p->value = (char*)val;
	return RERR_OK;
}


int
sar_adddir (sar, dir, zip, aa, ht, flags)
	struct sar	*sar;
	const char	*dir;
	int			zip, aa, ht;
	int			flags;
{
	DIR				*d;
	struct dirent	*dent;
	int				ret = RERR_OK;
	char				_buf[128], *buf;

	if (!sar || !dir) return RERR_PARAM;
	d = opendir (dir);
	if (!d) {
		FRLOGF (LOG_ERR, "error opening dir %s: %s", dir,
					rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dent = readdir (d))) {
		if (!strcmp (dent->d_name, ".") || !strcmp (dent->d_name, "..")) continue;
		buf = a2sprtf (_buf, sizeof (_buf), "%s/%s", dir, dent->d_name);
		if (!buf) {
			ret = RERR_NOMEM;
			break;
		}
		ret = sar_addfile (sar, buf, NULL, zip, aa, ht, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error adding file %s: %s", buf,
					rerr_getstr3(ret));
			if (buf != _buf) free (buf);
			break;
		}
		if (buf != _buf) free (buf);
	}
	closedir (d);
	return ret;
}


int
sar_addfile (sar, file, fn, zip, aa, ht, flags)
	struct sar	*sar;
	const char	*file, *fn;
	int			zip, aa, ht;
	int			flags;
{
	struct sar_entry	entry;
	int					ret;

	if (!sar || !file) return RERR_PARAM;
	if (!fn) fn = file;
	if (fop_isdirlink (file)) {
		return sar_adddir (sar, file, zip, aa, ht, flags);
	}
	entry = SAR_ENTRY_INIT;
	entry.fn = sar_maycpy (sar, fn, 1);
	entry.file = sar_maycpy (sar, file, 1);
	entry.zip = zip;
	entry.aa = aa;
	entry.ht = ht;
	entry.flags = flags;
	if (!entry.fn || !entry.file) return RERR_NOMEM;
	ret = TLST_ADD (sar->entries, entry);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding entry (%s): %s", file,
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


//#define SAR_SPACE	(128*1024)
#define SAR_SPACE 0

int
sar_create (sar)
	struct sar	*sar;
{
	struct sar_entry	*entry;
	int					ret;
	size_t				i;

	if (!sar) return RERR_PARAM;
	if (!sar->ar) {
		FRLOGF (LOG_ERR, "no archive name given");
		return RERR_INVALID_FILE;
	}
	ret = sar->fd = open (sar->ar, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error opening archive file (%s): %s",
				sar->ar, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (lseek (sar->fd, SAR_SPACE, SEEK_SET) == (off_t)-1) {
		close (sar->fd);
		sar->fd = -1;
		return RERR_SYSTEM;
	}
	fdprtf (sar->fd, ":BEGIN:\n");
	TLST_FOREACHPTR2 (entry, sar->entries, i) {
		if (entry->flags & SAR_F_VERBOSE) {
			fdprtf (2, "%s <- %s\n", entry->fn, entry->file);
		}
		ret = _sar_wrfile (sar, entry);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error writing entry (%s): %s", entry->fn, 
							rerr_getstr3(ret));
			goto out;
		}
	}
	fdprtf (sar->fd, ":END:\n");
	ret = _sar_wrhead (sar);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing head: %s", rerr_getstr3(ret));
		goto out;
	}
out:
	close (sar->fd);
	sar->fd = -1;
	if (!RERR_ISOK(ret)) {
		unlink (sar->ar);
		return ret;
	}
	return RERR_OK;
}


static
int
_sar_wrfile (sar, entry)
	struct sar			*sar;
	struct sar_entry	*entry;
{
	int				ofd, ifd, ret, num, olen;
	off_t				start, pos;
	struct stat		sbuf;
	char				buf[128];
	char				obuf[sizeof(buf)*2+1];
	union myhash	myhash;
	pid_t				pid = -1;
	int				aa, wstat, i;

	if (!sar || !entry) return RERR_PARAM;
	ofd = sar->fd;
	if (ofd < 0) return RERR_PARAM;
	fdprtf (ofd, ":ENTRY:\n");
	ifd = open (entry->file ?: entry->fn, O_RDONLY);
	if (ifd < 0) return RERR_SYSTEM;
	start = lseek (ofd, 0, SEEK_CUR);
	if (fstat (ifd, &sbuf) < 0) {
		close (ifd);
		return RERR_SYSTEM;
	}
	entry->uid = sbuf.st_uid;
	entry->gid = sbuf.st_gid;
	entry->umode = sbuf.st_mode;
	entry->fflags = sbuf.st_mode & S_IRUSR ? 0 : SAR_FF_R;
	entry->fflags |= sbuf.st_mode & S_IXUSR ? SAR_FF_X : 0;
	entry->sz = sbuf.st_size;
	entry->osz = sbuf.st_size;
	ret = myhash_init (&myhash, entry->ht);
	if (!RERR_ISOK(ret)) goto errout;
	if (entry->ht && (entry->ht & SAR_HT_T_MASK) == SAR_HT_T_PLAIN) {
		while (1) {
			num = read (ifd, buf, sizeof (buf));
			if (num < 0) {
				ret = RERR_SYSTEM;
				goto errout;
			}
			if (num == 0) break;
			ret = myhash_calc (&myhash, entry->ht, buf, num);
			if (!RERR_ISOK(ret)) goto errout;
			if (!entry->aa && !entry->zip) {
				if (fdwrite (ofd, buf, num, 0) != num) {
					ret = RERR_SYSTEM;
					goto errout;
				}
			}
		}
		ret = myhash_get (&myhash, entry->ht, buf);
		if (!RERR_ISOK(ret)) goto errout;
		entry->hash = sar_maycpy (sar, buf, 1);
		if (!entry->hash) {
			ret = RERR_NOMEM;
			goto errout;
		}
		if (!entry->aa && !entry->zip) {
			goto out;
		}
		lseek (ifd, 0, SEEK_SET);
	}
	if (entry->zip) {
		ret = _sar_zipopen (&pid, ifd, entry->zip);
		if (!RERR_ISOK(ret)) goto errout;
		ifd = ret;
	}
	switch (entry->aa) {
	case SAR_AA_NONE:
		aa = TENC_FMT_BIN;
		break;
	case SAR_AA_B64:
		aa = TENC_FMT_B64;
		break;
	case SAR_AA_B32:
		aa = TENC_FMT_B32;
		break;
	case SAR_AA_HEX:
		aa = TENC_FMT_HEX;
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		goto errout;
	}
	while (1) {
		num = read (ifd, buf, sizeof (buf));
		if (num < 0) {
			ret = RERR_SYSTEM;
			goto errout;
		}
		if (num == 0) break;
		if ((entry->ht & SAR_HT_T_MASK) == SAR_HT_T_Z) {
			ret = myhash_calc (&myhash, entry->ht, buf, num);
			if (!RERR_ISOK(ret)) goto errout;
		}
		if (entry->aa) {
			ret = tenc_encode (obuf, buf, num, aa);
			if (!RERR_ISOK(ret)) goto errout;
			olen = aa == TENC_FMT_BIN ? num : (int)strlen (obuf);
			if (fdwrite (ofd, obuf, aa, 0) != olen) {
				ret = RERR_SYSTEM;
				goto errout;
			}
			if ((entry->ht & SAR_HT_T_MASK) == SAR_HT_T_AA) {
				ret = myhash_calc (&myhash, entry->ht, obuf, olen);
				if (!RERR_ISOK(ret)) goto errout;
			}
		} else {
			if (fdwrite (ofd, buf, num, 0) != num) {
				ret = RERR_SYSTEM;
				goto errout;
			}
		}
	}
	ret = myhash_get (&myhash, entry->ht, buf);
	if (!RERR_ISOK(ret)) goto errout;
	entry->hash = sar_maycpy (sar, buf, 1);
	if (!entry->hash) {
		ret = RERR_NOMEM;
		goto errout;
	}
out:
	ret = RERR_SYSTEM;
	/* get size */
	pos = lseek (ofd, 0, SEEK_CUR);
	if (pos < (off_t)-1) goto errout;
	entry->sz = pos - start;
	/* write padding */
	num = entry->sz % 8;
	if (num) {
		num--;
		num = 8 - num;
		for (i=0; i<num; i++) {
			if (fdwrite (ofd, "#", 1, 0) < 1)  goto errout;
		}
		if (fdwrite (ofd, "\n", 1, 0) < 1) goto errout;
	}
	ret = RERR_OK;
	/* fall thru */
errout:
	close (ifd);
	if (pid > 0) {
		kill (pid, SIGKILL);
		waitpid (pid, &wstat, 0);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing file (%s) to archive: %s", entry->file,
				rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}
		
	

static
int
_sar_zipopen (opid, ifd, zip)
	pid_t		*opid;
	int		ifd, zip;
{
	int			fds[2], zip_w, zip_r;
	int			ret;
	pid_t			pid;
	const char *const	xz_arg[] = {"xz", "--format=auto", "--stdout", "-", NULL};
	const char *const	gz_arg[] = {"gzip", "-c", "-", NULL};
	const char *const	bz_arg[] = {"bzip2", "-c", "-", NULL};

#ifdef LINUX
	ret = pipe2 (fds, O_CLOEXEC);
#else
	ret = pipe (fds);
#endif
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot open pipe: %s", rerr_getstr3(ret));
		return ret;
	}
	zip_r = fds[0];
	zip_w = fds[1];
	pid = fork ();
	if (pid < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot fork(): %s", rerr_getstr3(ret));
		return ret;
	}
	if (pid > 0) {
		if (opid) *opid = pid;
		tmo_sleep (10000LL /* 10ms */);
		close (zip_w);
		close (ifd);
		return zip_r;
	}
	if (dup2 (ifd, 0) < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot dup to stdin: %s", rerr_getstr3(ret));
		exit (ret);
	}
	if (dup2 (zip_w, 1) < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot dup to stdout: %s", rerr_getstr3(ret));
		exit (ret);
	}
	close (zip_w);
	close (zip_r);
	ret = RERR_SYSTEM;
	switch (zip) {
	case SAR_ZIP_XZ:
	case SAR_ZIP_LZMA:
		execvp ("xz", (char * const*)xz_arg);
		break;
	case SAR_ZIP_GZ:
		execvp ("gzip", (char * const*)gz_arg);
		break;
	case SAR_ZIP_BZIP2:
		execvp ("bzip2", (char * const*)bz_arg);
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	FRLOGF (LOG_ERR, "error executing unzip command: %s", rerr_getstr3(ret));
	exit (ret);
}
	



static
int
myhash_init (myhash, ht)
	union myhash	*myhash;
	int				ht;
{
	if (!myhash) return RERR_PARAM;
	switch (ht & SAR_HT_MASK) {
	case SAR_HT_NONE:
		break;
	case SAR_HT_MD5:
		myhash->md5hash = MD5SUM_INIT;
		break;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}

static
int
myhash_calc (myhash, ht, buf, len)
	union myhash	*myhash;
	int				ht;
	const char		*buf;
	int				len;
{
	if (!myhash || !buf || len < 0) return RERR_PARAM;
	switch (ht & SAR_HT_MASK) {
	case SAR_HT_NONE:
		return RERR_OK;
	case SAR_HT_MD5:
		return md5sum_calc (&myhash->md5hash, buf, len);
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


static
int
myhash_get (myhash, ht, buf)
	union myhash	*myhash;
	int				ht;
	char				*buf;
{
	if (!myhash || !buf) return RERR_PARAM;
	switch (ht & SAR_HT_MASK) {
	case SAR_HT_NONE:
		*buf = 0;
		return RERR_OK;
	case SAR_HT_MD5:
		return md5sum_get (buf, &myhash->md5hash, TENC_FMT_HEX);
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


static
int
_sar_wrhead (sar)
	struct sar	*sar;
{
	BUFFER	head;
	int		ret;
	size_t	sz;
	char		*shead = NULL;

	if (!sar) return RERR_PARAM;
	head = bufopen (BUF_TYPE_BLOCK, 0);
	if (!head) return RERR_NOMEM;
	
	ret = bufprint (head, ":SAR:82a0fc0101\n");
	if (!RERR_ISOK(ret)) goto errout;

	/* write desc */
	ret = _sar_wrdesc (head, sar);
	if (!RERR_ISOK(ret)) goto errout;

	/* write cat */
	ret = _sar_wrcat (head, sar);
	if (!RERR_ISOK(ret)) goto errout;

	/* write sig */
	ret = _sar_wrsig (head, sar);
	if (!RERR_ISOK(ret)) goto errout;

	sz = buflen (head);
	ret = _sar_movearch (sar, sz);
	if (!RERR_ISOK(ret)) goto errout;

	ret = bufgetstr2 (head, &shead);
	if (!RERR_ISOK(ret)) goto errout;

	lseek (sar->fd, 0, SEEK_SET);
	ret = fdwrite (sar->fd, shead, sz, 0);
	if (!RERR_ISOK(ret)) goto errout;
errout:
	bufclose (head);
	if (shead) free (shead);
	return ret;
}


static
int
_sar_movearch (sar, sz)
	struct sar	*sar;
	size_t		sz;
{
	off_t		dst, src;
	ssize_t	num;
	size_t	bsz, diff;
	char		buf[256];

	if (!sar) return RERR_PARAM;
	if (sz == SAR_SPACE) return RERR_OK;
#if 0
	if (sz < SAR_SPACE) {
		/* move left */
		dst = sz;
		src = SAR_SPACE;
		sz = SAR_SPACE - sz;
		if (sz > sizeof (buf)) sz = sizeof (buf);
		while (1) {
			num = fdreadp (sar->fd, buf, sz, src, 0);
			if (!RERR_ISOK(num)) return num;
			if (num == 0) break;
			lseek (sar->fd, dst, SEEK_SET);
			num = fdwritep (sar->fd, buf, num, dst, 0);
			if (!RERR_ISOK(num)) return num;
			src += num;
			dst += num;
		}
		if (ftruncate (sar->fd, dst) < 0) return RERR_SYSTEM;
	}
#endif
	/* move right */
	diff = sz - SAR_SPACE;
	bsz = (diff > sizeof (buf)) ? sizeof (buf) : diff;
	dst = lseek (sar->fd, diff - bsz, SEEK_END);
	src = lseek (sar->fd, (-1)*bsz, SEEK_END);
	while (src >= SAR_SPACE) {
		lseek (sar->fd, src, SEEK_SET);
		num = fdread (sar->fd, buf, bsz, 0);
		if (num <= 0) return RERR_SYSTEM;
		lseek (sar->fd, dst, SEEK_SET);
		if (fdwrite (sar->fd, buf, num, 0) < num) return RERR_SYSTEM;
		src -= bsz;
		dst -= bsz;
	}
	if (src + bsz > SAR_SPACE) {
		num = src + bsz - SAR_SPACE;
		dst += bsz - num;
		bsz = num;
		lseek (sar->fd, SAR_SPACE, SEEK_SET);
		num = fdread (sar->fd, buf, bsz, 0);
		lseek (sar->fd, dst, SEEK_SET);
		if (fdwrite (sar->fd, buf, num, 0) < num) return RERR_SYSTEM;
	}
	return RERR_OK;
}

static
int
_sar_wrdesc (head, sar)
	BUFFER		head;
	struct sar	*sar;
{
	BUFFER	desc;
	int		ret;
	char		tbuf[64];
	unsigned	i;

	if (!head || !sar) return RERR_PARAM;
	desc = bufopen (BUF_TYPE_BLOCK, 0);
	if (!desc) return RERR_NOMEM;

	ret = bufprintf (desc, "fmagic: %s\n", sar->desc.fmagic ?: "SAR");
	if (!RERR_ISOK(ret)) goto errout;
	ret = bufprintf (desc, "fver: %s\n", sar->desc.fver ?: "0");
	if (!RERR_ISOK(ret)) goto errout;
	ret = bufprintf (desc, "fdesc: %s\n", sar->desc.fdesc ?: "");
	if (!RERR_ISOK(ret)) goto errout;
	cjg_strftime3 (tbuf, sizeof (tbuf), "%r", tmo_now());
	ret = bufprintf (desc, "date: %s\n", tbuf);
	if (!RERR_ISOK(ret)) goto errout;
	for (i=0; i<sar->desc.desc.numlines; i++) {
		sswitch (sar->desc.desc.lines[i].field) {
		sicase ("fmagic")
		sicase ("fver")
		sicase ("fdesc")
		sicase ("date")
			break;
		sdefault
			ret = bufprintf (desc, "%s: %s\n", sar->desc.desc.lines[i].field, sar->desc.desc.lines[i].value);
			if (!RERR_ISOK(ret)) goto errout;
			break;
		} esac
	}
	ret = _sar_wrfinish (head, desc, "DESC", 16);
	if (!RERR_ISOK(ret)) goto errout;
errout:
	bufclose (desc);
	return ret;
}


static
int
_sar_wrcat (head, sar)
	BUFFER		head;
	struct sar	*sar;
{
	BUFFER				cat;
	size_t				j;
	struct sar_entry	*entry;
	int					ret;

	if (!head || !sar) return RERR_PARAM;
	cat = bufopen (BUF_TYPE_BLOCK, 0);
	if (!cat) return RERR_NOMEM;
	TLST_FOREACHPTR2 (entry, sar->entries, j) {
		ret = _sar_wrentry (cat, entry);
		if (!RERR_ISOK(ret)) goto errout;
	}
	ret = _sar_wrfinish (head, cat, "CAT", 32);
	if (!RERR_ISOK(ret)) goto errout;
errout:
	bufclose (cat);
	return ret;
}


static
int
_sar_wrsig (head, sar)
	BUFFER		head;
	struct sar	*sar;
{
	BUFFER	sig;
	int		ret;

	if (!head || !sar) return RERR_PARAM;
	sig = bufopen (BUF_TYPE_BLOCK, 0);
	if (!sig) return RERR_NOMEM;
	/* to be done */
	ret = _sar_wrfinish (head, sig, "SIG", 32);
	if (!RERR_ISOK(ret)) goto errout;
errout:
	bufclose (sig);
	return ret;
}



static
int
_sar_wrfinish (head, sect, name, maxlen)
	BUFFER		head, sect;
	const char	*name;
	size_t		maxlen;
{
	int		ret, n, num, j;
	unsigned	i;
	size_t	sz, max;

	if (!head || !sect || !name || !maxlen) return RERR_PARAM;
	sz = buflen (sect);
	ret = _sar_wrpad (sect, sz);
	if (!RERR_ISOK(ret)) return ret;
	ret = bufprintf (head, ":%s:%n", name, &n);
	if (!RERR_ISOK(ret)) return ret;
	for (i=16; i<=maxlen; i+=8) {
		num = i-n-1;
		if (num<1) continue;
		for (max=10,j=1; j<num; j++,max*=10);
		if (sz < max) break;
	}
	ret = bufprintf (head, "%*llu\n", num, (unsigned long long)sz);
	if (!RERR_ISOK(ret)) return ret;
	ret = bufmv (head, sect);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
_sar_wrpad (buf, sz)
	BUFFER	buf;
	size_t	sz;
{
	size_t	num, i;
	int		ret;

	num = sz % 8;
	if (!num) return RERR_OK;
	num--;
	num = 8 - num;
	for (i=0; i<num; i++) {
		ret = bufputc (buf, '#');
		if (!RERR_ISOK(ret)) return ret;
	}
	ret = bufputc (buf, '\n');
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
_sar_wrentry (buf, entry)
	BUFFER				buf;
	struct sar_entry	*entry;
{
	int	ret;
	char	tbuf[64];

	if (!buf || !entry) return RERR_PARAM;
	ret = mybufdnprtf (buf, 1, "fn", "%s", entry->fn);
	if (!RERR_ISOK(ret)) return ret;
	ret = mybufdnprtf (buf, 0, "sz", "%llu", (unsigned long long) entry->sz);
	if (!RERR_ISOK(ret)) return ret;
	ret = mybufdnprtf (buf, 0, "osz", "%llu", (unsigned long long) entry->osz);
	if (!RERR_ISOK(ret)) return ret;
	switch (entry->zip) {
	case SAR_ZIP_NONE:
		ret = RERR_OK;
		break;
	case SAR_ZIP_XZ:
		ret = mybufdnprtf (buf, 0, "zip", "xz");
		break;
	case SAR_ZIP_LZMA:
		ret = mybufdnprtf (buf, 0, "zip", "lzma");
		break;
	case SAR_ZIP_GZ:
		ret = mybufdnprtf (buf, 0, "zip", "gz");
		break;
	case SAR_ZIP_LIBZ:
		ret = mybufdnprtf (buf, 0, "zip", "libz");
		break;
	case SAR_ZIP_BZIP2:
		ret = mybufdnprtf (buf, 0, "zip", "bz2");
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	if (!RERR_ISOK(ret)) return ret;
	switch (entry->aa) {
	case SAR_AA_NONE:
		ret = RERR_OK;
		break;
	case SAR_AA_B64:
		ret = mybufdnprtf (buf, 0, "aa", "base64");
		break;
	case SAR_AA_B32:
		ret = mybufdnprtf (buf, 0, "aa", "base32");
		break;
	case SAR_AA_HEX:
		ret = mybufdnprtf (buf, 0, "aa", "hex");
		break;
	case SAR_AA_PQUOTE:
		ret = mybufdnprtf (buf, 0, "aa", "quoted-printable");
		break;
	case SAR_AA_UTF7:
		ret = mybufdnprtf (buf, 0, "aa", "utf7");
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	if (!RERR_ISOK(ret)) return ret;
	switch (entry->ht) {
	case SAR_HT_NONE:
		ret = RERR_OK;
		break;
	case SAR_HT_MD5:
		ret = mybufdnprtf (buf, 0, "ht", "md5");
		break;
	case SAR_HT_Z_MD5:
		ret = mybufdnprtf (buf, 0, "ht", "z-md5");
		break;
	case SAR_HT_AA_MD5:
		ret = mybufdnprtf (buf, 0, "ht", "aa-md5");
		break;
	case SAR_HT_SHA1:
		ret = mybufdnprtf (buf, 0, "ht", "sha1");
		break;
	case SAR_HT_Z_SHA1:
		ret = mybufdnprtf (buf, 0, "ht", "z-sha1");
		break;
	case SAR_HT_AA_SHA1:
		ret = mybufdnprtf (buf, 0, "ht", "aa-sha1");
		break;
	case SAR_HT_SHA3:
		ret = mybufdnprtf (buf, 0, "ht", "sha3");
		break;
	case SAR_HT_Z_SHA3:
		ret = mybufdnprtf (buf, 0, "ht", "z-sha3");
		break;
	case SAR_HT_AA_SHA3:
		ret = mybufdnprtf (buf, 0, "ht", "aa-sha3");
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	if (!RERR_ISOK(ret)) return ret;
	if (entry->ht != SAR_HT_NONE) {
		ret = mybufdnprtf (buf, 0, "hash", "%s", entry->hash);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (entry->flags & SAR_F_HONOR_UID) {
		ret = mybufdnprtf (buf, 0, "uid", "%d", entry->uid);
		if (!RERR_ISOK(ret)) return ret;
		ret = mybufdnprtf (buf, 0, "gid", "%d", entry->gid);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (entry->flags & SAR_F_HONOR_UMODE) {
		ret = mybufdnprtf (buf, 0, "umode", "%04o", (unsigned)entry->umode & 07777);
		if (!RERR_ISOK(ret)) return ret;
	} else if (entry->fflags) {
		ret = mybufdnprtf (buf, 0, "ff", "%s%s", 
					((entry->fflags & SAR_FF_X) ? "x" : ""),
					((entry->fflags & SAR_FF_R) ? "r" : ""));
		if (!RERR_ISOK(ret)) return ret;
	}
	if (entry->ctime > 0) {
		ret = cjg_strftime3 (tbuf, sizeof (tbuf), "%D", entry->ctime);
		if (!RERR_ISOK(ret)) return ret;
		ret = mybufdnprtf (buf, 0, "ctime", "%s", tbuf);
		if (!RERR_ISOK(ret)) return ret;
	}
	ret = bufputc (buf, '\n');
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
mybufdnprtf (
	BUFFER		buf,
	int			first,
	const char	*var,
	const char	*fmt,
	...)
{
	char		_tbuf[128], *tbuf;
	char		_ebuf[256], *ebuf;
	va_list	ap;
	int		ret, qlen;

	if (!var || !fmt || !buf) return RERR_PARAM;
	va_start (ap, fmt);
	tbuf = va2sprtf (_tbuf, sizeof (_tbuf), fmt, ap);
	va_end (ap);
	if (!tbuf) return RERR_NOMEM;
	qlen = top_dnquotelen (tbuf);
	if (qlen < (int)sizeof (_ebuf)) {
		ebuf = _ebuf;
	} else {
		ebuf = malloc (qlen+1);
		if (!ebuf) {
			if (tbuf != _tbuf) free (tbuf);
			return RERR_NOMEM;
		}
	}
	ret = top_dnquote (ebuf, qlen+1, tbuf);
	if (tbuf != _tbuf) free (tbuf);
	if (!RERR_ISOK(ret)) {
		if (ebuf != _ebuf) free (ebuf);
		return ret;
	}
	ret = bufprintf (buf, "%s%s=%s", (first?"":","), var, ebuf);
	if (ebuf != _ebuf) free (ebuf);
	return ret;
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
