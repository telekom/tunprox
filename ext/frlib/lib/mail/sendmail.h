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

#ifndef _R__FRLIB_LIB_MAIL_SENDMAIL_H
#define _R__FRLIB_LIB_MAIL_SENDMAIL_H



#include "attach.h"

#ifdef __cplusplus
extern "C" {
#endif


#define SENDMAIL_F_NONE				0x00
#define SENDMAIL_F_NO_CRYPT		0x01
#define SENDMAIL_F_NO_SIGN			0x02
#define SENDMAIL_F_CLEAR_SIGN		0x04
#define SENDMAIL_F_CHECK_CRYPTO	0x08



int sendmail (	const char *msg, const char *to, const char *from,
					const char *subject, struct attach*, int flags, char **msgid);

int smim_sendmail (	const char *msg, const char *to, const char *from,
							const char *subject, struct attach*, const char *encsignfor,
							int flags, char **msgid);
int smim_sendmail2 (	const char *msg, const char *to, const char *from,
							const char *subject, struct attach*, const char *encryptfor,
							const char *signfor, int flags, char **msgid);
int smim_sendmail_cert (const char *msg, const char *to, const char *from,
								const char *subject, struct attach*, const char *enc_cert,
								const char *signfor, int flags, char **msgid);
int smim_sendmail_clear (	const char *msg, const char *to, const char *from,
									const char *subject, struct attach*,
									const char *signfor, int flags, char **msgid);
int smim_sendmailto (	const char *msg, const char *to, const char *from,
								const char *subject, struct attach*, int flags,
								char **msgid);
int smim_sendmailidx (	const char *msg, const char *idxto, const char *from,
								const char *subject, struct attach*, int flags,
								char **msgid);


















#ifdef __cplusplus
}	/* extern "C" */
#endif









#endif	/* _R__FRLIB_LIB_MAIL_SENDMAIL_H */


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
