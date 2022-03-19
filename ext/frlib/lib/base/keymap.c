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
#include <ctype.h>

#include "errors.h"
#include "keymap.h"
#include "slog.h"
#include "fileop.h"
#include "textop.h"



static int cfmap_parse (struct cfmap*);


int
map_insert (map, key, val, flags)
	struct keymap	*map;
	char				*key, *val;
	int				flags;
{
	struct keyval	*p;
	char				*s;

	if (!map || !key) return RERR_PARAM;
	if (flags&KEYMAP_F_CPY) return map_cpyinsert (map, key, val, flags);
	p = realloc (map->list, (map->listlen+1)*sizeof (struct keyval));
	if (!p) return RERR_NOMEM;
	map->list = p;
	p += map->listlen;
	if (flags & KEYMAP_F_TOLOWER) {
		for (s=key; s && *s; s++) *s = tolower (*s);
		for (s=val; s && *s; s++) *s = tolower (*s);
	} else if (flags & KEYMAP_F_TOUPPER) {
		for (s=key; s && *s; s++) *s = toupper (*s);
		for (s=val; s && *s; s++) *s = toupper (*s);
	}
	p->key = key;
	p->val = val;
	p->flags = flags;
	map->listlen++;
	return RERR_OK;
}


int
map_cpyinsert (map, key, val, flags)
	struct keymap	*map;
	char				*key, *val;
	int				flags;
{
	int	ret;

	if (!map || !key) return RERR_PARAM;
	flags &= ~KEYMAP_F_CPY;
	flags |= KEYMAP_F_DEL;
	flags |= KEYMAP_F_DELKEY;
	key = strdup (key);
	if (!key) return RERR_NOMEM;
	if (val) {
		val = strdup (val);
		if (!val) {
			free (key);
			return RERR_NOMEM;
		}
	}
	ret = map_insert (map, key, val, flags);
	if (!RERR_ISOK (ret)) {
		free (key);
		if (val) free (val);
		return ret;
	}
	return RERR_OK;
}


int
map_remove (map, key, flags)
	struct keymap	*map;
	char				*key;
	int				flags;
{
	int				i, j, found;
	struct keyval	*p;
	int (*mystrcmp)(const char*, const char*);

	if (!map || !key) return RERR_PARAM;
	mystrcmp = (flags & KEYMAP_F_IGNORE_CASE) ? &strcasecmp : &strcmp;
	for (i=found=0; i<map->listlen; i++) {
		if (!mystrcmp (map->list[i].key, key)) {
			found++;
			p=&(map->list[i]);
			if (p->flags & KEYMAP_F_DELKEY) {
				if (p->key) free (p->key);
			}
			if (p->flags & KEYMAP_F_DEL) {
				if (p->val) free (p->val);
			}
			*p = KEYVAL_NULL;
			for (j=i; j<map->listlen-1; j++) {
				map->list[j] = map->list[j+1];
			}
			map->listlen--;
		}
	}
	if (!found) return RERR_NOT_FOUND;
	return RERR_OK;
}



int
map_getnum (map)
	struct keymap	*map;
{
	if (!map) return RERR_PARAM;
	return map->listlen;
}

int
map_getithkeyval (keyval, map, num)
	struct keyval	**keyval;
	struct keymap	*map;
	int				num;
{
	if (!map) return RERR_PARAM;
	if (num < 0 || num >= map->listlen) return RERR_OUTOFRANGE;
	if (keyval) *keyval = map->list+num;
	return RERR_OK;
}


int
map_getkeyval (keyval, map, key, flags)
	struct keyval	**keyval;
	struct keymap	*map;
	char				*key;
	int				flags;
{
	int	i;
	int (*mystrcmp)(const char*, const char*);

	if (!map || !key) return RERR_PARAM;
	mystrcmp = (flags & KEYMAP_F_IGNORE_CASE) ? &strcasecmp : &strcmp;
	for (i=0; i<map->listlen; i++) {
		if (!mystrcmp (map->list[i].key, key)) {
			if (keyval) *keyval = map->list+i;
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
}



int
map_haskey (map, key, flags)
	struct keymap	*map;
	char				*key;
	int				flags;
{
	return map_getkeyval (NULL, map, key, flags);
}



int
map_getval (val, map, key, flags)
	char				**val;
	struct keymap	*map;
	char				*key;
	int				flags;
{
	int				ret;
	struct keyval	*keyval;

	ret = map_getkeyval (&keyval, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	if (val) *val = keyval->val;
	return RERR_OK;
}



int
map_getflags (oflags, map, key, flags)
	int				*oflags;
	struct keymap	*map;
	char				*key;
	int				flags;
{
	int				ret;
	struct keyval	*keyval;

	ret = map_getkeyval (&keyval, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	if (oflags) *oflags = keyval->flags;
	return RERR_OK;
}

int
map_isro (isro, map, key, flags)
	int				*isro;
	struct keymap	*map;
	char				*key;
	int				flags;
{
	int	ret, oflags;

	ret = map_getflags (&oflags, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	if (isro) *isro = (oflags & KEYMAP_F_RO) ? 1 : 0;
	return RERR_OK;
}




int
map_setval (map, key, val, flags)
	struct keymap	*map;
	char				*key, *val;
	int				flags;
{
	int				ret;
	struct keyval	*keyval;
	char				*s;

	ret = map_getkeyval (&keyval, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	if (keyval->flags & KEYMAP_F_RO) return RERR_FORBIDDEN;
	if ((keyval->flags & KEYMAP_F_DEL) && keyval->val) free (keyval->val);
	keyval->val = NULL;
	if (val && (flags & KEYMAP_F_CPY)) {
		val = strdup (val);
		if (!val) return RERR_NOMEM;
		keyval->flags |= KEYMAP_F_DEL;
	} else {
		keyval->flags &= ~KEYMAP_F_DEL;
	}
	if (flags & KEYMAP_F_TOLOWER) {
		for (s=val; s && *s; s++) *s = tolower (*s);
	} else if (flags & KEYMAP_F_TOUPPER) {
		for (s=val; s && *s; s++) *s = toupper (*s);
	}
	keyval->val = val;
	return RERR_OK;
}

int
map_setorinsert (map, key, val, flags)
	struct keymap	*map;
	char				*key, *val;
	int				flags;
{
	int	ret;

	ret = map_haskey (map, key, flags);
	if (RERR_ISOK (ret)) {
		ret = map_setval (map, key, val, flags);
	} else if (ret == RERR_NOT_FOUND) {
		ret = map_insert (map, key, val, flags);
	} else {
		return ret;
	}
	if (!RERR_ISOK (ret)) return ret;
	return RERR_OK;
}

int
map_setflags (map, key, iflags, flags)
	struct keymap	*map;
	char				*key;
	int				iflags, flags;
{
	int				ret;
	struct keyval	*keyval;

	ret = map_getkeyval (&keyval, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	keyval->flags |= iflags;
	return RERR_OK;
}


int
map_unsetflags (map, key, iflags, flags)
	struct keymap	*map;
	char				*key;
	int				iflags, flags;
{
	int				ret;
	struct keyval	*keyval;

	ret = map_getkeyval (&keyval, map, key, flags);
	if (!RERR_ISOK (ret)) return ret;
	keyval->flags &= ~iflags;
	return RERR_OK;
}


int
map_removeall (map)
	struct keymap	*map;
{
	int	i;

	if (!map) return RERR_PARAM;
	for (i=0; i<map->listlen; i++) {
		if (map->list[i].flags & KEYMAP_F_DELKEY) {
			if (map->list[i].key) free (map->list[i].key);
		}
		if (map->list[i].flags & KEYMAP_F_DEL) {
			if (map->list[i].val) free (map->list[i].val);
		}
		map->list[i] = KEYVAL_NULL;
	}
	if (map->list) free (map->list);
	*map = KEYMAP_NULL;
	return RERR_OK;
}


int
map_copy (dest, src, flags)
	struct keymap	*dest, *src;
	int				flags;
{
	int	ret, i, thisflags;

	if (!dest || !src) return RERR_PARAM;
	for (i=0; i<src->listlen; i++) {
		thisflags = src->list[i].flags & ~(KEYMAP_F_DEL | KEYMAP_F_DELKEY | 
														KEYMAP_F_CPY);
		thisflags |= flags;
		ret = map_insert (dest, src->list[i].key, src->list[i].val, thisflags);
		if (!RERR_ISOK (ret)) return ret;
	}
	return RERR_OK;
}


int
map_allsetflags (map, flags)
	struct keymap	*map;
	int				flags;
{
	int	i;

	if (!map) return RERR_PARAM;
	for (i=0; i<map->listlen; i++) {
		map->list[i].flags |= flags;
	}
	return RERR_OK;
}


int
map_allunsetflags (map, flags)
	struct keymap	*map;
	int				flags;
{
	int	i;

	if (!map) return RERR_PARAM;
	for (i=0; i<map->listlen; i++) {
		map->list[i].flags &= ~flags;
	}
	return RERR_OK;
}





/*************************************
 *  config map
 *************************************/


int
cfmap_setflags (map, flags)
	struct cfmap	*map;
	int				flags;
{
	if (!map) return RERR_PARAM;
	map->flags |= flags;
	return RERR_OK;
}


int
cfmap_unsetflags (map, flags)
	struct cfmap	*map;
	int				flags;
{
	if (!map) return RERR_PARAM;
	map->flags &= ~flags;
	return RERR_OK;
}


int
cfmap_config (map, key, val, flags)
	struct cfmap	*map;
	char				*key, *val;
	int				flags;
{
	int	ret;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	ret = map_insert (&(map->config), key, val, flags);
	if (!RERR_ISOK (ret)) {
		if (flags & CFMAP_F_LOG_ERROR) {
			FRLOGF (LOG_ERR, "error inserting (key,val)=(%s,%s): %s",
									key?key:"<NULL>", val?val:"<NULL>", 
									rerr_getstr3(ret));
		}
		return ret;
	}
	return RERR_OK;
}


int
cfmap_set (map, key, val, flags)
	struct cfmap	*map;
	char				*key, *val;
	int				flags;
{
	int	ret;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	ret = map_setval (&(map->set), key, val, flags);
	if ((ret == RERR_NOT_FOUND) && (flags & CFMAP_F_ALLOW_NONCONFIG)) {
		ret = map_insert (&(map->set), key, val, flags);
	}
	if (!RERR_ISOK (ret)) {
		if (flags & CFMAP_F_LOG_ERROR) {
			if (ret == RERR_NOT_FOUND) {
				FRLOGF (LOG_WARN, "key (tag) >>%s<< not configured", key);
			} else {
				FRLOGF (LOG_ERR, "error setting (key,val)=(%s,%s): %s",
										key?key:"<NULL>", val?val:"<NULL>", 
										rerr_getstr3(ret));
			}
		}
		return ret;
	}
	return RERR_OK;
}


int
cfmap_check (map, flags)
	struct cfmap	*map;
	int				flags;
{
	int	i;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	if (!(flags & CFMAP_F_LOG_ERROR)) return RERR_OK;
	for (i=0; i<map->set.listlen; i++) {
		if (!map->set.list[i].val && (map->set.list[i].flags & CFMAP_F_NOT_NULL)) {
			FRLOGF (LOG_WARN, "key (tag) >>%s<< has null value, "
									"which is not allowed", map->set.list[i].key);
		}
	}
	return RERR_OK;
}


int
cfmap_removenull (map, flags)
	struct cfmap	*map;
	int				flags;
{
	int	i, ret;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	for (i=0; i<map->set.listlen; i++) {
		if (!map->set.list[i].val) {
			ret = map_remove (&(map->set), map->set.list[i].key, flags);
			if (!RERR_ISOK (ret)) return ret;
			i--;
		}
	}
	return RERR_OK;
}


int
cfmap_removeempty (map, flags)
	struct cfmap	*map;
	int				flags;
{
	int	i, ret;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	for (i=0; i<map->set.listlen; i++) {
		if (!map->set.list[i].val || !*(map->set.list[i].val)) {
			ret = map_remove (&(map->set), map->set.list[i].key, flags);
			if (!RERR_ISOK (ret)) return ret;
			i--;
		}
	}
	return RERR_OK;
}


int
cfmap_reset (map, flags)
	struct cfmap	*map;
	int				flags;
{
	int	ret;

	if (!map) return RERR_PARAM;
	flags |= map->flags;
	ret = map_removeall (&(map->set));
	if (!RERR_ISOK (ret)) {
		if (flags & CFMAP_F_LOG_ERROR) {
			FRLOGF (LOG_ERR, "error emptying set-map: %s", rerr_getstr3 (ret));
		}
		return ret;
	}
	ret = map_copy (&(map->set), &(map->config), flags);
	if (!RERR_ISOK (ret)) {
		if (flags & CFMAP_F_LOG_ERROR) {
			FRLOGF (LOG_ERR, "error copying config-map: %s", rerr_getstr3 (ret));
		}
		return ret;
	}
	return RERR_OK;
}


int
cfmap_hfree (map)
	struct cfmap	*map;
{
	if (!map) return RERR_PARAM;
	map_removeall (&(map->set));
	map_removeall (&(map->config));
	if (map->buf) free (map->buf);
	return cfmap_init (map);
}


int
cfmap_init (map)
	struct cfmap	*map;
{
	if (!map) return RERR_PARAM;
	*map = CFMAP_NULL;
	return RERR_OK;
}


int
cfmap_readconfig (map, fname, flags)
	struct cfmap	*map;
	char				*fname;
	int				flags;
{
	int	ret;

	if (!map || !fname || !*fname) return RERR_PARAM;
	cfmap_hfree (map);
	map->flags = flags;
	map->buf = fop_read_fn (fname);
	if (!map->buf) {
		if (flags & CFMAP_F_LOG_ERROR) {
			FRLOGF (LOG_ERR, "error reading config map file >>%s<<: %s",
								fname, rerr_getstr3(RERR_SYSTEM));
		}
		return RERR_SYSTEM;
	}
	ret = cfmap_parse (map);
	if (!RERR_ISOK (ret)) {
		if (flags & CFMAP_F_LOG_ERROR) {
			FRLOGF (LOG_ERR, "error parsing config map file >>%s<<: %s",
								fname, rerr_getstr3(ret));
		}
		cfmap_hfree (map);
		return ret;
	}
	map->flags |= CFMAP_F_IS_CONFIGURED;
	return RERR_OK;
}


static
int
cfmap_parse (map)
	struct cfmap	*map;
{
	int	flags, thisflags, ret;
	char	*buf, *s, *s2, *line, *key, *val;

	if (!map || !map->buf) return RERR_PARAM;
	flags = map->flags;
	buf = map->buf;

	while ((line = top_getline (&buf, 0))) {
		key = top_getfield (&line, "=", TOP_F_NOSKIPBLANK);
		val = top_getfield (&line, NULL, TOP_F_NOSKIPBLANK|TOP_F_STRIPMIDDLE);
		if (!key || !*key) continue;
		for (s=s2=key; *s; s++) {
			if (isspace (*s)) continue;
			*s2 = *s; s2++;
		}
		*s2 = 0;
		if (flags & KEYMAP_F_TOLOWER) {
			for (s=key; *s; s++) *s = tolower (*s);
			for (s=val; s && *s; s++) *s = tolower (*s);
		} else if (flags & KEYMAP_F_TOUPPER) {
			for (s=key; *s; s++) *s = tolower (*s);
			for (s=val; s && *s; s++) *s = tolower (*s);
		}
		thisflags = flags;
		if (!val || !*val || !strcasecmp (val, "null")) {
			val = NULL;
		} else if (!strcasecmp (val, "notnull") || !strcasecmp (val, "not_null")
											|| !strcasecmp (val, "not null")) {
			val = NULL;
			thisflags |= CFMAP_F_NOT_NULL;
		} else if (*s == ':' || !strncasecmp (val, "def", 3)) {
			for (s=val; *s && *s != ':'; s++);
			val = top_skipwhite (s+1);
		} else {
			val = top_skipwhite (val);
		}
		ret = cfmap_config (map, key, val, thisflags);
		if (!RERR_ISOK (ret)) return ret;
	}
	return RERR_OK;
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
