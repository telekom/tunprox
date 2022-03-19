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

#ifndef _R__FRLIB_LIB_CONNECT_PIPECOMM_H
#define _R__FRLIB_LIB_CONNECT_PIPECOMM_H

#ifdef __cplusplus
extern "C" {
#endif


#define PCON_RDWR_NONE			0x00
#define PCON_RDWR_READ			0x01
#define PCON_RDWR_WRITE			0x02
#define PCON_RDWR_RDWR			0x03
#define PCON_RDWR_NOSAVE		0x08
#define PCON_RDWR_ECHOSTART	0x10
#define PCON_RDWR_EOT			0x20
#define PCON_RDWR_TILEOT		0x40
#define PCON_RDWR_NOEAT			0x80


#define PCON_EAT_NONE	0
#define PCON_EAT_NOLOG	1


extern pid_t	pcon_childpid;
extern int 		pcon_rdfd;
extern int 		pcon_wrfd;

int pcon_eatinput (int flags);
int pcon_openpipe (const char *prog, int rdwr);
int pcon_killchild ();
int pcon_reconnect ();
int pcon_readtileot(int fd);
char *pcon_gettileot();











#ifdef __cplusplus
}	/* extern "C" */
#endif








#endif	/* _R__FRLIB_LIB_CONNECT_PIPECOMM_H */


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
