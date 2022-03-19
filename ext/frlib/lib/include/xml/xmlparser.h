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

#ifndef _R__FRLIB_LIB_XML_XMLPARSER_H
#define _R__FRLIB_LIB_XML_XMLPARSER_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


struct xml_attr {
	char	*var, *val;
};
#define XML_ATTR_NULL	((struct xml_attr) {NULL, NULL})

struct xml_attr_list {
	struct xml_attr	*list;
	int					len;
};
#define XML_ATTR_LIST_NULL	((struct xml_attr_list) {NULL, 0})


struct xml_sub;
struct xml_sub_list {
	struct xml_sub	*list;
	int				len;
};
#define XML_SUB_LIST_NULL	((struct xml_sub_list) {NULL, 0})

struct xml_tag {
	char						*name;
	struct xml_attr_list	attr_list;
	struct xml_sub_list	sub_list;
	struct xml_tag			*father;
};
#define XML_TAG_NULL		((struct xml_tag) {NULL, {NULL, 0}, {NULL, 0}, NULL})

struct xml_sub {
	int	type;
	union {
		struct xml_tag	tag;
		char				*text;
	}		body;
};
#define SUB_TYPE_NONE			0
#define SUB_TYPE_MIN				1
#define SUB_TYPE_TAG				1
#define SUB_TYPE_QUESTION_TAG	2
#define SUB_TYPE_EXCLAM_TAG	3
#define SUB_TYPE_COMMENT		4
#define SUB_TYPE_TEXT			5
#define SUB_TYPE_EMPTY_TEXT	6
#define SUB_TYPE_MAX				6
#define SUB_TYPE_ISTAG(t)		(((t)==SUB_TYPE_TAG)||((t)==SUB_TYPE_EXCLAM_TAG))


struct xml {
	char				*_buffer;	/* should never be used directly */
	int				_buflen;		/* should never be used directly */
	int				flags;
	struct xml_tag	top_tag;		/* pseudo tag - it has only a sublist */
};


#define XMLPARSER_FLAG_NONE				0x000
#define XMLPARSER_FLAG_NO_QUESTION		0x001
#define XMLPARSER_FLAG_NO_COMMENT		0x002
#define XMLPARSER_FLAG_NO_EMPTY_TEXT	0x004
#define XMLPARSER_FLAG_NO_EXCLAM			0x008
#define XMLPARSER_FLAG_NO_NORMAL_TAG	0x010
#define XMLPARSER_FLAG_STRIP_ALL_TEXT	0x020
#define XMLPARSER_FLAG_LATIN				0x040	/* input is in latin1 - not utf8 */
#define XMLPARSER_FLAG_NOTCOPY			0x080
#define XMLPARSER_FLAG_NOTFREE			0x100
#define XMLPARSER_FLAGS_STANDARD	(XMLPARSER_FLAG_NO_QUESTION |\
											 XMLPARSER_FLAG_NO_COMMENT |\
											 XMLPARSER_FLAG_NO_EMPTY_TEXT |\
											 XMLPARSER_FLAG_NO_EXCLAM |\
											 XMLPARSER_FLAG_STRIP_ALL_TEXT)


int xml_parse (struct xml *xml, char *xml_text, int flags);
int xml_parse_withdtd (struct xml *xml, char *xml_text, char *dtdfile, int flags);
int xml_hfree (struct xml*);
int xml_free (struct xml*);
#define hfree_xml	xml_hfree
#define free_xml	xml_free


#define XML_SEARCH_NONE				0x000
#define XML_SEARCH_NO_ATTR			0x001
#define XML_SEARCH_TEXT_ONLY		0x002
#define XML_SEARCH_NO_NORMTAG		0x004
#define XML_SEARCH_QUESTION_TAG	0x008
#define XML_SEARCH_EXCLAM_TAG		0x010
#define XML_SEARCH_REQUIRED		0x020	/* write error message to log when not found */
#define XML_SEARCH_NULLEMPTY		0x040	/* missing elements are treated as empty */
#define XML_SEARCH_EMPTYNULL		0x080	/* empty elements are treated as missing */
#define XML_SEARCH_NULLOK			0x100	/* not found element produce RERR_OK */
#define XML_SEARCH_RECON_XML		0x200	/* reconstruct xml from tag */
#define XML_SEARCH_RECON_TAG		0x400	/* include tag in reconstructed xml, otherwise only subtags */

int xml_search (char **text, struct xml*, const char *query, int flags);
int xml_fsearch (char **text, struct xml*, int flags, const char *query_fmt, ...)
							__attribute__((format(printf, 4, 5)));
int xml_vfsearch (char **text, struct xml*, int flags, const char *query_fmt, va_list ap);
int xml_nsearch (char **text, struct xml*, const char *query, int n, int flags);
int xml_fnsearch (char **text, struct xml*, int flags, int n, const char *query_fmt, ...)
							__attribute__((format(printf, 5, 6)));
int xml_vfnsearch (char **text, struct xml*, int flags, int n, const char *query_fmt, va_list ap);
int xmltag_search (char **text, struct xml_tag*, const char *query, int flags);
int xmltag_fsearch (char **text, struct xml_tag*, int flags, const char *query_fmt, ...)
							__attribute__((format(printf, 4, 5)));
int xmltag_vfsearch (char **text, struct xml_tag*, int flags, const char *query_fmt, va_list ap);
int xmltag_nsearch (char **text, struct xml_tag*, const char *query, int n, int flags);
int xmltag_fnsearch (char **text, struct xml_tag*, int flags, int n, const char *query_fmt, ...)
							__attribute__((format(printf, 5, 6)));
int xmltag_vfnsearch (char **text, struct xml_tag*, int flags, int n, const char *query_fmt, va_list ap);
int xmltag_searchtag (struct xml_tag **otag, struct xml_tag*, const char *query, int flags);
int xmltag_fsearchtag (struct xml_tag **otag, struct xml_tag*, int flags, const char *query_fmt, ...)
							__attribute__((format(printf, 4, 5)));
int xmltag_vfsearchtag (struct xml_tag **otag, struct xml_tag*, int flags, const char *query_fmt, va_list ap);
int xmltag_nsearchtag (struct xml_tag **otag, struct xml_tag*, const char *query, int n, int flags);
int xmltag_fnsearchtag (struct xml_tag **otag, struct xml_tag*, int flags, int n, const char *query_fmt, ...)
							__attribute__((format(printf, 5, 6)));
int xmltag_vfnsearchtag (struct xml_tag **otag, struct xml_tag*, int flags, int n, const char *query_fmt, va_list ap);



int xml_print (struct xml*);



#define XML_F_UTF8		0
#define XML_F_LATIN1	XMLPARSER_FLAG_LATIN

/* if flas XML_F_LATIN1 is given, the input string (or outstr for
 *	xml_unquotestr) is considered in latin1 otherwise in utf8.
 */
int xml_quotestr (char **outstr, const char *str, int flags);
/* xml_unquotestr modifies str if outstr is NULL,
 *	otherwise *outstr is malloc'ed
 */
int xml_unquotestr (char **outstr, char *str, int flags);
/* like xml_unquotestr, but outstr must be present 
 */
int xml_unquotestrconst (char **outstr, const char *str, int flags);



/* used internally by xmlcpy and xmlcurs */
int xml_update_father (struct xml *xml);
int xml_hfree_sub (struct xml_sub*);
int xml_hfree_sub_list (struct xml_sub_list*);
int xmltag_hfree (struct xml_tag *tag);









#ifdef __cplusplus
}	/* extern "C" */
#endif









#endif	/* _R__FRLIB_LIB_XML_XMLPARSER_H */

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
