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

#ifndef _R__FRLIB_LIB_CC_XML_XMLPARSER_H
#define _R__FRLIB_LIB_CC_XML_XMLPARSER_H


#include <fr/xml/xmlparser.h>
#include <fr/xml/xmlcpy.h>
#include <fr/cc/xml/XmlTag.h>
#include <fr/base/errors.h>
#include <strings.h>


class XmlCursor;

class XmlParser {
public:
	XmlParser ()
	{
		bzero (&xml, sizeof(struct xml));
	};
	XmlParser (XmlParser &oxml)
	{
		bzero (&xml, sizeof(struct xml));
		xml_cpy (&xml, &(oxml.xml), XMLCPY_F_NEWBUF);
	};
	~XmlParser ()
	{
		xml_hfree (&xml);
	};
	XmlTag* getTag ()
	{
		return (XmlTag*)&(xml.top_tag);
	};
	int Parse (char *xml_text, int flags)
	{
		xml_hfree (&xml);
		return xml_parse (&xml, xml_text, flags);
	};
	int add (XmlParser *oxml, int flags)
	{
		if (!oxml) return RERR_PARAM;
		return xml_merge (&xml, &(oxml->xml), flags);
	};
	int add (char *xml_text, int flags)
	{
		struct xml	oxml;
		int			ret;

		ret = xml_parse (&oxml, xml_text, 0);
		if (!RERR_ISOK(ret)) return ret;
		ret = xml_merge (&xml, &oxml, flags);
		xml_hfree (&oxml);
		return ret;
	};

	int search (char **out, char *query, int flags)
	{
		return xml_search (out, &xml, query, flags);
	};
	int fsearch (char **out, int flags, char *query_fmt, ...)
					__attribute__((format(printf, 4, 5)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, query_fmt);
		ret = xml_vfsearch (out, &xml, flags, query_fmt, ap);
		va_end (ap);
		return ret;
	};
	int vfsearch (char **out, int flags, char *query_fmt, va_list ap)
	{
		return xml_vfsearch (out, &xml, flags, query_fmt, ap);
	};
	
	int printout ()
	{
		return xml_print (&xml);
	};

protected:
	friend class XmlCursor;
	struct xml	xml;
};













#endif	/* _R__FRLIB_LIB_CC_XML_XMLPARSER_H */

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
