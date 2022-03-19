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

#ifndef _R__FRLIB_LIB_BASE_SECURE_CF_H
#define _R__FRLIB_LIB_BASE_SECURE_CF_H


#ifdef __cplusplus
extern "C" {
#endif

#define SCF_FLAG_NONE		0
#define SCF_FLAG_STRICT		1
#define SCF_FLAG_VSTRICT2	2	/* very strict: do not use directly but SCF_FLAG VSTRICT instead */
#define SCF_FLAG_VSTRICT	(SCF_FLAG_STRICT|SCF_FLAG_VSTRICT2)



int scf_setfname (const char *fname);
int scf_getfile (const char * filename, char **buf, int *len);
int scf_askpass (const char *msg, char *passwd, int len);
int scf_verifypass (const char *cf, size_t cf_len, const char *passwd, int flags);
int scf_askverifypass (const char *msg, char *passwd, int pwdlen, const char *cf, int cflen);
int scf_askverifypass2 (const char *msg, char *passwd, int pwdlen);
int scf_doubleaskpass (char *passwd, int len);
int scf_decrypt (const char *cf, size_t cf_len, char **out, const char *passwd, int flags);
int scf_encrypt (const char *cf, char **out, int *out_len, const char *passwd);
int scf_readpass (int fd, char *passwd, int *len);
int scf_free ();
int scf_read (const char * passwd);
int scf_fdread (int fd);
int scf_askread ();
int scf_reread ();
void scf_forgetpass();
const char * scf_getpass();
int scf_havefile ();
int scf_writeout (const char *filename, const char *cf, int len);
int scf_changepass (const char *infile, const char *outfile);
int scf_create (const char * filename);
int scf_addvar (const char *infile, const char *outfile, const char *var);
int scf_rmvar (const char *infile, const char *outfile, const char *var);
int scf_encryptfile (const char *infile, const char *outfile);
int scf_list (const char *infile, const char *outfile);











#ifdef __cplusplus
}	/* extern "C" */
#endif













#endif	/* _R__FRLIB_LIB_BASE_SECURE_CF_H */

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
