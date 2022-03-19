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

#ifndef _R__FRLIB_LIB_BASE_FRFINISH_H
#define _R__FRLIB_LIB_BASE_FRFINISH_H

#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif



#define FRFINISH_F_ONFINISH	0x01
#define FRFINISH_F_ONDIE		0x02
#define FRFINISH_F_NOAUTO		0x04
#define FRFINISH_F_ONCE			0x08
#define FRFINISH_F_ISFILE		0x10	/* used internally only - don't use it directly! */


typedef void (*frfinishfunc_t)(int,void*);



int frregfinish (frfinishfunc_t func, int flags, void *arg);
int frunregfinish (frfinishfunc_t func);
int frregfile (const char *file, int flags);	/* register file for deletion */
int frunregfile (const char *file);
int frfinish ();
int frcalldie ();
void frdie (int ret, const char *msg) __attribute__((noreturn));
void frdief (int ret, const char *fmt, ...) __attribute__((format(printf, 2, 3))) __attribute__((noreturn));
void vfrdief (int ret, const char *fmt, va_list ap) __attribute__((noreturn));




#ifdef __cplusplus
}	/* extern "C" */
#endif








#endif	/* _R__FRLIB_LIB_BASE_FRFINISH_H */

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
