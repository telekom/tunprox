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

#ifndef _R__FRLIB_LIB_MAIL_SMIME_H
#define _R__FRLIB_LIB_MAIL_SMIME_H


#ifdef __cplusplus
extern "C" {
#endif


#define SMIM_CERT_PEM	1
#define SMIM_CERT_P12	2
#define SMIM_CERT_DER	3

#define SMIM_F_NONE				0x000
#define SMIM_F_PASS_DN			0x001
#define SMIM_F_FORCE_CLEAR		0x002
#define SMIM_F_SILENT			0x004
#define SMIM_F_VERBOSE			0x008
#define SMIM_F_VERBOSEONLAST	0x010
#define SMIM_F_CHECK_SIGN		0x020
#define SMIM_F_CHECK_CRYPT		0x040
#define SMIM_F_DELCR				0x080
#define SMIM_F_NO_VALIDATE_DN	0x100

int smim_encrypt (const char *in, int ilen, char **out, int *olen, 
						const char *encrypt_for, int flags);
int smim_encrypt2 (	const char *in, int ilen, char **out, int *olen, 
							const char *cf, int flags);
int smim_encryptlc (	const char * in, int ilen, char ** out, int * olen, 
							const char * cert, int certlen, int format, int flags);

int smim_decrypt (const char *in, int ilen, char **out, int *olen, int flags);
int smim_decrypt2 (	const char *in, int ilen, char **out, int *olen,
							const char *keyfile, const char *certfile, int flags);

int smim_verify (	const char *in, int ilen, char **out, int *olen, int flags, 
						char **signer);
int smim_verify2 (const char *in, int ilen, char **out, int *olen, int flags, 
						const char *signer);

int smim_sign (const char *in, int ilen, char **out, int *olen,
					const char *signfor, int flags);


#define SMIM_FMT_NONE	0
#define SMIM_FMT_MIN    1
#define SMIM_FMT_AUTO	1
#define SMIM_FMT_DER    2
#define SMIM_FMT_B64    3
#define SMIM_FMT_PEM    4
#define SMIM_FMT_SMIME  5
#define SMIM_FMT_MAX    5
#define SMIM_VALID_FMT(fmt) ((fmt) >= SMIM_FMT_MIN && (fmt) <= SMIM_FMT_MAX)
int smim_chfmt (char *in, int inlen, int infmt, char **out, int *outlen, int outfmt);


const char *smim_email2idx (const char *email);
int smim_cmpemail (const char *email1, const char *email2);
const char *smim_idx_getfrom (const char *idx);
const char *smim_idx_getto (const char *idx);






#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/* _R__FRLIB_LIB_MAIL_SMIME_H */


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
