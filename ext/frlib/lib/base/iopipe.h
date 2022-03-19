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

#ifndef _R__FRLIB_LIB_BASE_IOPIPE_H
#define _R__FRLIB_LIB_BASE_IOPIPE_H

#include <stdarg.h>
#include <fr/base/tmo.h>

#ifdef __cplusplus
extern "C" {
#endif


#define IOPIPE_F_NONE					0x000
#define IOPIPE_F_NODEBUG				0x001
#define IOPIPE_F_RET_EXIT_CODE		0x002
#define IOPIPE_F_NOFMT					0x004
#define IOPIPE_F_ONLY_Y					0x008
#define IOPIPE_F_USE_SH					0x010
#define IOPIPE_F_TIMEOUT_IS_ABS		0x020
#define IOPIPE_F_RETURN_IMMEDIATE	0x040
#define IOPIPE_F_WATCH_ERROR			0x080
#define IOPIPE_F_SCRIPT_IS_ARGL		0x100
#define IOPIPE_F_MULTILINE				0x200


int iopipe (const char *script, const char *in, int inlen, char **out, 
				int *outlen, int timeout, int flags);
int iopipe64 (	const char *script, const char *in, int inlen, char **out, 
					int *outlen, tmo_t timeout, int flags);

int iopipearg (const char **argl, const char *in, int inlen, char **out, 
					int *outlen, int timeout, int flags);
int iopipearg64 (	const char **argl, const char *in, int inlen, char **out, 
						int *outlen, tmo_t timeout, int flags);

/* Note: format checking cannot be used here - due to extra %y conversion */
int iopipef (	const char *in, int inlen, char **out, int *outlen, int timeout,
			 		int flags, const char *script, ...);
					/* __attribute__((format(printf, 7, 8))); */
int iopipef64 (const char *in, int inlen, char **out, int *outlen, tmo_t timeout,
			 		int flags, const char *script, ...);
					/* __attribute__((format(printf, 7, 8))); */

int viopipef (	const char *in, int inlen, char **out, int *outlen, int timeout,
			 		int flags, const char *script, va_list ap);
int viopipef64 (	const char *in, int inlen, char **out, int *outlen, tmo_t timeout,
			 			int flags, const char *script, va_list ap);

int iopipe_parsecmd (const char ***argl, char *cmd);




#define NEGWEXITSTATUS(rc)	((int)(signed char)(WEXITSTATUS(rc)))

















#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_IOPIPE_H */


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
