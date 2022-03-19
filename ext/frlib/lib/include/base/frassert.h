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

#ifndef _R__FRLIB_LIB_BASE_ASSERT_H
#define _R__FRLIB_LIB_BASE_ASSERT_H


#include <assert.h>
#include <fr/base/misc.h>


#if defined __cplusplus && __GNUC_PREREQ (2,95)
# define _R__ASSERT_VOID_CAST static_cast<void>
#else
# define _R__ASSERT_VOID_CAST (void)
#endif

#ifdef NDEBUG
# define _R__ASSERT_FAIL(s,f,l,a)	(_r__assert_warn ((s),(f),(l),(a)))
# define _R__ASSERT_RERR_FAIL(s,f,l,a)	(_r__assert_rerr_warn ((s),(f),(l),(a)))
#else
# define _R__ASSERT_FAIL(s,f,l,a)	(_r__assert_fail ((s),(f),(l),(a)))
# define _R__ASSERT_RERR_FAIL(s,f,l,a)	(_r__assert_rerr_fail ((s),(f),(l),(a)))
#endif


#ifndef __ASSERT_FUNCTION
# define __ASSERT_FUNCTION    FR__FUNC2__
#endif



#define frassert(expr)                                                    \
  ((expr)                                                                 \
   ? _R__ASSERT_VOID_CAST (0)                                             \
   : _R__ASSERT_FAIL ((#expr), __FILE__, __LINE__, __ASSERT_FUNCTION))
   

#define frassert_rerr(err)                                                \
  ((RERR_ISOK(err))                                                       \
   ? _R__ASSERT_VOID_CAST (0)                                             \
   : _R__ASSERT_RERR_FAIL ((err), __FILE__, __LINE__, __ASSERT_FUNCTION))


#ifdef __cplusplus
extern "C" {
#endif

void _r__assert_fail (const char *expr, const char *file, int line, const char *func) __attribute__((noreturn));
void _r__assert_rerr_fail (int err, const char *file, int line, const char *func) __attribute__((noreturn));
void _r__assert_warn (const char *expr, const char *file, int line, const char *func);
void _r__assert_rerr_warn (int err, const char *file, int line, const char *func);


#ifdef __cplusplus
}	/* extern "C" */
#endif



















#endif	/* _R__FRLIB_LIB_BASE_ASSERT_H */

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
