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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>


#include <fr/base.h>
#include <fr/arch.h>
#include <fr/cal.h>

#include "sar.h"

static const char *PROG="sar";

int do_create (const char*, struct tlst*, int, int, int, int, const char*, const char*, const char*, struct tlst*);
int do_extract (const char*, struct tlst*, const char*, int);
int do_list (const char*, int);
int do_desc (const char*, int);
int do_info (const char*, const char*, int);



/* ********************
 * main functions
 * ********************/


int
sar_usage ()
{
	PROG = fr_getprog ();
	printf (	"%s: usage: %s [<options>] <archive> [<file>...]\n", PROG, PROG);
	printf (	"  options are:\n"
				"    -h                 - this help screen\n"
				"    -C <configfile>    - alternative config file \n"
				"    -c                 - create archive\n"
				"    -x                 - extract from archive\n"
				"    -l|-t              - list content\n"
				"    -i                 - list description section\n"
				"    -I <key>           - get single information from description\n"
				"    -d <dir>           - directory to extract to\n"
				"    -p                 - plain - do not create subdirs\n"
				"    -z                 - compress files when creating (xz)\n"
				"    -Z <algo>          - compress with algorthm (xz, gzip, bzip2)\n"
				"    -A                 - ASCII armor files in archive\n"
				"    -m                 - create md5 hash\n"
				"    -H <algo>          - create <algo> hash (md5, sha1, sha3)\n"
				"    -s                 - sign archive (not yet implemented)\n"
				"    -f                 - force - ignore hash and sig when extracting\n"
				"    -u                 - extract unix permissions, if present\n"
				"    -v                 - be verbose\n"
				"    -M <fmagic>        - magic in description\n"
				"    -V <fver>          - version in description\n"
				"    -D <fdesc>         - description in description\n"
				"    -K <key>:<val>     - key: val in description\n"
				"\n");
	return 0;
}

//#define ACT_NONE		0
#define ACT_CREAT		1
#define ACT_EXTRACT	2
#define ACT_LIST		3
#define ACT_DESC		4
#define ACT_INFO		5

struct descval { const char	*key, *val; };

int
sar_main (argc, argv)
	int	argc;
	char	**argv;
{
	int				c, i, ret;
	int				zip=0, aa=0, ht=0;
	int				flags=SAR_F_DESTISDIR|SAR_F_EXCL;
	int				act=0;
	const char		*ar;
	const char		*ddir="./";
	struct tlst		flist = TLST_INIT_T(char*);
	const char		*fmagic=NULL, *fver=NULL, *fdesc=NULL;
	struct tlst		dlist = TLST_INIT_T(struct descval);
	struct descval	dval;
	char				*s;
	const char		*key=NULL;

	PROG = fr_getprog ();
	while ((c = getopt (argc, argv, "hC:cxltiI:d:pzZ:AmH:sfuvM:V:D:K:")) != -1) {
		switch (c) {
		case 'h':
			sar_usage ();
			return 0;
			break;
		case 'C':
			cf_set_cfname (optarg);
			break;
		case 'c':
			act = ACT_CREAT;
			break;
		case 'x':
			act = ACT_EXTRACT;
			break;
		case 'l': case 't':
			act = ACT_LIST;
			break;
		case 'i':
			act = ACT_DESC;
			break;
		case 'I':
			act = ACT_INFO;
			key = optarg;
			break;
		case 'd':
			ddir = optarg;
			break;
		case 'p':
			flags |= SAR_F_NOSUBDIR;
			break;
		case 'z':
			zip = SAR_ZIP_XZ;
			break;
		case 'Z':
			sswitch (optarg) {
			sicase ("xz")
				zip = SAR_ZIP_XZ;
				break;
			sicase ("gz")
			sicase ("gzip")
				zip = SAR_ZIP_GZ;
				break;
			sicase ("bzip2")
			sicase ("bzip")
				zip = SAR_ZIP_BZIP2;
				break;
			sicase ("lzma")
				zip = SAR_ZIP_LZMA;
				break;
			sdefault
				FRLOGF (LOG_ERR2, "unknown compression method %s", optarg);
				return RERR_PARAM;
			} esac;
			break;
		case 'A':
			aa = SAR_AA_B64;
			break;
		case 'm':
			ht = SAR_HT_MD5;
			break;
		case 'H':
			sswitch (optarg) {
			sicase ("md5")
				ht = SAR_HT_MD5;
				break;
			sicase ("sha1")
				ht = SAR_HT_SHA1;
				break;
			sicase ("sha3")
				ht = SAR_HT_SHA3;
				break;
			sdefault
				FRLOGF (LOG_ERR2, "unknown hash algorithm %s", optarg);
				return RERR_PARAM;
			} esac;
			break;
		case 's':
			/* ignore for now */
			break;
		case 'f':
			flags |= SAR_F_IGN_HASH | SAR_F_IGN_SIG;
			break;
		case 'u':
			flags |= SAR_F_HONOR_UMODE | SAR_F_HONOR_UID;
			break;
		case 'v':
			flags |= SAR_F_VERBOSE;
			break;
		case 'M':
			fmagic = optarg;
			break;
		case 'V':
			fver = optarg;
			break;
		case 'D':
			fdesc = optarg;
			break;
		case 'K':
			s = index (optarg, ':');
			if (!s) {
				FRLOGF (LOG_ERR2, "invalid key:val pair (%s)", optarg);
				return RERR_PARAM;
			}
			*s = 0;
			dval.key = optarg;
			dval.val = s+1;
			ret = TLST_ADD (dlist, dval);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error adding key:val pair to list: %s",
						rerr_getstr3(ret));
				return ret;
			}
			break;
		}
	}
	if (!act) {
		FRLOGF (LOG_ERR2, "no action given");
		return RERR_PARAM;
	}
	if (ht) {
		if (aa) {
			ht |= SAR_HT_T_AA;
		} else if (zip) {
			ht |= SAR_HT_T_Z;
		}
	}
	if (optind >= argc) {
		FRLOGF (LOG_ERR2, "no archive given");
		return RERR_PARAM;
	}
	ar = argv[optind];
	if (!ar || !*ar) {
		FRLOGF (LOG_ERR2, "no archive given");
		return RERR_PARAM;
	}
	for (i=optind+1; i<argc; i++) {
		ret = TLST_ADD (flist, argv[i]);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error creating file list: %s", rerr_getstr3(ret));
			return ret;
		}
	}
	cf_read ();	/* to avoid races */
	switch (act) {
	case ACT_CREAT:
		ret = do_create (ar, &flist, zip, aa, ht, flags, fmagic, fver, fdesc, &dlist);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error creating archive: %s", rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACT_EXTRACT:
		ret = do_extract (ar, &flist, ddir, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error extracting archive: %s", rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACT_LIST:
		ret = do_list (ar, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error listing archive: %s", rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACT_DESC:
		ret = do_desc (ar, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error printing descrition section: %s",
							rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACT_INFO:
		ret = do_info (ar, key, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error printing descrition section: %s",
							rerr_getstr3(ret));
			return ret;
		}
		break;
	}

	TLST_FREE (flist);
	TLST_FREE (dlist);
	return RERR_OK;
}



/* **********************
 * static functions 
 * **********************/


int
do_create (ar, flist, zip, aa, ht, flags, fmagic, fver, fdesc, dlist)
	const char		*ar;
	struct tlst		*flist, *dlist;
	int				zip, aa, ht, flags;
	const char		*fmagic, *fver, *fdesc;
{
	struct sar		sar = SAR_INIT;
	int				ret;
	size_t			i;
	char				*fn;
	struct descval	*dval;

	if (!ar || !flist) return RERR_PARAM;
	if (TLST_GETNUM(*flist) == 0) {
		FRLOGF (LOG_ERR2, "no files to be added");
		return RERR_PARAM;
	}
	ret = sar_setarch (&sar, ar, 0);
	if (!RERR_ISOK(ret)) return ret;
	if (fmagic) {
		ret = sar_desc_set_fmagic (&sar, fmagic, 1);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (fver) {
		ret = sar_desc_set_fver (&sar, fver, 1);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (fdesc) {
		ret = sar_desc_set_fdesc (&sar, fdesc, 1);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (dlist) {
		TLST_FOREACHPTR2 (dval, *dlist, i) {
			ret = sar_desc_add (&sar, dval->key, dval->val, 1, 1);
			if (!RERR_ISOK(ret)) return ret;
		}
	}
	TLST_FOREACH2 (fn, *flist, i) {
		ret = sar_addfile (&sar, fn, NULL, zip, aa, ht, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error adding file %s to archive: %s", fn,
					rerr_getstr3(ret));
			sar_close (&sar);
			return ret;
		}
	}
	ret = sar_create (&sar);
	sar_close (&sar);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error creating archive: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

int
do_extract (ar, flist, ddir, flags)
	const char	*ar, *ddir;
	struct tlst	*flist;	/* ignored for now */
	int			flags;
{
	struct sar	sar;
	int			ret;

	if (!ar || !ddir) return RERR_PARAM;
	ret = sar_open (&sar, ar, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error opening archive: %s", rerr_getstr3(ret));
		return ret;
	}
	flags |= SAR_F_DESTISDIR;
	ret = sar_extract_all (&sar, ddir, flags);
	sar_close (&sar);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error extracting files from archive: %s",
				rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


int
do_list (ar, flags)
	const char	*ar;
	int			flags;
{
	struct sar			sar;
	int					ret;
	struct sar_entry	*entry;
	size_t				i;

	if (!ar) return RERR_PARAM;
	ret = sar_open (&sar, ar, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error opening archive: %s", rerr_getstr3(ret));
		return ret;
	}
	TLST_FOREACHPTR2 (entry, sar.entries, i) {
		printf ("%s\n", entry->fn);
	}
	sar_close (&sar);
	return RERR_OK;
}


int
do_desc (ar, flags)
	const char	*ar;
	int			flags;
{
	struct sar	sar;
	int			ret;
	size_t		i;

	if (!ar) return RERR_PARAM;
	ret = sar_open (&sar, ar, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error opening archive: %s", rerr_getstr3(ret));
		return ret;
	}
	for (i=0; i<sar.desc.desc.numlines; i++) {
		printf ("%s: %s\n", sar.desc.desc.lines[i].field,
				sar.desc.desc.lines[i].value);
	}
	sar_close (&sar);
	return RERR_OK;
}


int
do_info (ar, key, flags)
	const char	*ar;
	const char	*key;
	int			flags;
{
	struct sar	sar;
	int			ret;
	const char	*val;

	if (!ar || !key) return RERR_PARAM;
	ret = sar_open (&sar, ar, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error opening archive: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = sar_getdesc (&val, &sar, key);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "cannot get %s from archive: %s", key,
				rerr_getstr3(ret));
		sar_close (&sar);
		return ret;
	}
	printf ("%s\n", val);
	sar_close (&sar);
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
