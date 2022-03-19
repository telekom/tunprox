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



#ifndef _R__FRLIB_LIB_BASE_FILEOP_H
#define _R__FRLIB_LIB_BASE_FILEOP_H

#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <fr/base/tmo.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define FOP_F_NONE	0x00
#define FOP_F_FORCE	0x01

ssize_t fop_fnread (const char *fname, char **obuf);
ssize_t fop_fdread (int fd, char **obuf);
ssize_t fop_fileread (FILE *f, char **obuf);

/* the following fop_read_* functions are depricated - better use fop_*read above */

char * fop_read_fn (const char *fname);
char * fop_read_fn2 (const char *fname, int *flen);
char * fop_read_file (FILE *f);
char * fop_read_file2 (FILE *f, int *flen);
char * fop_read_fd (int fd);
char * fop_read_fd2 (int fd, int *flen);


int fop_mkdir_rec (char *dir);		/* attention! dir will be modified temporarily */
int fop_mkdir_rec2 (const char *dir);
int fop_mkbasedir (const char *dir);
int fop_rmdir_rec (const char *dir);
int fop_getfilemode (const char *dir, const char *file);		/* follow link */
int fop_lgetfilemode (const char *dir, const char *file); /* don't follow link */
int fop_exist (const char *file);
int fop_issymlink (const char *file);
int fop_isdir (const char *file);	/* true if file is a dir */
int fop_isdirlink (const char *file);	/* true if file is dir or symlink to a dir */
int fop_exist2 (const char *dir, const char *file);
int fop_issymlink2 (const char *dir, const char *file);
int fop_isdir2 (const char *dir, const char *file);	/* true if file is a dir */
int fop_isdirlink2 (const char *dir, const char *file);	/* true if file is dir or symlink to a dir */
int fop_getfilelen (off_t *size, const char *file);
int fop_resolvepath (char **obuf, const char *file);
int fop_copyfile (const char *src, const char *dest, int flags);
int fop_foreachsubdir (const char *dir, int (*func) (void*, const char*), void *arg);
int fop_foreachsubdir_tout (const char *dir, int (*func) (void*, const char*, tmo_t), void *arg, tmo_t tout);
int fop_getcwd (char **res, char *buf, size_t blen);


#define FOP_CREAT_M_MODE		0x00fff	/* the lower 12 bits are the permissions */
#define FOP_CREAT_F_SETMODE	0x01000
#define FOP_CREAT_F_OVERWRITE	0x02000
#define FOP_CREAT_F_FILL		0x04000
#define FOP_CREAT_F_RMONDIE	0x08000
#define FOP_CREAT_F_RMONEXIT	0x10000
int fop_creatfile (const char *path, off_t size, int flags);

int64_t fop_filesize (const char *file);

#define FOP_F_NOHOME		0x02
#define FOP_F_NOLOCAL	0x04
int fop_sanitize (char **res, char *buf, size_t blen, const char *fname, int flags);

mode_t fop_getumask ();



#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_FILEOP_H */

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
