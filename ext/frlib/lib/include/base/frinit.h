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

#ifndef _R__FRLIB_LIB_BASE_FRINIT_H
#define _R__FRLIB_LIB_BASE_FRINIT_H

#include <string.h>
#include <fr/base/config.h>


#ifdef __cplusplus
extern "C" {
#endif


#define SETPROG(s)	{ (s)=(((s)=rindex(argv[0],'/'))?((s)+1):(argv[0])); }

#define FR_F_NONE				0x00
#define FR_F_USCORE			CF_F_USCORE		/* default */
#define FR_F_NOUSCORE		0x1000
#define FR_F_NOLOCAL_CF		CF_F_NOLOCAL
#define FR_F_NOHOME_CF		CF_F_NOHOME
#define FR_F_NOGLOBAL_CF	CF_F_NOGLOBAL
#define FR_F_NOENV_CF		CF_F_NOENV
#ifndef DEBUG
# define FR_F_DEFAULT		(FR_F_USCORE | FR_F_NOLOCAL_CF)
#else
# define FR_F_DEFAULT		(FR_F_USCORE)
#endif

/* for backward compatibility */
#define FR_FLAG_NONE		0
#define FR_FLAG_USCORE	FR_F_USCORE



int frinit (int argc, char **argv, int flags);
int frdaemonize ();
int frdaemonize_ret (int retval);
int fr_getargs (int *argc, char ***argv);
const char *fr_getprog ();
const char *fr_getprogpath ();
int fr_setflags (int flags);
int fr_addflags (int flags);
int fr_unsetflags (int flags);
int fr_getflags ();





#ifdef __cplusplus
}	/* extern "C" */
#endif










#endif	/* _R__FRLIB_LIB_BASE_FRINIT_H */

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
