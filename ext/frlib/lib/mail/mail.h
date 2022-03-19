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

#ifndef _R__FRLIB_LIB_MAIL_MAIL_H
#define _R__FRLIB_LIB_MAIL_MAIL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MAIL_F_NONE				0
#define MAIL_F_NEEDS_ENCRYPT	0x01
#define MAIL_F_NEEDS_SIGN		0x02
#define MAIL_F_CONT_ON_ERROR	0x04 
#define MAIL_F_PARSE_SIG		0x08
#define MAIL_F_PASS_DN			0x10
#define MAIL_F_CHECKSIGNER		0x20
#define MAIL_F_NODECRYPT		0x40
#define MAIL_F_HEADER_ONLY		0x80
#define MAIL_F_HTML_ONLY		0x100
#define MAIL_F_TEXT_ONLY		0x200
#define MAIL_F_PREFER_HTML		0x400


#define MHPARSE_F_ALLHDRS			0x01
#define MHPARSE_F_IGN_EMPTYLINES	0x02


struct mailhdr_line {
	const char	*field;
	char			*value;
};

struct mailhdr_lines {
	struct mailhdr_line	*lines;
	size_t					numlines;
};

struct mime_part;
struct mime_parts {
	struct mime_part	*parts;
	size_t				numparts;
};

struct body_text {
	char	*text;
	int	to_be_freed;
};


struct parsed_body;
struct body_part {
	int	is_multiparted;
	union body {
		struct mime_parts	parts;
		struct body_text	body_text;
	} body;
	struct parsed_body	*parsed_body;
};

struct mime_field {
	const char	*field;
	const char	*value;
};
struct mime_fields {
	char					*first_field;
	struct mime_field	*fields;
	int					numfields;
};
struct mime_info {
	struct mime_fields	content_type_field;
	struct mime_fields	content_encoding_field;
	struct mime_fields	content_disposition_field;
	const char				*content_type;
	const char				*smime_type;
	const char				*encoding;
	const char				*charset;
	const char				*boundary;
	const char				*disposition;
	const char				*filename;
};

struct mime_part {
	/* important header lines are separated in here */
	struct mime_info			mime_info;
	struct mailhdr_lines		full_header;	/* includes the lines above */
	struct body_part			body;
};
struct parsed_body {
	char					*buffer;
	struct mime_part	part;
};

struct to_header {
	char		**to;	/* contains To: and Cc: */
	size_t	numto;
};

struct mail {
	char							*buffer;	/* do not use directly */
	/* important header lines are separated in here */
	char							*header_text;
	char							*msg_id;
	const char					*from;
	const char					*replyto;
	const char					*sender;
	struct to_header			to;
	const char					*subject;
	const char					*signer;
	struct mime_info			mime_info;
	struct mailhdr_lines		full_header;	/* includes the lines above */
	struct body_part			body;
};





int mail_parse (char*, struct mail*, int flags);
int mail_hdr_parse (struct mailhdr_lines *hdr, char **startbody, char *buf, int flags);
void mail_hdr_free (struct mailhdr_lines *hdr);
int mail_decrypt (struct mail*, int flags);

const char* mail_getheader (struct mail*, const char *field);
int mail_getbody (struct mail*, char **bodybuf, int *bodylen, int flags);

int mail_hfree (struct mail *);
int mail_free (struct mail *);

int mail_mimeparse (char *, struct mime_part *, int flags);
int mail_mimehfree (struct mime_part *);

/* Note: fulladdr will be modified */
char *mail_getaddr (char *fulladdr);


#ifdef __cplusplus
}	/* extern "C" */
#endif



#include "attach.h"












#endif	/* _R__FRLIB_LIB_MAIL_MAIL_H */


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
