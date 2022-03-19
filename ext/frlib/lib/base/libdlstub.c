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
 * Portions created by the Initial Developer are Copyright (C) 2003-2016
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <dlfcn.h>

/* 
	dl*() stub routines for static compilation.  Prepared from
	/usr/include/dlfcn.h by Hal Pomeranz <hal@deer-run.com>
	modified by Frank Reker <frank@reker.net>
*/
int dldump(const char*, const char*, int);

void *dlopen(const char *str, int x) {return NULL;}
void *dlsym(void *ptr, const char *str) {return NULL;}
int dlclose(void *ptr) {return 0;}
//#if !defined __USE_GNU && !defined __UCLIBC__
#ifdef __ANDROID__
const
#endif
char *dlerror(void) {return (char*)"error";}
int dldump(const char *str1, const char *str2, int x) {return 0;}
#ifdef __UCLIBC__
int dlinfo(void);
int dlinfo(void) {return 0;}
#else
int dlinfo(void *ptr1, int x, void *ptr2);
int dlinfo(void *ptr1, int x, void *ptr2) {return 0;}
#endif

#ifdef __USE_GNU
void *_dlopen(const char *str, int x);
void *_dlopen(const char *str, int x) {return NULL;}
void *_dlsym(void *ptr, const char *str);
void *_dlsym(void *ptr, const char *str) {return NULL;}
int _dlclose(void *ptr);
int _dlclose(void *ptr) {return 0;}
char *_dlerror(void);
char *_dlerror(void) {return (char*)"error";}
int _dldump(const char *str1, const char *str2, int x);
int _dldump(const char *str1, const char *str2, int x) {return 0;}
#ifdef __UCLIBC__
int _dlinfo(void);
int _dlinfo(void) {return 0;}
#else
int _dlinfo(void *ptr1, int x, void *ptr2);
int _dlinfo(void *ptr1, int x, void *ptr2) {return 0;}
#endif
#endif

#if defined __USE_GNU && !defined __UCLIBC__
int dladdr(const void *ptr1, Dl_info *ptr2) {return 0;}
int _dladdr(const void *ptr1, Dl_info *ptr2);
int _dladdr(const void *ptr1, Dl_info *ptr2) {return 0;}
void *dlmopen(Lmid_t a, const char *str, int x) {return NULL;}
void *_dlmopen(Lmid_t a, const char *str, int x);
void *_dlmopen(Lmid_t a, const char *str, int x) {return NULL;}
#endif






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
