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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif


#include "errors.h"
#include "textop.h"
#include "config.h"
#include "slog.h"
#include "xmlparser.h"
#include "xmlcpy.h"


static int attr_cpy (struct xml_attr*, struct xml_attr*, char*, char*, int);
static int attrlist_cpy (	struct xml_attr_list*, struct xml_attr_list*, 
									char*, char*, int);
static int sub_cpy (struct xml_sub*, struct xml_sub*, char*, char*, int);
static int sublist_cpy (struct xml_sub_list*, struct xml_sub_list*, char*, 
								char*, int);
static int tag_cpy (struct xml_tag*, struct xml_tag*, char*, char*, int);
static int sublist_merge (	struct xml_sub_list*, struct xml_sub_list*, 
									char*, char*, int);
static int sublist_insert (struct xml_sub_list*, struct xml_sub*, char*, 
									char*, int);
static int attrlist_merge (struct xml_attr_list*, struct xml_attr_list*, 
									char*, char*, int);
static int attrlist_insert (	struct xml_attr_list*, struct xml_attr*, char*, 
										char*, int);
static int tag_merge (struct xml_tag*, struct xml_tag*, char*, char*, int);
static int del_all_text (struct xml_sub_list*);




int
xml_cpy (dest, src, flags)
	struct xml	*dest, *src;
	int			flags;
{
	int	ret;

	if (!dest || !src) return RERR_PARAM;
	bzero (dest, sizeof (struct xml));
	if (!src->_buffer) return RERR_OK;
	dest->flags = src->flags;
	if (flags & XMLCPY_F_SAMEBUF || (!(flags & XMLCPY_F_NEWBUF) &&
					(src->flags & XMLPARSER_FLAG_NOTCOPY) && 
					(src->flags & XMLPARSER_FLAG_NOTFREE))) {
		dest->_buffer = src->_buffer;
	} else {
		dest->flags &= ~XMLPARSER_FLAG_NOTFREE;
		dest->flags &= ~XMLPARSER_FLAG_NOTCOPY;
		dest->_buffer = malloc (src->_buflen);
		if (!dest->_buffer) return RERR_NOMEM;
		memcpy (dest->_buffer, src->_buffer, src->_buflen);
	}
	dest->_buflen = src->_buflen;
	ret = tag_cpy (&(dest->top_tag), &(src->top_tag), 
						dest->_buffer, src->_buffer, flags);
	if (!RERR_ISOK(ret)) {
		xml_hfree (dest);
		return ret;
	}
	ret = xml_update_father (dest);
	if (!RERR_ISOK(ret)) {
		xml_hfree (dest);
		return ret;
	}
	return RERR_OK;
}

int
xml_merge (dest, src, flags)
	struct xml	*dest, *src;
	int			flags;
{
	int	ret;
	char	*s;

	if (!dest || !src) return RERR_PARAM;
	if (!dest->_buffer) return xml_cpy (dest, src, flags);
	s = realloc (dest->_buffer, dest->_buflen + src->_buflen);
	if (!s) return RERR_NOMEM;
	dest->_buffer = s;
	s += dest->_buflen;
	memcpy (s, src->_buffer, src->_buflen);
	dest->_buflen += src->_buflen;
	ret = tag_merge (	&(dest->top_tag), &(src->top_tag),
							s, src->_buffer, flags);
	if (!RERR_ISOK(ret)) return ret;
	ret = xml_update_father (dest);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}



static
int
sublist_cpy (dest, src, destbuf, srcbuf, flags)
	struct xml_sub_list	*dest, *src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int	ret, i;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	if (src->len == 0) return RERR_OK;
	dest->list = malloc (src->len*sizeof (struct xml_sub));
	if (!dest->list) return RERR_NOMEM;
	dest->len = src->len;
	bzero (dest->list, sizeof (struct xml_sub) * src->len);
	for (i=0; i<src->len; i++) {
		ret = sub_cpy (&(dest->list[i]), &(src->list[i]), destbuf, 
							srcbuf, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
sub_cpy (dest, src, destbuf, srcbuf, flags)
	struct xml_sub	*dest, *src;
	char				*destbuf, *srcbuf;
	int				flags;
{
	int	ret;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	dest->type = src->type;
	if (!SUB_TYPE_ISTAG(src->type)) {
		dest->body.text = src->body.text - srcbuf + destbuf;
	} else {
		ret = tag_cpy (&(dest->body.tag), &(src->body.tag), destbuf, 
							srcbuf, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
tag_cpy (dest, src, destbuf, srcbuf, flags)
	struct xml_tag	*dest, *src;
	char				*destbuf, *srcbuf;
	int				flags;
{
	int	ret;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	dest->name = src->name - srcbuf + destbuf;
	ret = attrlist_cpy (	&(dest->attr_list), &(src->attr_list), 
								destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) return ret;
	ret = sublist_cpy (	&(dest->sub_list), &(src->sub_list), 
								destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
attrlist_cpy (dest, src, destbuf, srcbuf, flags)
	struct xml_attr_list	*dest, *src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int	ret, i;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	if (src->len == 0) return RERR_OK;
	dest->list = malloc (src->len * sizeof (struct xml_attr));
	if (!dest->list) return RERR_NOMEM;
	bzero (dest->list, src->len * sizeof (struct xml_attr));
	dest->len = src->len;
	for (i=0; i<src->len; i++) {
		ret = attr_cpy (	&(dest->list[i]), &(src->list[i]), 
								destbuf, srcbuf, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
attr_cpy (dest, src, destbuf, srcbuf, flags)
	struct xml_attr	*dest, *src;
	char					*destbuf, *srcbuf;
	int					flags;
{
	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	dest->var = src->var - srcbuf + destbuf;
	dest->val = src->val - srcbuf + destbuf;
	return RERR_OK;
}


static
int
sublist_merge (dest, src, destbuf, srcbuf, flags)
	struct xml_sub_list	*dest, *src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int	ret, i;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	if (src->len == 0) return RERR_OK;
	for (i=0; i<src->len; i++) {
		ret = sublist_insert (	dest, &(src->list[i]), 
										destbuf, srcbuf, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}



static
int
sublist_insert (dest, src, destbuf, srcbuf, flags)
	struct xml_sub_list	*dest;
	struct xml_sub			*src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int				ret, i;
	struct xml_sub	*dsub;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	if (SUB_TYPE_ISTAG (src->type)) {
		for (i=0; i<dest->len; i++) {
			dsub = &(dest->list[i]);
			if (!SUB_TYPE_ISTAG (dsub->type)) continue;
			if (!strcasecmp (dsub->body.tag.name, src->body.tag.name)) {
				return tag_merge ( &(dsub->body.tag), &(src->body.tag),
									destbuf, srcbuf, flags);
			}
		}
	} else if (flags & XMLCPY_F_TEXT_OVERWRITE) {
		del_all_text (dest);
	} else if (!(flags & XMLCPY_F_TEXT_APPEND)) {
		for (i=0; i<dest->len; i++) {
			dsub = &(dest->list[i]);
			if (dsub->type == SUB_TYPE_TEXT) return RERR_OK;
		}
	}
		
	dsub = realloc (dest->list, (dest->len+1)*sizeof (struct xml_sub));
	if (!dsub) return RERR_NOMEM;
	dest->list = dsub;
	dsub += dest->len;
	dest->len++;
	bzero (dsub, sizeof (struct xml_sub));
	ret = sub_cpy (dsub, src, destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) {
		xml_hfree_sub (dsub);
		dest->len--;
		return ret;
	}
	return RERR_OK;
}

static
int
del_all_text (sublist)
	struct xml_sub_list	*sublist;
{
	int	i,j;

	if (!sublist) return RERR_PARAM;
	for (i=0; i<sublist->len; i++) {
		if (sublist->list[i].type == SUB_TYPE_TEXT) {
			for (j=i; j<sublist->len-1; j++) {
				sublist->list[j] = sublist->list[j+1];
			}
			sublist->len--;
		}
	}
	return RERR_OK;
}



static
int
tag_merge (dest, src, destbuf, srcbuf, flags)
	struct xml_tag	*dest, *src;
	char				*destbuf, *srcbuf;
	int				flags;
{
	int	ret;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	ret = attrlist_merge (	&(dest->attr_list), &(src->attr_list),
									destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) return ret;
	ret = sublist_merge (&(dest->sub_list), &(src->sub_list),
								destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
attrlist_merge (dest, src, destbuf, srcbuf, flags)
	struct xml_attr_list	*dest, *src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int	ret, i;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	if (src->len == 0) return RERR_OK;
	if (dest->len == 0) {
		return attrlist_cpy (dest, src, destbuf, srcbuf, flags);
	}
	for (i=0; i<src->len; i++) {
		ret = attrlist_insert (	dest, &(src->list[i]), 
										destbuf, srcbuf, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}



static
int
attrlist_insert (dest, src, destbuf, srcbuf, flags)
	struct xml_attr_list	*dest;
	struct xml_attr		*src;
	char						*destbuf, *srcbuf;
	int						flags;
{
	int					ret, i;
	struct xml_attr	*dattr;

	if (!dest || !src) return RERR_PARAM;
	if (!destbuf || !srcbuf) return RERR_PARAM;
	for (i=0; i<dest->len; i++) {
		dattr = &(dest->list[i]);
		if (strcasecmp (dattr->var, src->var) != 0) continue;
		if (flags & XMLCPY_F_ATTR_OVERWRITE) {
			dattr->val = src->val - srcbuf + destbuf;
		}
		return RERR_OK;
	}
	/* append new attribute */
	dattr = realloc (dest->list, (dest->len+1) * sizeof (struct xml_attr));
	if (!dattr) return RERR_NOMEM;
	dattr += dest->len;
	dest->len++;
	bzero (dattr, sizeof (struct xml_attr));
	ret = attr_cpy (dattr, src, destbuf, srcbuf, flags);
	if (!RERR_ISOK(ret)) return ret;
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
