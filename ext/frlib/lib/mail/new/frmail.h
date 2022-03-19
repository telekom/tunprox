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
 * Portions created by the Initial Developer are Copyright (C) 2003-2015
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_MAIL_FRMAIL_H
#define _R__FRLIB_LIB_MAIL_FRMAIL_H

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



struct frmail_hdr {
	char		*hdr;
	char		*val;
	uint8_t	hdriscpy:1,
				valissplit:1,
				subfield:1,	/* sub mime field */
				conttype:1,	/* mime field of Content-Type */
				contenc:1,	/* mime field of Content-Encode */
				contdisp:1,	/* mime field of Content-Disposition */
				contid:1;	/* mime field of Content-ID */
};

struct frmail_msg {
	char		*msgid;
	char		*from;
	char		*replyto;
	char		*sender;
	char		**to;	/* contains to: and cc: */
	size_t	nto;
	char		*subject;
};

struct frmail {
	char						*buf;			/* needs to be freed if present */
	uint32_t					ismsg:1,
								hasmime:1,
								ismultipart:1,
									alternative:1,
									related:1,
									mixed:1,
									parallel:1,
									digest:1,
								isbody:1,
									isplain:1,
									ishtml:1,
								isinline:1;
	char						*conttype;
	char						*contid;
	char						*encoding;
	char						*charset;
	char						*boundary;
	char						*filename;
	struct frmail_hdr		*hdrlst;
	int				  		hdrnum;
	struct frmail_msg		*msginfo;
	union {
		struct {
			char				*full;
			struct frmail	*parts;
			int				nparts;
		};
		struct {
			char				*body_enc;
			size_t			enclen;
			char				*body_dec;
			size_t			declen;
		};
	};
};






int mail_parse (char*, struct mail*, int flags);
int mail_decrypt (struct mail*, int flags);

char * mail_getheader (struct mail*, char *field);
int mail_getbody (struct mail*, char **bodybuf, int *bodylen, int flags);

int mail_hfree (struct mail *);
int mail_free (struct mail *);

int mail_mimeparse (char *, struct mime_part *, int flags);
int mail_mimehfree (struct mime_part *);

char *mail_getaddr (char *fulladdr);


#ifdef __cplusplus
}	/* extern "C" */
#endif














#endif	/* _R__FRLIB_LIB_MAIL_FRMAIL_H */


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
