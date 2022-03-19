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
#include <assert.h>

#include "errors.h"
#include "tlst.h"

static int tlst_freesub (char**, unsigned);
static int tlst_logd2 (unsigned, unsigned);
int tlst_logd (unsigned num);
int tlst_depth (unsigned);



int
tlst_find (idx, data, list, cmp, key)
	unsigned		*idx;
	void			*data;
	struct tlst	*list;
	int			(*cmp) (void*, void*);
	void			*key;
{
	char		*ptr;
	unsigned	i;
	int		ret;

	if (!list || !cmp) return RERR_PARAM;
	TLST_FOREACHPTR2 (ptr, *list, i) {
		if (!ptr) continue;
		if (!cmp (ptr, key)) break;
	}
	if (i >= list->num) return RERR_NOT_FOUND;
	if (idx) *idx = i;
	if (data) {
		ret = tlst_get(data, list, i);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

int
tlst_cmpint (ptr, val)
	void	*ptr, *val;
{
	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	if (*(int*)ptr == *(int*)val) return 0;
	if (*(int*)ptr < *(int*)val) return -1;
	return 1;
}

int
tlst_cmpint64 (ptr, val)
	void	*ptr, *val;
{
	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	if (*(int64_t*)ptr == *(int64_t*)val) return 0;
	if (*(int64_t*)ptr < *(int64_t*)val) return -1;
	return 1;
}

int
tlst_cmpdblint (ptr, val)
	void	*ptr, *val;
{
	struct { int a, b; } *p, *v;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	p = ptr;
	v = val;
	if (p->a < v->a) return -1;
	if (p->a > v->a) return 1;
	if (p->b < v->b) return -1;
	if (p->b > v->b) return 1;
	return 0;
}

int
tlst_cmpdblint64 (ptr, val)
	void	*ptr, *val;
{
	struct { int64_t a, b; } *p, *v;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	p = ptr;
	v = val;
	if (p->a < v->a) return -1;
	if (p->a > v->a) return 1;
	if (p->b < v->b) return -1;
	if (p->b > v->b) return 1;
	return 0;
}

int
tlst_cmpstr (ptr, val)
	void	*ptr, *val;
{
	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	return strcmp (*(char**)ptr, *(char**)val);
}
int
tlst_cmpistr (ptr, val)
	void	*ptr, *val;
{
	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	return strcasecmp (*(char**)ptr, *(char**)val);
}
int
tlst_cmpdblstr (ptr, val)
	void	*ptr, *val;
{
	struct { char *a, *b; } *p, *v;
	int							ret;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	p = ptr;
	v = val;
	ret = tlst_cmpstr (&(p->a), &(v->a));
	if (ret) return ret;
	return tlst_cmpstr (&(p->b), &(v->b));
	if (ret) return ret;
	return 1;
}
int
tlst_cmpdblistr (ptr, val)
	void	*ptr, *val;
{
	struct { char *a, *b; } *p, *v;
	int							ret;

	if (!ptr && !val) return 0;
	if (!ptr) return 1;
	if (!val) return -1;
	p = ptr;
	v = val;
	ret = tlst_cmpistr (&(p->a), &(v->a));
	if (ret) return ret;
	return tlst_cmpistr (&(p->b), &(v->b));
	if (ret) return ret;
	return 1;
}


int
tlst_new (list, size)
	struct tlst	*list;
	size_t		size;
{
	if (!list) return RERR_PARAM;
	bzero (list, sizeof (struct tlst));
	list->size=size;
	return RERR_OK;
}

int
tlst_putlst (list, src, num, flags)
	struct tlst	*list;
	void			*src;
	unsigned		num;
	int			flags;
{
	size_t	size;

	/* flags are currently ignored */
	if (!list || !src || num > 16) return RERR_PARAM;
	if (list->data) {
		size = list->size;
		tlst_free (list);
		list->size = size;
	}
	list->data = src;
	list->num = num;
	list->max = num-1;
	list->depth = 0;
	return RERR_OK;
}

int
tlst_remove (list, idx, flags)
	struct tlst	*list;
	unsigned		idx;
	int			flags;
{
	char		*data;
	unsigned	i;
	int		ret;

	if (!list) return RERR_PARAM;
	if (idx >= list->num) return RERR_OK;
	ret = tlst_getptr (&data, list, idx);
	if (RERR_ISOK(ret)) {
		bzero (data, list->size);
	}
	if (list->num > 1) {
		switch (flags & TLST_MASK_RM) {
		case TLST_F_SHIFT:
			for (i=idx; i<list->num-1; i++) {
				ret = tlst_getptr (&data, list, i+1);
				if (!RERR_ISOK(ret)) return ret;
				ret = tlst_set (list, i, data);
				if (!RERR_ISOK(ret)) return ret;
			}
			list->num--;
			list->max--;
			idx = list->num;
			break;
		case TLST_F_CPYLAST:
			if (idx < list->num-1) {
				ret = tlst_getptr (&data, list, list->num-1);
				if (!RERR_ISOK(ret)) return ret;
				ret = tlst_set (list, idx, data);
				if (!RERR_ISOK(ret)) return ret;
			}
			list->num--;
			idx = list->num;
			list->max--;
			break;
		default:
			if (idx == list->max) {
				list->max--;
			}
			list->num--;
			break;
		}
	} else {
		list->num = 0;
		list->max = 0;
	}
	return RERR_OK;
}

int
tlst_add (list, data)
	struct tlst	*list;
	void			*data;
{
	int	ret;

	if (!list) return RERR_PARAM;
	ret = tlst_set (list, list->num, data);
	if (!RERR_ISOK(ret)) return ret;
	ret = list->num;
	list->num++;
	return ret;
}

int
tlst_addinsert (list, data)
	struct tlst	*list;
	void			*data;
{
	int		ret;
	char		*p;
	unsigned	n, i, idx;

	if (!list) return RERR_PARAM;
	if (list->num > 0 && list->num <= list->max) {
		if (list->size < sizeof (void*)) return RERR_FORBIDDEN;
		for (idx=0; idx < list->max; /* incremented below */) {
			p = list->data;
			for (i=list->depth; p && i>0; i--) {
				n = (idx & ((unsigned)0x0f << (4*i))) >> (4*i);
				p = ((char**)p)[n];
			}
			if (!p) break;
			for (i=0; i<16 && idx < list->max; i++, idx++) {
				if (!*((char**)p)) goto out;
				p += list->size;
			}
		}
		if (idx <= list->max) idx = list->max+1;
	} else {
		idx = list->num;
	}
out:
	ret = tlst_set (list, idx, data);
	if (!RERR_ISOK(ret)) return ret;
	list->num++;
	return idx;
}

int
tlst_insert (list, data, cmpfunc)
	struct tlst	*list;
	void			*data;
	int 			(*cmpfunc) (void*, void*);
{
	int	min, max, num, ret, i;
	void	*ptr;

	if (!list || !data || !cmpfunc) return RERR_PARAM;
	if (!list->num) {
		ret = tlst_set (list, 0, data);
		if (!RERR_ISOK(ret)) return ret;
		list->num++;
		return 0;
	}
	min = 0; max = list->num - 1;
	while (max >= min) {
		num = (max + min) / 2;
		ret = tlst_getptr ((void*)&ptr, list, num);
		if (!RERR_ISOK(ret)) return ret;
		ret = cmpfunc (data, ptr);
		if (ret == 0) break;
		if (ret < 0) max = num-1;
		if (ret > 0) min = num+1;
	}
	if (ret >= 0) num++;
	for (i=list->num-1; i>=num; i--) {
		ret = tlst_getptr ((void*)&ptr, list, i);
		if (!RERR_ISOK(ret)) return ret;
		ret = tlst_set (list, i+1, ptr);
		if (!RERR_ISOK(ret)) return ret;
	}
	list->num++;
	ret = tlst_set (list, num, data);
	if (!RERR_ISOK(ret)) return ret;
	return num;
}

int
tlst_search (list, key, cmpfunc)
	struct tlst	*list;
	void			*key;
	int 			(*cmpfunc) (void*, void*);
{
	int	min, max, num, ret;
	void	*ptr;

	if (!list || !key || !cmpfunc) return RERR_PARAM;
	min = 0; max = list->num - 1;
	while (max >= min) {
		num = (max + min) / 2;
		ret = tlst_getptr ((void*)&ptr, list, num);
		if (!RERR_ISOK(ret)) return ret;
		ret = cmpfunc (key, ptr);
		if (ret == 0) return num;
		if (ret < 0) max = num-1;
		if (ret > 0) min = num+1;
	}
	return RERR_NOT_FOUND;
}

int
tlst_reset (list)
	struct tlst	*list;
{
#if 0		/* buggy */
	if (!list) return RERR_PARAM;
	list->num = 0;
	list->max = 0;
#else
	size_t	size;
	int		ret;

	if (!list) return RERR_PARAM;
	size = list->size;
	ret = tlst_free (list);
	list->size=size;
	if (!RERR_ISOK(ret)) return ret;
#endif
	return RERR_OK;
}

int
tlst_getnum (list)
	struct tlst	*list;
{
	if (!list) return RERR_PARAM;
	return list->num;
}

int
tlst_getmax (list)
	struct tlst	*list;
{
	if (!list) return RERR_PARAM;
	return (int) list->max;
}

int
tlst_set (list, num, data)
	struct tlst	*list;
	unsigned		num;
	void			*data;
{
	char		*p, *q;
	unsigned	i, n, depth, size;

	if (!list || !data) return RERR_PARAM;
	if (num > list->max) {
		depth = tlst_depth (num);
		for (i=list->depth; i<depth; i++) {
			p = malloc (16*sizeof (char*));
			if (!p) return RERR_NOMEM;
			bzero (p, 16*sizeof(char*));
			((char**)p)[0] = list->data;
			list->data = p;
		}
		if (depth > list->depth) {
			list->depth = depth;
		}
	}
	if (!list->data) {
		list->data = malloc (16*list->size);
		if (!list->data) return RERR_NOMEM;
		bzero (list->data, 16*list->size);
		p = list->data;
	} else {
		p = list->data;
		for (i=list->depth; i>0; i--) {
			n = (num & ((unsigned)0x0f << (4*i))) >> (4*i);
			q = ((char**)p)[n];
			if (!q) {
				size = (i==1) ? list->size : sizeof (char*);
				q = malloc (16*size);
				if (!q) return RERR_NOMEM;
				bzero (q, 16*size);
				((char**)p)[n] = q;
			}
			p = q;
		}
	}
	n = num & 0x0f;
	p += n * list->size;
	memcpy (p, data, list->size);
	if (num > list->max) list->max = num;
	return RERR_OK;
}


int
tlst_get (out, list, num)
	void			*out;
	struct tlst	*list;
	unsigned		num;
{
	char	*ptr;
	int	ret;

	if (!list || !out) return RERR_PARAM;
	bzero (out, list->size);
	ret = tlst_getptr (&ptr, list, num);
	if (!RERR_ISOK(ret)) return ret;
	memcpy (out, ptr, list->size);
	return RERR_OK;
}


int
tlst_getptr (out, list, num)
	void			*out;
	struct tlst	*list;
	unsigned		num;
{
	char		*p;
	unsigned	n, i;

	if (!list) return RERR_PARAM;
	if (out) *(char**)out = NULL;
	if (num > list->max) return RERR_NOT_FOUND;
	p = list->data;
	if (!p) return RERR_NOT_FOUND;
	for (i=list->depth; i>0; i--) {
		n = (num & ((unsigned)0x0f << (4*i))) >> (4*i);
		p = ((char**)p)[n];
		if (!p) return RERR_NOT_FOUND;
	}
	n = num & 0x0f;
	p += n * list->size;
	if (out) *(char**)out = p;
	return RERR_OK;
}


int
tlst_free (list)
	struct tlst	*list;
{
	int	ret = RERR_OK;

	if (!list) return RERR_PARAM;
	if (list->data) {
		ret = tlst_freesub ((char**)list->data, list->depth);
		free (list->data);
	}
	bzero (list, sizeof (struct tlst));
	return ret;
}

static
int
tlst_freesub (data, depth)
	char		**data;
	unsigned	depth;
{
	int	i, ret, ret2=RERR_OK;

	if (!data) return RERR_PARAM;
	if (!depth) return RERR_OK;
	for (i=0; i<16; i++) {
		if (data[i]) {
			ret = tlst_freesub ((char**)(data[i]), depth-1);
			if (!RERR_ISOK(ret)) ret2 = ret;
			free (data[i]);
		}
	}
	bzero (data, sizeof (char*) * 16);
	return ret2;
}


int
tlst_depth (num)
	unsigned	num;
{
	return (tlst_logd (num) - 1) / 4;
}

int
tlst_logd (num)
	unsigned	num;
{
	return tlst_logd2 (num, sizeof (num) * 8);
}

static
int
tlst_logd2 (num, size)
	unsigned	num, size;
{
	unsigned	half;

	if (size < 1) return 0;
	if (size == 1) return num & 1;
	size /= 2;
	half = num >> size;
	if (half) return tlst_logd2 (half, size) + size;
	return tlst_logd2 (num, size);
}



int
tlst_cpy (dst, src)
	struct tlst			*dst;
	const struct tlst	*src;
{
	void		*p;
	size_t	i;
	int		ret;

	if (!dst || !src) return RERR_PARAM;
	*dst = TLST_INIT (src->size);
	TLST_FOREACHPTR2 (p, *(struct tlst*)src, i) {
		ret = tlst_add (dst, p);
		if (!RERR_ISOK(ret)) {
			tlst_free (dst);
			return ret;
		}
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
