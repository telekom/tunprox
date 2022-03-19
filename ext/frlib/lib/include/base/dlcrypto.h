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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_BASE_DLCRYPTO_H
#define _R__FRLIB_LIB_BASE_DLCRYPTO_H

#include <sys/types.h>


#ifdef  __cplusplus
extern "C" {
#endif

#include <fr/base/dlstub.h>

#ifndef AES_MAXNR
# define AES_ENCRYPT     1
# define AES_DECRYPT     0
# define AES_MAXNR 		14
# define AES_BLOCK_SIZE	16
 struct aes_key_st {
    char rd_key[8 * 4 *(AES_MAXNR + 1)];  /* not the original definition, but >= in size */
    int rounds;
 };
 typedef struct aes_key_st AES_KEY;
#endif


/* in the following functions: key is of type AES_KEY, but we define it here as
   void, it's a pointer, so the abi doesn't change 
 */

DLSTUB_MKSTUB_A3 (AES_set_encrypt_key, int, unsigned char*, \
						const int, void*);
DLSTUB_MKSTUB_A3 (AES_set_decrypt_key, int, unsigned char*, \
						const int, void*);
DLSTUB_MKSTUB_A3 (AES_encrypt, int, const unsigned char*, \
						unsigned char *, const void*);
DLSTUB_MKSTUB_A3 (AES_decrypt, int, const unsigned char*, \
						unsigned char *, const void*);
DLSTUB_MKSTUB_A6 (AES_cbc_encrypt, int, const unsigned char*, \
						unsigned char*, const unsigned long, const void*, \
						unsigned char*, const int);
DLSTUB_MKSTUB_A3 (ERR_error_string_n, void, unsigned long, char*, size_t);
DLSTUB_MKSTUB_A0 (ERR_get_error, unsigned long);





#ifdef  __cplusplus
}	/* extern "C" */
#endif









#endif	/* _R__FRLIB_LIB_BASE_DLCRYPTO_H */
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
