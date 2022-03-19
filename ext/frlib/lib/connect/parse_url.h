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

#ifndef _R__FRLIB_LIB_CONNECT_PARSE_URL_H
#define _R__FRLIB_LIB_CONNECT_PARSE_URL_H



#ifdef __cplusplus
extern "C" {
#endif


#define PROT_NONE			0
#define PROT_HTTP			1
#define PROT_HTTPS		2
#define PROT_SMTP			3
#define PROT_SMTPS		4
#define PROT_POP3			5
#define PROT_POP3S		6
#define PROT_IMAP			7
#define PROT_IMAPS		8
#define PROT_LDAP			9
#define PROT_LDAPS		10
#define PROT_SYNCML		11
#define PROT_SYNCMLS		12
#define PROT_FTP			13
#define PROT_TFTP			14
#define PROT_SFTP			15
#define PROT_RSYNC		16
#define PROT_FINGER		17
#define PROT_GOPHER		18

#define PURL_FLAG_NONE		0
#define PURL_FLAG_DEFAULT	1


/****************************************************************************
 * as the name says, this function parses a given url
 * and returns:
 *   - the protocoll (prot) as one of the above values (default is http, 
 *                          if not otherwise specified in the config file)
 *   - the host - attention the host variable is malloc'ed, so you
 *                          have to free it!!
 *   - the port
 *   - and the path of the file to download (this might contain http
 *            get-querries using the ?-sign). - this value is a pointer
 *            into the url string, it won't be malloc'ed, so you must NOT
 *            free it!!
 *   it is legal to pass a NULL pointer as one of the return values so it
 *   won't be returned.
 *   the default values for prot and port are assigned only if PURL_FLAG_DEFAULT
 *   is passed, otherwise the default is always 0
 ****************************************************************************/

int url_parse (	char *url, int *prot, char **host, int *port,
						const char **path, int *use_ssl, int flags);






/********************************************************************
 * make_url creates an url from its parts
 *   url is malloc'ed
 ********************************************************************/
int url_make (char **url, int prot, const char *host, int port, const char *path);



int url_map_prot (const char *sprot, int len, int *prot, int *default_port, int *use_ssl);
const char *url_prot_to_str (int prot);












#ifdef __cplusplus
}	/* extern "C" */
#endif





#endif	/* _R__FRLIB_LIB_CONNECT_PARSE_URL_H */

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
