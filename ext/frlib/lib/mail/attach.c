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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif


#include "config.h"
#include "errors.h"
#include "attach.h"
#include "mail.h"
#include "textop.h"
#include "strcase.h"



static int numattach_body (struct body_part *, struct mime_info *);
static int search_attach (struct body_part*, int*, struct mime_info*, 
									char**, char**, char**, int*);
static int isattach (struct mime_info *);

#define mystrdup(s)		(strdup((s)?(s):""))


int
mail_numattach (mail)
	struct mail	* mail;
{
	if (!mail) return RERR_PARAM;
	return numattach_body (&(mail->body), &(mail->mime_info));
}


static
int
isattach (mime_info)
	struct mime_info	* mime_info;
{
	if (!mime_info) return 0;
	return (mime_info->disposition && (
			!strcasecmp (mime_info->disposition, "attachment") ||
			(!strcasecmp (mime_info->disposition, "inline") &&
			(mime_info->filename && *(mime_info->filename)))) && 
			(!mime_info->content_type || !(!strcasecmp (mime_info->content_type,
						"application/x-pkcs7-signature") ||
			!strcasecmp (mime_info->content_type, "application/pkcs7-signature"))));
}

static
int
numattach_body (body, mime_info)
	struct body_part	* body;
	struct mime_info	* mime_info;
{
	if (body->parsed_body) {
		return numattach_body (&(body->parsed_body->part.body),
					&(body->parsed_body->part.mime_info));
	} else if (body->is_multiparted) {
		int		i, num = 0;

		for (i=0; i<(ssize_t)body->body.parts.numparts; i++) {
			num += numattach_body (&(body->body.parts.parts[i].body),
					&(body->body.parts.parts[i].mime_info));
		}
		return num;
	} else if (isattach (mime_info)) {
		return 1;
	} else {
		return 0;
	}
	return 0;
}


int
mail_getfirstattach (mail, filename, fnsize, obuf, olen)
	struct mail	*mail;
	char			*filename;
	size_t		fnsize;
	char			**obuf;
	int			*olen;
{
	char	*filename2;
	int	ret, noa;

	if (!mail || !filename || !fnsize || !obuf) {
		return RERR_PARAM;
	}
	*filename = 0;
	noa = 0;
	ret = search_attach (&(mail->body), &noa, &(mail->mime_info), 
							&filename2, NULL, obuf, olen);
	if (RERR_ISOK(ret) && filename2) {
		strncpy (filename, filename2, fnsize);
		free (filename2);
	}
	return ret;
}


int
mail_getattach (mail, attachments, numattachments)
	struct mail		*mail;
	struct attach	**attachments;
	int				*numattachments;
{
	int					i, j, numat, noa;
	struct attach		*attach;
	int					ret;

	if (!mail || !attachments || !numattachments) return RERR_PARAM;
	numat = mail_numattach (mail);
	if (numat < 0) return numat;
	*numattachments = numat;
	attach = malloc (sizeof (struct attach) * (numat+1));
	if (!attach) return RERR_NOMEM;
	bzero (attach, sizeof (struct attach) * (numat+1));
	for (i=0, j=0; i<numat; i++) {
		noa = i;
		ret = search_attach (&(mail->body), &noa, &(mail->mime_info),
										&(attach[j].filename), 
										&(attach[j].content_type),
										&(attach[j].content),
										&(attach[j].content_len));
		if (ret == RERR_NOMEM) {
			mail_freeattach (attach, j);
			*attachments = NULL;
			return ret;
		}
		attach[j].ismime = 0;
		if (RERR_ISOK(ret)) j++;
	}
	attach[j] = ATTACH_NULL;
	*attachments = attach;
	*numattachments = j;
	return RERR_OK;
}


static
int
search_attach (	body, numofattach, mime_info, filename, content_type,
						obuf, olen)
	struct body_part	*body;
	int					*numofattach;
	struct mime_info	*mime_info;
	char					**filename, **content_type;
	char					**obuf;
	int					*olen;
{
	int	i, ret;

	if (!body || !mime_info || !obuf) {
		return RERR_PARAM;
	}
	if (body->parsed_body) {
		return search_attach (&(body->parsed_body->part.body), numofattach,
								&(body->parsed_body->part.mime_info),
								filename, content_type,
								obuf, olen);
	} else if (body->is_multiparted) {
		for (i=0; i<(ssize_t)body->body.parts.numparts; i++) {
			ret = search_attach (&(body->body.parts.parts[i].body),
								numofattach,
								&(body->body.parts.parts[i].mime_info),
								filename, content_type,
								obuf, olen);
			if (RERR_ISOK(ret)) return ret;
		}
		return RERR_NO_VALID_ATTACH;
	} else if (!isattach (mime_info)) {
		return RERR_NO_VALID_ATTACH;
#if 0
	} else if (!mime_info->filename) {
		return RERR_NO_VALID_ATTACH;
#endif
	} else if (!body->body.body_text.text) {
		return RERR_NO_VALID_ATTACH;
	} else {
		int		etype;

		if (*numofattach>0) {
			(*numofattach)--;
			return RERR_NO_VALID_ATTACH;
		}
		etype=0;
		sswitch (mime_info->encoding) {
		sicase ("quoted-printable")
			etype=2;
			break;
		sincase ("8bit")
		sincase ("7bit")
		sincase ("ascii 7bit")
		sincase ("us-ascii 7bit")
			etype=1; break;
		sincase ("base64")
			etype=3; break;
		sdefault
			etype=0; break;
		} esac;
		switch (etype) {
		case 0:		/* handel as base64 - not good, but workaround for some broken mailers */
		case 3:		/* base64 */
			ret = top_base64decode (body->body.body_text.text, obuf, olen);
			if (!RERR_ISOK(ret)) return ret;
			break;
		case 2:		/* quoted printable */
			/* should be handeld separately */
		case 1:		/* ascii */
		default:
			*obuf = mystrdup (body->body.body_text.text);
			if (!*obuf) return RERR_NOMEM;
			*olen = strlen (*obuf);
			break;
		}
		if (filename) {
			*filename = mystrdup (mime_info->filename);
		}
		if (content_type) {
			*content_type = mystrdup (mime_info->content_type);
		}
		return RERR_OK;
	}
	return RERR_NO_VALID_ATTACH;
}


int
mail_freeattach (attach, numattach)
	struct attach	*attach;
	int				numattach;
{
	if (!attach || numattach < 0) return 0;
	mail_hfreeattach (attach, numattach);
	free (attach);
	return 1;
}

int
mail_hfreeattach (attach, numattach)
	struct attach	*attach;
	int				numattach;
{
	int		i;

	if (!attach || numattach < 0) return 0;
	for (i=0; i<numattach; i++) {
		if (attach[i].filename) free (attach[i].filename);
		if (attach[i].content) free (attach[i].content);
		if (attach[i].content_type) free (attach[i].content_type);
		bzero (&(attach[i]), sizeof (struct attach));
	}
	return 1;
}



















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
