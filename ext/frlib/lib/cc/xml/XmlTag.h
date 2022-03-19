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

#ifndef _R__FRLIB_LIB_CC_XML_XMLTAG_H
#define _R__FRLIB_LIB_CC_XML_XMLTAG_H


#include <fr/xml/xmlparser.h>
#include <fr/xml/xmltag.h>
#include <strings.h>


class XmlTag: public xml_tag {
public:
	XmlTag ()
	{
		bzero (this, sizeof(struct xml_tag));
	};
	~XmlTag ()
	{
		xmltag_hfree ((struct xml_tag*)this);
	};
	int findAttr (char **val, char *attr)
	{
		return xmltag_findattr (val, (struct xml_tag*)this, attr);
	};
	int findNAttr (char **val, char *attr, int num)
	{
		return xmltag_findnattr (val, (struct xml_tag*)this, attr, num);
	};
	int hasAttr (char *attr)
	{
		return xmltag_hasattr ((struct xml_tag*)this, attr);
	};
	char *getAttr (char *attr)
	{
		return xmltag_getattr ((struct xml_tag*)this, attr);
	};
	char *getNAttr (char *attr, int num)
	{
		return xmltag_getnattr ((struct xml_tag*)this, attr, num);
	};
	int numAttr (char *attr)
	{
		return xmltag_numattr ((struct xml_tag*)this, attr);
	};
	char *getName ()
	{
		return name;
	};
	int search (char **out, char *query, int flags)
	{
		return xmltag_search (out, (struct xml_tag*)this, query, flags);
	};
	int fsearch (char **out, int flags, char *query_fmt, ...)
						__attribute__((format(printf, 4, 5)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, query_fmt);
		ret = xmltag_vfsearch (	out, (struct xml_tag*)this, flags,
										query_fmt, ap);
		va_end (ap);
		return ret;
	};
	int vfsearch (char **out, int flags, char *query_fmt, va_list ap)
	{
		return xmltag_vfsearch (out, (struct xml_tag*)this, flags, 
										query_fmt, ap);
	};
};













#endif	/* _R__FRLIB_LIB_CC_XML_XMLTAG_H */

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
