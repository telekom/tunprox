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
#include <stdarg.h>
#include <ctype.h>

#include "textop.h"
#include "errors.h"
#include "slog.h"
#include "named_chars.h"
#include "xmlparser.h"


static int strip_it (struct xml_sub_list*, int);
static int add_utf8_char (char**, int);
static int normalize_string (char*, int);
static int is_empty_str (char*);
static int parse_body (struct xml_sub_list*, char*);
static int parse_attr (struct xml_attr_list*, char*);
static int is_name_char (char);
static int parse_tag (struct xml_tag*, char*);
static int parse_node (struct xml_tag*, char**);
static int is_question_tag (char*);
static int find_end_of_comment (char**);
static int find_end_of_str (char**);
static int is_comment (char*);
static int is_exclam_tag (char*);
static int cmp_tag_name (char*, char*, int*);
static int find_next_tag (char**);
static int find_end_of_tag (char**, int*);
static int find_end_tag (char**, char*, int, int);
static ssize_t xmlquotesize (const char *, int);

static int search_sublist (char**, struct xml_sub_list*, char*, int, int*, int);
static int search_tag (char**, struct xml_tag*, char*, int, int*, int);
static int tagsearch_sublist (struct xml_tag**, struct xml_sub_list*, char*, int, int*, int);
static int tag_search (struct xml_tag**, struct xml_tag*, char*, int, int*, int);
static int get_query_next (char**, int);
static int split_query (struct xml_attr_list*, char*);
static int hfree_attr_list (struct xml_attr_list*);
static int cmp_attr_list (struct xml_attr_list*, struct xml_attr_list*);
static int print_tag (struct xml_tag*, int);
static int print_space (int);
static int print_sublist (struct xml_sub_list*, int);
static int print_text (char*, int);
static int print_attr (struct xml_attr*, int);
static int tag_add_father (struct xml_tag*);

#define FLEXVAL_STRING	0
#define FLEXVAL_INT		1
#define FLEXVAL_FLOAT	2

struct flexval {
	char		*s;
	union {
		int	i;
		float	f;
	};
	int		what;
};

static int atoflex (struct flexval*, char *);
static int cmpflexval (struct flexval*, struct flexval*);
static int cmpflexstr (struct flexval*, char*);
static int reconstruct_xml (char**, struct xml_tag*, int);
static int recon_getsize (struct xml_tag*, int);
static int recon_writexml (char*, struct xml_tag*, int);



int
xml_parse (xml, xml_text, flags)
	struct xml	*xml;
	char			*xml_text;
	int			flags;
{
	int	ret;

	if (!xml || !xml_text) return RERR_PARAM;
	bzero (xml, sizeof (struct xml));
	xml->_buflen = strlen (xml_text) + 1;
	if ((flags & XMLPARSER_FLAG_NOTCOPY) || (flags & XMLPARSER_FLAG_NOTFREE)) {
		xml->_buffer = xml_text;
	} else {
		xml->_buffer = strdup (xml_text);
		if (!xml->_buffer) return RERR_NOMEM;
	}
	xml->flags = flags;
	xml->top_tag.name = (char*)"";	/* we don't want NULL pointer here */
	xml->top_tag.father = NULL;
	ret = parse_body (&(xml->top_tag.sub_list), xml->_buffer);
	if (!RERR_ISOK(ret)) {
		xml_hfree (xml);
		return ret;
	}
	ret = strip_it (&(xml->top_tag.sub_list), flags);
	if (!RERR_ISOK(ret)) {
		xml_hfree (xml);
		return ret;
	}
	ret = xml_update_father (xml);
	if (!RERR_ISOK(ret)) {
		xml_hfree (xml);
		return ret;
	}
	return RERR_OK;
}


int
xml_parse_withdtd (xml, xml_text, dtdfile, flags)
	struct xml	*xml;
	char			*xml_text;
	int			flags;
	char			*dtdfile;
{
	int			ret;
#if 0
	struct dtd	dtd;
#endif

	ret = xml_parse (xml, xml_text, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing xml file: %s", rerr_getstr3 (ret));
		return ret;
	}
#if 0	/* doesn't work properly */
	if (!dtdfile) return RERR_OK;
	ret = dtd_parse (&dtd, dtdfile, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing dtdfile >>%s<<: %s", dtdfile,
								rerr_getstr3(ret));
		return ret;
	}
	ret = dtd_check (xml, &dtd, flags);
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_NOT_MATCH_DTD) {
			FRLOGF (LOG_ERR, "error in checking xml: %s", rerr_getstr3(ret));
		} else {
			FRLOGF (LOG_DEBUG, "xml does not match dtd file");
		}
		return ret;
	}
	dtd_hfree (&dtd);
	if (!RERR_ISOK(ret)) return ret;
#endif
	return RERR_OK;
}



int
xml_hfree (xml)
	struct xml	*xml;
{
	int	ret;

	if (!xml) return 0;
	if (xml->_buffer && !(xml->flags & XMLPARSER_FLAG_NOTFREE)) {
		free (xml->_buffer);
		xml->_buffer = NULL;
	}
	ret = xml_hfree_sub_list (&(xml->top_tag.sub_list));
	bzero (xml, sizeof (struct xml));
	return ret;
}

int
xml_free (xml)
	struct xml	*xml;
{
	if (!xml) return 0;
	xml_hfree (xml);
	free (xml);
	return 1;
}


int
xml_search (text, xml, query, flags)
	char			**text;
	struct xml	*xml;
	const char	*query;
	int			flags;
{
	return xmltag_nsearch (text, &(xml->top_tag), query, 0, flags);
}

int
xml_nsearch (text, xml, query, n, flags)
	char			**text;
	struct xml	*xml;
	const char	*query;
	int			flags, n;
{
	return xmltag_nsearch (text, &(xml->top_tag), query, n, flags);
}

int
xml_fsearch (
	char			**text,
	struct xml	*xml,
	int			flags,
	const char	*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!text || !xml || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xml_vfnsearch (text, xml, flags, 0, query_fmt, ap);
	va_end (ap);
	return ret;
}

int
xml_fnsearch (
	char			**text,
	struct xml	*xml,
	int			flags,
	int			n,
	const char	*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!text || !xml || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xml_vfnsearch (text, xml, flags, n, query_fmt, ap);
	va_end (ap);
	return ret;
}

int
xml_vfsearch (text, xml, flags, query_fmt, ap)
	char			**text;
	struct xml	*xml;
	const char	*query_fmt;
	int			flags;
	va_list		ap;
{
	return xml_vfnsearch (text, xml, flags, 0, query_fmt, ap);
}

int
xml_vfnsearch (text, xml, flags, n, query_fmt, ap)
	char			**text;
	struct xml	*xml;
	const char	*query_fmt;
	int			flags, n;
	va_list		ap;
{
	char	query[1024];
	char	*query2=NULL, *query3;
	int	num, ret;

	if (!text || !xml || !query_fmt) return RERR_PARAM;
	num = vsnprintf (query, sizeof (query)-1, query_fmt, ap);
	query[sizeof(query)-1]=0;
	query3=query;
	if (num<0) return RERR_SYSTEM;
	if (num>=(ssize_t)sizeof (query)-1) {
		query2 = malloc (num+1);
		if (!query2) return RERR_NOMEM;
		vsprintf (query2, query_fmt, ap);
		query3 = query2;
	}
	ret = xml_nsearch (text, xml, query3, n, flags);
	if (query2) free (query2);
	return ret;
}



int
xml_hfree_sub_list (sub_list)
	struct xml_sub_list	*sub_list;
{
	int	i;

	if (!sub_list) return 0;
	for (i=0; i<sub_list->len; i++) {
		xml_hfree_sub (&(sub_list->list[i]));
	}
	if (sub_list->list) {
		bzero (sub_list->list, sizeof (struct xml_sub) * sub_list->len);
		free (sub_list->list);
	}
	bzero (sub_list, sizeof (struct xml_sub_list));
	return 1;
}

int
xml_hfree_sub (sub)
	struct xml_sub	*sub;
{
	struct xml_tag	*tag;

	if (!sub) return 0;
	if (SUB_TYPE_ISTAG (sub->type)) {
		tag=&(sub->body.tag);
		if (tag->attr_list.list) free (tag->attr_list.list);
		xml_hfree_sub_list (&(tag->sub_list));
		bzero (tag, sizeof (struct xml_tag));
	}
	return 1;
}




int
xml_print (xml)
	struct xml	*xml;
{
	if (!xml) return RERR_PARAM;
	return print_sublist (&(xml->top_tag.sub_list), 0);
}


int
xml_update_father (xml)
	struct xml	*xml;
{
	if (!xml) return RERR_PARAM;
	xml->top_tag.father = NULL;		/* the top tag has no father */
	return tag_add_father (&(xml->top_tag));
}


int
xmltag_search (text, tag, query, flags)
	char				**text;
	struct xml_tag	*tag;
	const char		*query;
	int				flags;
{
	return xmltag_nsearch (text, tag, query, 0, flags);
}

int
xmltag_nsearch (text, tag, query, n, flags)
	char				**text;
	struct xml_tag	*tag;
	const char		*query;
	int				flags, n;
{
	int	ret;
	char	*nquery;

	if (!text || !tag || !query) return RERR_PARAM;
	nquery = strdup (query);
	if (!nquery) return RERR_NOMEM;
	n++;
	ret = search_tag (text, tag, nquery, 1, &n, flags);
	if (RERR_ISOK(ret) && text && (!*text || !**text) &&
				(flags & XML_SEARCH_EMPTYNULL)) {
		*text = NULL;
		ret = RERR_NOT_FOUND;
	}
	if (ret == RERR_NOT_FOUND && (flags & XML_SEARCH_REQUIRED)) {
		FRLOGF (LOG_ERR, "required field >>%s<< is missing", query);
	}
	if (ret == RERR_NOT_FOUND && (flags & XML_SEARCH_NULLEMPTY)) {
		if (text) *text = (char*)"";
		ret = RERR_OK;
	}
	if (ret == RERR_NOT_FOUND && (flags & XML_SEARCH_NULLOK)) {
		ret = RERR_OK;
	}
	free (nquery);
	return ret;
}


int
xmltag_fsearch (
	char				**text,
	struct xml_tag	*tag,
	int				flags,
	const char		*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!text || !tag || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xmltag_vfnsearch (text, tag, flags, 0, query_fmt, ap);
	va_end (ap);
	return ret;
}


int
xmltag_fnsearch (
	char				**text,
	struct xml_tag	*tag,
	int				flags,
	int				n,
	const char		*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!text || !tag || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xmltag_vfnsearch (text, tag, flags, n, query_fmt, ap);
	va_end (ap);
	return ret;
}

int
xmltag_vfsearch (text, tag, flags, query_fmt, ap)
	char				**text;
	struct xml_tag	*tag;
	const char		*query_fmt;
	int				flags;
	va_list			ap;
{
	return xmltag_vfnsearch (text, tag, flags, 0, query_fmt, ap);
}

int
xmltag_vfnsearch (text, tag, flags, n, query_fmt, ap)
	char				**text;
	struct xml_tag	*tag;
	const char		*query_fmt;
	int				flags, n;
	va_list			ap;
{
	char	query[1024];
	char	*query2=NULL, *query3;
	int	num, ret;

	if (!text || !tag || !query_fmt) return RERR_PARAM;
	num = vsnprintf (query, sizeof (query)-1, query_fmt, ap);
	query[sizeof(query)-1]=0;
	query3=query;
	if (num<0) return RERR_SYSTEM;
	if (num>=(ssize_t)sizeof (query)-1) {
		query2 = malloc (num+1);
		if (!query2) return RERR_NOMEM;
		vsprintf (query2, query_fmt, ap);
		query3 = query2;
	}
	ret = xmltag_nsearch (text, tag, query3, n, flags);
	if (query2) free (query2);
	return ret;
}

int
xmltag_searchtag (otag, tag, query, flags)
	struct xml_tag	**otag, *tag;
	const char		*query;
	int				flags;
{
	return xmltag_nsearchtag (otag, tag, query, 0, flags);
}

int
xmltag_nsearchtag (otag, tag, query, n, flags)
	struct xml_tag	**otag, *tag;
	const char		*query;
	int				flags, n;
{
	n++;
	return tag_search (otag, tag, (char*)query, 1, &n, flags);
}

int
xmltag_vfsearchtag (otag, tag, flags, query_fmt, ap)
	struct xml_tag	**otag, *tag;
	const char		*query_fmt;
	int				flags;
	va_list			ap;
{
	return xmltag_vfnsearchtag (otag, tag, flags, 0, query_fmt, ap);
}

int
xmltag_vfnsearchtag (otag, tag, flags, n, query_fmt, ap)
	struct xml_tag	**otag, *tag;
	const char		*query_fmt;
	int				flags, n;
	va_list			ap;
{
	char	query[1024];
	char	*query2=NULL, *query3;
	int	num, ret;

	if (!otag || !tag || !query_fmt) return RERR_PARAM;
	num = vsnprintf (query, sizeof (query)-1, query_fmt, ap);
	query[sizeof(query)-1]=0;
	query3=query;
	if (num<0) return RERR_SYSTEM;
	if (num>=(ssize_t)sizeof (query)-1) {
		query2 = malloc (num+1);
		if (!query2) return RERR_NOMEM;
		vsprintf (query2, query_fmt, ap);
		query3 = query2;
	}
	ret = xmltag_nsearchtag (otag, tag, query3, n, flags);
	if (query2) free (query2);
	return ret;
}

int
xmltag_fsearchtag (
	struct xml_tag	**otag,
	struct xml_tag	*tag,
	int				flags,
	const char		*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!otag || !tag || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xmltag_vfnsearchtag (otag, tag, flags, 0, query_fmt, ap);
	va_end (ap);
	return ret;
}

int
xmltag_fnsearchtag (
	struct xml_tag	**otag,
	struct xml_tag	*tag,
	int				flags,
	int				n,
	const char		*query_fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!otag || !tag || !query_fmt) return RERR_PARAM;
	va_start (ap, query_fmt);
	ret = xmltag_vfnsearchtag (otag, tag, flags, n, query_fmt, ap);
	va_end (ap);
	return ret;
}


#if 0	/* we already have it in xmltag.[ch] */
int
xmltag_searchattr (val, tag, attr)
	char				**val, *attr;
	struct xml_tag	*tag;
{
	if (!tag || !attr) return RERR_PARAM;
	for (i=0; i<tag->attr_list->len; i++) {
		if (!strcasecmp (tag->attr_list->list[i].var, attr)) {
			if (val) *val = tag->attr_list->list[i].val;
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
}

int
xmltag_hasattr (tag, attr)
	char				*attr;
	struct xml_tag	*tag;
{
	return xmltag_searchattr (NULL, tag, attr);
}
#endif




/***************************
 *  static functions
 ***************************/



static
int
find_end_tag (ptr, name, isintag, null_body)
	char	**ptr, *name;
	int	isintag, null_body;
{
	int	ret, isendtag, num_nested;
	char	*str;

	if (!ptr || !*ptr || !name) return RERR_PARAM;
	if (isintag) {
		ret = find_end_of_tag (ptr, &isendtag);
		if (!RERR_ISOK(ret)) return ret;
		if (isendtag) return RERR_OK;
	}
	num_nested=1;
	while (1) {
		ret = find_next_tag (ptr);
		if (!RERR_ISOK(ret)) return ret;
		if (is_comment (*ptr)) {
			ret = find_end_of_comment (ptr);
			if (!RERR_ISOK(ret)) return ret;
		}
		if (!is_question_tag (*ptr)) {
			if (cmp_tag_name (*ptr, name, &isendtag)) {
				if (isendtag) {
					num_nested--;
				} else {
					num_nested++;
				}
			}
			if (!num_nested && null_body) {
				str = *ptr;
				*str=0;
				str++;
				*ptr=str;
			}
		}
		ret = find_end_of_tag (ptr, NULL);
		if (!RERR_ISOK(ret)) return ret;
		if (!num_nested) return RERR_OK;
	}
	return RERR_INTERNAL;
}


static
int
find_end_of_tag (ptr, isendtag)
	char	**ptr;
	int	*isendtag;
{
	char	*s;
	int	ret;

	if (!ptr || !*ptr) return RERR_PARAM;
	for (s=*ptr; *s && *s!='>'; s++) {
		if (*s=='"') {
			s++;
			ret = find_end_of_str (&s);
			if (!RERR_ISOK(ret)) {
				*ptr=s;
				return ret;
			}
		}
	}
	if (!*s) {
		*ptr = s;
		/* return RERR_NOT_FOUND; */
		return RERR_OK;
	}
	if (isendtag) {
		*isendtag = s[-1]=='/';
	}
	s++;
	*ptr=s;
	return RERR_OK;
}


static
int
find_next_tag (ptr)
	char	**ptr;
{
	char	*s;

	if (!ptr || !*ptr) return RERR_PARAM;
	for (s=*ptr; *s && *s!='<'; s++);
	*ptr = s;
	if (!*s) return RERR_NOT_FOUND;
	return RERR_OK;
}


static
int
cmp_tag_name (ptr, name, isendtag)
	char	*ptr, *name;
	int	*isendtag;
{
	int	len;

	if (!ptr || !name) return 0;
	if (isendtag) *isendtag=0;
	if (*ptr=='<') ptr++;
	if (*ptr=='/') {
		if (isendtag) *isendtag=1;
		ptr++;
	}
	ptr = top_skipwhite (ptr);
	len = strlen (name);
	if (strncasecmp (ptr, name, len) != 0) return 0;
	ptr+=len;
	if (is_name_char (*ptr)) return 0;
	return 1;
}


static
int
find_end_of_str (ptr)
	char	**ptr;
{
	char	*s;

	if (!ptr || !*ptr) return RERR_PARAM;
	for (s=*ptr; *s && *s!='"'; s++);
	*ptr=s;
	if (!*s) return RERR_NOT_FOUND;
	return RERR_OK;
}


static
int
is_comment (str)
	char	*str;
{
	if (!str) return 0;
	if (*str=='<') str++;
	str = top_skipwhite (str);
	return strncmp (str, "!--", 3) == 0;
}

static
int
is_exclam_tag (str)
	char	*str;
{
	if (!str) return 0;
	if (*str=='<') str++;
	str = top_skipwhite (str);
	return (*str=='!') && (strncmp (str, "!--", 3) != 0);
}


static
int
find_end_of_comment (ptr)
	char	**ptr;
{
	char	*s;
	int	have_end=0;

	if (!ptr || !*ptr) return RERR_PARAM;
	for (s=*ptr; *s; s++) {
		if (*s=='-' && s[1]=='-' && s[2]=='>') {
			s+=3; 
			have_end=1;
			break;
		}
	}
	*ptr=s;
	if (!have_end) return RERR_NOT_FOUND;
	return RERR_OK;
}

static
int
is_question_tag (str)
	char	*str;
{
	if (!str) return 0;
	if (*str=='<') str++;
	str = top_skipwhite (str);
	return *str=='?';
}


static
int
parse_node (tag, ptr)
	struct xml_tag	*tag;
	char				**ptr;
{
	int	ret;
	char	*str, *s;
	int	isendtag;

	if (!tag || !ptr || !*ptr) return RERR_PARAM;
	*tag = XML_TAG_NULL;
	str=*ptr;
	ret = find_end_of_tag (ptr, &isendtag);
	if (!RERR_ISOK(ret)) return ret;
	s=*ptr;
	s[-1]=0;
	if (isendtag) s[-2]=0;
	ret = parse_tag (tag, str);
	if (!RERR_ISOK(ret)) return ret;
	if (isendtag) return RERR_OK;
	str = *ptr;
	ret = find_end_tag (ptr, tag->name, /*isintag=*/ 0, /*null_body=*/ 1);
	/* if (!RERR_ISOK(ret)) return ret; */
	ret = parse_body (&(tag->sub_list), str);
	return ret;
}


static
int
parse_tag (tag, ptr)
	struct xml_tag	*tag;
	char				*ptr;
{
	char	*str;
	int	has_attr, ret;

	if (!tag || !ptr) return RERR_PARAM;
	for (str=ptr; *ptr && is_name_char (*ptr); ptr++);
	has_attr = *ptr != '>' && *ptr && *ptr != '/';
	if (*ptr) {
		*ptr=0;
		ptr++;
	}
	tag->name = str;
	if (has_attr) {
		ret = parse_attr (&(tag->attr_list), ptr);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
is_name_char (c)
	char	c;
{
	return isalnum (c) || c=='_';
}

static
int
parse_attr (attr_list, ptr)
	struct xml_attr_list	*attr_list;
	char						*ptr;
{
	char					*var, *val;
	int					num, has_eq, ret, has_quot;
	struct xml_attr	*attr;

	if (!attr_list || !ptr) return RERR_PARAM;
	*attr_list = XML_ATTR_LIST_NULL;
	while (ptr && *ptr) {
		ptr = top_skipwhite (ptr);
		if (!*ptr || *ptr=='>' || (*ptr=='/' && (ptr[1]=='>' || !ptr[1]))) 
			break;
		if (!is_name_char (*ptr)) return RERR_INVALID_FORMAT;
		for (var = ptr; is_name_char (*ptr); ptr++);
		if (!*ptr || *ptr=='>') {
			val=NULL;
		} else {
			has_eq = *ptr == '=';
			has_quot= *ptr == '"';
			*ptr = 0;
			ptr = top_skipwhite (ptr+1);
			if (*ptr == '=') has_eq=1;
			ptr = top_skipwhiteplus (ptr, "=");
			if (!has_eq) {
				val=NULL;
			} else if (*ptr == '"' || has_quot) {
				val=++ptr;
				ret = find_end_of_str (&ptr);
				if (RERR_ISOK(ret)) {
					*ptr=0;
					ptr++;
				}
			} else if (is_name_char (*ptr)) {
				for (val=ptr; is_name_char (*ptr); ptr++);
				*ptr=0;
			} else {
				return RERR_INVALID_FORMAT;
			}
		}
		/* if (val) normalize_string (val); */
		num=attr_list->len++;
		attr_list->list = realloc (attr_list->list, attr_list->len *
									sizeof (struct xml_attr));
		if (!attr_list->list) return RERR_NOMEM;
		attr = &(attr_list->list[num]);
		*attr = XML_ATTR_NULL;
		attr->var = var;
		attr->val = val;
	}
	return RERR_OK;
}


static
int
parse_body (sub_list, ptr)
	struct xml_sub_list	*sub_list;
	char						*ptr;
{
	struct xml_sub	*sub;
	int				num, ret;
	char				*str;
	int				istag, isend;

	if (!sub_list || !ptr) return RERR_PARAM;
	istag = *ptr == '<';
	isend = !*ptr;
	while (!isend) {
		num = sub_list->len++;
		sub_list->list = realloc (sub_list->list, sub_list->len * 
										sizeof (struct xml_sub));
		if (!sub_list->list) return RERR_NOMEM;
		sub = &(sub_list->list[num]);
		bzero (sub, sizeof (struct xml_sub));
		if (!istag) {
			str = ptr;
			ret = find_next_tag (&ptr);
			if (RERR_ISOK(ret)) {
				istag=1;
				*ptr=0;
				ptr++;
			} else {
				istag=0;
				isend=1;
			}
			/* normalize_string (str); */
			if (is_empty_str (str)) {
				sub->type = SUB_TYPE_EMPTY_TEXT;
			} else {
				sub->type = SUB_TYPE_TEXT;
			}
			sub->body.text = str;
			continue;
		}
		if (*ptr == '<') ptr++;
		ptr = top_skipwhite (ptr);
		if (is_question_tag (ptr)) {
			str = ptr;
			ret = find_end_of_tag (&ptr, NULL);
			if (RERR_ISOK(ret)) {
				istag = *ptr == '<';
				ptr[-1]=0;
			} else {
				istag=0;
				isend=1;
			}
			sub->type = SUB_TYPE_QUESTION_TAG;
			sub->body.text = str;
		} else if (is_comment (ptr)) {
			str = ptr+3;
			ret = find_end_of_comment (&ptr);
			if (RERR_ISOK(ret)) {
				if (ptr-str>=3) {
					ptr[-3]=0;
				} else {
					*str=0;
				}
				istag = *ptr == '<';
			} else {
				istag = 0;
				isend = 1;
			}
			sub->type = SUB_TYPE_COMMENT;
			sub->body.text = str;
		} else if (is_exclam_tag (ptr)) {
			ptr++;
			str = ptr;
			ret = find_end_of_tag (&ptr, NULL);
			if (RERR_ISOK(ret)) {
				istag = *ptr == '<';
				ptr[-1]=0;
			} else {
				istag=0;
				isend=1;
			}
			sub->type = SUB_TYPE_EXCLAM_TAG;
			ret = parse_tag (&(sub->body.tag), ptr);
			if (!RERR_ISOK(ret)) return ret;
		} else {
			sub->type = SUB_TYPE_TAG;
			ret = parse_node (&(sub->body.tag), &ptr);
			if (!RERR_ISOK(ret)) return ret;
			istag = *ptr == '<';
		}
		if (!*ptr) isend=1;
	}
	return RERR_OK;
}



static
int
is_empty_str (str)
	char	*str;
{
	if (!str || !*str) return 1;
	for (; *str; str++) {
		if (!isspace (*str)) return 0;
	}
	return 1;
}

int
xml_quotestr (outstr, str, flags)
	char			**outstr;
	const char	*str;
	int			flags;
{
	ssize_t		len;
	const char	*s;
	char			*s2;
	int			c;

	if (!outstr || !str) return RERR_PARAM;
	len = xmlquotesize (str, flags);
	if (len < 0) return (int)len;
	*outstr = s2 = malloc (len + 1);
	if (!s2) return RERR_NOMEM;

	for (s=str; *s; s++) {
		if ((unsigned char)*s < 128) {
			switch (*s) {
			case '<':
				s2 += sprintf (s2, "&lt;");
				break;
			case '>':
				s2 += sprintf (s2, "&gt;");
				break;
			case '&':
				s2 += sprintf (s2, "&amp;");
				break;
			case '\'':
				s2 += sprintf (s2, "&apos;");
				break;
			case '\"':
				s2 += sprintf (s2, "&quot;");
				break;
			default:
				if (*s < 32 && *s != '\n' && *s != '\t') {
					s2 += sprintf (s2, "&#%u;", (unsigned) *s);
				} else if (*s == 127) {
					s2 += sprintf (s2, "&#%u;", (unsigned) *s);
				} else {
					*s2 = *s;
					s2++;
				}
				break;
			}
		} else if (flags & XML_F_LATIN1) {
			s2 += sprintf (s2, "&#%u;", (unsigned) *s);
		} else if ((unsigned char)*s < 0xc0) {
			/* do nothing */
		} else if ((unsigned char)*s < 0xe0) {
			c = (((unsigned)*s & 0x1f) << 6);
			s++;
			c |= ((unsigned)*s & 0x3f);
			s2 += sprintf (s2, "&#%u;", (unsigned) c);
		} else if ((unsigned char)*s < 0xf0) {
			c = ((unsigned)*s & 0x0f) << 12;
			s++;
			c |= ((unsigned)*s & 0x3f) << 6;
			s++;
			c |= ((unsigned)*s & 0x3f);
			s2 += sprintf (s2, "&#%u;", (unsigned) c);
		} else if ((unsigned char)*s < 0xf8) {
			c = ((unsigned)*s & 0x07) << 18;
			s++;
			c |= ((unsigned)*s & 0x3f) << 12;
			s++;
			c |= ((unsigned)*s & 0x3f) << 6;
			s++;
			c |= ((unsigned)*s & 0x3f);
			s2 += sprintf (s2, "&#%u;", (unsigned) c);
		} else if ((unsigned char)*s < 0xfc) {
			c = ((unsigned)*s & 0x03) << 24;
			s++;
			c |= ((unsigned)*s & 0x3f) << 18;
			s++;
			c |= ((unsigned)*s & 0x3f) << 12;
			s++;
			c |= ((unsigned)*s & 0x3f) << 6;
			s++;
			c |= ((unsigned)*s & 0x3f);
			s2 += sprintf (s2, "&#%u;", (unsigned) c);
		} else if ((unsigned char)*s < 0xfe) {
			c = ((unsigned)*s & 0x01) << 30;
			s++;
			c |= ((unsigned)*s & 0x3f) << 24;
			s++;
			c |= ((unsigned)*s & 0x3f) << 18;
			s++;
			c |= ((unsigned)*s & 0x3f) << 12;
			s++;
			c |= ((unsigned)*s & 0x3f) << 6;
			s++;
			c |= ((unsigned)*s & 0x3f);
			s2 += sprintf (s2, "&#%u;", (unsigned) c);
		}
	}
	*s2 = 0;
	return RERR_OK;
}

static
ssize_t
xmlquotesize (str, flags)
	const char	*str;
	int			flags;
{
	ssize_t		len;
	const char	*s;

	if (!str) return RERR_PARAM;
	for (len=0, s=str; *s; s++) {
		if ((unsigned char)*s < 128) {
			switch (*s) {
			case '<':
			case '>':
				len += 4;
				break;
			case '&':
				len += 5;
				break;
			case '\'':
			case '\"':
				len += 6;
				break;
			default:
				if (*s < 32 && *s != '\n' && *s != '\t') {
					len += 5;
				} else if (*s == 127) {
					len += 6;
				} else {
					len ++;
				}
				break;
			}
		} else if (flags & XML_F_LATIN1) {
			len += 6;
		} else if ((unsigned char)*s < 0xc0) {
			/* do nothing */
		} else if ((unsigned char)*s < 0xe0) {
			len += 7;
			s++;
		} else if ((unsigned char)*s < 0xf0) {
			len += 8;
			s+=2;
		} else if ((unsigned char)*s < 0xf8) {
			len += 10;
			s+=3;
		} else {
			len += 13;
			s+=4;
		}
	}
	return len;
}

int
xml_unquotestrconst (outstr, str, flags)
	char 			**outstr;
	const char	*str;
	int			flags;
{
	if (!outstr) return RERR_PARAM;
	return xml_unquotestr (outstr, (char*)(void*)str, flags);
}

int
xml_unquotestr (outstr, str, flags)
	char	**outstr;
	char	*str;
	int	flags;
{
	if (!str) return RERR_PARAM;
	if (outstr) {
		*outstr = strdup (str);
		if (!*outstr) return RERR_NOMEM;
		str = *outstr;
	}
	return normalize_string (str, flags);
}

#define ISHEX(c)	((((c)>='0')&&((c)<='9'))||(((c)>='A')&&((c)<='F'))||\
						(((c)>='a')&&((c)<='f')))

static
int
normalize_string (str, flags)
	char	*str;
	int	flags;
{
	char							*s, *s2;
	struct xml_named_chars	*nc;
	int							len;
	int							num, islatin1;

	if (!str) return RERR_PARAM;
	islatin1 = (flags & XMLPARSER_FLAG_LATIN) ? 1:0;
	for (s=str; *s; s++, str++) {
		if (*s!='&') {
			if (islatin1) {
				add_utf8_char (&str, (int)((unsigned)((unsigned char)*s)));
			} else {
				*str=*s;
			}
			continue;
		}
		if (s[1] != '#') {
			for (nc=named_chars; nc->name; nc++) {
				len = strlen (nc->name);
				if (!strncasecmp (nc->name, s, len)) break;
			} 
			if (nc->name) {
				add_utf8_char (&str, nc->uni_val);
				s+=len-1;
			} else {
				/* ignore named char - it's unknown */
				for (s2=s; *s2 && *s2!=';' && s2-s<10; s2++);
				if (*s2==';') s=s2;
			}
		} else {
			if (s[2] == 'x' || s[2] == 'X') {
				for (s+=3,num=0; ISHEX (*s); s++) {
					num*=16;
					if (*s>='0' && *s<='9') {
						num+=*s-'0';
					} else if (*s>='a' && *s<='f') {
						num+=*s-'a'+10;
					} else if (*s>='A' && *s<='F') {
						num+=*s-'A'+10;
					}
				}
			} else {
				for (s+=2,num=0; *s>='0' && *s<='9'; s++) {
					num*=10;
					num+=*s-'0';
				}
			}
			add_utf8_char (&str, num);
		}
	}
	*str=0;
	return RERR_OK;
}


static
int
add_utf8_char (ptr, val)
	char	**ptr;
	int	val;
{
	char	*str;

	str=*ptr;
	if (val<0) {
		*str='?';
		return RERR_PARAM;
	} else if (val<128) {
		*str=val;
	} else if (val<(1<<11)) {
		*str=0xc0 | ((val>>6)&0x1f);
		str++;
		*str=0x80 | (val&0x3f);
	} else if (val<(1<<16)) {
		*str=0xe0 | ((val>>12)&0x0f);
		str++;
		*str=0x80 | ((val>>6)&0x3f);
		str++;
		*str=0x80 | (val&0x3f);
	} else if (val<(1<<21)) {
		*str=0xf0 | ((val>>18)&0x07);
		str++;
		*str=0x80 | ((val>>12)&0x3f);
		str++;
		*str=0x80 | ((val>>6)&0x3f);
		str++;
		*str=0x80 | (val&0x3f);
	} else if (val<(1<<26)) {
		*str=0xf8 | ((val>>24)&0x03);
		str++;
		*str=0x80 | ((val>>18)&0x3f);
		str++;
		*str=0x80 | ((val>>12)&0x3f);
		str++;
		*str=0x80 | ((val>>6)&0x3f);
		str++;
		*str=0x80 | (val&0x3f);
	} else {
		*str=0xfc | ((val>>30)&0x01);
		str++;
		*str=0x80 | ((val>>24)&0x3f);
		str++;
		*str=0x80 | ((val>>18)&0x3f);
		str++;
		*str=0x80 | ((val>>12)&0x3f);
		str++;
		*str=0x80 | ((val>>6)&0x3f);
		str++;
		*str=0x80 | (val&0x3f);
	}
	return RERR_OK;
}


static
int
strip_it (sub_list, flags)
	struct xml_sub_list	*sub_list;
	int						flags;
{
	int						i,j, type;
	struct xml_sub			*sub;
	char						*s;
	struct xml_attr_list	*attr_list;

	if (!sub_list) return RERR_PARAM;
	for (i=0; i<sub_list->len; i++) {
		sub=&(sub_list->list[i]);
		type=sub->type;
		if (type==SUB_TYPE_TEXT && !sub->body.text) {
			type=sub->type=SUB_TYPE_EMPTY_TEXT;
		}
		if (type<SUB_TYPE_MIN || type > SUB_TYPE_MAX ||
				(type==SUB_TYPE_QUESTION_TAG && 
					(flags & XMLPARSER_FLAG_NO_QUESTION)) ||
				(type==SUB_TYPE_COMMENT && 
					(flags & XMLPARSER_FLAG_NO_COMMENT)) ||
				(type==SUB_TYPE_EMPTY_TEXT && 
					(flags & XMLPARSER_FLAG_NO_EMPTY_TEXT)) ||
				(type==SUB_TYPE_EXCLAM_TAG &&
					(flags & XMLPARSER_FLAG_NO_EXCLAM)) ||
				(type==SUB_TYPE_TAG &&
					(flags & XMLPARSER_FLAG_NO_NORMAL_TAG))) {
			if (SUB_TYPE_ISTAG (type)) {
				xmltag_hfree (&(sub_list->list[j].body.tag));
			}
			for (j=i; j<sub_list->len-1; j++) {
				sub_list->list[j] = sub_list->list[j+1];
			}
			sub_list->len--;
			i--;
		} else if (type==SUB_TYPE_TEXT) {
			normalize_string (sub->body.text, flags);
			if (flags & XMLPARSER_FLAG_STRIP_ALL_TEXT) {
				for (s=sub->body.text; isspace (*s); s++);
				sub->body.text=s;
				for (s+=strlen(s)-1; s>=sub->body.text && isspace (*s); s--);
				s++;
				*s=0;
			}
			if (!*(sub->body.text)) type=sub->type=SUB_TYPE_EMPTY_TEXT;
		} else if (type==SUB_TYPE_TAG) {
			attr_list = &(sub->body.tag.attr_list);
			for (j=0; j<attr_list->len; j++) {
				s = attr_list->list[j].val;
				if (s) normalize_string (s, flags);
			}
			strip_it (&(sub->body.tag.sub_list), flags);
		}
	}
	return RERR_OK;
}





static
int
search_sublist (text, sub_list, query, first, n, flags)
	char						**text;
	struct xml_sub_list	*sub_list;
	char						*query;
	int						first, flags, *n;
{
	int						maysub;
	int						ret, type;
	char						*next, *query_bak;
	struct xml_tag			*tag;
	struct xml_attr_list	attr_list;
	int						i;
	
	if (!sub_list || !query || !text || !n) return RERR_PARAM;
	if (!*query) return RERR_NOT_FOUND;
	maysub=first;
	if (*query == '/') {
		maysub=0;
		for (; *query == '/'; query++);
	}
	query_bak = query;
	query = strdup (query);
	if (!query) return RERR_NOMEM;
	next = query;
	get_query_next (&next, 1);
	ret = split_query (&attr_list, query);
	if (!RERR_ISOK(ret)) {
		free (query);
		return ret;
	}
	for (i=0; i<sub_list->len; i++) {
		type = sub_list->list[i].type;
		if (!(SUB_TYPE_ISTAG (type))) continue;
		if ((flags & XML_SEARCH_NO_NORMTAG) && type == SUB_TYPE_TAG) continue;
		if (!(flags & XML_SEARCH_EXCLAM_TAG) && type == SUB_TYPE_EXCLAM_TAG) 
			continue;
		if (!(flags & XML_SEARCH_QUESTION_TAG) && type == SUB_TYPE_QUESTION_TAG)
			continue;
		tag = &(sub_list->list[i].body.tag);
		if (!tag->name || strcasecmp (tag->name, query) != 0) continue;
		if (!cmp_attr_list (&(tag->attr_list), &attr_list)) continue;
		ret = search_tag (text, tag, next, 0, n, flags);
		if (ret != RERR_NOT_FOUND) {
			hfree_attr_list (&attr_list);
			free (query);
			return ret;
		}
	}
	hfree_attr_list (&attr_list);
	free (query);
	if (!maysub) return RERR_NOT_FOUND;

	/* continue search in subtags */
	ret = RERR_NOT_FOUND;
	for (i=0; i<sub_list->len; i++) {
		if (!SUB_TYPE_ISTAG (sub_list->list[i].type)) continue;
		tag = &(sub_list->list[i].body.tag);
		ret = search_tag (text, tag, query_bak, maysub, n, flags);
		if (ret != RERR_NOT_FOUND) break;
	}
	return ret;
}

static
int
search_tag (text, tag, query, first, n, flags)
	char				**text;
	struct xml_tag	*tag;
	char				*query;
	int				first, flags, *n;
{
	int	i;
	char	*next;

	if (!text || !tag || !*n) return RERR_PARAM;
	if (query) {
		query = top_skipwhiteplus (query, "/");
		if (!*query) query=NULL;
	}
	if (!query) {
		if (flags & XML_SEARCH_RECON_XML) {
			return reconstruct_xml (text, tag, flags);
		}
		if ((flags & XML_SEARCH_TEXT_ONLY) && tag->sub_list.len != 1) {
			return RERR_NOT_FOUND;
		}
		for (i=0; i<tag->sub_list.len; i++) {
			if (tag->sub_list.list[i].type==SUB_TYPE_TEXT) {
				(*n)--;
				if (*n<=0) {
					*text = tag->sub_list.list[i].body.text;
					return RERR_OK;
				}
			}
		}
		for (i=0; i<tag->sub_list.len; i++) {
			if (tag->sub_list.list[i].type==SUB_TYPE_EMPTY_TEXT) {
				(*n)--;
				if (*n<=0) {
					*text = tag->sub_list.list[i].body.text;
					return RERR_OK;
				}
			}
		}
		*text=(char*)"";
		return RERR_OK;
	}
	if (/*first || */(flags & XML_SEARCH_NO_ATTR)) {
		return search_sublist (text, &(tag->sub_list), query, first, n, flags);
	}
	next = query;
	get_query_next (&next, 0);
	if (next) {
		return search_sublist (text, &(tag->sub_list), query, first, n, flags);
	}
	next = query;
	get_query_next (&next, 1);
	if (index (query, '?')) {
		return search_sublist (text, &(tag->sub_list), query, first, n, flags);
	}
	for (i=0; i<tag->attr_list.len; i++) {
		if (!strcasecmp (query, tag->attr_list.list[i].var)) {
			(*n)--;
			if (*n<=0) {
				*text = tag->attr_list.list[i].val;
				return RERR_OK;
			}
		}
	}
	return search_sublist (text, &(tag->sub_list), query, first, n, flags);
}



static
int
tag_search (otag, tag, query, first, n, flags)
	struct xml_tag	**otag, *tag;
	char				*query;
	int				first, flags, *n;
{
	int	i;
	char	*next;

	if (!otag || !tag || !n) return RERR_PARAM;
	if (query) {
		query = top_skipwhiteplus (query, "/");
		if (!*query) query=NULL;
	}
	if (!query) {
		(*n)--;
		if (*n<=0) {
			*otag = tag;
			return RERR_OK;
		} else {
			return RERR_NOT_FOUND;
		}
	}
	if (/* first || */ (flags & XML_SEARCH_NO_ATTR)) {
		return tagsearch_sublist (otag, &(tag->sub_list), query, first, n, flags);
	}
	next = query;
	get_query_next (&next, 0);
	if (next) {
		return tagsearch_sublist (otag, &(tag->sub_list), query, first, n, flags);
	}
	next = query;
	get_query_next (&next, 1);
	if (index (query, '?')) {
		return tagsearch_sublist (otag, &(tag->sub_list), query, first, n, flags);
	}
	for (i=0; i<tag->attr_list.len; i++) {
		if (!strcasecmp (query, tag->attr_list.list[i].var)) {
			(*n)--;
			if (*n<=0) {
				*otag = tag;
				return RERR_OK;
			}
		}
	}
	return tagsearch_sublist (otag, &(tag->sub_list), query, first, n, flags);
}

static
int
tagsearch_sublist (otag, sub_list, query, first, n, flags)
	struct xml_tag			**otag;
	struct xml_sub_list	*sub_list;
	char						*query;
	int						first, flags, *n;
{
	int						maysub;
	int						ret;
	char						*next, *query_bak;
	struct xml_tag			*tag;
	struct xml_attr_list	attr_list;
	int						i;
	
	if (!sub_list || !query || !otag || !n) return RERR_PARAM;
	if (!*query) return RERR_NOT_FOUND;
	maysub=first;
	if (*query == '/') {
		maysub=0;
		for (; *query == '/'; query++);
	}
	query_bak = query;
	query = strdup (query);
	if (!query) return RERR_NOMEM;
	next = query;
	get_query_next (&next, 1);
	ret = split_query (&attr_list, query);
	if (!RERR_ISOK(ret)) {
		free (query);
		return ret;
	}
	for (i=0; i<sub_list->len; i++) {
		if (sub_list->list[i].type != SUB_TYPE_TAG) continue;
		tag = &(sub_list->list[i].body.tag);
		if (!tag->name || strcasecmp (tag->name, query) != 0) continue;
		if (!cmp_attr_list (&(tag->attr_list), &attr_list)) continue;
		ret = tag_search (otag, tag, next, 0, n, flags);
		if (ret != RERR_NOT_FOUND) {
			hfree_attr_list (&attr_list);
			free (query);
			return ret;
		}
	}
	hfree_attr_list (&attr_list);
	free (query);
	if (!maysub) return RERR_NOT_FOUND;

	/* continue search in subtags */
	ret = RERR_NOT_FOUND;
	for (i=0; i<sub_list->len; i++) {
		if (sub_list->list[i].type != SUB_TYPE_TAG) continue;
		tag = &(sub_list->list[i].body.tag);
		ret = tag_search (otag, tag, query_bak, maysub, n, flags);
		if (ret != RERR_NOT_FOUND) break;
	}
	return ret;
}



static
int
get_query_next (query, nul_end)
	char	**query;
	int	nul_end;
{
	char	*next;

	if (!query && !*query) return RERR_PARAM;
	for (next = *query; *next && *next!='/'; next++) {
		if (*next == '"') 
			for (next++; *next && *next!='"'; next++);
	}
 	if (*next=='/') {
		if (nul_end) *next=0;
		for (next++; *next=='/'; next++);
	}
	if (!*next) next=NULL;
	*query=next;
	return RERR_OK;
}


static
int
split_query (attr_list, query)
	struct xml_attr_list	*attr_list;
	char						*query;
{
	char					*str;
	struct xml_attr	*attr;
	int					num /* , have_eq */;

	if (!attr_list || !query) return RERR_PARAM;
	*attr_list = XML_ATTR_LIST_NULL;
	str = index (query, '?');
	if (!str) return RERR_OK;
	*str=0; str++;
	while (1) {
		str = top_skipwhite (str);
		if (!*str) break;
		num = attr_list->len++;
		attr_list->list = realloc (attr_list->list, attr_list->len 
										* sizeof (struct xml_attr));
		if (!attr_list->list) return RERR_NOMEM;
		attr = &(attr_list->list[num]);
		*attr = XML_ATTR_NULL;
		attr->var = str;
		for (; is_name_char (*str); str++);
		/* have_eq = (*str=='='); */
		*str = 0;
		str = top_skipwhite (str+1);
		/* if (*str == '=') have_eq=1; */
		str = top_skipwhiteplus (str, "=");
		if (*str=='"') {
			str++;
			attr->val = str;
			for (; *str && *str!='"'; str++);
			if (*str) { *str=0; str++; }
		} else if (*str!=',') {
			attr->val = str;
			for (; *str && *str!=' ' && *str!=','; str++);
			if (*str) { *str=0; str++; }
		} else {
			attr->val = NULL;
		}
		for (; *str && *str!=','; str++);
	}
	return RERR_OK;
}


static
int
reconstruct_xml (text, tag, flags)
	char				**text;
	struct xml_tag	*tag;
	int				flags;
{
	int	ret, num;

	if (!text || !tag) return RERR_PARAM;
	num = recon_getsize (tag, flags);
	if (num < 0) return num;
	*text = malloc (num+1);
	if (!*text) return RERR_NOMEM;
	ret = recon_writexml (*text, tag, flags);
	if (!RERR_ISOK(ret)) {
		free (*text);
		*text = NULL;
		return ret;
	}
	return RERR_OK;
}


static
int
recon_getsize (tag, flags)
	struct xml_tag	*tag;
	int				flags;
{
	int	num, ret, i;

	if (!tag) return RERR_PARAM;
	if (flags & XML_SEARCH_RECON_TAG) {
		num = 5 + (tag->name ? 2 * strlen (tag->name) : 0);
		for (i=0; i<tag->attr_list.len; i++) {
			num += 4 + strlen (tag->attr_list.list[i].var) + 
							strlen (tag->attr_list.list[i].val);
		}
	} else {
		flags |= XML_SEARCH_RECON_TAG;
		num = 0;
	}
	for (i=0; i<tag->sub_list.len; i++) {
		switch (tag->sub_list.list[i].type) {
		case SUB_TYPE_TEXT:
		case SUB_TYPE_EMPTY_TEXT:
			num += strlen (tag->sub_list.list[i].body.text);
			break;
		case SUB_TYPE_TAG:
			ret = recon_getsize (&(tag->sub_list.list[i].body.tag), flags);
			if (!RERR_ISOK(ret)) return ret;
			num += ret;
			break;
		/* ignore the rest for now - to be done ... */
		}
	}
	return num;
}

/* this function needs to be modified:
	- it should print question and exclam tags as well as comments, too
	- it should quote texts according to xml standard
 */
static
int
recon_writexml (text, tag, flags)
	char				*text;
	struct xml_tag	*tag;
	int				flags;
{
	int	ret, i;
	char	*beg;

	if (!tag) return RERR_PARAM;
	beg = text;
	if (flags & XML_SEARCH_RECON_TAG) {
		text += sprintf (text, "<%s", tag->name);
		for (i=0; i<tag->attr_list.len; i++) {
			text += sprintf (text, " %s=\"%s\"", 
								tag->attr_list.list[i].var, 
								tag->attr_list.list[i].val);
		}
		text += sprintf (text, ">");
	}
	for (i=0; i<tag->sub_list.len; i++) {
		switch (tag->sub_list.list[i].type) {
		case SUB_TYPE_TEXT:
		case SUB_TYPE_EMPTY_TEXT:
			text += sprintf (text, "%s", tag->sub_list.list[i].body.text);
			break;
		case SUB_TYPE_TAG:
			ret = recon_writexml (text, &(tag->sub_list.list[i].body.tag), flags 
										| XML_SEARCH_RECON_TAG);
			if (!RERR_ISOK(ret)) return ret;
			text += ret;
			break;
		/* ignore the rest for now - to be done ... */
		}
	}
	if (flags & XML_SEARCH_RECON_TAG) {
		text += sprintf (text, "</%s>", tag->name);
	}
	return text - beg;
}


int
xmltag_hfree (tag)
	struct xml_tag	*tag;
{
	if (!tag) return 0;
	hfree_attr_list (&(tag->attr_list));
	xml_hfree_sub_list (&(tag->sub_list));
	bzero (tag, sizeof (struct xml_tag));
	return 1;
}

static
int
hfree_attr_list (attr_list)
	struct xml_attr_list	*attr_list;
{
	if (!attr_list) return 0;
	if (attr_list->list) free (attr_list->list);
	*attr_list = XML_ATTR_LIST_NULL;
	return 1;
}



static
int
atoflex (fval, str)
	struct flexval	*fval;
	char				*str;
{
	int	hasfloat;
	char	*s;

	if (!fval) return RERR_PARAM;
	if (!str || !*str) return RERR_NOT_FOUND;

	fval->s = str;
	hasfloat = 0;
	for (s=str; *s; s++) {
		if (*s=='.' && !hasfloat) {
			hasfloat = 1;
		} else if (!(isdigit(*s) || ((s==str) && (*s=='-' || *s=='+')))) {
			continue;
		}
	}
	if (*s) {
		fval->what = FLEXVAL_STRING;
	} else if (hasfloat) {
		fval->what = FLEXVAL_FLOAT;
		fval->f = atof (str);
	} else {
		fval->what = FLEXVAL_INT;
		fval->i = atoi (str);
	}
	return RERR_OK;
}

static
int
cmpflexval (fval1, fval2)
	struct flexval	*fval1, *fval2;
{
	float	f1, f2;

	if (!fval1 && !fval2) return 0;
	if (!fval1) return -1;
	if (!fval2) return 1;
	if (fval1->what == FLEXVAL_STRING || fval2->what == FLEXVAL_STRING) {
		return strcasecmp (fval1->s, fval2->s);
	} else if (fval1->what == FLEXVAL_INT && fval2->what == FLEXVAL_INT) {
		if (fval1->i < fval2->i) return -1;
		if (fval1->i > fval2->i) return 1;
		return 0;
	} else {
		if (fval1->what == FLEXVAL_FLOAT) {
			f1 = fval1->f;
		} else {
			f1 = (float) fval1->i;
		}
		if (fval2->what == FLEXVAL_FLOAT) {
			f2 = fval2->f;
		} else {
			f2 = (float) fval2->i;
		}
		if (f1 < f2) return -1;
		if (f1 > f2) return 1;
		return 0;
	}
	return 0;
}

static
int
cmpflexstr (fval, str)
	struct flexval	*fval;
	char				*str;
{
	struct flexval	fval2;
	int				ret;

	if (!fval && (!str || !*str)) return 0;
	if (!fval) return -1;
	if (!str || !*str) return 1;
	if (fval->what == FLEXVAL_STRING) {
		return strcasecmp (fval->s, str);
	}
	ret = atoflex (&fval2, str);
	if (!RERR_ISOK(ret)) {
		return strcasecmp (fval->s, str);
	}
	return cmpflexval (fval, &fval2);
}


static
int
cmp_attr_list (have_list, require_list)
	struct xml_attr_list	*have_list, *require_list;
{
	struct xml_attr	*hattr, *rattr;
	int					i, j, ret;
	struct flexval		fval;

	if (!have_list || !require_list) return 0;
	for (i=0; i<require_list->len; i++) {
		rattr = &(require_list->list[i]);
		if (!rattr->var) continue;
		if (rattr->val) {
			ret = atoflex (&fval, rattr->val);
			if (!RERR_ISOK(ret)) return 0;
		}
		for (j=0; j<have_list->len; j++) {
			hattr = &(have_list->list[j]);
			if (!hattr->var) continue;
			if (!strcasecmp (rattr->var, hattr->var)) {
				if (!rattr->val) break;
				if (!hattr->val) continue;
				if (!cmpflexstr (&fval, hattr->val)) break;
			}
		}
		if (j>=have_list->len) return 0;
	}
	return 1;
}



static
int
print_sublist (sub_list, num_space)
	struct xml_sub_list	*sub_list;
	int						num_space;
{
	int	i;

	if (!sub_list) return RERR_PARAM;
	for (i=0; i<sub_list->len; i++) {
		if (sub_list->list[i].type==SUB_TYPE_TEXT) {
			print_text (sub_list->list[i].body.text, num_space);
		} else if (sub_list->list[i].type==SUB_TYPE_TAG) {
			print_tag (&(sub_list->list[i].body.tag), num_space);
		}
	}
	return RERR_OK;
}

static
int
print_text (text, num_space)
	char	*text;
	int	num_space;
{
	if (!text) return RERR_PARAM;
	print_space (num_space);
	printf ("text: >>%s<<\n", text);
	return RERR_OK;
}

static
int
print_space (num_space)
	int	num_space;
{
	int	i;

	for (i=0; i<num_space; i++) putchar (' ');
	return RERR_OK;
}

static
int
print_tag (tag, num_space)
	struct xml_tag	*tag;
	int				num_space;
{
	int	i;

	if (!tag) return RERR_PARAM;
	print_space (num_space);
	printf ("tag: >>%s<<\n", tag->name?tag->name:"<NULL>");
	print_space (num_space);
	printf ("attr: %d\n", tag->attr_list.len);
	if (tag->attr_list.len>0) {
		for (i=0; i<tag->attr_list.len; i++) {
			print_attr (&(tag->attr_list.list[i]), num_space+6);
		}
	}
	print_space (num_space);
	printf ("subs: %d\n", tag->sub_list.len);
	print_sublist (&(tag->sub_list), num_space+6);
	return RERR_OK;
}


static
int
print_attr (attr, num_space)
	struct xml_attr	*attr;
	int					num_space;
{
	if (!attr) return RERR_PARAM;
	print_space (num_space);
	printf ("%s=\"%s\"\n", attr->var?attr->var:"<NULL>", 
							attr->val?attr->val:"<NULL>");
	return RERR_OK;
}






static
int
tag_add_father (tag)
	struct xml_tag	*tag;
{
	int				i, ret;
	struct xml_tag	*stag;

	if (!tag) return RERR_PARAM;
	for (i=0; i<tag->sub_list.len; i++) {
		if (!SUB_TYPE_ISTAG(tag->sub_list.list[i].type)) continue;
		stag = &(tag->sub_list.list[i].body.tag);
		stag->father = tag;
		ret = tag_add_father (stag);
		if (!RERR_ISOK(ret)) return ret;
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
