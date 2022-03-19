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
 * Portions created by the Initial Developer are Copyright (C) 2003-2015
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_BASE_UID_H
#define _R__FRLIB_LIB_BASE_UID_H

#include <sys/types.h>
#include <pwd.h>


#ifdef __cplusplus
extern "C" {
#endif



int fpw_getpwnam (struct passwd **res, char *buf, size_t blen, const char *name);
int fpw_getpwuid (struct passwd **res, char *buf, size_t blen, uid_t uid);

/* home dir */
int fpw_gethomebyname (char **res, char *buf, size_t blen, const char *name);
int fpw_gethomebyuid (char **res, char *buf, size_t blen, uid_t uid);
int fpw_getmyhome (char **res, char *buf, size_t blen);
int fpw_gethometilde (char **res, char *buf, size_t blen, const char *fn);

/* users shell */
int fpw_getshellbyname (char **res, char *buf, size_t blen, const char *name);
int fpw_getshellbyuid (char **res, char *buf, size_t blen, uid_t uid);
int fpw_getmyshell (char **res, char *buf, size_t blen);

/* user info from passwd */
int fpw_getuinfobyname (char **res, char *buf, size_t blen, const char *name);
int fpw_getuinfobyuid (char **res, char *buf, size_t blen, uid_t uid);
int fpw_getrealnamebyname (char **res, char *buf, size_t blen, const char *name);
int fpw_getrealnamebyuid (char **res, char *buf, size_t blen, uid_t uid);

/* user name <-> uid mapping */
int fpw_getnamebyuid (char **res, char *buf, size_t blen, uid_t uid);
int fpw_getuidbyname (uid_t *res, const char *name);
int fpw_getuidbyname2 (const char *name);

/* group id */
int fpw_getgidbyname (gid_t *res, const char *name);
int fpw_getgidbyname2 (const char *name);
int fpw_getgidbyuid (gid_t *res, uid_t uid);
int fpw_getgidbyuid2 (uid_t uid);



#if 0	/* example */
char buf[256];
struct passwd *pwd;
ret = fpw_getpwnam (&pwd, buf, sizeof (buf), "tex");
if (!RERR_ISOK(ret)) return ret;
...
if ((char*)pwd != buf) free (pwd);
#endif




#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_UID_H */

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
