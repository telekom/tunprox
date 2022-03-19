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

#ifndef _R__FRLIB_LIB_ARCH_SAR_H
#define _R__FRLIB_LIB_ARCH_SAR_H

#define _FILE_OFFSET_BITS 64
#include <stdint.h>
#include <pthread.h>

#include <fr/base/tmo.h>
#include <fr/base/tlst.h>
#include <fr/base/bufref.h>
#include <fr/mail/mail.h>

#ifdef __cplusplus
extern "C" {
#endif



#define SAR_ZIP_NONE		0
#define SAR_ZIP_XZ		1
#define SAR_ZIP_LZMA		2
#define SAR_ZIP_GZ		3
#define SAR_ZIP_LIBZ		4		/* not implemented yet */
#define SAR_ZIP_BZIP2	5

#define SAR_AA_NONE		0
#define SAR_AA_B64		1
#define SAR_AA_B32		2
#define SAR_AA_HEX		3
#define SAR_AA_PQUOTE	4		/* not implemented yet */
#define SAR_AA_UTF7		5		/* not implemented yet */

#define SAR_HT_NONE		0x00
#define SAR_HT_MD5		0x10
#define SAR_HT_Z_MD5		0x11
#define SAR_HT_AA_MD5	0x12
#define SAR_HT_SHA1		0x20
#define SAR_HT_Z_SHA1	0x21
#define SAR_HT_AA_SHA1	0x22
#define SAR_HT_SHA3		0x30	/* not implemented yet */
#define SAR_HT_Z_SHA3	0x31	/* not implemented yet */
#define SAR_HT_AA_SHA3	0x32	/* not implemented yet */

#define SAR_HT_MASK		0xfff0
#define SAR_HT_T_MASK	0x0f
#define SAR_HT_T_PLAIN	0
#define SAR_HT_T_Z		1
#define SAR_HT_T_AA		2

#define SAR_FF_NONE		0x00
#define SAR_FF_X			0x01	/* executable */
#define SAR_FF_R			0x02	/* read-only */




struct sar_desc {
	struct mailhdr_lines	desc;
	const char				*fmagic;
	const char				*fver;
	const char				*fdesc;
	const char				*creator;
	const char				*ca;
	tmo_t						date;
};


struct sar_entry {
	const char	*fn;
	const char	*file;	/* for adding */
	uint64_t		sz;
	uint64_t		osz;
	int64_t		rsz;
	int			zip;
	int			aa;		/* ascii armored */
	int			ht;		/* hash type */
	const char	*hash;
	tmo_t			ctime;
	int			fflags;	/* file flags */
	int			umode;
	int			uid;
	int			gid;
	int			flags;
	uint64_t		offset;
};	

#define SAR_ENTRY_SINIT { .rsz = -1 }
#define SAR_ENTRY_INIT ((struct sar_entry) SAR_ENTRY_SINIT)

struct sar {
	const char			*ar;		/* archive file name */
	int					fd;
	uint64_t				fsz;		/* file size */
	size_t				psize;	/* page size, but at least 4096 */
	struct bufref		bufref;
	struct sar_desc	desc;
	struct tlst			entries;
	int					num_openfiles;
	pthread_mutex_t	mutex;
};

#define SAR_SINIT	{ \
		.fd = -1, \
		.bufref = BUFREF_SINIT, \
		.entries = TLST_SINIT_T(struct sar_entry), \
		.mutex = PTHREAD_MUTEX_INITIALIZER, \
	}

#define SAR_INIT ((struct sar)SAR_SINIT)



#define SAR_F_NONE			0x000
#define SAR_F_IGN_HASH		0x001
#define SAR_F_IGN_SIG		0x002
#define SAR_F_FORCE_HASH	0x004
#define SAR_F_FORCE_SIG		0x008
#define SAR_F_HONOR_UMODE	0x010
#define SAR_F_HONOR_UID		0x020
#define SAR_F_NOSUBDIR  	0x040		/* don't extract path */
#define SAR_F_DESTISDIR 	0x080		/* destination is dir */
#define SAR_F_EXCL      	0x100		/* don't overwrite */
#define SAR_F_VERBOSE		0x200




struct sar_file {
	struct sar			*sar;
	struct sar_entry	*entry;
	int					how;
	int					needunzip;
	int					needaa;
	int					aa;
	int					zip_pid;
	int					wr_pid;
	int					zip_fd;
	off_t					pos;
	off_t					rpos;
	char					buf[256];
	size_t				blen;
	size_t				idx;
};

#define SAR_FILE_SINIT { .zip_fd = -1, }
#define SAR_FILE_INIT ((struct sar_file)SAR_FILE_SINIT)




int sar_open (struct sar *sar, const char *file, int flags);
int sar_close (struct sar *sar);

int sar_fopen (struct sar_file *sfile, struct sar *sar, const char *entry);
int sar_fclose (struct sar_file *sfile);
ssize_t sar_fread (struct sar_file *sfile, char *buf, size_t bsize);
/* sar_fread2() guarantees that bsize bytes are read unless EOF is met */
ssize_t sar_fread2 (struct sar_file *sfile, char *buf, size_t bsize);
off_t sar_seek (struct sar_file *sfile, off_t offset, int whence);

ssize_t sar_fgets2 (char *buf, size_t size, struct sar_file*);
char *sar_fgets (char *buf, int size, struct sar_file*);
/* if line fits into buf, buf will be returned in obuf, otherwise
   obuf is alloced */
ssize_t sar_readln (char **obuf, char *buf, size_t bsize, struct sar_file*);
/* as sar_readln with buf==NULL => obuf is alloced */
ssize_t sar_readln2 (char **obuf, struct sar_file*);
/* reads whole file, buf is alloced */
ssize_t sar_readfile (struct sar_file *sf, char **buf);
ssize_t sar_readfn (struct sar *sar, const char *fn, char **buf);

int sar_exist (struct sar *sar, const char *entry);
ssize_t sar_getsize (struct sar *sar, const char *entry);
ssize_t sar_fgetsize (struct sar_file *sfile);

int sar_extract (struct sar*, const char *file, const char *dest, int flags);
int sar_extract_all (struct sar*, const char *dest, int flags);
int sar_getdesc (const char**val, struct sar*, const char *key);


int sar_new (struct sar *);
int sar_bufref (struct sar*, char *buf);
int sar_setarch (struct sar*, const char *ar, int cpy);
int sar_desc_set_fmagic (struct sar*, const char *fmagic, int cpy);
int sar_desc_set_fver (struct sar*, const char *fver, int cpy);
int sar_desc_set_fdesc (struct sar*, const char *fdesc, int cpy);
int sar_desc_add (struct sar*, const char *key, const char *val, int cpykey, int cpyval);
int sar_addfile (struct sar*, const char *file, const char *fn, int zip, int aa, int ht, int flags);
int sar_adddir (struct sar*, const char *dir, int zip, int aa, int ht, int flags);
int sar_create (struct sar*);



#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_ARCH_SAR_H */

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
