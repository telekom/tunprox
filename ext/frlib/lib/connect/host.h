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

#ifndef _R__FRLIB_LIB_CONNECT_HOST_H
#define _R__FRLIB_LIB_CONNECT_HOST_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0	/* not implemented fully - so disable it for now */
# if defined Linux || defined SunOS
#	define HAVE_IP6
# endif
#endif


#ifdef HAVE_IP6
	typedef struct in_addr6	ip_t;
#else
	typedef uint32_t			ip_t;
#endif



int host_islocal (int *islocal, const char *name);
int host_getip (ip_t *ip, const char *name);
int host_getname (char *name, const char *str);
int host_getIPv4 (uint32_t *ip, const char *str);
char *host_GetLocalHostName ();
int host_ip2str (char *str, ip_t ip);
int host_str2ip (ip_t *ip, const char *str);










#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/* _R__FRLIB_LIB_CONNECT_HOST_H */

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
