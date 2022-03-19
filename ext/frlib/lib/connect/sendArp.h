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

#ifndef _R__FRLIB_LIB_CONNECT_IF_SENDARP_H
#define _R__FRLIB_LIB_CONNECT_IF_SENDARP_H


#include <sys/types.h>
#include <netinet/in.h>




#ifdef __cplusplus
extern "C" {
#endif

#define MAC_ADDR_LEN		6

#define OP_ARP_REQUEST	1
#define OP_ARP_REPLY		2

int if_sendarp (const char *iface);
int if_sendarp2 (const char *iface, uint32_t ip, const u_char *mac, int optype);

int if_getipaddr (struct in_addr*, const char *iface);
int if_gethwaddr (u_char *obuf, const char *iface);






#ifdef __cplusplus
}	/* extern "C" */
#endif


















#endif	/* _R__FRLIB_LIB_CONNECT_IF_SENDARP_H */

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
