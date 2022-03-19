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

#ifndef _R__FRLIB_LIB_CONNECT_MULTISTREAM_H
#define _R__FRLIB_LIB_CONNECT_MULTISTREAM_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define MSTR_PROT_0		0x000
#define MSTR_PROT_1		0x001
#define MSTR_PROT_MAX	0x001
#define MSTR_PROT_MASK	0x0ff
#define MSTR_PROT_F_LE	0x100

#define MSTR_PROT_ISLE(p) 	(((p) & MSTR_PROT_F_LE) ? 1 : 0)
#define MSTR_GETPROT(p)		((p) & MSTR_PROT_MASK)
#define MSTR_PROT_VALID(p)	(MSTR_GETPROT(p) <= MSTR_PROT_MAX)


#define MSTR_PROT_MAGIC		((uint32_t)0x37bc83c0L)
#define MSTR_PROT_REVMAGIC	((uint32_t)0xc083bc37L)


/* checks wether the message is ok,
 * the following return values can occur:
 * RERR_INVALID_LEN        - message length not correct
 * RERR_INVALID_SIGNATURE  - magic number is not correct (prot 1 only)
 * RERR_CHKSUM             - one of the checksums are not correct (prot 1 only)
 * RERR_PARAM              - invalid parameters passed to function call
 * RERR_OK                 - everything is ok
 * note: the counter is not checked!
 * as for all following functions:
 * the prot parameter does contain the protocol number (0 or 1) or'ed
 * with MSTR_PROT_F_LE (if little endian is used).
 * note: the little endian flag is used for prot 0 only. for prot 1
 *       the magic number is used to determine endiness.
 */
int mstr_check (char *msg, int msglen, int prot);

/* checks header only,
 * the return values are as for mstr_check
 */
int mstr_checkheader (const char *data, int dlen, int prot);


/* the following functions do return information from the message.
 * the functions do rely on a prior check, and that at least
 * 4 (prot 0) or 12 (prot 1) bytes are passed.
 * a negative value or NULL-pointer does indicate an error.
 */

/* checks, wether the message is little endian.
 * For protocol 0 the information in prot is returned.
 * For protocol 1 the magic number is checked. This is
 * used by all other functions (apart of mstr_guessprot).
 */
int mstr_isle (char *msg, int prot);

/* does return the length of the real message.
 */
int mstr_getmsglen (char *msg, int prot);

/* does return the beginning of the real message.
 */
char *mstr_getmsg (char *msg, int prot);

/* does return the count field (0 for prot 0).
 */
int mstr_getcount (char *msg, int prot);

/* does return the channel/stream number
 */
int mstr_getchan (char *msg, int prot);
#define mstr_getstream mstr_getchan



/* allocates and creates a message with header and footer
 * the counter and stream number needs to be passed
 * for prot 0 the counter is ignored
 */
int mstr_create (	char **omsg, int *omsglen, char *msg, int msglen,
						int prot, int stream, uint16_t cnt);
int mstr_create2 (char *omsg, int *omsglen, char *msg, int msglen,
						int prot, int stream, uint16_t cnt);
int mstr_create3 (char *omsg, char *msg, int msglen,
						int prot, int stream, uint16_t cnt);
int mstr_getsendmsglen (int msglen, int prot);


/* the following 3 functions are intended for the scon 
 * fullmsg function
 */
int mstr_termprot0be (char *data, int dlen, int *tlen);
int mstr_termprot0le (char *data, int dlen, int *tlen);
int mstr_termprot1 (char *data, int dlen, int *tlen);


/* this function makes some guesses about the protocol of the message.
 * it is not necc. to pass the whole message, but just the first
 * 4 bytes (msglen must be >= 4). 
 * The function checks whether the first four bytes are equal to
 * the magic or reverse magic number and then assumes protocol 1
 * otherwise protocol 0.
 * note: it is possible but unlikely that this happens randomly
 *       on protocol 0. if one relies on protocol guessing, it is
 *       wise to avoid the usage of channel numbers 14268 and 49283
 * if msglen is >= 12 the header checksum is taken into account
 * as well. This reduces the probability of false guessing, if and
 * only if there are no transmission errors in the first 12 bytes.
 * If transmission errors are likely to occur, it is wise to pass
 * a msglen smaller than 12, to force magic-checking only.
 * 
 * The function even guesses the endiness. This is reliable for
 * protocol 1. For protocol 0 however this is just a heuristics,
 * assuming that smaller channel numbers are more probable than
 * higher numbers. The same for the message length. But giving
 * priority to channel numbers. However, for protocol 0 this can be
 * considered a fair guessing only.
 */
int mstr_guessprot (char *msg, int msglen);





#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_CONNECT_MULTISTREAM_H */

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
