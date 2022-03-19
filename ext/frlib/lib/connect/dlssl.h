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

#ifndef _R__FRLIB_LIB_CONNECT_DLSSL_H
#define _R__FRLIB_LIB_CONNECT_DLSSL_H

#include <sys/types.h>


#ifdef  __cplusplus
extern "C" {
#endif

#include <fr/base/dlstub.h>

#define SSL_FILETYPE_PEM	1


DLSTUB_MKSTUB_A1 (SSL_accept, int, void*)
DLSTUB_MKSTUB_A1 (SSL_connect, int, void*)
DLSTUB_MKSTUB_A1 (SSL_CTX_free, void*, void*)
DLSTUB_MKSTUB_A1 (SSL_CTX_new, void*, void*)
DLSTUB_MKSTUB_A1 (SSL_free, void, void*)
DLSTUB_MKSTUB_A0 (SSL_library_init, int)
DLSTUB_MKSTUB_A0 (SSL_load_error_strings, void)
DLSTUB_MKSTUB_A1 (SSL_new, void*, void*)
DLSTUB_MKSTUB_A3 (SSL_read, int, void*, void*, int)
DLSTUB_MKSTUB_A2 (SSL_set_fd, int, void*, int)
DLSTUB_MKSTUB_A1 (SSL_shutdown, int, void*)
DLSTUB_MKSTUB_A0 (SSLv23_client_method, void*)
DLSTUB_MKSTUB_A0 (SSLv23_server_method, void*)
DLSTUB_MKSTUB_A3 (SSL_write, int, void*, const void*, int)
DLSTUB_MKSTUB_A3 (SSL_CTX_load_verify_locations, int, void*, const char*, const char*)
DLSTUB_MKSTUB_A3 (SSL_CTX_use_PrivateKey_file, int, void*, const char*, int)
DLSTUB_MKSTUB_A3 (SSL_CTX_use_certificate_file, int, void*, const char*, int)
DLSTUB_MKSTUB_A1 (SSL_CTX_check_private_key, int, void*)





#ifdef  __cplusplus
}	/* extern "C" */
#endif









#endif	/* _R__FRLIB_LIB_CONNECT_DLSSL_H */
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
