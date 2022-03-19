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
#include <signal.h>
#include <errno.h>
extern int errno;


#include <fr/base.h>
#include <fr/mail.h>
#include <fr/cal.h>
#include <fr/thread.h>

#include "sar.h"


static int read_head (struct sar*, int, char**, size_t*);
static void sar_hfree (struct sar*);
static int parse_desc (struct sar*, char*);
static int parse_cat (struct sar*, char*, int);
static int parse_catentry (struct sar_entry*, struct dn*, int);
static int verify_cat (struct sar*, size_t, char**, size_t*, int);
static int verify_catentry (struct sar*, struct sar_entry*, size_t, char**, size_t*, int);
static int verify_hash (struct sar*, struct sar_entry*, int);
static int verify_sig (struct sar*, char*, size_t, char*, int);
static int _sar_fopen (struct sar_file*, struct sar*, struct sar_entry*, int);
static int _sar_chkfile (char *file, int flags);
static int calc_md5 (struct sar_file*, char*);
static int calc_sha1 (struct sar_file*, char*);
static int calc_sha3 (struct sar_file*, char*);
static int _sar_doread (struct sar_file*, char*, size_t);
static int _sar_run_wchild (struct sar_file*, int);
static int _sar_run_unzip (int, int, int);
static int _sar_fopen_unzip (struct sar_file*);


int
sar_open (sar, file, flags)
	struct sar	*sar;
	const char	*file;
	int			flags;
{
	int			ret;
	char			*maddr = NULL;
	size_t		mapsz;
	struct stat	sb;

	if (!sar || !file) return RERR_PARAM;
	*sar = SAR_INIT;
	sar->fd = open (file, O_RDONLY);
	if (sar->fd < 0) {
		FRLOGF (LOG_ERR, "cannot open file >>%s<<: %s", file, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	FRLOGF (LOG_DEBUG, "file (%s) opened readonly with fd=%d", file, sar->fd);
	if (fstat(sar->fd, &sb) == -1) {
		FRLOGF (LOG_ERR, "error getting file size: %s", rerr_getstr3(RERR_SYSTEM));
		close (sar->fd);
		*sar = SAR_INIT;
		return RERR_SYSTEM;
	}
	sar->fsz = sb.st_size;
	sar->ar = strdup (file);
	if (!sar->ar) {
		close (sar->fd);
		*sar = SAR_INIT;
		return RERR_NOMEM;
	}
	ret = bufref_addref (&sar->bufref, (char*)sar->ar);
	if (!RERR_ISOK(ret)) {
		close (sar->fd);
		free ((char*)sar->ar);
		*sar = SAR_INIT;
		return RERR_NOMEM;
	}
	ret = read_head (sar, flags, &maddr, &mapsz);
	if (maddr) {
		munmap (maddr, mapsz);
	}
	if (!RERR_ISOK(ret)) {
		sar_hfree (sar);
		FRLOGF (LOG_ERR, "error reading sar header of file >>%s<<: %s", file,
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
read_head (sar, flags, maddr, mapsz)
	struct sar	*sar;
	int			flags;
	char			**maddr;
	size_t		*mapsz;
{
	ssize_t	psize, msize, start;
	char		*addr, *s, *s2;
	int		major, ret;
	int64_t	len;
	char		*desc, *cat, *sig;

	if (!maddr) return RERR_PARAM;	
	*maddr = NULL;
	if (!sar || !mapsz) return RERR_PARAM;	
	psize = sysconf(_SC_PAGE_SIZE);
	if (psize <= 0) psize = 4096;
	while (psize <= 4096) psize*=2;
	sar->psize = psize;
	msize=psize;
	addr = mmap (NULL, msize, PROT_READ, MAP_PRIVATE, sar->fd, 0);
	if (addr == MAP_FAILED) return RERR_SYSTEM;
	*maddr = addr;
	*mapsz = msize;
	if (!strncasecmp (addr, ":SAR:82a0fc", 11)) {
		start = 0;
	} else if (!strncasecmp (addr+16, ":SAR:82a0fc", 11)) {
		start = 16;
	} else if (!strncasecmp (addr+32, ":SAR:82a0fc", 11)) {
		start = 32;
	} else if (!strncasecmp (addr+64, ":SAR:82a0fc", 11)) {
		start = 64;
	} else if (!strncasecmp (addr+128, ":SAR:82a0fc", 11)) {
		start = 128;
	} else {
		return RERR_INVALID_FILE;
	}
	s = addr + start;
	major = HEX2NUM(s[11]) << 4 | HEX2NUM(s[12]);
	if (major != 1) {
		FRLOGF (LOG_WARN, "version %d is not supported", major);
		return RERR_NOT_SUPPORTED;
	}
	if (s[15] == '\n') {
		s+=16;
	} else {
		for (s+=11; *s && *s!='\n' && (s-addr < msize-1); s++);
		if (*s)s++;
	}
	if (strncasecmp (s, ":DESC:", 6) != 0) return RERR_INVALID_FILE;
	len = top_atoi64 (top_skipwhite (s+6));
	if (len < 0) return RERR_INVALID_FILE;
#define MAYREMAP do { \
	if (((len+15) & ~0x7) + 64 + (s-addr) < msize) { \
		size_t myoff = s-addr; \
		size_t nsize = msize + ((len + 73 + psize) & ~(psize-1)); \
		addr = mremap (addr, msize, nsize, MREMAP_MAYMOVE); \
		if (addr == MAP_FAILED) return RERR_SYSTEM; \
		msize = nsize; \
		*maddr = addr; \
		*mapsz = msize; \
		s = addr + myoff; \
	} } while (0)
#define JMPBEGSECT do { \
	if (s[15] == '\n') { \
		s+=16; \
	} else if (s[23] == '\n') { \
		s+=24; \
	} else if (s[31] == '\n') { \
		s+=32; \
	} else { \
		for (s+=11; *s && *s!='\n' && (s-addr < msize-1); s++); \
		if (*s)s++; \
	} } while (0)
	MAYREMAP;
	JMPBEGSECT;
	desc = malloc (len+1);
	if (!desc) return RERR_NOMEM;
	memcpy (desc, s, len);
	desc[len]=0;
	ret = bufref_addref (&sar->bufref, desc);
	if (!RERR_ISOK(ret)) {
		free (desc);
		return ret;
	}
#define JMPNEXTSECT do { \
		for (s+=len; (*s=='#' || *s=='\n') && (s-addr < msize); s++); \
	} while (0)
	JMPNEXTSECT;
	if (strncasecmp (s, ":CAT:", 5) != 0) return RERR_INVALID_FILE;
	len = top_atoi64 (top_skipwhite (s+5));
	if (len < 0) return RERR_INVALID_FILE;
	MAYREMAP;
	JMPBEGSECT;
	cat = malloc (len+1);
	if (!cat) return RERR_NOMEM;
	memcpy (cat, s, len);
	cat[len]=0;
	ret = bufref_addref (&sar->bufref, cat);
	if (!RERR_ISOK(ret)) {
		free (desc);
		return ret;
	}
	JMPNEXTSECT;
	while (1) {
		if (*s != ':') return RERR_INVALID_FILE;
		if (!strncasecmp (s, ":BEGIN:", 7)) break;
		if (!strncasecmp (s, ":SIG:", 5)) {
			size_t siglen=(s-addr) - start;
			len = top_atoi64 (top_skipwhite (s+5));
			if (len < 0) return RERR_INVALID_FILE;
			MAYREMAP;
			JMPBEGSECT;
			sig = malloc (len+1);
			if (!sig) return RERR_NOMEM;
			memcpy (sig, s, len);
			sig[len]=0;
			ret = verify_sig (sar, addr+start, siglen, sig, flags);
			free (sig);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "error in signature verification: %s",
						rerr_getstr3(ret));
				return ret;
			}
			JMPNEXTSECT;
			continue;
		}
		/* unknown section - skip */
		for (s2=s+1; *s2 != ':' && (s2-s) < 12; s2++);
		if (*s2 != ':') return RERR_INVALID_FILE;
		len = top_atoi64 (top_skipwhite (s2+1));
		if (len < 0) return RERR_INVALID_FILE;
		MAYREMAP;
		JMPBEGSECT;
		JMPNEXTSECT;
	}
	if (strncasecmp (s, ":BEGIN:", 7) != 0) return RERR_INVALID_FILE;
	start = s-addr;
	munmap (addr, msize);
	*maddr = NULL;
#undef MAYREMAP
#undef JMPBEGSECT
#undef JMPNEXTSECT
	ret = parse_desc (sar, desc);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing description section: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = parse_cat (sar, cat, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing catalogue section: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = verify_cat (sar, start, maddr, mapsz, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error verifying catalogue section: %s",
					rerr_getstr3(ret));
		return ret;
	}
	/* we are done */
	return RERR_OK;
}
	
	
static
int
parse_desc (sar, desc)
	struct sar	*sar;
	char			*desc;
{
	int	ret, i;
	char	*s;

	if (!sar || !desc) return RERR_PARAM;
	ret = mail_hdr_parse (&sar->desc.desc, NULL, desc, 
					MHPARSE_F_IGN_EMPTYLINES|MHPARSE_F_ALLHDRS);
	if (!RERR_ISOK(ret)) return ret;
	for (i=0; (unsigned)i<sar->desc.desc.numlines; i++) {
		bufref_ref (&sar->bufref, sar->desc.desc.lines[i].field);
		bufref_ref (&sar->bufref, sar->desc.desc.lines[i].value);
		sswitch (sar->desc.desc.lines[i].field) {
		sicase ("fmagic")
			sar->desc.fmagic = sar->desc.desc.lines[i].value;
			break;
		sicase ("fdesc")
			sar->desc.fdesc = sar->desc.desc.lines[i].value;
			break;
		sicase ("fver")
			sar->desc.fver = sar->desc.desc.lines[i].value;
			break;
		sicase ("creator")
			sar->desc.creator = sar->desc.desc.lines[i].value;
			break;
		sicase ("ca")
			sar->desc.ca = sar->desc.desc.lines[i].value;
			break;
		sicase ("date")
			s = sar->desc.desc.lines[i].value;
			if (!isdigit (*s)) {
				ret = cjg_strptime3 (&sar->desc.date, s, "%r");
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_NOTICE, "error parsing date %s: %s", s,
									rerr_getstr3(ret));
				}
			} else if (index (s, 'D')) {
				ret = cjg_strptime3 (&sar->desc.date, s, "%D");
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_NOTICE, "error parsing date %s: %s", s,
									rerr_getstr3(ret));
				}
			} else if (index (s, 'T')) {
				ret = cjg_strptime3 (&sar->desc.date, s, "%e");
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_NOTICE, "error parsing date %s: %s", s,
									rerr_getstr3(ret));
				}
			} else {
				FRLOGF (LOG_NOTICE, "unknown date format %s", s);
			}
			break;
		} esac;
	}
	return RERR_OK;
}

static
int
parse_cat (sar, cat, flags)
	struct sar	*sar;
	char			*cat;
	int			flags;
{
	char					*line;
	struct sar_entry	entry;
	struct dn			dn;
	int					lnum, ret;

	if (!sar || !cat) return RERR_PARAM;
	lnum=0;
	while ((line = top_getline (&cat, 0))) {
		lnum++;
		FRLOGF (LOG_VERB, "split cat entry %d >>%s<<", lnum, line);
		entry = (struct sar_entry) { .fn = NULL };
		ret = dn_split_inplace (&dn, line);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error splitting cat entry %d: %s", lnum,
					rerr_getstr3(ret));
			return ret;
		}
		ret = parse_catentry (&entry, &dn, flags);
		dn_hfree (&dn);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error parsing cat entry %d (%s): %s", lnum,
					entry.fn ? entry.fn : "", rerr_getstr3(ret));
			return ret;
		}
		ret = TLST_ADD (sar->entries, entry);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error adding file entry %d (%s): %s", 
						lnum, entry.fn, rerr_getstr3(ret));
			return ret;
		}
		bufref_ref (&sar->bufref, entry.fn);
		if (entry.hash) bufref_ref (&sar->bufref, entry.hash);
	}
	return RERR_OK;
}

static
int
parse_catentry (entry, dn, flags)
	struct sar_entry	*entry;
	struct dn			*dn;
	int					flags;
{
	const char			*s;
	int					ret, umask;

	*entry = SAR_ENTRY_INIT;
	entry->fn = dn_getpart (dn, "fn");
	if (!entry->fn) {
		FRLOGF (LOG_NOTICE, "no function name in entry");
		return RERR_INVALID_ENTRY;
	}
	entry->sz = top_atoi64 (dn_getpart (dn, "sz"));
	entry->osz = top_atoi64 (dn_getpart (dn, "osz"));
	s = dn_getpart (dn, "z");
	if (!s) s = dn_getpart (dn, "zip");
	if (!s) {
		entry->zip = SAR_ZIP_NONE;
	} else {
		sswitch (s) {
		sicase ("xz")
			entry->zip = SAR_ZIP_XZ;
			break;
		sicase ("lzma")
			entry->zip = SAR_ZIP_LZMA;
			break;
		sicase ("gzip")
		sicase ("gz")
			entry->zip = SAR_ZIP_GZ;
			break;
		sicase ("libz")
			entry->zip = SAR_ZIP_LIBZ;
			break;
		sicase ("bzip2")
			entry->zip = SAR_ZIP_BZIP2;
			break;
		sicase ("none")
			entry->zip = SAR_ZIP_NONE;
			break;
		sdefault
			FRLOGF (LOG_NOTICE, "unknown compression algorithm %s", s);
			return RERR_INVALID_ENTRY;
		} esac;
	}
	s = dn_getpart (dn, "aa");
	if (!s) {
		entry->aa = SAR_AA_NONE;
	} else {
		sswitch (s) {
		sicase ("none")
			entry->aa = SAR_AA_NONE;
			break;
		sicase ("base64")
		sicase ("b64")
			entry->aa = SAR_AA_B64;
			break;
		sicase ("base32")
		sicase ("b32")
			entry->aa = SAR_AA_B32;
			break;
		sicase ("hex")
			entry->aa = SAR_AA_HEX;
			break;
		sicase ("pquote")
		sicase ("quoted-printable")
			entry->aa = SAR_AA_PQUOTE;
			break;
		sicase ("utf7")
		sicase ("utf-7")
			entry->aa = SAR_AA_UTF7;
			break;
		sdefault
			FRLOGF (LOG_NOTICE, "unknown ascii armor algorithm %s", s);
			return RERR_INVALID_ENTRY;
		} esac;
	}
	s = dn_getpart (dn, "ht");
	if (!s) {
		entry->ht = SAR_HT_NONE;
	} else {
		sswitch (s) {
		sicase ("none")
			entry->ht = SAR_HT_NONE;
			break;
		sicase ("md5")
			entry->ht = SAR_HT_MD5;
			break;
		sicase ("z-md5")
			entry->ht = SAR_HT_Z_MD5;
			break;
		sicase ("aa-md5")
			entry->ht = SAR_HT_AA_MD5;
			break;
		sicase ("sha1")
			entry->ht = SAR_HT_SHA1;
			break;
		sicase ("z-sha1")
			entry->ht = SAR_HT_Z_SHA1;
			break;
		sicase ("aa-sha1")
			entry->ht = SAR_HT_AA_SHA1;
			break;
		sicase ("sha3")
			entry->ht = SAR_HT_SHA3;
			break;
		sicase ("z-sha3")
			entry->ht = SAR_HT_Z_SHA3;
			break;
		sicase ("aa-sha3")
			entry->ht = SAR_HT_AA_SHA3;
			break;
		sdefault
			FRLOGF (LOG_NOTICE, "unknown hash type %s", s);
			return RERR_INVALID_ENTRY;
		} esac;
	}
	if (entry->ht != SAR_HT_NONE) {
		s = dn_getpart (dn, "h");
		if (!s) s = dn_getpart (dn, "hash");
		if (!s) {
			if (!(flags & SAR_F_IGN_HASH)) {
				FRLOGF (LOG_ERR, "no hash in entry");
				return RERR_INVALID_ENTRY;
			}
			FRLOGF (LOG_NOTICE, "no hash in entry");
		} else {
			entry->hash = s;
		}
	} else if (flags & SAR_F_FORCE_HASH) {
		FRLOGF (LOG_NOTICE, "no hash but forced");
		return RERR_INVALID_ENTRY;
	}
	entry->fflags = 0;
	umask = fop_getumask();
	entry->umode = 0666 & ~umask;
	s = dn_getpart (dn, "ff");
	if (s) {
		for (; s && *s; s++) {
			switch (*s) {
			case 'x': case 'X':
				entry->fflags |= SAR_FF_X;
				entry->umode |= 0111; 
				break;
			case 'r': case 'R':
				entry->fflags |= SAR_FF_R;
				entry->umode &= ~0222;
				break;
			}
		}
		entry->umode &= ~fop_getumask();
	}
	if (!s || (flags & SAR_F_HONOR_UMODE)) {
		s = dn_getpart (dn, "umode");
		if (s) {
			entry->umode = atoi (s);
			entry->fflags = ((entry->umode & 0300) >> 6) ^ 2;
			if (!(flags & SAR_F_HONOR_UMODE)) {
				entry->umode = (entry->umode & 0700) | 
									((entry->umode & 0700) >> 3) | 
									((entry->umode & 0700) >> 6);
				entry->umode |= 0x444;
				entry->umode &= ~fop_getumask();
			}
		}
	}
	if (flags & SAR_F_HONOR_UID) {
		s = dn_getpart (dn, "uid");
		if (s) {
			entry->uid = atoi (s);
		} else {
			s = dn_getpart (dn, "uname");
			if (s) {
				ret = entry->uid = fpw_getuidbyname2 (s);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_NOTICE, "error getting uid by name (%s): %s",
								s, rerr_getstr3(ret));
					return ret;
				}
			}
		}
		s = dn_getpart (dn, "gid");
		if (s) {
			entry->gid = atoi (s);
		} else {
			s = dn_getpart (dn, "ugroup");
			if (s) {
				ret = entry->gid = fpw_getgidbyname2 (s);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_NOTICE, "error getting uid by name (%s): %s",
								s, rerr_getstr3(ret));
					return ret;
				}
			}
		}
	}
	s = dn_getpart (dn, "ctime");
	if (s) {
		ret = cjg_strptime3 (&entry->ctime, s, "%D");
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "invalid ctime (%s): %s",
					s, rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}

static
int
verify_cat (sar, start, maddr, mapsz, flags)
	struct sar	*sar;
	size_t		start;
	char			**maddr;
	size_t		*mapsz;
	int			flags;
{
	struct sar_entry	*entry;
	size_t				i;
	int					ret;

	if (!maddr) return RERR_PARAM;
	*maddr = NULL;
	if (!sar || !mapsz) return RERR_PARAM;
	/* get offset */
	TLST_FOREACHPTR2 (entry, sar->entries, i) {
		FRLOGF (LOG_VVERB, "verify entry (%s) at %lld", entry->fn,
						(long long)start);
		ret = verify_catentry (sar, entry, start, maddr, mapsz, flags);
		if (!RERR_ISOK(ret)) return ret;
		start = entry->offset + entry->sz;
		FRLOGF (LOG_VVERB, "entry (%s) has offset %lld and size %lld",
					entry->file, (long long)entry->offset, (long long)entry->sz);
	}
	/* verify hash */
	TLST_FOREACHPTR2 (entry, sar->entries, i) {
		ret = verify_hash (sar, entry, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
verify_catentry (sar, entry, start, maddr, mapsz, flags)
	struct sar			*sar;
	struct sar_entry	*entry;
	size_t				start;
	char					**maddr;
	size_t				*mapsz;
	int					flags;
{
	char		*addr, *p;
	ssize_t	psize, off, len;

	if (!maddr) return RERR_PARAM;
	*maddr = NULL;
	if (!sar || !entry || !mapsz) return RERR_PARAM;
	if (start > sar->fsz) return RERR_INVALID_FILE;
	psize = sar->psize < 4096 ? 4096 : sar->psize;
	off = start & ~(psize - 1);
	len = 2*psize;
	if (off+len > (ssize_t)sar->fsz) len = sar->fsz-off;
	*mapsz = len;
	addr = mmap (NULL, len, PROT_READ, MAP_PRIVATE, sar->fd, off);
	if (!addr) return RERR_SYSTEM;
	*maddr = addr;
	p = addr + (start-off);
	if (!strncasecmp (p, ":BEGIN:", 7)) {
		if (p[7] == '\n') {
			p+=8;
		} else if (p[15] == '\n') {
			p+=16;
		} else if (p[23] == '\n') {
			p+=24;
		} else if (p[31] == '\n') {
			p+=32;
		} else {
			for (; *p && *p!='\n' && p-addr < len-64; p++);
			if (*p != '\n') return RERR_INVALID_FILE;
			p++;
		}
	} else if (*p=='#') {
		for (; *p == '#' && p-addr < len-64; p++);
		if (*p != '\n') return RERR_INVALID_FILE;
		p++;
	}
	if (strncasecmp (p, ":ENTRY:", 7) != 0) {
		return RERR_INVALID_FILE;
	}
	if (p[7] == '\n') {
		p+=8;
	} else if (p[15] == '\n') {
		p+=16;
	} else if (p[23] == '\n') {
		p+=24;
	} else if (p[31] == '\n') {
		p+=32;
	} else {
		for (; *p && *p!='\n' && p-addr < len-4; p++);
		if (*p != '\n') return RERR_INVALID_FILE;
		p++;
	}
	entry->offset = off + (p - addr);
	munmap (addr, len);
	*maddr = NULL;
	return RERR_OK;
}

static
int
verify_hash (sar, entry, flags)
	struct sar			*sar;
	struct sar_entry	*entry;
	int					flags;
{
	struct sar_file	file;
	int					ret;
	char					hash[128];

	if (!sar || !entry) return RERR_PARAM;
	if (flags & SAR_F_IGN_HASH) return RERR_OK;
	if (entry->ht == SAR_HT_NONE) {
		if (flags & SAR_F_FORCE_HASH) return RERR_INVALID_ENTRY;
		return RERR_OK;
	}
	FRLOGF (LOG_DEBUG, "verify hash for %s", entry->fn);
	if (flags & SAR_F_VERBOSE) {
		fdprtf (2, "verify hash for %s: ", entry->fn);
	}
	ret = _sar_fopen (&file, sar, entry, entry->ht & SAR_HT_T_MASK);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error opening entry (%s): %s", 
					entry->fn, rerr_getstr3(ret));
		return ret;
	}
	switch (entry->ht & SAR_HT_MASK) {
	case SAR_HT_MD5:
		ret = calc_md5 (&file, hash);
		break;
	case SAR_HT_SHA1:
		ret = calc_sha1 (&file, hash);
		break;
	case SAR_HT_SHA3:
		ret = calc_sha3 (&file, hash);
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	sar_fclose (&file);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error calculating hash: %s",
				rerr_getstr3(ret));
		return ret;
	}
	ret = strcasecmp (hash, entry->hash);
	if (ret != 0) {
		if (flags & SAR_F_VERBOSE) {
			fdprtf (2, "not ok (should: %s - is: %s)\n", entry->hash, hash);
		}
		FRLOGF (LOG_NOTICE, "hash is not correct (should: %s - is: %s)",
						entry->hash, hash);
		return RERR_INVALID_ENTRY;
	}
	FRLOGF (LOG_DEBUG, "hash for %s is ok (%s)", entry->fn, hash);
	if (flags & SAR_F_VERBOSE) {
		fdprtf (2, "ok (%s)\n", hash);
	}
	return RERR_OK;
}

static
int
calc_md5 (file, hash)
	struct sar_file	*file;
	char					*hash;
{
	struct md5sum	md5hash;
	int				ret, len;
	char				buf[256];

	if (!file || !hash) return RERR_PARAM;
	md5hash = MD5SUM_INIT;
	while (1) {
		ret = len = sar_fread (file, buf, sizeof (buf));
		if (!RERR_ISOK(ret)) return ret;
		if (len == 0) break;
		ret = md5sum_calc (&md5hash, buf, len);
		if (!RERR_ISOK(ret)) return ret;
	}
	return md5sum_get (hash, &md5hash, TENC_FMT_HEX);
}

static
int
calc_sha1 (file, hash)
	struct sar_file	*file;
	char					*hash;
{
	return RERR_NOT_SUPPORTED;
}

static
int
calc_sha3 (file, hash)
	struct sar_file	*file;
	char					*hash;
{
	return RERR_NOT_SUPPORTED;
}


static
int
verify_sig (sar, addr, siglen, sig, flags)
	struct sar	*sar;
	char			*addr;
	size_t		siglen;
	char			*sig;
	int			flags;
{
	if (!sar) return RERR_PARAM;
	if (flags & SAR_F_IGN_SIG) return RERR_OK;
	if (!addr || !sig || siglen == 0) {
		if (flags & SAR_F_FORCE_SIG) return RERR_INVALID_FILE;
		return RERR_OK;
	}
	/* do check - to be done ... */
	return RERR_OK;
}

static
void
sar_hfree (sar)
	struct sar	*sar;
{
	if (!sar) return;
	bufref_free (&sar->bufref);
	mail_hdr_free (&sar->desc.desc);
	TLST_FREE (sar->entries);
	pthread_mutex_destroy (&sar->mutex);
	*sar = SAR_INIT;
	return;
}

int
sar_close (sar)
	struct sar	*sar;
{
	if (!sar) return RERR_PARAM;
	if (sar->num_openfiles) return RERR_BUSY;
	sar_hfree (sar);
	return RERR_OK;
}


int
sar_fopen (file, sar, entry)
	struct sar_file	*file;
	struct sar			*sar;
	const char			*entry;
{
	struct sar_entry	*sentry;
	int					ret;
	size_t				i;

	if (!file || !sar || !entry || !*entry) return RERR_PARAM;
	pthread_mutex_lock (&sar->mutex);
	TLST_FOREACHPTR2 (sentry, sar->entries, i) {
		if (!strcasecmp (sentry->fn, entry)) goto found;
	}
	ret = RERR_NOT_FOUND;
	goto out;
found:
	ret = _sar_fopen (file, sar, sentry, SAR_HT_T_PLAIN);
	if (!RERR_ISOK(ret)) goto out;
	sar->num_openfiles++;
out:
	pthread_mutex_unlock (&sar->mutex);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error open file entry >>%s<<: %s", entry,
					rerr_getstr3(ret));
	}
	return ret;
}

int
sar_fclose (file)
	struct sar_file	*file;
{
	int	wstatus;

	if (!file || !file->sar) return RERR_PARAM;
	if (file->zip_pid > 0)
		kill (file->zip_pid, SIGKILL);
	if (file->wr_pid > 0)
		kill (file->wr_pid, SIGKILL);
	if (file->zip_pid > 0)
		waitpid (file->zip_pid, &wstatus, 0);
	if (file->wr_pid > 0)
		waitpid (file->wr_pid, &wstatus, 0);
	if (file->zip_fd > 2)
		close (file->zip_fd);
	pthread_mutex_lock (&file->sar->mutex);
	file->sar->num_openfiles --;
	pthread_mutex_unlock (&file->sar->mutex);
	*file = SAR_FILE_INIT;
	return RERR_OK;
}

ssize_t
sar_fread (file, buf, blen)
	struct sar_file	*file;
	char					*buf;
	size_t				blen;
{
	ssize_t			ret;
	size_t			cur=0;

	if (!file || !file->sar || !file->entry) return RERR_PARAM;
	if (!file->needunzip) {
		ret = _sar_doread (file, buf, blen);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error reading file (%s): %s",
					file->entry->fn, rerr_getstr3(ret));
			return ret;
		}
		cur = ret;
	} else {
		if (file->zip_fd < 2) return 0;
		cur=0;
		while (cur < blen) {
			ret = read (file->zip_fd, buf+cur, blen-cur);
			if (ret < 0) {
				if (errno == EPIPE) {
					ret=0;
				} else if (errno == EINTR) {
					continue;
				} else {
					ret = RERR_SYSTEM;
				}
			}
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error reading from pipe (%s): %s",
						file->entry->fn, rerr_getstr3(ret));
				return ret;
			}
			if (ret == 0) break;
			cur += ret;
		}
	}
	file->rpos += cur;
	return cur;
}

ssize_t
sar_fread2 (file, buf, blen)
	struct sar_file	*file;
	char					*buf;
	size_t				blen;
{
	ssize_t	ret;
	size_t	num=0;

	while (1) {
		ret = sar_fread (file, buf+num, blen-num);
		if (!RERR_ISOK(ret)) return ret;
		num += ret;
		if (num == blen) return num;
		if ((size_t) file->rpos == file->entry->osz) return num;
	}
	return 0;
}


off_t
sar_seek (sfile, offset, whence)
	struct sar_file	*sfile;
	off_t					offset;
	int					whence;
{
	off_t		pos;
	char		buf[128];
	int		ret;
	int64_t	rsz;
	size_t	num;

	if (!sfile) return RERR_PARAM;
	pthread_mutex_lock (&sfile->sar->mutex);
	rsz = sfile->entry->osz;
	sfile->entry->rsz = rsz;
	pthread_mutex_unlock (&sfile->sar->mutex);
	switch (whence) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_CUR:
		pos = sfile->rpos + offset;
		break;
	case SEEK_END:
#if 0
		if (rsz < 0) {
			if (!sfile->entry->aa && !sfile->entry->zip) {
				rsz = sfile->entry->sz;
			} else {
				sfile->pos = sfile->rpos = 0;
				sfile->idx = 0;
				while ((ret = sar_fread (sfile, buf, sizeof (buf)))) {
					if (!RERR_ISOK(ret)) return ret;
				};
				rsz = sfile->rpos;
			}
			pthread_mutex_lock (&sfile->sar->mutex);
			sfile->entry->rsz = rsz;
			pthread_mutex_unlock (&sfile->sar->mutex);
		}
#endif
		pos = rsz + offset;
		break;
	default:
		return RERR_PARAM;
	}
	if (pos < 0 || (pos >= rsz && pos != 0)) {
		return RERR_OUTOFRANGE;
	}
	if (!sfile->entry->aa && !sfile->entry->zip) {
		sfile->pos = pos;
		sfile->rpos = pos;
		sfile->idx = 0;
		return sfile->rpos;
	}
	if (sfile->rpos == pos) return sfile->rpos;
	if (pos < sfile->rpos) {
		sfile->pos = sfile->rpos = 0;
		sfile->idx = 0;
	}
	while (sfile->rpos < pos) {
		if ((size_t)(pos - sfile->rpos) > sizeof (buf)) {
			num = sizeof (buf);
		} else {
			num = pos - sfile->rpos;
		}
		ret = sar_fread (sfile, buf, num);
		if (!RERR_ISOK(ret)) return ret;
		if (ret == 0) break;
	}
	return sfile->rpos;
}
	


int
sar_extract_all (sar, dest, flags)
	struct sar	*sar;
	const char	*dest;
	int			flags;
{
	struct sar_entry	*entry;
	ssize_t				i;
	int					ret, ret2 = RERR_OK;
	int					num=0;

	if (!sar) return RERR_PARAM;
	if (!dest) dest = "./";
	flags |= SAR_F_DESTISDIR;
	TLST_FOREACHPTR2 (entry, sar->entries, i) {
		ret = sar_extract (sar, entry->fn, dest, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error extracting %s to %s: %s", entry->fn, dest,
					rerr_getstr3(ret));
			ret2 = ret;
		} else {
			num++;
		}
	}
	if (!RERR_ISOK(ret2)) {
		FRLOGF (LOG_ERR, "error extracting some files, still extracted %d "
						"files", num);
		return ret2;
	}
	FRLOGF (LOG_DEBUG, "extracted %d files from archive", num);
	return num;
}

int
sar_extract (sar, file, dest, flags)
	struct sar	*sar;
	const char	*file, *dest;
	int			flags;
{
	struct sar_file	sfile;
	int					ret, fd, len;
	char					_buf[128], *buf, *s;

	if (!sar || !file) return RERR_PARAM;
	ret = sar_fopen (&sfile, sar, file);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error opening file %s: %s", file, rerr_getstr3(ret));
		return ret;
	}
	if (!dest) {
		if (flags & SAR_F_NOSUBDIR) {
			s = rindex (file, '/');
			if (s && s[1]) {
				buf = top_strcpdup (_buf, sizeof (_buf), s+1);
			} else {
				buf = top_strcpdup (_buf, sizeof (_buf), file);
			}
		} else {
			buf = top_strcpdup (_buf, sizeof (_buf), file);
		}
	} else if (flags & SAR_F_DESTISDIR) {
		if (flags & SAR_F_NOSUBDIR) {
			s = rindex (file, '/');
			if (s && s[1]) {
				buf = a2sprtf (_buf, sizeof (_buf), "%s/%s", dest, s+1);
			} else {
				buf = a2sprtf (_buf, sizeof (_buf), "%s/%s", dest, file);
			}
		} else {
			buf = a2sprtf (_buf, sizeof (_buf), "%s/%s", dest, file);
		}
	} else {
		buf = top_strcpdup (_buf, sizeof (_buf), dest);
	}
	if (flags & SAR_F_VERBOSE) {
		fdprtf (2, "%s -> %s\n", file, buf);
	}
	if (!buf) {
		sar_fclose (&sfile);
		return RERR_NOMEM;
	}
	ret = fd = _sar_chkfile (buf, flags);
	if (!RERR_ISOK(ret)) {
		sar_fclose (&sfile);
		if (flags & SAR_F_VERBOSE) {
			FRLOGF (LOG_ERR2, "cannot write to %s: %s", buf, rerr_getstr3(ret));
		} else {
			FRLOGF (LOG_ERR, "cannot write to %s: %s", buf, rerr_getstr3(ret));
		}
		if (buf != _buf) free (buf);
		return ret;
	}
	if (buf != _buf) free (buf);
	while (1) {
		ret = len = sar_fread (&sfile, _buf, sizeof (_buf));
		if (ret <= 0) break;
		ret = fdwrite (fd, _buf, len, 0);
		if (!RERR_ISOK(ret)) break;
	}
	if ((flags & SAR_F_HONOR_UID) && (getuid() == 0)) {
		if (fchown (fd, sfile.entry->uid, sfile.entry->gid) < 0) {
			FRLOGF (LOG_WARN, "error changing owner/group: %s",
						rerr_getstr3(RERR_SYSTEM));
		}
	}
	FRLOGF (LOG_DEBUG, "change permission of %s to %o", sfile.entry->fn, 
									(unsigned)sfile.entry->umode);
	if (fchmod (fd, sfile.entry->umode) < 0) {
		FRLOGF (LOG_WARN, "error changing permessions: %s", 
						rerr_getstr3(RERR_SYSTEM));
	}
	sar_fclose (&sfile);
	close (fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error extracting file: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
_sar_chkfile (file, flags)
	char	*file;
	int	flags;
{
	char	*s;
	int	fd, ret;

	if (!file) return RERR_PARAM;
	if (fop_exist (file)) {
		if (flags & SAR_F_EXCL) {
			return RERR_ALREADY_EXIST;
		}
		if (fop_isdirlink (file)) {
			return RERR_ALREADY_EXIST;
		}
	} else {
		s = rindex (file, '/');
		if (s && s>file) {
			*s = 0;
			if (fop_exist (file)) {
				if (!fop_isdirlink (file)) {
					*s = '/';
					return RERR_NODIR;
				}
				*s = '/';
			} else {
				ret = fop_mkdir_rec (file);
				*s = '/';
				if (!RERR_ISOK(ret)) return ret;
			}
		}
	}
	fd = open (file, O_RDWR|O_CREAT, 0666);
	if (fd < 0) return RERR_SYSTEM;
	return fd;
}



/* **********************
 * Thread functions
 * **********************/

static
int
_sar_fopen (file, sar, entry, how)
	struct sar_file	*file;
	struct sar			*sar;
	struct sar_entry	*entry;
	int					how;
{
	int				ret;

	if (!file || !sar || !entry) return RERR_PARAM;
	*file = SAR_FILE_INIT;
	switch (how) {
	case SAR_HT_T_AA:
		if (entry->aa == SAR_AA_NONE) return RERR_PARAM;
		break;
	case SAR_HT_T_Z:
		if (entry->zip == SAR_ZIP_NONE) return RERR_PARAM;
		break;
	case SAR_HT_T_PLAIN:
		break;
	default:
		return RERR_PARAM;
	}
	file->sar = sar;
	file->entry = entry;
	file->how = how;
	file->pos = 0;
	file->needunzip = file->how == SAR_HT_T_PLAIN && 
					file->entry->zip != SAR_ZIP_NONE;
	file->needaa = file->entry->aa != SAR_ZIP_NONE &&
							file->how == SAR_HT_T_AA;
	if (file->needaa) {
		switch (file->entry->aa) {
		case SAR_AA_B64:
			file->aa = TENC_FMT_B64;
			break;
		case SAR_AA_B32:
			file->aa = TENC_FMT_B32;
			break;
		case SAR_AA_HEX:
			file->aa = TENC_FMT_HEX;
			break;
		default:
			return RERR_NOT_SUPPORTED;
		}
	}

	if (file->needunzip) {
		ret = _sar_fopen_unzip (file);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error starting unzip childs: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}

	return RERR_OK;
}


static
int
_sar_fopen_unzip (file)
	struct sar_file	*file;
{
	int			fds[2], post_w, post_r;
	int			pre_w, pre_r;
	int			ret;
	pid_t			pid;

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
	post_r = fds[0];
	post_w = fds[1];
#ifdef LINUX
	ret = pipe2 (fds, O_CLOEXEC);
#else
	ret = pipe (fds);
#endif
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot open pipe: %s", rerr_getstr3(ret));
		close (post_r);
		close (post_w);
		return ret;
	}
	pre_r = fds[0];
	pre_w = fds[1];

	/* run zip child */
	pid = fork ();
	if (pid < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot fork(): %s", rerr_getstr3(ret));
		close (post_r);
		close (post_w);
		close (pre_r);
		close (pre_w);
		return ret;
	}
	if (pid == 0) {
		close (pre_w);
		close (post_r);
		ret = _sar_run_unzip (pre_r, post_w, file->entry->zip);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot run unzip: %s", rerr_getstr3(ret));
			exit (ret);
		}
		/* should not arrive here */
		exit (RERR_INTERNAL);
	}
	file->zip_pid = pid;
	close (pre_r);
	close (post_w);

	/* run writer child */
	pid = fork ();
	if (pid < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot fork(): %s", rerr_getstr3(ret));
		close (post_r);
		close (pre_w);
		kill (file->zip_pid, SIGKILL);
		waitpid (file->zip_pid, NULL, 0);
		file->zip_pid = 0;
		return ret;
	}
	if (pid == 0) {
		close (post_r);
		ret = _sar_run_wchild (file, pre_w);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot run writer child: %s", rerr_getstr3(ret));
			exit (ret);
		}
		exit (0);
	}
	file->wr_pid = pid;
	close (pre_w);
	file->zip_fd = post_r;
	return RERR_OK;
}

static
int
_sar_run_unzip (fd_r, fd_w, zip)
	int			fd_r, fd_w, zip;
{
	int			ret;
	//const char	*xz_arg[] = {"xz", "-d", "--format=auto", "--stdout", "-", NULL};
	const char	*xz_arg[] = {"unxz", "-c", NULL};
	const char	*gz_arg[] = {"gunzip", "-c", NULL};
	const char	*bz_arg[] = {"bunzip2", "-c", NULL};

	if (fd_r < 2 || fd_w < 2) return RERR_PARAM;
	if (dup2 (fd_r, 0) < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot dup to stdin: %s", rerr_getstr3(ret));
		exit (ret);
	}
	if (dup2 (fd_w, 1) < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot dup to stdout: %s", rerr_getstr3(ret));
		exit (ret);
	}
	close (fd_r);
	close (fd_w);
	ret = RERR_SYSTEM;
	switch (zip) {
	case SAR_ZIP_XZ:
	case SAR_ZIP_LZMA:
		execvp ("xz", (char *const*)xz_arg);
		break;
	case SAR_ZIP_GZ:
		execvp ("gzip", (char *const*)gz_arg);
		break;
	case SAR_ZIP_BZIP2:
		execvp ("bzip2", (char *const*)bz_arg);
		break;
	default:
		ret = RERR_NOT_SUPPORTED;
		break;
	}
	FRLOGF (LOG_ERR, "error executing unzip command: %s", rerr_getstr3(ret));
	exit (ret);
	return 0;
}


static
int
_sar_run_wchild (file, fd)
	struct sar_file	*file;
	int					fd;
{
	int	ret, num;
	char	buf[128];

	if (!file || fd < 0) return RERR_PARAM;
	close (file->zip_fd);
	while (1) {
		ret = num = _sar_doread (file, buf, sizeof (buf));
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error reading file (%s): %s", file->entry->fn,
						rerr_getstr3(ret));
			return ret;
		}
		if (num == 0) break;
		ret = fdwrite (fd, buf, num, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error writing to pipe: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
	return RERR_OK;
}




static
int
_sar_doread (file, buf, blen)
	struct sar_file	*file;
	char					*buf;
	size_t				blen;
{
	int		len;
	char		_buf[sizeof (file->buf)], *xbuf;
	int		ret, num;

	if (!file || !buf) return RERR_PARAM;
	if (blen == 0) return 0;
	if (file->blen) goto wrtobuf;

	if (file->needaa) {
		xbuf = _buf;
		len = sizeof (_buf);
	} else {
		xbuf = file->buf;
		len = sizeof (file->buf);
	}
	if (file->pos + len > (off_t)file->entry->sz) {
		len = file->entry->sz - file->pos;
		if (len <= 0) return 0;
	}
	//FRLOGF (LOG_VVERBOSE, "call fdreadp() with offset=%lld, pos=%lld",
	//			(long long) file->entry->offset, (long long)file->pos);
	ret = num = fdreadp (file->sar->fd, xbuf, (size_t)len, file->entry->offset + file->pos, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error reading file (%s) at (%Lu): %s",
					file->entry->fn, (unsigned long long)file->pos, rerr_getstr3(ret));
		return ret;
	}
	if (num == 0) {
		FRLOGF (LOG_ERR, "read truncated (%s)", file->entry->fn);
		return RERR_INVALID_FILE;
	}
	file->pos += num;
	if (file->needaa) {
		ret = tenc_decode (file->buf, &file->blen, _buf, num, file->aa);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error decoding: %s", rerr_getstr3(ret));
			return ret;
		}
	} else {
		file->blen = num;
	}

wrtobuf:
	if (file->blen < blen) blen = file->blen;
	memcpy (buf, file->buf+file->idx, blen);
	file->blen -= blen;
	if (file->blen > 0) {
		file->idx += blen;
	} else {
		file->idx = 0;
	}
	return blen;
}


int
sar_getdesc (val, sar, key)
	const char	**val;
	struct sar	*sar;
	const char	*key;
{
	unsigned	i;

	if (!sar || !key || !val) return RERR_PARAM;
	sswitch (key) {
	sicase ("fmagic")
		*val = sar->desc.fmagic;
		break;
	sicase ("fver")
		*val = sar->desc.fver;
		break;
	sicase ("fdesc")
		*val = sar->desc.fdesc;
		break;
	sdefault
		for (i=0; i<sar->desc.desc.numlines; i++) {
			if (!strcasecmp (sar->desc.desc.lines[i].field, key)) {
				*val = sar->desc.desc.lines[i].value;
				break;
			}
		}
		break;
	} esac;
	if (!*val) return RERR_NOT_FOUND;
	return RERR_OK;
}


ssize_t
sar_getsize (sar, file)
	struct sar	*sar;
	const char	*file;
{
	struct sar_entry	*sentry;
	int					ret=RERR_OK;
	size_t				i;
	ssize_t				sz=0;

	if (!file || !sar || !*file) return RERR_PARAM;
	pthread_mutex_lock (&sar->mutex);
	TLST_FOREACHPTR2 (sentry, sar->entries, i) {
		if (!strcasecmp (sentry->fn, file)) goto found;
	}
	ret = RERR_NOT_FOUND;
	goto out;
found:
	sz = sentry->osz;
out:
	pthread_mutex_unlock (&sar->mutex);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error open file entry >>%s<<: %s", file,
					rerr_getstr3(ret));
		return ret;
	}
	return sz;
}

int
sar_exist (sar, file)
	struct sar	*sar;
	const char	*file;
{
	struct sar_entry	*sentry;
	int					ret=0;;
	size_t				i;

	if (!file || !sar || !*file) return 0;
	pthread_mutex_lock (&sar->mutex);
	TLST_FOREACHPTR2 (sentry, sar->entries, i) {
		if (!strcasecmp (sentry->fn, file)) {
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock (&sar->mutex);
	return ret;
}

ssize_t
sar_fgetsize (sfile)
	struct sar_file	*sfile;
{
	if (!sfile) return RERR_PARAM;
	return sfile->entry->osz;
}


ssize_t
sar_readfile (sf, buf)
	struct sar_file	*sf;
	char					**buf;
{
	ssize_t	ret;
	char		*xbuf;
	size_t	sz;

	if (!sf || !buf) return RERR_PARAM;
	sz = sf->entry->osz;
	xbuf = malloc (sz+1);
	if (!xbuf) return RERR_NOMEM;
	ret = sar_fread2 (sf, xbuf, sz);
	if (!RERR_ISOK(ret)) {
		free (xbuf);
		return ret;
	}
	xbuf[sz]=0;
	*buf = xbuf;
	return sz;
}

ssize_t
sar_readfn (sar, fn, buf)
	struct sar	*sar;
	const char	*fn;
	char			**buf;
{
	struct sar_file	sf;
	int					ret;
	ssize_t				sz;

	if (!sar || !fn || !buf) return RERR_PARAM;
	ret = sar_fopen (&sf, sar, fn);
	if (!RERR_ISOK(ret)) return ret;
	sz = sar_readfile (&sf, buf);
	sar_fclose (&sf);
	return sz;
}

ssize_t
sar_fgets2 (buf, size, sf)
	char					*buf;
	size_t				size;
	struct sar_file	*sf;
{
	ssize_t	ret;
	char		*s;

	if (!buf || !sf) return RERR_PARAM;
	if (size == 0) return 0;
	ret = sar_fread2 (sf, buf, size-1);
	if (!RERR_ISOK(ret)) return ret;
	buf[ret]=0;
	s = index (buf, '\n');
	if (s) {
		s++;
		*s = 0;
		sar_seek (sf, (s-buf)-ret, SEEK_CUR);
		ret = s-buf;
	}
	return ret;
}

char *
sar_fgets (buf, size, sf)
	char					*buf;
	int					size;
	struct sar_file	*sf;
{
	ssize_t	ret;

	ret = sar_fgets2 (buf, size, sf);
	if (!RERR_ISOK(ret)) return NULL;
	if (ret == 0) return NULL;
	return buf;
}


ssize_t
sar_readln (obuf, buf, bsize, sf)
	char					**obuf, *buf;
	size_t				bsize;
	struct sar_file	*sf;
{
	char		_xbuf[256], *xbuf = NULL, *p;
	ssize_t	ret;
	size_t	bsz=0, csz=0;

	if (!obuf || !sf) return RERR_PARAM;
	if (buf && bsize > 1) {
		ret = sar_fgets2 (buf, bsize, sf);
		*obuf = buf;
		if (!RERR_ISOK(ret)) return ret;
		if ((size_t)ret < bsize) return ret;
		if (buf[bsize-2] == '\n') return ret;
		bsz = bsize + sizeof (_xbuf) + 1;
		xbuf = malloc (bsz);
		if (!xbuf) return RERR_NOMEM;
		memcpy (xbuf, buf, bsize);
		csz = bsize-1;
	}
	while (1) {
		ret = sar_fgets2 (_xbuf, sizeof (_xbuf), sf);
		if (!RERR_ISOK(ret)) {
			if (xbuf) free (xbuf);
			return ret;
		}
		if (ret > 0) {
			if (bsz-csz <= (size_t)ret) {
				bsz += sizeof (_xbuf);
				p = realloc (xbuf, bsz);
				if (!p) {
					if (xbuf) free (xbuf);
					return RERR_NOMEM;
				}
				xbuf = p;
			}
			memcpy (xbuf + csz, _xbuf, ret);
			csz += ret;
		}
		*obuf = xbuf;
		if ((size_t)ret < sizeof (_xbuf)) return csz;
	}
	return 0;
}

ssize_t
sar_readln2 (obuf, sf)
	char					**obuf;
	struct sar_file	*sf;
{
	return sar_readln (obuf, NULL, 0, sf);
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
