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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif

#include "prtf.h"
#include "errors.h"



static struct rerrmap	rerrmap[] = {
	{ RERR_OK, "RERR_OK", "ok" },
	{ RERR_FALSE, "RERR_FALSE", "condition is false" },
	{ RERR_FAIL, "RERR_FAIL", "general unspecified error or false value" },
	{ RERR_EOT, "RERR_EOT", "end of text (not an error)" },
	{ RERR_REDIRECT, "RERR_REDIRECT", "connection redirected (not an error)" },
	{ RERR_DELAYED, "RERR_DELAYED", "operation delayed or scheduled for later "
						"operation" },
	{ RERR_NODATA, "RERR_NODATA", "no data available" },
	{ RERR_BREAK, "RERR_BREAK", "break encountered (not en error)" },
	{ RERR_INTERNAL, "RERR_INTERNAL", "internal error - probably an "
						"implementation error or a hard to trace back error" },
	{ RERR_SYSTEM, "RERR_SYSTEM", "error in system call" },
	{ RERR_PARAM, "RERR_PARAM", "invalid parameter" },
	{ RERR_NOMEM, "RERR_NOMEM", "out of memory" },
	{ RERR_NOT_FOUND, "RERR_NOT_FOUND", "object not found" },
	{ RERR_TIMEDOUT, "RERR_TIMEDOUT", "operation timed out" },
	{ RERR_FORBIDDEN, "RERR_FORBIDDEN", "operation forbidden" },
	{ RERR_DISABLED, "RERR_DISABLED", "requested function disabled" },
	{ RERR_NOT_SUPPORTED, "RERR_NOT_SUPPORTED", "requested function not "
						"supported or implemented" },
	{ RERR_NOT_AVAILABLE, "RERR_NOT_AVAILABLE", "requested resource not "
						"available" },
	{ RERR_NODIR, "RERR_NODIR", "not a directory" },
	{ RERR_CHILD, "RERR_CHILD", "some child process produced an error or "
						"could not be created" },
	{ RERR_SCRIPT, "RERR_SCRIPT", "the script child returned a non zero value" },
	{ RERR_CONFIG, "RERR_CONFIG", "invalid or missing configuration variable" },
	{ RERR_AUTH, "RERR_AUTH", "authentication failed" },
	{ RERR_CONNECTION, "RERR_CONNECTION", "connection error" },
	{ RERR_SSL_CONNECTION, "RERR_SSL_CONNECTION", "error in ssl connection" },
	{ RERR_SERVER, "RERR_SERVER", "server error" },
	{ RERR_UTF8, "RERR_UTF8", "invalid utf8 char" },
	{ RERR_EMPTY_BODY, "RERR_EMPTY_BODY", "mail body empty" },
	{ RERR_DECRYPTING, "RERR_DECRYPTING", "error decrypting" },
	{ RERR_INVALID_CF, "RERR_INVALID_CF", "invalid or missing config file" },
	{ RERR_INVALID_SCF, "RERR_INVALID_SCF", "invalid secure config file" },
	{ RERR_NO_CRYPT_CF, "RERR_NO_CRYPT_CF", "missing secure config file" },
	{ RERR_INVALID_DATE, "RERR_INVALID_DATE", "invalid date" },
	{ RERR_INVALID_ENTRY, "RERR_INVALID_ENTRY", "invalid entry" },
	{ RERR_INVALID_FILE, "RERR_INVALID_FILE", "invalid filename or content in "
						"invalid format" },
	{ RERR_INVALID_FORMAT, "RERR_INVALID_FORMAT", "invalid format" },
	{ RERR_INVALID_HOST, "RERR_INVALID_HOST", "invalid host" },
	{ RERR_INVALID_IP, "RERR_INVALID_IP", "invalid ip" },
	{ RERR_INVALID_MAIL, "RERR_INVALID_MAIL", "invalid mail" },
	{ RERR_INVALID_PROT, "RERR_INVALID_PROT", "invalid protocol" },
	{ RERR_INVALID_PWD, "RERR_INVALID_PWD", "invalid password" },
	{ RERR_INVALID_SIGNATURE, "RERR_INVALID_SIGNATURE","invalid signature" },
	{ RERR_INVALID_SPOOL, "RERR_INVALID_SPOOL", "invalid spool file" },
	{ RERR_INVALID_TYPE, "RERR_INVALID_TYPE", "invalid type" },
	{ RERR_INVALID_URL, "RERR_INVALID_URL", "invalid url" },
	{ RERR_INVALID_REPVAR, "RERR_INVALID_REPVAR", "invalid repository variable" },
	{ RERR_INVALID_NAME, "RERR_INVALID_NAME", "invalid name" },
	{ RERR_INVALID_SHM, "RERR_INVALID_SHM", "invalid shared memory page" },
	{ RERR_INVALID_VAL, "RERR_INVALID_VAL", "invalid value" },
	{ RERR_INVALID_VAR, "RERR_INVALID_VAR", "invalid variable" },
	{ RERR_INVALID_CMD, "RERR_INVALID_CMD", "invalid command" },
	{ RERR_INVALID_CHAN, "RERR_INVALID_CHAN", "invalid channel" },
	{ RERR_INVALID_PATH, "RERR_INVALID_PATH", "invalid path element" },
	{ RERR_INVALID_PAGE, "RERR_INVALID_PAGE", "invalid page" },
	{ RERR_INVALID_LEN, "RERR_INVALID_LEN", "invalid length" },
	{ RERR_INVALID_TAG, "RERR_INVALID_TAG", "invalid tag" },
	{ RERR_INVALID_TZ, "RERR_INVALID_TZ", "invalid timezone" },
	{ RERR_INVALID_ID, "RERR_INVALID_ID", "invalid identifier" },
	{ RERR_INVALID_MODE, "RERR_INVALID_MODE", "invalid mode" },
	{ RERR_INVALID_PARAM, "RERR_INVALID_PARAM", "invalid parameter in user input" },
	{ RERR_LOOP, "RERR_LOOP", "loop encountered" },
	{ RERR_MAIL_NOT_ENCRYPTED, "RERR_MAIL_NOT_ENCRYPTED", "mail not encrypted" },
	{ RERR_MAIL_NOT_SIGNED, "RERR_MAIL_NOT_SIGNED", "mail not signed" },
	{ RERR_NO_VALID_ATTACH, "RERR_NO_VALID_ATTACH", "mail has no valid attachment" },
	{ RERR_NOIPV4, "RERR_NOIPV4", "IPv6 is not a mapped IPv4" },
	{ RERR_NOKEY, "RERR_NOKEY", "invalid key value" },
	{ RERR_NOT_BASE64, "RERR_NOT_BASE64", "not in base64 format" },
	{ RERR_NO_XML, "RERR_NO_XML", "not an xml" },
	{ RERR_REGISTRY, "RERR_REGISTRY", "problems accessing the windows registry" },
	{ RERR_OUTOFRANGE, "RERR_OUTOFRANGE", "value out of range" },
	{ RERR_NOTINIT, "RERR_NOTINIT", "value or function not initialized yet" },
	{ RERR_ALREADY_OPEN, "RERR_ALREADY_OPEN", "object is already open" },
	{ RERR_ALREADY_EXIST, "RERR_ALREADY_EXIST", "object does already exist" },
	{ RERR_LOCKED, "RERR_LOCKED", "object is locked" },
	{ RERR_FULL, "RERR_FULL", "object is full" },
	{ RERR_INSUFFICIENT_DATA, "RERR_INSUFFICIENT_DATA", "have insufficient data "
						"to complete task" },
	{ RERR_TOO_LONG, "RERR_TOO_LONG", "object too long" },
	{ RERR_TOO_MANY_HOPS, "RERR_TOO_MANY_HOPS", "too many hops encountered" },
	{ RERR_STACK_OVERFLOW, "RERR_STACK_OVERFLOW", "stack overflow" },
	{ RERR_NOT_UNIQ, "RERR_NOT_UNIQ", "object is not unique" },
	{ RERR_SYNTAX, "RERR_SYNTAX", "syntax error in user input" },
	{ RERR_PROTOCOL, "RERR_PROTOCOL", "protocol error" },
	{ RERR_CHKSUM, "RERR_CHKSUM", "checksum error" },
	{ RERR_VERSION, "RERR_VERSION", "invalid version number" },
	{ RERR_BUSY, "RERR_BUSY", "system is busy" },
	{ RERR_TRUNCATED, "RERR_TRUNCATED", "object or write truncated" },
	{ 1, NULL, NULL }};

static int getspecialmap (int, const char **, const char**);	

struct rerrmap_list {
	struct rerrmap	**list;
	int				len;
	int				initialized;
};
static struct rerrmap_list	rerrmap_list = {
	(struct rerrmap**)&rerrmap,
	1, 0
};

int
rerr_register (newmap)
	struct rerrmap	*newmap;
{
	struct rerrmap	**p;

	if (!newmap && rerrmap_list.initialized) return RERR_PARAM;
	if (rerrmap_list.initialized) {
		p = realloc (rerrmap_list.list, (rerrmap_list.len+1)*sizeof (void*));
	} else {
		p = malloc ((newmap?2:1)*sizeof (void*));
	}
	if (!p) return RERR_NOMEM;
	rerrmap_list.list = p;
	if (newmap) p[rerrmap_list.len] = newmap;
	if (!rerrmap_list.initialized) {
		rerrmap_list.list[0] = rerrmap;
		rerrmap_list.initialized = 1;
	}
	if (newmap) {
		rerrmap_list.len++;
	} else {
		rerrmap_list.len = 1;
	}
	return RERR_OK;
}

int
rerr_unregister (newmap)
	struct rerrmap	*newmap;
{
	int		i;

	if (!newmap) return RERR_PARAM;
	for (i=1; i<rerrmap_list.len; i++) {
		if (newmap == rerrmap_list.list[i]) break;
	}
	if (i >= rerrmap_list.len) return RERR_NOT_FOUND;
	for (; i<rerrmap_list.len-1; i++) {
		rerrmap_list.list[i] = rerrmap_list.list[i+1];
	}
	rerrmap_list.len--;
	return RERR_OK;
}

const char *
rerr_getstr (err)
	int		err;
{
	struct rerrmap	*p;
	int				i;

	if (!(rerrmap_list.initialized)) 
		rerr_register (NULL);
	for (i=0; i<rerrmap_list.len; i++) {
		for (p=rerrmap_list.list[i]; p->rerrstr; p++) {
			if (err == p->rerrno) return p->rerrstr;
		}
	}
	return "unknown error";
}


char*
rerr_getstr2s (err)
	int		err;
{
	static char	errstr[1024];
	const char	*errstr2, *pref;
	int			ret;

	*errstr = 0;
	ret = getspecialmap (err, &pref, &errstr2);
	if (!RERR_ISOK(ret)) {
		errstr2 = rerr_getstr (err);
		pref = NULL;
	}
	if (!errstr2) {
		snprintf (errstr, sizeof(errstr)-1, "error in rerr_getstr()???");
	} else if (err == RERR_SYSTEM) {
		snprintf (errstr, sizeof(errstr)-1, "%s(%d): %s(%d)", errstr2, 
						err, strerror (errno), errno);
	} else {
		snprintf (errstr, sizeof(errstr)-1, "%s%s%s(%d)", pref?pref:"",
						pref?" ":"", errstr2, err);
	}
	errstr[sizeof(errstr)-1]=0;
	return errstr;
}


char*
rerr_getstr2m (err)
	int		err;
{
	const char	*errstr2, *pref;
	int			ret;

	ret = getspecialmap (err, &pref, &errstr2);
	if (!RERR_ISOK(ret)) {
		errstr2 = rerr_getstr (err);
		pref = NULL;
	}
	if (!errstr2) {
		return strdup ("error in rerr_getstr()???");
	} else if (err == RERR_SYSTEM) {
		return asprtf ("%s(%d): %s(%d)", errstr2, err, strerror (errno), errno);
	} else {
		return asprtf ("%s%s%s(%d)", pref?pref:"", pref?" ":"", errstr2, err);
	}
	return NULL;
}



char*
rerr_getstr2 (errstr, len, err)
	char	*errstr;
	int	len, err;
{
	const char	*errstr2, *pref = NULL;
	int			ret;

	*errstr = 0;
	ret = getspecialmap (err, &pref, &errstr2);
	if (!RERR_ISOK(ret)) {
		errstr2 = rerr_getstr (err);
		pref = NULL;
	}
	if (!errstr2) {
		snprintf (errstr, len-1, "error in rerr_getstr()???");
	} else if (err == RERR_SYSTEM) {
		snprintf (errstr, len-1, "%s(%d): %s(%d)", errstr2, 
						err, strerror (errno), errno);
	} else {
		snprintf (errstr, len-1, "%s%s%s(%d)", pref?pref:"",
					pref?" ":"", errstr2, err);
	}
	errstr[len-1]=0;
	return errstr;
}



const char*
rerr_getname (err)
	int	err;
{
	struct rerrmap	*p;
	int				i;

	if (!(rerrmap_list.initialized)) 
		rerr_register (NULL);
	for (i=0; i<rerrmap_list.len; i++) {
		for (p=rerrmap_list.list[i]; p->rerrno>=0; p++) {
			if (err == p->rerrno) return p->rerrname;
		}
	}
	return "NULL";
}



int
rerr_mapname (errname)
	const char	*errname;
{
	struct rerrmap	*p;
	int				i;

	if (!(rerrmap_list.initialized)) 
		rerr_register (NULL);
	for (i=0; i<rerrmap_list.len; i++) {
		for (p=rerrmap_list.list[i]; p->rerrno>=0; p++) {
			if (errname == p->rerrname) return p->rerrno;
		}
	}
	return -1;
}


struct rerr_specialmap {
	int			from, to;
	const char* (*func) (int);
	const char	*prefix;
};

static struct rerr_specialmap	*specialmap = NULL;
static int							num_specialmap = 0;

int
rerr_regspecialmap (from, to, func, prefix)
	int			from, to;
	const char* (*func) (int);
	const char	*prefix;
{
	struct rerr_specialmap	map, *p;

	if (!func) return RERR_PARAM;
	map = (struct rerr_specialmap) { .func = func, .prefix = prefix };
	if (from > to) {
		map.from = to;
		map.to = from;
	} else {
		map.from = from;
		map.to = to;
	}
	p = realloc (specialmap, (num_specialmap+1) * sizeof (struct rerr_specialmap));
	if (!p) return RERR_NOMEM;
	specialmap = p;
	p[num_specialmap] = map;
	num_specialmap ++;
	return RERR_OK;
}


static
int
getspecialmap (idx, pref, str)
	int			idx;
	const char	**pref, **str;
{
	int		i;

	if (!pref || !str) return RERR_PARAM;
	for (i=0; i<num_specialmap; i++) {
		if (idx >= specialmap[i].from && idx <= specialmap[i].to) {
			*pref = specialmap[i].prefix;
			*str = specialmap[i].func (idx);
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
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
