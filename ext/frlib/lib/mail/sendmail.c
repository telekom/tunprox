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
#include <netdb.h>
#include <sys/types.h>
#ifdef SunOS
extern int errno;
#endif
#include <errno.h>
#include <time.h>
#include <netdb.h>


#include <fr/base/config.h>
#include <fr/base/textop.h>
#include <fr/base/slog.h>
#include <fr/base/errors.h>
#include <fr/base/iopipe.h>
#include <fr/base/random.h>
#include <fr/mail/sendmail.h>
#include <fr/mail/smime.h>


static int read_config ();
static int prepmail1 (const char*, char**);
static int prepmail2 (const char*, char**, const char**, const char*, const char*, char**);
static int prepmail3 (const char *, char **, struct attach *);
static char * getmdate();
static char * getsender();
static int dosendmail (	const char*, const char*, const char*, const char*, struct attach*,
								const char*, const char*, const char*, int, char**);

static int			config_read = 0;
static char			*def_from = NULL;
static const char	*sendmail_prog = NULL;
static int			timeout = 120;

#ifndef __USE_XOPEN
char * cuserid (char*);
#endif


int
sendmail (body, to, from, subject, attach, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	flags |= SENDMAIL_F_NO_SIGN | SENDMAIL_F_NO_CRYPT;
	return dosendmail (	body, to, from, subject, attach, NULL, NULL, NULL,
								flags, msgid);
}

int
smim_sendmail (body, to, from, subject, attach, encsignfor, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject,
						*encsignfor;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	return dosendmail (	body, to, from, subject, attach, NULL, encsignfor, 
								encsignfor, flags, msgid);
}

int
smim_sendmail2 (body, to, from, subject, attach, encryptfor, signfor, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject,
						*encryptfor, *signfor;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	return dosendmail (	body, to, from, subject, attach, NULL, encryptfor, 
								signfor, flags, msgid);
}

int
smim_sendmail_cert (body, to, from, subject, attach, enc_cert, signfor, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject,
						*enc_cert, *signfor;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	return dosendmail (	body, to, from, subject, attach, enc_cert, NULL,
								signfor, flags, msgid);
}

int
smim_sendmail_clear (body, to, from, subject, attach, signfor, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject,
						*signfor;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	flags |= SENDMAIL_F_NO_CRYPT | SENDMAIL_F_CLEAR_SIGN;
	return dosendmail (	body, to, from, subject, attach, NULL, NULL,
								signfor, flags, msgid);
}

int
smim_sendmailto (body, to, from, subject, attach, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	return dosendmail (	body, to, from, subject, attach, NULL, NULL, NULL,
								flags, msgid);
}

int
smim_sendmailidx (body, idxto, from, subject, attach, flags, msgid)
	const char		*body,
						*idxto,
						*from,
						*subject;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	const char	*to;

	to = smim_idx_getto (idxto);
	if (!from) smim_idx_getfrom (idxto);
	flags |= SENDMAIL_F_CHECK_CRYPTO;
	return dosendmail (	body, to, from, subject, attach, NULL, idxto, idxto,
								flags, msgid);
}


static
int
dosendmail (body, to, from, subject, attach, enc_cert, encryptfor,
				signfor, flags, msgid)
	const char		*body,
						*to,
						*from,
						*subject,
						*enc_cert, *encryptfor,
						*signfor;
	char				**msgid;
	struct attach	*attach;
	int				flags;
{
	char	*ibuf, *obuf;
	int	ret;
	int	sflags;

	if (!config_read) read_config();

	/* add mime header to mail */
	obuf = NULL;
	if (attach) {
		ret = prepmail3 (body, &obuf, attach);
	} else {
		ret = prepmail1 (body, &obuf);
	}
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	/* change \n into \r\n */
	ibuf = obuf;
	obuf = NULL;
	ret = top_unix2dos (ibuf, &obuf);
	if (!RERR_ISOK(ret)) {
		obuf = ibuf;
	} else {
		free (ibuf);
	}
	/* sign mail */
	if (!(flags & SENDMAIL_F_NO_SIGN)) {
		ibuf = obuf;
		obuf = NULL;
		sflags = (flags & SENDMAIL_F_CLEAR_SIGN) ? SMIM_F_FORCE_CLEAR : 0;
		sflags |= (flags & SENDMAIL_F_CHECK_CRYPTO) ? SMIM_F_CHECK_SIGN : 0;
		if (!signfor) signfor = smim_email2idx (to);
		ret = smim_sign (ibuf, 0, &obuf, NULL, signfor, sflags);
		free (ibuf); ibuf = NULL;
		if (!RERR_ISOK(ret)) {
			return ret;
		}
	} else {
		ibuf = NULL;
	}
	if (!(flags & SENDMAIL_F_NO_CRYPT)) {
		/* encrypt mail */
		ibuf = obuf;
		obuf = NULL;
		sflags = (flags & SENDMAIL_F_CHECK_CRYPTO) ? SMIM_F_CHECK_CRYPT : 0;
		if (enc_cert) {
			ret = smim_encrypt2 (ibuf, 0, &obuf, NULL, enc_cert, sflags);
		} else {
			if (!encryptfor) encryptfor = smim_email2idx (to);
			ret = smim_encrypt2 (ibuf, 0, &obuf, NULL, encryptfor, sflags);
		}
		free (ibuf); ibuf = NULL;
		if (!RERR_ISOK(ret)) {
			return ret;
		}
	}
	/* add other header lines to mail */
	ibuf = obuf;
	obuf = NULL;
	ret = prepmail2 (ibuf, &obuf, &to, from, subject, msgid);
	free (ibuf); ibuf = NULL;
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	/* mail is ready for sending now */

	ret = iopipef (obuf, 0, NULL, NULL, timeout, 0, "%s %s", 
						sendmail_prog, to);
	free (obuf);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending mail: %s", rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


static
int
prepmail3 (in, out, attach)
	const char		*in;
	char				**out;
	struct attach	*attach;
{
	char				*obuf;
	int				size, i;
	struct attach	*p;
	char				randb[16];
	char				boundary[32];
	int				len;
	char				*b64out;
	int				ret;
	const char		*final;

	if (!in || !out || !attach) return RERR_PARAM;
	/* calculate size */
	for (p=attach, i=size=0; p->content; p++, i++) {
		size += p->content_len * 3 / 2 + strlen (p->content_type) 
					+ strlen (p->filename) + 256;
	}
	size += 512 + strlen (in) + 256;
	/* get boundary */
	frnd_getbytes (randb, 16);
	top_base64encode_short ((char*)randb, 16, boundary+2);
	boundary[0] = boundary[1] = '-';
	obuf = malloc (size+2);
	if (!obuf) return RERR_NOMEM;
	final = "";
	if (!attach || !attach->content) final = "--";
	snprintf (obuf, size, "Content-Type: multipart/mixed; boundary=\"%s\"\n"
					"Content-Disposition: inline\n"
					"Content-Transfer-Encoding: 8bit\n\n"
					"\n%s\n"
					"Content-Type: text/plain; charset=iso-8859-1\n"
					"Content-Disposition: inline\n"
					"Content-Transfer-Encoding: 8bit\n\n"
					"%s\n\n%s%s\n",
					boundary+2, boundary, in, boundary, final);
	obuf[size-1]=0;
	for (p=attach; p&&p->content; p++) {
		len = strlen (obuf);
		if (p->ismime) {
			snprintf (obuf+len, size-len, "%s\n%s\n", p->content, boundary);
		} else {
			if (!p->no_base64) {
				ret = top_base64encode (p->content, p->content_len, &b64out);
				if (!RERR_ISOK(ret)) {
					free (obuf);
					return ret;
				}
			} else {
				b64out=p->content;
			}
			if (!(p+1)->content) final = "--";
			snprintf (obuf+len, size-len, "Content-Type: %s\n"
						"Content-Disposition: attachment; filename=\"%s\"\n"
						"Content-Transfer-Encoding: %s\n\n"
						"%s\n%s%s\n", p->content_type, p->filename, 
						p->no_base64?"8bit":"base64", b64out, boundary, final);
			if (!p->no_base64) {
				if (b64out) free (b64out);
			}
		}
		obuf[size-1]=0;
	}
	strcat (obuf, "\n");
	*out = obuf;
	return RERR_OK;
}

static
int
prepmail1 (in, out)
	const char	*in;
	char			**out;
{
	char	*obuf;

	if (!in || !out) {
		FRLOGF (LOG_ERR, "%s is NULL", !in?"in":"out");
		return RERR_PARAM;
	}
	obuf = malloc (strlen (in) + 256);
	if (!obuf) {
		return RERR_NOMEM;
	}
	strcpy (obuf, "Content-Type: text/plain; charset=iso-8859-1\n"
					"Content-Disposition: inline\n"
					"Content-Transfer-Encoding: 8bit\n\n");
	strcat (obuf, in);
	strcat (obuf, "\n");
	*out = obuf;

	return RERR_OK;
}

#define HOSTNAMELEN	1024
static char	xmailer[] = "X-Mailer: P.E.C.-Buffer_CA\n";
static int hashostname = 0;
static char domainname[HOSTNAMELEN];
static char hostname[HOSTNAMELEN];
static char * user = NULL;



static
int
prepmail2 (in, out, to, from, subject, msgid)
	char			**out;
	const char	*in, **to, *from, *subject;
	char			**msgid;
{
	char			*obuf, *s2;
	char			message_id[1024];
	int			msglen;
	const char	*date, *sender;
	char			randb[16];
	char			randa[25];

	if (!in || !out || !to) {
		FRLOGF (LOG_ERR, "%s is NULL", !in?"in":!out?"out":"to");
		return RERR_PARAM;
	}
	if (!from) from = def_from;
	if (!from) return RERR_NOMEM;	/* hack: only in that case def_from is null */
	if (!subject) subject = "(no subject)";

	date = getmdate ();
	sender = getsender ();
	if (!sender) sender = "";

	/* create a provisorial message id */
	bzero (message_id, 1024);
	snprintf (message_id, 1023, "Message-ID: <1234567890123456789012%s%s@%s>\n",
				(msgid&&*msgid)?".":"", (msgid&&*msgid)?*msgid:"", domainname);
	/* create message, with the provisorial message_id */
	msglen = strlen (message_id) + strlen (*to) + 5 + strlen (from) + 7
			+ strlen (subject) + 10 + strlen (date) + strlen (sender)
			+ strlen (in) + strlen (xmailer) + 4;
	obuf = malloc (msglen);
	if (!obuf) {
		return RERR_NOMEM;
	}
	bzero (obuf, msglen);
	snprintf (obuf, msglen-1, "%s%sFrom: %s\n%sTo: %s\nSubject: %s\n"
							"%s%s\n", message_id, date, from, sender,
							*to, subject, xmailer, in);
	/* calculate message_id - random number */
	frnd_getbytes (randb, 16);
	top_base64encode_short (randb, 16, randa);
	memcpy (obuf+13, randa, 22);
	memcpy (message_id+13, randa, 22);
	if (msgid) {
		*msgid = s2 = strdup (message_id+12);
		if (s2) {
			s2+=strlen(s2);
			for (s2--; s2>=*msgid && isspace (*s2); s2--) {}; s2++;
			*s2=0;
		} else {
			FRLOGF (LOG_WARN, "could not copy messagid: %s",
						rerr_getstr3(RERR_NOMEM));
		}
	}

	*out = obuf;
	return RERR_OK;
}

static
char *
getmdate ()
{
	time_t		t;
	struct tm	* ts;
	static char	timestr [128];

	time (&t);
#ifdef Linux
	ts = localtime (&t);
	strftime (timestr, sizeof (timestr)-1, "Date: %a, %d %b %Y %T %z\n", ts);
#endif
#ifdef SunOS
	ts = gmtime (&t);
	strftime (timestr, sizeof (timestr)-1, "Date: %a, %d %b %Y %T +0000\n", ts);
#endif
	return timestr;
}


static
char *
getsender ()
{
	static char	*sender = NULL;

	if (sender) return sender;
	if (!hashostname) {
		hashostname = 1;
		if (gethostname (hostname, HOSTNAMELEN) != 0) {
			FRLOGF (LOG_WARN, "cannot get hostname: %s",
						rerr_getstr3(RERR_SYSTEM));
			strcpy (hostname, "<NULL>");
			hashostname=0;
		}
		hostname[HOSTNAMELEN-1] = 0;
		if (!*hostname) {
			strcpy (hostname, "localhost");
		}
		if (getdomainname (domainname, HOSTNAMELEN) != 0) {
			FRLOGF (LOG_WARN, "cannot get domainname: %s",
						rerr_getstr3(RERR_SYSTEM));
			strcpy (domainname, "<NULL>");
			hashostname=0;
		}
		domainname[HOSTNAMELEN-1]=0;
		if (!*domainname) {
			strcpy (domainname, "localdomain");
		}
	}
	if (!user) {
		user = cuserid (NULL);
		if (!user) 
			user = getenv ("USER");
		if (user) user = strdup (user);
	}

	sender = malloc (strlen (domainname) + strlen (hostname) + 
					(user ? strlen (user) : 6) + 16);
	if (!sender) {
		return NULL;
	}
	sprintf (sender, "Sender: %s@%s.%s\n", user?user:"<NULL>", hostname, domainname);

	return sender;
}



static
int
read_config()
{
	const char	*s;

	cf_begin_read ();
	if (def_from) free (def_from);
	s = cf_getval ("sendmail_default_from");
	if (s) {
		def_from = strdup (s);
	} else {
		s = getenv ("USER");
		if (!s) s = "nobody";
		def_from = malloc (strlen (s) + 12);
		if (def_from) {
			sprintf (def_from, "%s@localhost", s);
		}
	}
	sendmail_prog = cf_getarr2 ("prog", "sendmail", "/usr/bin/sendmail");
	timeout = cf_atoi (cf_getval2 ("sendmail_timeout", "120"));
	config_read = 1;
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
