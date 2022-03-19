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

#ifndef _R__FRLIB_LIB_BASE_PRTF_H
#define _R__FRLIB_LIB_BASE_PRTF_H


#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


int fdprtf (int fd, const char *fmt, ...);
int vfdprtf (int fd, const char *fmt, va_list ap);

char *asprtf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *vasprtf (const char *fmt, va_list ap);
char *sasprtf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *vsasprtf (const char *fmt, va_list ap);
/* the following two will print into buf if, buf is not NULL and 
 * the resulting string will fit into buf, otherwise a new string
 * is allocated. one must or must not free the buffer. it can be
 * checked by:
 * res = a2sprtf (buf, ...);
 * if (res != buf) free (res);
 */
char *a2sprtf (char *buf, size_t bufsz, const char *fmt, ...)
					__attribute__((format(printf, 3, 4)));
char *va2sprtf (char *buf, size_t bufsz, const char *fmt, va_list ap);





#ifdef __cplusplus
}	/* extern "C" */
#endif













#endif	/* _R__FRLIB_LIB_BASE_PRTF_H */

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
