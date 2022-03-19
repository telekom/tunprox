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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <assert.h>

#include <fr/base/errors.h>
#include <fr/base/textop.h>
#include <fr/base/strcase.h>
#include <fr/base/slog.h>
#include <fr/base/md5sum.h>
#include <fr/mail/mail.h>
#include <fr/mail/smime.h>




static int split_multipart (char *, struct mime_parts *, const char *, int);
static int hfree_mime_parts (struct mime_parts *);
static int split_header (char *, struct mailhdr_lines *, char **);
static int hfree_header (struct mailhdr_lines *);
static int get_mime_info (struct mailhdr_lines *, struct mime_info *);
static int hfree_mime_info (struct mime_info *);
static int get_to (struct mailhdr_lines *, struct to_header *);
static int hfree_to (struct to_header *);
static int parse_body (	char *, char *, struct mime_info *, 
								struct body_part *, int);
static int hfree_body (struct body_part *);
static int decrypt_mime_part (struct mime_part *, int, char**);
static int decrypt_body (struct body_part*, const char*, const char *, int, char**);
static char * get_msgid (const char * msg_id, const char * mail);
static int hfree_mime_fields (struct mime_fields *);
static int split_mime_line (char *line, struct mime_fields *);
static const char *search_mime_field (struct mime_fields *, const char *);
static char * search_header (struct mailhdr_lines *, const char *header);
static int do_mail_decrypt (struct mail*, int, char**);
static int do_mail_decrypt2 (struct mail*, int, char**);
static int do_mail_parse (char*, struct mail*, int);
static int do_mail_getbody2 (struct body_part*, struct mime_info*, char**, int*, int);
static int do_mail_getbody (struct body_part*, struct mime_info*, char **, int*, int);
static int isbody (struct mime_info*);


/* the following is for debugging purpose only */
#define STOP_ON_INVALID_SIG		0	/* may be 0 (false) or 1 (true) */


int
mail_parse (msg, mail, flags)
	char			*msg;
	struct mail	*mail;
	int			flags;
{
	int	ret;

	if (!msg || !mail) return RERR_PARAM;
	ret = do_mail_parse (msg, mail, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (flags & MAIL_F_NODECRYPT) return RERR_OK;
	return mail_decrypt (mail, flags);
}


int
mail_decrypt (mail, flags)
	struct mail	*mail;
	int			flags;
{
	int	ret;
	char	*idx=NULL;

	ret = do_mail_decrypt (mail, flags, &idx);
	if (RERR_ISOK(ret) || (ret == RERR_INVALID_SIGNATURE 
							&& (flags & MAIL_F_CONT_ON_ERROR))) {
		mail->signer = idx;
	} else {
		if (idx) free (idx);
		mail_hfree (mail);
	}
	return ret;
}


int
mail_getbody (mail, bodybuf, bodylen, flags)
	struct mail	*mail;
	char			**bodybuf;
	int			*bodylen;
	int			flags;
{
	if (!bodybuf) return RERR_PARAM;
	return do_mail_getbody (&(mail->body), &(mail->mime_info), bodybuf, 
									bodylen, flags);
}



static
int
do_mail_parse (buffer, mail, flags)
	char			*buffer;
	struct mail	*mail;
	int			flags;
{
	char			*buf;
	char			*bodytext;
	char			*buf_sav;
	int			ret;
	char			*s, c;
	const char	*sc;

	if (!buffer || !mail) return RERR_PARAM;
	bzero (mail, sizeof (struct mail));

	/* test if "From " line exists and cut it off */
	if (!strncmp (buffer, "From ", 5)) {
		buf = index (buffer, '\n');
		if (!buf) return RERR_INVALID_MAIL;
		buf++;
	} else {
		buf = buffer;
	}
	if (!*buf) return RERR_INVALID_MAIL;

	/* save buffer before splitting it, maybe we still need it */
	buf_sav = strdup (buf);
	if (!buf_sav) return RERR_NOMEM;

	/* get header as text */
	for (s=buf; *s; s++) {
		if (*s=='\n' && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n'))) {
			s++; c=*s; *s=0;
			mail->header_text = strdup (buf);
			*s=c;
			break;
		}
	}

	/* split the header into lines */
	ret = split_header (buf, &(mail->full_header), &bodytext);
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		return ret;
	}
	
	/* get From:, Reply-To:, Subject: and Sender: lines */
	mail->from = search_header (&(mail->full_header), "From");
	mail->replyto = search_header (&(mail->full_header), "Reply-To");
	mail->sender = search_header (&(mail->full_header), "Sender");
	mail->subject = search_header (&(mail->full_header), "Subject");
	sc = search_header (&(mail->full_header), "Message-ID");
	mail->msg_id = get_msgid (sc, buf_sav);
	if (!mail->msg_id) {
		free (buf_sav);
		hfree_header (&(mail->full_header));
		return RERR_NOMEM;
	}

	ret = get_mime_info (&(mail->full_header), &(mail->mime_info));
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		free (mail->msg_id);
		hfree_header (&(mail->full_header));
		return ret;
	}

	ret = get_to (&(mail->full_header), &(mail->to));
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		free (mail->msg_id);
		hfree_header (&(mail->full_header));
		hfree_mime_info (&(mail->mime_info));
		return ret;
	}

	ret = parse_body (bodytext, buf_sav, &(mail->mime_info), &(mail->body), flags);
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		free (mail->msg_id);
		hfree_header (&(mail->full_header));
		hfree_mime_info (&(mail->mime_info));
		hfree_to (&(mail->to));
		return ret;
	}

	mail->buffer = buffer;

	return RERR_OK;
}


int
mail_hfree (mail)
	struct mail	* mail;
{
	if (!mail) return 0;
	if (mail->buffer) free (mail->buffer);
	if (mail->msg_id) free (mail->msg_id);
	if (mail->header_text) free (mail->header_text);
	hfree_to (&(mail->to));
	hfree_mime_info (&(mail->mime_info));
	hfree_header (&(mail->full_header));
	hfree_body (&(mail->body));
	bzero (mail, sizeof (struct mail));
	return 1;
}

int
mail_free (mail)
	struct mail	* mail;
{
	if (!mail) return 0;
	mail_hfree (mail);
	free (mail);
	return 1;
}

void
mail_hdr_free (hdr)
	struct mailhdr_lines	*hdr;
{
	if (!hdr) return;
	if (hdr->lines) free (hdr->lines);
	*hdr = (struct mailhdr_lines) { .numlines = 0, };
}

static
int
hfree_body (body)
	struct body_part	* body;
{
	if (!body) return 0;
	if (!body->is_multiparted) {
		if (body->body.body_text.to_be_freed && body->body.body_text.text)
			free (body->body.body_text.text);
	} else {
		hfree_mime_parts (&(body->body.parts));
	}
	if (body->parsed_body) {
		mail_mimehfree (&(body->parsed_body->part));
		free (body->parsed_body->buffer);
		free (body->parsed_body);
	}
	bzero (body, sizeof (struct body_part));
	return 1;
}

static
int
split_multipart (buffer, parts, boundary, flags)
	char					*buffer;
	struct mime_parts	*parts;
	const char			*boundary;
	int					flags;
{
	char					*rbound;
	char					*spart, *s;
	size_t				boundlen;
	struct mime_part	part;
	struct mime_part	*myparts;
	int					ret;

	if (!buffer || !parts) return RERR_PARAM;
	if (!boundary) return RERR_INVALID_MAIL;
	bzero (parts, sizeof (struct mime_parts));

	rbound = malloc (strlen (boundary) + 4);
	if (!rbound) return RERR_NOMEM;
	sprintf (rbound, "--%s", boundary);
	boundlen = strlen (rbound);

	s = strstr (buffer, rbound);
	if (!s) {
		free (rbound);
		return RERR_INVALID_MAIL;
	}
	spart = s+boundlen;
	if (*spart == '\n') {
		spart++;
	} else {
		return RERR_INVALID_MAIL;
	}
	s = strstr (spart, rbound);
	if (!s) {
		free (rbound);
		return RERR_INVALID_MAIL;
	}
	do {
		if (s>=spart+2) {
			if (*(s-1) == '\n' && *(s-2) == '\n') *(s-1) = 0;
		} else if (s==spart+1) {
			if (*(s-1) == '\n') *(s-1) = 0;
		}
		*s = 0;
		/* scan part */
		ret = mail_mimeparse (spart, &part, flags);
		if (!RERR_ISOK(ret)) {
			free (rbound);
			hfree_mime_parts (parts);
			return ret;
		}
		myparts = realloc (parts->parts, (parts->numparts+1) * 
						sizeof (struct mime_part));
		if (!myparts) {
			free (rbound);
			//hfree_mime_parts (parts);
			return RERR_NOMEM;
		}
		parts->parts = myparts;
		parts->parts[parts->numparts] = part;
		parts->numparts++;
		spart = s+boundlen;
		if (*spart == '\n') {
			spart++;
		} else if (!strncmp (spart, "--\n", 3)) {
			break;
		} else {
			return RERR_INVALID_MAIL;
		}
		s = strstr (spart, rbound);
	} while (s);
	free (rbound);

	return RERR_OK;
}


static
int
hfree_mime_parts (parts)
	struct mime_parts	*parts;
{
	int	i;

	if (!parts) return 0;
	for (i=0; i<(ssize_t)parts->numparts; i++) {
		mail_mimehfree (&(parts->parts[i]));
	}
	free (parts->parts);
	bzero (parts, sizeof (struct mime_parts));
	return 1;
}


int
mail_mimehfree (part)
	struct mime_part	*part;
{
	if (!part) return 0;
	hfree_mime_info (&(part->mime_info));
	hfree_header (&(part->full_header));
	hfree_body (&(part->body));
	bzero (part, sizeof (struct mime_part));
	return 1;
}



int
mail_mimeparse (buffer, part, flags)
	char					*buffer;
	struct mime_part	*part;
	int					flags;
{
	char	*bodytext;
	char	*buf_sav;
	int	ret;

	if (!buffer || !part) return RERR_PARAM;
	bzero (part, sizeof (struct mime_part));

	/* save buffer before splitting it, maybe we still need it */
	buf_sav = strdup (buffer);
	if (!buf_sav) return RERR_NOMEM;

	/* split the header into lines */
	ret = split_header (buffer, &(part->full_header), &bodytext);
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		return ret;
	}
	
	ret = get_mime_info (&(part->full_header), &(part->mime_info));
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		hfree_header (&(part->full_header));
		return ret;
	}

	ret = parse_body (bodytext, buf_sav, &(part->mime_info), &(part->body), flags);
	if (!RERR_ISOK(ret)) {
		free (buf_sav);
		hfree_header (&(part->full_header));
		hfree_mime_info (&(part->mime_info));
		return ret;
	}

	return RERR_OK;
}


static
int
hfree_header (header)
	struct mailhdr_lines	*header;
{
	if (!header) return 0;
	if (header->lines) free (header->lines);
	bzero (header, sizeof (struct mailhdr_lines));
	return 1;
}

static
int
hfree_mime_info (mime)
	struct mime_info	*mime;
{
	if (!mime) return 0;
	hfree_mime_fields (&(mime->content_type_field));
	hfree_mime_fields (&(mime->content_encoding_field));
	hfree_mime_fields (&(mime->content_disposition_field));
	bzero (mime, sizeof (struct mime_info));
	return 1;
}

static
int
split_header (buffer, header, startbody)
	char							*buffer;
	struct mailhdr_lines		*header;
	char							**startbody;	/* return value */
{
	return mail_hdr_parse (header, startbody, buffer, 0);
}


int
mail_hdr_parse (header, startbody, buffer, flags)
	struct mailhdr_lines		*header;
	char							**startbody;	/* return value */
	char							*buffer;
	int							flags;
{
	int	i, numheader;
	char	*s, *line, *el=NULL;

	/* count upper bound for header lines */
	for (numheader=0, s=buffer; *s; s++) {
		if (*s == '\n') {
			numheader++;
			if (*(s+1) == '\n') break;
		}
	}
	header->numlines = numheader;
	header->lines = calloc (sizeof (struct mailhdr_line), numheader);
	if (!header->lines) {
		return RERR_NOMEM;
	}
	/* break up header */
	i=0; line=buffer;
	while (1) {
		if (!*line) break;
		if (*line == '\n') { 
			line++;
			if (flags & MHPARSE_F_IGN_EMPTYLINES) {
				continue;
			} else {
				break;
			}
		}
		el = index (line, '\n');
		while (el && iswhite (*(el+1)) && *(el+1) != '\n')
			el = index (el+1, '\n');
		if (el) *el = 0;
		s = index (line, ':');
		if (!s) {
			if (!el) break;
			line = el+1;
			continue;
		}
		*s = 0;
		s = top_skipwhite (s+1);
		sswitch (line) {
		sicase ("Received")
		sicase ("Status")
			if (!(flags & MHPARSE_F_ALLHDRS)) break;
			/* fall thru */
		sdefault
			header->lines[i].field = line;
			header->lines[i].value = s;
			i++;
			break;
		} esac;
		if (!el) break;
		line = el+1;
	}

	if (i==0) {
		free (header->lines);
		return RERR_INVALID_MAIL;
	}
	if (i<numheader) {
		numheader = i;
		header->lines = realloc (header->lines, numheader*sizeof (struct mailhdr_line));
		if (!header->lines) {
			return RERR_NOMEM;
		}
		header->numlines = numheader;
	}

	if (startbody) {
		if (!el) {
			*startbody = NULL;
		} else {
			*startbody = line;
		}
	}

	return RERR_OK;
}


const char*
mail_getheader (mail, field)
	struct mail		*mail;
	const char		*field;
{
	if (!mail || !field) return NULL;
	return search_header (&(mail->full_header), field);
}


static
char*
search_header (header, field)
	struct mailhdr_lines		*header;
	const char					*field;
{
	int		i;

	if (!header || !field) return NULL;
	for (i=0; i<(ssize_t)header->numlines; i++) {
		if (!strcasecmp (header->lines[i].field, field)) 
			return header->lines[i].value;
	}
	return NULL;
}

static
int
get_mime_info (header, mime)
	struct mailhdr_lines		*header;
	struct mime_info			*mime;
{
	char	*s;
	int	ret;

	if (!header || !mime) return RERR_PARAM;

	bzero (mime, sizeof (struct mime_info));
	/* get Content-Type: and Encoding: lines */
	s = search_header (header, "Content-Transfer-Encoding");
	if (s) {
		ret = split_mime_line (s, &(mime->content_encoding_field));
		if (ret!=RERR_OK) {
			hfree_mime_info (mime);
			return ret;
		}
		mime->encoding = mime->content_encoding_field.first_field;
	}
	s = search_header (header, "Content-Type");
	if (s) {
		ret = split_mime_line (s, &(mime->content_type_field));
		if (ret!=RERR_OK) {
			hfree_mime_info (mime);
			return ret;
		}
		mime->content_type = mime->content_type_field.first_field;
		mime->boundary = search_mime_field (&(mime->content_type_field), 
											"boundary");
		mime->charset = search_mime_field (&(mime->content_type_field), 
											"charset");
		mime->smime_type = search_mime_field (&(mime->content_type_field),
											"smime-type");
	}
	s = search_header (header, "Content-Disposition");
	if (s) {
		ret = split_mime_line (s, &(mime->content_disposition_field));
		if (ret!=RERR_OK) {
			hfree_mime_info (mime);
			return ret;
		}
		mime->disposition = mime->content_disposition_field.first_field;
		mime->filename = search_mime_field (&(mime->content_disposition_field),
											"filename");
	}

	return RERR_OK;
}


static
const char *
search_mime_field (fields, field)
	struct mime_fields	*fields;
	const char				*field;
{
	int	i;

	if (!fields || !field) return NULL;
	for (i=0; i<fields->numfields; i++) {
		if (!strcasecmp (fields->fields[i].field, field))
			return fields->fields[i].value;
	}
	return NULL;
}

static
int
split_mime_line (line, fields)
	char						*line;
	struct mime_fields	*fields;
{
	char	*s, *s2;
	int	i,numfields;

	if (!line || !fields) return RERR_PARAM;
	bzero (fields, sizeof (struct mime_fields));
	line = top_skipwhite (line);
	line = strdup (line);
	if (!line) return RERR_NOMEM;
	fields->first_field = line;
	for (s=line,i=0; *s; s++) if (*s == ';') i++;
	fields->numfields = i;
	if (!i) {
		for (s=line+strlen (line)-1; s>=line && iswhite (*s); s--);
		s++;
		*s = 0;
		fields->fields = NULL;
		return RERR_OK;
	}
	fields->fields = calloc (sizeof (struct mime_field), i);
	if (!fields->fields) {
		free (line);
		bzero (fields, sizeof (struct mime_fields));
		return RERR_NOMEM;
	}
	s = index (line, ';');
	if (!s) {	/* very strange */
		FRLOGF (LOG_ERR, "no semicolon - very very strange");
		free (line);
		free (fields->fields);
		bzero (fields, sizeof (struct mime_fields));
		return RERR_INTERNAL;
	}
	for (s2=s-1; s2>=line && iswhite (*s2); s2--) {}; s2++;
	*s2 = 0;
	numfields=0;
	while (1) {
beg_loop:
		s = top_skipwhite (s+1);
		if (!*s) break;
		line = s;
		for (; *s; s++) {
			if (*s == '=') break;
			if (*s == ';') {	/* error condition; ignore this field */
				goto beg_loop;
			}
		}
		if (!*s) break;
		for (s2=s-1; s2>=line && iswhite (*s2); s2--) {}; s2++;
		*s2 = 0;
		fields->fields[numfields].field = line;
		s = top_skipwhite (s+1);
		if (!*s) break;
		line = s;
		if (*s == '"') {
			line++;
			s = index (line, '"');
			if (s) {
				*s = 0;
				s = index (s+1, ';');
			}
		} else if (*s == '\'') {
			line++;
			s=index (line, '\'');
			if (s) {
				*s = 0;
				s = index (s+1, ';');
			}
		} else {
			s = index (line, ';');
			if (s) {
				for (s2=s-1; s2>=line && iswhite (*s2); s2--) {}; s2++;
				*s2 = 0;
			} else {
				for (s2=line+strlen(line)-1; s2>=line && iswhite (*s2); s2--);
				s2++;
			}
		}
		fields->fields[numfields].value = line;
		numfields++;
		if (!s) break;
	}
	if (numfields != fields->numfields) {
		assert (numfields <= fields->numfields);
		fields->numfields = numfields;
		fields->fields = realloc (fields->fields, numfields);
		if (!fields->fields && numfields > 0) {
			free (fields->first_field);
			bzero (fields, sizeof (struct mime_fields));
			return RERR_NOMEM;
		}
	}

	return RERR_OK;
}


static
int
hfree_mime_fields (fields)
	struct mime_fields	*fields;
{
	if (!fields) return 0;
	if (fields->first_field) free (fields->first_field);
	if (fields->fields) free (fields->fields);
	bzero (fields, sizeof (struct mime_fields));
	return 1;
}


static
int
hfree_to (to)
	struct to_header	*to;
{
	if (!to) return 0;
	if (to->to) {
		if (to->to[0]) free (to->to[0]);
		free (to->to);
	}
	bzero (to, sizeof (struct to_header));
	return 1;
}

static
int
get_to (header, to)
	struct mailhdr_lines		*header;
	struct to_header			*to;
{
	int	i, j;
	char	*s, *sto;
	char	*s2;

	/* get To: and Cc: - fields */
	for (i=0,j=0; i<(ssize_t)header->numlines; i++) {
		if (!strcasecmp (header->lines[i].field, "To") ||
				!strcasecmp (header->lines[i].field, "Cc")) {
			j+=strlen (header->lines[i].value) + 2;
		}
	}
	sto = malloc (j);
	if (!sto) {
		return RERR_NOMEM;
	}
	*sto=0;
	for (i=0; i<(ssize_t)header->numlines; i++) {
		if (!strcasecmp (header->lines[i].field, "To") ||
				!strcasecmp (header->lines[i].field, "Cc")) {
			if (*sto) 
				strcat (sto, ", ");
			strcat (sto, header->lines[i].value);
		}
	}

	/* split up to fields */
	for (i=0, j=1, s=sto; *s; s++)
		if (*s == ',' || *s == ';') j++;
	to->numto = j;
	to->to = calloc (sizeof (char *), j);
	if (!to->to) {
		free (sto);
		return RERR_NOMEM;
	}
	to->to[0] = sto;
	for (i=0, s=sto; 1; s++) {
		if (*s == ',' || *s == ';' || !*s) {
			for (s2=s-1; *s2 && s2 >= sto && iswhite (*s2); s2--);
			s2++;
			*s2 = 0;
			if (s2 > to->to[i]) {
				for (j=0; j < i; j++) 
					if (!strcmp (to->to[i], to->to[j])) break;
				if (j == i) i++;
			}
			if (!*s) break;
			for (s++; *s && iswhite (*s); s++);
			if (!*s) break;
			to->to[i] = s;
		}
	}
	if (i < (ssize_t)to->numto) {
		to->numto = i;
		to->to = realloc (to->to, sizeof (char *) * i);
		if (!to->to) {
			free (sto);
			return RERR_NOMEM;
		}
	}
	return RERR_OK;
}



static
int
parse_body (bodytext, full_mail, mime, body, flags)
	char					*bodytext;
	char					*full_mail;
	struct mime_info	*mime;
	struct body_part	*body;
	int					flags;
{
	int			ret;
	const char	*ct;

	if (!bodytext || !full_mail || !mime || !body) {
		return RERR_PARAM;
	}
	bzero (body, sizeof (struct body_part));

	ct = mime->content_type;
	if (!ct) ct = "";
	if ((!strcasecmp (ct, "multipart/signed") && !(flags & MAIL_F_PARSE_SIG))
				|| (!strcasecmp (ct, "application/x-pkcs7-mime")
				|| !strcasecmp (ct, "application/pkcs7-mime"))) {
		body->body.body_text.text = full_mail;
		body->body.body_text.to_be_freed = 1;
		body->is_multiparted = 0;
	} else if (strncasecmp (ct, "multipart", 9)) {
		free (full_mail);
		body->body.body_text.text = bodytext;
		body->body.body_text.to_be_freed = 0;
		body->is_multiparted = 0;
	} else {
		body->is_multiparted = 1;
		ret = split_multipart (bodytext, &(body->body.parts), mime->boundary, 
									flags);
		if (!RERR_ISOK(ret)) {
			return ret;
		}
		free (full_mail);
	}

	return RERR_OK;
}


static
int
decrypt_body (body, content_type, smime_type, flags, signer)
	struct body_part	*body;
	const char			*content_type, *smime_type;
	int					flags;
	char					**signer;
{
	int	ret=RERR_OK, ret2;
	char	*obuf;
	int	hasdone = 0;
	int	i;
	int	do_free_obuf = 1;
	int	parse_flags = 0;
	int	vflags = 0;

	if (!body) return RERR_PARAM;
	if (!content_type || !*content_type) return RERR_OK;
	
	if (flags & MAIL_F_PASS_DN) {
		vflags |= SMIM_F_PASS_DN;
	}
	/* walk thru tree and decrypt */
	if (!body->parsed_body) {
		if (!strcasecmp (content_type, "multipart/signed") || (smime_type &&
				(!strcasecmp (content_type, "application/x-pkcs7-mime") ||
				!strcasecmp (content_type, "application/pkcs7-mime")) &&
				!strcasecmp (smime_type, "signed-data"))) {
			ret = smim_verify (	body->body.body_text.text, -1, &obuf, NULL,
									vflags, signer);
			if (ret == RERR_INVALID_SIGNATURE) {
				if (!(flags & MAIL_F_CONT_ON_ERROR) || STOP_ON_INVALID_SIG)
					return ret;
				obuf = body->body.body_text.text;
				do_free_obuf = 0;
				parse_flags = MAIL_F_PARSE_SIG;
			} else if (!RERR_ISOK(ret)) {
				return ret;
			}
			hasdone = 1;
		} else if (!strcasecmp (content_type, "application/x-pkcs7-mime")
					|| !strcasecmp (content_type, "application/pkcs7-mime")) {
			ret = smim_decrypt (body->body.body_text.text, -1, &obuf, NULL, 0);
			if (!RERR_ISOK(ret)) {
				ret2 = smim_verify (	body->body.body_text.text, -1, &obuf, 
										NULL, vflags, signer);
				if (ret2 == RERR_INVALID_SIGNATURE) {
					if (!(flags & MAIL_F_CONT_ON_ERROR) || STOP_ON_INVALID_SIG)
						return ret;
					obuf = body->body.body_text.text;
					do_free_obuf = 0;
					parse_flags = MAIL_F_PARSE_SIG;
				} else if (ret2 != RERR_OK) {
					return ((ret==RERR_SCRIPT)?RERR_DECRYPTING:ret);
				}
			}
			hasdone = 1;
		}
	}
	if (hasdone) {
		body->parsed_body = malloc (sizeof (struct parsed_body));
		if (!body->parsed_body) {
			if (do_free_obuf) free (obuf);
			return RERR_NOMEM;
		}
		ret2 = mail_mimeparse (obuf, &(body->parsed_body->part), 
									parse_flags);
		if (ret2 != RERR_OK) {
			if (do_free_obuf) free (obuf);
			return ret2;
		}
		if (do_free_obuf) {
			body->parsed_body->buffer = obuf;
		} else {
			body->parsed_body->buffer = NULL;
		}
		ret2 = decrypt_mime_part (&(body->parsed_body->part), flags, signer);
		return ret2==RERR_OK?ret:ret2;
	}
	if (body->parsed_body) {
		return decrypt_mime_part (&(body->parsed_body->part), flags, signer);
	} else if (body->is_multiparted) {
		for (i=0; i<(ssize_t)body->body.parts.numparts; i++) {
			ret2 = decrypt_mime_part (&(body->body.parts.parts[i]), flags, signer);
			if (ret2 == RERR_INVALID_SIGNATURE) {
				ret = ret2;
				if (!(flags & MAIL_F_CONT_ON_ERROR)) return ret;
			} else if (ret2 != RERR_OK) return ret2;
		}
		return ret;
	}
	return ret;
}



static
int
decrypt_mime_part (mime, flags, signer)
	struct mime_part	*mime;
	int					flags;
	char					**signer;
{
	if (!mime) return RERR_PARAM;
	return decrypt_body (&(mime->body), mime->mime_info.content_type, 
						 	mime->mime_info.smime_type, flags, signer);
}





				




#define IS_SIGNED(mime_info)	((mime_info)->content_type && \
				(!strcasecmp ((mime_info)->content_type, \
				"multipart/signed") || (!strcasecmp (\
				(mime_info)->content_type, "application/x-pkcs7-mime") \
				|| !strcasecmp ((mime_info)->content_type, \
				"application/pkcs7-mime"))))
#define IS_SIGNED_PART(part)	(IS_SIGNED(&((part)->mime_info)))
static
int
do_mail_decrypt (mail, flags, signer)
	struct mail	*mail;
	int			flags;
	char			**signer;
{
	int					ret;
	int					issigned, isencrypt;
	struct mime_part	*part;
	int					checksigner;
	const char			*idxfrom;

	if (!mail || !signer) return RERR_PARAM;
	checksigner = (flags & MAIL_F_CHECKSIGNER) ? 1 : 0;
	if (flags & MAIL_F_PASS_DN) checksigner = 0;

	ret = do_mail_decrypt2 (mail, flags, signer);
	if (ret == RERR_INVALID_SIGNATURE) {
		if (!(flags & MAIL_F_CONT_ON_ERROR)) return ret;
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	isencrypt = (mail->mime_info.content_type && (!strcasecmp (
					mail->mime_info.content_type, "application/x-pkcs7-mime")
					|| !strcasecmp (mail->mime_info.content_type, 
					"application/pkcs7-mime"))
					&& mail->body.parsed_body);
	if (isencrypt) {
		part = &(mail->body.parsed_body->part);
		issigned = IS_SIGNED_PART (part);
	} else {
		issigned = IS_SIGNED (&(mail->mime_info));
	}
	if (!isencrypt && (flags & MAIL_F_NEEDS_ENCRYPT)) {
		return RERR_MAIL_NOT_ENCRYPTED;
	}
	if (!issigned && (flags & MAIL_F_NEEDS_SIGN)) {
		return RERR_MAIL_NOT_SIGNED;
	}
	if (issigned && checksigner) {
		if (!*signer) return RERR_INVALID_SIGNATURE;
		idxfrom = smim_idx_getfrom (*signer);
		if (!idxfrom) return RERR_INVALID_SIGNATURE;
		if (smim_cmpemail (idxfrom, mail->from) != 0) {
			return RERR_INVALID_SIGNATURE;
		}
	}
	return RERR_OK;
}

static
int
do_mail_decrypt2 (mail, flags, signer)
	struct mail	*mail;
	int			flags;
	char			**signer;
{
	if (!mail) return RERR_PARAM;
	return decrypt_body (&(mail->body), mail->mime_info.content_type, 
								mail->mime_info.smime_type, flags, signer);
}


static
char*
get_msgid (msg_id, mail)
	const char	*msg_id;
	const char	*mail;
{
	char	*msgid;

	if (msg_id && *msg_id) {
		msgid = strdup (msg_id);
		if (msgid) return msgid;
	}
	if (!mail) return NULL;

	msgid = malloc (48);
	if (!msgid) return NULL;
	md5sum (msgid+1, mail, strlen (mail), TENC_FMT_B64);
	msgid[0] = '<';
	strcpy (msgid+23, "@localhost>");

	return msgid;
}



static
int
do_mail_getbody (body, mime_info, obuf, olen, flags)
	struct body_part	*body;
	struct mime_info	*mime_info;
	char					**obuf;
	int					*olen;
	int					flags;
{
	int	ret;

	if (!body || !mime_info || !obuf) return RERR_PARAM;
	if ((flags & MAIL_F_HTML_ONLY) || (flags & MAIL_F_TEXT_ONLY)) {
		if ((flags & MAIL_F_HTML_ONLY) && (flags & MAIL_F_TEXT_ONLY)) {
			flags &= ~(MAIL_F_TEXT_ONLY|MAIL_F_HTML_ONLY);
		}
		ret = do_mail_getbody2 (body, mime_info, obuf, olen, flags);
	} else if (flags & MAIL_F_PREFER_HTML) {
		ret = do_mail_getbody2 (body, mime_info, obuf, olen,
											flags | MAIL_F_HTML_ONLY);
		if (RERR_ISOK(ret)) return ret;
		ret = do_mail_getbody2 (body, mime_info, obuf, olen,
											flags | MAIL_F_TEXT_ONLY);
	} else {
		ret = do_mail_getbody2 (body, mime_info, obuf, olen,
											flags | MAIL_F_TEXT_ONLY);
		if (RERR_ISOK(ret)) return ret;
		ret = do_mail_getbody2 (body, mime_info, obuf, olen,
											flags | MAIL_F_HTML_ONLY);
	}
	if (RERR_ISOK(ret)) return ret;
	if (ret != RERR_NO_VALID_ATTACH) return ret;
	*obuf = strdup ("");
	if (!*obuf) return RERR_NOMEM;
	if (olen) *olen=0;
	return RERR_OK;
}


#define mystrdup(s)		(strdup((s)?(s):""))

static
int
do_mail_getbody2 (body, mime_info, obuf, olen, flags)
	struct body_part	*body;
	struct mime_info	*mime_info;
	char					**obuf;
	int					*olen;
	int					flags;
{
	int	i, ret;

	if (!body || !mime_info || !obuf) return RERR_PARAM;
	if (body->parsed_body) {
		return do_mail_getbody2 (	&(body->parsed_body->part.body), 
											&(body->parsed_body->part.mime_info),
											obuf, olen, flags);
	} else if (body->is_multiparted) {
		for (i=0; i<(ssize_t)body->body.parts.numparts; i++) {
			ret = do_mail_getbody2 (&(body->body.parts.parts[i].body),
												&(body->body.parts.parts[i].mime_info),
												obuf, olen, flags);
			if (RERR_ISOK(ret)) return ret;
		}
		return RERR_NO_VALID_ATTACH;
	} else if (!isbody (mime_info)) {
		return RERR_NO_VALID_ATTACH;
	} else if (!body->body.body_text.text) {
		return RERR_NO_VALID_ATTACH;
	} else if (flags & MAIL_F_HTML_ONLY) {
		if (!mime_info || !mime_info->content_type ||
				(strcasecmp (mime_info->content_type, "text/html") != 0))
			return RERR_NO_VALID_ATTACH;
	} else if (flags & MAIL_F_TEXT_ONLY) {
		if (mime_info && mime_info->content_type && 
				(strcasecmp (mime_info->content_type, "text/plain") != 0))
			return RERR_NO_VALID_ATTACH;
	} 
	if (mime_info->encoding) {
		sswitch (mime_info->encoding) {
		sincase ("base64")
			ret = top_base64decode (body->body.body_text.text, obuf, olen);
			if (!RERR_ISOK(ret)) return ret;
			break;
		sicase ("quoted-printable")
			/* should be handeld separately */
			/* fall thru */
		sincase ("8bit")
		sincase ("7bit")
		sincase ("ascii 7bit")
		sincase ("us-ascii 7bit")
			/* fall thru */
		sdefault
			*obuf = mystrdup (body->body.body_text.text);
			if (!*obuf) return RERR_NOMEM;
			if (olen) *olen = strlen (*obuf);
		} esac;
	} else {
		*obuf = mystrdup (body->body.body_text.text);
		if (!*obuf) return RERR_NOMEM;
		if (olen) *olen = strlen (*obuf);
	}
	return RERR_OK;
}


static
int
isbody (mime_info)
	struct mime_info	*mime_info;
{
	if (!mime_info) return 1;
	if (mime_info->disposition && *mime_info->disposition) {
		if (!strcasecmp (mime_info->disposition, "attachment"))
			return 0;
		if (!strcasecmp (mime_info->disposition, "inline"))
			return 1;
	}
	if (mime_info->filename && *mime_info->filename) {
		return 0;
	} else if (!mime_info->content_type) {
		return 1;
	} else {
		sswitch (mime_info->content_type) {
		sincase ("text/")
			return 1;
		sdefault
			return 0;
		} esac;
	}
	return 0;
}



static const char	*mail_valid_chars = ".-_@%";
static const char	*domain_valid_chars = ".-_";
#define MD_IS_VALID_CHAR(str,c)	(isalnum(c) || ((unsigned)c)>=128 || index ((str), (c)))
#define MAIL_IS_VALID_CHAR(c)	(MD_IS_VALID_CHAR(mail_valid_chars,(c)))
#define DOMAIN_IS_VALID_CHAR(c)	(MD_IS_VALID_CHAR(domain_valid_chars,(c)))



char *
mail_getaddr (full)
	char	*full;
{
	char	*s, *at;

	if (!full) return NULL;
	at = rindex (full, '@');
	if (!at) return NULL;
	for (s=at+1; *s && DOMAIN_IS_VALID_CHAR (*s); s++);
	if (s==at+1) return NULL;
	*s=0;
	for (s=at-1; s>=full && MAIL_IS_VALID_CHAR(*s); s--);
	for (s++; *s=='@'; s++);
	if (s>=at) return NULL;
	return s;
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
