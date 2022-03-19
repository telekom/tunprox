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


#include <fr/base/errors.h>
#include <fr/base/config.h>
#include <fr/base/slog.h>
#include <fr/base/textop.h>
#include <fr/xml/xmlparser.h>
#include <fr/xml/xmlcursor.h>


int
xmlcurs_new (curs, xml)
	struct xml_cursor	*curs;
	struct xml			*xml;
{
	if (!curs || !xml) return RERR_PARAM;
	curs->xml = xml;
	curs->tag = &(xml->top_tag);
	curs->oldtag = NULL;
	return RERR_OK;
}

int
xmlcurs_newtag (curs, tag)
	struct xml_cursor	*curs;
	struct xml_tag		*tag;
{
	if (!curs || !tag) return RERR_PARAM;
	curs->xml = NULL;
	curs->tag = tag;
	curs->oldtag = NULL;
	return RERR_OK;
}

struct xml_tag*
xmlcurs_tag (curs)
	struct xml_cursor	*curs;
{
	if (!curs) return NULL;
	return curs->tag;
}


int
xmlcurs_up (curs)
	struct xml_cursor	*curs;
{
	if (!curs || !curs->tag) return RERR_PARAM;
	if (!curs->tag->father) return RERR_NOT_FOUND;
	curs->oldtag = curs->tag;
	curs->tag = curs->tag->father;
	return RERR_OK;
}


int
xmlcurs_down (curs)
	struct xml_cursor	*curs;
{
	int	i;

	if (!curs || !curs->tag) return RERR_PARAM;
	for (i=0; i<curs->tag->sub_list.len; i++) {
		if (!SUB_TYPE_ISTAG (curs->tag->sub_list.list[i].type)) continue;
		curs->oldtag = curs->tag;
		curs->tag = &(curs->tag->sub_list.list[i].body.tag);
		return RERR_OK;
	}
	return RERR_NOT_FOUND;
}


int
xmlcurs_next (curs)
	struct xml_cursor	*curs;
{
	int				i;
	struct xml_tag	*father;

	if (!curs || !curs->tag) return RERR_PARAM;
	father = curs->tag->father;
	if (!father) return RERR_NOT_FOUND;
	for (i=0; i<father->sub_list.len; i++) {
		if (!SUB_TYPE_ISTAG (father->sub_list.list[i].type)) continue;
		if (&(father->sub_list.list[i].body.tag) == curs->tag) break;
	}
	for (i++; i<father->sub_list.len; i++) {
		if (!SUB_TYPE_ISTAG (father->sub_list.list[i].type)) continue;
		curs->oldtag = curs->tag;
		curs->tag = &(father->sub_list.list[i].body.tag);
		return RERR_OK;
	}
	return RERR_NOT_FOUND;
}

int
xmlcurs_back (curs)
	struct xml_cursor	*curs;
{
	struct xml_tag	*dummy;

	if (!curs || !curs->tag) return RERR_PARAM;
	if (!curs->oldtag) return RERR_NOT_FOUND;
	dummy = curs->tag;
	curs->tag = curs->oldtag;
	curs->oldtag = dummy;
	return RERR_OK;
}

int
xmlcurs_top (curs)
	struct xml_cursor	*curs;
{
	if (!curs || !curs->xml) return RERR_PARAM;
	curs->tag = &(curs->xml->top_tag);
	return RERR_OK;
}


int
xmlcurs_gettext (text, curs, flags)
	struct xml_cursor	*curs;
	char					**text;
	int					flags;
{
	if (!curs || !curs->tag) return RERR_PARAM;
	return xmltag_search (text, curs->tag, "", flags);
}

int
xmlcurs_search (text, curs, query, flags)
	struct xml_cursor	*curs;
	char					**text, *query;
	int					flags;
{
	if (!curs || !curs->tag || !query) return RERR_PARAM;
	return xmltag_search (text, curs->tag, query, flags);
}

int
xmlcurs_vfsearch (text, curs, flags, query, ap)
	struct xml_cursor	*curs;
	char					**text, *query;
	va_list				ap;
	int					flags;
{
	if (!curs || !curs->tag || !query) return RERR_PARAM;
	return xmltag_vfsearch (text, curs->tag, flags, query, ap);
}


int
xmlcurs_fsearch (
	char					**text,
	struct xml_cursor	*curs,
	int					flags,
	char					*query,
	...)
{
	va_list		ap;
	int			ret;

	va_start (ap, query);
	ret = xmltag_vfsearch (text, curs->tag, flags, query, ap);
	va_end (ap);
	return ret;
}



int
xmlcurs_searchtag (curs, query, flags)
	struct xml_cursor	*curs;
	char					*query;
	int					flags;
{
	struct xml_tag	*otag;
	int				ret;

	if (!curs || !curs->tag || !query) return RERR_PARAM;
	ret = xmltag_searchtag (&otag, curs->tag, query, flags);
	if (!RERR_ISOK(ret)) return ret;
	curs->oldtag = curs->tag;
	curs->tag = otag;
	return RERR_OK;
}

int
xmlcurs_vfsearchtag (curs, flags, query, ap)
	struct xml_cursor	*curs;
	char					*query;
	va_list				ap;
	int					flags;
{
	struct xml_tag	*otag;
	int				ret;

	if (!curs || !curs->tag || !query) return RERR_PARAM;
	ret = xmltag_vfsearchtag (&otag, curs->tag, flags, query, ap);
	if (!RERR_ISOK(ret)) return ret;
	curs->oldtag = curs->tag;
	curs->tag = otag;
	return RERR_OK;
}


int
xmlcurs_fsearchtag (
	struct xml_cursor	*curs,
	int					flags,
	char					*query,
	...)
{
	va_list		ap;
	int			ret;

	va_start (ap, query);
	ret = xmlcurs_vfsearchtag (curs, flags, query, ap);
	va_end (ap);
	return ret;
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
