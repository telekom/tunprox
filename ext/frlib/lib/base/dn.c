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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif



#include "dn.h"
#include "errors.h"
#include "textop.h"




int
dn_split (dn, dnstr)
	struct dn	*dn;
	const char	*dnstr;
{
	char	*str;
	int	ret;

	if (!dn || !dnstr) return RERR_PARAM;
	str = strdup (dnstr);
	ret = dn_split_inplace (dn, str);
	if (!RERR_ISOK(ret)) {
		free (str);
		return ret;
	}
	dn->hascpy = 1;
	return RERR_OK;
}


int
dn_split_inplace (dn, str)
	struct dn	*dn;
	char			*str;
{
	char				*var, *val, *s;
	struct dn_part	entry;
	int				ret;

	if (!dn || !str) return RERR_PARAM;
	*dn = DN_NULL;
	ret = bufref_addref (&dn->bref, str);
	while ((var=top_getfield (&str, ",", TOP_F_DNQUOTE))) {
		val = index (var, '=');
		if (!val) continue;
		for (s=val-1; s>=var && iswhite (*s); s--) {}; s++;
		*s=0;
		if (!*var) continue;
		val = top_skipwhite (val+1);
		val = top_dnunquote (val);
		entry = (struct dn_part){ .var = var, .val = val };
		ret = TLST_INSERT (dn->list, entry, tlst_cmpistr);
		if (!RERR_ISOK(ret)) {
			TLST_FREE (dn->list);
			*dn = DN_NULL;
			return ret;
		}
		bufref_ref (&dn->bref, var);
		bufref_ref (&dn->bref, val);
	}
	return RERR_OK;
}


int
dn_hfree (dn)
	struct dn	*dn;
{
	if (!dn) return 0;
	if (dn->hascpy) {
		bufref_free (&dn->bref);
	} else {
		bufref_clear (&dn->bref);
	}
	TLST_FREE (dn->list);
	*dn=DN_NULL;
	return 1;
}

const char *
dn_getpart (dn, var)
	const struct dn	*dn;
	const char			*var;
{
	int				ret;
	struct dn_part	*p;

	if (!dn || !var) return NULL;
	ret = tlst_search ((struct tlst*)&dn->list, &var, tlst_cmpistr);
	if (!RERR_ISOK(ret)) return NULL;
	ret = tlst_getptr (&p, (struct tlst*)&dn->list, ret);
	if (!RERR_ISOK(ret)) return NULL;
	return p->val;
}

int
dn_cmp_struct (dn1, dn2)
	const struct dn	*dn1, *dn2;
{
	int				ret;
	size_t			num, i;
	struct dn_part	*e1, *e2;

	if (!dn1 && !dn2) return 0;
	if (!dn1) return -1;
	if (!dn2) return 1;
	num = TLST_GETNUM (dn1->list);
	if (num < TLST_GETNUM(dn2->list)) return -1;
	if (num > TLST_GETNUM(dn2->list)) return 1;
	for (i=0; i<num; i++) {
		ret = tlst_getptr (&e1, (struct tlst*)&dn1->list, i);
		if (!RERR_ISOK(ret)) return -2;
		ret = tlst_getptr (&e2, (struct tlst*)&dn2->list, i);
		if (!RERR_ISOK(ret)) return 2;
		
		if ((ret=strcasecmp (e1->var, e2->var))!=0)
			return ret;
		if ((ret=strcmp (e1->val, e2->val))!=0) 
			return ret;
	}
	return 0;
}

int
dn_cmp (dn1, dn2)
	const char	*dn1, *dn2;
{
	struct dn	sdn1, sdn2;
	int			ret;

	if (!dn1 && !dn2) return 0;
	if (!dn1) return -1;
	if (!dn2) return 1;
	if (!strcmp (dn1, dn2)) return 0;
	if (!RERR_ISOK (dn_split (&sdn1, dn1))) return -2;
	if (!RERR_ISOK (dn_split (&sdn2, dn2))) {
		dn_hfree (&sdn1);
		return 2;
	}
	ret = dn_cmp_struct (&sdn1, &sdn2);
	dn_hfree (&sdn1);
	dn_hfree (&sdn2);
	return ret;
}

int
dn_cpy (dst, src)
	struct dn			*dst;
	const struct dn	*src;
{
	size_t			i;
	int				ret;
	struct dn_part	*p;
	struct bufref	bref;

	if (!dst || !src) return RERR_PARAM;
	*dst = DN_NULL;
	ret = TLST_CPY (dst->list, src->list);
	if (!RERR_ISOK(ret)) return ret;
	ret = bufref_cpy (&bref, &src->bref);
	if (!RERR_ISOK(ret)) {
		TLST_FREE (dst->list);
		*dst = DN_NULL;
		return ret;
	}
	TLST_FOREACHPTR2 (p, dst->list, i) {
		p->var = bufref_refcpy (&bref, p->var);
		p->val = bufref_refcpy (&bref, p->val);
	}
	ret = bufref_cpyfinish (&dst->bref, &bref);
	if (!RERR_ISOK(ret)) {
		TLST_FREE (dst->list);
		*dst = DN_NULL;
		return ret;
	}
	dst->hascpy = 1;
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
