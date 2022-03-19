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

#ifndef _R__FRLIB_LIB_CC_XML_XMLCURSOR_H
#define _R__FRLIB_LIB_CC_XML_XMLCURSOR_H


#include <fr/xml/xmlparser.h>
#include <fr/xml/xmlcursor.h>
#include <fr/cc/xml/XmlParser.h>



class XmlCursor: public xml_cursor {
public:
	XmlCursor (XmlParser &parser)
	{
		xmlcurs_new ((struct xml_cursor*)this, &(parser.xml));
	};
	XmlCursor (XmlCursor &oxml)
	{
		*this = oxml;
	};
	~XmlCursor () {};

	int top ()
	{
		return xmlcurs_top ((struct xml_cursor*) this);
	};
	int up ()
	{
		return xmlcurs_up ((struct xml_cursor*) this);
	};
	int down ()
	{
		return xmlcurs_down ((struct xml_cursor*) this);
	};
	int next ()
	{
		return xmlcurs_next ((struct xml_cursor*) this);
	};
	int back ()
	{
		return xmlcurs_back ((struct xml_cursor*) this);
	};

	int search (char **out, char *query, int flags)
	{
		return xmltag_search (out, tag, query, flags);
	};
	int fsearch (char **out, int flags, char *query_fmt, ...)
				__attribute__((format(printf, 4, 5)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, query_fmt);
		ret = xmltag_vfsearch (out, tag, flags, query_fmt, ap);
		va_end (ap);
		return ret;
	};
	int vfsearch (char **out, int flags, char *query_fmt, va_list ap)
	{
		return xmltag_vfsearch (out, tag, flags, query_fmt, ap);
	};

	int searchTag (char *query, int flags)
	{
		return xmlcurs_searchtag ((struct xml_cursor*)this, query, flags);
	};
	int fsearchTag (int flags, char *query_fmt, ...)
				__attribute__((format(printf, 3, 4)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, query_fmt);
		ret = vfsearchTag (flags, query_fmt, ap);
		va_end (ap);
		return ret;
	};
	int vfsearchTag (int flags, char *query_fmt, va_list ap)
	{
		return xmlcurs_vfsearchtag ((struct xml_cursor*)this, flags, query_fmt, ap);
	};
};













#endif	/* _R__FRLIB_LIB_CC_XML_XMLCURSOR_H */

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
