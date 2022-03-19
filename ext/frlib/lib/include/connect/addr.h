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

#ifndef _R__FRLIB_LIB_CONNECT_ADDR_H
#define _R__FRLIB_LIB_CONNECT_ADDR_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef union {
	struct sockaddr		ad;
	struct sockaddr_in	v4;
	struct sockaddr_in6	v6;
} frad_t;


#define FRAD_FAM(a)		((a).ad.sa_family)
#define FRADP_FAM(a)		(FRAD_FAM(*a))
#define FRAD_ISIPV6(a)	(FRAD_FAM(a) == AF_INET6)
#define FRADP_ISIPV6(a)	(FRAD_ISIPV6(*a))
#define FRAD_SIZE(a)		(FRAD_ISIPV6(a) ? sizeof (struct sockaddr_in6) : \
														sizeof (struct sockaddr_in))
#define FRADP_SIZE(a)	(FRAD_SIZE(*a))

void frad_setipv4 (frad_t *ad, uint32_t ad4, uint16_t port);
void frad_setipv6 (frad_t *ad, uint8_t ad6[16], uint16_t port);
void frad_setipv4only (frad_t *ad, uint32_t ad4);
void frad_setipv6only (frad_t *ad, uint8_t ad6[16]);
void frad_setport (frad_t *ad, uint16_t port);
uint16_t frad_getport (frad_t *ad);
int frad_isany (frad_t *ad);
int frad_eq (frad_t *ad1, frad_t *ad2);
void frad_cp (frad_t *dest, frad_t *src);
ssize_t frad_sprint (char *buf, size_t blen, frad_t *addr);

#define FRAD_F_NOPORT	0x01
#define FRAD_F_NODNS		0x02
#define FRAD_F_IPV4		0x04
#define FRAD_F_IPV6		0x08
#define FRAD_F_DNSV4		0x10
#define FRAD_F_DNSV6		0x20

int frad_getaddr (frad_t *addr, const char *str, int flags);


typedef struct {
	frad_t	local, remote;
} frad_pair_t;

int frad_getaddrpair (frad_pair_t *addr, const char *str, int flags);
void frad_cppair (frad_pair_t *dest, frad_pair_t *src);






#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_CONNECT_ADDR_H */

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
