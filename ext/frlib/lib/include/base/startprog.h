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

#ifndef _R__FRLIB_LIB_BASE_STARTPROG_H
#define _R__FRLIB_LIB_BASE_STARTPROG_H



#ifdef __cplusplus
extern "C" {
#endif


#define STPRG_F_NONE					0x00
#define STPRG_F_WATCH_ERRORS		0x01
#define STPRG_F_NO_WATCH_ERRORS	0x02
#define STPRG_F_DAEMONIZE			0x04
#define STPRG_F_NO_DAEMONIZE		0x08
#define STPRG_F_NO_SCF				0x10
#define STPRG_F_NO_CFG				0x20
#define STPRG_F_NO_SPOOLD			0x40
#define STPRG_F_NOWAIT				0x80




int startprog (const char *prog, int flags);
int startprog2 (const char *prog, int flags, const char *cfgopt, const char *scfopt);






#ifdef __cplusplus
}	/* extern "C" */
#endif



















#endif	/* _R__FRLIB_LIB_BASE_STARTPROG_H */


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
